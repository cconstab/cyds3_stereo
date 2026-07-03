// CYD-S3 Stereo firmware update server.
// - Devices poll GET /api/manifest and self-update from /firmware/... (OTA app image)
// - Admins upload builds on the dashboard (/) — optionally with a merged image for
//   the browser-based USB flasher at /flash (ESP Web Tools, vendored from npm).
// - State lives in DATA_DIR (versions.json + firmware binaries): mount it as a volume.
import express from "express";
import multer from "multer";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export const DEFAULT_DEVICE = "cyds3-stereo";

export function createApp({ dataDir, uploadToken }) {
  fs.mkdirSync(dataDir, { recursive: true });
  const stateFile = path.join(dataDir, "versions.json");

  const state = fs.existsSync(stateFile)
    ? JSON.parse(fs.readFileSync(stateFile, "utf8"))
    : { devices: {}, checkins: {} };

  const save = () => fs.writeFileSync(stateFile, JSON.stringify(state, null, 2));

  const deviceState = (device) =>
    (state.devices[device] ??= { current: null, history: [] });

  const requireAuth = (req, res, next) => {
    if (!uploadToken) return next();
    const header = req.get("authorization") || "";
    const token = header.replace(/^Bearer\s+/i, "") || req.body?.token;
    if (token === uploadToken) return next();
    res.status(401).json({ error: "invalid or missing upload token" });
  };

  const upload = multer({
    storage: multer.memoryStorage(),
    limits: { fileSize: 20 * 1024 * 1024 },
  });

  const app = express();
  app.use(express.json());

  // ---- Device-facing ----

  app.get("/api/manifest", (req, res) => {
    const device = req.query.device || DEFAULT_DEVICE;
    const ds = state.devices[device];

    // record check-in for the dashboard
    if (req.query.id) {
      state.checkins[req.query.id] = {
        device,
        fw: req.query.fw || "unknown",
        lastSeen: new Date().toISOString(),
        ip: req.ip,
      };
      save();
    }

    if (!ds?.current) return res.status(404).json({ error: "no firmware published" });
    const cur = ds.history.find((h) => h.version === ds.current);
    res.json({
      version: cur.version,
      url: `/firmware/${device}/${cur.version}/firmware.bin`,
      sha256: cur.sha256,
      size: cur.size,
      notes: cur.notes || "",
    });
  });

  app.get("/firmware/:device/:version/:file", (req, res) => {
    const { device, version, file } = req.params;
    if (![device, version, file].every((s) => /^[\w.\-]+$/.test(s))) {
      return res.status(400).end();
    }
    const p = path.join(dataDir, device, version, file);
    if (!fs.existsSync(p)) return res.status(404).end();
    res.setHeader("Content-Type", "application/octet-stream");
    res.sendFile(p);
  });

  // ---- Admin API ----

  app.post(
    "/api/upload",
    upload.fields([
      { name: "firmware", maxCount: 1 },
      { name: "merged", maxCount: 1 },
    ]),
    requireAuth,
    (req, res) => {
      const device = req.body.device || DEFAULT_DEVICE;
      const version = (req.body.version || "").trim();
      const fw = req.files?.firmware?.[0];
      if (!fw) return res.status(400).json({ error: "firmware file required" });
      if (!/^\d+\.\d+\.\d+(\.\d+)?$/.test(version)) {
        return res.status(400).json({ error: "version must be like 1.2.3" });
      }
      const ds = deviceState(device);
      if (ds.history.some((h) => h.version === version)) {
        return res.status(409).json({ error: `version ${version} already exists` });
      }

      const dir = path.join(dataDir, device, version);
      fs.mkdirSync(dir, { recursive: true });
      fs.writeFileSync(path.join(dir, "firmware.bin"), fw.buffer);
      const entry = {
        version,
        sha256: crypto.createHash("sha256").update(fw.buffer).digest("hex"),
        size: fw.buffer.length,
        notes: req.body.notes || "",
        uploadedAt: new Date().toISOString(),
        hasMerged: false,
      };
      const merged = req.files?.merged?.[0];
      if (merged) {
        fs.writeFileSync(path.join(dir, "merged.bin"), merged.buffer);
        entry.hasMerged = true;
      }
      ds.history.unshift(entry);
      ds.current = version;
      save();
      res.json({ ok: true, current: ds.current, entry });
    }
  );

  app.post("/api/rollback", express.json(), requireAuth, (req, res) => {
    const device = req.body.device || DEFAULT_DEVICE;
    const version = req.body.version;
    const ds = state.devices[device];
    if (!ds?.history.some((h) => h.version === version)) {
      return res.status(404).json({ error: "unknown version" });
    }
    ds.current = version;
    save();
    res.json({ ok: true, current: version });
  });

  app.get("/api/versions", (req, res) => {
    const device = req.query.device || DEFAULT_DEVICE;
    const ds = state.devices[device] || { current: null, history: [] };
    res.json(ds);
  });

  app.get("/api/devices", (_req, res) => res.json(state.checkins));

  // ---- Browser flasher (ESP Web Tools, vendored from node_modules) ----

  app.get("/flash/manifest.json", (req, res) => {
    const device = req.query.device || DEFAULT_DEVICE;
    const ds = state.devices[device];
    const cur = ds?.history.find((h) => h.version === ds.current && h.hasMerged);
    if (!cur) return res.status(404).json({ error: "no merged image published" });
    res.json({
      name: "CYD-S3 Stereo",
      version: cur.version,
      new_install_prompt_erase: true,
      builds: [
        {
          chipFamily: "ESP32-S3",
          parts: [{ path: `/firmware/${device}/${cur.version}/merged.bin`, offset: 0 }],
        },
      ],
    });
  });

  app.use(
    "/vendor/esp-web-tools",
    express.static(path.join(__dirname, "node_modules", "esp-web-tools", "dist"))
  );
  app.use(express.static(path.join(__dirname, "public")));

  return app;
}

// Entrypoint (skipped under tests, which import createApp directly)
if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const port = Number(process.env.PORT || 8080);
  const dataDir = process.env.DATA_DIR || path.join(__dirname, "data");
  const uploadToken = process.env.UPLOAD_TOKEN || "";
  if (!uploadToken) {
    console.warn("WARNING: UPLOAD_TOKEN not set — uploads are unauthenticated");
  }
  createApp({ dataDir, uploadToken }).listen(port, () => {
    console.log(`CYD-S3 update server on :${port}, data in ${dataDir}`);
  });
}
