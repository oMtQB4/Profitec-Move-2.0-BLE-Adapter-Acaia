// Page logic of the Bluetooth configurator; the protocol lives in protocol.js.
import {
  ACAIA_SERVICE_UUID,
  AdapterMode,
  CONTROL_UUID,
  ConfigError,
  ControlOp,
  PROTOCOL_VERSION,
  SERVICE_UUID,
  SETTINGS_UUID,
  STATUS_UUID,
  UPDATE_SERVICE_UUID,
  encodeSettings,
  parseSettings,
  parseStatus,
  settingsMatch,
} from "./protocol.js";

const $ = (id) => document.getElementById(id);
window.configuratorLoaded = true;

/** The machine can't connect while this page holds the adapter's only free connection. */
const IDLE_DISCONNECT_MS = 3 * 60 * 1000;
/** The adapter applies a write on its next main loop pass (10 ms); give it some slack. */
const APPLY_DELAY_MS = 150;

const state = {
  device: null,    // BluetoothDevice
  chars: null,     // { settings, status, control }
  settings: null,
  status: null,
  supported: true,
  leaving: false,  // disconnect requested by the page or the user
  queue: Promise.resolve(), // Web Bluetooth allows one GATT operation at a time
  reading: { settings: false, status: false },
  idleTimer: null,
  savedTimer: null,
};

const toBytes = (dataView) => new Uint8Array(dataView.buffer.slice(dataView.byteOffset, dataView.byteOffset + dataView.byteLength));
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/** Runs GATT operations one after another. */
function gatt(operation) {
  const run = state.queue.then(operation);
  state.queue = run.catch(() => {});
  return run;
}

function showConnectError(message) {
  const box = $("connect-result");
  box.hidden = !message;
  box.textContent = message ?? "";
}

function showSaved(kind, message) {
  const box = $("saved");
  clearTimeout(state.savedTimer);
  box.hidden = !message;
  box.className = `notice ${kind}`;
  box.textContent = message ?? "";
  if (message && kind === "ok") {
    state.savedTimer = setTimeout(() => (box.hidden = true), 2500);
  }
}

/** Turns requestDevice()/connect() failures into something a user can act on. */
function connectErrorMessage(error) {
  if (error instanceof ConfigError) return error.message;
  if (/adapter not available|bluetooth.*(off|unavailable|not available)/i.test(error.message)) {
    return (
      "This browser can't reach Bluetooth. Switch Bluetooth on. On macOS also allow it under " +
      "System Settings → Privacy & Security → Bluetooth for your browser, then restart the browser."
    );
  }
  if (error.name === "SecurityError") {
    return "Bluetooth only works on secure pages: open the configurator via https:// or http://localhost.";
  }
  if (error.name === "NotAllowedError") {
    return "Bluetooth access was blocked. Allow Bluetooth for this site in the browser settings and try again.";
  }
  return `Connecting failed (${error.name}: ${error.message}). Reload the page and try again.`;
}

function describeScale(status) {
  if (status.mode === AdapterMode.Update) return "– (the bridge is off in update mode)";
  return status.scaleConnected ? status.scaleName || "connected" : "not connected";
}

function describeMachine(status) {
  return status.machine
    ? `${status.machine}, recognised by its Bluetooth address`
    : "not known yet: the adapter learns it at the next shot";
}

