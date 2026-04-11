<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="200">
</p>

<h1 align=”center”>NC — Build Apps in Plain English</h1>
<h4 align=”center”>No-Code. AI Built-In. One Binary. Zero Friction.</h4>

<p align=”center”>
  <a href=”https://devheallabs.in”><strong>Built by DevHeal Labs AI</strong></a><br>
  Describe what you want. NC builds it, runs it, and deploys it.<br>
  No frameworks. No syntax to memorize. Just plain English.
</p>

<p align=”center”>
  <a href=”https://devheallabs.in/playground.html”><strong>Try the Playground</strong></a> &bull;
  <a href=”INSTALL.md”>Install</a> &bull;
  <a href=”TUTORIAL.md”>Tutorial</a> &bull;
  <a href=”docs/EXAMPLES.md”>Examples</a> &bull;
  <a href=”docs/NC_USER_MANUAL.md”>Docs</a> &bull;
  <a href=”docs/NC_LANGUAGE_GUIDE.md”>Language Guide</a>
</p>

<p align=”center”>
  <img src=”https://img.shields.io/badge/version-v1.3.0-orange” alt=”v1.3.0”>
  <img src=”https://img.shields.io/badge/license-Apache%202.0-green” alt=”Apache 2.0”>
  <img src=”https://img.shields.io/badge/platforms-Linux%20%7C%20macOS%20%7C%20Windows-blue” alt=”cross-platform”>
  <img src=”https://img.shields.io/badge/tests-113%20files%20passing-brightgreen” alt=”tests passing”>
  <img src=”https://img.shields.io/badge/binary-~600KB-purple” alt=”~600KB binary”>
  <img src=”https://img.shields.io/badge/no--code-plain%20English-00d4ff” alt=”no-code”>
</p>

---

## What Is NC?

NC is a **no-code platform** that turns plain English descriptions into real, running applications — APIs, AI pipelines, dashboards, automations. You describe what you want; NC handles the rest.

```nc
service “classifier”
version “1.0.0”

to classify with ticket:
    ask AI to “classify this support ticket” using ticket
        save as category
    respond with {“category”: category}

api:
    POST /classify runs classify
```

```bash
NC_AI_KEY=sk-xxx nc serve classifier.nc
# Server running on :8000 — that's your entire deployment
```

No web framework. No `requirements.txt`. No boilerplate. One 570KB binary.

| Today's stack | NC |
|---|---|
| Framework + HTTP client + JSON + middleware | One `.nc` file |
| 25 lines of boilerplate for one AI call | `ask AI to “classify” using ticket` |
| Install runtime + package manager + 10 deps | One 570KB binary |
| Separate framework, AI SDK, auth layer | Built into the language |

---

## Install

The installers show a license agreement before installation. On supported desktops, this appears as a popup dialog; otherwise it falls back to a terminal prompt. NC also enforces this agreement on first run and stores acceptance for the current user. For CI or other non-interactive installs, set `NC_ACCEPT_LICENSE=1`.

```bash
# macOS / Linux
curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | bash

# macOS / Linux (non-interactive)
curl -sSL https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.sh | NC_ACCEPT_LICENSE=1 bash

# Windows (PowerShell)
irm https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.ps1 | iex

# Windows (non-interactive)
$env:NC_ACCEPT_LICENSE=1; irm https://raw.githubusercontent.com/DevHealLabs/nc-lang/main/install.ps1 | iex

# Docker
docker run -it nc:latest version

# Verify
nc version
```

## Terminal and Animation Controls (Cross-Platform)

NC now supports consistent terminal behavior across CMD, PowerShell, Git Bash, Linux, and macOS terminals.

- `NC_NO_ANIM=1`: disable animations/spinners/progress redraws (plain text mode)
- `NO_COLOR=1`: disable ANSI colors and render plain text markers

Examples:

```bash
NC_NO_ANIM=1 nc ai status
NO_COLOR=1 nc version
```

