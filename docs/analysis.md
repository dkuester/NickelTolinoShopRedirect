# Tolino 5.18: Store-Bindung und Skoobe – Analyse

Stand: 2026-10-20 · Gerät: tolino shine (5. Gen., BW) · Firmware 5.18.264769
Quelle: `ereaderfiles.kobo.com/firmwares/kobo11/Jul2026/kobo-update-5.18.264769/update.tar`
(laut mytolino.de dieselbe Datei für shine 5 und shine 5 color). Analysiert: `libnickel.so.1.0.0`
aus `rootfs.img` (Symbole, Strings, Disassembly) und die öffentliche Tolino-Konfig-API.

## 1. Wie der Store festgelegt wird

### 1.1 Ablauf

1. **Händlerwahl im Setup**: `ChooseSetupTypeController::tolinoResellerSelection()` →
   `ResellerSelectionController`. Die Liste kommt vom Server:
   `GET https://api.pageplace.de/v1/reseller-group/devices/<hardwareId>/resellers?client_type=…&countryCode=…&languageCode=…`
   Sie hängt also von der Hardware-ID und vom Land des Geräts ab. Bei nur einem Eintrag wird die Auswahl übersprungen
   (`"only one reseller in the list - skipping reseller selection"`).
2. **Händler-Konfiguration laden**: `ResellerSelectionController::buildResellerLoginUrl()` →
   `SyncTolinoResellerConfigCommand::syncResellerConfig(resellerId)` →
   `GET https://api.pageplace.de/v1/resellers/<id>/client-types/<typ>/config`
   (öffentlich, ohne Login). Die Antwort enthält u. a.:
   `OAUTH_CLIENT_ID`, `URL_OAUTH_*`, `SHOP_BASE`, `URL_SHOP_EBOOK_START_PAGE`, `URL_SHOP_EBOOK_SEARCH`,
   `STRING_BRAND_NAME`, `SKOOBE_ACTIVATED`, `SKOOBE_SHOP_ACTIVATED`, `URL_SKOOBE_*`, `URL_HANDSHAKE` …
3. **OAuth-Login beim Händler** (WebView). Das Token ist das Händler-Token, damit hängt das Tolino-Konto am Händler.
4. **Speicherung**: `TolinoServiceSettings` ist eine `Settings`/`QSettings`-Klasse, also
   **Klartext in `.kobo/Kobo/Kobo eReader.conf`, Abschnitt `[TolinoServices]`**. Schlüssel u. a.:
   `pref_reseller_id`, `pref_reseller_brand_name`, `pref_reseller_shop_base_url`,
   `pref_reseller_shop_start_url`, `pref_reseller_shop_search_url`, `pref_skoobe_active`,
   `pref_skoobe_shop_activated`, `pref_url_skoobe_*`, `pref_oauth_*`, `pref_access_token`, `pref_refresh_token`.
   Diese Werte sind per USB les- und schreibbar. Achtung: Die Datei enthält auch die Tokens.
5. **Periodischer Re-Sync**: `SyncTolinoResellerConfigCommand::execute()` liest `getResellerId()`
   und lädt bei jedem Sync die Konfiguration neu. Danach werden **alle** Werte mit den Server-Werten
   überschrieben. Ein reines Editieren der `.conf` hält also nur bis zum nächsten Sync.

### 1.2 Was die Händler-ID steuert

`getResellerId()` geht in die Header aller Cloud-Requests (`RxCli::buildGenericV1/V2Headers`,
`getContentHeaders`, `downloadContent`, `registerHw`, `RegisterOrUpdateRequest` …). Händler-ID und
OAuth-Token gehören zusammen. Wer nur `pref_reseller_id` umschreibt, bekommt Requests mit
Händler-ID X und dem Token von Händler Y. Das scheitert sehr wahrscheinlich, und der nächste Sync
überschreibt ohnehin alles mit der Konfiguration von X.

### 1.3 Shop-WebView und Token-Weitergabe

`ResellerRequestInterceptor` hängt an jeden Request, dessen **Host == Host von `SHOP_BASE`** ist,
die Header `X-Access-Token: <Händler-Token>` und `X-Force-Auth` an. Ein Umbiegen von `SHOP_BASE` auf
einen fremden Shop würde das Thalia-Token an diesen Shop schicken. **Das darf ein Plugin nicht tun.**
Startseite und Suche (`getResellerShopStartUrl` / `getResellerShopSearchUrl`) lassen sich dagegen
gefahrlos umbiegen: Für fremde Hosts werden keine Tokens eingefügt.

### 1.4 „libri.de“ in der Tolino-Welt

