#!/usr/bin/env node

"use strict";

const { spawn } = require("child_process");
const path = require("path");
const fs = require("fs");
const os = require("os");

/**
 * Locate the NC binary.
 *
 * Search order:
 *   1. NC_BINARY environment variable
 *   2. Binary installed alongside this package (./bin/nc or ./bin/nc.exe)
 *   3. Binary cached in ~/.nc/bin/
 */
function findBinary() {
  // 1. Explicit override
  const override = process.env.NC_BINARY;
  if (override) {
    if (!fs.existsSync(override)) {
      console.error(`nc-lang: NC_BINARY points to missing file: ${override}`);
      process.exit(1);
    }
    return override;
  }

  const ext = process.platform === "win32" ? ".exe" : "";
  const binaryName = `nc${ext}`;

  // 2. Installed alongside this package
  const localBin = path.join(__dirname, binaryName);
  if (fs.existsSync(localBin)) {
    return localBin;
  }

  // 3. Cached in ~/.nc/bin/
  const homeBin = path.join(os.homedir(), ".nc", "bin", binaryName);
  if (fs.existsSync(homeBin)) {
    return homeBin;
  }

  console.error(
    "nc-lang: NC binary not found.\n" +
      "  Run the postinstall script: node scripts/install.js\n" +
      "  Or reinstall: npm install -g nc-lang"
  );
  process.exit(1);
}

// Locate and run
const binary = findBinary();
const args = process.argv.slice(2);

const child = spawn(binary, args, {
  stdio: "inherit",
  env: process.env,
});

child.on("error", (err) => {
  if (err.code === "ENOENT") {
    console.error(`nc-lang: could not execute ${binary}`);
    console.error("  Try reinstalling: npm install -g nc-lang");
  } else {
    console.error(`nc-lang: ${err.message}`);
  }
  process.exit(1);
});

child.on("exit", (code, signal) => {
  if (signal) {
    process.kill(process.pid, signal);
  } else {
    process.exit(code ?? 1);
  }
});
