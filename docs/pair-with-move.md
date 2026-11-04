# Connect the bridge to the Move 2.0 and the app

After [flashing](flash-in-browser.md), the bridge has to be paired once with the Profitec Move 2.0
through the **Make Coffee Yours** app. From then on the machine connects to it by itself at every shot,
without the phone.

## How it fits together

- The bridge connects to your real scale first and only then appears as an Acaia Lunar named
  `LUNAR-xxxxxx` (the last six characters come from the board). While the scale is off, the Lunar
  is invisible.
- In offline mode the machine remembers one scale. The app writes that scale into the machine's
  settings. On iOS the app identifies the scale by its serial number, which the bridge reports as its
  Bluetooth address (e.g. `28372F80E382` for `LUNAR-80E382`).
- The emulated Lunar accepts **one connection at a time**: while the phone is connected to it, the
  machine can't be.

## Pairing (once)

1. **Power the bridge** near the machine (any USB charger) and **switch the scale on**.
   Wait for the LED to turn **yellow**: scale connected, Lunar visible.
2. **Open Make Coffee Yours** and connect to your Move 2.0 as usual.
3. **Connect the scale** in the app: pick `LUNAR-xxxxxx` from the list of scales.
4. **Enable offline mode**: in the settings turn **Offline mode** (*Offline-Modus*) on, or off and on
   again if it was already on, and tap **Save** (*Speichern*). This stores the bridge in the machine.
   The app then shows that `LUNAR-xxxxxx` is used for offline mode.
5. **Let go with the phone**: close the app or switch off the phone's Bluetooth, so the phone releases
   the Lunar.

## Brewing

1. Switch the scale on and wait for the yellow LED.
2. **Put the cup on the scale and wait a second**: the bridge tares the scale as soon as the cup is
   steady, so the display shows 0.
3. Start the shot. A few seconds in, the machine connects to the bridge: the LED turns **blue** and
   the weight starts counting from 0.
4. The machine stops at its target weight. Afterwards it disconnects; the LED goes back to yellow.
   Taking the cup off tares the scale again.

The machine connects only for the shot and sends no commands of its own, not even a tare. That is
why the bridge tares on the cup, like a Lunar in its espresso modes. If the cup wasn't tared (put
down very slowly, or the bridge was started with the cup already on the scale), the bridge zeroes the
weight when the machine connects instead.

## When to pair again

Repeat the pairing steps after anything that changes the bridge's identity:

- a different ESP32 board (new Bluetooth address, new `LUNAR-xxxxxx` name),
- a changed name prefix (`BRIDGE_NAME_PREFIX` in `make menuconfig`).

Flashing a newer release does **not** require pairing again: the identity comes from the chip.

## If it doesn't work

| Problem | Fix |
|---|---|
| `LUNAR-xxxxxx` doesn't appear in the app | The LED must be yellow: switch the scale on and wait. A red blinking LED means the bridge hasn't found the scale. |
| The machine doesn't connect during the shot (LED stays yellow) | The phone may still hold the connection: close the app or turn off the phone's Bluetooth. Then toggle **Offline mode** off/on and **Save** again in the app. |
| The app shows old or missing scale data | iOS caches the services of known devices: switch the phone's Bluetooth off and on once, then pair again. |
| The weight stops or jumps during the shot | Weak Bluetooth signal. Place the bridge within a metre or two of both the scale and the machine. |
| The scale doesn't tare when the cup is put down | Put it down in one go and wait a second; it needs at least 20 g. The log shows `cup placed or removed: weight zeroed`. |
| The weight doesn't start at 0 | Check that the LED turns blue during the shot; without a tared cup the bridge zeroes when the machine connects. |

For a look behind the scenes, open the serial log (see
[flash-in-browser.md](flash-in-browser.md#optional-watch-the-log)): it shows when the scale, the
phone or the machine connect and disconnect.
