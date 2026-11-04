// Page logic of the Bluetooth updater; the protocol lives in protocol.js.
import {
  CONTROL_UUID,
  DATA_UUID,
  INFO_UUID,
  Inbox,
  PROTOCOL_VERSION,
  SERVICE_UUID,
  UpdateError,
  checkImage,
  parseInfo,
  upload,
} from "./protocol.js";

const $ = (id) => document.getElementById(id);
window.updaterLoaded = true;

const state = {
  latest: null,    // { bytes, image } from firmware/manifest.json
  file: null,      // { bytes, image } chosen by the user
  device: null,    // BluetoothDevice
  adapter: null,   // { transport, info }
  supported: true,
  updating: false,
  finished: false,
  abort: null,     // AbortController of the running update
};

const toBytes = (dataView) => new Uint8Array(dataView.buffer.slice(dataView.byteOffset, dataView.byteOffset + dataView.byteLength));
const kilobytes = (bytes) => `${Math.round(bytes / 1024)} KB`;

function firmware() {
  const source = document.querySelector('input[name="source"]:checked')?.value;
  return source === "latest" ? state.latest : state.file;
}

function compareVersions(a, b) {
  const parts = (v) => v.replace(/^v/, "").split(/[.-]/).map((p) => (/^\d+$/.test(p) ? Number(p) : p));
  const pa = parts(a);
  const pb = parts(b);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    if (pa[i] === pb[i]) continue;
    if (pa[i] === undefined) return -1;
    if (pb[i] === undefined) return 1;
    return pa[i] < pb[i] ? -1 : 1;
  }
  return 0;
}

function showResult(kind, message) {
  const result = $("result");
  result.hidden = !message;
  result.className = `notice ${kind}`;
  result.textContent = message ?? "";
}

function showConnectError(message) {
  const box = $("connect-result");
  box.hidden = !message;
  box.textContent = message ?? "";
}

/** Turns requestDevice()/connect() failures into something a user can act on. */
function connectErrorMessage(error) {
  if (error instanceof UpdateError) return error.message;
  const text = `${error.name}: ${error.message}`;
  if (/adapter not available|bluetooth.*(off|unavailable|not available)/i.test(error.message)) {
    return (
      "This browser can't reach Bluetooth. Switch Bluetooth on. On macOS also allow it under " +
      "System Settings → Privacy & Security → Bluetooth for your browser, then restart the browser."
    );
  }
  if (error.name === "SecurityError") {
    return "Bluetooth only works on secure pages: open the updater via https:// or http://localhost.";
  }
  if (error.name === "NotAllowedError") {
    return "Bluetooth access was blocked. Allow Bluetooth for this site in the browser settings and try again.";
  }
  return `Connecting failed (${text}). Reload the page and try again.`;
}

function render() {
  const fw = firmware();
  const adapter = state.adapter;

  $("firmware-status").textContent = fw
    ? `Acaia Bridge ${fw.image.version} · ${kilobytes(fw.bytes.length)}`
    : state.latest || state.file ? "" : "Choose a firmware file.";

  $("adapter-summary").hidden = !adapter;
  if (adapter) {
    $("adapter-name").textContent = state.device.name ?? "Acaia Bridge";
    $("adapter-version").textContent = adapter.info.firmwareVersion || "unknown";
  }
  $("connect").hidden = Boolean(adapter);
  $("disconnect").hidden = !adapter || state.updating;

  const ready = Boolean(fw && adapter) && !state.updating && !state.finished;
  $("update-step").setAttribute("aria-disabled", String(!(fw && adapter)));
  $("start").disabled = !ready;
  $("start").hidden = state.updating || state.finished;
  $("cancel").hidden = !state.updating;
  $("file-input").disabled = state.updating;
  $("connect").disabled = state.updating || !state.supported;

  if (fw && adapter) {
    const from = adapter.info.firmwareVersion || "unknown";
    const order = adapter.info.firmwareVersion ? compareVersions(fw.image.version, from) : 1;
    const note = order === 0 ? " (reinstall the same version)" : order < 0 ? " (older version)" : "";
    $("update-plan").textContent = `Install ${fw.image.version} over ${from}${note}. This takes about a minute.`;
  } else {
    $("update-plan").textContent = "Choose the firmware and connect the adapter first.";
  }
}

async function sha256Hex(bytes) {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** The deployed page carries the latest release next to it (see .github/workflows/ci.yml). */
async function loadLatest() {
  try {
    const response = await fetch("firmware/manifest.json", { cache: "no-cache" });
    if (!response.ok) return;
    const manifest = await response.json();
    const binary = await fetch(`firmware/${manifest.file}`, { cache: "no-cache" });
    if (!binary.ok) return;
    const bytes = new Uint8Array(await binary.arrayBuffer());
    if (manifest.sha256 && (await sha256Hex(bytes)) !== manifest.sha256) {
      throw new UpdateError("The downloaded firmware is damaged. Reload the page.");
    }
    state.latest = { bytes, image: checkImage(bytes) };
    $("latest-choice").hidden = false;
    $("latest-version").textContent = manifest.version;
    $("latest-detail").textContent = `${kilobytes(bytes.length)}, released ${manifest.date ?? ""}`.replace(/, released $/, "");
  } catch (error) {
    console.warn("latest firmware unavailable", error);
  }
  if (!state.latest) {
    $("file-radio").checked = true;
  }
  render();
}

async function onFileChosen() {
  const file = $("file-input").files[0];
  state.file = null;
  $("file-radio").checked = true;
  if (file) {
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      state.file = { bytes, image: checkImage(bytes) };
      showResult("", null);
    } catch (error) {
      showResult("error", error.message);
    }
  }
  render();
}

