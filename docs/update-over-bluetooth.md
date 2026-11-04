# Update the firmware over Bluetooth

From version 2.0 on, the adapter installs new firmware over Bluetooth, straight from a web page.
No cable, no installation, and the adapter can stay where it is.

> Coming from version 1.x? Flash 2.0 **once over USB** ([guide](flash-in-browser.md)): 2.0 changes
> the flash layout so that the adapter can hold two firmware versions. All later updates work over
> Bluetooth.

## You need

- **Chrome or Edge** on Windows, macOS, Linux or Android. Brave works after enabling
  `brave://flags/#brave-web-bluetooth-api`. On iPhone and iPad, use the free
  [Bluefy](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) browser: Safari has no
  Web Bluetooth. Firefox doesn't support it either.
- The adapter within a few metres.

## Steps

1. **Open the updater:** <https://omtqb4.github.io/Profitec-Move-2.0-BLE-Adapter-Acaia/>.
   It offers the latest release; to install another version, download its `…-ota_update.bin` from the
   [releases](https://github.com/oMtQB4/Profitec-Move-2.0-BLE-Adapter-Acaia/releases) and choose
   the file instead.
2. **Put the adapter into update mode.** The LED turns **purple**.
   - Hold the **BOOT** button for 3 seconds, or
   - without access to the button: plug the adapter in, unplug it again within a few seconds,
     and repeat until it has been plugged in **3 times in a row**. It starts in update mode at the
     third time.

   In update mode the bridge is off: the machine sees no scale. Update mode ends by itself after
   5 minutes without an update, when you hold BOOT for 3 seconds again, or with any restart (also
   a reset from a computer that opens the adapter's USB serial port).

   The power-on sequence counts power-ons, not pauses: a power-on counts if the power is cut again
   within 10 seconds; after 10 seconds of running the count starts over. How long the adapter was
   unplugged doesn't matter (it has no clock that runs without power). If it ever starts in update
   mode by accident, unplug and replug it once.
3. **Connect.** Click **Connect** and pick `AcaiaBridge-xxxxxx` in the browser's list. The page
   shows the installed version. Computers and phones that saw the adapter before may list it under
   its old name `LUNAR-xxxxxx`; the list only shows adapters in update mode, so that's the one.
4. **Install.** Click **Install** and keep the page open. The LED blinks purple while firmware
   arrives; a transfer takes roughly 15 to 60 seconds, depending on the phone or computer.
5. **Done.** The adapter checks the new firmware, restarts and the LED blinks red until the scale
   is connected. The machine stays paired: the adapter's Bluetooth identity doesn't change.

## What can go wrong (and why it's safe)

The adapter holds two firmware slots. An update always goes into the slot that is **not** running,
so the running firmware is never touched while data arrives.

| Situation | What happens |
|---|---|
| Connection lost, page closed, cancelled, power cut during the transfer | The half-written slot is ignored. The adapter keeps running the current firmware. Start again. |
| The file is damaged | The adapter verifies the image (SHA-256) before switching slots and rejects it. |
| Wrong file (another project, another chip, the `-to_flash_via_usb.bin`) | Rejected by the page and again by the adapter before anything is written. |
| The new firmware crashes or hangs after the restart | It has 10 seconds to start up properly and confirm itself. Otherwise the next restart (automatic after at most 60 s) goes back to the previous firmware. |

Only cut the power in the first 10 seconds after an update if you want to go back: an unconfirmed
new firmware counts as failed.

## Troubleshooting

| Problem | Fix |
|---|---|
| Nothing happens on **Connect**, or "the page didn't load completely" | The page was opened as a file (`file://`). Open the web address above, or run `make updater` and open <http://localhost:8000>. |
| **Brave**: "Brave has Web Bluetooth switched off" | Open `brave://flags/#brave-web-bluetooth-api`, set it to *Enabled*, relaunch Brave. |
| "This browser can't reach Bluetooth" | Switch Bluetooth on. macOS: System Settings → Privacy & Security → Bluetooth, allow Chrome/Edge, restart the browser. |
| The page says the browser can't use Bluetooth | Use Chrome or Edge (desktop or Android) or Bluefy on iOS. On Linux, enable `chrome://flags/#enable-web-bluetooth` if needed. |
| `AcaiaBridge-…` doesn't show up in the list | Is the LED purple? If not, enter update mode again. Close the Make Coffee Yours app and other apps connected to the adapter. On iOS, toggle Bluetooth off and on once. |
| "The adapter is not in update mode" | The LED isn't purple: hold BOOT for 3 seconds (or use the power-on sequence) and connect again. |
| The transfer stops or fails repeatedly | Move closer, keep the screen on, try another device. A failed transfer never harms the installed firmware. |
| Nothing works anymore | Flash over USB as in [flash-in-browser.md](flash-in-browser.md); that always works. |

## How it works

For developers: the protocol is documented in [`main/ota/OtaProtocol.hpp`](../main/ota/OtaProtocol.hpp)
and implemented by the page in [`web/updater/protocol.js`](../web/updater/protocol.js).

- A GATT service (`7e1a0001-…`) with Info, Control and Data characteristics. It is part of the
  GATT table in both modes, after the Acaia and Device Information services, so the table never
  changes shape and their attribute handles stay put. Outside update mode it refuses to start an
  update.
- The client sends `Begin` with the image size, then numbered data packets of up to 510 bytes
  (write without response), at most a window of 16 packets ahead of the adapter's
  acknowledgements, then `End`. The adapter writes with `esp_ota_write` (erasing sector by sector),
  verifies with `esp_ota_end` and switches slots with `esp_ota_set_boot_partition`.
- Before anything is written, the adapter checks the image header: magic byte, chip
  (ESP32-S3) and project name (`acaia-bridge`).
- Rollback: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`. `ota::AppValidity` confirms a new image after
  10 s of running main loop.
- Security: an update needs physical access (button or power sequence) and is only accepted for
  5 minutes. The image is not signed: someone within Bluetooth range during those 5 minutes could
  install other firmware for this project.

## Flash layout

`partitions.csv`, fixed for all versions from 2.0 on (only a USB flash could change it):

| Partition | Offset | Size | Purpose |
|---|---|---|---|
| bootloader | `0x0` | 32 KB | second-stage bootloader with rollback |
| partition table | `0x8000` | 4 KB | |
| nvs | `0x9000` | 80 KB | settings, counters, Bluetooth data |
| otadata | `0x1D000` | 8 KB | which slot boots, rollback state |
| phy_init | `0x1F000` | 4 KB | radio calibration data (optional) |
| ota_0 | `0x20000` | 1792 KB | firmware slot A |
| ota_1 | `0x1E0000` | 1792 KB | firmware slot B |
| storage | `0x3A0000` | 320 KB | reserved for a file system |
| coredump | `0x3F0000` | 64 KB | last crash (`idf.py coredump-info`) |

It fits the smallest ESP32-S3 flash (4 MB); larger flash stays unused. The firmware takes about
0.6 MB, so each slot has room for roughly three times the current size.
