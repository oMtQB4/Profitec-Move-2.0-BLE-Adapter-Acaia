# Acaia Bridge {{VERSION}} for ESP32-S3

Firmware that makes an ESP32-S3 appear as an Acaia Lunar to the Profitec Move 2.0 and relays the
weight of another Bluetooth scale. Project, supported scales and pairing guide:
{{REPOSITORY_URL}}

## Contents

| File | Purpose |
|---|---|
| `acaia-bridge-esp32s3-{{VERSION}}-merged.bin` | Complete image (bootloader, partition table, app). Flash at `0x0`. |
| `parts/` | The separate images plus `flash_args` and the ELF (for debugging). |
| `CHANGELOG.md` | What changed in this and earlier versions. |
| `LICENSE` | MIT license. The firmware comes without any warranty. |

## What you need

- An ESP32-S3 board with at least 2 MB flash.
- A USB data cable (not a charge-only cable). On boards with two USB-C ports, use the one marked
  **USB** (native USB), not **UART/COM**: the UART port is slower and less reliable here.

## Option A: flash in the browser (no installation)

Works in Chrome or Edge on desktop (Web Serial). Safari and Firefox are not supported.

1. Open <https://espressif.github.io/esptool-js/>.
2. Baudrate `460800` → **Connect** → pick the board's serial port.
3. Flash Address `0x0`, choose `acaia-bridge-esp32s3-{{VERSION}}-merged.bin`.
4. **Program**, wait for "Leaving…", then press the board's **RESET** button.

## Option B: flash with esptool

With [uv](https://docs.astral.sh/uv/), `uvx esptool …` runs esptool without installing it.
Alternatively run `pip install esptool` and call `esptool` directly.

```sh
uvx esptool --chip esp32s3 -p PORT write-flash 0x0 acaia-bridge-esp32s3-{{VERSION}}-merged.bin
```

`PORT` is the board's serial port:

| System | Typical port | Find it |
|---|---|---|
| macOS | `/dev/cu.usbmodem1101` | `ls /dev/cu.usbmodem*` |
| Linux | `/dev/ttyACM0` | `ls /dev/ttyACM*` (user needs the `dialout` group) |
| Windows | `COM5` | Device Manager → Ports (COM & LPT) |

Flashing the separate images instead (same result):

```sh
cd parts
uvx esptool --chip esp32s3 -p PORT write-flash @flash_args
```

## If the board is not found

Put it into download mode by hand: hold **BOOT**, press and release **RESET**, release **BOOT**,
then flash again. Press **RESET** afterwards to start the firmware.

## After flashing

1. The LED blinks **red**: no scale connected yet.
2. Switch on your scale. The LED turns **yellow** once the adapter is connected to it, and the
   adapter appears as `LUNAR-xxxxxx`.
3. First use only: pair the Move 2.0 with `LUNAR-xxxxxx` in the Make Coffee Yours app and enable
   the offline mode (see "Pairing with the Move 2.0" in the project README).
4. The LED turns **blue** while the machine is connected during a shot.

Updating to a newer release works the same way. The adapter keeps its Bluetooth identity (it comes
from the chip), so the machine stays paired.

To see the log, open the serial port in any serial terminal, e.g. `screen PORT 115200`. After the
boot messages, the line `acaia-bridge X.Y.Z` shows the firmware version.
