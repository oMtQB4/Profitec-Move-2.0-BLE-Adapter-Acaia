// Run with: node --test web/configurator/protocol.test.mjs
// The byte sequences match test/test_config_protocol.cpp, so both sides agree.
import assert from "node:assert/strict";
import { test } from "node:test";

import {
  AdapterMode,
  AutoTareMode,
  ClientKind,
  ConfigError,
  LedMode,
  encodeSettings,
  parseRecords,
  parseSettings,
  parseStatus,
  settingsMatch,
} from "./protocol.js";

const hex = (text) => new Uint8Array(text.trim().split(/\s+/).map((b) => parseInt(b, 16)));

test("parses the adapter's settings", () => {
  const settings = parseSettings(hex("00 01 01  01 01 01  02 01 01  03 01 00  04 01 01  05 01 80"));
  assert.deepEqual(settings, {
    version: 1,
    autoTare: true,
    autoTareMode: AutoTareMode.MachineConnects,
    tareScale: false,
    ledMode: LedMode.ActiveOnly,
    ledBrightness: 0x80,
  });
});

test("encodes only the changed settings", () => {
  assert.deepEqual(encodeSettings({ tareScale: false }), hex("03 01 00"));
  assert.deepEqual(
    encodeSettings({ autoTare: true, autoTareMode: AutoTareMode.CupPlaced, ledMode: LedMode.Off, ledBrightness: 7 }),
    hex("01 01 01  02 01 00  04 01 02  05 01 07")
  );
  assert.deepEqual(encodeSettings({}), new Uint8Array());
});

test("rejects a brightness out of range", () => {
  assert.throws(() => encodeSettings({ ledBrightness: 0 }), ConfigError);
  assert.throws(() => encodeSettings({ ledBrightness: 256 }), ConfigError);
  assert.throws(() => encodeSettings({ ledBrightness: Number.NaN }), ConfigError);
});

test("parses the status", () => {
  const status = parseStatus(
    hex("00 01 01  01 01 00  02 03 31 2E 32  03 01 01  04 06 45 75 72 65 6B 61  05 01 02  06 02 41 41")
  );
  assert.deepEqual(status, {
    version: 1,
    mode: AdapterMode.Bridge,
    firmware: "1.2",
    scaleConnected: true,
    scaleName: "Eureka",
    client: ClientKind.Machine,
    machine: "AA",
  });
});

test("status without scale and machine", () => {
  const status = parseStatus(hex("00 01 01  01 01 01  02 00  03 01 00  05 01 00"));
  assert.equal(status.mode, AdapterMode.Update);
  assert.equal(status.firmware, "");
  assert.equal(status.scaleName, null);
  assert.equal(status.machine, null);
});

test("skips unknown records and rejects truncated ones", () => {
  assert.equal(parseSettings(hex("7F 03 AA BB CC  04 01 02")).ledMode, LedMode.Off);
  assert.throws(() => parseRecords(hex("01 02 01")), ConfigError);
  assert.throws(() => parseRecords(hex("01")), ConfigError);
});

test("checks whether a write took effect", () => {
  const settings = parseSettings(hex("01 01 01  03 01 00"));
  assert.ok(settingsMatch(settings, { autoTare: true, tareScale: false }));
  assert.ok(!settingsMatch(settings, { tareScale: true }));
});
