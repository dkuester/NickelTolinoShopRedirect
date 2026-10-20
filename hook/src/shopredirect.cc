/*
 * NickelTolinoShopRedirect — open another tolino reseller's shop on the device
 * while the device stays signed in with its own reseller (firmware 5.18+,
 * Qt 6.5.2; verified against Tolino 5.18.264769)
 *
 * Background (see docs/analysis.md):
 *
 *   The reseller a Tolino is signed in with is fixed by the server (the list
 *   offered at setup depends on the device's hardware id). All shop URLs come
 *   from that reseller's server config and land in TolinoServiceSettings
 *   ([TolinoServices] in "Kobo eReader.conf"); every sync overwrites them.
 *
 * What it does:
 *
 *   1. TolinoServiceSettings::getResellerShopStartUrl() and
 *      getResellerShopSearchUrl() return the URLs from config.ini instead, but
 *      only for the shop button, "browse more" and the store search (checked
 *      by caller via dladdr). Other users keep the reseller's URL: the home
 *      screen recommendations build links to the reseller's article pages
 *      from the start URL. getResellerShopBaseUrl() is never touched.
 *
 *   2. The device's access token must not reach a foreign shop. The firmware
 *      sends it in three ways, each guarded here:
 *        - ResellerRequestInterceptor: X-Access-Token on requests to the base
 *          URL's host only. Unchanged, the base URL stays the reseller's.
 *        - ResellerShopController::goToTargetUrl(): loads every shop page
 *          (initial load, and the target of epublishing://requestauth) with
 *          X-Access-Token, whatever the host. For foreign hosts the hook runs
 *          it with getAccessToken() returning an empty string.
 *        - epublishing://requestauth?targeturl=…: refreshes the token and
 *          opens targeturl with it. Dropped when targeturl is foreign.
 *
 *   3. The shop view exposes a JS bridge (ResellerJsInterface, registered as
 *      "screenController" on a web channel in the main world) whose setTokens()
 *      slot overwrites the device's access and refresh token. For shop views
 *      opened on a redirect host, that object is not registered; the keyboard
 *      bridge stays.
 *
 *   4. Tolino only: init() refuses to run unless Device::isTolino() is true.
 *      NickelHook then restores every hook and parks the plugin, so on a Kobo
 *      (or a Tolino converted to Kobo firmware) nothing changes.
 *
 * "Foreign" host: one of the configured redirect hosts, or any host while the
 * current shop view is a redirected one, unless it belongs to the reseller's
 * own domain (the last two labels of the base URL's host, e.g. thalia.de).
 *
 * Settings: /mnt/onboard/.adds/NickelTolinoShopRedirect/config.ini (created
 * with defaults on first start). Log: NickelTolinoShopRedirect.log next to it.
 */

#include <NickelHook.h>

#include <dlfcn.h>
#include <string.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

typedef void Device;
typedef void TolinoServiceSettings;
typedef void ResellerShopController;
typedef void EpublishingNetworkAccessManager;
typedef void QWebChannel;

static const char kDataDir[]    = "/mnt/onboard/.adds/NickelTolinoShopRedirect";
static const char kConfigFile[] = "/mnt/onboard/.adds/NickelTolinoShopRedirect/config.ini";
static const char kLogFile[]    = "/mnt/onboard/.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log";
static const qint64 kLogMaxBytes = 64 * 1024;

// meineBUCHhandlung (Libri, reseller id 80), from its TOLINO_SHINE_5 config.
static const char kDefaultStartUrl[]  = "https://mt.shop-asp.de/de/";
static const char kDefaultSearchUrl[] = "https://mt.shop-asp.de/de/search/tolino?q=";

// Callers (libnickel, 5.18) whose shop URL is redirected. Matched against the
// exported symbol dladdr() finds for the hook's return address.
static const char *const kStartUrlCallers[] = {
    "_ZN16DiscoverNavMixin12resellerShopEv",    // shop button
    "_ZN16DiscoverNavMixin10goShoppingEv",      // "go shopping" entries
    "_ZN19PreviewActionWidget12onBrowseMoreEv", // end of a preview
    nullptr,
};
static const char *const kSearchUrlCallers[] = {
    "_ZN24TolinoStoreSearchBuilder15buildControllerERK7QString", // store search
    nullptr,
};

