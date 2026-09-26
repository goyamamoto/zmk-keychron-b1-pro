# ZMK for the Keychron B1 Pro

Firmware for the Keychron B1 Pro US (PID `0x0711`) on **upstream ZMK with Zephyr 4.1**, instead of Keychron's ZMK fork on Zephyr 3.2. It brings **ZMK Studio** to the keyboard and contains only open-source code (ZMK, Zephyr and this repository), so ready-to-flash firmware is published in the releases. It comes in two builds: one with the keys as printed, and one with features for typing Japanese (a US-JIS mode for hosts set to the Japanese keyboard layout, IME keys beside Space).

Japanese: [README.ja.md](README.ja.md)

At a glance:

- **ZMK Studio**: change the keymap live from the browser over USB, without rebuilding or reflashing ([zmk.studio](https://zmk.studio)).
- **Current ZMK**: ZMK `main` at the v0.4.0 release candidate, Zephyr 4.1, pinned and built reproducibly.
- Two builds: `keychron-b1-pro.uf2` with the keys as printed, and `keychron-b1-pro-usjis.uf2` with:
  - **US-JIS mode** (Fn+Tab): on a Windows host set to the Japanese keyboard layout, `` ` ~ @ ^ & * ( ) _ = + [ { ] } \ | : ' " `` type as printed on the US keycaps;
  - **IME keys beside Space**: tap for IME off / on, hold for Alt / Cmd;
  - **Caps Lock and Left Ctrl swapped**.
- The keyboard's own hardware keeps working as with the stock firmware: the connection switch, the Mac/Win switch, the LEDs, battery level on Fn+B, charging.
- **Not supported**: the 2.4 GHz link (upstream ZMK has no 2.4 GHz support; the 2.4G position turns the keyboard off) and the Keychron Launcher. For Keychron's own ZMK with 2.4 GHz and the Launcher, see [goyamamoto/zmk-kb1-usjis](https://github.com/goyamamoto/zmk-kb1-usjis).

## Supported hardware: check your PID first

Keychron sells several B1 Pro versions under one name, with different key matrices. This firmware is for exactly one of them; on another version keys will be wrong or dead.

| USB PID | Keychron version | Layout | Here |
| --- | --- | --- | --- |
| `0x0711` | B1 Pro US | ANSI | **Supported** |
| `0x071a` | B1 Pro US "n" version | ANSI | Not supported (different key matrix) |
| `0x0714` | B1 Pro | ANSI | Not supported |
| `0x0712`, `0x071b`, `0x0713`, `0x071c` | UK and JIS versions | ISO, JIS | Not supported |

How to find your PID with the stock firmware:

- Keychron Launcher: Settings → Device Info.
- macOS: `ioreg -p IOUSB -l -w0 | grep -A25 'Keychron B1 Pro@' | grep -E '"(idVendor|idProduct)"'`. The value is decimal; `1809` is `0x0711`.
- Windows: Device Manager → the keyboard → Details → Hardware Ids, which contain `VID_3434&PID_0711`.
- Linux: `lsusb`, which lists `3434:0711`.

## Flashing

1. Download one of the two files from the latest release (or build them, see [Building](#building)):
   - `keychron-b1-pro.uf2`: the keys as printed, no Japanese features.
   - `keychron-b1-pro-usjis.uf2`: with US-JIS mode, the IME keys and Caps Lock / Left Ctrl swapped ([Keys](#keys)).
2. Hold the reset switch in the hole on the back of the keyboard while connecting USB. A drive named `NRF52BOOT` appears.
3. Copy the `.uf2` file to the drive. The drive disappears and the keyboard restarts.
4. Coming from the stock firmware: clear the Bluetooth bonds it left behind with Fn+Shift+Esc held for 10 s, then pair again (Fn+1 held for 3 s).

The bootloader is not touched, so the same path works from any firmware. To go back to Keychron's firmware, flash Keychron's official B1 Pro firmware the same way.

The keyboard identifies itself with ZMK's USB ID (`1d50:615e`), so the Keychron Launcher does not recognize it.

## Keys

The layout of the US keycaps, with a Mac layer and a Win layer selected by the Mac/Win switch, each with an Fn layer:

| Keys | Mac | Win |
| --- | --- | --- |
| F row (without Fn) | Brightness, Mission Control, Launchpad, Search, Lock, media, volume | F1–F12 |
| Fn + F row | F1–F12 | Brightness, Task View, Explorer, Search, Lock, media, volume |
| Fn+1 … Fn+4 | Bluetooth profile 1–4; held 3 s: forget it and pair anew | same |
| Fn+Shift+Esc held 10 s | Clear all Bluetooth bonds (Fn+Esc alone is Esc) | same |
| Fn+B (held) | Battery level on the RGB LED | same |
| Fn+Del | Unlock ZMK Studio | same |
| Fn+= | Search | Calculator |
| Fn+I | Insert | Insert |
| Fn+\\ | Screenshot (Cmd+Shift+4) | Snipping (Win+Shift+S) |
| Fn+Right Shift | Emoji (Ctrl+Cmd+Space) | Emoji (Win+.) |
| Fn+arrows | Home, Page Up, Page Down, End | same |
| Fn + key right of Space | Right Ctrl | Right Ctrl |

`keychron-b1-pro-usjis.uf2` differs in these keys:

| Keys | Mac | Win |
| --- | --- | --- |
| Fn+Tab | US-JIS mode on / off (applies in Win only) | same |
| Keys beside Space | Tap: Eisu / Kana (IME off / on). Hold: Cmd | Tap: Muhenkan / Henkan. Hold: Alt |
| Caps Lock position | Left Ctrl | Left Ctrl |
| Left Ctrl position | Caps Lock | Caps Lock |

These are options in [config/keymap-options.h](config/keymap-options.h) (`B1_USJIS`, `B1_IME_TAP`, `B1_SWAP_CTRL_CAPS`), which the `-usjis` build takes as they are and the plain build turns off. Change them and rebuild, or change the keys in ZMK Studio. On Windows, the IME keys need one setting in Microsoft IME: assign Muhenkan to "IME-off" and Henkan to "IME-on" (設定 → 時刻と言語 → 言語と地域 → 日本語 → Microsoft IME → キーとタッチのカスタマイズ).

## ZMK Studio

Connect USB (cable position), open [zmk.studio](https://zmk.studio) in a browser that supports Web Serial, and press Fn+Del to unlock. Studio shows the B1 Pro's physical layout and all layers; changes are saved on the keyboard. Two spare layers are reserved for your own use. Besides ZMK's behaviors, Studio offers this firmware's: **Indicator** (battery level) and, in the `-usjis` build, **US-JIS** (Toggle, On, Off). Studio works over USB only, not over Bluetooth.

The five last positions of the layout (drawn small below the keys) are the Mac/Win switch, the two inputs of the connection switch and the charger signals. Leave them as they are.

## Connection switch and power

The switch reads BT / cable / 2.4G from left to right. The position alone decides the output; ZMK's automatic fallback between USB and Bluetooth is never used:

| Position | Output | Turns off |
| --- | --- | --- |
| BT | Bluetooth while the active profile is connected, else nothing | No |
| Cable | USB once the host has configured it, else nothing | 5 s after USB power is gone, as the stock firmware does |
| 2.4G | Nothing | After 300 ms in the position |

Turning off is nRF52840 System OFF:

| Reason | Wakes on |
| --- | --- |
| 2.4G position, cable position without USB | Moving the switch, plugging in USB |
| Low battery: below 3045 mV (the stock threshold), checked every 60 s, without USB power | Moving the switch, plugging in USB |
| Idle for 2 h without USB power (as the stock firmware) | A key, moving the switch, plugging in USB |

Waking restarts the keyboard. While it is off, only the board's pull-up resistors on the switch inputs and the battery voltage divider draw current.

## LEDs

The meanings follow the stock firmware:

| LED | Shows |
| --- | --- |
| All but Num Lock | On for 2.5 s after the keyboard starts |
| Blue (Bluetooth), BT position only | Pairing: slow blink for up to 60 s. Reconnecting: fast blink for up to 30 s. Connected: on for 3 s |
| Caps Lock | The host's Caps Lock state, as the host reports it |
| RGB | While Fn+B is held: battery level, green (70 % or more), blue (30 % or more), red (below). Otherwise three red flashes each minute while the battery is low; red while charging; green when charged (charging shows in the BT and cable positions) |

## US-JIS mode

In `keychron-b1-pro-usjis.uf2`, for a Windows host set to the Japanese keyboard layout (106/109). Fn+Tab switches it on or off. While it is on and the OS switch is in the Win position, the symbol keys type what the US keycaps show: Shift+2 types `@`, `=` types `=`, Shift+; types `:`, and so on (the table C01–C20 of [the spec](docs/usjis-substitution.md)). In the Mac position nothing is substituted; macOS does not need it. It is off at first; the mode is kept across power cycles. A change made while a key is held applies once all keys are released. No LED shows the mode: on a Japanese host the `=` key types `=` when it is on and `^` when it is off.

Holding keys together follows fixed rules, so that no key or modifier stays stuck; see the [spec](docs/usjis-substitution.md) and the [architecture](docs/usjis-architecture.md). The substitution table was worked out from the observable behavior of the US-key-on-JIS-OS key override of [Keyboard Quantizer](https://github.com/sekigon-gonnoc/vial-qmk); no code, table or comment of it is used.

## Building

You need Git and Docker; the toolchain runs in the pinned `zmkfirmware/zmk-build-arm:4.1` image. On Apple Silicon the image runs under emulation.

```sh
bash scripts/build-firmware.sh            # fetch the pinned sources (network), then build (offline)
bash scripts/build-firmware.sh build      # rebuild from the fetched sources, no network
```

The first run downloads the image and the pinned sources into `workspace/firmware/` (a few GB). Edit `config/keymap-options.h` or `config/keychron_b1_pro.keymap` before building to change the defaults. Each run builds both firmware files. Outputs in `build/firmware/`:

- `keychron-b1-pro.uf2` and `keychron-b1-pro-usjis.uf2` (also `.hex`, `.elf`, `.map`, and the `.config` and `.dts` of each build)
- `THIRD-PARTY-NOTICES.txt`: the components linked into the firmware, with their licenses, generated from the link map. The build fails if linked code carries a license the notice does not cover.
- `build-info.json`: the repository commit, the commit of every west project, the image digest, tool versions and the SHA-256 of the outputs.

Pins: ZMK `9ebbeff0a8b69a42f14aec022cdf16c7a107b9e0`, Zephyr `10ba6d0cb38bc3d258775d27982f707599320085` (v4.1.0+zmk-fixes), in [config/west.yml](config/west.yml); the image by digest in [scripts/build-firmware.sh](scripts/build-firmware.sh). The build date is the commit time, so two builds of the same commit give identical files.

## How it works

- **Board** (`boards/keychron/keychron_b1_pro/`): the nRF52840 with its internal RC oscillator (the board has no 32 kHz crystal), the key matrix, the five direct inputs, the LEDs, the battery divider and the physical layout for ZMK Studio.
- **Matrix without diodes** (`src/kscan_gpio_matrix_nodiode.c`, `src/ghost_filter.c`): a column is driven only while it is scanned, then discharged and left floating, and ghost keys (three corners of a rectangle make the fourth) are kept out; keys already pressed always release.
- **Connection switch and power** (`src/behavior_conn_switch.c`, `src/conn_policy.c`, `src/kb1_power.c`, `src/switch_waker.c`, `src/key_waker.c`): the output policy, ZMK's soft off, and the wake sources armed for the level opposite to the one at power-off.
- **Switches at power-on** (`src/early_events.c`): with ZMK Studio, ZMK's keymap gets its bindings only at the end of its initialization, after the key scan has started; a switch already on at power-on (Win, BT) would be dropped. These early events are held back and replayed.
- **LEDs and indicators** (`src/kb1_leds.c`, `src/led_logic.c`, `src/behavior_kb1_indicator.c`).
- **US-JIS** (`src/usjis.c`, `src/usjis_resolver.c`, `src/behavior_usjis.c`): a listener placed just before ZMK's `hid_listener`, checked at build time and at startup. It is built only when the keymap defines the `&usjis` behavior (`B1_USJIS`).

## Tests

```sh
bash scripts/run-host-tests.sh   # pure logic on the host; after a build also a devicetree check
bash scripts/run-zmk-tests.sh    # ZMK tests on native_sim (Docker, uses the build's workspace)
```

The host tests cover the ghost filter (fed with the readings a matrix without diodes really produces), the output policy and the LED logic. The ZMK tests build the whole ZMK application with this repository's code for ZMK's `native_sim` test board, feed it mock key events and compare every HID report it sends with the expected one: the US-JIS table C01–C20 and the scenarios of the spec (written from the spec by `tests/zmk/generate.py`), the B1 Pro's layers with Fn+Tab and the IME keys, and a switch held at power-on with ZMK Studio enabled. GitHub Actions runs the build and both test sets on every push.

## Repository layout

```text
.
├── .github/     workflows (build and tests, releases), Dependabot
├── boards/      the keychron_b1_pro board
├── config/      west manifest, keymap, keymap options, configuration
├── docs/        US-JIS spec and architecture
├── dts/         devicetree bindings of this repository's drivers and behaviors
├── include/     headers and dt-bindings
├── scripts/     build, notices and test runners
├── src/         drivers, behaviors and modules
├── tests/       host tests and ZMK tests
└── zephyr/      Zephyr module metadata
```

## Issues

Bug reports and pull requests are welcome, in Japanese or English. Please report security problems privately ([SECURITY.md](SECURITY.md)).

## License

This repository is under the [MIT License](LICENSE). The firmware built from it also contains ZMK (MIT), Zephyr (Apache-2.0) and other components; each release includes `THIRD-PARTY-NOTICES.txt` with their licenses.
