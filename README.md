<div align="center">

# NickelTolinoShopRedirect

**Shop at any tolino reseller from your tolino, without changing the account it is signed in with.**

[![Release](https://img.shields.io/github/v/release/dkuester/NickelTolinoShopRedirect?include_prereleases&label=release)](https://github.com/dkuester/NickelTolinoShopRedirect/releases)
[![CI](https://github.com/dkuester/NickelTolinoShopRedirect/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/dkuester/NickelTolinoShopRedirect/actions/workflows/ci.yml)
[![Firmware](https://img.shields.io/badge/firmware-5.18%2B%20(Qt%206)-informational)](#compatibility)
[![License](https://img.shields.io/github/license/dkuester/NickelTolinoShopRedirect)](LICENSE)

[Installation](#installation) · [Configuration](#configuration) · [How it works](#how-it-works) · [Building](#building) · [Releases](https://github.com/dkuester/NickelTolinoShopRedirect/releases) · [Deutsch](README.de.md)

</div>

The tolino server decides which resellers a device can sign in with, based on
its hardware id. A tolino sold by Thalia only offers Thalia.de and buecher.de at
setup. NickelTolinoShopRedirect is a [NickelHook](https://github.com/pgaskin/NickelHook)
plugin that points the shop button, the store search and the home screen
recommendations to **another tolino reseller**, e.g. **meineBUCHhandlung**
(Libri), while the device stays signed in with its own reseller. Sync, library
and Skoobe keep working as before.

> [!NOTE]
> **v0.2.0 is a test release.** It is tested on one device (tolino shine 5,
> firmware 5.18.264769). Feedback from other models is very welcome.

---

## Features

| Feature | Description |
|---|---|
| **Any reseller's shop** | The shop button, the store search and "Weitere Bücher" at the end of a preview open the shop configured in `config.ini`. |
| **Recommendations in your shop** | A home screen recommendation tile searches the configured shop for the book's EAN (title and author if there is none). |
| **Device login stays put** | The device stays signed in with its own reseller. Sync, library and Skoobe are unchanged. |
| **Token protection** | Pages of the other shop are loaded without your device's access token and get no access to the firmware's `setTokens` function, so they cannot read or overwrite your device login. |
| **All resellers included** | [examples/config.ini](examples/config.ini) has ready-made entries for every tolino reseller in Germany, Austria, Switzerland, the Netherlands and Italy. |
| **No restart for changes** | Edits to `config.ini` apply the next time the shop opens. `enabled=false` switches the plugin off without uninstalling it. |
| **Tolino only** | On a Kobo, or a tolino running Kobo firmware, the plugin refuses to start and does nothing. |

If you link both reseller accounts ("Konten verbinden"), books bought in the
other shop show up in the library after a sync.

## Compatibility

| Device / firmware | Status |
|---|---|
| tolino shine 5, firmware 5.18.264769 | ✅ Tested on device |
| Other tolino models, firmware 5.18+ (Qt 6) | ⚠️ Expected to work, not verified yet |
| tolino / Kobo, firmware 4.x (Qt 5) | ❌ Not supported |
| Kobo devices | ❌ Plugin stays inactive |

## Installation

> [!TIP]
> No NickelMenu or other mod is required.

1. Download `Kobo.tgz` from the [latest release](https://github.com/dkuester/NickelTolinoShopRedirect/releases).
2. Connect the tolino over USB and copy `Kobo.tgz` into the hidden `.kobo/` folder.
3. Eject. The tolino installs the plugin and restarts.

On first start the plugin creates `.adds/NickelTolinoShopRedirect/config.ini`,
preset to meineBUCHhandlung. To buy, sign in to the other shop once, like on
any website. The device login does not change.

**Update:** copy the newer `Kobo.tgz` the same way.

**Uninstall:** create an empty file `.adds/NickelTolinoShopRedirect/uninstall`,
eject, and **power the tolino off and on again** (hold the power button →
"Power off"). Ejecting or sleep is not enough: the file is only checked when the
tolino UI starts. Afterwards the file is gone and the plugin is removed;
`config.ini` and the log stay.

## Configuration

`.adds/NickelTolinoShopRedirect/config.ini`:

```ini
[shop]
enabled=true
start_url=https://mt.shop-asp.de/de/
search_url=https://mt.shop-asp.de/de/search/tolino?q=
recommendations=true
```

| Key | Meaning |
|---|---|
| `enabled` | `false` switches the plugin off without uninstalling it. |
| `start_url` | Start page of the shop. Only `https://` URLs are accepted; an invalid or empty value keeps the reseller's original URL. |
| `search_url` | The search term is appended to this URL. Same rules as `start_url`. |
| `recommendations` | `true` (default, also when missing): a tapped home screen recommendation tile opens `search_url` with the book's EAN. `false`: the reseller's article page. |

To switch to another reseller, enable its block in
[examples/config.ini](examples/config.ini) and copy the file to the tolino. The
URLs come from the public tolino reseller config
(`https://api.pageplace.de/v1/resellers/<id>/client-types/TOLINO_SHINE_5/config`).

Log file: `.adds/NickelTolinoShopRedirect/NickelTolinoShopRedirect.log`

## How it works

- The plugin replaces the shop's **start page** and **search URL** with the values from `config.ini`.
- A tapped **recommendation tile** opens the search URL with the book's EAN,
  taken from the recommendation's id. The recommendations themselves still come
  from the device's reseller; sync and the stored recommendations are untouched.
- It leaves the shop's **base URL** alone. The firmware attaches your login
  token to requests to that address, so the token never reaches the other shop.
- Pages of the other shop get **no access** to the firmware's `setTokens`
  function, and `epublishing://requestauth` requests from them are dropped.
  Otherwise a login in that shop could overwrite your device login. The
  on-screen keyboard keeps working.

The full analysis (German) is in [docs/analysis.md](docs/analysis.md).

## Known limitations

- You sign in to the other shop through its own login form, e.g. when buying.
  The "Zur Anmeldung" link on meineBUCHhandlung's start page does nothing on
  purpose, because it asks for a login with the device token.
- Only the tiles on the home screen open the configured shop. The list behind
  "Alle anschauen" and the recommendations at the end of a book still open the
  reseller's article page.
- A recommendation the configured shop does not sell ends in an empty search.
- Tested on a single model so far (tolino shine 5).

## Building

```bash
git clone --recurse-submodules https://github.com/dkuester/NickelTolinoShopRedirect.git
cd NickelTolinoShopRedirect/hook
docker run --rm -v "$PWD:$PWD" -w "$PWD" -e VERSION="$(git describe --tags --always --dirty)" \
  ghcr.io/dkuester/nickelstorygraph-toolchain sh -c 'make && make kobo'
```

The result is `hook/Kobo.tgz` (contains `imageformats/libntshopredirect.so`).
`VERSION` is passed in because `git describe` does not run inside the container.

<details>
<summary><strong>About the toolchain image</strong></summary>

The image must contain the Qt 6.5.2 toolchain (`/tc/arm-kobo-linux-gnueabihf`).
[`ghcr.io/dkuester/nickelstorygraph-toolchain`](https://github.com/dkuester/NickelStorygraph/pkgs/container/nickelstorygraph-toolchain)
is built by NickelStorygraph's CI and is the one this project's CI uses. An image
with the Qt 5 toolchain (`arm-nickel-…`, e.g. `strayrose/nickeltc`) stops with
"Could not find the supplied Qt6 cross compiler".

</details>

### Tests

| Level | How |
|---|---|
| Logic unit tests (Qt 6) | `make -C test check`, also checks every URL in `examples/config.ini` |
| Symbol check against firmware 5.18 | `python3 test/check_symbols.py` |
| Cross build | CI builds `Kobo.tgz` on every push to `main` and every pull request |
| On device | Manual checklist in [docs/testing.md](docs/testing.md) |

Every `v*` tag builds `Kobo.tgz` in CI and publishes it as a release.
Changes are listed in [CHANGELOG.md](CHANGELOG.md).

## Support

| | |
|---|---|
| Bug reports and questions | [Issues](https://github.com/dkuester/NickelTolinoShopRedirect/issues). Please include device model, firmware version (`.kobo/version`), plugin version and the log file. |
| Downloads and changelog | [Releases](https://github.com/dkuester/NickelTolinoShopRedirect/releases) · [CHANGELOG.md](CHANGELOG.md) |
| Earlier history | Pull requests up to v0.1.0 are in the [archived Codeberg repository](https://codeberg.org/hortizept/NickelTolinoShopRedirect). |
| Contributing | Pull requests against `main`; CI must pass. |

> [!WARNING]
> This plugin changes the behaviour of your e-reader's software. It is not
> affiliated with the tolino alliance, Thalia, Libri or any other reseller. Use
> it at your own risk.

## Acknowledgements

- **[NickelHook](https://github.com/pgaskin/NickelHook)** (MIT, Patrick Gaskin),
  used in the `qt6-support` fork by **Nico Verbruggen**
  ([nicoverbruggen/NickelHook](https://github.com/nicoverbruggen/NickelHook)).
- **[NickelStorygraph](https://github.com/dkuester/NickelStorygraph)**: the Qt 6
  toolchain image.

## AI assistance

This plugin was analysed and built with the help of AI assistants (Claude Code,
Anthropic). Technical review, on-device testing and sign-off were done by a
human.

## License

[MIT](LICENSE)
