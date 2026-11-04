# Acaia Bridge {{VERSION}} for ESP32-S3

Firmware that makes an ESP32-S3 appear as an Acaia Lunar to the Profitec Move 2.0 and relays the
weight of another Bluetooth scale. Project, supported scales and pairing guide:
{{REPOSITORY_URL}}

## Contents

| File | Purpose |
|---|---|
| `acaia-bridge-esp32s3-{{VERSION}}-to_flash_via_usb.bin` | Complete image (bootloader, partition table, app). Flash at `0x0` over USB. |
| `acaia-bridge-esp32s3-{{VERSION}}-ota_update.bin` | App alone, for updates over Bluetooth. |
| `parts/` | The separate images plus `flash_args` and the ELF (for debugging). |
| `CHANGELOG.md` | What changed in this and earlier versions. |
| `LICENSE` | MIT license. The firmware comes without any warranty. |

## What you need

- An ESP32-S3 board (all have at least 4 MB flash).
- A USB data cable (not a charge-only cable). On boards with two USB-C ports, use the one marked
  **USB** (native USB), not **UART/COM**: the UART port is slower and less reliable here.

## Already on 1.1.0 or later? Update over Bluetooth

No cable needed: hold the adapter's BOOT button for 3 s (LED purple), open
https://omtqb4.github.io/Profitec-Move-2.0-BLE-Adapter-Acaia/ in Chrome, Edge or (iOS) Bluefy,
connect and install. Guide: {{REPOSITORY_URL}}/blob/main/docs/update-over-bluetooth.md

Coming from 1.0.x, flash once over USB as below and erase the flash first (Option A: tick
*Erase all*; Option B: run `erase-flash` before `write-flash`): 1.1.0 changed the flash layout.

## Option A: flash in the browser (no installation)

Works in Chrome or Edge on desktop (Web Serial). Safari and Firefox are not supported.

1. Open <https://espressif.github.io/esptool-js/>.
2. Baudrate `460800` → **Connect** → pick the board's serial port.
3. Change Flash Address from the pre-filled `0x1000` to **`0x0`** (at `0x1000` the ESP32-S3 doesn't
   boot), choose `acaia-bridge-esp32s3-{{VERSION}}-to_flash_via_usb.bin`.
4. **Program**, wait until the progress reaches 100 %, click **Disconnect**, then press **RST** on the board.

Detailed guide with troubleshooting: {{REPOSITORY_URL}}/blob/main/docs/flash-in-browser.md

## Option B: flash with esptool

With [uv](https://docs.astral.sh/uv/), `uvx esptool …` runs esptool without installing it.
Alternatively run `pip install esptool` and call `esptool` directly.

```sh
uvx esptool --chip esp32s3 -p PORT write-flash 0x0 acaia-bridge-esp32s3-{{VERSION}}-to_flash_via_usb.bin
```

Over a 1.0.x firmware, erase first: `uvx esptool --chip esp32s3 -p PORT erase-flash`.

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
   the offline mode. Guide: {{REPOSITORY_URL}}/blob/main/docs/pair-with-move.md
4. The LED turns **blue** while the machine is connected during a shot.

Later updates go over Bluetooth (see above); flashing over USB works too. Either way the adapter
keeps its Bluetooth identity (it comes from the chip), so the machine stays paired.

To see the log, open the serial port in any serial terminal, e.g. `screen PORT 115200`. After the
boot messages, the line `acaia-bridge X.Y.Z` shows the firmware version.
