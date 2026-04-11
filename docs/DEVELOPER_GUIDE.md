# NC Developer Guide

A comprehensive guide for developers who want to build, extend, and contribute to the NC (Notation-as-Code) programming language.

---

## Table of Contents

1. [Project Structure](#project-structure)
2. [Architecture Overview](#architecture-overview)
3. [Building from Source](#building-from-source)
4. [Running Tests](#running-tests)
5. [How to Add a New Built-in Function](#how-to-add-a-new-built-in-function)
6. [How to Add a New Keyword or Synonym](#how-to-add-a-new-keyword-or-synonym)
7. [Writing NC Programs](#writing-nc-programs)
8. [Creating a Service](#creating-a-service)
9. [Using the Showcase Projects](#using-the-showcase-projects)
10. [Docker and Kubernetes](#docker-and-kubernetes)
11. [Debugging Tips](#debugging-tips)
12. [Contributing](#contributing)

---

## Project Structure

```
nc-lang/
├── engine/                   # C11 source code — the NC compiler and VM
│   ├── src/                  # All .c source files
│   │   ├── main.c            # Entry point, CLI parsing
│   │   ├── nc_lexer.c        # Tokenizer (plain English → tokens)
│   │   ├── nc_parser.c       # Parser (tokens → AST)
│   │   ├── nc_compiler.c     # Bytecode compiler (AST → opcodes)
│   │   ├── nc_vm.c           # Stack-based virtual machine
│   │   ├── nc_value.c        # Value types (text, number, list, record)
│   │   ├── nc_gc.c           # Garbage collector
│   │   ├── nc_json.c         # JSON encoding/decoding
│   │   ├── nc_http.c         # HTTP client (gather data from)
│   │   ├── nc_server.c       # HTTP server (service + api)
│   │   ├── nc_stdlib.c       # Built-in functions (len, str, upper, etc.)
│   │   ├── nc_middleware.c   # Middleware pipeline (auth, CORS, rate limit)
│   │   ├── nc_enterprise.c   # Enterprise features (audit, compliance)
│   │   ├── nc_async.c        # Async/concurrent execution
│   │   ├── nc_module.c       # Import system
│   │   ├── nc_optimizer.c    # Bytecode optimizer
│   │   ├── nc_semantic.c     # Semantic analysis pass
│   │   ├── nc_suggestions.c  # Error message suggestions / autocorrect
│   │   ├── nc_migrate.c      # Python-to-NC migration helper
│   │   ├── nc_repl.c         # Interactive REPL
│   │   ├── nc_debug.c        # Debugger
│   │   ├── nc_lsp.c          # Language Server Protocol
│   │   ├── nc_tensor.c       # Tensor operations (ML)
│   │   ├── nc_autograd.c     # Automatic differentiation
│   │   ├── nc_llvm.c         # LLVM IR generation
│   │   ├── nc_jit.c          # JIT compilation
│   │   ├── nc_embed.c        # Embedding API (C host → NC)
│   │   ├── nc_plugin.c       # Plugin system
│   │   ├── nc_pkg.c          # Package manager
│   │   ├── nc_distributed.c  # Distributed execution
│   │   ├── nc_database.c     # Database operations
│   │   ├── nc_websocket.c    # WebSocket support
│   │   ├── nc_polyglot.c     # Polyglot (call Python/JS from NC)
│   │   └── nc_interp.c       # Tree-walk interpreter (fallback)
│   ├── include/
│   │   └── nc.h              # Main header — all types and declarations
│   ├── tests/
│   │   └── test_nc.c         # C-level unit tests
│   ├── build/                # Build output (nc binary, .o files)
│   ├── Makefile              # Cross-platform Makefile
│   └── CMakeLists.txt        # CMake alternative
│
├── lib/                      # NC standard library (.nc files)
│   ├── math.nc               # Math utilities
│   ├── string.nc             # String utilities
│   ├── json/                 # JSON helpers
│   ├── http/                 # HTTP client/server
│   ├── ai/                   # AI provider abstractions
│   ├── database/             # Database connectors
│   ├── datetime/             # Date and time
│   ├── os/                   # OS interaction
│   ├── collections/          # Data structures
│   ├── asyncio/              # Async primitives
│   ├── logging/              # Logging
│   ├── re/                   # Regular expressions
│   ├── migrate/              # Python migration helpers
│   └── ...                   # ~40+ modules
│
├── tests/                    # Test suite (~113 tests, all passing)
│   ├── lang/                 # Language feature tests (*.nc files)
│   ├── security/             # Security and fuzzing tests
│   ├── run_tests.sh          # Bash test runner
│   ├── run_tests.ps1         # PowerShell test runner
│   └── README.md             # Test documentation
│
├── examples/                 # Example programs
│   ├── 01_hello_world.nc     # Hello world service
│   ├── 02_ai_classifier.nc   # AI ticket classifier
│   ├── real_world/           # Production-style examples
│   │   ├── 01_chat_assistant.nc
│   │   ├── 04_ai_agent.nc
│   │   └── ...
│   └── ...
│
├── showcase/                 # 4 complete showcase applications
│   ├── code-review-api/      # AI code review service
│   ├── task-manager/         # Task management with AI prioritization
│   ├── weather-bot/          # AI weather assistant
│   └── smart-notes/          # AI-tagged notes app
│
├── docs/                     # Documentation
│   ├── NC_LANGUAGE_GUIDE.md  # Full language tutorial
│   ├── LANGUAGE_SPEC.md      # Formal specification
│   ├── DEVELOPER_GUIDE.md    # This file
│   ├── QUICK_REFERENCE.md    # Cheat sheet
│   └── ...
│
├── deploy/                   # Deployment configurations
│   ├── helm/nc/              # Helm chart for Kubernetes
│   ├── monitoring/           # Prometheus + Grafana configs
│   └── terraform/            # AWS Terraform modules
│
├── editor/                   # Editor plugins
├── bindings/                 # Language bindings
├── website/                  # NC website
├── templates/                # Project templates
├── packages/                 # Package registry
├── grammar/                  # TextMate grammar
├── formula/                  # Homebrew formula
│
├── Dockerfile                # Production image (Alpine, ~20MB)
├── Dockerfile.dev            # Development image with tools
├── Dockerfile.slim           # Debian slim variant
├── Dockerfile.test           # Test runner image
├── docker-compose.yml        # Docker Compose (with monitoring)
│
├── start.sh                  # Project starter (macOS/Linux)
├── start.bat                 # Project starter (Windows)
├── install.sh                # One-line installer (macOS/Linux)
├── install.bat               # One-line installer (Windows CMD)
├── install.ps1               # One-line installer (PowerShell)
│
├── README.md
├── LICENSE                   # Apache 2.0
├── CONTRIBUTING.md
├── CHANGELOG.md
├── ROADMAP.md
├── SECURITY.md
└── TUTORIAL.md
```

---

## Architecture Overview

NC programs follow this compilation pipeline:

```
Source Code (.nc)
       │
       ▼
┌─────────────┐
│   Lexer      │  nc_lexer.c — Tokenizes plain English into tokens
│              │  Handles keywords, synonyms, string interpolation
└──────┬──────┘
       │  Token stream
       ▼
┌─────────────┐
│   Parser     │  nc_parser.c — Builds Abstract Syntax Tree
│              │  Parses services, behaviors, API routes, types
└──────┬──────┘
       │  AST
       ▼
┌─────────────┐
│  Semantic    │  nc_semantic.c — Validates types, scopes, references
│  Analysis    │  Catches errors before compilation
└──────┬──────┘
       │  Validated AST
       ▼
┌─────────────┐
│  Compiler    │  nc_compiler.c — Emits bytecode opcodes
│              │  Each behavior compiles to a separate chunk
└──────┬──────┘
       │  Bytecode
       ▼
┌─────────────┐
│   VM         │  nc_vm.c — Stack-based virtual machine
│              │  30+ opcodes, GC-managed heap
│              │  Handles HTTP serving, AI calls, etc.
└─────────────┘
```

**Key design decisions:**

- **Stack-based VM** with 30+ opcodes (similar to CPython/Lua).
- **Garbage collector** (`nc_gc.c`) manages all heap-allocated values.
- **Single-header design**: `nc.h` declares all types and function prototypes.
- **Zero external dependencies** for the core binary (curl is the only runtime dep for HTTP).
- **Natural language keywords** with synonym support (e.g., `otherwise` = `else`).

---

## Building from Source

### Prerequisites

| Platform | Requirements |
|----------|-------------|
| **macOS** | Xcode Command Line Tools: `xcode-select --install` |
| **Ubuntu/Debian** | `sudo apt install gcc make libcurl4-openssl-dev` |
| **Fedora/RHEL** | `sudo dnf install gcc make libcurl-devel` |
| **Arch Linux** | `sudo pacman -S gcc make curl` |
| **Windows** | [MSYS2](https://www.msys2.org/) with `pacman -S mingw-w64-x86_64-gcc` |

### macOS / Linux

```bash
# Clone and build
cd nc-lang/engine
make

# The binary is at engine/build/nc
./build/nc version
```

Or use the starter script:

```bash
./start.sh build
```

### Windows (MSYS2/MinGW)

Using the Makefile (from MSYS2 terminal):

```bash
cd engine
make
```

Using the starter script (from CMD):

```cmd
start.bat build
```

The Windows build uses direct GCC compilation without make:

```cmd
gcc -o build\nc.exe -Iinclude -O2 -DNDEBUG -DNC_NO_REPL -w ^
    src\main.c src\nc_lexer.c src\nc_parser.c src\nc_compiler.c ^
    src\nc_vm.c src\nc_value.c src\nc_gc.c src\nc_json.c ^
    src\nc_http.c src\nc_server.c src\nc_stdlib.c src\nc_middleware.c ^
    src\nc_enterprise.c src\nc_async.c src\nc_module.c ^
    src\nc_optimizer.c src\nc_semantic.c src\nc_suggestions.c ^
    src\nc_migrate.c src\nc_repl.c src\nc_debug.c src\nc_lsp.c ^
    src\nc_tensor.c src\nc_autograd.c src\nc_llvm.c src\nc_jit.c ^
    src\nc_embed.c src\nc_plugin.c src\nc_pkg.c src\nc_distributed.c ^
    src\nc_database.c src\nc_websocket.c src\nc_polyglot.c ^
    src\nc_interp.c -lws2_32 -lwinhttp
```

### Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| Default | `make` | Optimized build |
| Debug | `make debug` | With debug symbols, no optimization |
| Static | `make static` | Fully static (for Alpine/Docker) |
| Clean | `make clean` | Remove build artifacts |
| Install | `make install` | Copy to `/usr/local/bin/nc` |

---

## Running Tests

### All Tests

```bash
# Using starter script
./start.sh test

# Or directly
cd engine && make test
```

### Language Tests Only

The `tests/lang/` directory contains ~90 `.nc` test files. Each test is a self-contained NC program that validates a language feature.

```bash
# Run a single test
./engine/build/nc run tests/lang/test_strings.nc

# Run all language tests
for f in tests/lang/*.nc; do
    echo "Testing: $f"
    ./engine/build/nc run "$f"
done
```

### C Unit Tests

```bash
cd engine
make test-unit
```

### Security Tests

```bash
cd engine
make test-vulnerabilities
```

### Windows

```cmd
start.bat test
```

Or via PowerShell:

```powershell
.\tests\run_tests.ps1
```

---

## How to Add a New Built-in Function

Built-in functions live in `engine/src/nc_stdlib.c`. Here is the process:

### Step 1: Declare the function

In `engine/include/nc.h`, find the built-in function declarations section and add your prototype:

```c
NcValue nc_builtin_my_function(NcVM *vm, int argc, NcValue *args);
```

### Step 2: Implement the function

In `engine/src/nc_stdlib.c`, add the implementation:

```c
NcValue nc_builtin_my_function(NcVM *vm, int argc, NcValue *args) {
    // Validate argument count
    if (argc < 1) {
        return nc_runtime_error(vm, "my_function() requires at least 1 argument");
    }

    // Get the argument
    NcValue arg = args[0];

    // Do your work...
    NcValue result = nc_value_number(42);

    return result;
}
```

### Step 3: Register the function

In `nc_stdlib.c`, find the function registration table (the `nc_register_stdlib` function or similar) and add an entry:

```c
nc_define_builtin(vm, "my_function", nc_builtin_my_function);
```

### Step 4: Write a test

Create `tests/lang/test_my_function.nc`:

```
service "test-my-function"
version "1.0.0"

to test_basic:
    set result to my_function("input")
    if result is not equal to 42:
        log "FAIL: expected 42, got " + str(result)
        fail "test failed"
    log "PASS: my_function works"

api:
    GET /test runs test_basic
```

### Step 5: Build and test

```bash
cd engine && make && ./build/nc run ../tests/lang/test_my_function.nc
```

---

## How to Add a New Keyword or Synonym

NC supports plain-English synonyms for many keywords. The lexer handles keyword recognition.

### Step 1: Add the token type (if needed)

In `engine/include/nc.h`, find the `NcTokenType` enum and add a new token if needed:

```c
TOKEN_MY_KEYWORD,
```

### Step 2: Register the keyword in the lexer

In `engine/src/nc_lexer.c`, find the keyword table and add your entry:

```c
{"my_keyword", TOKEN_MY_KEYWORD},
```

To add a synonym for an existing keyword:

```c
{"otherwise", TOKEN_ELSE},   // existing
{"else",      TOKEN_ELSE},   // synonym
```

### Step 3: Handle it in the parser

In `engine/src/nc_parser.c`, add handling for your keyword in the appropriate parsing function.

### Step 4: Handle it in the compiler

In `engine/src/nc_compiler.c`, add code generation for the new AST node.

### Step 5: Test

Write a test in `tests/lang/` that exercises the new keyword.

---

## Writing NC Programs

### Quick Syntax Reference

```
// This is a comment

service "my-app"
version "1.0.0"

// Variables
set name to "Alice"
set count to 0
set items to [1, 2, 3]

// Functions (behaviors)
to greet with name:
    purpose: "Say hello"
    respond with "Hello, " + name + "!"

// Control flow
if score is above 90:
    log "excellent"
otherwise:
    log "keep going"

// Loops
repeat for each item in items:
    log item

repeat 5 times:
    log "hello"

while count is below 10:
    set count to count + 1

// AI calls
ask AI to "classify this ticket" using ticket_text save as classification

// Data gathering
gather data from "https://api.example.com/data"

// Error handling
try:
    gather data from unreliable_source
on error:
    log "failed to fetch"

// API routes
api:
    GET /greet runs greet
    POST /process runs process_data
```

### Running Programs

```bash
# Run a script
nc run myfile.nc

# Start a service (HTTP server)
nc serve myfile.nc

# Validate syntax
nc validate myfile.nc

# Interactive REPL
nc repl
```

---

## Creating a Service

NC services are HTTP APIs defined in plain English.

### Minimal Service

```
service "my-api"
version "1.0.0"

to health:
    respond with "ok"

to greet with name:
    respond with "Hello, " + name

api:
    GET /health runs health
    GET /greet runs greet
```

### With AI Integration

```
service "ai-classifier"
version "1.0.0"
model "nova"

to classify with body:
    set text to body["text"]
    ask AI to "classify this text as positive, negative, or neutral" using text save as result
    respond with {"classification": result, "input": text}

api:
    POST /classify runs classify
```

### With Middleware

```
service "secure-api"
version "1.0.0"

configure:
    port: 8080
    cors_origin: "*"
    rate_limit: 100

middleware:
    use cors
    use rate_limit
    use auth jwt

to protected_endpoint with body:
    respond with "you are authenticated"

api:
    POST /data runs protected_endpoint
```

### Deploying

```bash
# Local
nc serve my_service.nc

# Docker
docker build -t my-service .
docker run -p 8080:8080 my-service serve /app/my_service.nc

# Kubernetes (Helm)
helm install my-service deploy/helm/nc/ --set service.file=my_service.nc
```

---

## Using the Showcase Projects

The `showcase/` directory contains four complete applications.

### Code Review API

AI-powered code review service.

```bash
nc serve showcase/code-review-api/code_review.nc

# Test it
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"code": "def f(x): return x+1", "language": "python", "focus": "all"}'
```

### Task Manager

Task management with AI prioritization.

```bash
nc serve showcase/task-manager/task_manager.nc

# Create a task
curl -X POST http://localhost:8080/tasks \
  -H "Content-Type: application/json" \
  -d '{"title": "Build login page", "priority": "high"}'
```

### Weather Bot

AI weather assistant with live data.

```bash
export WEATHER_API_KEY=your_key_here
nc serve showcase/weather-bot/weather_bot.nc

# Get weather
curl http://localhost:8080/weather/Tokyo
```

### Smart Notes

AI-tagged, semantically searchable notes.

```bash
nc serve showcase/smart-notes/smart_notes.nc

# Create a note
curl -X POST http://localhost:8080/notes \
  -H "Content-Type: application/json" \
  -d '{"title": "Meeting Notes", "content": "Discussed Q2 roadmap and priorities."}'
```

---

## Docker and Kubernetes

### Docker Images

| Image | Base | Size | Use Case |
|-------|------|------|----------|
| `Dockerfile` | Alpine 3.21 | ~20MB | Production |
| `Dockerfile.slim` | Debian slim | ~80MB | When Alpine causes issues |
| `Dockerfile.dev` | Full toolchain | ~200MB | Development |
| `Dockerfile.test` | Alpine + tests | ~25MB | CI/CD test runner |

### Building Images

```bash
# Production
docker build -t nc:latest .

# Development
docker build -f Dockerfile.dev -t nc:dev .

# Multi-platform
docker buildx build --platform linux/amd64,linux/arm64 -t nc:latest .
```

### Docker Compose

```bash
# Start NC service
docker compose up

# With Prometheus + Grafana monitoring
docker compose --profile monitoring up
```

### Kubernetes with Helm

```bash
# Install
helm install my-nc-service deploy/helm/nc/ \
  --set image.tag=latest \
  --set replicaCount=3 \
  --set service.port=8080

# Upgrade
helm upgrade my-nc-service deploy/helm/nc/ --set image.tag=v1.1

# Uninstall
helm uninstall my-nc-service
```

The Helm chart includes:
- Deployment with configurable replicas
- Service and Ingress
- Horizontal Pod Autoscaler (HPA)
- Pod Disruption Budget (PDB)
- Network Policy
- ServiceMonitor (Prometheus)
- ServiceAccount

### Terraform (AWS ECS)

```bash
cd deploy/terraform/aws
cp terraform.tfvars.example terraform.tfvars
# Edit terraform.tfvars with your settings
terraform init && terraform apply
```

---

## Debugging Tips

### Validate First

```bash
nc validate myfile.nc
```

This runs the lexer, parser, and semantic analysis without executing — catching syntax errors and type issues early.

### Enable Debug Logging

```bash
NC_LOG_LEVEL=debug nc serve myfile.nc
```

### Common Issues

**"undefined behavior" error**: You called a behavior that does not exist. Check spelling — NC is case-sensitive for behavior names.

**"expected indent" error**: NC uses indentation (4 spaces recommended). Make sure your blocks are properly indented. Do not mix tabs and spaces.

**API route not matching**: Routes are matched in declaration order. Make sure more specific routes come before generic ones.

**AI calls returning nothing**: Ensure `NC_AI_KEY` and `NC_AI_URL` environment variables are set. You can also set `model` in the service header.

**Import not found**: NC looks for imports in the `lib/` directory. Set `NC_LIB_PATH` if your library is elsewhere.

### Useful CLI Commands

```bash
nc version          # Show version
nc run file.nc      # Run a script
nc serve file.nc    # Start HTTP server
nc validate file.nc # Check syntax
nc test             # Run built-in tests
nc repl             # Interactive REPL
nc migrate file.py  # Migrate Python to NC
nc lsp              # Start Language Server
```

---

## Contributing

### Workflow

1. Fork the repository.
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes.
4. Write tests in `tests/lang/`.
5. Run the full test suite: `./start.sh test`
6. Commit with a descriptive message.
7. Open a pull request.

### Code Style (C)

- C11 standard.
- 4-space indentation.
- `snake_case` for functions and variables.
- Prefix all public symbols with `nc_`.
- Keep functions under ~100 lines where practical.
- Every public function needs a prototype in `nc.h`.

### Code Style (NC)

- 4-space indentation.
- `snake_case` for behavior names.
- Always include `service` and `version` headers.
- Add `purpose:` annotations to behaviors.
- Use descriptive variable names (this is plain English, after all).

### What to Contribute

- New standard library modules (`lib/`)
- Test cases (`tests/lang/`)
- Example programs (`examples/`)
- Editor plugins (`editor/`)
- Documentation improvements (`docs/`)
- Bug fixes in the engine (`engine/src/`)
- Performance optimizations

See [CONTRIBUTING.md](../CONTRIBUTING.md) for the full contribution guidelines.

---

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)**
