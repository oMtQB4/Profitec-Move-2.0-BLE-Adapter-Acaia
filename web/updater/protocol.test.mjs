// Run with: node --test web/updater/protocol.test.mjs
import assert from "node:assert/strict";
import { test } from "node:test";

import {
  CHIP_ID_ESP32S3,
  HEADER_LENGTH,
  Inbox,
  Opcode,
  PROJECT_NAME,
  Status,
  UpdateError,
  checkImage,
  encodeBegin,
  encodeData,
  parseInfo,
  parseNotification,
  upload,
} from "./protocol.js";

function makeImage(size, { project = PROJECT_NAME, chipId = CHIP_ID_ESP32S3, version = "1.2.3" } = {}) {
  const image = new Uint8Array(size).fill(0x5a);
  const view = new DataView(image.buffer);
  image[0] = 0xe9;
  view.setUint16(12, chipId, true);
  view.setUint32(32, 0xabcd5432, true);
  image.fill(0, 32 + 16, 32 + 80);
  image.set(new TextEncoder().encode(version), 32 + 16);
  image.set(new TextEncoder().encode(project), 32 + 48);
  return image;
}

/**
 * Stand-in for the adapter: same rules as main/ota/OtaSession.cpp
 * (sequence check, window, progress every window/2 packets).
 */
class FakeAdapter {
  constructor({ chunk = 180, window = 16, failAtPacket = -1, verifyStatus = Status.Ok } = {}) {
    Object.assign(this, { chunk, window, failAtPacket, verifyStatus });
    this.inbox = new Inbox();
    this.content = [];
    this.queued = 0;
    this.maxQueued = 0;
    this.packets = 0;
    this.nextSeq = 0;
    this.sinceAck = 0;
    this.size = 0;
    this.receiving = false;
    this.commands = [];
  }

  notify(bytes) {
    // Asynchronous like a real notification.
    setTimeout(() => this.inbox.push(Uint8Array.from(bytes)), 0);
  }

  async writeControl(bytes) {
    this.commands.push(bytes[0]);
    if (bytes[0] === Opcode.Begin) {
      this.size = new DataView(bytes.buffer, bytes.byteOffset).getUint32(1, true);
      this.receiving = true;
      this.notify([0x81, 0, this.chunk & 0xff, this.chunk >> 8, this.window, 0]);
    } else if (bytes[0] === Opcode.End) {
      this.notify([0x82, this.content.length === this.size ? this.verifyStatus : Status.Incomplete]);
    } else if (bytes[0] === Opcode.Abort) {
      this.receiving = false;
      this.notify([0x83, 0]);
    }
  }

  async writeData(bytes) {
    assert.ok(this.receiving, "data before Begin");
    assert.ok(bytes.length - 2 <= this.chunk, "packet larger than the chunk");
    assert.equal(bytes[0] | (bytes[1] << 8), this.nextSeq, "sequence");
    this.nextSeq = (this.nextSeq + 1) & 0xffff;
    this.queued += 1;
    this.maxQueued = Math.max(this.maxQueued, this.queued);
    // The app task drains the queue a moment later.
    setTimeout(() => {
      this.queued -= 1;
      if (this.packets === this.failAtPacket) {
        this.receiving = false;
        this.notify([0x90, Status.FlashError, 0, 0, 0, 0]);
        return;
      }
      this.packets += 1;
      this.content.push(...bytes.subarray(2));
      this.sinceAck += 1;
      if (this.content.length === this.size || this.sinceAck >= this.window / 2) {
        this.sinceAck = 0;
        const n = this.content.length;
        this.notify([0x90, 0, n & 0xff, (n >> 8) & 0xff, (n >> 16) & 0xff, n >>> 24]);
      }
    }, 1);
  }
}

test("checkImage accepts acaia-bridge firmware and reads its version", () => {
  const image = checkImage(makeImage(HEADER_LENGTH));
  assert.equal(image.version, "1.2.3");
  assert.equal(image.projectName, PROJECT_NAME);
});

