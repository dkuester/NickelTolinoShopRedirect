/*
 * shopredirect_logic.h — the decisions of NickelTolinoShopRedirect that do not
 * touch libnickel: which URLs are accepted, which hosts count as foreign, and
 * reading config.ini. QtCore only, so test/test_logic.cpp can run it on the
 * host against Qt 5 or Qt 6.
 */

#pragma once

#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

namespace shopredirect {

struct Config {
    bool enabled = false;
    QString start_url;   // empty: keep the reseller's URL
    QString search_url;  // empty: keep the reseller's URL
};

// Only absolute https URLs with a host are accepted; anything else yields an
// empty string (the original reseller URL is used).
inline QString valid_url(const QString &s)
{
    const QString t = s.trimmed();
    const QUrl u(t, QUrl::StrictMode);
    if (t.isEmpty() || !u.isValid() || u.scheme() != QLatin1String("https") || u.host().isEmpty()) {
        return QString();
    }
    return t;
}

inline QString host_of(const QString &url)
{
    return QUrl(url).host().toLower();
}

// The registrable domain as the last two labels of a host
// (ereader.thalia.de -> thalia.de). Empty for hosts with fewer labels.
// Good enough for the tolino resellers (.de/.at/.ch/.nl/.it), not for
// multi-label public suffixes such as .co.uk.
inline QString domain_of(const QString &host)
{
    const QStringList labels = host.toLower().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (labels.size() < 2) {
        return QString();
    }
    return labels.mid(labels.size() - 2).join(QLatin1Char('.'));
}

inline bool in_domain(const QString &host, const QString &domain)
{
    return !host.isEmpty() && !domain.isEmpty()
        && (host == domain || host.endsWith(QLatin1Char('.') + domain));
}

inline bool is_redirect_host(const QString &host, const Config &c)
{
    return !host.isEmpty()
        && ((!c.start_url.isEmpty() && host == host_of(c.start_url))
            || (!c.search_url.isEmpty() && host == host_of(c.search_url)));
}

// A shop view opened on this host is "foreign" (no token-setting JS bridge):
// a redirect host outside the reseller's own domain. An unknown reseller
// domain counts as foreign: losing the bridge is harmless, leaking is not.
inline bool is_foreign_shop_view(const QString &host, const QString &reseller_domain, const Config &c)
{
    return !in_domain(host, reseller_domain) && is_redirect_host(host, c);
}

// Whether a page on this host must not get the device's access token:
// never for the reseller's own domain; otherwise a redirect host, or any host
// while the current shop view is a foreign one.
inline bool is_foreign_host(const QString &host, const QString &reseller_domain,
                            bool current_shop_foreign, const Config &c)
{
    if (host.isEmpty() || in_domain(host, reseller_domain)) {
        return false;
    }
    return current_shop_foreign || is_redirect_host(host, c);
}

// The decoded targeturl of an epublishing://requestauth?targeturl=… URL.
inline QString auth_request_target(const QUrl &url)
{
    return QUrlQuery(url).queryItemValue(QStringLiteral("targeturl"), QUrl::FullyDecoded);
}

inline Config read_config(const QString &path)
{
    Config c;
    if (!QFileInfo::exists(path)) {
        return c;
    }
    QSettings s(path, QSettings::IniFormat);
    c.enabled    = s.value(QStringLiteral("shop/enabled"), false).toBool();
    c.start_url  = valid_url(s.value(QStringLiteral("shop/start_url")).toString());
    c.search_url = valid_url(s.value(QStringLiteral("shop/search_url")).toString());
    return c;
}

} // namespace shopredirect