libri.de ist der Großhändler Libri (B2B). Für Endkunden läuft Tolino über Libri unter der Marke
**„meineBUCHhandlung“** (Libri.Shopline, ca. 1.300 unabhängige Buchhandlungen):
**Händler-ID 80**, Shop `https://mt.shop-asp.de/de/`, eigenes OAuth (`mt.shop-asp.de/oauth/…`).
*Annahme: Das ist mit „libri.de als Store“ gemeint.* (Die ID 81 ist eBook.de.)

## 2. Wie Skoobe freigeschaltet wird

### 2.1 Client-Seite: genau ein Schalter

- `SKOOBE_ACTIVATED` aus der Händler-Konfiguration → `TolinoServiceSettings::setSkoobeActive()` →
  `pref_skoobe_active`.
- **Einzige Leser** von `getSkoobeActive()` in 5.18:
  - `MoreView::updateVisibleButtons()`: blendet den Skoobe-Eintrag unter „Mehr“ ein oder aus.
  - `EpublishingNetworkAccessManager::handleOpenSkoobeLibraryCommand()`: öffnet die Skoobe-Bibliothek
    (z. B. aus dem Shop heraus).
- `getSkoobeShopActive()` hat in 5.18 **keine Aufrufer**.

### 2.2 Händler-Vergleich (Client-Typ `TOLINO_SHINE_5`, live abgefragt)

| ID | Händler | SKOOBE_ACTIVATED | URL_SKOOBE_* vorhanden |
|---|---|---|---|
| 3 | Thalia.de | true | ja |
| 4 | Thalia.at | true | ja |
| 8 | Orell Füssli | true | ja |
| 23 | OSIANDER.de | true | ja |
| 24 | Mayersche | true | ja |
| 13 | Hugendubel.de | – | ja |
| 80 | **meineBUCHhandlung (Libri)** | **–** | **ja, identisch zu Thalia** |
| 81 | eBook.de | – | ja |

Thalia (3) und meineBUCHhandlung (80) unterscheiden sich nur in OAuth, Shop-URLs, Branding und
**dem fehlenden `SKOOBE_ACTIVATED`**. Die Skoobe-Endpunkte (`nomad-prod-open.skoobenet.de/...`) sind
dieselben. Auf dem Gerät ist Skoobe für ID 80 also nur durch diesen einen Schalter ausgeblendet.

### 2.3 Server-Seite: das eigentliche Risiko

`SkoobeRestApi::buildSkoobeHeaders()` sendet `Authorization: Bearer <Händler-Access-Token>`
(+ Header `Client`), **ohne Händler-ID**. Registrierung (`URL_SKOOBE_REGISTER`), Ausleihen,
Rückgabe und Lesezeit laufen über den Skoobe-Proxy mit diesem Token. Ob der Proxy Tokens eines
Händlers ohne Skoobe-Vertrag (z. B. `mt.shop-asp.de`) annimmt, lässt sich offline **nicht**
feststellen. Möglich ist eine Ablehnung bei der Geräte-Registrierung (dann greift
`URL_SKOOBE_REGISTER_ERROR`). Klären lässt sich das nur mit einem Test auf dem Gerät.

## 3. Machbarkeit und Entscheidung

### Ziel 1: Store selbst wählen

| Weg | Bewertung |
|---|---|
| **A: Offizieller Händlerwechsel** (abmelden, im Setup anderen Händler wählen) | **Ausgeschlossen.** Am Gerät getestet: Beim Abmelden stehen nur Thalia.de und buecher.de zur Wahl. Die Liste kommt von `/v1/reseller-group/devices/<hardwareId>/resellers`, das Gerät ist serverseitig der Thalia-Gruppe zugeordnet. Die Liste per Plugin zu fälschen wäre riskant: `RxCli::registerHw()` meldet die Händler-ID an den Server, der kann das jederzeit ablehnen. |
| **B: Login bleibt bei Thalia, nur der Shop wird umgebogen** + Konten über „Konten verbinden“ (`URL_HANDSHAKE`, Tabelle `tolino_resellerHandshake`) | **Gewählt.** Shop-Knopf und Suche öffnen den anderen Shop. Gekauft wird mit dessen eigenem Login in der Shop-Ansicht. Käufe kommen über die verbundenen Konten in die Bibliothek. Skoobe läuft unverändert weiter. |
| C: Nur `.conf` editieren | Nicht tragfähig: wird beim nächsten Sync überschrieben, das Token passt nicht zur ID. |

### Ziel 2: Skoobe

