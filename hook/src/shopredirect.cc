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
 *      getResellerShopSearchUrl() return the URLs from config.ini instead, so
 *      the shop button and the store search open the other reseller's shop.
 *      getResellerShopBaseUrl() is NOT touched: ResellerRequestInterceptor
 *      adds the device's access token (X-Access-Token) to every request to
 *      that host, and it must keep pointing at the device's own reseller.
 *
 *   2. The shop view exposes a JS bridge (ResellerJsInterface, web channel in
 *      the main world) whose setTokens() slot overwrites the device's access
 *      and refresh token. A foreign shop page must not reach it, or a login in
 *      that shop could replace the device's tokens. For shop views opened on a
 *      redirect host, the ResellerJsInterface object is not registered on the
 *      web channel; the keyboard bridge (KeyboardJsInterface) stays.
 *
 *   3. Tolino only: init() refuses to run unless Device::isTolino() is true.
 *      NickelHook then restores every hook and parks the plugin, so on a Kobo
 *      (or a Tolino converted to Kobo firmware) nothing changes.
 *
 * Settings: /mnt/onboard/.adds/NickelTolinoShopRedirect/config.ini (created
 * with defaults on first start). Log: NickelTolinoShopRedirect.log next to it.
 */

#include <NickelHook.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>

typedef void Device;
typedef void TolinoServiceSettings;
typedef void ResellerShopController;
typedef void QWebChannel;

static const char kDataDir[]    = "/mnt/onboard/.adds/NickelTolinoShopRedirect";
static const char kConfigFile[] = "/mnt/onboard/.adds/NickelTolinoShopRedirect/config.ini";
static const char kLogFile[]    = "/mnt/onboard/.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log";
static const qint64 kLogMaxBytes = 64 * 1024;

// meineBUCHhandlung (Libri, reseller id 80), from its TOLINO_SHINE_5 config.
static const char kDefaultStartUrl[]  = "https://mt.shop-asp.de/de/";
static const char kDefaultSearchUrl[] = "https://mt.shop-asp.de/de/search/tolino?q=";

// Originals, filled by NickelHook (.out).
// QString is returned through a hidden pointer (r0), `this` follows in r1;
// declaring the return type as QString gives exactly that ABI.
static QString (*TolinoServiceSettings__getResellerShopStartUrl)(TolinoServiceSettings *);
static QString (*TolinoServiceSettings__getResellerShopSearchUrl)(TolinoServiceSettings *);
// The QString url is passed by value, i.e. as a pointer to the caller's copy.
static void (*ResellerShopController__ctor)(ResellerShopController *, QString *, bool, QObject *);
static void (*QWebChannel__registerObject)(QWebChannel *, const QString &, QObject *);

// Resolved via dlsym.
static Device *(*Device__getCurrentDevice)();  // returns Device& (r0)
static bool (*Device__isTolino)(const Device *);
static TolinoServiceSettings *(*TolinoServiceSettings__sharedInstance)();
static QString (*TolinoServiceSettings__getResellerShopBaseUrl)(TolinoServiceSettings *);

// Set at the end of a successful init(); every hook passes through until then.
static bool g_active = false;

// Set by the ResellerShopController constructor hook for the shop view being
// built; read when that view registers its web channel objects (loadView()
// follows the constructor on the GUI thread before another shop can open).
static bool g_current_shop_foreign = false;

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

extern "C" __attribute__((visibility("default"))) QString
_nh_TolinoServiceSettings__getResellerShopStartUrl(TolinoServiceSettings *self)
{
    if (g_active) {
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
    if (g_active) {
        const Config c = read_config();
        if (c.enabled && !c.search_url.isEmpty()) {
            log_line(QStringLiteral("shop search url -> ") + c.search_url);
            return c.search_url;
        }
    }
    return TolinoServiceSettings__getResellerShopSearchUrl(self);
}

// A shop view is "foreign" when it opens on one of the configured redirect
// hosts and that host is not the device reseller's own shop host (the one
// that receives the access token). Shop views on any other host keep the
// stock behaviour.
static bool is_foreign_shop_url(const QString &url)
{
    const QString host = host_of(url);
    if (host.isEmpty()) {
        return false;
    }

    const Config c = read_config();
    const bool redirect_host = (!c.start_url.isEmpty() && host == host_of(c.start_url))
                            || (!c.search_url.isEmpty() && host == host_of(c.search_url));
    if (!redirect_host) {
        return false;
    }

    // If the reseller's own host cannot be determined, treat the view as
    // foreign: losing the JS bridge is harmless, leaking setTokens is not.
    if (TolinoServiceSettings__sharedInstance && TolinoServiceSettings__getResellerShopBaseUrl) {
        TolinoServiceSettings *settings = TolinoServiceSettings__sharedInstance();
        if (settings && host == host_of(TolinoServiceSettings__getResellerShopBaseUrl(settings))) {
            return false;
        }
    }
    return true;
}

extern "C" __attribute__((visibility("default"))) void
_nh_ResellerShopController__ctor(ResellerShopController *self, QString *url, bool flag, QObject *parent)
{
    if (g_active) {
        g_current_shop_foreign = url && is_foreign_shop_url(*url);
        if (g_current_shop_foreign) {
            log_line(QStringLiteral("foreign shop view: ") + *url + QStringLiteral(" (JS bridge disabled)"));
        }
    }
    ResellerShopController__ctor(self, url, flag, parent);
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

// All hooks are mandatory: redirecting without the JS-bridge guard (or the
// other way round) is never applied; NickelHook then parks the plugin.
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
        .sym     = "_ZN22ResellerShopControllerC1E7QStringbP7QObject",
        .sym_new = "_nh_ResellerShopController__ctor",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(ResellerShopController__ctor),
        .desc    = "Detect shop views opened on a redirect host",
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
        .desc = "Reseller's own shop host",
        .optional = true,
    },
    {
        .name = "_ZN21TolinoServiceSettings22getResellerShopBaseUrlEv",
        .out  = nh_symoutptr(TolinoServiceSettings__getResellerShopBaseUrl),
        .desc = "Reseller's own shop host",
        .optional = true,
    },
    {0},
};

NickelHook(
    .init  = shopredirect_init,
    .info  = &NickelTolinoShopRedirect,
    .hook  = NickelTolinoShopRedirectHook,
    .dlsym = NickelTolinoShopRedirectDlsym,
)