// Originals, filled by NickelHook (.out).
// QString is returned through a hidden pointer (r0), `this` follows in r1;
// declaring the return type as QString gives exactly that ABI.
static QString (*TolinoServiceSettings__getResellerShopStartUrl)(TolinoServiceSettings *);
static QString (*TolinoServiceSettings__getResellerShopSearchUrl)(TolinoServiceSettings *);
// Static in 5.18 (reads the account directly, ignores r1): r0 = result only.
static QString (*TolinoServiceSettings__getAccessToken)();
// QString/QUrl passed by value arrive as a pointer to the caller's copy.
static void (*ResellerShopController__ctor)(ResellerShopController *, QString *, bool, QObject *);
static void (*ResellerShopController__goToTargetUrl)(ResellerShopController *, QString *);
static void (*EpublishingNetworkAccessManager__handleAuthRequestCommand)(EpublishingNetworkAccessManager *, QUrl *);
static void (*QWebChannel__registerObject)(QWebChannel *, const QString &, QObject *);

// Resolved via dlsym.
static Device *(*Device__getCurrentDevice)();  // returns Device& (r0)
static bool (*Device__isTolino)(const Device *);
static TolinoServiceSettings *(*TolinoServiceSettings__sharedInstance)();
static QString (*TolinoServiceSettings__getResellerShopBaseUrl)(TolinoServiceSettings *);

// Set at the end of a successful init(); every hook passes through until then.
static bool g_active = false;

// Set by the ResellerShopController constructor hook for the shop view being
// built; read when that view registers its web channel objects and loads
// pages (GUI thread; only one shop view is open at a time).
static bool g_current_shop_foreign = false;

// While set, getAccessToken() returns an empty string. Set only around
// goToTargetUrl() for a foreign URL; thread_local so sync and other threads
// keep the real token.
static thread_local bool g_hide_token = false;

static void log_line(const QString &msg)
{
    nh_log("%s", qPrintable(msg));

    QFile f(QString::fromLatin1(kLogFile));
    if (f.exists() && f.size() > kLogMaxBytes) {
        f.remove();
    }
    if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        f.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
        f.write(" ");
        f.write(msg.toUtf8());
        f.write("\n");
    }
}

struct Config {
    bool enabled = false;
    QString start_url;
    QString search_url;
};

// Only absolute https URLs with a host are accepted; anything else disables
// that URL (the original reseller URL is used).
static QString valid_url(const QString &s)
{
    const QUrl u(s.trimmed(), QUrl::StrictMode);
    if (!u.isValid() || u.scheme() != QLatin1String("https") || u.host().isEmpty()) {
        return QString();
    }
    return s.trimmed();
}

static void write_default_config()
{
    QDir().mkpath(QString::fromLatin1(kDataDir));
    QSettings s(QString::fromLatin1(kConfigFile), QSettings::IniFormat);
    s.setValue(QStringLiteral("shop/enabled"), true);
    s.setValue(QStringLiteral("shop/start_url"), QString::fromLatin1(kDefaultStartUrl));
    s.setValue(QStringLiteral("shop/search_url"), QString::fromLatin1(kDefaultSearchUrl));
    s.sync();
}

// Read on every use so edits over USB apply without a restart.
static Config read_config()
{
    Config c;
    if (!QFileInfo::exists(QString::fromLatin1(kConfigFile))) {
        return c;
    }
    QSettings s(QString::fromLatin1(kConfigFile), QSettings::IniFormat);
    c.enabled    = s.value(QStringLiteral("shop/enabled"), false).toBool();
    c.start_url  = valid_url(s.value(QStringLiteral("shop/start_url")).toString());
    c.search_url = valid_url(s.value(QStringLiteral("shop/search_url")).toString());
    return c;
}

static QString host_of(const QString &url)
{
    return QUrl(url).host().toLower();
}

static bool caller_allowed(void *ret, const char *const *callers)
{
    Dl_info info;
    if (!dladdr(ret, &info) || !info.dli_sname) {
        return false;
    }
    for (const char *const *c = callers; *c; c++) {
        if (strcmp(info.dli_sname, *c) == 0) {
            return true;
        }
    }
    return false;
}

// The reseller's own domain: last two labels of the base URL's host
// (ereader.thalia.de -> thalia.de). Empty if it cannot be determined.
static QString reseller_domain()
{
    if (!TolinoServiceSettings__sharedInstance || !TolinoServiceSettings__getResellerShopBaseUrl) {
        return QString();
    }
    TolinoServiceSettings *settings = TolinoServiceSettings__sharedInstance();
    if (!settings) {
        return QString();
    }
    const QStringList labels = host_of(TolinoServiceSettings__getResellerShopBaseUrl(settings))
                                   .split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (labels.size() < 2) {
        return QString();
    }
    return labels.mid(labels.size() - 2).join(QLatin1Char('.'));
}

