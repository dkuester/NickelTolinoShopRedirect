# NickelTolinoShopRedirect

[Deutsch](README.de.md)

A NickelHook plugin for **tolino** e-readers on firmware **5.18+** (Qt 6). The shop button and the
store search open the shop of **another tolino reseller**, while the device stays signed in with its
own reseller (e.g. Thalia), so Skoobe keeps working.

> Tolino only. On a Kobo (or a tolino running Kobo firmware) the plugin refuses to start and does
> nothing.

## Why

The tolino server decides which resellers a device can sign in with, based on its hardware id. A
device sold by Thalia only offers Thalia.de and buecher.de at setup. With this plugin you can still
shop at another reseller directly on the device, e.g. **meineBUCHhandlung** (Libri). If you link both
reseller accounts ("Konten verbinden"), the purchases show up in the library.

The analysis behind it (German) is in [docs/analysis.md](docs/analysis.md).

## What it does

- Replaces the shop's **start page** and **search URL** with the values from `config.ini`.
- Leaves the shop's **base URL** alone. The firmware attaches your login token to requests to that
  address, so the token never reaches the other shop.
- Gives the other shop's pages **no access** to the firmware's `setTokens` function. Otherwise a login
  in that shop could overwrite your device login. The on-screen keyboard keeps working.

To buy, sign in to the other shop once, like on any website. The device login does not change.

## Installation

1. Download `Kobo.tgz` from a release or build it (see below).
2. Connect the tolino over USB and copy `Kobo.tgz` to `.kobo/`.
3. Eject. The tolino installs the plugin and restarts.

On first start the plugin creates `.adds/NickelTolinoShopRedirect/config.ini`, preset to
meineBUCHhandlung.

## Configuration

`.adds/NickelTolinoShopRedirect/config.ini`:

```ini
[shop]
enabled=true
start_url=https://mt.shop-asp.de/de/
search_url=https://mt.shop-asp.de/de/search/tolino?q=
```

- Only `https://` URLs are accepted. An invalid or empty value keeps the reseller's original URL.
- The search term is appended to `search_url`.
- Changes apply the next time the shop opens; no restart needed.
- `enabled=false` turns the plugin off without uninstalling it.

Other resellers' shop URLs are in their public tolino config
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`, fields
`URL_SHOP_EBOOK_START_PAGE` and `URL_SHOP_EBOOK_SEARCH`).

Log: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## Uninstall

Create an empty file `.adds/NickelTolinoShopRedirect/uninstall` and restart the tolino.

## Build

```bash
git clone --recurse-submodules https://codeberg.org/hortizept/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" strayrose/nickeltc sh -c 'make && make kobo'
```

The image must contain the Qt 6 toolchain (`/tc/arm-kobo-linux-gnueabihf`). Some `strayrose/nickeltc` tags only ship the Qt 5 toolchain (`arm-nickel-…`); the build then stops with "Could not find the supplied Qt6 cross compiler".

The result is `hook/Kobo.tgz` (contains `imageformats/libntshopredirect.so`).

## Tested

- Statically against tolino firmware 5.18.264769 (shine 5): all hook symbols present, ABI checked.
- On device: pending.

## License

MIT, see [LICENSE](LICENSE). Uses [NickelHook](https://github.com/nicoverbruggen/NickelHook) (branch `qt6-support`).