test("checkImage rejects other projects, chips and the USB flash image", () => {
  assert.throws(() => checkImage(makeImage(1000, { project: "other" })), /not for the Acaia Bridge/);
  assert.throws(() => checkImage(makeImage(1000, { chipId: 5 })), /different ESP32 chip/);
  const usbImage = makeImage(1000);
  usbImage.fill(0, 32, 36); // bootloader at offset 0: no app description
  assert.throws(() => checkImage(usbImage), /-ota_update\.bin/);
  assert.throws(() => checkImage(new Uint8Array(10)), UpdateError);
});

test("byte layout matches the firmware", () => {
  assert.deepEqual([...encodeBegin(0x12345)], [0x01, 0x45, 0x23, 0x01, 0x00]);
  assert.deepEqual([...encodeData(0x1234, Uint8Array.of(0xaa))], [0x34, 0x12, 0xaa]);
  assert.deepEqual(parseNotification(Uint8Array.of(0x81, 0, 0xfe, 0x01, 16, 0)), {
    opcode: 0x81,
    status: 0,
    chunk: 510,
    window: 16,
  });
  assert.equal(parseNotification(Uint8Array.of(0x90, 0, 0xd0, 0x07, 0, 0)).bytes, 2000);
  // Same vector as testInfoEncoding in test/test_ota.cpp.
  const info = parseInfo(Uint8Array.of(1, 0x01, 0xfe, 0x01, 16, 0, 0x00, 0x00, 0x1c, 0x00, 0x31, 0x2e, 0x32, 0x2e, 0x33));
  assert.deepEqual(info, {
    protocolVersion: 1,
    updateMode: true,
    transferring: false,
    chunk: 510,
    window: 16,
    capacity: 0x1c0000,
    firmwareVersion: "1.2.3",
  });
});

test("upload sends the whole image within the window", async () => {
  const adapter = new FakeAdapter({ chunk: 180, window: 16 });
  const image = makeImage(20000);
  const progress = [];
  await upload(adapter, image, { onProgress: (done) => progress.push(done) });
  assert.deepEqual(Uint8Array.from(adapter.content), image);
  assert.ok(adapter.maxQueued <= 16, `queued ${adapter.maxQueued} packets`);
  assert.equal(progress.at(-1), image.length);
  assert.deepEqual(adapter.commands, [Opcode.Begin, Opcode.End]);
});

test("upload works with the smallest MTU and a window of one", async () => {
  const adapter = new FakeAdapter({ chunk: 18, window: 1 });
  const image = makeImage(1000);
  await upload(adapter, image);
  assert.deepEqual(Uint8Array.from(adapter.content), image);
  assert.equal(adapter.maxQueued, 1);
});

test("upload reports an error from the adapter", async () => {
  const adapter = new FakeAdapter({ failAtPacket: 5 });
  await assert.rejects(upload(adapter, makeImage(20000)), (error) => error.status === Status.FlashError);
});

test("upload reports a failed verification", async () => {
  const adapter = new FakeAdapter({ verifyStatus: Status.VerifyFailed });
  await assert.rejects(upload(adapter, makeImage(3000)), /rejected/);
});

test("cancelling sends Abort", async () => {
  const adapter = new FakeAdapter();
  const controller = new AbortController();
  const done = upload(adapter, makeImage(50000), {
    signal: controller.signal,
    onProgress: (bytes) => bytes > 5000 && controller.abort(),
  });
  await assert.rejects(done, /cancelled/);
  assert.equal(adapter.commands.at(-1), Opcode.Abort);
});

test("a silent adapter times out instead of hanging", { timeout: 15000 }, async () => {
  const inbox = new Inbox();
  const transport = { inbox, writeControl: async () => {}, writeData: async () => {} };
  await assert.rejects(upload(transport, makeImage(1000)), /stopped answering/);
});