static bool is_reseller_host(const QString &host)
{
    const QString domain = reseller_domain();
    return !host.isEmpty() && !domain.isEmpty()
        && (host == domain || host.endsWith(QLatin1Char('.') + domain));
}

static bool is_redirect_host(const QString &host, const Config &c)
{
    return !host.isEmpty()
        && ((!c.start_url.isEmpty() && host == host_of(c.start_url))
            || (!c.search_url.isEmpty() && host == host_of(c.search_url)));
}

// Whether a page on this host must not get the device's access token or the
// token-setting JS bridge. Hosts in the reseller's own domain never are, so
// the stock behaviour of the reseller's shop is unchanged.
static bool is_foreign_host(const QString &host)
{
    if (host.isEmpty() || is_reseller_host(host)) {
        return false;
    }
    return g_current_shop_foreign || is_redirect_host(host, read_config());
}

extern "C" __attribute__((visibility("default"))) QString
_nh_TolinoServiceSettings__getResellerShopStartUrl(TolinoServiceSettings *self)
{
    if (g_active && caller_allowed(__builtin_return_address(0), kStartUrlCallers)) {
        const Config c = read_config();
        if (c.enabled && !c.start_url.isEmpty()) {
            log_line(QStringLiteral("shop start url -> ") + c.start_url);
            return c.start_url;
        }
    }
    return TolinoServiceSettings__getResellerShopStartUrl(self);
}

extern "C" __attribute__((visibility("default"))) QString
_nh_TolinoServiceSettings__getResellerShopSearchUrl(TolinoServiceSettings *self)
{
    if (g_active && caller_allowed(__builtin_return_address(0), kSearchUrlCallers)) {
        const Config c = read_config();
        if (c.enabled && !c.search_url.isEmpty()) {
            log_line(QStringLiteral("shop search url -> ") + c.search_url);
            return c.search_url;
        }
    }
    return TolinoServiceSettings__getResellerShopSearchUrl(self);
}

extern "C" __attribute__((visibility("default"))) QString
_nh_TolinoServiceSettings__getAccessToken()
{
    if (g_hide_token) {
        return QString();
    }
    return TolinoServiceSettings__getAccessToken();
}

extern "C" __attribute__((visibility("default"))) void
_nh_ResellerShopController__ctor(ResellerShopController *self, QString *url, bool flag, QObject *parent)
{
    if (g_active) {
        const QString host = url ? host_of(*url) : QString();
        g_current_shop_foreign = !is_reseller_host(host) && is_redirect_host(host, read_config());
        if (g_current_shop_foreign) {
            log_line(QStringLiteral("foreign shop view: ") + *url);
        }
    }
    ResellerShopController__ctor(self, url, flag, parent);
}

extern "C" __attribute__((visibility("default"))) void
_nh_ResellerShopController__goToTargetUrl(ResellerShopController *self, QString *url)
{
    if (g_active && url && is_foreign_host(host_of(*url))) {
        log_line(QStringLiteral("loading foreign page without access token: ") + host_of(*url));
        const bool prev = g_hide_token;
        g_hide_token = true;
        ResellerShopController__goToTargetUrl(self, url);
        g_hide_token = prev;
        return;
    }
    ResellerShopController__goToTargetUrl(self, url);
}

extern "C" __attribute__((visibility("default"))) void
_nh_EpublishingNetworkAccessManager__handleAuthRequestCommand(EpublishingNetworkAccessManager *self, QUrl *url)
{
    if (g_active && url) {
        const QString target = QUrlQuery(*url).queryItemValue(QStringLiteral("targeturl"), QUrl::FullyDecoded);
        const QString host = host_of(target);
        // A requestauth without a resolvable target is left to the firmware;
        // goToTargetUrl() is still guarded.
        if (is_foreign_host(host)) {
            log_line(QStringLiteral("dropped epublishing://requestauth for foreign target: ") + host);
            return;
        }
    }
    EpublishingNetworkAccessManager__handleAuthRequestCommand(self, url);
}

extern "C" __attribute__((visibility("default"))) void
_nh_QWebChannel__registerObject(QWebChannel *self, const QString &id, QObject *object)
{
    if (g_active && g_current_shop_foreign && object
        && qstrcmp(object->metaObject()->className(), "ResellerJsInterface") == 0) {
        log_line(QStringLiteral("skipped web channel object '") + id + QStringLiteral("' for foreign shop"));
        return;
    }
    QWebChannel__registerObject(self, id, object);
}

