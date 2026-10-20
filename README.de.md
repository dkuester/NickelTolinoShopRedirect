# NickelTolinoShopRedirect

[English](README.md)

Ein NickelHook-Plugin für **tolino**-eReader mit Firmware **5.18+** (Qt 6). Der Shop-Knopf und die
Shop-Suche öffnen den Shop eines **anderen tolino-Händlers**. Das Gerät bleibt dabei bei seinem
eigenen Händler angemeldet, etwa Thalia, mit dem dann auch Skoobe weiter funktioniert.

> Nur für tolino. Auf einem Kobo (oder einem tolino mit Kobo-Firmware) verweigert das Plugin den
> Start und bleibt wirkungslos.

## Warum

Bei welchem Händler sich ein tolino anmelden kann, legt der tolino-Server anhand der Hardware-ID
fest. Ein Gerät von Thalia bietet beim Einrichten nur Thalia.de und buecher.de an. Mit diesem Plugin
kaufst du trotzdem direkt auf dem Gerät bei einem anderen Händler, z. B. **meineBUCHhandlung**
(Libri). Verbindest du die beiden Händlerkonten über „Konten verbinden“, erscheinen die Käufe in der
Bibliothek.

Die Analyse dahinter steht in [docs/analysis.md](docs/analysis.md).

## Was das Plugin tut

- Ersetzt die **Startseite** und die **Such-URL** des Shops durch die Werte aus `config.ini`.
- Lässt die **Basis-URL** des Shops unverändert. An diese Adresse hängt die Firmware dein Login-Token
  an, deshalb gelangt es nicht zum fremden Shop.
- Gibt Seiten des fremden Shops **keinen Zugriff** auf die Firmware-Funktion `setTokens`. Sonst könnte
  ein Login im fremden Shop deinen Geräte-Login überschreiben. Die Bildschirmtastatur funktioniert weiter.

Zum Kaufen meldest du dich einmal im fremden Shop an, wie auf einer normalen Webseite. Der Login des
Geräts ändert sich nicht.

## Installation

1. `Kobo.tgz` aus einem Release herunterladen oder selbst bauen (siehe unten).
2. Den tolino per USB verbinden und `Kobo.tgz` nach `.kobo/` kopieren.
3. Auswerfen. Der tolino installiert das Plugin und startet neu.

Beim ersten Start legt das Plugin `.adds/NickelTolinoShopRedirect/config.ini` an. Dort steht
standardmäßig meineBUCHhandlung.

## Konfiguration

`.adds/NickelTolinoShopRedirect/config.ini`:

```ini
[shop]
enabled=true
start_url=https://mt.shop-asp.de/de/
search_url=https://mt.shop-asp.de/de/search/tolino?q=
```

- Nur `https://`-Adressen werden akzeptiert. Ein ungültiger oder leerer Wert lässt die
  Originaladresse des Händlers stehen.
- Der Suchbegriff wird an `search_url` angehängt.
- Änderungen wirken beim nächsten Öffnen des Shops, ein Neustart ist nicht nötig.
- Mit `enabled=false` ist das Plugin aus, ohne es zu deinstallieren.

Die Shop-URLs anderer Händler stehen in deren öffentlicher tolino-Konfiguration
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`, Felder
`URL_SHOP_EBOOK_START_PAGE` und `URL_SHOP_EBOOK_SEARCH`).

Log: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## Deinstallation

Eine leere Datei `.adds/NickelTolinoShopRedirect/uninstall` anlegen und den tolino neu starten.

## Bauen

```bash
git clone --recurse-submodules https://codeberg.org/hortizept/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" strayrose/nickeltc sh -c 'make && make kobo'
```

Das Ergebnis ist `hook/Kobo.tgz` (enthält `imageformats/libntshopredirect.so`).

## Getestet

- Statisch gegen tolino-Firmware 5.18.264769 (shine 5): alle Hook-Symbole vorhanden, ABI geprüft.
- Auf dem Gerät: noch offen.

## Lizenz

MIT, siehe [LICENSE](LICENSE). Verwendet [NickelHook](https://github.com/nicoverbruggen/NickelHook) (Branch `qt6-support`).
