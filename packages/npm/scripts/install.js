#!/usr/bin/env node

"use strict";

const https = require("https");
const http = require("http");
const fs = require("fs");
const path = require("path");
const os = require("os");
const { execSync } = require("child_process");

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

const VERSION = "1.3.0";
const GITHUB_REPO = "devheallabs-ai/nc";
const BASE_URL = `https://github.com/${GITHUB_REPO}/releases/download/v${VERSION}`;

const PLATFORM_MAP = {
  "linux-x64": { asset: "nc-linux-x86_64", sha256: "" },
  "darwin-x64": { asset: "nc-macos-x86_64", sha256: "" },
  "darwin-arm64": { asset: "nc-macos-arm64", sha256: "" },
  "win32-x64": { asset: "nc-windows-x86_64.exe", sha256: "" },
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function getPlatformKey() {
  const p = process.platform;
  const a = process.arch;
  return `${p}-${a}`;
}

function getBinaryName() {
  return process.platform === "win32" ? "nc.exe" : "nc";
}

function getInstallDir() {
  // Install into the bin/ directory of this package
  return path.join(__dirname, "..", "bin");
}

/**
 * Follow redirects and download a URL to a file.
 */
function download(url, dest) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest);
    const proto = url.startsWith("https") ? https : http;

    function doRequest(requestUrl, redirectCount) {
      if (redirectCount > 10) {
        reject(new Error("Too many redirects"));
        return;
      }

      proto
        .get(requestUrl, { headers: { "User-Agent": "nc-lang-npm" } }, (res) => {
          // Follow redirects
          if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
            res.resume(); // consume response body
            doRequest(res.headers.location, redirectCount + 1);
            return;
          }

          if (res.statusCode !== 200) {
            res.resume();
            reject(new Error(`Download failed: HTTP ${res.statusCode} from ${requestUrl}`));
            return;
          }

          const total = parseInt(res.headers["content-length"] || "0", 10);
          let downloaded = 0;

          res.on("data", (chunk) => {
            downloaded += chunk.length;
            if (total > 0) {
              const pct = Math.floor((downloaded / total) * 100);
              const bar = "#".repeat(Math.floor(pct / 3.33)) + "-".repeat(30 - Math.floor(pct / 3.33));
              process.stdout.write(`\r  [${bar}] ${pct}%  (${Math.floor(downloaded / 1024)} KB)`);
            }
          });

          res.pipe(file);

          file.on("finish", () => {
            if (total > 0) process.stdout.write("\n");
            file.close(resolve);
          });

          file.on("error", (err) => {
            fs.unlink(dest, () => {}); // clean up
            reject(err);
          });
        })
        .on("error", (err) => {
          fs.unlink(dest, () => {});
          reject(err);
        });
    }

    doRequest(url, 0);
  });
}

function makeExecutable(filePath) {
  if (process.platform !== "win32") {
    fs.chmodSync(filePath, 0o755);
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main() {
  const key = getPlatformKey();
  const info = PLATFORM_MAP[key];

  if (!info) {
    console.error(
      `nc-lang: unsupported platform "${key}".\n` +
        "  Prebuilt binaries are currently published for linux-x64, darwin-x64, darwin-arm64, and win32-x64.\n" +
        "  Please build NC from source: https://nc.devheallabs.in/docs/install"
    );
    process.exit(1);
  }

  const installDir = getInstallDir();
  const binaryName = getBinaryName();
  const dest = path.join(installDir, binaryName);

  // Skip if already present
  if (fs.existsSync(dest)) {
    console.log(`nc-lang: binary already installed at ${dest}`);
    return;
  }

  const url = `${BASE_URL}/${info.asset}`;
  console.log(`nc-lang: installing NC v${VERSION} for ${process.platform} ${process.arch}...`);
  console.log(`  Source: ${url}`);

  // Ensure bin directory exists
  fs.mkdirSync(installDir, { recursive: true });

  const tmpDest = dest + ".tmp";

  try {
    await download(url, tmpDest);

    // Verify checksum if available
    if (info.sha256) {
      const crypto = require("crypto");
      const hash = crypto.createHash("sha256");
      const data = fs.readFileSync(tmpDest);
      hash.update(data);
      const actual = hash.digest("hex");

      if (actual !== info.sha256) {
        fs.unlinkSync(tmpDest);
        console.error(
          `nc-lang: checksum mismatch!\n` +
            `  Expected: ${info.sha256}\n` +
            `  Got:      ${actual}`
        );
        process.exit(1);
      }
    }

    // Make executable and move into place
    makeExecutable(tmpDest);
    fs.renameSync(tmpDest, dest);

    console.log(`  Installed: ${dest}`);
  } catch (err) {
    if (fs.existsSync(tmpDest)) {
      fs.unlinkSync(tmpDest);
    }
    console.error(`nc-lang: installation failed: ${err.message}`);
    console.error("  You can install NC manually: https://nc.devheallabs.in/docs/install");
    process.exit(1);
  }
}

main();