Mit Weg B bleibt das Gerät bei Thalia (`SKOOBE_ACTIVATED=true`), Skoobe funktioniert ohne Eingriff.
Ein Hook auf `getSkoobeActive()` (Abschnitt 2.1) wäre nur für einen echten Händlerwechsel nötig.

### 3.1 Zusatzbefund: JS-Brücke der Shop-Ansicht

`ResellerShopController::loadView()` legt einen `QWebChannel` an, registriert darauf
`ResellerJsInterface` und `KeyboardJsInterface` und hängt ihn mit
`QWebEnginePage::setWebChannel(channel, 0)` in die **Main World** der Seite. Die Seite selbst kann
also über `qt.webChannelTransport` darauf zugreifen. `ResellerJsInterface` bietet u. a. den Slot
`setTokens(accessToken, refreshToken)`, der direkt `TolinoServiceSettings::setAccessToken()` und
`setRefreshToken()` aufruft (Log: `resellershop: setTokens`).

Ein Shop eines anderen Händlers in dieser Ansicht (meineBUCHhandlung ist selbst ein Tolino-Shop)
könnte nach seinem Login eigene Tokens setzen und damit den Thalia-Login des Geräts ersetzen. Das
Plugin registriert deshalb für Shop-Ansichten auf einem Umleitungs-Host das `ResellerJsInterface`
**nicht**. Die Tastatur-Brücke bleibt, damit die Bildschirmtastatur in Eingabefeldern weiter funktioniert.

### 3.2 Umsetzung (NickelTolinoShopRedirect)

| Hook (GOT von libnickel) | Zweck |
|---|---|
| `TolinoServiceSettings::getResellerShopStartUrl()` | Startseite aus `config.ini`, nur für Shop-Knopf, `goShopping` und „Mehr stöbern“ (Aufrufer per `dladdr` geprüft) |
| `TolinoServiceSettings::getResellerShopSearchUrl()` | Such-URL aus `config.ini`, nur für `TolinoStoreSearchBuilder` |
| `TolinoRecommendation::retrieveShopUrl()` | Empfehlungs-Kachel: Such-URL mit der EAN, nur für `HomeRecommendationsWidget::bookTapped` (3.4) |
| `TolinoServiceSettings::getAccessToken()` | liefert leer, solange eine fremde Seite geladen wird (thread-lokal) |
| `ResellerShopController::ResellerShopController(QString, bool, QObject*)` | erkennt Shop-Ansichten auf einem Umleitungs-Host |
| `ResellerShopController::goToTargetUrl(QString)` | lädt fremde Seiten ohne `X-Access-Token` |
| `EpublishingNetworkAccessManager::handleAuthRequestCommand(QUrl)` | verwirft `epublishing://requestauth` mit fremder `targeturl` |
| `QWebChannel::registerObject(QString const&, QObject*)` | lässt `ResellerJsInterface` („screenController“) bei fremden Shops weg |

`getResellerShopBaseUrl()` bleibt unberührt (Token-Header, 1.3). Rückgabe-ABI geprüft: Die
URL-Getter liefern `QString` über einen versteckten Zeiger (r0), `this` in r1. Alle Aufrufe in
libnickel laufen über die PLT, der GOT-Hook greift also überall.

**Nur Tolino:** `init()` prüft `Device::isTolino()`. Im Kobo-Modus meldet es einen Fehler.
NickelHook nimmt dann alle Hooks zurück und parkt das Plugin. Eine Installation zu verhindern ist
nicht möglich, weil die Firmware `Kobo.tgz` selbst entpackt. Das Plugin bleibt dann aber wirkungslos.

### 3.3 Befunde aus dem ersten Gerätetest (v1, 2026-10-20)

- **Empfehlungen:** `TolinoRecommendation::retrieveShopUrl()` und `buildUrlShopRecommendation()`
  bauen die Links der Empfehlungs-Kacheln (12 pro Sync) aus der Startseiten-URL. v1 hat sie mit
  umgeleitet, die Links zeigten auf Thalia-Artikelpfade bei meineBUCHhandlung. Seit v2 wird nur noch
  für die Aufrufer aus 3.2 umgeleitet.
- **Token-Leck in v1:** `ResellerShopController::goToTargetUrl()` lädt *jede* Seite der Shop-Ansicht
  (Startseite aus `loadView()`, Ziel von `epublishing://requestauth`) mit
  `X-Access-Token: <Access-Token>`, **unabhängig vom Host**. v1 hat das übersehen. Damit ging das
  Thalia-Access-Token beim Öffnen des umgeleiteten Shops und beim Klick auf „Zur Anmeldung“ an
  `mt.shop-asp.de`. Das Refresh-Token war nicht betroffen. Seit v2 sind beide Wege gesperrt (3.2).
