// Client side of the firmware update protocol, version 1.
// The adapter side and the byte layout are documented in main/ota/OtaProtocol.hpp;
// keep both in sync. No browser APIs here, so `node --test` covers it.

export const SERVICE_UUID = "7e1a0001-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const CONTROL_UUID = "7e1a0002-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const DATA_UUID = "7e1a0003-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const INFO_UUID = "7e1a0004-8f3c-4b6e-9a51-3c7d2e4f6a10";

export const PROTOCOL_VERSION = 1;
export const PROJECT_NAME = "acaia-bridge";
export const CHIP_ID_ESP32S3 = 9;
export const HEADER_LENGTH = 24 + 8 + 256;

export const Opcode = Object.freeze({
  Begin: 0x01,
  End: 0x02,
  Abort: 0x03,
  BeginResponse: 0x81,
  EndResponse: 0x82,
  AbortResponse: 0x83,
  Progress: 0x90,
});

export const Status = Object.freeze({
  Ok: 0,
  NotInUpdateMode: 1,
  InvalidCommand: 2,
  InvalidSize: 3,
  NotStarted: 4,
  SequenceError: 5,
  Overflow: 6,
  InvalidImage: 7,
  TooMuchData: 8,
  Incomplete: 9,
  FlashError: 10,
  VerifyFailed: 11,
  Timeout: 12,
});

const STATUS_MESSAGES = {
  [Status.NotInUpdateMode]: "The adapter is not in update mode. Hold its BOOT button for 3 seconds until the LED turns purple.",
  [Status.InvalidCommand]: "The adapter did not understand the updater. Reload the page and try again.",
  [Status.InvalidSize]: "The firmware file is too large for this adapter or empty.",
  [Status.NotStarted]: "The adapter lost track of the update. Try again.",
  [Status.SequenceError]: "A Bluetooth packet went missing. Move closer to the adapter and try again.",
  [Status.Overflow]: "The adapter could not keep up. Try again.",
  [Status.InvalidImage]: "This file is not Acaia Bridge firmware for the ESP32-S3.",
  [Status.TooMuchData]: "The adapter received more data than announced. Try again.",
  [Status.Incomplete]: "The transfer ended early. Try again.",
  [Status.FlashError]: "Writing to the adapter's flash failed. Try again; if it keeps failing, flash over USB.",
  [Status.VerifyFailed]: "The firmware arrived damaged and was rejected; the adapter keeps its current firmware. Try again.",
  [Status.Timeout]: "The adapter stopped waiting for data. Try again.",
};

export class UpdateError extends Error {
  constructor(message, status) {
    super(message);
    this.name = "UpdateError";
    this.status = status;
  }
}

export function statusMessage(status) {
  return STATUS_MESSAGES[status] ?? `The adapter reported error ${status}.`;
}

function readString(bytes, offset, length) {
  const field = bytes.subarray(offset, offset + length);
  const end = field.indexOf(0);
  return new TextDecoder().decode(end < 0 ? field : field.subarray(0, end));
}

/**
 * Reads the app description from an ESP-IDF app image (image header, first
 * segment header, esp_app_desc_t). Throws UpdateError for anything else,
 * including the -to_flash_via_usb image (bootloader first).
 */
export function parseImage(bytes) {
  if (bytes.length < HEADER_LENGTH || bytes[0] !== 0xe9) {
    throw new UpdateError("This is not an ESP32 firmware image.");
  }
  // The -to_flash_via_usb image also starts with 0xE9, but with the bootloader.
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (view.getUint32(32, true) !== 0xabcd5432) {
    throw new UpdateError(
      "This is the image for USB flashing. For updates over Bluetooth choose the file ending in -ota_update.bin."
    );
  }
  return {
    chipId: view.getUint16(12, true),
    version: readString(bytes, 32 + 16, 32),
    projectName: readString(bytes, 32 + 48, 32),
    size: bytes.length,
  };
}

/** Checks that `bytes` is firmware for this adapter; returns its description. */
export function checkImage(bytes) {
  const image = parseImage(bytes);
  if (image.projectName !== PROJECT_NAME) {
    throw new UpdateError(`This firmware is for "${image.projectName}", not for the Acaia Bridge.`);
  }
  if (image.chipId !== CHIP_ID_ESP32S3) {
    throw new UpdateError("This firmware is for a different ESP32 chip, not the ESP32-S3.");
  }
  return image;
}

export function encodeBegin(size) {
  const out = new Uint8Array(5);
  out[0] = Opcode.Begin;
  new DataView(out.buffer).setUint32(1, size, true);
  return out;
}

export const encodeEnd = () => Uint8Array.of(Opcode.End);
export const encodeAbort = () => Uint8Array.of(Opcode.Abort);

export function encodeData(seq, payload) {
  const out = new Uint8Array(2 + payload.length);
  out[0] = seq & 0xff;
  out[1] = (seq >> 8) & 0xff;
  out.set(payload, 2);
  return out;
}

