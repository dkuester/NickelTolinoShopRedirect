<div align="center">

# NickelTolinoShopRedirect

**Auf dem tolino bei jedem tolino-Händler einkaufen, ohne das angemeldete Konto zu wechseln.**

[![Release](https://img.shields.io/github/v/release/dkuester/NickelTolinoShopRedirect?include_prereleases&label=release)](https://github.com/dkuester/NickelTolinoShopRedirect/releases)
[![CI](https://github.com/dkuester/NickelTolinoShopRedirect/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/dkuester/NickelTolinoShopRedirect/actions/workflows/ci.yml)
[![Firmware](https://img.shields.io/badge/firmware-5.18%2B%20(Qt%206)-informational)](#kompatibilität)
[![Lizenz](https://img.shields.io/github/license/dkuester/NickelTolinoShopRedirect)](LICENSE)

[Installation](#installation) · [Konfiguration](#konfiguration) · [Funktionsweise](#funktionsweise) · [Bauen](#bauen) · [Releases](https://github.com/dkuester/NickelTolinoShopRedirect/releases) · [English](README.md)

</div>

Bei welchem Händler sich ein tolino anmelden kann, legt der tolino-Server
anhand der Hardware-ID fest. Ein Gerät von Thalia bietet beim Einrichten nur
Thalia.de und buecher.de an. NickelTolinoShopRedirect ist ein
[NickelHook](https://github.com/pgaskin/NickelHook)-Plugin, das den Shop-Knopf
und die Shop-Suche auf einen **anderen tolino-Händler** umleitet, z. B.
**meineBUCHhandlung** (Libri). Das Gerät bleibt dabei bei seinem eigenen
Händler angemeldet; Sync, Bibliothek und Skoobe funktionieren wie bisher.

> [!NOTE]
> **v0.1.0 ist ein Testrelease.** Getestet ist es auf einem Gerät (tolino
> shine 5, Firmware 5.18.264769). Rückmeldungen zu anderen Modellen sind sehr
> willkommen.

---

## Funktionen

| Funktion | Beschreibung |
|---|---|
| **Shop eines beliebigen Händlers** | Shop-Knopf, Shop-Suche und „Weitere Bücher“ am Ende einer Leseprobe öffnen den Shop aus `config.ini`. |
| **Geräte-Login bleibt** | Das Gerät bleibt beim eigenen Händler angemeldet. Sync, Bibliothek, Skoobe und die Empfehlungen auf dem Startbildschirm bleiben unverändert. |
| **Schutz des Tokens** | Seiten des fremden Shops werden ohne das Access-Token des Geräts geladen und erhalten keinen Zugriff auf die Firmware-Funktion `setTokens`. Sie können den Geräte-Login also weder lesen noch überschreiben. |
| **Alle Händler dabei** | [examples/config.ini](examples/config.ini) enthält fertige Einträge für alle tolino-Händler in Deutschland, Österreich, der Schweiz, den Niederlanden und Italien. |
| **Änderungen ohne Neustart** | Änderungen an `config.ini` gelten beim nächsten Öffnen des Shops. `enabled=false` schaltet das Plugin ab, ohne es zu deinstallieren. |
| **Nur tolino** | Auf einem Kobo oder einem tolino mit Kobo-Firmware verweigert das Plugin den Start und bleibt wirkungslos. |

Verbindest du die beiden Händlerkonten über „Konten verbinden“, erscheinen im
fremden Shop gekaufte Bücher nach einem Sync in der Bibliothek.

## Kompatibilität

| Gerät / Firmware | Status |
|---|---|
| tolino shine 5, Firmware 5.18.264769 | ✅ Auf dem Gerät getestet |
| Andere tolino-Modelle, Firmware 5.18+ (Qt 6) | ⚠️ Sollte funktionieren, noch nicht geprüft |
| tolino / Kobo, Firmware 4.x (Qt 5) | ❌ Nicht unterstützt |
| Kobo-Geräte | ❌ Plugin bleibt inaktiv |

## Installation

> [!TIP]
> NickelMenu oder andere Mods sind nicht nötig.

1. `Kobo.tgz` aus dem [neuesten Release](https://github.com/dkuester/NickelTolinoShopRedirect/releases) herunterladen.
2. Den tolino per USB verbinden und `Kobo.tgz` in den versteckten Ordner `.kobo/` kopieren.
3. Auswerfen. Der tolino installiert das Plugin und startet neu.

Beim ersten Start legt das Plugin `.adds/NickelTolinoShopRedirect/config.ini`
an; voreingestellt ist meineBUCHhandlung. Zum Kaufen meldest du dich einmal im
fremden Shop an, wie auf einer normalen Webseite. Der Login des Geräts ändert
sich nicht.

**Aktualisieren:** die neuere `Kobo.tgz` genauso kopieren.

**Deinstallieren:** eine leere Datei `.adds/NickelTolinoShopRedirect/uninstall`
anlegen, auswerfen und den tolino **aus- und wieder einschalten**
(Ein/Aus-Taste halten → „Ausschalten“). Auswerfen oder Standby reicht nicht:
Die Datei wird nur beim Start der tolino-Oberfläche geprüft. Danach ist die
Datei weg und das Plugin entfernt; `config.ini` und das Log bleiben.

## Konfiguration

`.adds/NickelTolinoShopRedirect/config.ini`:

```ini
[shop]
enabled=true
start_url=https://mt.shop-asp.de/de/
search_url=https://mt.shop-asp.de/de/search/tolino?q=
```

| Schlüssel | Bedeutung |
|---|---|
| `enabled` | `false` schaltet das Plugin ab, ohne es zu deinstallieren. |
| `start_url` | Startseite des Shops. Nur `https://`-URLs werden akzeptiert; ein ungültiger oder leerer Wert behält die Original-URL des Händlers. |
| `search_url` | An diese URL wird der Suchbegriff angehängt. Gleiche Regeln wie bei `start_url`. |

Für einen anderen Händler dessen Block in [examples/config.ini](examples/config.ini)
aktivieren und die Datei auf den tolino kopieren. Die URLs stammen aus der
öffentlichen tolino-Händlerkonfiguration
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`).

Logdatei: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## Funktionsweise

- Das Plugin ersetzt die **Startseite** und die **Such-URL** des Shops durch die Werte aus `config.ini`.
- Die **Basis-URL** des Shops bleibt unverändert. An diese Adresse hängt die
  Firmware dein Login-Token an, deshalb gelangt es nie zum fremden Shop.
- Seiten des fremden Shops erhalten **keinen Zugriff** auf die
  Firmware-Funktion `setTokens`, und ihre `epublishing://requestauth`-Anfragen
  werden verworfen. Sonst könnte ein Login im fremden Shop deinen Geräte-Login
  überschreiben. Die Bildschirmtastatur funktioniert weiter.

Die vollständige Analyse steht in [docs/analysis.md](docs/analysis.md).

## Bekannte Einschränkungen

- Die Anmeldung im fremden Shop läuft über dessen eigenes Formular, z. B. beim
  Kauf. Der Link „Zur Anmeldung“ auf der Startseite von meineBUCHhandlung tut
  absichtlich nichts, weil er eine Anmeldung mit dem Geräte-Token verlangt.
- Bisher nur auf einem Modell getestet (tolino shine 5).

## Bauen

```bash
git clone --recurse-submodules https://github.com/dkuester/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" -e VERSION="$(git describe --tags --always --dirty)" \
  ghcr.io/dkuester/nickelstorygraph-toolchain sh -c 'make && make kobo'
```

Ergebnis ist `hook/Kobo.tgz` (enthält `imageformats/libntshopredirect.so`).
`VERSION` wird übergeben, weil `git describe` im Container nicht läuft.

<details>
<summary><strong>Zum Toolchain-Image</strong></summary>

Das Image muss die Qt-6.5.2-Toolchain enthalten (`/tc/arm-kobo-linux-gnueabihf`).
[`ghcr.io/dkuester/nickelstorygraph-toolchain`](https://github.com/dkuester/NickelStorygraph/pkgs/container/nickelstorygraph-toolchain)
wird von der CI von NickelStorygraph gebaut und auch von der CI dieses Projekts
verwendet. Ein Image mit der Qt-5-Toolchain (`arm-nickel-…`, z. B.
`strayrose/nickeltc`) bricht mit „Could not find the supplied Qt6 cross
compiler“ ab.

</details>

### Tests

| Ebene | Wie |
|---|---|
| Unit-Tests der Logik (Qt 6) | `make -C test check`, prüft auch jede URL in `examples/config.ini` |
| Symbolprüfung gegen Firmware 5.18 | `python3 test/check_symbols.py` |
| Cross-Build | Die CI baut `Kobo.tgz` bei jedem Push auf `main` und jedem Pull-Request |
| Auf dem Gerät | Manuelle Checkliste in [docs/testing.md](docs/testing.md) |

Jeder Tag `v*` baut `Kobo.tgz` in der CI und veröffentlicht es als Release.
Änderungen stehen in [CHANGELOG.md](CHANGELOG.md).

## Support

| | |
|---|---|
| Fehlermeldungen und Fragen | [Issues](https://github.com/dkuester/NickelTolinoShopRedirect/issues). Bitte Gerätemodell, Firmware-Version (`.kobo/version`), Plugin-Version und die Logdatei angeben. |
| Downloads und Änderungen | [Releases](https://github.com/dkuester/NickelTolinoShopRedirect/releases) · [CHANGELOG.md](CHANGELOG.md) |
| Frühere Historie | Pull-Requests bis v0.1.0 liegen im [archivierten Codeberg-Repo](https://codeberg.org/hortizept/NickelTolinoShopRedirect). |
| Mitwirken | Pull-Requests gegen `main`; die CI muss grün sein. |

> [!WARNING]
> Dieses Plugin verändert das Verhalten der Software deines E-Readers. Es steht
> in keiner Verbindung zur tolino-Allianz, zu Thalia, Libri oder einem anderen
> Händler. Nutzung auf eigene Gefahr.

## Danksagung

- **[NickelHook](https://github.com/pgaskin/NickelHook)** (MIT, Patrick Gaskin),
  verwendet in der `qt6-support`-Fork von **Nico Verbruggen**
  ([nicoverbruggen/NickelHook](https://github.com/nicoverbruggen/NickelHook)).
- **[NickelStorygraph](https://github.com/dkuester/NickelStorygraph)**: das
  Qt-6-Toolchain-Image.

## KI-Unterstützung

Dieses Plugin wurde mit Hilfe von KI-Assistenten analysiert und gebaut (Claude
Code, Anthropic). Fachliche Kontrolle, Tests auf dem Gerät und Freigabe lagen
bei einem Menschen.

## Lizenz

[MIT](LICENSE)
