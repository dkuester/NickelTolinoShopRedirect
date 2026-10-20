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

1. Download `Kobo.tgz` from the [releases](https://codeberg.org/hortizept/NickelTolinoShopRedirect/releases) or build it (see below).
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

Ready-made entries for all tolino resellers (Germany, Austria, Switzerland, Netherlands, Italy) are in
[examples/config.ini](examples/config.ini): enable the block you want and copy the file to the tolino.
The URLs come from the public tolino reseller config
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`).

Log: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## Uninstall

Create an empty file `.adds/NickelTolinoShopRedirect/uninstall` and restart the tolino.

## Build

```bash
git clone --recurse-submodules https://codeberg.org/hortizept/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" -e VERSION="$(git describe --tags --always --dirty)" \
  strayrose/nickeltc-qt6 sh -c 'make && make kobo'
```

The image must contain the Qt 6.5.2 toolchain (`/tc/arm-kobo-linux-gnueabihf`), e.g. built from
NickelStorygraph's `nickeltc-qt6` Dockerfile. An image with the Qt 5 toolchain (`arm-nickel-…`) stops
with "Could not find the supplied Qt6 cross compiler". `VERSION` is passed in because `git describe`
does not run inside the container.

The result is `hook/Kobo.tgz` (contains `imageformats/libntshopredirect.so`).

## Tests

- Automated (CI): unit tests of the logic and a symbol check against 5.18, see [docs/testing.md](docs/testing.md).
  Locally: `make -C test check` and `python3 test/check_symbols.py`.
- Manual on-device checklist: [docs/testing.md](docs/testing.md).

## Tested

- Statically against tolino firmware 5.18.264769 (shine 5): all hook symbols present, ABI checked.
- On device (tolino shine 5, 5.18.264769): shop button and search open meineBUCHhandlung, buying with the
  shop's own login works, the book shows up in the library after a sync. Thalia login, sync, Skoobe and
  the home screen recommendations (→ Thalia) are unchanged. Foreign shop pages load without the access
  token (log). "Weitere Bücher" at the end of a preview opens meineBUCHhandlung too. The shop login
  survives a restart.

## License

MIT, see [LICENSE](LICENSE). Uses [NickelHook](https://github.com/nicoverbruggen/NickelHook) (branch `qt6-support`).
