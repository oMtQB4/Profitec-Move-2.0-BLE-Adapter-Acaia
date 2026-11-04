# Flash the firmware in the browser

Espressif's [ESP Tool](https://espressif.github.io/esptool-js/) flashes the ESP32-S3 straight from the
browser. Nothing to install, and the firmware file never leaves your computer.

## You need

- **Chrome, Edge or Opera on a desktop computer** (Windows, macOS, Linux). Safari and Firefox can't
  access serial ports; use the [command line](../README.md#flashing-a-release) there.
- The ESP32-S3 board and a **USB data cable**. On boards with two USB-C ports use the one labelled
  **USB**, not **COM/UART**.
- The firmware: `acaia-bridge-esp32s3-vX.Y.Z-to_flash_via_usb.bin` from the
  [latest release](https://github.com/oMtQB4/Profitec-Move-2.0-BLE-Adapter-Acaia/releases/latest).

## Steps

1. **Plug in the board** and open <https://espressif.github.io/esptool-js/>.
2. **Connect.** Under *Program*, set *Baudrate* to `460800`, leave *WebUSB (CH340)* unticked and click
   **Connect**. The browser lists the serial ports; pick the board:
   - native USB port: "USB JTAG/serial debug unit" (macOS `cu.usbmodem…`, Windows `COMx`)
   - COM/UART port: e.g. "USB Single Serial" or "CP2102"

   The page then shows *Connected to device: ESP32-S3 …*.
3. **Set the address to `0x0`.** The file table has one row. Its *Flash Address* is pre-filled with
   `0x1000` (correct for the original ESP32 only). **Change it to `0x0`**: at `0x1000` the ESP32-S3
   doesn't boot.
4. **Choose the file**: click the file field in that row and select the `…-to_flash_via_usb.bin`.
   Leave *Flash Mode* `keep` and *Flash Size* `detect`. Tick *Erase all* only when you install 1.1.0
   or later over a 1.0.x firmware (the flash layout changed); otherwise leave it unticked.
5. **Program.** Click **Program** and wait until the progress bar reaches 100 % and the log stops.
6. **Restart.** Click **Disconnect**, then press **RST** on the board (or unplug and replug it).

The status LED now blinks red: the bridge is waiting for your scale. From now on you can install
updates [over Bluetooth](update-over-bluetooth.md) instead. Continue with
[Connect the bridge to the Move 2.0 and the app](pair-with-move.md).

## Optional: watch the log

Under *Console* click **Start**, pick the same port and press **Reset**. After the boot messages a line
`acaia-bridge X.Y.Z` names the firmware version; switching the scale on prints the connection.

## If it doesn't work

| Problem | Fix |
|---|---|
| The board is not in the port list | Charge-only cable or wrong port: use a data cable and the **USB** port. |
| "Failed to connect" / timeout | Put the board into download mode: hold **BOOT**, press and release **RST**, release **BOOT**, then click **Connect** again. |
| Errors during Program | Disconnect, choose a lower baudrate (`115200`), connect and program again. |
| LED stays dark, log shows boot errors | Most likely flashed at `0x1000`: flash again at `0x0` with *Erase all* ticked. |
| Windows: COM/UART port has no driver | Use the **USB** port, or install the board's USB-serial driver (CH343 or CP210x). |