/** One notification from the Control characteristic. */
export function parseNotification(bytes) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const frame = { opcode: bytes[0], status: bytes.length > 1 ? bytes[1] : Status.InvalidCommand };
  if (frame.opcode === Opcode.BeginResponse && bytes.length >= 6) {
    frame.chunk = view.getUint16(2, true);
    frame.window = view.getUint16(4, true);
  } else if (frame.opcode === Opcode.Progress && bytes.length >= 6) {
    frame.bytes = view.getUint32(2, true);
  }
  return frame;
}

/** The Info characteristic. */
export function parseInfo(bytes) {
  if (bytes.length < 10) {
    throw new UpdateError("The adapter's update service answered with an unexpected format.");
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return {
    protocolVersion: bytes[0],
    updateMode: (bytes[1] & 0x01) !== 0,
    transferring: (bytes[1] & 0x02) !== 0,
    chunk: view.getUint16(2, true),
    window: view.getUint16(4, true),
    capacity: view.getUint32(6, true),
    firmwareVersion: new TextDecoder().decode(bytes.subarray(10)),
  };
}

/** Buffers notifications so the uploader can wait for the next one of a kind. */
export class Inbox {
  #frames = [];
  #waiters = [];

  push(bytes) {
    const frame = parseNotification(bytes);
    const index = this.#waiters.findIndex((w) => w.opcodes.includes(frame.opcode));
    if (index >= 0) {
      const [waiter] = this.#waiters.splice(index, 1);
      clearTimeout(waiter.timer);
      waiter.resolve(frame);
    } else {
      this.#frames.push(frame);
    }
  }

  /** Fails every pending wait, e.g. when the connection drops. */
  fail(error) {
    for (const waiter of this.#waiters.splice(0)) {
      clearTimeout(waiter.timer);
      waiter.reject(error);
    }
  }

  next(opcodes, timeoutMs) {
    const index = this.#frames.findIndex((f) => opcodes.includes(f.opcode));
    if (index >= 0) {
      return Promise.resolve(this.#frames.splice(index, 1)[0]);
    }
    return new Promise((resolve, reject) => {
      const waiter = { opcodes, resolve, reject };
      waiter.timer = setTimeout(() => {
        this.#waiters.splice(this.#waiters.indexOf(waiter), 1);
        reject(new UpdateError("The adapter stopped answering. Move closer and try again.", Status.Timeout));
      }, timeoutMs);
      this.#waiters.push(waiter);
    });
  }
}

const RESPONSE_TIMEOUT_MS = 10000;
const PROGRESS_TIMEOUT_MS = 10000;
const VERIFY_TIMEOUT_MS = 30000;

function check(frame) {
  if (frame.status !== Status.Ok) {
    throw new UpdateError(statusMessage(frame.status), frame.status);
  }
  return frame;
}

/**
 * Sends `image` to the adapter.
 *
 * transport: {
 *   writeControl(bytes): Promise   write with response
 *   writeData(bytes): Promise      write without response
 *   inbox: Inbox                   receives Control notifications
 * }
 * onProgress(bytesAcknowledged, total) is called after every acknowledgement.
 * signal: AbortSignal; aborting sends Abort and rejects.
 */
export async function upload(transport, image, { onProgress = () => {}, signal } = {}) {
  const { inbox } = transport;
  const throwIfAborted = async () => {
    if (signal?.aborted) {
      await transport.writeControl(encodeAbort()).catch(() => {});
      throw new UpdateError("Update cancelled. The adapter keeps its current firmware.");
    }
  };

  await transport.writeControl(encodeBegin(image.length));
  const begin = check(await inbox.next([Opcode.BeginResponse], RESPONSE_TIMEOUT_MS));
  const chunk = begin.chunk;
  const window = Math.max(1, begin.window);
  if (!chunk) {
    throw new UpdateError("The adapter reported no usable packet size.");
  }

  let sent = 0;
  let acked = 0;
  let seq = 0;
  const inFlight = []; // end offset of every unacknowledged packet

  const waitForProgress = async () => {
    const frame = check(await inbox.next([Opcode.Progress], PROGRESS_TIMEOUT_MS));
    acked = Math.max(acked, frame.bytes);
    while (inFlight.length > 0 && inFlight[0] <= acked) {
      inFlight.shift();
    }
    onProgress(acked, image.length);
  };

  onProgress(0, image.length);
  while (sent < image.length) {
    await throwIfAborted();
    if (inFlight.length >= window) {
      await waitForProgress();
      continue;
    }
    const end = Math.min(sent + chunk, image.length);
    await transport.writeData(encodeData(seq, image.subarray(sent, end)));
    seq = (seq + 1) & 0xffff;
    sent = end;
    inFlight.push(end);
  }
  while (acked < image.length) {
    await throwIfAborted();
    await waitForProgress();
  }

  // The adapter verifies the whole image before answering.
  await transport.writeControl(encodeEnd());
  check(await inbox.next([Opcode.EndResponse], VERIFY_TIMEOUT_MS));
}
