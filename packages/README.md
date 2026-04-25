# NC Language — Distribution Packages

This directory contains the distribution packaging for NC across multiple package managers.

## Directory Structure

```
packages/
  pip/            PyPI package (pip install nc-lang)
  npm/            npm package (npm install -g nc-lang)
  chocolatey/     Chocolatey package for Windows (existing NC library)
  ai-tools/       NC library package
  auth-utils/     NC library package
  data-parsers/   NC library package
  http-helpers/   NC library package
  text-utils/     NC library package
```

Additional distribution configs:

```
formula/
  nc.rb           Homebrew formula (brew install nc-lang)
```

---

## PyPI Package (`pip install nc-lang`)

**Directory:** `packages/pip/`

A thin Python wrapper that downloads and manages the NC binary.

### Build

```bash
cd packages/pip
pip install build twine
make build
```

### Test Locally

```bash
pip install -e packages/pip/
nc version
```

### Publish to Test PyPI

```bash
make publish-test
```

### Publish to PyPI

```bash
make publish
```

### Prerequisites

- Python >= 3.8
- `build` and `twine` packages
- PyPI account with nc-lang project access

---

## npm Package (`npm install -g nc-lang`)

**Directory:** `packages/npm/`

A thin Node.js wrapper that downloads the NC binary during install.

### Build / Pack

```bash
cd packages/npm
npm pack
```

### Test Locally

```bash
npm install -g ./packages/npm/
nc version
```

Or without global install:

```bash
npx ./packages/npm/ version
```

### Publish

```bash
cd packages/npm
npm publish
```

### Prerequisites

- Node.js >= 14
- npm account with nc-lang package access

---

## Homebrew Formula (`brew install nc-lang`)

**File:** `formula/nc.rb`

Builds NC from source using `make`.

### Test Locally

```bash
brew install --build-from-source ./formula/nc.rb
nc version
```

### Publish

Submit to a Homebrew tap repository or the official homebrew-core:

```bash
# Create a tap
brew tap sainarender2222/nc https://github.com/sainarender2222/homebrew-nc
# Copy formula
cp formula/nc.rb $(brew --repo sainarender2222/nc)/Formula/
```

### Prerequisites

- macOS or Linux with Homebrew
- A C compiler (make, cc)

---

## Release Checklist

When releasing a new version of NC:

1. **Update version numbers** in all package configs:
   - `packages/pip/pyproject.toml` — `version`
   - `packages/pip/nc_lang/__init__.py` — `__version__`
   - `packages/pip/nc_lang/installer.py` — `VERSION`
   - `packages/npm/package.json` — `version`
   - `packages/npm/scripts/install.js` — `VERSION`
   - `formula/nc.rb` — `url` tag version

2. **Create GitHub release** with platform binaries:
   - `nc-linux-x86_64`
   - `nc-macos-x86_64`
   - `nc-macos-arm64`
   - `nc-windows-x86_64.exe`

3. **Compute checksums** and update:
   - `packages/pip/nc_lang/installer.py` — `sha256` values
   - `packages/npm/scripts/install.js` — `sha256` values
   - `formula/nc.rb` — `sha256` for source tarball

4. **Publish packages**:
   ```bash
   # PyPI
   cd packages/pip && make publish

   # npm
   cd packages/npm && npm publish

   # Homebrew
   # Update tap repository
   ```

5. **Verify installations**:
   ```bash
   pip install nc-lang && nc version
   npm install -g nc-lang && nc version
   brew install sainarender2222/nc/nc && nc version
   ```
