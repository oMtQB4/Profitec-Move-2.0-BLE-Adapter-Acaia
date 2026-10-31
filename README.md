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

| Scale | Advertised name |
|---|---|
| Eureka Precisa | `CFS-9002` |
| Solo Barista (same protocol) | `LSJ-001` |

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

### Prebuilt firmware

CI (`.github/workflows/ci.yml`) runs the host tests and builds the firmware on every push and
pull request. Each run uploads an artifact with the binaries; `merged-binary.bin` flashes in one go:

```sh
uvx esptool --chip esp32s3 -p /dev/cu.usbmodem1101 write-flash 0x0 merged-binary.bin
```

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

1. Create `main/scale/drivers/MyScale.{hpp,cpp}`:

   ```cpp
   class MyScale : public scale::Driver {
   public:
       static constexpr const char* kName = "My Scale";
       static bool matches(const scale::Advertisement& adv) { return adv.name.starts_with("MYSCALE"); }

       std::string_view name() const override { return kName; }
       const scale::GattProfile& profile() const override;          // service / notify / write UUIDs
       void onNotify(std::span<const uint8_t> data) override;       // decode, then publish(Reading{...})
       bool send(scale::Command cmd, scale::Transport& t) override; // tare / timer
       // optional: onConnected() for a handshake, onTick() for heartbeats
   };
   ```

2. Register it in `main/scale/drivers/Drivers.cpp`: `registry.add<MyScale>();`
3. Add the `.cpp` to `main/CMakeLists.txt` (and `test/CMakeLists.txt` with a test).

## Protocol references

- Eureka Precisa: [Beanconqueror `eurekaPrecisaScale.ts`](https://github.com/graphefruit/Beanconqueror/blob/master/src/classes/devices/eurekaPrecisaScale.ts),
  [Decent de1app `bluetooth.tcl`](https://github.com/decentespresso/de1app/blob/main/de1plus/bluetooth.tcl)
- Acaia: [AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE), [pyacaia](https://github.com/lucapinello/pyacaia),
  [aioacaia](https://github.com/zweckj/aioacaia) (real Lunar 2021 GATT dump and frame captures)