function onDisconnected() {
  const wasUpdating = state.updating;
  state.adapter?.transport.inbox.fail(
    new UpdateError("The Bluetooth connection was lost. The adapter keeps its current firmware; connect again and retry.")
  );
  state.adapter = null;
  if (!wasUpdating && !state.finished) {
    render();
  }
}

async function connect() {
  showResult("", null);
  showConnectError(null);
  let device;
  try {
    device = await navigator.bluetooth.requestDevice({ filters: [{ services: [SERVICE_UUID] }] });
  } catch (error) {
    console.warn("requestDevice", error);
    // Closing the chooser is a NotFoundError too: stay quiet only for that one.
    if (!(error.name === "NotFoundError" && /cancel/i.test(error.message))) {
      showConnectError(connectErrorMessage(error));
    }
    return;
  }
  state.device = device;
  state.finished = false;
  device.addEventListener("gattserverdisconnected", onDisconnected);
  $("connect").disabled = true;
  $("connect").textContent = "Connecting…";
  try {
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    const info = parseInfo(toBytes(await (await service.getCharacteristic(INFO_UUID)).readValue()));
    if (info.protocolVersion !== PROTOCOL_VERSION) {
      throw new UpdateError("This adapter uses a different update protocol. Update it once over USB.");
    }
    if (!info.updateMode) {
      throw new UpdateError("The adapter is not in update mode. Hold its BOOT button for 3 seconds until the LED turns purple.");
    }
    const control = await service.getCharacteristic(CONTROL_UUID);
    const data = await service.getCharacteristic(DATA_UUID);
    const inbox = new Inbox();
    control.addEventListener("characteristicvaluechanged", (event) => inbox.push(toBytes(event.target.value)));
    await control.startNotifications();
    const withoutResponse = data.properties.writeWithoutResponse;
    state.adapter = {
      info,
      transport: {
        inbox,
        writeControl: (bytes) => control.writeValueWithResponse(bytes),
        writeData: (bytes) => (withoutResponse ? data.writeValueWithoutResponse(bytes) : data.writeValueWithResponse(bytes)),
      },
    };
  } catch (error) {
    console.warn("connect", error);
    device.gatt.disconnect();
    state.adapter = null;
    showConnectError(connectErrorMessage(error));
  } finally {
    $("connect").textContent = "Connect";
    render();
  }
}

async function start() {
  const fw = firmware();
  const adapter = state.adapter;
  if (!fw || !adapter) return;
  if (fw.bytes.length > adapter.info.capacity) {
    showResult("error", "This firmware is too large for the adapter's flash layout. Flash it over USB.");
    return;
  }

  state.updating = true;
  state.abort = new AbortController();
  showResult("", null);
  $("progress-box").hidden = false;
  render();

  let wakeLock = null;
  try {
    wakeLock = await navigator.wakeLock?.request("screen");
  } catch {
    // Not available or denied: the update works without it.
  }

  const startedAt = performance.now();
  const onProgress = (done, total) => {
    $("progress").value = done / total;
    $("progress-percent").textContent = `${Math.floor((100 * done) / total)} %`;
    const seconds = (performance.now() - startedAt) / 1000;
    if (done > 0 && seconds > 1) {
      const rate = done / seconds;
      const remaining = Math.ceil((total - done) / rate);
      $("progress-rate").textContent = `${(rate / 1024).toFixed(1)} KB/s · ${remaining} s left`;
    }
  };

  try {
    await upload(adapter.transport, fw.bytes, { onProgress, signal: state.abort.signal });
    state.finished = true;
    $("progress-rate").textContent = "";
    showResult(
      "ok",
      `Done. The adapter restarts with ${fw.image.version}; its LED blinks red until the scale is connected. ` +
        "The pairing with the machine stays as it is."
    );
  } catch (error) {
    const lost = !state.adapter && !(error instanceof UpdateError);
    showResult(
      "error",
      lost
        ? "The Bluetooth connection was lost. The adapter keeps its current firmware; connect again and retry."
        : error instanceof UpdateError
          ? error.message
          : `Update failed: ${error.message}`
    );
    $("progress-box").hidden = true;
  } finally {
    state.updating = false;
    state.abort = null;
    wakeLock?.release().catch(() => {});
    if (state.finished) {
      state.device?.gatt.disconnect();
    }
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
  $("file-input").addEventListener("change", onFileChosen);
  for (const radio of document.querySelectorAll('input[name="source"]')) {
    radio.addEventListener("change", render);
  }
  $("connect").addEventListener("click", connect);
  $("disconnect").addEventListener("click", () => state.device?.gatt.disconnect());
  $("start").addEventListener("click", start);
  $("cancel").addEventListener("click", () => state.abort?.abort());
  window.addEventListener("beforeunload", (event) => {
    if (state.updating) event.preventDefault();
  });
  render();
  await loadLatest();
}

init();