```powershell
$env:NC_NO_ANIM="1"; nc ai status
$env:NO_COLOR="1"; nc version
```

---

## Hello World

Create `hello.nc`:

```nc
service "hello"
version "1.0.0"

to greet:
    respond with "Hello from NC!"

to health_check:
    respond with {"status": "healthy"}

api:
    GET /hello runs greet
    GET /health runs health_check
```

Run it:

```bash
nc run hello.nc           # compile + run
nc serve hello.nc         # start as HTTP server
nc validate hello.nc      # syntax check
```

---

## Why NC?

NC replaces the **stack**. One language, one binary, one deployable file.

### The Stack Problem

Every AI API you build today requires:

1. A web framework (Flask / FastAPI / Express)
2. An HTTP client (requests / httpx / fetch)
3. JSON parsing and serialization
4. API key management and environment config
5. Error handling, retries, and timeouts
6. Auth middleware (JWT, API keys)
7. Rate limiting, CORS, logging

NC handles all of this out of the box. Your entire service is one file:

```nc
service "my-api"
version "1.0.0"

configure:
    ai_model is "nova"
    ai_key is "env:NC_AI_KEY"

to process with data:
    ask AI to "extract key insights" using data
        save as insights
    respond with insights

to health_check:
    respond with {"status": "healthy"}

api:
    POST /process runs process
    GET /health runs health_check
```

```bash
nc serve my-api.nc    # production server on :8000 — done
```

### Readable by Non-Engineers

```nc
to handle_order with order:
    if order.total is above 1000:
        ask AI to "flag this for review" using order
            save as review
        notify "finance-team" review
    otherwise:
        store order into "approved_orders"
    respond with {"status": "processed"}
```

A product manager can read this. A QA engineer can review it. Your entire team can understand what the service does.

---

## Quick Start — `nc init`

```bash
nc init my-service
cd my-service
export NC_AI_KEY="sk-your-key"
nc serve service.nc
# Server running on :8000 — done
```

Or clone a production-ready template:

```bash
cp templates/ticket-classifier.nc my-api.nc
NC_AI_KEY=sk-xxx nc serve my-api.nc
```

See all templates in [`templates/`](templates/).

---

## Language Features

```nc
// Variables
set name to "NC"
set items to [1, 2, 3]
set config to {"port": 8080, "debug": yes}

// AI (native — no imports)
ask AI to "translate to Spanish" using text
    save as translated

// Data gathering (HTTP, APIs)
gather users from "https://api.example.com/users"

// Control flow
if score is above 90:
    set grade to "A"
otherwise:
    set grade to "B"

// Pattern matching
match status:
    when "active": log "online"
    when "away": log "idle"
    otherwise: log "offline"

// Loops
repeat for each user in users:
    log user.name

// Error handling
try:
    gather data from "https://unreliable-api.com/data"
on error:
    set data to {"fallback": true}

// Functions (behaviors)
to calculate_score with data:
    set total to sum(data.values)
    set avg to average(data.values)
    respond with {"total": total, "average": avg}

// HTTP server
api:
    POST /score runs calculate_score
    GET /health runs health_check
```

---

## Built-in Functions

NC comes with 80+ functions — no imports needed:

| Category | Functions |
|----------|-----------|
| **Strings** | `upper`, `lower`, `trim`, `split`, `join`, `replace`, `contains`, `starts_with` |
| **Lists** | `len`, `first`, `last`, `append`, `remove`, `sort`, `reverse`, `sum`, `average`, `unique`, `flatten`, `slice` |
| **Math** | `abs`, `sqrt`, `pow`, `min`, `max`, `round`, `ceil`, `floor`, `random` |
| **JSON/Data** | `json_encode`, `json_decode`, `csv_parse`, `yaml_parse`, `xml_parse`, `toml_parse` |
| **Files** | `read_file`, `write_file`, `file_exists`, `read_lines`, `append_file`, `delete_file` |
| **Time** | `time_now`, `time_ms`, `time_format`, `sleep` |
| **Platform/Env** | `env`, `platform`, `arch`, `hostname`, `cpu_count`, `mem_total`, `mem_free`, `os_name` |
| **AI/ML** | `load_model`, `predict`, `memory_new`, `memory_add`, `memory_get`, `memory_clear`, `memory_summary` |
| **Cache** | `cache`, `cached`, `is_cached` |
| **Security** | `hash_sha256`, `hash_md5`, `hmac_sha256`, `jwt_encode` |
| **Session** | `session_new`, `session_get`, `session_set`, `session_delete`, `session_clear` |
| **Validation** | `validate`, `token_count` |
| **Shell/Exec** | `exec`, `shell`, `shell_capture` |
| **System** | `print`, `input`, `type`, `str`, `int`, `float` |

---

## Standard Library

NC ships 27 standard library modules (10 fully implemented, 17 in development):

> Modules marked with **\*** are in active development (stub/partial implementations).

```nc
// Fully implemented
import "math"          // abs, factorial, power, percentage, clamp, etc.
import "statistics"    // mean, median, variance, range, percentile
import "string"        // join, repeat, reverse, pad, truncate
import "random"        // random generation
import "platform"      // os_name, arch, version, info
import "functools"     // map, filter, reduce, pipe, compose
import "queue"         // queue operations
import "pathlib"       // path joining
import "typing"        // type checking
import "threading"     // parallel execution

// In development *
import "json"          // parse, encode *
import "datetime"      // now, format, is_past, is_future *
import "csv"           // parse, encode *
import "collections"   // counter, group_by, zip, merge *
import "logging"       // debug, info, warning, error, critical *
import "unittest"      // assert_equal, assert_true, assert_contains *
import "base64"        // encode, decode *
import "copy"          // deep_copy, shallow_copy *
import "hashlib"       // sha256, md5, hmac *
import "socket"        // connect, send, receive, close *
import "email"         // email notification *
import "subprocess"    // run, capture, pipe *
import "tempfile"      // temp_file, temp_dir, cleanup *
import "secrets"       // token generation *
import "pprint"        // pretty_print, format_table, format_tree *
import "re"            // match, find_all, replace, split *
import "ai"            // classify, summarize, analyze, generate, extract *
```

Write your own modules in `.nc` and share them.

---

## Auto-Correct and Suggestions

NC uses Damerau-Levenshtein distance to detect typos and suggest corrections automatically:

```
$ nc run app.nc
Error: unknown function 'pritn' on line 5
  Is this what you meant → 'print'?

Error: undefined variable 'scroe' on line 8
  Is this what you meant → 'score'?
```

Handles transpositions (`lne` → `len`), extra characters (`sortt` → `sort`), missing characters (`uper` → `upper`), and substitutions (`pritn` → `print`). Integrated into the parser and interpreter — works for keywords, variables, and function names.

---

## Synonym Engine

NC understands keywords from other languages. Write `def` and NC reads it as `to`. Write `return` and NC reads it as `respond with`. 39 cross-language synonym mappings from Python, JavaScript, Go, Rust, Ruby, Java, C, and Swift:

```nc
# All of these work — NC translates automatically
def greet(name):          # → to greet with name:
  return "Hello " + name  # → respond with "Hello " + name

print("done")             # → log "done"
for item in items:        # → repeat for each item in items:
```

Enable mapping notices with `NC_SYNONYM_NOTICES=1` to see what NC translates.

---

## Call Traceback

When errors occur, NC shows the full call chain with behavior names, file, and line numbers:

```
Error: division by zero
  at calculate_average (app.nc:14)
  at process_report (app.nc:28)
  at main (app.nc:3)
```

---

## Universal AI Engine

NC's C runtime has zero knowledge of any AI company, model, or API format. All format knowledge lives in `nc_ai_providers.json`:

- **Template engine** fills `{{placeholders}}` in JSON request templates
- **Path extractor** navigates JSON responses by dot-path (e.g. `choices.0.message.content`)
- **Named adapter presets** — switch AI providers by editing JSON, no recompile
- Every `ask AI` call returns `{ok, response, model, raw}` guaranteed

You bring YOUR API key and YOUR endpoint. NC reads them from YOUR environment variables at runtime.

---

## Commands

```bash
# Core (stable)
nc init my-service         # create a new NC project
nc run app.nc              # run a program
nc run app.nc -b greet     # run a specific behavior
nc serve app.nc            # start HTTP server
nc serve app.nc -p 3000    # start on custom port
nc validate app.nc         # check syntax
nc test                    # run all tests
nc fmt app.nc              # format code
nc repl                    # interactive mode
nc bytecode app.nc         # show compiled bytecodes
nc tokens app.nc           # show token stream
nc -c "show 42"            # execute inline code
nc version                 # show version
nc mascot                  # show the NC mascot

# Built-in AI (generate NC code from English)
nc ai create "a blog app with user auth"       # generate full project (backend + frontend + tests)
nc ai generate "create a REST API for users"   # generate single NC file from description
nc ai generate "build a blog" --tokens 200     # custom max tokens
nc ai generate "todo app" --output todo.nc     # save to file
nc ai status                                   # show model architecture & status
nc ai train                                    # training instructions
nc ai serve                                    # start AI generation API

# Build & Analysis
nc profile app.nc          # profile performance
nc analyze app.nc          # semantic analysis

# Utilities
nc get <url>               # HTTP GET (curl-like)
nc post <url> <body>       # HTTP POST with JSON
nc digest app.py           # convert Python/JS/YAML to NC (offline)
nc migrate app.py          # AI-powered code migration

# Experimental / In Development
nc build app.nc -o myapp   # compile to native binary (partial)
nc build . -o build/ -j 4  # parallel batch build (partial)
nc compile app.nc          # generate LLVM IR (experimental — generates IR text, not runnable)
nc debug app.nc            # step-through debugger (experimental)
nc debug app.nc --dap      # DAP server for IDE integration (experimental)
nc pkg install <name>      # install packages (experimental — untested)
nc pkg list                # list installed packages (experimental)
nc lsp                     # start language server (experimental)
nc conformance             # run conformance suite
```

---

## Cross-Platform Support

NC runs everywhere — one codebase, all platforms:

| Platform | Build | Server | REPL | Build Binary | Parallel `-j` |
|----------|-------|--------|------|-------------|---------------|
| **macOS** (ARM + Intel) | `make` | Yes | Yes (readline) | Yes | Yes (fork) |
| **Linux** (x86 + ARM) | `make` | Yes | Yes (readline) | Yes | Yes (fork) |
| **Windows** (MinGW) | `make` | Yes | Yes (basic) | Yes | Yes (CreateProcess) |
| **Windows** (MSVC) | `cmake` | Yes | Yes (basic) | Yes | Yes (CreateProcess) |
| **Docker** (Alpine/Debian) | Yes | Yes | Yes | Yes | Yes |

### Security Features

| Feature | Details |
|---------|---------|
| **TLS verification** | Explicit `CURLOPT_SSL_VERIFYPEER` on all HTTPS requests |
| **Timing-safe auth** | Constant-time API key comparison prevents timing attacks |
| **SSRF protection** | `NC_HTTP_ALLOWLIST` restricts outbound hosts; `NC_HTTP_STRICT` denies all by default |
| **Path traversal** | Module imports reject `..`, `/`, `\`; file I/O rejects control characters |
| **Command injection** | `shell()` blocks metacharacters unless `NC_ALLOW_EXEC=unsafe` |
| **CORS** | Defaults to deny when auth is enabled (`NC_API_KEY` / `NC_JWT_SECRET`) |
| **Rate limiting** | Thread-safe per-IP rate limiting with mutex protection |
| **Secret redaction** | API keys, tokens, and passwords auto-masked in all log output |
| **Supply chain** | Package extraction uses `--strip-components=1` to prevent path traversal |

---

## Testing

### NC Language Tests

Write tests in NC:

```nc
to test_math:
    if abs(-5) is equal 5:
        log "pass"
    if sqrt(16) is equal 4:
        log "pass"
    respond with "ok"
