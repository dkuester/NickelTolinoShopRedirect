# Tests

## Automatisch (CI, `.github/workflows/ci.yml`)

| Job | Was | Lokal |
|---|---|---|
| `logic-qt6` | Unit-Tests der reinen Logik (`hook/src/shopredirect_logic.h`): URL-Prüfung, Domain-Entscheidung „fremd/eigen“, `requestauth`-Ziel, `config.ini` lesen, alle URLs in `examples/config.ini` | `make -C test check` (Qt 6 oder Qt 5.14+ auf dem Host) |
| `symbols` | Jeder Hook, jedes dlsym-Symbol und jeder erlaubte Aufrufer existiert in libnickel 5.18 (`test/libnickel-5.18.syms.gz`) | `python3 test/check_symbols.py` |
| `build` | ARM-Cross-Build von `Kobo.tgz` im Qt-6-Toolchain-Image `ghcr.io/dkuester/nickelstorygraph-toolchain`, als Artefakt hochgeladen | siehe README, Abschnitt „Bauen“ |

Die Hooks selbst lassen sich nur auf dem Gerät testen. Ein Tag `v*` baut `Kobo.tgz` in der CI und veröffentlicht es mit `config.ini` und `SHA256SUMS` als Release (`.github/workflows/release.yml`).

## Manuell auf dem Gerät (vor jedem Release)

Voraussetzung: tolino mit Firmware 5.18.x, `Kobo.tgz` nach `.kobo/` kopiert, Gerät neu gestartet.
Log: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`.

| # | Schritt | Erwartet | Log |
|---|---|---|---|
| 1 | Erster Start nach der Installation | `config.ini` wird angelegt | `created default config.ini`, `active on Tolino; enabled=true …` |
| 2 | Shop-Knopf | Umleitungs-Shop öffnet sich | `shop start url ->`, `foreign shop view:`, `skipped web channel object 'screenController'`, `loading foreign page without access token` |
| 3 | Shop-Suche | Treffer im Umleitungs-Shop | `shop search url ->` |
| 4 | „Weitere Bücher“ am Ende einer Leseprobe | Umleitungs-Shop | `shop start url ->` |
| 5 | Empfehlungs-Kachel auf dem Startbildschirm | Shop des eigenen Händlers | kein `shop start url` |
| 6 | Shop des eigenen Händlers (über eine Empfehlung) | weiter angemeldet | keine `foreign`-Zeile |
| 7 | Im Umleitungs-Shop anmelden (z. B. beim Kauf), Tastatur | Login-Formular, Tastatur erscheint | – |
| 8 | Kauf oder Leseprobe im Umleitungs-Shop, dann Sync | Buch erscheint in der Bibliothek (Konten verbunden) | – |
| 9 | Sync, Bibliothek, Skoobe | unverändert | – |
| 10 | Neustart | Login im Umleitungs-Shop bleibt | – |
| 11 | `enabled=false` in `config.ini`, ohne Neustart; danach wieder `enabled=true` | Shop-Knopf und Suche öffnen den eigenen Händler, danach wieder den Umleitungs-Shop | bei `false` keine neue Zeile |
| 12 | Leere Datei `.adds/NickelTolinoShopRedirect/uninstall`, dann **ausschalten und einschalten** (Auswerfen/Ruhemodus reicht nicht) | Plugin entfernt, `uninstall` gelöscht, Original-Verhalten | kein neues `active on Tolino` |

Nicht am Gerät prüfbar, nur im Code abgesichert: Start im Kobo-Modus (`init()` verweigert, NickelHook parkt das Plugin).

### Ergebnisse

| Version | Gerät / Firmware | Ergebnis |
|---|---|---|
| v2 (vor v0.1.0) | tolino shine 5, 5.18.264769 | 1–10 bestanden (2026-10-20). 11 und 12 nicht getestet. |
| v0.1.0 (Release-Paket) | tolino shine 5, 5.18.264769 | 1–12 bestanden (2026-10-20). Deinstallation erst nach Aus- und Einschalten (siehe 12). |
