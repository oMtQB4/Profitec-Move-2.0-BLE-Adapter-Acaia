# Profitec-Move-2.0-BLE-Adapter-Acaia

[![CI](https://github.com/oMtQB4/Profitec-Move-2.0-BLE-Adapter-Acaia/actions/workflows/ci.yml/badge.svg)](https://github.com/oMtQB4/Profitec-Move-2.0-BLE-Adapter-Acaia/actions/workflows/ci.yml)


ESP32-S3 firmware that makes any supported BLE coffee scale look like an Acaia Lunar.

The Profitec Move 2.0 only does brew-by-weight with Acaia scales. This adapter connects to
your real scale as a BLE central, advertises itself as `LUNAR-xxxxxx` towards the machine
and relays in both directions:

```
 real scale  ──notify weight──▶  ESP32-S3  ──Acaia weight events──▶  Profitec Move 2.0
 (Eureka …)  ◀──tare / timer───  (bridge)  ◀──tare / timer cmds────  (thinks: Acaia Lunar)
```

The emulated Lunar only advertises while a real scale is connected, like a scale that is
switched off (configurable).

## Status

Working end to end: Eureka Precisa → adapter → Profitec Move 2.0 in offline mode, shot stops
at the target weight.

## Pairing with the Move 2.0 (offline mode)

The machine connects to the scale stored in its offline-mode setting. The Make Coffee Yours
app writes that setting, and it identifies the scale through the scale's Device Information
Service: on iOS the app cannot see Bluetooth addresses, so it derives the address from the
**serial number** (the adapter reports its Bluetooth address as hex, e.g. `28372F80E382`).

1. Switch on the real scale and wait for the LED to turn yellow.
2. In the app, connect the machine and the scale `LUNAR-xxxxxx`.
3. Settings → turn **Offline-Modus** on (or off and on again) → **Speichern**.
4. Close the app or turn off Bluetooth on the phone: the emulated Lunar accepts one
   connection at a time, and a connected phone hides it from the machine.

During a shot the Move only connects and listens to the weight: it sends no tare, timer or
heartbeat. The adapter therefore zeroes the weight itself when the machine connects and tares the real scale
(`BRIDGE_TARE_ON_CLIENT_CONNECT`), so a cup on the scale doesn't count towards the target.

Re-pair after anything changes the adapter's identity (another ESP32 board, a different
`BRIDGE_NAME_PREFIX`). If the phone saw an older firmware without the Device Information
Service, toggle the phone's Bluetooth once so iOS refreshes its cached services.

## Supported scales

Only the Eureka Precisa has been tested with a real device. All other drivers are
**integrated but not tested with a real device**: they implement the protocols documented by the
open-source projects listed under [Protocol references](#protocol-references) and pass unit tests
with the frames from those sources, but nobody has run them against the actual scale yet. Reports
(serial log with `BRIDGE_LOG_SCAN_RESULTS` on) are welcome.

"Move app" marks scales Profitec lists as compatible with the Move 2.0 in app mode.

| Scale | Advertised name / match | Move app | Tare | Timer | Protocol confidence | Status |
|---|---|---|---|---|---|---|
| Eureka Precisa | `CFS-9002` | yes | yes | yes | 2 sources | **tested on hardware** |
| Solo Barista | `LSJ-001` | yes | yes | yes | 2 sources (same as Eureka) | integrated, not tested |
| Decent Scale (+ ButtsHaus clones) | `Decent Scale…`, `ButtsHaus Scale…` | yes | yes | yes | 2 sources | integrated, not tested |
| EspressiScale (DIY) | `EspressiScale` | – | yes | yes | 2 sources + firmware | integrated, not tested |
| DiFluid Microbalance / Microbalance Ti | service `00EE` / `00DD`, or name `Microbalance…` | yes | yes | start/stop | 3 sources incl. vendor doc | integrated, not tested |
| Felicita Arc / Parallel / Incline | contains `felicita` | yes | yes | yes | 2 sources | integrated, not tested |
| Hiroia Jimmy | `HIROIA…` | yes | yes | – | 2 sources | integrated, not tested |
| Skale II (Atomax) | `Skale…` | yes | yes | yes | 2 sources | integrated, not tested |
| Varia AKU / AKU Mini | contains `Varia AKU` / `AKU MINI` | yes | yes | yes | 2 sources | integrated, not tested |
| Goat Story Gina | service `91341521-…` | yes | software | – | 1 source | integrated, not tested |
| Bookoo Themis Mini / Ultra | contains `BOOKOO_SC` | – | yes | yes | 4 sources incl. vendor doc | integrated, not tested |
| Timemore Black Mirror | contains `Timemore Scale` | – | yes | yes | 1 source | integrated, not tested |
| Timemore Basic 3 | `Basic3`, `Basic 3`, `Timemore…Basic` | – | yes | yes | 1 source | integrated, not tested |
| Timemore Dot | `TES017`, `Timemore…Dot` | – | yes | yes | 2 sources (commands differ) | integrated, not tested |
| Smartchef | contains `smartchef` | – | software | – | 2 sources | integrated, not tested |
| Blackcoffee.io | contains `blackcoffee` / `my_scale` | – | software | – | 1 source | integrated, not tested |
| Futula Kitchen Scale 3 (LEFU CK811) | contains `LFSmart Scale`, `LEFU-CK811…` | – | yes | – | 3 sources | integrated, not tested |
| Mantabrew WeighMaster 2.0 | contains `WeighMaster` / `Mantabrew` | – | yes | yes | 1 source | integrated, not tested |
| WeighMyBru (DIY) | `WeighMyBru…` | – | yes | yes | 2 sources + firmware | integrated, not tested |
| DIY mpy-coffee / coffee-scale (beeb) | `mpy-coffee` / `coffee-scale` | – | software | – | 2 sources + firmware | integrated, not tested |

"software" tare: the scale accepts no tare command; the adapter zeroes the weight itself when the
machine connects (`BRIDGE_TARE_ON_CLIENT_CONNECT`), which is all the Move 2.0 needs.

**Not supported**
- **Ikape scale, Brewista X** (on the Move app list): no public BLE protocol found. A Bluetooth
  capture (Android HCI snoop log) with the vendor app would be needed.
- **Acaia scales** need no adapter: the Move 2.0 pairs with them directly.

## Build

ESP-IDF **v5.4.4**. By default the Makefile uses a local install at `~/esp/esp-idf-v5.4.4`
(override with `IDF_DIR=...`).

```sh
make build           # firmware → build/
make test            # host unit tests (protocols, drivers)
make menuconfig      # "Acaia Bridge" menu for project options
make shell           # shell with the IDF environment loaded
```

Without a local install, add `DOCKER=1` to any target to use the official image
`espressif/idf:v5.4.4` (output goes to `build-docker/`).

## Flash and monitor

```sh
make flash                             # auto-detects /dev/cu.usbmodem*
make flash PORT=/dev/cu.usbmodem1101
make monitor                           # idf.py monitor; quit with Ctrl-]
make flash-monitor
```

With `DOCKER=1`, flashing runs on the host via [esptool](https://github.com/espressif/esptool)
(`uvx esptool`, or set `ESPTOOL=esptool`) and `monitor` uses `screen` (quit with Ctrl-A K),
because Docker Desktop on macOS cannot pass USB devices into containers.

## Flashing a release

No ESP-IDF needed. Each [release](../../releases) has two assets:

- `acaia-bridge-esp32s3-vX.Y.Z-merged.bin`: the complete image, flashed at address `0x0`.
- `acaia-bridge-esp32s3-vX.Y.Z.zip`: the same image plus a flashing guide (README), the changelog
  and the separate images with `flash_args`.

Connect the board with a USB data cable. On boards with two USB-C ports use the native **USB**
port, not **UART/COM**.

**In the browser** (Chrome or Edge on desktop):

1. Open [ESP Tool](https://espressif.github.io/esptool-js/), baudrate `460800`, **Connect**, pick the port.
2. Flash Address `0x0`, choose the `-merged.bin` file, **Program**.
3. Press **RESET** on the board when it is done.

**With esptool** (`uvx esptool` runs it via [uv](https://docs.astral.sh/uv/); or `pip install esptool`):

```sh
uvx esptool --chip esp32s3 -p PORT write-flash 0x0 acaia-bridge-esp32s3-vX.Y.Z-merged.bin
```

`PORT` is `/dev/cu.usbmodem*` on macOS, `/dev/ttyACM0` on Linux, `COMx` on Windows.

If the board is not detected, enter download mode by hand: hold **BOOT**, press and release
**RESET**, release **BOOT**, flash again, then press **RESET**.

After flashing, the LED blinks red until the scale is connected (see [Status LED](#status-led)).
Updating to a newer release works the same way and keeps the pairing with the machine: the
adapter's Bluetooth identity comes from the chip, not from the firmware.

CI (`.github/workflows/ci.yml`) also builds the firmware on every push and pull request and
uploads the same files as a workflow artifact (`merged-binary.bin`), e.g. for testing a branch.

## Releases and versioning

Versions follow [Semantic Versioning](https://semver.org) and are derived from
[Conventional Commits](https://www.conventionalcommits.org) by
[release-please](https://github.com/googleapis/release-please):

1. Every push to `main` (after tests and build pass) updates a release pull request that bumps
   `version.txt` and adds the new entries to `CHANGELOG.md`.
2. Merging that pull request raises the version; CI then tags `vX.Y.Z`, creates the GitHub release
   and attaches the firmware built from that commit. The flashing guide inside the zip is
   `release/README.md` (`{{VERSION}}` and `{{REPOSITORY_URL}}` are filled in by CI).

| Commit | Version bump |
|---|---|
| `fix: …` | patch |
| `feat: …` | minor |
| `feat!: …` or a `BREAKING CHANGE:` footer | major (minor while the version is 0.x) |
| `docs:`, `ci:`, `build:`, `refactor:`, `test:`, `chore:` | none, hidden from the changelog |

ESP-IDF reads `version.txt` as the app version, so the firmware logs it at boot
(`acaia-bridge 0.1.0`). Don't edit `version.txt` or `CHANGELOG.md` by hand; release-please owns them.

The repository needs *Settings → Actions → General → Workflow permissions*: "Read and write
permissions" and "Allow GitHub Actions to create and approve pull requests".

## Configuration (`make menuconfig` → Acaia Bridge)

| Option | Default | Meaning |
|---|---|---|
| `BRIDGE_NAME_PREFIX` | `LUNAR-` | advertised name prefix, MAC suffix appended |
| `BRIDGE_ADVERTISE_WITHOUT_SCALE` | off | advertise even with no real scale, to test the machine side alone |
| `BRIDGE_TARE_ON_CLIENT_CONNECT` | on | weight reads 0 when the machine connects (shot start), like a Lunar in auto-tare mode; zeroes instantly in software, then tares the real scale too |
| `BRIDGE_STATUS_LED` | on | status on an addressable RGB LED (WS2812), see below |
| `BRIDGE_STATUS_LED_GPIO` | 48 | LED pin (DevKitC-1 v1.0: 48, v1.1: 38) |
| `BRIDGE_STATUS_LED_BRIGHTNESS` | 24 | 1–255 |
| `BRIDGE_LOG_SCAN_RESULTS` | off | log every named BLE device seen while scanning (find the name of a new scale) |
| `BRIDGE_LOG_ACAIA_FRAMES` | on | hex-log machine ↔ adapter frames |

## Status LED

| LED | Meaning |
|---|---|
| red, blinking | searching for the real scale; emulated Lunar hidden |
| yellow | scale connected; Lunar visible, waiting for the coffee machine |
| blue | coffee machine connected; weight is being relayed |

## Architecture

```
main/
├── main.cpp                    composition root, app loop
├── bridge/Bridge               mediator: scale readings → Acaia, Acaia commands → scale
├── status/                     status LED: LedPattern (pure, host-tested) + StatusLed (WS2812)
├── acaia/
│   ├── AcaiaProtocol           pure frame encode/decode (host-tested)
│   └── AcaiaPeripheral         NimBLE GATT server posing as a Lunar
└── scale/
    ├── Driver.hpp              Driver (strategy), Listener (observer), Transport
    ├── DriverRegistry.hpp      factory: advertisement → driver
    ├── ScaleClient             NimBLE central: scan, connect, subscribe, feed driver
    └── drivers/                one class per scale model + Drivers.cpp registration
```

- **Drivers know no BLE.** A driver declares its GATT profile and translates bytes.
  `ScaleClient` does scanning, connecting and subscribing for every driver, and drivers
  write through the `Transport` interface. Drivers are therefore tested on the host with
  a fake transport.
- **One app task.** NimBLE callbacks only queue events. `ScaleClient::poll()` and
  `Bridge::poll()` run on the app task, so drivers and the bridge never run concurrently.
- BLE: [esp-nimble-cpp](https://github.com/h2zero/esp-nimble-cpp) (C++ wrapper over NimBLE).

## Adding a scale

1. Create `main/scale/drivers/MyScale.{hpp,cpp}`. Most drivers decode without state and derive
   from `DecodingDriver`, which supplies `name()` and `onNotify()`:

   ```cpp
   class MyScale : public scale::DecodingDriver<MyScale> {
   public:
       static constexpr const char* kName = "My Scale";
       static bool matches(const scale::Advertisement& adv);              // name and/or payload (service UUIDs)
       static std::optional<scale::Reading> decode(std::span<const uint8_t> data); // 0.1 g

       const scale::GattProfile& profile() const override;          // service / notify / write (nullptr = no commands)
       bool send(scale::Command cmd, scale::Transport& t) override; // tare / timer; false if unsupported

   protected:
       void onConnected(scale::Transport& t) override;              // optional handshake
       void onTick(scale::Transport& t, uint32_t nowMs) override;   // optional heartbeat
   };
   ```

   Helpers: `schedule(delayMs, bytes)` and `sendTwice(transport, bytes)` for timed writes,
   `transport.read(uuid)` for reading a characteristic, `scale/Bytes.hpp` for byte order and
   checksums, `scale/NameMatch.hpp` for case-insensitive names, `advertisesService16/128()` in
   `scale/AdvertisingData.hpp`, `fromCentigrams()`/`fromGrams()` for other resolutions.
   Drivers must not include ESP-IDF headers, so they stay testable on the host.
2. Register it in `main/scale/drivers/Drivers.cpp` (`registry.add<MyScale>();`); order matters,
   the first matching driver wins.
3. Add tests with frames from the protocol source (see `test/test_drivers_*.cpp`) and a sample
   name in `test/test_driver_registry.cpp`. Driver files are globbed: run `make fullclean build`
   (or `idf.py reconfigure`) once after adding a file.

## Protocol references

The drivers re-implement byte formats documented by these projects; no code was copied.

- [Beanconqueror](https://github.com/graphefruit/Beanconqueror) (`src/classes/devices/`): most scales
- [Decent de1app](https://github.com/decentespresso/de1app) (`de1plus/bluetooth.tcl`, `machine.tcl`, `binary.tcl`)
- Vendor documentation: [Bookoo](https://github.com/BooKooCode/OpenSource), DiFluid (`difluid-sdk-demo`, `protocolMicrobalance.md`)
- Firmware / apps: [EspressiScale](https://github.com/Whauu/EspressiScale), [weighmybru2](https://github.com/031devstudios/weighmybru2),
  [beeb/coffee-scale-app](https://github.com/beeb/coffee-scale-app), [FutulaCoffeeScale](https://github.com/wdrs/FutulaCoffeeScale),
  [Pourista](https://github.com/DrSkyFly/Pourista), [talla](https://github.com/talla-speciality/talla) (Goat Story Gina)
- Acaia: [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE), [pyacaia](https://github.com/lucapinello/pyacaia),
  [aioacaia](https://github.com/zweckj/aioacaia) (real Lunar 2021 GATT dump and frame captures)