- **„Zur Anmeldung“ auf der Startseite von meineBUCHhandlung:** Bei unbekannter Buchhandlung zeigt der
  Shop für Tolino-Geräte einen Link `epublishing://requestauth?targeturl=https://<shop>/wsapi/rest/v1/authentication/byaccesstokenwithredirect?url=…`.
  Gedacht ist eine Anmeldung per Geräte-Token. Mit dem Thalia-Token kann das nicht klappen. v2 verwirft
  diesen Aufruf. Die Anmeldung läuft stattdessen über den normalen Login des Shops, z. B. beim Kauf.

### 3.4 Empfehlungs-Kacheln im Umleitungs-Shop

**Codepfad (5.18 und 6.0 gleich).** `HomeRecommendationsWidget::bookTapped(publicationId)` sucht die
Empfehlung, ruft über die PLT `TolinoRecommendation::retrieveShopUrl()` auf und öffnet das Ergebnis mit
`new ResellerShopController(url, true, nullptr)`. `retrieveShopUrl()` hängt das Feld `shop_url` an
`getResellerShopBaseUrl()` und gibt das an `buildUrlShopRecommendation()`. Dieses nimmt Schema und Host
der Start-URL, übernimmt deren Query-Parameter (Tracking wie `einsprung`) und hängt den Artikelpfad an.
Deshalb lieferte v1 mit umgebogener Start-URL kaputte Links.

Weitere Aufrufer von `retrieveShopUrl()`: `convertToRecoVolume()`, aufgerufen vom Sync
(`SyncTolinoRecommendationsCommand::syncHomeRecommendations`, Volumes für die Cover), von
`HomeRecommendationsWidget::setBooks`, `TolinoRecommendationsProvider::contents` und
`NextReadDataProvider::getTolinoRecommendations`. Die fertige URL wird nur im `Volume` gehalten, in der
Datenbank steht das rohe Segment. Die Liste „Alle anschauen“ (`TolinoRecommendationsView::onVolumeSelected`)
und die Empfehlungen am Buchende (`RelatedReadingWidget::bookCoverTapped`) lesen die URL aus diesem Volume.

**Buchdaten.** `tolino_recommendations` hat keine ISBN-Spalte (publicationId, author, cover_url,
description, publicationType, shop_url, title, context, recosJson). Auf dem Gerät (Thalia) enthält die
`publicationId` aber die EAN: `DT0244.9783843737241` (Startseite), `DT0400.9783257608267_A30250671`
(Shop), selten keine (`DT0402.1S2vjR6…`). Titel sind nach 40 Zeichen abgeschnitten, der Autor steht als
„Nachname, Vorname“. Die Shop-Suche von meineBUCHhandlung findet über die EAN genau das Buch (am Gerät
geprüft mit 9783843737241).

**Umsetzung.** Ein Hook auf `retrieveShopUrl()` liefert für den Aufrufer `bookTapped` `search_url` +
EAN (mit Prüfziffer; ohne EAN Titel und Autor, sonst die Original-URL). Alle anderen Aufrufer laufen
unverändert durch: ein Adressvergleich mit `bookTapped` (per `dlsym`) sortiert sie aus, bevor `dladdr`
oder `config.ini` gelesen werden. Sync und gespeicherte Daten bleiben damit unberührt. Die neue URL
liegt auf dem Such-Host, also einem Umleitungs-Host: Der Konstruktor-Hook markiert die Ansicht als fremd,
`goToTargetUrl` lädt ohne Token, `ResellerJsInterface` wird nicht registriert und `requestauth` verworfen.
Thalias Tracking-Parameter gehen nicht an den fremden Shop, weil die Thalia-URL gar nicht gebaut wird.
Der fremde Shop erfährt nur die EAN der Empfehlung.

**Nicht abgedeckt.** „Alle anschauen“ und die Empfehlungen am Buchende (siehe oben). Sie zu erfassen
hieße, `convertToRecoVolume()` je nach Aufrufer umzuleiten, und das läuft auch im Sync.

## 4. Offene Punkte

1. Test auf dem Gerät: Shop-Knopf, Suche, Login im fremden Shop, Kauf, Erscheinen in der Bibliothek.
2. Prüfen, ob der Login im fremden Shop einen Neustart übersteht (Cookies im Profil der Shop-Ansicht).
3. Thalia-Login nach einem Login im fremden Shop prüfen (Sync läuft weiter, Bibliothek vollständig).
