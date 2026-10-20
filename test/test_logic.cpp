// Host unit tests for hook/src/shopredirect_logic.h (QtCore only; Qt 5.14+ or 6).
//
// The security-relevant part is the host classification: a wrong answer either
// sends the device's access token to another shop or hands that shop the
// token-setting JS bridge. examples/config.ini is checked as well: every URL in
// it must survive QSettings parsing and valid_url().

#include "../hook/src/shopredirect_logic.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdio>

using namespace shopredirect;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            ++g_failures;                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                           \
    } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        ++g_checks;                                                                 \
        const QString _a = (a), _b = (b);                                           \
        if (_a != _b) {                                                             \
            ++g_failures;                                                           \
            std::fprintf(stderr, "FAIL %s:%d: %s == %s\n  got:      '%s'\n  expected: '%s'\n", \
                         __FILE__, __LINE__, #a, #b, qPrintable(_a), qPrintable(_b));\
        }                                                                           \
    } while (0)

static const QString kThalia = QStringLiteral("thalia.de");

static Config mbh()
{
    Config c;
    c.enabled = true;
    c.start_url = QStringLiteral("https://mt.shop-asp.de/de/");
    c.search_url = QStringLiteral("https://mt.shop-asp.de/de/search/tolino?q=");
    return c;
}

static void test_valid_url()
{
    CHECK_EQ(valid_url("https://mt.shop-asp.de/de/"), "https://mt.shop-asp.de/de/");
    CHECK_EQ(valid_url("  https://mt.shop-asp.de/de/  "), "https://mt.shop-asp.de/de/");
    CHECK_EQ(valid_url("https://ereader.thalia.de/ers/suche?einsprung=firmware&search_complex="),
             "https://ereader.thalia.de/ers/suche?einsprung=firmware&search_complex=");
    CHECK(valid_url("http://mt.shop-asp.de/de/").isEmpty());
    CHECK(valid_url("HTTP://mt.shop-asp.de/").isEmpty());
    CHECK(valid_url("ftp://example.org/").isEmpty());
    CHECK(valid_url("javascript:alert(1)").isEmpty());
    CHECK(valid_url("epublishing://requestauth").isEmpty());
    CHECK(valid_url("https://").isEmpty());
    CHECK(valid_url("mt.shop-asp.de/de/").isEmpty());
    CHECK(valid_url("").isEmpty());
    CHECK(valid_url("   ").isEmpty());
}

static void test_domain()
{
    CHECK_EQ(domain_of("ereader.thalia.de"), "thalia.de");
    CHECK_EQ(domain_of("EREADER.Thalia.DE"), "thalia.de");
    CHECK_EQ(domain_of("thalia.de"), "thalia.de");
    CHECK_EQ(domain_of("mt.shop-asp.de"), "shop-asp.de");
    CHECK(domain_of("localhost").isEmpty());
    CHECK(domain_of("").isEmpty());

    CHECK(in_domain("thalia.de", kThalia));
    CHECK(in_domain("ereader.thalia.de", kThalia));
    CHECK(in_domain("www.thalia.de", kThalia));
    CHECK(!in_domain("evilthalia.de", kThalia));          // suffix, not a subdomain
    CHECK(!in_domain("thalia.de.evil.example", kThalia));
    CHECK(!in_domain("ereader.thalia.at", kThalia));
    CHECK(!in_domain("mt.shop-asp.de", kThalia));
    CHECK(!in_domain("", kThalia));
    CHECK(!in_domain("ereader.thalia.de", ""));            // unknown reseller domain
}

static void test_host_of()
{
    CHECK_EQ(host_of("https://MT.Shop-ASP.de/de/x?y=1"), "mt.shop-asp.de");
    CHECK_EQ(host_of("https://ereader.thalia.de/ers/artikel?einsprung=fwReco&artikelHinweis="), "ereader.thalia.de");
    CHECK(host_of("not a url").isEmpty());
    CHECK(host_of("").isEmpty());
}

static void test_foreign_shop_view()
{
    const Config c = mbh();
    CHECK(is_foreign_shop_view("mt.shop-asp.de", kThalia, c));
    CHECK(!is_foreign_shop_view("ereader.thalia.de", kThalia, c));   // own shop: stock behaviour
    CHECK(!is_foreign_shop_view("www.example.org", kThalia, c));     // not a redirect host
    CHECK(!is_foreign_shop_view("", kThalia, c));
    CHECK(is_foreign_shop_view("mt.shop-asp.de", "", c));            // unknown domain: fail closed

    // A redirect to a shop on the reseller's own domain (OSIANDER on
    // ereader.thalia.de for a Thalia device) keeps the stock behaviour.
    Config osiander;
    osiander.start_url = QStringLiteral("https://ereader.thalia.de/ers/osiander?einsprung=firmware");
    CHECK(!is_foreign_shop_view("ereader.thalia.de", kThalia, osiander));

    // Redirect with only a search URL on another host.
    Config mixed;
    mixed.start_url = QStringLiteral("https://ereader.thalia.de/ers/mayersche");
    mixed.search_url = QStringLiteral("https://www.mayersche.de/index.php?searchparam=");
    CHECK(is_foreign_shop_view("www.mayersche.de", kThalia, mixed));
    CHECK(!is_foreign_shop_view("ereader.thalia.de", kThalia, mixed));

    // Nothing configured: nothing is foreign.
    CHECK(!is_foreign_shop_view("mt.shop-asp.de", kThalia, Config()));
}