```

Run them:

```bash
nc test                           # run all 113 test files
nc test tests/my_test.nc          # run specific test
nc test -v                        # verbose output
```

### Extended Validation Suite

Independent external validation — tests NC like a QA team:

```bash
bash tests/run_tests.sh                   # run all NC language tests
nc test                                   # run NC native test suite
nc test -v                                # verbose output
```

### Windows Test Runner

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

### Docker (Linux Validation)

```bash
docker build -f Dockerfile.test -t nc-test .
docker run --rm nc-test           # runs both test suites on Linux
```

---

## VS Code Extension

Install from the marketplace or:

```bash
code --install-extension nc-lang
```

Features: syntax highlighting, snippets, hover docs, run/serve/validate commands.

---

## Docker

```bash
# Run any .nc file
docker run -v $(pwd):/app nc:latest run /app/service.nc

# Start a server
docker run -p 8080:8000 -v $(pwd):/app nc:latest serve /app/service.nc

# Interactive development
docker run -it -v $(pwd):/workspace nc:dev
```

---

## Documentation

- [Install Guide](INSTALL.md) — get NC running in 30 seconds
- [Tutorial](TUTORIAL.md) — learn NC step by step
- [Language Guide](docs/NC_LANGUAGE_GUIDE.md) — complete syntax reference
- [Examples](docs/EXAMPLES.md) — 31 real-world workflows
- [User Manual](docs/NC_USER_MANUAL.md) — everything you need to know
- [FAQ](docs/FAQ.md) — common questions answered
- [Contributing](CONTRIBUTING.md) — help build NC
- [Security](SECURITY.md) — vulnerability reporting
- [Security Certificate](SECURITY_CERTIFICATE.md) — full hardening audit

---

## Language Features at a Glance

| Feature | NC Syntax |
|---|---|
| **AI as native instruction** | `ask AI to "classify" save as result` |
| **HTTP server** | `api: POST /route runs handler` |
| **Behavior composition** | `set result to add(3, 4)` — behaviors call behaviors |
| **Middleware** | `rate_limit`, `cors`, `auth`, `log_requests` |
| **Data types** | `define User as: name is text` |
| **Error handling** | `try: ... on error: ... finally:` |
| **Loops** | `repeat for each item in items:` |
| **Pattern matching** | `match status: when "active": ...` |
| **80+ built-in functions** | `sum`, `average`, `upper`, `sort`, `len`, etc. |
| **Secrets management** | `.env` auto-loaded, keys never in logs |

---

## AI Commands — `nc ai`

NC includes first-class AI commands in the language and CLI. In an open-source
`nc-lang` release, these commands can target external providers or compatible
local runtimes.

```bash
# Create a full project (backend + frontend + tests + docs) from English
nc ai create "a blog app with user auth and dark theme"

# Generate a single NC file from English
nc ai generate "create a REST API for managing users with CRUD operations"

# Check AI model status
nc ai status
```

For repo-local built-in model verification on Windows, stage the local model
into `training_data/` and run the dedicated smoke script:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-local-ai-model.ps1
powershell -ExecutionPolicy Bypass -File tests\run_ai_smoke.ps1 -Prompt "inventory dashboard"
```

For a more demanding end-to-end validation of `nc ai create`, run the current
release binary against a complex prompt:

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

That flow validates the full project contract: `service.nc`, `app.ncui`,
`test_app.nc`, `README.md`, a compiled `dist/app.html` bundle, and semantic
coverage for tenants, roles, analytics, approvals, and alerts. The latest
validated run passed on April 3, 2026 on both Windows PowerShell and the
shell-path harness. This enterprise path is validated with the trained
repo-local model plus explicit template fallback when the LLM draft misses the
required feature set. See
[docs/AI_CREATE_VALIDATION.md](docs/AI_CREATE_VALIDATION.md) for details.

