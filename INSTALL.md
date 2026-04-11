<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="200">
</p>

# Install NC

## One-Line Install

The installers require license acceptance before installation. On supported desktops, this appears as a popup dialog; otherwise it falls back to a terminal prompt. NC also enforces this agreement on first run and stores acceptance for the current user. For CI or unattended installs, set `NC_ACCEPT_LICENSE=1`.

**macOS / Linux:**
```bash
curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | bash

# Non-interactive
curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | NC_ACCEPT_LICENSE=1 bash
```

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.ps1 | iex

# Non-interactive
$env:NC_ACCEPT_LICENSE=1; irm https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.ps1 | iex
```

**Docker:**
```bash
docker pull nc:latest
docker run -it nc:latest version
```

**Verify:**
```bash
nc version
```

You should see `NC v1.0.0`.

---

## Your First NC Program

Create `hello.nc`:

```nc
service "hello"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name + "!"

api:
    POST /greet runs greet
```

Run it:

```bash
nc run hello.nc
```

Start as a server:

```bash
nc serve hello.nc
```

---

## Using NC Daily

```bash
nc run app.nc              # run any .nc file
nc run app.nc -b greet     # run a specific behavior
nc serve app.nc            # start HTTP server
nc serve app.nc -p 3000    # start on custom port
nc validate app.nc         # check syntax
nc test                    # run all tests
nc repl                    # interactive mode (like Python)
nc fmt app.nc              # format code
nc build app.nc -o myapp   # compile to native binary
nc build .                 # build all .nc files in current directory
nc build --all -o build/ -j 4  # parallel batch build
nc init my-service         # scaffold a new project
nc setup my-service        # scaffold + configure + start server
nc doctor                  # check project setup
nc debug app.nc            # step-through debugger
nc debug app.nc --dap      # DAP server for VS Code
nc pkg install <package>   # install packages
nc lsp                     # start language server
nc version                 # check version
```

### Security Configuration

```bash
export NC_HTTP_ALLOWLIST="api.openai.com,api.anthropic.com"  # restrict outbound hosts
export NC_HTTP_STRICT=1               # deny all outbound when no allowlist
export NC_CORS_ORIGIN="https://myapp.com"  # restrict CORS origin
export NC_API_KEY="your-api-key"      # enable API key auth
export NC_JWT_SECRET="your-secret"    # enable JWT auth
```

### Interactive Mode

```
$ nc repl
NC v1.0.0 — type "help" or "quit"
>>> show "hello"
hello
>>> set x to 42
>>> show x * 2
84
>>> quit
```

### Run with AI

NC includes built-in AI commands that work with local or external providers.

```bash
# Check AI status
nc ai status

# Generate NC code from description
nc ai generate "a REST API for user management"

# AI-powered code creation (backend + frontend + tests)
nc ai create "todo app with authentication"

# Interactive AI chat
nc ai chat

# Math evaluation
nc ai math "factorial(10)"

# Chain-of-thought reasoning
nc ai reason "what is the time complexity of merge sort"

# Code review
nc ai review myservice.nc

# Train on your own project
nc ai learn ./my-project/

# Synthetic training data generation
nc ai synth -n 1000 -o training_data.nc
```

#### Repo-local built-in model smoke (Windows)

If you are working from source and want to exercise the built-in local model
path end to end, use the repo-local setup and smoke scripts:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-local-ai-model.ps1
powershell -ExecutionPolicy Bypass -File tests\run_ai_smoke.ps1 -Prompt "inventory dashboard"
```

For a stronger repo-local validation of `nc ai create`, use the v1 release
binary with the semantic smoke harness:

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_ai_semantic_smoke.ps1 `
    -NcBin .\engine\build\nc_release_ready.exe `
    -ModelPath .\engine\build\nc_model_enterprise_ft120.bin `
    -TokenizerPath .\engine\build\nc_model_enterprise_ft120_tokenizer.bin `
    -OutputDir .\engine\build\ai_create_semantic_ops_dashboard_win_ft120 `
    -Force
```

For the shell-path validation, run:

