# NC Publishing Guide

Complete step-by-step instructions for publishing NC to every major package manager and distribution platform.

**Repository:** `https://github.com/devheallabs-ai/nc-lang`
**License:** Apache-2.0
**Binary name:** `nc`

---

## Table of Contents

1. [Docker Hub](#1-docker-hub)
2. [PyPI (pip)](#2-pypi-pip)
3. [Homebrew](#3-homebrew)
4. [npm](#4-npm)
5. [Chocolatey](#5-chocolatey)
6. [Snap](#6-snap)
7. [Cargo / crates.io](#7-cargo--cratesio)
8. [GitHub Releases](#8-github-releases)
9. [AUR (Arch User Repository)](#9-aur-arch-user-repository)
10. [APT / DEB Repository](#10-apt--deb-repository)
11. [RPM / Fedora](#11-rpm--fedora)
12. [WinGet](#12-winget-windows-package-manager)

---

## 1. Docker Hub

### Prerequisites

- Docker Desktop or Docker Engine installed
- Docker Hub account (https://hub.docker.com)
- `docker buildx` available (ships with Docker Desktop)

### Step 1: Log in to Docker Hub

```bash
docker login
# Enter your Docker Hub username and password/token
```

### Step 2: Create a buildx builder for multi-arch

```bash
docker buildx create --name nc-builder --use
docker buildx inspect --bootstrap
```

### Step 3: Build and push multi-arch image

```bash
# From the repository root (where Dockerfile lives)
VERSION="1.0.0"

# Build + push for amd64 and arm64
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag sainarender2222/nc:${VERSION} \
  --tag sainarender2222/nc:latest \
  --push \
  .
```

### Step 4: Build the slim variant

```bash
docker buildx build \
  --platform linux/amd64,linux/arm64 \
  --tag sainarender2222/nc:${VERSION}-slim \
  --tag sainarender2222/nc:slim \
  --push \
  -f Dockerfile.slim \
  .
```

### Step 5: Verify the pushed images

```bash
docker manifest inspect sainarender2222/nc:latest
docker run --rm sainarender2222/nc:latest version
```

### Docker Hub Automated Builds

1. Go to https://hub.docker.com/repository/create
2. Connect your GitHub account
3. Select the `notation-as-code` repository
4. Under **Build Rules**, add:
   - Source: `/^v([0-9.]+)$/` (tag) -> Docker Tag: `{\1}` -> Dockerfile: `Dockerfile`
   - Source: `main` (branch) -> Docker Tag: `latest` -> Dockerfile: `Dockerfile`

### docker-compose.yml for users

Create `docker-compose.yml` in the repository root:

```yaml
# docker-compose.yml
# Usage:
#   docker compose run nc version
#   docker compose run nc run /app/service.nc
#   docker compose up nc-server

version: "3.9"

services:
  nc:
    image: sainarender2222/nc:latest
    volumes:
      - ./:/app
    working_dir: /app
    entrypoint: ["nc"]
    command: ["version"]

  nc-server:
    image: sainarender2222/nc:latest
    ports:
      - "8080:8080"
    volumes:
      - ./:/app
    working_dir: /app
    entrypoint: ["nc"]
    command: ["serve", "/app/service.nc"]
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "nc", "version"]
      interval: 30s
      timeout: 5s
      retries: 3

  nc-dev:
    build:
      context: .
      dockerfile: Dockerfile.dev
    volumes:
      - ./:/app
    working_dir: /app
    entrypoint: ["nc"]
    command: ["version"]
```

### Testing

```bash
# Test locally before pushing
docker build -t nc:test .
docker run --rm nc:test version
docker run --rm -v $(pwd)/examples:/app nc:test run /app/hello.nc
```

### Common Pitfalls

- Multi-arch builds require QEMU emulation for non-native platforms. Docker Desktop handles this automatically; on Linux CI, run `docker run --privileged --rm tonistiigi/binfmt --install all` first.
- Alpine uses musl libc, not glibc. The Makefile already handles this, but custom C extensions may need adjustment.
- The `--push` flag pushes directly; omit it and use `--load` for local testing (single-platform only with `--load`).

---

## 2. PyPI (pip)

### Prerequisites

- Python 3.8+
- `pip install build twine`
- PyPI account (https://pypi.org/account/register/)
- PyPI API token (https://pypi.org/manage/account/token/)

### Step 1: Create the package structure

```
packaging/pypi/
  nc-lang/
    pyproject.toml
    setup.py
    nc_lang/
      __init__.py
      __main__.py
      _binary.py
    README.md
    LICENSE
```

### Step 2: pyproject.toml

```toml
# packaging/pypi/nc-lang/pyproject.toml

[build-system]
requires = ["setuptools>=68.0", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "nc-lang"
version = "1.0.0"
description = "NC (Notation-as-Code) - The AI Programming Language"
readme = "README.md"
license = {text = "Apache-2.0"}
requires-python = ">=3.8"
authors = [
    {name = "Nuckala Sai Narender", email = "support@devheallabs.in"},
]
keywords = ["nc", "ai", "programming-language", "notation-as-code"]
classifiers = [
    "Development Status :: 4 - Beta",
    "Intended Audience :: Developers",
    "License :: OSI Approved :: Apache Software License",
    "Operating System :: OS Independent",
    "Programming Language :: C",
    "Topic :: Software Development :: Compilers",
    "Topic :: Software Development :: Interpreters",
    "Topic :: Scientific/Engineering :: Artificial Intelligence",
]

[project.urls]
Homepage = "https://github.com/devheallabs-ai/nc-lang"
Repository = "https://github.com/devheallabs-ai/nc-lang"
Issues = "https://github.com/devheallabs-ai/nc-lang/issues"

[project.scripts]
nc = "nc_lang.__main__:main"

[tool.setuptools.packages.find]
include = ["nc_lang*"]
```

### Step 3: setup.py (for binary wheel building)

```python
# packaging/pypi/nc-lang/setup.py

import os
import platform
import subprocess
import shutil
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class NCBuildExt(build_ext):
    """Custom build that compiles the NC binary from C source."""

    def build_extensions(self):
        # Skip default extension building
        pass

    def run(self):
        src_dir = os.path.join(os.path.dirname(__file__), "..", "..", "..", "nc")
        if not os.path.exists(src_dir):
            # If building from sdist, source is bundled
            src_dir = os.path.join(os.path.dirname(__file__), "nc_src")

        build_dir = os.path.join(self.build_lib, "nc_lang", "bin")
        os.makedirs(build_dir, exist_ok=True)

        # Build NC
        subprocess.check_call(["make", "clean"], cwd=src_dir)
        subprocess.check_call(["make"], cwd=src_dir)

        # Copy binary
        system = platform.system()
        if system == "Windows":
            binary_name = "nc.exe"
        else:
            binary_name = "nc"

        src_binary = os.path.join(src_dir, "build", binary_name)
        dst_binary = os.path.join(build_dir, binary_name)
        shutil.copy2(src_binary, dst_binary)
        os.chmod(dst_binary, 0o755)


setup(
    ext_modules=[Extension("nc_lang._placeholder", sources=[])],
    cmdclass={"build_ext": NCBuildExt},
)
```

### Step 4: nc_lang/__init__.py

```python
# packaging/pypi/nc-lang/nc_lang/__init__.py

"""NC (Notation-as-Code) - The AI Programming Language."""

__version__ = "1.0.0"
```

### Step 5: nc_lang/__main__.py

```python
# packaging/pypi/nc-lang/nc_lang/__main__.py

"""Entry point for `python -m nc_lang` and the `nc` console script."""

import os
import sys
import platform
import subprocess


def _find_binary():
    """Locate the NC binary."""
    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    system = platform.system()
    binary_name = "nc.exe" if system == "Windows" else "nc"

    # Check bundled binary first
    bundled = os.path.join(pkg_dir, "bin", binary_name)
    if os.path.isfile(bundled):
        return bundled

    # Fall back to PATH
    from shutil import which
    found = which("nc")
    if found:
        return found

    print("Error: NC binary not found.", file=sys.stderr)
    print("Try reinstalling: pip install --force-reinstall nc-lang", file=sys.stderr)
    sys.exit(1)


def main():
    binary = _find_binary()
    result = subprocess.run([binary] + sys.argv[1:])
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
```

### Step 6: nc_lang/_binary.py

```python
# packaging/pypi/nc-lang/nc_lang/_binary.py

"""
Binary download helper for platforms where we ship pre-built wheels.

Used by the postinstall hook or on first run if the binary is missing.
"""

import os
import sys
import platform
import urllib.request
import zipfile
import stat

VERSION = "1.0.0"
BASE_URL = "https://github.com/devheallabs-ai/nc-lang/releases/download"

PLATFORM_MAP = {
    ("Linux", "x86_64"):   "nc-linux-amd64",
    ("Linux", "aarch64"):  "nc-linux-arm64",
    ("Darwin", "x86_64"):  "nc-darwin-amd64",
    ("Darwin", "arm64"):   "nc-darwin-arm64",
    ("Windows", "AMD64"):  "nc-windows-amd64.exe",
}


def download_binary():
    """Download the correct NC binary for the current platform."""
    system = platform.system()
    machine = platform.machine()
    key = (system, machine)

    if key not in PLATFORM_MAP:
        print(f"Error: No pre-built binary for {system}/{machine}", file=sys.stderr)
        print("Build from source: https://github.com/devheallabs-ai/nc-lang", file=sys.stderr)
        sys.exit(1)

    binary_name = PLATFORM_MAP[key]
    url = f"{BASE_URL}/v{VERSION}/{binary_name}"

    pkg_dir = os.path.dirname(os.path.abspath(__file__))
    bin_dir = os.path.join(pkg_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)

    local_name = "nc.exe" if system == "Windows" else "nc"
    dest = os.path.join(bin_dir, local_name)

    print(f"Downloading NC v{VERSION} for {system}/{machine}...")
    urllib.request.urlretrieve(url, dest)

    # Make executable on Unix
    if system != "Windows":
        st = os.stat(dest)
        os.chmod(dest, st.st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)

    print(f"Installed NC binary to {dest}")
    return dest


if __name__ == "__main__":
    download_binary()
```

### Step 7: Build the package

```bash
cd packaging/pypi/nc-lang

# Build source distribution and wheel
python -m build

# Output goes to dist/
ls dist/
#   nc_lang-1.0.0.tar.gz
#   nc_lang-1.0.0-cp311-cp311-macosx_14_0_arm64.whl  (example)
```

### Step 8: Test locally

```bash
# Install locally to test
pip install dist/nc_lang-1.0.0.tar.gz
nc version

# Or install in editable mode
pip install -e .
```

### Step 9: Upload to Test PyPI first

```bash
# Upload to test index
twine upload --repository testpypi dist/*

# Test install from test index
pip install --index-url https://test.pypi.org/simple/ nc-lang
nc version
```

### Step 10: Upload to production PyPI

```bash
# Upload to production PyPI
twine upload dist/*

# Verify
pip install nc-lang
nc version
```

### Building platform-specific wheels

To create wheels for each platform, build on each target OS/arch or use CI:

```bash
# On macOS arm64
python -m build --wheel
# Produces: nc_lang-1.0.0-cp311-cp311-macosx_14_0_arm64.whl

# On Linux x86_64
python -m build --wheel
# Produces: nc_lang-1.0.0-cp311-cp311-manylinux_2_17_x86_64.whl
```

For manylinux compliance, build inside the official manylinux Docker container:

```bash
docker run -v $(pwd):/io quay.io/pypa/manylinux2014_x86_64 \
  bash -c "cd /io && python3 -m build --wheel"
```

### Common Pitfalls

- Package name `nc-lang` (with hyphen) becomes `nc_lang` (with underscore) as the import name. This is standard Python behavior.
- PyPI does not allow re-uploading the same version. Bump the version number for every upload.
- Always test on Test PyPI first to catch metadata issues.
- For binary wheels, you must build separately on each platform (or use cibuildwheel in CI).

---

## 3. Homebrew

### Prerequisites

- macOS or Linux with Homebrew installed
- A GitHub account
- A GitHub repository for the tap

### Step 1: Create the Homebrew tap repository

```bash
# Create a new GitHub repo named "homebrew-tap"
gh repo create nc-lang/homebrew-tap --public --description "Homebrew tap for NC"
```

### Step 2: Formula file

The formula already exists at `Formula/nc.rb`. Copy it to the tap:

```ruby
# Formula/nc.rb (already in repo)

class Nc < Formula
  desc "NC -- The AI Language. Write AI APIs in plain English."
  homepage "https://github.com/devheallabs-ai/nc-lang"
  url "https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "REPLACE_WITH_ACTUAL_SHA256"
  license "Apache-2.0"
  head "https://github.com/devheallabs-ai/nc-lang.git", branch: "main"

  depends_on "curl"

  def install
    cd "nc" do
      system "make", "clean"
      system "make"
      bin.install "build/nc"
    end

    pkgshare.install "nc_ai_providers.json"
    pkgshare.install "Lib"
    pkgshare.install "examples"
  end

  def caveats
    <<~EOS
      To use AI features, set your API key:
        export NC_AI_KEY="your-api-key"

      Quick start:
        nc version
        nc "show 42 + 8"
        nc serve service.nc
    EOS
  end

  test do
    assert_match "NC", shell_output("#{bin}/nc version")

    (testpath/"hello.nc").write('show "hello from NC"')
    assert_match "hello from NC", shell_output("#{bin}/nc run #{testpath}/hello.nc")
  end
end
```

### Step 3: Compute SHA256 after creating a GitHub release

```bash
# After creating the GitHub release v1.0.0:
curl -sL https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz \
  | shasum -a 256

# Update the sha256 line in Formula/nc.rb with the output
```

### Step 4: Push formula to the tap

```bash
git clone https://github.com/nc-lang/homebrew-tap.git
mkdir -p homebrew-tap/Formula
cp Formula/nc.rb homebrew-tap/Formula/nc.rb
cd homebrew-tap
git add Formula/nc.rb
git commit -m "Add NC formula v1.0.0"
git push origin main
```

### Step 5: Test the formula locally

```bash
# Install from local formula file
brew install --build-from-source ./Formula/nc.rb

# Or audit the formula for issues
brew audit --strict --new-formula Formula/nc.rb

# Run the formula tests
brew test nc
```

### Step 6: Users install via tap

```bash
brew tap nc-lang/tap
brew install nc
```

### Updating the formula for new releases

```bash
# Compute new sha256
curl -sL https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz \
  | shasum -a 256

# Update url and sha256 in the formula, then push
```

### Common Pitfalls

- The tap repository must be named `homebrew-tap` (with the `homebrew-` prefix) so that `brew tap nc-lang/tap` works.
- `brew audit --strict` will catch common issues. Run it before pushing.
- If `sha256` is empty or wrong, Homebrew will refuse to install.
- The `head` stanza lets users install from git main: `brew install --HEAD nc`.

---

## 4. npm

### Prerequisites

- Node.js 16+ and npm
- npmjs.com account (https://www.npmjs.com/signup)
- `npm login` completed

### Step 1: Create the package structure

```
packaging/npm/nc-lang/
  package.json
  index.js
  install.js
  bin/
    nc          (placeholder, replaced by postinstall)
  README.md
  LICENSE
```

### Step 2: package.json

```json
{
  "name": "nc-lang",
  "version": "1.0.0",
  "description": "NC (Notation-as-Code) - The AI Programming Language. Write AI APIs in plain English.",
  "keywords": ["nc", "ai", "programming-language", "notation-as-code", "compiler"],
  "homepage": "https://github.com/devheallabs-ai/nc-lang",
  "repository": {
    "type": "git",
    "url": "https://github.com/devheallabs-ai/nc-lang.git"
  },
  "license": "Apache-2.0",
  "author": "Nuckala Sai Narender <support@devheallabs.in>",
  "bin": {
    "nc": "./bin/nc-wrapper.js"
  },
  "scripts": {
    "postinstall": "node install.js"
  },
  "engines": {
    "node": ">=16"
  },
  "os": ["darwin", "linux", "win32"],
  "cpu": ["x64", "arm64"]
}
```

### Step 3: install.js (postinstall script)

```javascript
// packaging/npm/nc-lang/install.js

"use strict";

const https = require("https");
const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");

const VERSION = "1.0.0";
const GITHUB_RELEASE = `https://github.com/devheallabs-ai/nc-lang/releases/download/v${VERSION}`;

const PLATFORMS = {
  "darwin-x64":   "nc-darwin-amd64",
  "darwin-arm64": "nc-darwin-arm64",
  "linux-x64":    "nc-linux-amd64",
  "linux-arm64":  "nc-linux-arm64",
  "win32-x64":    "nc-windows-amd64.exe",
};

function getPlatformKey() {
  return `${process.platform}-${process.arch}`;
}

function download(url, dest) {
  return new Promise((resolve, reject) => {
    const follow = (url) => {
      https.get(url, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          follow(res.headers.location);
          return;
        }
        if (res.statusCode !== 200) {
          reject(new Error(`Download failed: HTTP ${res.statusCode} for ${url}`));
          return;
        }
        const file = fs.createWriteStream(dest);
        res.pipe(file);
        file.on("finish", () => { file.close(); resolve(); });
        file.on("error", reject);
      }).on("error", reject);
    };
    follow(url);
  });
}

async function main() {
  const key = getPlatformKey();
  const binaryName = PLATFORMS[key];

  if (!binaryName) {
    console.error(`Unsupported platform: ${key}`);
    console.error("Build from source: https://github.com/devheallabs-ai/nc-lang");
    process.exit(1);
  }

  const url = `${GITHUB_RELEASE}/${binaryName}`;
  const binDir = path.join(__dirname, "bin");

  if (!fs.existsSync(binDir)) {
    fs.mkdirSync(binDir, { recursive: true });
  }

  const localName = process.platform === "win32" ? "nc.exe" : "nc";
  const dest = path.join(binDir, localName);

  console.log(`Downloading NC v${VERSION} for ${key}...`);

  try {
    await download(url, dest);
    fs.chmodSync(dest, 0o755);
    console.log(`NC installed successfully: ${dest}`);
  } catch (err) {
    console.error(`Failed to download NC: ${err.message}`);
    console.error("You can build from source instead.");
    process.exit(1);
  }
}

main();
```

### Step 4: bin/nc-wrapper.js

```javascript
#!/usr/bin/env node

// packaging/npm/nc-lang/bin/nc-wrapper.js

"use strict";

const path = require("path");
const { execFileSync } = require("child_process");

const ext = process.platform === "win32" ? ".exe" : "";
const binary = path.join(__dirname, `nc${ext}`);

try {
  execFileSync(binary, process.argv.slice(2), { stdio: "inherit" });
} catch (err) {
  if (err.status != null) {
    process.exit(err.status);
  }
  console.error("Failed to run NC binary. Try reinstalling: npm install -g nc-lang");
  process.exit(1);
}
```

### Step 5: Build and publish

```bash
cd packaging/npm/nc-lang

# Login to npm
npm login

# Do a dry run first
npm publish --dry-run

# Publish
npm publish --access public

# Verify
npm info nc-lang
```

### Step 6: Users install and run

```bash
# Global install
npm install -g nc-lang
nc version

# Or run without installing
npx nc-lang version
```

### Testing locally

```bash
# Pack and install locally
npm pack
npm install -g nc-lang-1.0.0.tgz
nc version

# Clean up
npm uninstall -g nc-lang
```

### Common Pitfalls

- The `postinstall` script runs on `npm install`. Some corporate environments block post-install scripts (`--ignore-scripts`). Document a manual binary download fallback.
- Follow GitHub redirects in the download script (GitHub releases redirect to S3/CDN).
- npm requires 2FA for publishing. Set it up before your first publish.
- The `bin/nc-wrapper.js` file needs the `#!/usr/bin/env node` shebang and must be committed with executable permissions.

---

## 5. Chocolatey

### Prerequisites

- Windows machine (or Windows VM/CI)
- Chocolatey installed (https://chocolatey.org/install)
- Chocolatey account and API key (https://community.chocolatey.org/account)

### Step 1: Create the package structure

```
packaging/chocolatey/nc/
  nc.nuspec
  tools/
    chocolateyInstall.ps1
    chocolateyUninstall.ps1
    LICENSE.txt
    VERIFICATION.txt
```

### Step 2: nc.nuspec

```xml
<?xml version="1.0" encoding="utf-8"?>
<!-- packaging/chocolatey/nc/nc.nuspec -->
<package xmlns="http://schemas.chocolatey.org/2010/07/nuspec.xsd">
  <metadata>
    <id>nc</id>
    <version>1.0.0</version>
    <title>NC - The AI Programming Language</title>
    <authors>Nuckala Sai Narender</authors>
    <owners>sainarender2222</owners>
    <projectUrl>https://github.com/devheallabs-ai/nc-lang</projectUrl>
    <licenseUrl>https://github.com/devheallabs-ai/nc-lang/blob/main/LICENSE</licenseUrl>
    <iconUrl>https://raw.githubusercontent.com/sainarender2222/notation-as-code/main/assets/nc-icon.png</iconUrl>
    <requireLicenseAcceptance>false</requireLicenseAcceptance>
    <description>
NC (Notation-as-Code) is the AI Programming Language. Write AI-powered APIs, services, and automations in plain English notation.

## Features
- Write AI APIs in plain English
- Built-in HTTP server
- Multi-provider AI support
- Zero-config deployment
    </description>
    <summary>NC - Write AI APIs in plain English</summary>
    <tags>nc ai programming-language compiler notation-as-code</tags>
    <releaseNotes>https://github.com/devheallabs-ai/nc-lang/releases/tag/v1.0.0</releaseNotes>
    <copyright>Copyright 2024-2026 Nuckala Sai Narender</copyright>
  </metadata>
  <files>
    <file src="tools\**" target="tools" />
  </files>
</package>
```

### Step 3: chocolateyInstall.ps1

```powershell
# packaging/chocolatey/nc/tools/chocolateyInstall.ps1

$ErrorActionPreference = 'Stop'

$packageName = 'nc'
$version     = '1.0.0'
$url64       = "https://github.com/devheallabs-ai/nc-lang/releases/download/v$version/nc-windows-amd64.exe"
$checksum64  = 'REPLACE_WITH_SHA256_OF_WINDOWS_BINARY'

$toolsDir = "$(Split-Path -Parent $MyInvocation.MyCommand.Definition)"
$installDir = Join-Path $toolsDir 'bin'

if (!(Test-Path $installDir)) {
    New-Item -ItemType Directory -Path $installDir | Out-Null
}

$ncExe = Join-Path $installDir 'nc.exe'

Get-ChocolateyWebFile -PackageName $packageName `
    -FileFullPath $ncExe `
    -Url64bit $url64 `
    -Checksum64 $checksum64 `
    -ChecksumType64 'sha256'

# Add to PATH via shim
Install-BinFile -Name 'nc' -Path $ncExe

Write-Host "NC $version installed. Run 'nc version' to verify."
```

### Step 4: chocolateyUninstall.ps1

```powershell
# packaging/chocolatey/nc/tools/chocolateyUninstall.ps1

$ErrorActionPreference = 'Stop'

Uninstall-BinFile -Name 'nc'

Write-Host "NC has been uninstalled."
```

### Step 5: tools/VERIFICATION.txt

```
VERIFICATION

To verify the NC binary:

1. Download the binary from:
   https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-windows-amd64.exe

2. Compute SHA256:
   Get-FileHash nc-windows-amd64.exe -Algorithm SHA256

3. Compare with the checksum in chocolateyInstall.ps1.

Licensed under Apache-2.0.
```

### Step 6: Build and publish

```powershell
cd packaging\chocolatey\nc

# Pack the .nupkg
choco pack nc.nuspec

# Test locally
choco install nc --source="'.'" --force

# Verify
nc version

# Push to Chocolatey community repo
choco apikey --key YOUR_API_KEY --source https://push.chocolatey.org/
choco push nc.1.0.0.nupkg --source https://push.chocolatey.org/
```

### Testing

```powershell
# Install from local .nupkg
choco install nc -s . --force

# Run tests
nc version
nc "show 42 + 8"

# Uninstall
choco uninstall nc
```

### Common Pitfalls

- Chocolatey community packages go through moderation (can take days). Start the submission process early.
- Always include `VERIFICATION.txt` for packages that embed or download binaries.
- SHA256 checksums must match exactly, or the install will fail.
- Test on a clean Windows VM to catch PATH and dependency issues.

---

## 6. Snap

### Prerequisites

- Ubuntu/Linux machine
- `sudo snap install snapcraft --classic`
- Snap Store account (https://snapcraft.io/account)

### Step 1: snapcraft.yaml

Create `packaging/snap/snapcraft.yaml`:

```yaml
# packaging/snap/snapcraft.yaml

name: nc
base: core22
version: '1.0.0'
summary: NC - The AI Programming Language
description: |
  NC (Notation-as-Code) is the AI programming language. Write AI-powered
  APIs, services, and automations in plain English notation.

  Features:
  - Write AI APIs in plain English
  - Built-in HTTP server with middleware support
  - Multi-provider AI support (NC AI built-in, or external gateways)
  - Zero-config deployment

  Quick start:
    nc version
    nc "show 42 + 8"
    nc serve service.nc

grade: stable
confinement: strict

license: Apache-2.0

architectures:
  - build-on: amd64
  - build-on: arm64

apps:
  nc:
    command: bin/nc
    plugs:
      - network
      - network-bind
      - home

parts:
  nc:
    plugin: make
    source: https://github.com/devheallabs-ai/nc-lang.git
    source-tag: v1.0.0
    source-subdir: nc
    build-packages:
      - gcc
      - make
      - libcurl4-openssl-dev
    stage-packages:
      - libcurl4
    override-build: |
      cd $CRAFT_PART_SRC
      make clean
      make
      mkdir -p $CRAFT_PART_INSTALL/bin
      cp build/nc $CRAFT_PART_INSTALL/bin/nc

  nc-lib:
    plugin: dump
    source: https://github.com/devheallabs-ai/nc-lang.git
    source-tag: v1.0.0
    organize:
      Lib/*: lib/nc/
    stage:
      - lib/nc/**
```

### Step 2: Build the snap

```bash
cd packaging/snap

# Build locally
snapcraft

# Output: nc_1.0.0_amd64.snap
```

### Step 3: Test locally

```bash
# Install the locally built snap
sudo snap install nc_1.0.0_amd64.snap --dangerous

# Test
nc version
nc "show 42 + 8"

# Remove
sudo snap remove nc
```

### Step 4: Publish to Snap Store

```bash
# Login to Snap Store
snapcraft login

# Register the snap name (one-time)
snapcraft register nc

# Upload and release
snapcraft upload nc_1.0.0_amd64.snap --release=stable
```

### Step 5: Users install

```bash
sudo snap install nc
nc version
```

### Common Pitfalls

- Snap confinement (`strict`) restricts filesystem access. Users can only access `$HOME` via the `home` plug.
- The `network` and `network-bind` plugs are needed for NC's HTTP server and AI API calls.
- Building for arm64 on an amd64 machine requires `snapcraft --use-lxd` with a multipass or LXD VM.
- The snap name `nc` may be taken (there is a traditional `nc`/netcat). If so, use `nc-lang` as the snap name.

---

## 7. Cargo / crates.io

### Prerequisites

- Rust toolchain (`rustup`)
- crates.io account (log in with GitHub at https://crates.io)
- `cargo login` with API token

### Step 1: Create a thin wrapper crate

```
packaging/cargo/nc-lang/
  Cargo.toml
  src/
    main.rs
    lib.rs
  build.rs
```

### Step 2: Cargo.toml

```toml
# packaging/cargo/nc-lang/Cargo.toml

[package]
name = "nc-lang"
version = "1.0.0"
edition = "2021"
authors = ["Nuckala Sai Narender <support@devheallabs.in>"]
description = "NC (Notation-as-Code) - The AI Programming Language"
license = "Apache-2.0"
repository = "https://github.com/devheallabs-ai/nc-lang"
homepage = "https://github.com/devheallabs-ai/nc-lang"
keywords = ["nc", "ai", "programming-language", "notation-as-code"]
categories = ["command-line-utilities", "compilers", "development-tools"]
readme = "README.md"

[[bin]]
name = "nc"
path = "src/main.rs"

[build-dependencies]
cc = "1.0"
```

### Step 3: build.rs

```rust
// packaging/cargo/nc-lang/build.rs

use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());

    // Clone NC source if not present
    let nc_src = out_dir.join("nc-src");
    if !nc_src.exists() {
        Command::new("git")
            .args(["clone", "--depth", "1", "--branch", "v1.0.0",
                   "https://github.com/devheallabs-ai/nc-lang.git"])
            .arg(&nc_src)
            .status()
            .expect("Failed to clone NC source");
    }

    // Build using make
    let nc_dir = nc_src.join("nc");
    Command::new("make")
        .arg("clean")
        .current_dir(&nc_dir)
        .status()
        .expect("make clean failed");

    Command::new("make")
        .current_dir(&nc_dir)
        .status()
        .expect("make failed");

    // Copy the built binary
    let binary = nc_dir.join("build").join("nc");
    let dest = out_dir.join("nc");
    std::fs::copy(&binary, &dest).expect("Failed to copy NC binary");

    println!("cargo:rustc-env=NC_BINARY={}", dest.display());
}
```

### Step 4: src/main.rs

```rust
// packaging/cargo/nc-lang/src/main.rs

use std::os::unix::process::CommandExt;
use std::process::Command;

const NC_BINARY: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/nc"));

fn main() {
    // Write binary to a temp location and execute
    let tmp = std::env::temp_dir().join("nc-lang-bin");
    std::fs::create_dir_all(&tmp).unwrap();
    let binary_path = tmp.join("nc");

    if !binary_path.exists() {
        std::fs::write(&binary_path, NC_BINARY).unwrap();
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            std::fs::set_permissions(&binary_path,
                std::fs::Permissions::from_mode(0o755)).unwrap();
        }
    }

    let args: Vec<String> = std::env::args().skip(1).collect();

    #[cfg(unix)]
    {
        let err = Command::new(&binary_path).args(&args).exec();
        eprintln!("Failed to exec NC: {}", err);
        std::process::exit(1);
    }

    #[cfg(not(unix))]
    {
        let status = Command::new(&binary_path).args(&args).status().unwrap();
        std::process::exit(status.code().unwrap_or(1));
    }
}
```

### Step 5: src/lib.rs

```rust
// packaging/cargo/nc-lang/src/lib.rs

//! NC (Notation-as-Code) - The AI Programming Language
//!
//! This crate provides a Rust wrapper for the NC binary.
//! Install with `cargo install nc-lang` and run with `nc`.
//!
//! For more information, see: https://github.com/devheallabs-ai/nc-lang

pub const VERSION: &str = "1.0.0";
```

### Step 6: Publish

```bash
cd packaging/cargo/nc-lang

# Check the package
cargo package --list
cargo publish --dry-run

# Publish
cargo publish

# Verify
cargo install nc-lang
nc version
```

### Common Pitfalls

- crates.io has a 10MB upload limit. The `include_bytes!` approach embeds the binary in the crate, which may exceed this. If so, use the download-at-build approach instead.
- The crate name `nc` is likely taken. Use `nc-lang`.
- `build.rs` requires network access during `cargo install`, which some restricted environments disallow.
- Cross-compilation is tricky since the C build is platform-specific. Document that `cargo install` must happen on the target platform.

---

## 8. GitHub Releases

### Prerequisites

- GitHub account with push access to the repository
- `gh` CLI installed (`brew install gh`)

### Step 1: Binary naming convention

```
nc-linux-amd64
nc-linux-arm64
nc-darwin-amd64
nc-darwin-arm64
nc-windows-amd64.exe
```

### Step 2: GitHub Actions release workflow

Create `.github/workflows/release.yml`:

```yaml
# .github/workflows/release.yml

name: Release

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write

jobs:
  build:
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            arch: amd64
            target: linux-amd64
            cc: gcc
            ldflags: "-lm -lcurl -lpthread -ldl"
          - os: ubuntu-24.04-arm
            arch: arm64
            target: linux-arm64
            cc: gcc
            ldflags: "-lm -lcurl -lpthread -ldl"
          - os: macos-13
            arch: amd64
            target: darwin-amd64
            cc: clang
            ldflags: "-lm -lcurl -lpthread -ldl"
          - os: macos-14
            arch: arm64
            target: darwin-arm64
            cc: clang
            ldflags: "-lm -lcurl -lpthread -ldl"
          - os: windows-latest
            arch: amd64
            target: windows-amd64
            cc: gcc
            ldflags: "-lm -lwinhttp -lws2_32"

    runs-on: ${{ matrix.os }}

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies (Linux)
        if: runner.os == 'Linux'
        run: sudo apt-get update && sudo apt-get install -y gcc make libcurl4-openssl-dev

      - name: Install dependencies (Windows)
        if: runner.os == 'Windows'
        uses: msys2/setup-msys2@v2
        with:
          msystem: MINGW64
          install: mingw-w64-x86_64-gcc make

      - name: Build NC
        if: runner.os != 'Windows'
        run: |
          cd nc
          make clean
          make CC=${{ matrix.cc }}
          mv build/nc ../nc-${{ matrix.target }}

      - name: Build NC (Windows)
        if: runner.os == 'Windows'
        shell: msys2 {0}
        run: |
          cd nc
          make clean
          make
          mv build/nc.exe ../nc-${{ matrix.target }}.exe

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: nc-${{ matrix.target }}
          path: nc-${{ matrix.target }}*

  release:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts/

      - name: Prepare release assets
        run: |
          mkdir -p release
          find artifacts -type f -name 'nc-*' -exec cp {} release/ \;
          cd release
          sha256sum nc-* > checksums-sha256.txt
          cat checksums-sha256.txt

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          files: release/*
          generate_release_notes: true
          body: |
            ## Installation

            **macOS (Homebrew):**
            ```bash
            brew tap nc-lang/tap
            brew install nc
            ```

            **pip:**
            ```bash
            pip install nc-lang
            ```

            **npm:**
            ```bash
            npm install -g nc-lang
            ```

            **Docker:**
            ```bash
            docker pull sainarender2222/nc:${{ github.ref_name }}
            ```

            **Binary download:**
            Download the binary for your platform below, make it executable, and add to PATH.

            ## Checksums
            See `checksums-sha256.txt` for SHA256 verification.
          draft: false
          prerelease: false
```

### Step 3: Create a release manually (alternative)

```bash
VERSION="1.0.0"

# Tag the release
git tag -a "v${VERSION}" -m "Release v${VERSION}"
git push origin "v${VERSION}"

# Or create release manually with gh CLI
gh release create "v${VERSION}" \
  --title "NC v${VERSION}" \
  --generate-notes \
  nc-linux-amd64 \
  nc-linux-arm64 \
  nc-darwin-amd64 \
  nc-darwin-arm64 \
  nc-windows-amd64.exe \
  checksums-sha256.txt
```

### Step 4: Generate checksums

```bash
cd release/
sha256sum nc-* > checksums-sha256.txt
# or on macOS:
shasum -a 256 nc-* > checksums-sha256.txt
```

### Testing

```bash
# Test the workflow on a branch before tagging
gh workflow run release.yml --ref my-test-branch

# After release, verify downloads
gh release download v1.0.0 --pattern 'nc-darwin-arm64'
chmod +x nc-darwin-arm64
./nc-darwin-arm64 version
```

### Common Pitfalls

- The tag must match the `on.push.tags` pattern (e.g., `v1.0.0`). Pushing a tag without the `v` prefix will not trigger the workflow.
- Windows builds require MSYS2 in CI. The `msys2/setup-msys2` action handles this.
- Artifacts from `actions/upload-artifact` are stored in nested directories. The `find` command in the release job flattens them.
- `softprops/action-gh-release` requires `permissions: contents: write`.

---

## 9. AUR (Arch User Repository)

### Prerequisites

- Arch Linux (or Manjaro, EndeavourOS, etc.)
- AUR account (https://aur.archlinux.org/register)
- SSH key added to AUR account

### Step 1: PKGBUILD

Create `packaging/aur/nc/PKGBUILD`:

```bash
# packaging/aur/nc/PKGBUILD

# Maintainer: Nuckala Sai Narender <support@devheallabs.in>

pkgname=nc-lang
pkgver=1.0.0
pkgrel=1
pkgdesc="NC (Notation-as-Code) - The AI Programming Language"
arch=('x86_64' 'aarch64')
url="https://github.com/devheallabs-ai/nc-lang"
license=('Apache-2.0')
depends=('curl')
makedepends=('gcc' 'make')
source=("${pkgname}-${pkgver}.tar.gz::https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('REPLACE_WITH_ACTUAL_SHA256')

build() {
    cd "notation-as-code-${pkgver}/nc"
    make clean
    make
}

package() {
    cd "notation-as-code-${pkgver}"

    # Install binary
    install -Dm755 engine/build/nc "${pkgdir}/usr/bin/nc"

    # Install standard library
    install -dm755 "${pkgdir}/usr/share/nc/lib"
    cp -r lib/* "${pkgdir}/usr/share/nc/lib/"

    # Install examples
    install -dm755 "${pkgdir}/usr/share/nc/examples"
    cp -r examples/* "${pkgdir}/usr/share/nc/examples/"

    # Install license
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    # Install AI providers config
    install -Dm644 nc_ai_providers.json "${pkgdir}/usr/share/nc/nc_ai_providers.json"
}
```

### Step 2: .SRCINFO

Generate this after writing the PKGBUILD:

```bash
cd packaging/aur/nc
makepkg --printsrcinfo > .SRCINFO
```

### Step 3: Create and push to AUR

```bash
# Clone the AUR package (first time)
git clone ssh://aur@aur.archlinux.org/nc-lang.git aur-nc-lang
cd aur-nc-lang

# Copy files
cp ../packaging/aur/nc/PKGBUILD .
makepkg --printsrcinfo > .SRCINFO

# Commit and push
git add PKGBUILD .SRCINFO
git commit -m "Initial upload: nc-lang 1.0.0"
git push origin master
```

### Step 4: Test locally

```bash
cd packaging/aur/nc

# Build the package
makepkg -si

# Test
nc version

# Or use an AUR helper
yay -S nc-lang
```

### Step 5: Also create nc-lang-bin (pre-built binary variant)

```bash
# packaging/aur/nc-bin/PKGBUILD

pkgname=nc-lang-bin
pkgver=1.0.0
pkgrel=1
pkgdesc="NC (Notation-as-Code) - The AI Programming Language (pre-built binary)"
arch=('x86_64' 'aarch64')
url="https://github.com/devheallabs-ai/nc-lang"
license=('Apache-2.0')
depends=('curl')
provides=('nc-lang')
conflicts=('nc-lang')
source_x86_64=("nc-linux-amd64-${pkgver}::https://github.com/devheallabs-ai/nc-lang/releases/download/v${pkgver}/nc-linux-amd64")
source_aarch64=("nc-linux-arm64-${pkgver}::https://github.com/devheallabs-ai/nc-lang/releases/download/v${pkgver}/nc-linux-arm64")
sha256sums_x86_64=('REPLACE_WITH_AMD64_SHA256')
sha256sums_aarch64=('REPLACE_WITH_ARM64_SHA256')

package() {
    if [ "$CARCH" = "x86_64" ]; then
        install -Dm755 "nc-linux-amd64-${pkgver}" "${pkgdir}/usr/bin/nc"
    else
        install -Dm755 "nc-linux-arm64-${pkgver}" "${pkgdir}/usr/bin/nc"
    fi
}
```

### Common Pitfalls

- AUR package names must be unique. Check https://aur.archlinux.org/packages/ first.
- The `.SRCINFO` file must be regenerated every time you change `PKGBUILD`.
- AUR uses git over SSH. Make sure your SSH key is configured.
- Use `namcap PKGBUILD` to lint the PKGBUILD for common issues.

---

## 10. APT / DEB Repository

### Prerequisites

- Debian/Ubuntu system (or Docker container)
- `sudo apt install dpkg-dev debhelper devscripts gpg`
- GPG key for signing

### Step 1: Create .deb package structure

```
packaging/deb/
  nc-lang_1.0.0/
    DEBIAN/
      control
      postinst
      prerm
    usr/
      bin/
        nc          (the compiled binary)
      share/
        nc/
          lib/      (NC standard library)
        doc/
          nc-lang/
            copyright
```

### Step 2: DEBIAN/control

```
Package: nc-lang
Version: 1.0.0
Section: devel
Priority: optional
Architecture: amd64
Depends: libcurl4 (>= 7.68.0)
Maintainer: Nuckala Sai Narender <support@devheallabs.in>
Description: NC (Notation-as-Code) - The AI Programming Language
 NC is the AI programming language. Write AI-powered APIs,
 services, and automations in plain English notation.
 .
 Features include a built-in HTTP server, multi-provider AI
 support, and zero-config deployment.
Homepage: https://github.com/devheallabs-ai/nc-lang
```

### Step 3: DEBIAN/postinst

```bash
#!/bin/bash
set -e
echo "NC installed successfully. Run 'nc version' to verify."
```

### Step 4: DEBIAN/prerm

```bash
#!/bin/bash
set -e
# Nothing to do for now
```

### Step 5: Build the .deb

```bash
#!/bin/bash
# packaging/deb/build-deb.sh

set -e

VERSION="1.0.0"
ARCH="amd64"  # or arm64
PKG_DIR="nc-lang_${VERSION}_${ARCH}"

# Clean and create structure
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}/DEBIAN"
mkdir -p "${PKG_DIR}/usr/bin"
mkdir -p "${PKG_DIR}/usr/share/nc/lib"
mkdir -p "${PKG_DIR}/usr/share/doc/nc-lang"

# Copy control files
cp control "${PKG_DIR}/DEBIAN/"
cp postinst "${PKG_DIR}/DEBIAN/"
cp prerm "${PKG_DIR}/DEBIAN/"
chmod 755 "${PKG_DIR}/DEBIAN/postinst" "${PKG_DIR}/DEBIAN/prerm"

# Build NC from source
cd ../../engine
make clean && make
cd ../packaging/deb

# Copy binary
cp ../../engine/build/nc "${PKG_DIR}/usr/bin/nc"
chmod 755 "${PKG_DIR}/usr/bin/nc"

# Copy standard library
cp -r ../../lib/* "${PKG_DIR}/usr/share/nc/lib/"

# Copy copyright
cp ../../LICENSE "${PKG_DIR}/usr/share/doc/nc-lang/copyright"

# Set permissions
find "${PKG_DIR}" -type d -exec chmod 755 {} \;

# Build the .deb
dpkg-deb --build --root-owner-group "${PKG_DIR}"

echo "Built: ${PKG_DIR}.deb"
echo "Install with: sudo dpkg -i ${PKG_DIR}.deb"
```

### Step 6: Test the .deb

```bash
# Install
sudo dpkg -i nc-lang_1.0.0_amd64.deb

# Fix any dependency issues
sudo apt-get install -f

# Test
nc version

# Remove
sudo dpkg -r nc-lang
```

### Step 7: Set up a custom APT repository

```bash
# Create repo structure
mkdir -p apt-repo/pool/main
mkdir -p apt-repo/dists/stable/main/binary-amd64
mkdir -p apt-repo/dists/stable/main/binary-arm64

# Copy .deb files
cp nc-lang_1.0.0_amd64.deb apt-repo/pool/main/
cp nc-lang_1.0.0_arm64.deb apt-repo/pool/main/

# Generate package index
cd apt-repo
dpkg-scanpackages pool/main /dev/null > dists/stable/main/binary-amd64/Packages
gzip -k dists/stable/main/binary-amd64/Packages

# Create Release file
cd dists/stable
cat > Release <<EOF
Origin: NC
Label: NC
Suite: stable
Codename: stable
Architectures: amd64 arm64
Components: main
Description: NC (Notation-as-Code) APT Repository
EOF

apt-ftparchive release . >> Release

# Sign with GPG
gpg --default-key YOUR_GPG_KEY_ID -abs -o Release.gpg Release
gpg --default-key YOUR_GPG_KEY_ID --clearsign -o InRelease Release
```

### Step 8: Host the repository

Host the `apt-repo/` directory on any static file server (GitHub Pages, S3, your own server).

### Step 9: Users add the repository

```bash
# Add GPG key
curl -fsSL https://nc-lang.dev/gpg-key.asc | sudo gpg --dearmor -o /usr/share/keyrings/nc-lang.gpg

# Add repository
echo "deb [signed-by=/usr/share/keyrings/nc-lang.gpg] https://apt.nc-lang.dev stable main" \
  | sudo tee /etc/apt/sources.list.d/nc-lang.list

# Install
sudo apt update
sudo apt install nc-lang
```

### Using a PPA (Ubuntu-specific alternative)

```bash
# Requires a Launchpad account (https://launchpad.net)
# PPAs are Ubuntu-specific and build from source on Launchpad servers

# Install packaging tools
sudo apt install dput devscripts

# Create source package and upload
debuild -S -sa
dput ppa:sainarender2222/nc-lang ../*.changes
```

### Common Pitfalls

- The `Architecture` field in `control` must match the binary. Build separately for each architecture.
- `dpkg-deb` requires specific directory permissions: dirs must be 755, control scripts must be 755.
- When hosting your own repo, the GPG signing is essential. Without it, `apt` will refuse to install with default settings.
- The `Depends` line must list runtime dependencies (not build dependencies).

---

## 11. RPM / Fedora

### Prerequisites

- Fedora/RHEL/CentOS system (or Docker container)
- `sudo dnf install rpm-build rpmdevtools gcc make libcurl-devel`
- COPR account (https://copr.fedorainfracloud.org/)

### Step 1: Set up RPM build environment

```bash
rpmdev-setuptree
# Creates ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
```

### Step 2: Download source tarball

```bash
cd ~/rpmbuild/SOURCES
curl -L -o nc-lang-1.0.0.tar.gz \
  https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v1.0.0.tar.gz
```

### Step 3: nc-lang.spec

Create `packaging/rpm/nc-lang.spec`:

```spec
# packaging/rpm/nc-lang.spec

Name:           nc-lang
Version:        1.0.0
Release:        1%{?dist}
Summary:        NC (Notation-as-Code) - The AI Programming Language

License:        Apache-2.0
URL:            https://github.com/devheallabs-ai/nc-lang
Source0:        https://github.com/devheallabs-ai/nc-lang/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  libcurl-devel

Requires:       libcurl

%description
NC (Notation-as-Code) is the AI programming language. Write AI-powered
APIs, services, and automations in plain English notation.

Features:
- Write AI APIs in plain English
- Built-in HTTP server with middleware support
- Multi-provider AI support (NC AI built-in, or external gateways)
- Zero-config deployment

%prep
%autosetup -n notation-as-code-%{version}

%build
cd engine
make clean
%make_build

%install
# Binary
install -Dm755 engine/build/nc %{buildroot}%{_bindir}/nc

# Standard library
install -dm755 %{buildroot}%{_datadir}/nc/lib
cp -r lib/* %{buildroot}%{_datadir}/nc/lib/

# Examples
install -dm755 %{buildroot}%{_datadir}/nc/examples
cp -r examples/* %{buildroot}%{_datadir}/nc/examples/

# AI providers config
install -Dm644 nc_ai_providers.json %{buildroot}%{_datadir}/nc/nc_ai_providers.json

# License
install -Dm644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE

%files
%license LICENSE
%{_bindir}/nc
%{_datadir}/nc/

%changelog
* Sun Mar 22 2026 Nuckala Sai Narender <support@devheallabs.in> - 1.0.0-1
- Initial RPM release
```

### Step 4: Build the RPM

```bash
# Copy spec file
cp packaging/rpm/nc-lang.spec ~/rpmbuild/SPECS/

# Build SRPM and RPM
rpmbuild -ba ~/rpmbuild/SPECS/nc-lang.spec

# Output:
#   ~/rpmbuild/RPMS/x86_64/nc-lang-1.0.0-1.fc39.x86_64.rpm
#   ~/rpmbuild/SRPMS/nc-lang-1.0.0-1.fc39.src.rpm
```

### Step 5: Test locally

```bash
# Install
sudo dnf install ~/rpmbuild/RPMS/x86_64/nc-lang-1.0.0-1.fc39.x86_64.rpm

# Test
nc version

# Remove
sudo dnf remove nc-lang
```

### Step 6: Set up COPR repository

```bash
# Install copr-cli
sudo dnf install copr-cli

# Create API token at https://copr.fedorainfracloud.org/api/
# Save to ~/.config/copr

# Create COPR project
copr-cli create nc-lang \
  --chroot fedora-rawhide-x86_64 \
  --chroot fedora-39-x86_64 \
  --chroot fedora-40-x86_64 \
  --chroot epel-9-x86_64 \
  --description "NC (Notation-as-Code) - The AI Programming Language" \
  --instructions "sudo dnf copr enable sainarender2222/nc-lang && sudo dnf install nc-lang"

# Upload SRPM to build
copr-cli build nc-lang ~/rpmbuild/SRPMS/nc-lang-1.0.0-1.fc39.src.rpm
```

### Step 7: Users install via COPR

```bash
sudo dnf copr enable sainarender2222/nc-lang
sudo dnf install nc-lang
```

### Common Pitfalls

- The `%autosetup` macro expects the extracted directory name to match `notation-as-code-%{version}`. Verify the tarball structure.
- `%make_build` uses system-optimal make flags. Do not hardcode `-j` values.
- COPR builds happen on Fedora infrastructure. Ensure all `BuildRequires` are available in Fedora repos.
- The `nc` binary name may conflict with `nmap-ncat`. If so, use `Conflicts: nmap-ncat` or rename to `nc-lang`.

---

## 12. WinGet (Windows Package Manager)

### Prerequisites

- Windows 10 1809+ or Windows 11
- Fork of https://github.com/microsoft/winget-pkgs
- `wingetcreate` tool (optional, for automation)

### Step 1: Create manifest files

WinGet uses a directory structure based on the package ID:

```
manifests/
  s/
    sainarender2222/
      NC/
        1.0.0/
          sainarender2222.NC.installer.yaml
          sainarender2222.NC.locale.en-US.yaml
          sainarender2222.NC.yaml
```

### Step 2: sainarender2222.NC.yaml (version manifest)

```yaml
# sainarender2222.NC.yaml

PackageIdentifier: sainarender2222.NC
PackageVersion: 1.0.0
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
```

### Step 3: sainarender2222.NC.locale.en-US.yaml

```yaml
# sainarender2222.NC.locale.en-US.yaml

PackageIdentifier: sainarender2222.NC
PackageVersion: 1.0.0
PackageLocale: en-US
Publisher: Nuckala Sai Narender
PublisherUrl: https://github.com/sainarender2222
PublisherSupportUrl: https://github.com/devheallabs-ai/nc-lang/issues
Author: Nuckala Sai Narender
PackageName: NC
PackageUrl: https://github.com/devheallabs-ai/nc-lang
License: Apache-2.0
LicenseUrl: https://github.com/devheallabs-ai/nc-lang/blob/main/LICENSE
ShortDescription: NC (Notation-as-Code) - The AI Programming Language
Description: |
  NC is the AI programming language. Write AI-powered APIs, services,
  and automations in plain English notation. Features include a built-in
  HTTP server, multi-provider AI support, and zero-config deployment.
Tags:
  - ai
  - programming-language
  - compiler
  - cli
  - notation-as-code
ManifestType: defaultLocale
ManifestVersion: 1.6.0
```

### Step 4: sainarender2222.NC.installer.yaml

```yaml
# sainarender2222.NC.installer.yaml

PackageIdentifier: sainarender2222.NC
PackageVersion: 1.0.0
MinimumOSVersion: 10.0.17763.0
InstallerType: portable
Commands:
  - nc
Installers:
  - Architecture: x64
    InstallerUrl: https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-windows-amd64.exe
    InstallerSha256: REPLACE_WITH_SHA256_OF_WINDOWS_BINARY
    InstallerType: portable
  - Architecture: arm64
    InstallerUrl: https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-windows-arm64.exe
    InstallerSha256: REPLACE_WITH_SHA256_OF_WINDOWS_ARM64_BINARY
    InstallerType: portable
ManifestType: installer
ManifestVersion: 1.6.0
```

### Step 5: Validate manifests

```powershell
# Install winget-create
winget install wingetcreate

# Validate the manifests
winget validate --manifest manifests/s/sainarender2222/NC/1.0.0/
```

### Step 6: Submit to winget-pkgs

```bash
# Fork https://github.com/microsoft/winget-pkgs
# Clone your fork
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs

# Create branch
git checkout -b add-nc-1.0.0

# Copy manifests
mkdir -p manifests/s/sainarender2222/NC/1.0.0
cp path/to/manifests/* manifests/s/sainarender2222/NC/1.0.0/

# Commit and push
git add .
git commit -m "Add sainarender2222.NC version 1.0.0"
git push origin add-nc-1.0.0

# Create PR to microsoft/winget-pkgs
gh pr create --repo microsoft/winget-pkgs \
  --title "Add sainarender2222.NC version 1.0.0" \
  --body "New package submission for NC (Notation-as-Code) - The AI Programming Language"
```

### Step 7: Using wingetcreate (automated alternative)

```powershell
# One-command submission
wingetcreate submit `
  --id sainarender2222.NC `
  --version 1.0.0 `
  --url "https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-windows-amd64.exe" `
  --token YOUR_GITHUB_PAT
```

### Step 8: Users install

```powershell
winget install sainarender2222.NC
nc version
```

### Testing locally

```powershell
# Test with local manifest
winget install --manifest manifests\s\sainarender2222\NC\1.0.0\

# Verify
nc version
```

### Common Pitfalls

- WinGet manifest validation is strict. Use `winget validate` before submitting.
- The SHA256 hash must match the binary exactly. Recompute after every build.
- The `portable` installer type simply downloads the exe. For a proper installer experience, consider creating an MSIX or NSIS installer.
- WinGet PRs go through automated testing and manual review. Response time varies.

---

## Release Checklist

When publishing a new version, follow this order:

```
1. [ ] Update version numbers in all packaging files
2. [ ] Tag the release in git: git tag -a v{VERSION} -m "Release v{VERSION}"
3. [ ] Push tag: git push origin v{VERSION}
4. [ ] Wait for GitHub Actions to build all binaries
5. [ ] Verify GitHub Release has all artifacts + checksums
6. [ ] Compute SHA256 for each binary
7. [ ] Update and push Docker images
8. [ ] Update Homebrew formula (sha256, url) and push to tap
9. [ ] Build and upload PyPI wheels + sdist
10. [ ] Build and publish npm package
11. [ ] Build and push Chocolatey package
12. [ ] Build and push Snap package
13. [ ] Update AUR PKGBUILD and push
14. [ ] Build .deb and update APT repo
15. [ ] Build .rpm and trigger COPR build
16. [ ] Submit WinGet manifest PR
17. [ ] Update Cargo crate (if applicable)
18. [ ] Announce release
```

### Version Bump Script

```bash
#!/bin/bash
# scripts/bump-version.sh
# Usage: ./scripts/bump-version.sh 1.1.0

set -e
VERSION="$1"

if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>"
    exit 1
fi

echo "Bumping version to ${VERSION}..."

# Python package
sed -i "s/^version = .*/version = \"${VERSION}\"/" packaging/pypi/nc-lang/pyproject.toml
sed -i "s/^__version__ = .*/__version__ = \"${VERSION}\"/" packaging/pypi/nc-lang/nc_lang/__init__.py
sed -i "s/^VERSION = .*/VERSION = \"${VERSION}\"/" packaging/pypi/nc-lang/nc_lang/_binary.py

# npm package
cd packaging/npm/nc-lang && npm version "${VERSION}" --no-git-tag-version && cd -

# Chocolatey
sed -i "s/<version>.*<\/version>/<version>${VERSION}<\/version>/" packaging/chocolatey/nc/nc.nuspec

# Snap
sed -i "s/^version: .*/version: '${VERSION}'/" packaging/snap/snapcraft.yaml

# Homebrew (URL only, sha256 needs manual update)
sed -i "s|/tags/v[0-9.]*\.tar\.gz|/tags/v${VERSION}.tar.gz|" Formula/nc.rb

# AUR
sed -i "s/^pkgver=.*/pkgver=${VERSION}/" packaging/aur/nc/PKGBUILD

# RPM
sed -i "s/^Version:.*/Version:        ${VERSION}/" packaging/rpm/nc-lang.spec

# Cargo
sed -i "s/^version = .*/version = \"${VERSION}\"/" packaging/cargo/nc-lang/Cargo.toml

echo "Version bumped to ${VERSION} across all packaging files."
echo ""
echo "Next steps:"
echo "  1. Compute new SHA256 hashes after GitHub Release is created"
echo "  2. Update sha256 in Homebrew formula, AUR PKGBUILD, Chocolatey, WinGet"
echo "  3. git commit -am 'Bump version to ${VERSION}'"
echo "  4. git tag -a v${VERSION} -m 'Release v${VERSION}'"
echo "  5. git push origin main --tags"
```

