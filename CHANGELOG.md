# Changelog

## v0.2.0 – 2026-10-28 (Testrelease)

Getestet: tolino shine 5, 5.18.264769.

- Empfehlungs-Kacheln auf dem Startbildschirm suchen das Buch im Shop aus `config.ini`: `search_url`
  mit der EAN aus der Kennung der Empfehlung, ohne EAN mit Titel und Autor. Neuer Schalter
  `recommendations` (Voreinstellung `true`, auch bei fehlendem Eintrag). Die Liste „Alle anschauen“
  und die Empfehlungen am Buchende öffnen weiter den eigenen Händler. Sync und gespeicherte
  Empfehlungen bleiben unverändert; die bestehenden Token-Schutzmechanismen greifen auch hier.

## v0.1.0 – 2026-10-20 (Testrelease)

Erste Version für tolino-eReader mit Firmware 5.18 (getestet: tolino shine 5, 5.18.264769).

- Shop-Knopf, „Weitere Bücher“ am Ende einer Leseprobe und die Shop-Suche öffnen den Shop aus
  `.adds/NickelTolinoShopRedirect/config.ini` (Voreinstellung: meineBUCHhandlung). Das Gerät bleibt
  beim eigenen Händler angemeldet, Sync, Bibliothek und Skoobe bleiben unverändert.
- Empfehlungs-Kacheln auf dem Startbildschirm führen weiter zum eigenen Händler.
- Schutz des Geräte-Logins: Fremde Shop-Seiten werden ohne Access-Token geladen,
  `epublishing://requestauth` für fremde Shops wird verworfen, und fremde Shops erhalten keinen
  Zugriff auf die JS-Brücke mit `setTokens`.
- Nur tolino: Im Kobo-Modus verweigert das Plugin den Start.
- `examples/config.ini` mit den Shop-Adressen aller tolino-Händler.
- Bekannt: Der Login im Umleitungs-Shop läuft über dessen eigenes Formular (z. B. beim Kauf). Der
  Link „Zur Anmeldung“ auf der Startseite von meineBUCHhandlung tut absichtlich nichts, weil er eine
  Anmeldung mit dem Geräte-Token verlangt.