static void test_foreign_host()
{
    const Config c = mbh();
    // The reseller's own domain always gets the token, even from a foreign view.
    CHECK(!is_foreign_host("ereader.thalia.de", kThalia, false, c));
    CHECK(!is_foreign_host("ereader.thalia.de", kThalia, true, c));
    CHECK(!is_foreign_host("www.thalia.de", kThalia, true, c));
    // Redirect hosts never get it.
    CHECK(is_foreign_host("mt.shop-asp.de", kThalia, false, c));
    CHECK(is_foreign_host("mt.shop-asp.de", kThalia, true, c));
    // Other hosts: only while a foreign shop view is open (links out of it).
    CHECK(!is_foreign_host("management.mytolino.com", kThalia, false, c));
    CHECK(is_foreign_host("payment.example.org", kThalia, true, c));
    // Lookalike of the reseller domain is foreign inside a foreign view.
    CHECK(is_foreign_host("evilthalia.de", kThalia, true, c));
    // Unknown reseller domain: redirect hosts stay foreign.
    CHECK(is_foreign_host("mt.shop-asp.de", "", false, c));
    CHECK(!is_foreign_host("", kThalia, true, c));
}

static void test_auth_request_target()
{
    // As meineBUCHhandlung builds it for an unknown bookshop.
    const QString inner = QStringLiteral("https://mt.shop-asp.de/wsapi/rest/v1/authentication/byaccesstokenwithredirect?url=");
    const QString page = QStringLiteral("https://mt.shop-asp.de/de/");
    const QString raw = QStringLiteral("epublishing://requestauth?targeturl=")
        + QString::fromLatin1(QUrl::toPercentEncoding(inner))
        + QString::fromLatin1(QUrl::toPercentEncoding(page))
        + QStringLiteral("&oAuthCreateFullAccess=true");
    const QUrl url(raw);
    CHECK_EQ(host_of(auth_request_target(url)), "mt.shop-asp.de");
    CHECK(is_foreign_host(host_of(auth_request_target(url)), kThalia, false, mbh()));

    const QUrl own(QStringLiteral("epublishing://requestauth?targeturl=")
                   + QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("https://ereader.thalia.de/ers/konto"))));
    CHECK(!is_foreign_host(host_of(auth_request_target(own)), kThalia, true, mbh()));

    CHECK(auth_request_target(QUrl(QStringLiteral("epublishing://requestauth"))).isEmpty());
}

static void write_file(const QString &path, const QString &text)
{
    QFile f(path);
    CHECK(f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    f.write(text.toUtf8());
}

static void test_read_config(const QString &tmp)
{
    CHECK(!read_config(tmp + "/missing.ini").enabled);

    // What the plugin writes on first start (QSettings may quote values).
    const QString written = tmp + "/written.ini";
    {
        QSettings s(written, QSettings::IniFormat);
        s.setValue("shop/enabled", true);
        s.setValue("shop/start_url", mbh().start_url);
        s.setValue("shop/search_url", mbh().search_url);
    }
    const Config w = read_config(written);
    CHECK(w.enabled);
    CHECK_EQ(w.start_url, mbh().start_url);
    CHECK_EQ(w.search_url, mbh().search_url);

    // Hand-written values: http rejected, missing enabled means off.
    const QString hand = tmp + "/hand.ini";
    write_file(hand, "[shop]\nstart_url=http://mt.shop-asp.de/de/\nsearch_url=https://mt.shop-asp.de/de/search/tolino?q=\n");
    const Config h = read_config(hand);
    CHECK(!h.enabled);
    CHECK(h.start_url.isEmpty());
    CHECK_EQ(h.search_url, "https://mt.shop-asp.de/de/search/tolino?q=");
}

// Every start_url/search_url in examples/config.ini, active or commented out,
// must parse through QSettings into a valid URL.
static void test_example_config(const QString &example, const QString &tmp)
{
    const Config active = read_config(example);
    CHECK(active.enabled);
    CHECK_EQ(active.start_url, mbh().start_url);
    CHECK_EQ(active.search_url, mbh().search_url);

    QFile f(example);
    CHECK(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    const QRegularExpression re(QStringLiteral("^;?(start_url|search_url)=(.*)$"));
    int n = 0;
    for (const QString &line : lines) {
        const auto m = re.match(line.trimmed());
        if (!m.hasMatch() || m.captured(2).contains(QLatin1Char('<'))) {
            continue;  // not a URL line, or a placeholder like <buchhandlung>
        }
        const QString key = m.captured(1);
        const QString path = tmp + QStringLiteral("/example-%1.ini").arg(n++);
        write_file(path, QStringLiteral("[shop]\nenabled=true\n%1=%2\n").arg(key, m.captured(2)));
        const Config c = read_config(path);
        const QString got = key == QLatin1String("start_url") ? c.start_url : c.search_url;
        if (got != m.captured(2).trimmed()) {
            std::fprintf(stderr, "example line rejected or changed: %s\n", qPrintable(line));
        }
        CHECK_EQ(got, m.captured(2).trimmed());
    }
    CHECK(n >= 20);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString example = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("../examples/config.ini");
    QTemporaryDir tmp;
    CHECK(tmp.isValid());

    test_valid_url();
    test_domain();
    test_host_of();
    test_foreign_shop_view();
    test_foreign_host();
    test_auth_request_target();
    test_read_config(tmp.path());
    test_example_config(example, tmp.path());

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
