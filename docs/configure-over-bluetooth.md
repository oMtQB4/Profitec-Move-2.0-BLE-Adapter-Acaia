# Change the settings over Bluetooth

How the adapter tares and what its LED shows can be changed from a web page, without rebuilding
the firmware. The settings are stored on the adapter and survive restarts and updates.

## You need

- The same browsers as for the [Bluetooth updater](update-over-bluetooth.md#you-need): Chrome or
  Edge on Windows, macOS, Linux or Android, or Bluefy on iPhone and iPad.
- Firmware with the configurator (the release that introduced it, or newer). Older firmware:
  update over Bluetooth first.

## Steps

1. **Make the adapter visible.** Switch the scale on and wait for the **yellow** LED, or put the
   adapter into update mode (**purple** LED, see the [updater guide](update-over-bluetooth.md)); the
   settings work in both. Close the Make Coffee Yours app: the adapter takes one connection at a time.
2. **Open the configurator:** <https://omtqb4.github.io/Profitec-Move-2.0-BLE-Adapter-Acaia/config/>
   and click **Connect**. Pick `LUNAR-xxxxxx` (or `AcaiaBridge-xxxxxx` in update mode).
3. **Change what you like.** Every change is saved on the adapter at once ("Saved on the adapter").
4. **Disconnect.** While the page is connected, the coffee machine can't connect. The page lets go
   by itself after 3 minutes without a change, and when you close it.

## Settings

| Setting | Choices | Default |
|---|---|---|
| Auto-tare | on, off | on |
| Auto-tare when | the cup has settled; the machine connects | the cup has settled |
| Tare the real scale too | on, off | on |
| Status LED | standard, only when active, off | standard |
| Brightness | 1–255 | 24 |

**Auto-tare** zeroes the weight the machine sees, because the Move 2.0 never sends a tare itself:

- *When the cup has settled*: put the cup down, wait a second, and the weight is zeroed. Taking the
  cup off zeroes too. Slow changes such as pouring don't trigger it. If the cup wasn't tared (put
  down very slowly, or already on the scale when the adapter started), the weight is zeroed when
  the machine connects instead.
- *When the machine connects*: the weight is zeroed the moment the machine connects, a few seconds
  into the shot. Whatever ran into the cup before that doesn't count.
- *Off*: the machine sees the weight as the scale reports it, cup included. Only useful if you tare
  the scale by hand before every shot.

**Tare the real scale too**: the adapter always zeroes the weight it sends to the machine
instantly, in software. With this on, it also sends a tare command to your scale, so its display
shows 0 as well. Turn it off if your scale's display should keep its own zero (for example to read
the total weight), or if the scale reacts badly to tare commands. The machine sees the same weight
either way.

**Status LED**:

- *Standard*: every state, as in the table below.
- *Only when active*: dark until the machine or another device connects to the adapter, then blue
  or green.
- *Off*: dark. Update mode still lights up purple, because you enter it on purpose and need to see it.

| LED | Meaning |
|---|---|
| red, blinking | searching for the scale |
| yellow | scale connected, waiting for the machine |
| blue | the coffee machine is connected |
| green | another device is connected (an app, the configurator) |
| purple | update mode |

## How the adapter recognises the machine

The adapter tells the Move 2.0 apart from phones, apps and the configurator, which also connect
to it as `LUNAR-xxxxxx`. That decides the LED colour and, with *When the machine connects*, who
triggers the tare.

- **By its behaviour**: the Move subscribes to the weight and never sends anything: no ident, no
  heartbeat, no tare. Acaia apps send an ident right after connecting; the configurator never
  subscribes to the weight. A client that subscribes and stays silent for 3 seconds is the machine.
- **By its address**: 3 seconds is too late for the tare at the start of a shot. So the adapter
  remembers the machine's Bluetooth address the first time it recognises it and from then on knows
  the machine the moment it connects. The configurator shows the remembered address under
  *Machine*.

Until the address is known, every connecting client that might be the machine gets the tare, as
before the configurator existed. Phones and computers change their address every few minutes, so
the adapter never remembers one of those as the machine.

**Forget machine** (in the configurator) clears the remembered address; the adapter learns it again
at the next shot. Use it after replacing the machine: a new machine that connects while the old
address is remembered is only recognised after 3 seconds, too late for the tare at connect.

## How it works

For developers: the protocol is documented in
[`main/config/ConfigProtocol.hpp`](../main/config/ConfigProtocol.hpp) and implemented by the page
in [`web/configurator/protocol.js`](../web/configurator/protocol.js).

- A GATT service (`7e1a0010-…`) with Settings (read, write, notify), Status (read, notify) and
  Control (write) characteristics, in both boot modes. It comes after the Acaia, Device Information
  and update services, so their attribute handles don't move.
- Values are `<key> <length> <value>` records. Both sides skip keys they don't know, so newer pages
  work with older firmware and the other way round. A write may carry only the changed settings; an
  invalid write changes nothing.
- Settings are stored in NVS (namespace `config`) in the same record format; the Kconfig options
  (`make menuconfig`) only set the defaults before the first change. The learned machine address is
  stored next to them.
- Security: anyone within Bluetooth range can change the settings while the adapter is visible and
  not connected to the machine. They control only tare and LED behaviour.
