// Client side of the configuration protocol, version 1.
// The adapter side and the byte layout are documented in main/config/ConfigProtocol.hpp;
// keep both in sync. No browser APIs here, so `node --test` covers it.

export const SERVICE_UUID = "7e1a0010-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const SETTINGS_UUID = "7e1a0011-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const STATUS_UUID = "7e1a0012-8f3c-4b6e-9a51-3c7d2e4f6a10";
export const CONTROL_UUID = "7e1a0013-8f3c-4b6e-9a51-3c7d2e4f6a10";

/** What the adapter advertises: the Acaia service while bridging, the update service in update mode. */
export const ACAIA_SERVICE_UUID = "49535343-fe7d-4ae5-8fa9-9fafd205e455";
export const UPDATE_SERVICE_UUID = "7e1a0001-8f3c-4b6e-9a51-3c7d2e4f6a10";

export const PROTOCOL_VERSION = 1;

export const SettingKey = Object.freeze({
  Version: 0,
  AutoTare: 1,
  AutoTareMode: 2,
  TareScale: 3,
  LedMode: 4,
  LedBrightness: 5,
});

export const StatusKey = Object.freeze({
  Version: 0,
  Mode: 1,
  Firmware: 2,
  ScaleConnected: 3,
  ScaleName: 4,
  Client: 5,
  Machine: 6,
});

export const AutoTareMode = Object.freeze({ CupPlaced: 0, MachineConnects: 1 });
export const LedMode = Object.freeze({ Standard: 0, ActiveOnly: 1, Off: 2 });
export const AdapterMode = Object.freeze({ Bridge: 0, Update: 1 });
export const ClientKind = Object.freeze({ None: 0, Identifying: 1, Machine: 2, Other: 3 });
export const ControlOp = Object.freeze({ ForgetMachine: 0x01 });

export class ConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = "ConfigError";
  }
}

/** Splits <key> <length> <value> records into a Map. Later records of the same key win. */
export function parseRecords(bytes) {
  const records = new Map();
  let offset = 0;
  while (offset < bytes.length) {
    if (bytes.length - offset < 2 || bytes.length - offset - 2 < bytes[offset + 1]) {
      throw new ConfigError("The adapter sent a damaged reply. Reconnect and try again.");
    }
    const length = bytes[offset + 1];
    records.set(bytes[offset], bytes.subarray(offset + 2, offset + 2 + length));
    offset += 2 + length;
  }
  return records;
}

const u8 = (records, key) => {
  const value = records.get(key);
  return value?.length === 1 ? value[0] : undefined;
};
const text = (records, key) => {
  const value = records.get(key);
  return value === undefined ? undefined : new TextDecoder().decode(value);
};

export function parseSettings(bytes) {
  const records = parseRecords(bytes);
  return {
    version: u8(records, SettingKey.Version),
    autoTare: u8(records, SettingKey.AutoTare) === 1,
    autoTareMode: u8(records, SettingKey.AutoTareMode),
    tareScale: u8(records, SettingKey.TareScale) === 1,
    ledMode: u8(records, SettingKey.LedMode),
    ledBrightness: u8(records, SettingKey.LedBrightness),
  };
}

/** Records for the given fields only; the adapter keeps the others. */
export function encodeSettings(changes) {
  const out = [];
  const put = (key, value) => out.push(key, 1, value);
  const bool = (value) => (value ? 1 : 0);
  if (changes.autoTare !== undefined) put(SettingKey.AutoTare, bool(changes.autoTare));
  if (changes.autoTareMode !== undefined) put(SettingKey.AutoTareMode, changes.autoTareMode);
  if (changes.tareScale !== undefined) put(SettingKey.TareScale, bool(changes.tareScale));
  if (changes.ledMode !== undefined) put(SettingKey.LedMode, changes.ledMode);
  if (changes.ledBrightness !== undefined) {
    const brightness = Math.round(changes.ledBrightness);
    if (!(brightness >= 1 && brightness <= 255)) {
      throw new ConfigError("Brightness must be between 1 and 255.");
    }
    put(SettingKey.LedBrightness, brightness);
  }
  return new Uint8Array(out);
}

export function parseStatus(bytes) {
  const records = parseRecords(bytes);
  return {
    version: u8(records, StatusKey.Version),
    mode: u8(records, StatusKey.Mode),
    firmware: text(records, StatusKey.Firmware) ?? "",
    scaleConnected: u8(records, StatusKey.ScaleConnected) === 1,
    scaleName: text(records, StatusKey.ScaleName) ?? null,
    client: u8(records, StatusKey.Client) ?? ClientKind.None,
    machine: text(records, StatusKey.Machine) ?? null,
  };
}

/** True when every field of `wanted` has that value in `settings` (to check a write took effect). */
export function settingsMatch(settings, wanted) {
  return Object.entries(wanted).every(([key, value]) => settings[key] === value);
}