```bash
bash ./tests/run_ai_semantic_smoke.sh \
    --nc-bin engine/build/nc_release_ready.exe \
    --model-path engine/build/nc_model_enterprise_ft120.bin \
    --tokenizer-path engine/build/nc_model_enterprise_ft120_tokenizer.bin \
    --output-dir engine/build/ai_create_semantic_ops_dashboard_unix_ft120 \
    --force \
    --copy-model
```

This checks `service.nc`, `app.ncui`, `test_app.nc`, `README.md`, the built
`dist/app.html` output, and the enterprise feature set requested by the complex
prompt. See [docs/AI_CREATE_VALIDATION.md](docs/AI_CREATE_VALIDATION.md) for the
latest verified Windows and shell-path results.

The setup script stages the local model and tokenizer into `training_data\`
so repo-root commands can use them without keeping binary aliases in the
engine root.

**Using External AI Providers (optional):**

Set your AI provider credentials to use external LLMs alongside NC AI:

```bash
export NC_AI_URL="<YOUR_PROVIDER_ENDPOINT>"
export NC_AI_KEY="<YOUR_API_KEY>"
export NC_AI_MODEL="<YOUR_MODEL_NAME>"
```

Then in NC code:

```nc
ask AI to "write a haiku about programming"
    save as poem
show poem
```

---

## Platform Support

| Platform | Status | Install Method | Binary Size |
|----------|--------|---------------|-------------|
| **macOS (ARM64)** | âœ… Verified | `curl ... \| bash` | 1.3 MB |
| **macOS (x86_64)** | âœ… Supported | `curl ... \| bash` | ~1.4 MB |
| **Windows 11 (ARM64)** | âœ… Verified | `install.bat` or PowerShell | 1.6 MB |
| **Windows 10/11 (x64)** | âœ… Supported | `install.bat` or PowerShell | ~1.5 MB |
| **Linux (Alpine/Docker)** | âœ… Verified | `docker pull nc:v1` | 24.4 MB image |
| **Linux (Ubuntu/Debian)** | âœ… Supported | `curl ... \| bash` | ~1.4 MB |
| **Linux (Fedora/RHEL)** | âœ… Supported | `curl ... \| bash` | ~1.4 MB |

NC compiles from a single C11 codebase with zero external dependencies at runtime.
AI features are available via the `nc ai` command surface and can connect to local or external providers.

---

## VS Code Extension

```bash
code --install-extension nc-lang
```

Or search "NC" in the VS Code marketplace. Includes syntax highlighting, snippets, and hover documentation.

---

## Docker

```bash
# Run a file
docker run -v $(pwd):/app nc:latest run /app/service.nc

# Start a server
docker run -p 8080:8000 -v $(pwd):/app nc:latest serve /app/service.nc

# Interactive development
docker run -it -v $(pwd):/workspace nc:dev
```

Images:
- `nc:latest` — Alpine-based, 22.9MB
- `nc:slim` — Debian-based, 158MB
- `nc:dev` — Full dev environment with git + readline, 278MB

---

## Homebrew (macOS)

```bash
brew install nc
```

---

## Uninstall

```bash
# macOS / Linux
sudo rm /usr/local/bin/nc

# Windows
# Remove %LOCALAPPDATA%\nc\
```

---

<details>
<summary><strong>Advanced: Build from Source</strong></summary>

If you want to build NC yourself (for contributing or custom builds):

### Prerequisites

| OS | Install |
|----|---------|
| macOS | `xcode-select --install` |
| Ubuntu/Debian | `sudo apt install build-essential libcurl4-openssl-dev` |
| Fedora | `sudo dnf install gcc make libcurl-devel` |
| Windows | Install [MSYS2](https://www.msys2.org/), then `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-curl make` |

### Build

```bash
git clone https://github.com/devheallabs-ai/nc-lang.git
cd nc/engine
make
```

### Test

```bash
./build/nc test                       # 85 language tests (947 behaviors)
make test-unit                        # runtime unit tests
python3 tests/validate_nc.py          # 67 external validation tests
python3 tests/validate_nc_deep.py     # 45 deep integration tests (HTTP, build, REPL)
```

### Install

```bash
sudo make install         # copies to /usr/local/bin/nc
```

### CMake (Windows/MSVC)

```bash
mkdir build && cd build
cmake .. -DNC_NO_REPL=ON
cmake --build . --config Release
```

</details>