function render() {
  const connected = Boolean(state.chars && state.settings && state.status);
  $("adapter-summary").hidden = !connected;
  $("busy-note").hidden = !connected || state.status?.mode === AdapterMode.Update;
  $("connect").hidden = connected;
  $("connect").disabled = !state.supported;
  $("disconnect").hidden = !connected;
  $("forget").hidden = !connected || !state.status?.machine;
  for (const id of ["tare-step", "led-step"]) {
    $(id).setAttribute("aria-disabled", String(!connected));
  }
  $("tare-fields").disabled = !connected;
  $("led-fields").disabled = !connected;
  if (!connected) return;

  const { settings, status } = state;
  $("adapter-name").textContent = state.device.name ?? "Acaia Bridge";
  $("adapter-version").textContent =
    (status.firmware || "unknown") + (status.mode === AdapterMode.Update ? " (update mode)" : "");
  $("adapter-scale").textContent = describeScale(status);
  $("adapter-machine").textContent = describeMachine(status);

  $("auto-tare").checked = settings.autoTare;
  $("tare-mode-fields").disabled = !settings.autoTare;
  for (const radio of document.querySelectorAll('input[name="tare-mode"]')) {
    radio.checked = Number(radio.value) === settings.autoTareMode;
  }
  $("tare-scale").checked = settings.tareScale;
  for (const radio of document.querySelectorAll('input[name="led-mode"]')) {
    radio.checked = Number(radio.value) === settings.ledMode;
  }
  if (document.activeElement !== $("brightness")) {
    $("brightness").value = settings.ledBrightness;
  }
  $("brightness-value").textContent = $("brightness").value;
}

function restartIdleTimer() {
  clearTimeout(state.idleTimer);
  state.idleTimer = setTimeout(() => {
    showConnectError(null);
    showSaved("warn", "Disconnected after 3 minutes without a change, so the coffee machine can connect again.");
    disconnect();
  }, IDLE_DISCONNECT_MS);
}

/*
 * Notifications only say "changed": with a small MTU a long status arrives
 * cut off, so the full value is read. A read fires the same event, which is
 * ignored while the read runs.
 */
async function refresh(which) {
  const chars = state.chars;
  if (!chars) return;
  state.reading[which] = true;
  try {
    const value = toBytes(await gatt(() => chars[which].readValue()));
    if (which === "settings") {
      state.settings = parseSettings(value);
    } else {
      state.status = parseStatus(value);
    }
  } finally {
    state.reading[which] = false;
  }
  render();
}

function onChanged(which) {
  if (state.reading[which]) return;
  refresh(which).catch((error) => console.warn(`refresh ${which}`, error));
}

async function writeSettings(changes) {
  if (!state.chars) return;
  restartIdleTimer();
  try {
    await gatt(() => state.chars.settings.writeValueWithResponse(encodeSettings(changes)));
    await sleep(APPLY_DELAY_MS);
    await refresh("settings");
    if (settingsMatch(state.settings, changes)) {
      showSaved("ok", "Saved on the adapter.");
    } else {
      showSaved("error", "The adapter did not accept this setting. Update its firmware and try again.");
    }
  } catch (error) {
    console.warn("write", error);
    showSaved("error", state.chars ? `Saving failed (${error.message}). Try again.` : "The Bluetooth connection was lost. Connect again.");
    render(); // show what the adapter has, not what was clicked
  }
}

async function forgetMachine() {
  if (!state.chars) return;
  if (!confirm("Forget the coffee machine? The adapter learns it again at the next shot.")) return;
  restartIdleTimer();
  try {
    await gatt(() => state.chars.control.writeValueWithResponse(new Uint8Array([ControlOp.ForgetMachine])));
    await sleep(APPLY_DELAY_MS);
    await refresh("status");
    showSaved("ok", "Machine forgotten. It is learned again at the next shot.");
  } catch (error) {
    console.warn("forget", error);
    showSaved("error", `That didn't work (${error.message}). Try again.`);
  }
}

function onDisconnected() {
  const unexpected = !state.leaving;
  state.chars = null;
  state.settings = null;
  state.status = null;
  state.leaving = false;
  clearTimeout(state.idleTimer);
  if (unexpected) {
    showConnectError("The Bluetooth connection was lost. Connect again.");
  }
  render();
}

function disconnect() {
  state.leaving = true;
  state.device?.gatt.disconnect();
}