This keeps `engine/nova_model.bin` and `engine/nc_ai_tokenizer.bin` out of the
engine root while preserving repo-root commands like `nc ai status` and
`nc ai create`.

The current codebase includes support for:
- **AI command surface** — `nc ai create`, `generate`, `chat`, `review`, and related flows
- **Provider adapters** — AI-provider-compatible and local-runtime endpoints
- **Optional local acceleration hooks** — optimized CPU paths and macOS acceleration code

The AI command surface works with external providers out of the box. Local model
runtimes (NC AI) are available separately via `nc model pull`.

---

## Roadmap

| Feature | Status |
|---------|--------|
| **AI command surface (`nc ai`)** | Working — external providers + local runtimes |
| **File-backed persistent store** | Done — `file://./data.json`, atomic writes, thread-safe |
| **Extended stdlib** | Done — math, random, string, URL, log, collections, type modules |
| **`nc pkg install`** | Working — shorthand `gh:user/repo`, GitHub fallback |
| **Web playground** | Live — [devheallabs.in/playground.html](https://devheallabs.in/playground.html) |
| **LLVM IR compilation** | Generates IR text; not integrated into runnable pipeline |
| **Async/await** | Infrastructure exists; runtime integration incomplete |
| **`nc build` → native binary** | Partial implementation |

---

## Compatibility Policy

NC follows semantic versioning. For v1.x releases:
- **Patch releases** (1.0.x): Bug fixes only. No breaking changes.
- **Minor releases** (1.x.0): New features, backwards-compatible. Deprecated features emit warnings.
- **Major releases** (x.0.0): May include breaking changes. Migration guides provided.

---

## DevHeal Labs AI

NC was created by **Nuckala Sai Narender**, Founder & CEO of **[DevHeal Labs AI](https://devheallabs.in)**.

DevHeal Labs AI builds open-source developer tools and platforms for AI.

- Creator: **Nuckala Sai Narender**
- Company: [DevHeal Labs AI](https://devheallabs.in)
- Email: [support@devheallabs.in](mailto:support@devheallabs.in)

---

## License

Apache License 2.0 — see [LICENSE](LICENSE).

<details>
<summary><strong>For Contributors: Engine Internals</strong></summary>

NC's runtime engine is written in C11 for speed and portability.

- **Source:** `engine/src/`, `engine/include/`
- **Architecture:** Lexer → Parser → AST → Bytecode Compiler → VM (computed-goto fast dispatch)
- **AI integration surface:** provider adapters and runtime hooks
- **Cross-platform:** `nc_platform.h` (600+ lines) abstracts Linux/macOS/Windows — 34/34 source files compile on all platforms. Validated: macOS ARM64 (570KB), Windows x64 (788KB). Platform abstractions: `nc_setenv`/`nc_unsetenv`/`nc_mkdir_p`/`nc_tempdir`/`nc_secrets_dir`
- **Extensions:** Plugin system via `nc_plugin.h` (dlopen/LoadLibrary FFI)
- **Security:** 22 CWE-class vulnerabilities fixed, timing-safe auth, TLS verification, SSRF protection, integer overflow protection, stack bounds checks, OWASP Top 10 coverage, platform-aware secrets directory
- **Tests:** 90 language tests (.nc, 1192 behaviors) cross-platform + 485 C unit assertions. All 90 .nc tests pass on both macOS and Windows (0.28s macOS, 0.81s Windows)
- **CI:** GitHub Actions — macOS (ARM + Intel), Linux (x86 + ARM), Windows (MinGW + MSVC) — 6 build targets
- **Coroutines:** Windows Fibers, macOS setjmp/longjmp, Linux ucontext (experimental)
- **See:** [Developer Internals](docs/NC_DEVELOPER_INTERNALS.md)

</details>

