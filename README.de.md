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

1. `Kobo.tgz` aus den [Releases](https://codeberg.org/hortizept/NickelTolinoShopRedirect/releases) herunterladen oder selbst bauen (siehe unten).
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

Fertige Einträge für alle tolino-Händler (Deutschland, Österreich, Schweiz, Niederlande, Italien)
stehen in [examples/config.ini](examples/config.ini): den gewünschten Block aktivieren und die Datei auf
den tolino kopieren. Die Adressen stammen aus der öffentlichen tolino-Händlerkonfiguration
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`).

Log: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## Deinstallation

Eine leere Datei `.adds/NickelTolinoShopRedirect/uninstall` anlegen und den tolino neu starten.

## Bauen

```bash
git clone --recurse-submodules https://codeberg.org/hortizept/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" -e VERSION="$(git describe --tags --always --dirty)" \
  strayrose/nickeltc-qt6 sh -c 'make && make kobo'
```

Das Image muss die Qt-6.5.2-Toolchain enthalten (`/tc/arm-kobo-linux-gnueabihf`), z. B. gebaut aus
dem `nickeltc-qt6`-Dockerfile von NickelStorygraph. Ein Image mit der Qt-5-Toolchain (`arm-nickel-…`)
bricht mit „Could not find the supplied Qt6 cross compiler“ ab. `VERSION` wird von außen übergeben,
weil im Container kein `git describe` läuft.

Das Ergebnis ist `hook/Kobo.tgz` (enthält `imageformats/libntshopredirect.so`).

## Tests

- Automatisch (CI): Unit-Tests der Logik und Symbol-Check gegen 5.18, siehe [docs/testing.md](docs/testing.md).
  Lokal: `make -C test check` und `python3 test/check_symbols.py`.
- Manuelle Checkliste für das Gerät: [docs/testing.md](docs/testing.md).

## Getestet

- Statisch gegen tolino-Firmware 5.18.264769 (shine 5): alle Hook-Symbole vorhanden, ABI geprüft.
- Auf dem Gerät (tolino shine 5, 5.18.264769): Shop-Knopf und Suche öffnen meineBUCHhandlung, Kauf mit
  dem Login des Shops, Buch erscheint nach dem Sync in der Bibliothek. Thalia-Login, Sync, Skoobe,
  Empfehlungs-Kacheln (→ Thalia) unverändert. Fremde Shop-Seiten werden ohne Access-Token geladen (Log).
  „Weitere Bücher“ am Ende einer Leseprobe öffnet ebenfalls meineBUCHhandlung. Der Shop-Login übersteht
  einen Neustart.

## Lizenz

MIT, siehe [LICENSE](LICENSE). Verwendet [NickelHook](https://github.com/nicoverbruggen/NickelHook) (Branch `qt6-support`).