async function connect() {
  showConnectError(null);
  showSaved("", null);
  let device;
  try {
    device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [ACAIA_SERVICE_UUID] }, { services: [UPDATE_SERVICE_UUID] }],
      optionalServices: [SERVICE_UUID],
    });
  } catch (error) {
    console.warn("requestDevice", error);
    // Closing the chooser is a NotFoundError too: stay quiet only for that one.
    if (!(error.name === "NotFoundError" && /cancel/i.test(error.message))) {
      showConnectError(connectErrorMessage(error));
    }
    return;
  }
  if (state.device !== device) {
    state.device?.removeEventListener("gattserverdisconnected", onDisconnected);
    device.addEventListener("gattserverdisconnected", onDisconnected);
  }
  state.device = device;
  $("connect").disabled = true;
  $("connect").textContent = "Connecting…";
  try {
    const server = await device.gatt.connect();
    let service;
    try {
      service = await server.getPrimaryService(SERVICE_UUID);
    } catch (error) {
      if (error.name === "NotFoundError") {
        throw new ConfigError(
          "This device has no settings to change: either it is a real Acaia scale, or the adapter's firmware " +
            "is older than the configurator. Update the adapter with the Bluetooth updater first."
        );
      }
      throw error;
    }
    const chars = {
      settings: await service.getCharacteristic(SETTINGS_UUID),
      status: await service.getCharacteristic(STATUS_UUID),
      control: await service.getCharacteristic(CONTROL_UUID),
    };
    state.chars = chars;
    state.queue = Promise.resolve();
    await refresh("settings");
    await refresh("status");
    if (state.settings.version !== PROTOCOL_VERSION) {
      throw new ConfigError("This adapter uses a different settings protocol. Reload the page; if that doesn't help, update the adapter.");
    }
    chars.settings.addEventListener("characteristicvaluechanged", () => onChanged("settings"));
    chars.status.addEventListener("characteristicvaluechanged", () => onChanged("status"));
    await gatt(() => chars.settings.startNotifications());
    await gatt(() => chars.status.startNotifications());
    restartIdleTimer();
  } catch (error) {
    console.warn("connect", error);
    state.chars = null;
    disconnect();
    showConnectError(connectErrorMessage(error));
  } finally {
    $("connect").textContent = "Connect";
    render();
  }
}

async function init() {
  state.supported = Boolean(navigator.bluetooth);
  // Brave ships Web Bluetooth behind a flag; tell its users where to switch it on.
  const brave = Boolean(navigator.brave) || (await navigator.brave?.isBrave?.().catch(() => false));
  $("unsupported").hidden = state.supported || brave;
  $("unsupported-brave").hidden = state.supported || !brave;
  if (state.supported && (await navigator.bluetooth.getAvailability?.().catch(() => true)) === false) {
    showConnectError("Bluetooth is switched off or this browser may not use it. Switch it on (macOS: also allow the browser under Privacy & Security → Bluetooth) and reload the page.");
  }

  $("connect").addEventListener("click", connect);
  $("disconnect").addEventListener("click", disconnect);
  $("forget").addEventListener("click", forgetMachine);
  $("auto-tare").addEventListener("change", (e) => writeSettings({ autoTare: e.target.checked }));
  $("tare-scale").addEventListener("change", (e) => writeSettings({ tareScale: e.target.checked }));
  for (const radio of document.querySelectorAll('input[name="tare-mode"]')) {
    radio.addEventListener("change", (e) => writeSettings({ autoTareMode: Number(e.target.value) }));
  }
  for (const radio of document.querySelectorAll('input[name="led-mode"]')) {
    radio.addEventListener("change", (e) => writeSettings({ ledMode: Number(e.target.value) }));
  }
  $("brightness").addEventListener("input", (e) => ($("brightness-value").textContent = e.target.value));
  $("brightness").addEventListener("change", (e) => writeSettings({ ledBrightness: Number(e.target.value) }));
  window.addEventListener("pagehide", disconnect);
  render();
}

init();