static int shopredirect_init()
{
    Device *device = Device__getCurrentDevice ? Device__getCurrentDevice() : nullptr;
    if (!device || !Device__isTolino || !Device__isTolino(device)) {
        // Non-zero: NickelHook restores all hooks and parks the plugin.
        log_line(QStringLiteral("not a Tolino (Kobo mode) - refusing to run, plugin parked"));
        return 1;
    }

    if (!QFileInfo::exists(QString::fromLatin1(kConfigFile))) {
        write_default_config();
        log_line(QStringLiteral("created default config.ini"));
    }

    const Config c = read_config();
    log_line(QStringLiteral("active on Tolino; enabled=%1 start_url=%2 search_url=%3")
                 .arg(c.enabled ? QStringLiteral("true") : QStringLiteral("false"),
                      c.start_url.isEmpty() ? QStringLiteral("(stock)") : c.start_url,
                      c.search_url.isEmpty() ? QStringLiteral("(stock)") : c.search_url));
    g_active = true;
    return 0;
}

static struct nh_info NickelTolinoShopRedirect = {
    .name = "NickelTolinoShopRedirect",
    .desc = "Open another tolino reseller's shop while staying signed in with the device's reseller (Tolino 5.18+)",
    .uninstall_flag = "/mnt/onboard/.adds/NickelTolinoShopRedirect/uninstall",
};

// All hooks are mandatory: redirecting without every token guard is never
// applied; NickelHook then parks the plugin.
static struct nh_hook NickelTolinoShopRedirectHook[] = {
    {
        .sym     = "_ZN21TolinoServiceSettings23getResellerShopStartUrlEv",
        .sym_new = "_nh_TolinoServiceSettings__getResellerShopStartUrl",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(TolinoServiceSettings__getResellerShopStartUrl),
        .desc    = "Shop button opens the configured shop",
    },
    {
        .sym     = "_ZN21TolinoServiceSettings24getResellerShopSearchUrlEv",
        .sym_new = "_nh_TolinoServiceSettings__getResellerShopSearchUrl",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(TolinoServiceSettings__getResellerShopSearchUrl),
        .desc    = "Store search uses the configured shop",
    },
    {
        .sym     = "_ZN21TolinoServiceSettings14getAccessTokenEv",
        .sym_new = "_nh_TolinoServiceSettings__getAccessToken",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(TolinoServiceSettings__getAccessToken),
        .desc    = "Hide the access token while a foreign page loads",
    },
    {
        .sym     = "_ZN22ResellerShopControllerC1E7QStringbP7QObject",
        .sym_new = "_nh_ResellerShopController__ctor",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(ResellerShopController__ctor),
        .desc    = "Detect shop views opened on a redirect host",
    },
    {
        .sym     = "_ZN22ResellerShopController13goToTargetUrlE7QString",
        .sym_new = "_nh_ResellerShopController__goToTargetUrl",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(ResellerShopController__goToTargetUrl),
        .desc    = "Load foreign shop pages without the access token header",
    },
    {
        .sym     = "_ZN31EpublishingNetworkAccessManager24handleAuthRequestCommandE4QUrl",
        .sym_new = "_nh_EpublishingNetworkAccessManager__handleAuthRequestCommand",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(EpublishingNetworkAccessManager__handleAuthRequestCommand),
        .desc    = "Ignore token logins requested for foreign shops",
    },
    {
        .sym     = "_ZN11QWebChannel14registerObjectERK7QStringP7QObject",
        .sym_new = "_nh_QWebChannel__registerObject",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(QWebChannel__registerObject),
        .desc    = "Keep the token-setting JS bridge away from foreign shops",
    },
    {0},
};

static struct nh_dlsym NickelTolinoShopRedirectDlsym[] = {
    {
        .name = "_ZN6Device16getCurrentDeviceEv",
        .out  = nh_symoutptr(Device__getCurrentDevice),
        .desc = "Tolino check",
    },
    {
        .name = "_ZNK6Device8isTolinoEv",
        .out  = nh_symoutptr(Device__isTolino),
        .desc = "Tolino check",
    },
    {
        .name = "_ZN21TolinoServiceSettings14sharedInstanceEv",
        .out  = nh_symoutptr(TolinoServiceSettings__sharedInstance),
        .desc = "Reseller's own shop domain",
    },
    {
        .name = "_ZN21TolinoServiceSettings22getResellerShopBaseUrlEv",
        .out  = nh_symoutptr(TolinoServiceSettings__getResellerShopBaseUrl),
        .desc = "Reseller's own shop domain",
    },
    {0},
};

NickelHook(
    .init  = shopredirect_init,
    .info  = &NickelTolinoShopRedirect,
    .hook  = NickelTolinoShopRedirectHook,
    .dlsym = NickelTolinoShopRedirectDlsym,
)
