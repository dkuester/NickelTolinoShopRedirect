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
 *   2. A tapped home screen recommendation tile opens the configured search
 *      URL with the book's EAN (from its publicationId; title and author if
 *      there is none) instead of the reseller's article page:
 *      TolinoRecommendation::retrieveShopUrl(), only when called from
 *      HomeRecommendationsWidget::bookTapped(). Its other callers (sync, the
 *      home screen, "show all", end of book) keep the reseller's URL, and
 *      nothing stored changes. The search URL is a redirect host, so the
 *      guards below apply to the shop view it opens.
 *
 *   3. The device's access token must not reach a foreign shop. The firmware
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
 *   4. The shop view exposes a JS bridge (ResellerJsInterface, registered as
 *      "screenController" on a web channel in the main world) whose setTokens()
 *      slot overwrites the device's access and refresh token. For shop views
 *      opened on a redirect host, that object is not registered; the keyboard
 *      bridge stays.
 *
 *   5. Tolino only: init() refuses to run unless Device::isTolino() is true.
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
#include <stdint.h>
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

#include "shopredirect_logic.h"

using shopredirect::Config;

typedef void Device;
typedef void TolinoServiceSettings;
typedef void TolinoRecommendation;
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
static const char *const kRecoUrlCallers[] = {
    "_ZN25HomeRecommendationsWidget10bookTappedERK7QString", // home screen tile
    nullptr,
};

// Originals, filled by NickelHook (.out).
// QString is returned through a hidden pointer (r0), `this` follows in r1;
// declaring the return type as QString gives exactly that ABI.
static QString (*TolinoServiceSettings__getResellerShopStartUrl)(TolinoServiceSettings *);
static QString (*TolinoServiceSettings__getResellerShopSearchUrl)(TolinoServiceSettings *);
static QString (*TolinoRecommendation__retrieveShopUrl)(TolinoRecommendation *);
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
static QString (*TolinoRecommendation__getPublicationId)(const TolinoRecommendation *);
static QString (*TolinoRecommendation__getTitle)(const TolinoRecommendation *);
static QString (*TolinoRecommendation__getAuthor)(const TolinoRecommendation *);
// Only its address: a cheap first test of retrieveShopUrl()'s caller.
static void *HomeRecommendationsWidget__bookTapped;

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

static void write_default_config()
{
    QDir().mkpath(QString::fromLatin1(kDataDir));
    QSettings s(QString::fromLatin1(kConfigFile), QSettings::IniFormat);
    s.setValue(QStringLiteral("shop/enabled"), true);
    s.setValue(QStringLiteral("shop/start_url"), QString::fromLatin1(kDefaultStartUrl));
    s.setValue(QStringLiteral("shop/search_url"), QString::fromLatin1(kDefaultSearchUrl));
    s.setValue(QStringLiteral("shop/recommendations"), true);
    s.sync();
}

// Read on every use so edits over USB apply without a restart.
static Config read_config()
{
    return shopredirect::read_config(QString::fromLatin1(kConfigFile));
}

using shopredirect::host_of;

// Whether ret may lie in bookTapped(). retrieveShopUrl() also runs at every
// sync and home screen refresh; this keeps dladdr() off those calls.
static bool near_book_tapped(void *ret)
{
    const uintptr_t start = reinterpret_cast<uintptr_t>(HomeRecommendationsWidget__bookTapped);
    return start && reinterpret_cast<uintptr_t>(ret) - start < 0x10000;
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

// The reseller's own domain (ereader.thalia.de -> thalia.de), from the base
// URL that receives the access token. Empty if it cannot be determined.
static QString reseller_domain()
{
    if (!TolinoServiceSettings__sharedInstance || !TolinoServiceSettings__getResellerShopBaseUrl) {
        return QString();
    }
    TolinoServiceSettings *settings = TolinoServiceSettings__sharedInstance();
    if (!settings) {
        return QString();
    }
    return shopredirect::domain_of(host_of(TolinoServiceSettings__getResellerShopBaseUrl(settings)));
}

static bool is_foreign_host(const QString &host)
{
    return shopredirect::is_foreign_host(host, reseller_domain(), g_current_shop_foreign, read_config());
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
_nh_TolinoRecommendation__retrieveShopUrl(TolinoRecommendation *self)
{
    void *ret = __builtin_return_address(0);
    if (g_active && near_book_tapped(ret) && caller_allowed(ret, kRecoUrlCallers)) {
        const QString term = shopredirect::reco_search_term(TolinoRecommendation__getPublicationId(self),
                                                            TolinoRecommendation__getTitle(self),
                                                            TolinoRecommendation__getAuthor(self));
        const QString url = shopredirect::reco_search_url(read_config(), term);
        if (!url.isEmpty()) {
            log_line(QStringLiteral("recommendation url -> ") + url);
            return url;
        }
    }
    return TolinoRecommendation__retrieveShopUrl(self);
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
        g_current_shop_foreign = shopredirect::is_foreign_shop_view(host, reseller_domain(), read_config());
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
        const QString host = host_of(shopredirect::auth_request_target(*url));
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
    log_line(QStringLiteral("active on Tolino; enabled=%1 start_url=%2 search_url=%3 recommendations=%4")
                 .arg(c.enabled ? QStringLiteral("true") : QStringLiteral("false"),
                      c.start_url.isEmpty() ? QStringLiteral("(stock)") : c.start_url,
                      c.search_url.isEmpty() ? QStringLiteral("(stock)") : c.search_url,
                      c.recommendations ? QStringLiteral("true") : QStringLiteral("false")));
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
        .sym     = "_ZN20TolinoRecommendation15retrieveShopUrlEv",
        .sym_new = "_nh_TolinoRecommendation__retrieveShopUrl",
        .lib     = "libnickel.so.1.0.0",
        .out     = nh_symoutptr(TolinoRecommendation__retrieveShopUrl),
        .desc    = "Recommendation tiles search the configured shop",
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
    {
        .name = "_ZNK20TolinoRecommendation16getPublicationIdEv",
        .out  = nh_symoutptr(TolinoRecommendation__getPublicationId),
        .desc = "Recommendation tile search term",
    },
    {
        .name = "_ZNK20TolinoRecommendation8getTitleEv",
        .out  = nh_symoutptr(TolinoRecommendation__getTitle),
        .desc = "Recommendation tile search term",
    },
    {
        .name = "_ZNK20TolinoRecommendation9getAuthorEv",
        .out  = nh_symoutptr(TolinoRecommendation__getAuthor),
        .desc = "Recommendation tile search term",
    },
    {
        .name = "_ZN25HomeRecommendationsWidget10bookTappedERK7QString",
        .out  = nh_symoutptr(HomeRecommendationsWidget__bookTapped),
        .desc = "Recommendation tile caller check",
    },
    {0},
};

NickelHook(
    .init  = shopredirect_init,
    .info  = &NickelTolinoShopRedirect,
    .hook  = NickelTolinoShopRedirectHook,
    .dlsym = NickelTolinoShopRedirectDlsym,
)
