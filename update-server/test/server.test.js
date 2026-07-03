// End-to-end tests: upload -> manifest -> download -> rollback -> auth -> flasher manifest.
import { test, before, after } from "node:test";
import assert from "node:assert";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import crypto from "node:crypto";
import { createApp } from "../server.js";

let server, base, dataDir;
const TOKEN = "test-token";

const fakeBin = (label) => {
  const buf = Buffer.alloc(4096, 0xff);
  buf.write(`FAKE-FIRMWARE-${label}`, 0);
  return buf;
};

const uploadForm = ({ version, notes = "", token = TOKEN, merged = null, bin }) => {
  const fd = new FormData();
  fd.append("version", version);
  fd.append("notes", notes);
  fd.append("token", token);
  fd.append("firmware", new Blob([bin]), "firmware.bin");
  if (merged) fd.append("merged", new Blob([merged]), "merged.bin");
  return fd;
};

before(async () => {
  dataDir = fs.mkdtempSync(path.join(os.tmpdir(), "cyds3-test-"));
  const app = createApp({ dataDir, uploadToken: TOKEN });
  await new Promise((resolve) => {
    server = app.listen(0, resolve);
  });
  base = `http://127.0.0.1:${server.address().port}`;
});

after(() => {
  server.close();
  fs.rmSync(dataDir, { recursive: true, force: true });
});

test("manifest 404 before any upload", async () => {
  const r = await fetch(`${base}/api/manifest?device=cyds3-stereo`);
  assert.equal(r.status, 404);
});

test("upload rejects bad token", async () => {
  const r = await fetch(`${base}/api/upload`, {
    method: "POST",
    body: uploadForm({ version: "0.1.0", token: "wrong", bin: fakeBin("x") }),
  });
  assert.equal(r.status, 401);
});

test("upload rejects malformed version", async () => {
  const r = await fetch(`${base}/api/upload`, {
    method: "POST",
    body: uploadForm({ version: "banana", bin: fakeBin("x") }),
  });
  assert.equal(r.status, 400);
});

test("upload + manifest + download round-trip", async () => {
  const bin = fakeBin("0.1.0");
  const up = await fetch(`${base}/api/upload`, {
    method: "POST",
    body: uploadForm({ version: "0.1.0", notes: "first", bin }),
  });
  assert.equal(up.status, 200);

  const m = await (
    await fetch(`${base}/api/manifest?device=cyds3-stereo&fw=0.0.1&id=abc123`)
  ).json();
  assert.equal(m.version, "0.1.0");
  assert.equal(m.sha256, crypto.createHash("sha256").update(bin).digest("hex"));
  assert.equal(m.size, bin.length);

  const dl = await fetch(base + m.url);
  assert.equal(dl.status, 200);
  const body = Buffer.from(await dl.arrayBuffer());
  assert.ok(body.equals(bin), "downloaded bin matches upload");

  // the poll above should have recorded a check-in
  const devices = await (await fetch(`${base}/api/devices`)).json();
  assert.equal(devices.abc123.fw, "0.0.1");
});

test("newer upload becomes current, rollback restores old", async () => {
  const up = await fetch(`${base}/api/upload`, {
    method: "POST",
    body: uploadForm({ version: "0.2.0", bin: fakeBin("0.2.0"), merged: fakeBin("merged") }),
  });
  assert.equal(up.status, 200);

  let m = await (await fetch(`${base}/api/manifest`)).json();
  assert.equal(m.version, "0.2.0");

  // duplicate version rejected
  const dup = await fetch(`${base}/api/upload`, {
    method: "POST",
    body: uploadForm({ version: "0.2.0", bin: fakeBin("dup") }),
  });
  assert.equal(dup.status, 409);

  // rollback requires auth
  const noAuth = await fetch(`${base}/api/rollback`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ version: "0.1.0" }),
  });
  assert.equal(noAuth.status, 401);

  const rb = await fetch(`${base}/api/rollback`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Authorization: `Bearer ${TOKEN}` },
    body: JSON.stringify({ version: "0.1.0" }),
  });
  assert.equal(rb.status, 200);
  m = await (await fetch(`${base}/api/manifest`)).json();
  assert.equal(m.version, "0.1.0");
});

test("flasher manifest uses newest version with a merged image", async () => {
  const r = await fetch(`${base}/flash/manifest.json`);
  // current is 0.1.0 (no merged image) -> 404
  assert.equal(r.status, 404);

  await fetch(`${base}/api/rollback`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Authorization: `Bearer ${TOKEN}` },
    body: JSON.stringify({ version: "0.2.0" }),
  });
  const m = await (await fetch(`${base}/flash/manifest.json`)).json();
  assert.equal(m.builds[0].chipFamily, "ESP32-S3");
  assert.equal(m.builds[0].parts[0].offset, 0);
  const dl = await fetch(base + m.builds[0].parts[0].path);
  assert.equal(dl.status, 200);
});

test("path traversal blocked on firmware download", async () => {
  const r = await fetch(`${base}/firmware/..%2F..%2Fetc/passwd/x`);
  assert.notEqual(r.status, 200);
});

test("state survives restart (versions.json reload)", async () => {
  const app2 = createApp({ dataDir, uploadToken: TOKEN });
  const s2 = await new Promise((resolve) => {
    const s = app2.listen(0, () => resolve(s));
  });
  const m = await (
    await fetch(`http://127.0.0.1:${s2.address().port}/api/manifest`)
  ).json();
  assert.equal(m.version, "0.2.0");
  s2.close();
});
