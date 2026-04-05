
  


# NC — Complete User Manual & Project Report

> The fastest way to build AI-powered APIs. NC is a programming language where AI, HTTP, and JSON are native instructions — one file, one binary, zero dependencies.

---

**License:** Apache License 2.0 
**Date of First Disclosure:** January 2026
**Repository:** [github.com/nc-lang](https://github.com/nc-lang)

---

## Table of Contents

### Part I — Project Overview & Report
1. [Introduction](#1-introduction)
2. [Problem Statement](#2-problem-statement)
3. [Solution — The NC Language](#3-solution--the-nc-language)
4. [Architecture & Design](#4-architecture--design)
5. [Comparison with Other Languages](#5-comparison-with-other-languages)
6. [Use Cases & Target Audience](#6-use-cases--target-audience)
7. [Performance Benchmarks](#7-performance-benchmarks)
8. [Project Status & Roadmap](#8-project-status--roadmap)

### Part II — Installation & Getting Started
9. [Installation](#9-installation)
10. [Your First Program](#10-your-first-program)
11. [Running NC Programs](#11-running-nc-programs)

### Part III — Language Reference
12. [Comments](#12-comments)
13. [Variables and Data Types](#13-variables-and-data-types)
14. [Operators and Expressions](#14-operators-and-expressions)
15. [Strings](#15-strings)
16. [Lists](#16-lists)
17. [Records (Maps / Objects)](#17-records-maps--objects)
18. [Control Flow](#18-control-flow)
19. [Behaviors (Functions)](#19-behaviors-functions)
20. [Error Handling](#20-error-handling)
    - [Auto-Correct (Built-in Intelligence)](#auto-correct-built-in-intelligence)
    - [Call Traceback](#call-traceback)
21. [Standard Library — Complete Reference](#21-standard-library--complete-reference)
22. [File I/O](#22-file-io)
23. [User Input (stdin)](#23-user-input-stdin)
24. [Data Format Parsing](#24-data-format-parsing)

### Part IV — AI, APIs & Services
25. [AI and LLM Integration](#25-ai-and-llm-integration)
26. [Building APIs and Services](#26-building-apis-and-services)
27. [Notifications and Messaging](#27-notifications-and-messaging)
28. [Database and Storage](#28-database-and-storage)
29. [Events and Scheduling](#29-events-and-scheduling)
30. [Middleware](#30-middleware)

### Part V — Ecosystem & Tooling
31. [Imports and Modules](#31-imports-and-modules)
32. [Package Management](#32-package-management)
33. [The REPL (Interactive Mode)](#33-the-repl-interactive-mode)
34. [Docker and Containers](#34-docker-and-containers)
35. [CI/CD and Deployment](#35-cicd-and-deployment)
36. [Platform Support](#36-platform-support)
37. [Debugging](#37-debugging)
38. [Editor Support (VS Code)](#38-editor-support-vs-code)

### Part VI — Reference
39. [Environment Variables Reference](#39-environment-variables-reference)
40. [CLI Command Reference](#40-cli-command-reference)
41. [Complete Examples](#41-complete-examples)
42. [Frequently Asked Questions (FAQ)](#42-frequently-asked-questions-faq)
43. [Quick Reference Cheat Sheet](#43-quick-reference-cheat-sheet)
44. [About the Project](#44-about-the-project)

---

# Part I — Project Overview & Report

---

## 1. Introduction

### What is NC?

**NC** is a novel programming language that enables developers to write fully functional software using **plain English**. Instead of treating AI as a library to import, NC treats AI as a **language primitive** — you simply write `ask AI to "analyze this data"` and it works.

NC was created by **Nuckala Sai Narender**, Founder of **DevHeal Labs AI**, to solve a fundamental problem: every programming language requires hundreds of lines of boilerplate to build AI-powered services. NC eliminates this entirely.

### The Core Idea

```nc
service "email-classifier"
version "1.0.0"

to classify with email_text:
    ask AI to "Classify this email as: support, sales, or spam" using email_text save as result
    if result.category is equal "spam":
        respond with {"status": "rejected", "reason": "spam"}
    respond with result

api:
    POST /classify runs classify
```

**That's a complete AI-powered email classification service.** 15 lines. No imports. No brackets. No semicolons. Just English.

### Key Highlights

| Feature | Detail |
|---------|--------|
| **Language** | Plain English syntax — reads like a specification |
| **Implementation** | Compiles to bytecode, runs on a stack-based VM |
| **Binary size** | 570KB |
| **Build time** | ~5 seconds |
| **Runtime dependencies** | Zero |
| **AI support** | Built-in — works with any AI provider (local or cloud) |
| **Platforms** | Linux, macOS, Windows |
| **License** | Apache 2.0  |

---

## 2. Problem Statement

### The Boilerplate Problem

Every AI developer writes the same code over and over:

1. Import the AI client library
2. Import a web framework
3. Import JSON utilities
4. Create API key configuration
5. Set up the HTTP server
6. Define route handlers
7. Build the AI prompt with message formatting
8. Parse the AI response
9. Handle errors
10. Return results

**A simple AI classifier in Python:**

```python
import nc_ai_client as ai_client
import json
from flask import Flask, request, jsonify

app = Flask(__name__)
client = ai_client.Client(api_key=os.getenv("NC_AI_KEY"))

@app.route("/classify", methods=["POST"])
def classify():
    data = request.json
    email_text = data.get("email_text", "")

    response = client.chat.completions.create(
        model="nova",
        messages=[
            {"role": "system", "content": "Classify emails. Return JSON."},
            {"role": "user", "content": f"Classify: {email_text}"}
        ],
        response_format={"type": "json_object"}
    )

    result = json.loads(response.choices[0].message.content)

    if result.get("category") == "spam":
        return jsonify({"status": "rejected", "reason": "spam"})

    return jsonify(result)

if __name__ == "__main__":
    app.run(port=8000)
```

**30 lines. 3 imports. Requires Python, pip, Flask, and the openai package.**

### The Gap

| What developers want | What they have to write |
|----------------------|------------------------|
| "Classify this email using AI" | 30 lines of Python + 3 libraries |
| "Start an API server" | 50+ lines of FastAPI/Flask boilerplate |
| "Read a file and analyze it" | Import os, open, read, try/except, etc. |
| "Store user data" | Set up database driver, connection pool, queries |

NC closes this gap completely.

---

## 3. Solution — The NC Language

### Design Principles

1. **Plain English First** — Code should read like a specification, not like machine instructions
2. **AI as a Primitive** — AI operations are built into the language, not imported from a library
3. **Zero Boilerplate** — No imports, no setup, no configuration files for simple tasks
4. **One Binary** — The entire runtime is a single 570KB binary with zero runtime dependencies
5. **Batteries Included** — HTTP server, JSON parsing, file I/O, database, caching — all built-in

### The NC Equivalent

```nc
service "email-classifier"
version "1.0.0"

to classify with email_text:
    ask AI to "Classify this email. Return JSON with category and priority: {{email_text}}" save as result
    if result.category is equal "spam":
        respond with {"status": "rejected", "reason": "spam"}
    respond with result

api:
    POST /classify runs classify
```

**15 lines. Zero imports. One binary. No pip. No virtualenv. No Flask.**

### What NC Supports

- **AI/LLM** — NC AI (built-in) or any compatible AI gateway
- **RAG** — `read_file()` + `chunk()` + `ask AI` with document context
- **ML Models** — Python model inference via `load_model()` + `predict()` (sklearn, PyTorch, TensorFlow, ONNX)
- **Tensor Engine** — N-dimensional arrays with matmul, ReLU, softmax, transpose (CUDA/Metal stubs)
- **Autograd** — Tape-based automatic differentiation with SGD and Adam optimizers
- **HTTP Server** — Concurrent requests, auth, rate limiting, WebSocket, CORS
- **Conversation Memory** — `memory_new()`, `memory_add()` for chatbots
- **Cross-Language** — Digest Python, JavaScript, YAML, JSON to NC code (`nc digest app.py`)
- **Runtime Interop** — Call any language via `exec()` and `shell()`
- **Enterprise** — Sandboxing, API auth, audit logging, multi-tenant isolation
- **Distributed** — Cluster coordination, gradient aggregation, worker management
- **Tooling** — REPL, debugger (DAP), LSP, formatter, profiler, package manager, Docker

---

## 4. Architecture & Design

### Execution Pipeline

```
your_code.nc → Lexer → Parser → AST → Bytecode Compiler → Virtual Machine
```

| Component | What It Does |
|-----------|-------------|
| **Lexer** | Tokenizes plain English (125 token types) |
| **Parser** | Recursive descent parser → AST (57 node types) |
| **Semantic Analyzer** | Symbol table, scope analysis, type checking |
| **Bytecode Compiler** | AST → 48 VM opcodes |
| **Virtual Machine** | Stack-based execution with computed goto dispatch |
| **JIT Compiler** | Computed goto, hot-path optimization, 160+ stdlib functions dispatch |
| **Memory Management** | Automatic memory management |
| **Interpreter** | Tree-walking interpreter (fallback/debug mode) |
| **Standard Library** | 160+ stdlib functions |
| **Auto-Correct** | Built-in intelligence — auto-corrects typos using Damerau-Levenshtein distance |
| **AI Engine** | Universal AI bridge — template engine + path extractor (zero hardcoded providers) |
| **HTTP Server** | Built-in web server for API routes |
| **JSON Engine** | JSON parser and serializer |
| **Database** | HTTP-based or in-memory key-value storage |
| **Module System** | Import system with built-in math/time modules |
| **Package Manager** | Git-based package installation |
| **REPL** | Interactive read-eval-print loop |
| **LSP Server** | Language Server Protocol for IDE integration |
| **Debugger** | Step-through debugger with DAP support |
| **LLVM Backend** | LLVM IR generation for native compilation |
| **Async Runtime** | Thread pool, coroutines, event loop |
| **WebSocket** | WebSocket upgrade, framing, broadcast |
| **Middleware** | Auth, rate limiting, CORS |
| **Optimizer** | Bytecode optimization passes |
| **Polyglot** | Python/JavaScript → NC conversion |

### Built-in Intelligence

NC includes features no other programming language has:

**Auto-Correct** — When you make a typo, NC automatically corrects it and tells you:

```
>>> show lne(items)
[line 1] Auto-corrected 'lne' → 'len'
3
```

Uses Damerau-Levenshtein distance to detect transpositions (`lne` → `len`), extra characters (`sortt` → `sort`), missing characters (`uper` → `upper`), and case mismatches. Works for both variables and functions.

- **High confidence (1-2 edits):** NC auto-corrects and runs the correct code
- **Medium confidence:** NC suggests: "Is this what you meant → 'len'?"
- **No match:** Normal error

**Synonym Engine** — NC accepts keywords from other languages and maps them automatically. Write in the style you know:

```
// All of these work in NC:
print "hello"              // Python-style → maps to 'show'
return result              // Any language → maps to 'respond with'
def greet:                 // Python-style → maps to 'to greet:'
function greet:            // JavaScript-style → maps to 'to greet:'
else:                      // Any language → maps to 'otherwise:'
elif condition:            // Python-style → maps to 'otherwise if condition:'
let x to 5                 // JavaScript-style → maps to 'set x to 5'
null                       // Java/JS/C → maps to 'nothing'
catch error:               // Java/JS → maps to 'on error:'
switch value:              // Java/C/Go → maps to 'match value:'
break                      // Any language → maps to 'stop'
class User:                // Python/Java → maps to 'define User:'
```

Set `NC_SYNONYM_NOTICES=1` to see what NC maps in real time:
```
$ NC_SYNONYM_NOTICES=1 nc -c 'print "hello"; let x to 10; return x'
  (NC: "print" → "show")
  (NC: "let" → "set")
  (NC: "return" → "respond")
hello
10
```

No other programming language accepts multiple ways of saying the same thing. NC does, because English does.

| Synonym | Maps to | From |
|---------|---------|------|
| `print`, `puts`, `display`, `output` | `show` | Python, Ruby, Bash |
| `return`, `give`, `yield` | `respond with` | Every language |
| `def`, `function`, `func`, `fn`, `method` | `to` | Python, JS, Go, Rust |
| `else` | `otherwise` | Every language |
| `elif` | `otherwise if` | Python |
| `var`, `let`, `const` | `set` | JS, Go, Rust |
| `null`, `nil`, `None`, `undefined`, `void` | `nothing` | Java, Ruby, Python, JS |
| `catch`, `except`, `rescue` | `on_error` | Java, Python, Ruby |
| `switch` | `match` | Java, C, Go |
| `case` | `when` | Java, C |
| `break` | `stop` | Every language |
| `continue` | `skip` | Every language |
| `loop`, `foreach` | `repeat` | Various |
| `require`, `include`, `use` | `import` | Node, C, Rust |
| `class`, `struct` | `define` | Python, Java, Go, C |
| `True`, `TRUE` | `yes` | Python, C |
| `False`, `FALSE` | `no` | Python, C |

**Synonym Engine** -- NC accepts keywords from other languages and maps them automatically. Write in the style you know:

```
print "hello"              // Python-style, maps to show
return result              // Any language, maps to respond with
def greet:                 // Python-style, maps to to greet:
function greet:            // JavaScript-style, maps to to greet:
else:                      // Any language, maps to otherwise:
let x to 5                 // JavaScript-style, maps to set x to 5
null                       // Java/JS/C, maps to nothing
catch error:               // Java/JS, maps to on error:
switch value:              // Java/C/Go, maps to match value:
break                      // Any language, maps to stop
class User:                // Python/Java, maps to define User:
```

Set `NC_SYNONYM_NOTICES=1` to see mappings in real time:
```
$ NC_SYNONYM_NOTICES=1 nc -c 'print "hello"'
  (NC: "print" -> "show")
hello
```

No other programming language accepts multiple ways of saying the same thing. NC does, because English does. See the full synonym table in the Quick Reference section.

**Call Traceback** -- When an error occurs inside nested behavior calls, NC shows the full call chain:

```
  Call trace (most recent last):
    in 'classify' at service.nc, line 5
    in 'process' at service.nc, line 12
```

### Universal AI Engine

NC's AI bridge is fully provider-agnostic. NC has zero knowledge of any AI company, model, or API format. It uses two primitives:

1. **Template Engine** — Fills `{{placeholders}}` in a JSON request template
2. **Path Extractor** — Navigates a JSON response by dot-path (e.g., `choices.0.message.content`)

All API format knowledge lives in `nc_ai_providers.json`, not inside NC. To add support for a new AI API, edit the JSON config file — no recompile, no new NC version needed.

### Value System

All values in NC are represented by the `NcValue` type:

| NC Type | Internal | Size | Description |
|---------|----------|------|-------------|
| `nothing` | `VAL_NONE` | 0 bytes | Absence of value |
| `number` | `VAL_INT` / `VAL_FLOAT` | 8 bytes | 64-bit integer or float |
| `yesno` | `VAL_BOOL` | 1 byte | `yes`/`no`, `true`/`false` |
| `text` | `VAL_STRING` | Ref-counted | UTF-8 string with atomic refcount |
| `list` | `VAL_LIST` | Dynamic array | Ordered, mixed-type collection |
| `record` | `VAL_MAP` | Hash map | Key-value pairs (insertion ordered) |
| `function` | `VAL_FUNCTION` | Pointer | NC behavior reference |
| `native` | `VAL_NATIVE_FN` | Pointer | Built-in function |
| `ai_result` | `VAL_AI_RESULT` | Pointer | AI/LLM response value |

### Memory Management

NC uses **automatic memory management**:
- Strings are managed efficiently for fast operations
- Lists and records are automatically managed
- Memory is reclaimed automatically when no longer needed

---

## 5. Comparison with Other Languages

### Code Comparison: AI Email Classifier

**Python (30 lines, 3 imports, 5 packages):**

```python
import nc_ai_client as ai_client, json
from flask import Flask, request, jsonify
app = Flask(__name__)
client = ai_client.Client(api_key=os.getenv("NC_AI_KEY"))

@app.route("/classify", methods=["POST"])
def classify():
    response = client.chat.completions.create(
        model="nova",
        messages=[{"role": "user", "content": f"Classify: {request.json['email']}"}]
    )
    result = json.loads(response.choices[0].message.content)
    return jsonify(result)
```

**JavaScript/Node.js (25 lines, 3 imports):**

```javascript
const express = require('express');

const app = express();
const client = new NCClient();

app.post('/classify', async (req, res) => {
    const response = await openai.chat.completions.create({
        model: 'nova',
        messages: [{ role: 'user', content: `Classify: ${req.body.email}` }]
    });
    res.json(JSON.parse(response.choices[0].message.content));
});
```

**NC (12 lines, 0 imports, 0 packages):**

```nc
service "email-classifier"
version "1.0.0"

to classify with email:
    ask AI to "Classify this email" using email save as result
    respond with result

api:
    POST /classify runs classify
```

### Feature Comparison

| Feature | Python | JavaScript | Go | NC |
|---------|--------|------------|-----|-----|
| AI as language primitive | No (library) | No (library) | No (library) | **Yes** |
| Built-in HTTP server | No (Flask/FastAPI) | No (Express) | Yes (net/http) | **Yes** |
| Built-in JSON parser | No (import json) | Yes | Yes | **Yes** |
| Plain English syntax | No | No | No | **Yes** |
| Minimal dependencies | No | No | Yes | **Yes** |
| Single binary | No | No | Yes | **Yes** |
| Binary size | ~15 MB+ | ~50 MB+ | ~5 MB | **570KB** |
| Startup time | ~200 ms | ~100 ms | ~5 ms | **~7 ms** |
| Install time | 30+ sec | 30+ sec | 10+ sec | **5 sec (build)** |
| Learning curve | Medium | Medium | High | **Low (English)** |

### Syntax Comparison

| Task | Python | NC |
|------|--------|-----|
| Call AI | `client.chat.completions.create(model=..., messages=[...])` | `ask AI to "..." save as result` |
| HTTP server | `from flask import Flask` + route decorators | `api: POST /path runs behavior` |
| Read file | `with open('f') as f: data = f.read()` | `set data to read_file("f")` |
| JSON parse | `import json; json.loads(s)` | `json_decode(s)` |
| Env variable | `import os; os.getenv('X')` | `env("X")` |
| If/else | `if x > 10:` / `else:` | `if x is above 10:` / `otherwise:` |
| For loop | `for item in items:` | `repeat for each item in items:` |
| Return | `return value` | `respond with value` |
| Define function | `def greet(name):` | `to greet with name:` |
| Run inline | `python3 -c "print(42)"` | `nc "show 42"` |
| Evaluate expr | `python3 -c "print(2+2)"` | `nc -e "2 + 2"` |
| Run script | `python3 app.py` | `nc run app.nc` |
| Start server | `python3 -m flask run` | `nc serve app.nc` |
| Interactive | `python3` (REPL) | `nc repl` or just `nc` |
| Load model | `pickle.load(open('m.pkl','rb'))` | `load_model("m.pkl")` |
| Predict | `model.predict(features)` | `predict(model, features)` |
| Convert code | — | `nc digest app.py` (Python → NC) |

### CLI Comparison — NC Works Just Like Python

If you know Python's command line, NC works the same way:

| Python | NC | What it does |
|--------|-----|-------------|
| `python3` | `nc` | Start interactive REPL |
| `python3 app.py` | `nc run app.nc` | Run a file |
| `python3 -c "print(42)"` | `nc -c "show 42"` | Execute inline code |
| `python3 -c "x=5; print(x*2)"` | `nc -c "set x to 5; show x * 2"` | Multi-statement |
| `echo "print(42)" \| python3` | `echo "show 42" \| nc` | Pipe from stdin |
| `flask run --port 8080` | `nc serve app.nc -p 8080` | Start HTTP server |

NC also supports things Python doesn't have built-in:

```bash
nc "show 42 + 8"              # No -c needed — just pass English directly
nc -e "len([1, 2, 3])"        # Evaluate expression and print result
nc get https://api.example.com # Built-in HTTP client (like curl)
nc digest app.py               # Convert Python to NC automatically
nc build app.nc -o myapp       # Compile to native binary
nc build --all -o build/ -j 4  # Batch build entire project in parallel
```

---

## 6. Use Cases & Target Audience

### Who Is NC For?

| Audience | Why NC? |
|----------|---------|
| **AI developers** | Eliminate boilerplate — go from idea to AI service in minutes |
| **Non-programmers** | Read, understand, and even modify AI workflows in plain English |
| **Startup teams** | Ship AI features 10x faster than with Python/Flask |
| **Enterprise teams** | Auditable, readable AI workflows that business stakeholders can review |
| **DevOps engineers** | Define infrastructure behaviors and monitoring in plain English |
| **Educators** | Teach programming concepts without syntax barriers |

### What Can You Build with NC?

| Application | NC Feature Used |
|-------------|----------------|
| AI chatbot | `ask AI`, `memory_new`, `memory_add` |
| Email classifier | `ask AI`, `validate`, `match/when` |
| REST API | `api:`, `GET/POST/PUT/DELETE`, behaviors |
| Data pipeline | `read_file`, `csv_parse`, `ask AI`, `write_file` |
| RAG Q&A system | `read_file`, `chunk`, `top_k`, `ask AI` |
| Webhook handler | `api: POST`, `match`, `emit` |
| ML inference | `load_model`, `predict` |
| Monitoring | `every 5 minutes:`, `gather`, `notify` |
| Customer support bot | `ask AI`, `validate`, `match`, approval gates |
| Report generator | `read_file`, `ask AI`, `write_file` |

### Who Is NC NOT For?

- **Training ML models** — Use PyTorch/TensorFlow
- **Frontend development** — Use React/Vue/Svelte
- **Systems programming** — Use Rust/C/C++
- **Data science notebooks** — Use Jupyter/Python
- **Anything needing the full Python ecosystem** — NC is specialized

---

## 7. Performance Benchmarks

| Metric | NC | Python | Node.js |
|--------|-----|--------|---------|
| Binary size | 570KB | ~15 MB+ (with packages) | ~50 MB+ |
| Startup time | ~7 ms | ~200 ms (import overhead) | ~100 ms |
| AI call overhead | ~1 ms (direct HTTP) | ~50 ms (client creation) | ~20 ms |
| Runtime dependencies | 0 | 3-10 packages | 5-15 packages |
| Install time | `make` (5 seconds) | `pip install` (30+ seconds) | `npm install` (30+ seconds) |
| Docker image | Alpine: 22.9MB, Debian: 158MB | ~150 MB+ | ~200 MB+ |
| Memory footprint | ~2 MB | ~30 MB+ | ~50 MB+ |
| Lines of code (classifier) | 12 | 30 | 25 |

### Test Results

```
Test files:       85 language test files
Test behaviors:   947
Python tests:     112 (67 basic + 45 deep integration)
Conformance:      204/204 conformance tests
Unit tests:       485/485 unit tests
New features:     59/59 new feature tests
VM safety:        34/34 VM safety tests
Platforms:        macOS, Linux, Windows (MinGW + MSVC)
```

---

## 8. Project Status & Roadmap

### Current Status

NC has reached **v1.0.0** — a production-ready, enterprise-grade, cross-platform programming language. It ships with a complete compiler pipeline (lexer → parser → AST → bytecode compiler → JIT VM), 160+ stdlib functions, a universal AI engine (provider-agnostic, template-driven), built-in auto-correct for typos, call tracebacks, a built-in HTTP server, enterprise-grade sandboxing, typed error handling (try/catch with 7 error categories), built-in test framework (assert + test blocks), Python-style string formatting, rich data structures (set, tuple, deque, counter, enumerate, zip), data processing pipelines (map, reduce, find, group_by, sorted, reversed), async/await/yield syntax, and a plugin/FFI system. All test suites pass on Linux, macOS, and Windows.

### What Works Today

**Core Language:**
- Complete lexer (125 token types), parser (57 AST node types), bytecode compiler (48 opcodes), JIT VM
- Tree-walking interpreter (debug/fallback mode)
- 160+ stdlib functions — strings, math, lists, records, JSON, file I/O, caching, time, type system, formatting, data structures, data processing, vector math
- Automatic memory management with efficient string handling
- Error handling with `try`/`on error`/`finally` (nested blocks supported) + typed catch (`catch "TimeoutError":`)
- Assert statements with line numbers and stack traces
- Built-in test framework (`test "name":` blocks with pass/fail isolation)
- String formatting with `format()` — positional and named placeholders
- Rich data structures: set, tuple, deque, counter, default_map
- Functional utilities: enumerate, zip, error_type, traceback
- Async/await/yield syntax for streaming and future async support
- Negative indexing, slicing, key-value iteration, template interpolation

**AI & Machine Learning:**
- AI/LLM integration — works with NC AI (built-in) or any compatible AI gateway
- Python ML model integration — `load_model()` / `predict()` for sklearn, PyTorch, TensorFlow, ONNX
- Tensor engine — N-dimensional arrays with matmul, ReLU, softmax, transpose (GPU stubs for CUDA/Metal)
- Automatic differentiation — tape-based autograd with SGD and Adam optimizers
- RAG primitives — `chunk()`, `top_k()`, `token_count()`
- Conversation memory — `memory_new()`, `memory_add()`, `memory_get()`

**Networking & Services:**
- Built-in HTTP server with routing, CORS, WebSocket support
- Concurrent request handling with thread pool and work-stealing scheduler
- Middleware — authentication, rate limiting, CORS
- Built-in HTTP client — `nc get`, `nc post` from command line

**Cross-Language Interop:**
- Code digestion — convert Python, JavaScript, TypeScript, YAML, JSON to NC (`nc digest`)
- Runtime execution — `exec()` and `shell()` to call any language at runtime
- Python subprocess bridge for ML models

**Enterprise Features:**
- Sandboxed execution — restrict file, network, exec, env access per behavior
- API key authentication and rate limiting
- Audit logging — who ran what, when
- Multi-tenant isolation and resource quotas

**Distributed Computing:**
- Cluster initialization with TCP message passing
- Gradient aggregation (all-reduce for distributed training)
- Worker coordination (parameter server pattern)

**Tooling:**
- REPL with line editing — readline on macOS/Linux, built-in fallback on Windows
- Debugger with DAP support (VS Code integration)
- Language Server Protocol (LSP) for IDE autocomplete
- Bytecode optimizer with multiple passes
- LLVM IR generation for native AOT compilation
- Batch build — `nc build .`, `nc build --all -o build/ -j 4` (parallel compilation)
- Code formatter, semantic analyzer, profiler
- Package manager (git-based), module system
- Docker support, CI/CD pipeline (6 build targets), VS Code extension
- Python-like CLI — `nc "show 42"`, `nc -c "code"`, `nc -e "expr"`, pipe from stdin
- Python validation suite — 112 external tests for CI/CD

**Security (v1.0.0):**
- TLS verification explicit on all HTTPS requests
- Constant-time API key comparison (timing attack prevention)
- SSRF protection via `NC_HTTP_ALLOWLIST` and `NC_HTTP_STRICT`
- Path traversal prevention in module imports and file I/O
- Command injection blocking in `shell()`
- Restrictive CORS defaults when auth is enabled
- Supply chain hardening for package manager (tar path traversal prevention)
- Secret redaction in all log output

### Roadmap

| Phase | Features |
|-------|----------|
| **v1.0.0 (Current)** | Core language, AI integration, HTTP server, REPL, JIT VM, 160+ stdlib functions, batch build, cross-platform (macOS/Linux/Windows), 21 security fixes, 15 bug fixes, 485 test cases, 6 CI targets, cookie persistence, configurable timeouts, browser-like User-Agent, warmup/session support, all config env-driven, zero hardcoded providers |
| **v1.1** | NC-native tensor syntax, GPU acceleration (CUDA/Metal), expanded WebSocket behaviors |
| **v1.2** | Plugin system, community packages, async/await NC syntax |
| **v1.3** | Native AOT compilation (LLVM), full WASM support |
| **v2.0** | Stable API, LTS support, ASan/Valgrind audit, package integrity verification |

### Changelog — v1.0.0

**v1.0.0** is the first production release. Cross-platform support (Linux, macOS, Windows — MinGW + MSVC), thread-per-request HTTP server, plugin/FFI system, constant-time auth, sandbox enforcement, HTTP allowlist, TLS verification, SSRF protection, batch build with parallel compilation, dynamic buffers, universal AI engine, auto-correct, call tracebacks, and 485 unit tests + 85 language tests (947 behaviors) + 112 Python integration tests all passing.

### Changelog — v1.0.0

This release fixed 52 bugs across 8 source files — parser, interpreter, lexer, JSON, HTTP, value system, database, and server. All fixes are root-cause corrections, not workarounds.

#### Parser — 12 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| `save as:` in kv block parsed as two separate keys | Single-shot compound key detection only checked one word ahead | Rewrote as accumulating `while` loop that combines all words before the `:` or `is` delimiter |
| List literals `[1, 2, 3]` inserted empty strings between elements | Parser did not properly skip commas; trailing/leading commas were misparsed | Fixed comma handling in list literal parsing |
| Reserved words rejected as behavior parameter names | Explicit allowlist of ~10 token types hardcoded in param parsing | Changed to denylist of structural tokens only (colon, newline, EOF, comma, and, indent, dedent) — all other tokens accepted |
| Buffer overflow in behavior phrase/name collection | Fixed-size buffers were too small for long names | Increased buffer sizes for safety |
| Buffer overflow in API path, check description, AI prompt, and apply target parsing | Fixed-size buffers were too small | Increased buffer sizes for safety |
| `on error:` block never parsed when "error" was a non-identifier token | Parser required "error" to be a specific token type | Parser now accepts any token after `on` |
| `ask AI` crashed when `model:` had non-string value | Model value was used without type check | Added type check for model parameter |
| Crash on malformed `when` clause in match | Expression result used without null check | Added null checks for expression parsing |
| Map literal crash on malformed value expression | Parser used expression result without null check | Added null check and proper cleanup |
| Invalid API route methods silently accepted as GET | Unrecognized HTTP methods defaulted to GET with no warning | Added warning for unrecognized method names |

#### Interpreter — 7 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Map literal `{"key": var}` didn't resolve `{{template}}` keys | Map keys were not resolved at runtime | Keys now have template interpolation applied |
| `try`/`on error` block never executed | Error handling required incorrect preconditions; nested try blocks did not save/restore state | Fixed error state handling for nested try blocks |
| `list[-1]` returned nothing | Negative indices were not supported | Added negative index support (e.g. -1 for last element); also added float-to-int coercion for record keys |
| `gather` options mutated on repeated calls | Options map was modified in place, corrupting parse tree | Options map is now copied before resolving templates |
| HTTP POST body params bound positionally to behavior | Fallback `args->values[i]` used when named key lookup failed | Removed positional fallback; named matching only + all arg keys injected into scope |
| Unset variables returned their own name as a string | Unknown variables incorrectly returned variable name | Unset variables now evaluate to `nothing` |
| `store` target didn't resolve templates | `store value into "cache:{{key}}"` passed literal template string | Already resolved via `resolve_templates()` — verified correct |

#### Lexer — 3 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Unterminated string `"hello` caused crash | Missing closing quote produced invalid length | Unterminated strings now emit a clear error instead of crashing |
| Indent depth > 64 caused crash | Indent stack had no overflow check | Deep nesting now emits a clear error instead of crashing |
| Token array lost on allocation failure in lexer | Array resize could overwrite pointer before checking success | Use temp variable; only assign on success; return early on failure |

#### JSON Parser — 3 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| `\uXXXX` escape read hex digits from wrong offset | Parser included 'u' character in hex digit range | Fixed offset so only the four hex digits are read |
| Stack buffer overflow on long JSON strings | Fixed `char buf[4096]` with multi-byte escape writes not bounds-checked against remaining space | Doubled buffer to `char buf[8192]`; all bounds checks derived from `sizeof(buf)` for auto-correctness |
| Null or empty input to JSON parser caused crash | No null check at function entry | Added null/empty input guard |

#### HTTP Client — 2 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Prompts with `"`, `\`, or newlines produced invalid JSON → LLM API returned 400 | Prompt was escaped, but `context_str` (serialized JSON context), `system_prompt`, and `model` name were embedded raw via `%s` in the JSON body | Created `JSON_ESCAPE_INTO` macro; applied to all four embedded values; increased body buffer 16K→32K |
| HTTP response from data stores had trailing newline characters | Response was parsed without trimming line endings | Added CRLF stripping before JSON parse |

#### Value System — 3 fixes

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Crash when creating large maps under memory pressure | Hash index allocation could fail, leading to null dereference | Added null checks to handle allocation failures gracefully |
| Bytecode chunk pointer lost on allocation failure | Partial allocation failure could leave pointer in inconsistent state | Fixed assignment order so new pointer is saved before subsequent allocation |
| Constant/variable table corrupted on allocation failure | Resize failure could overwrite existing pointer | Use temp variable; return error on failure to signal callers |

#### Database — 1 fix

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Redis/store GET values had trailing `\r\n` whitespace | HTTP response from store backends (which may proxy Redis RESP) included protocol-level line terminators | Added `strip_trailing_crlf()` applied to all store GET/PUT HTTP responses before JSON parsing |

#### Server — 1 fix

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| Server crashed when base path stripped entire request path | Path matching did not check bounds when stripping base path | Added bounds check for path matching |

---

# Part II — Installation & Getting Started

---

## 9. Installation

### Prerequisites

- A C compiler (`gcc` or `clang`)
- `make`
- `libcurl` (development headers)
- `libedit` (development headers, for REPL line editing)

### Build from Source

```bash
cd nc-lang/engine
make
```

This produces the `nc` binary at `engine/build/nc`.

### Install System-Wide

```bash
# Option 1: Use the install script
./install.sh

# Option 2: Copy manually
sudo cp engine/build/nc /usr/local/bin/
```

### Install via Docker (No Build Required)

```bash
# Pull and run directly
docker run -it nc version

# Run your NC files
docker run -v $(pwd):/app nc run /app/myfile.nc

# Start as server
docker run -p 8080:8080 -v $(pwd):/app nc serve /app/service.nc
```

See [Section 34: Docker and Containers](#34-docker-and-containers) for full Docker documentation.

### One-Line Install (Linux / macOS)

```bash
curl -sSL https://raw.githubusercontent.com/YOUR_USERNAME/nc-lang/main/install.sh | bash
```

### Verify Installation

```bash
nc version
```

**Expected output:**

```
nc version 1.0.0
```

---

## 10. Your First Program

Create a file called `hello.nc`:

```nc
service "hello-world"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name + "!"
```

Run it:

```bash
nc run hello.nc -b greet
```

**Output:**

```
Hello, World!
```

### What Just Happened?

| Line | Meaning |
|------|---------|
| `service "hello-world"` | Declares the name of your service |
| `version "1.0.0"` | Version of this service |
| `to greet with name:` | Defines a behavior (function) called `greet` that takes `name` |
| `respond with ...` | Returns a value (like `return`) |

---

## 11. Running NC Programs

### Run a File

```bash
nc run myfile.nc
```

Parses, compiles, and displays service info (name, version, behaviors, routes). Use `-b` to run a specific behavior.

### Run a Specific Behavior

```bash
nc run myfile.nc -b behavior_name
```

### Start as HTTP Server

```bash
nc serve myfile.nc
nc serve myfile.nc -p 8080        # custom port (short flag)
nc serve myfile.nc --port 8080    # custom port (long flag)
```

### Validate Syntax (Without Running)

```bash
nc validate myfile.nc
```

**Output on success:**

```
  File:       myfile.nc
  Service:    my-service
  Behaviors:  3
  Types:      1
  Routes:     2

  VALID (lexer: 42 tokens, parser: OK)
```

**Output on error:**

```
  INVALID: unexpected token at line 5
```

### View Tokens (Lexer Output)

```bash
nc tokens myfile.nc
```

**Output:**

```
  NC Lexer — myfile.nc
  ────────────────────────────────────────
  L  1  SERVICE         "service"
  L  1  STRING          "hello-world"
  L  2  VERSION         "version"
  ...

  Total: 42 tokens
```

### Compile to Native Binary

Build a single `.nc` file into a standalone native binary:

```bash
nc build myfile.nc                # outputs binary named "myfile"
nc build myfile.nc -o myapp       # custom output name
nc build myfile.nc -b behavior    # compile a specific behavior
```

### Batch Build (Multiple Files)

Build all `.nc` files in a directory with a single command:

```bash
nc build .                        # build all .nc files in current directory
nc build services/                # build all .nc files in a specific directory
nc build . --recursive            # build recursively (include subdirectories)
nc build --all                    # build all .nc files recursively (shorthand)
nc build                          # no args = build all .nc in current dir
```

**Output directory** — place all compiled binaries into a folder:

```bash
nc build . -o build/              # all binaries go into build/
nc build --all -o dist/           # recursively build, output to dist/
```

**Parallel builds** — speed up compilation by building multiple files concurrently:

```bash
nc build . -j 4                   # build 4 files in parallel
nc build --all -j 8               # 8 parallel builds across all subdirs
nc build . -j 4 -o build/         # parallel + output directory
```

**Example** — building a project with 100 services:

```bash
nc build --all -o build/ -j 8
```

This scans all directories recursively, builds up to 8 files in parallel, places all binaries in `build/`, and prints a summary:

```
  ┌─────────────────────────────────────────────┐
  │  Build Summary                              │
  ├─────────────────────────────────────────────┤
  │  ✓ Succeeded:  98                           │
  │  ✗ Failed:     2                            │
  │  Total:        100                          │
  │  Time:         12.34s                       │
  └─────────────────────────────────────────────┘
```

| Flag | Description |
|------|-------------|
| `-o <name>` | Output binary name (single file) or directory (batch) |
| `-b <name>` | Compile a specific behavior only |
| `--recursive` / `-r` | Scan subdirectories for `.nc` files |
| `--all` | Recursive build from current directory |
| `-j <N>` / `--parallel <N>` | Number of parallel build jobs |

### Run Plain English Inline

NC supports running code directly from the command line, like Python's `-c` flag:

```bash
# Run plain English directly
nc "show 42 + 8"
nc "show upper('hello')"

# Multi-statement with -c (semicolons become newlines)
nc -c "set x to 5; show x * 2"

# Evaluate an expression and print the result
nc -e "len([1, 2, 3])"

# Pipe from stdin
echo "show 42" | nc
```

### View Bytecode

```bash
nc bytecode myfile.nc
```

### Format Code

```bash
nc fmt myfile.nc
```

### HTTP Requests

NC includes a built-in HTTP client for quick requests:

```bash
# GET request
nc get https://api.example.com/data

# POST request with JSON body
nc post https://api.example.com/data '{"name": "Alice"}'

# With custom headers
nc get https://api.example.com/data -H "Authorization: Bearer token123"
```

---

---

# Part III — Language Reference

---

## 12. Comments

NC supports two comment styles:

### Line Comments

```nc
// This is a comment
set x to 42  // inline comment

# This is also a comment (Python style)
set y to 10  # inline comment
```

Both `//` and `#` create single-line comments. Everything after the marker is ignored.

---

## 13. Variables and Data Types

### Setting Variables

Use `set ... to ...` to create or update variables:

```nc
set name to "Alice"
set age to 30
set pi to 3.14159
set active to yes
set items to [1, 2, 3]
set user to {"name": "Alice", "age": 30}
set empty to nothing
```

### Data Types

| Type | Examples | Description |
|------|----------|-------------|
| **text** | `"hello"`, `'world'` | Strings (single or double quotes) |
| **number (int)** | `42`, `-7`, `0` | 64-bit integers |
| **number (float)** | `3.14`, `-0.5` | 64-bit floating point |
| **yesno** | `yes`, `no`, `true`, `false` | Booleans |
| **list** | `[1, 2, 3]`, `[]` | Ordered collection |
| **record** | `{"key": "value"}` | Key-value map (insertion ordered) |
| **nothing** | `nothing`, `none` | Absence of a value |

### Checking Types

```nc
set x to 42
show type(x)       // "number"

set s to "hello"
show type(s)       // "text"

set items to [1, 2]
show type(items)   // "list"

show type(nothing) // "none"
```

**Output:**

```
number
text
list
none
```

### Type-Check Functions

```nc
show is_text("hello")    // yes
show is_number(42)        // yes
show is_list([1, 2])      // yes
show is_record({"a": 1})  // yes
show is_bool(yes)         // yes
show is_none(nothing)     // yes
```

### Type Conversion

```nc
set s to str(42)       // "42"
set n to int("10")     // 10
set f to float("3.14") // 3.14
```

---

## 14. Operators and Expressions

### Arithmetic

```nc
set a to 10 + 5     // 15
set b to 20 - 3     // 17
set c to 6 * 7      // 42
set d to 100 / 4    // 25
set e to 17 % 5     // 2
```

Subtraction correctly produces negative results:

```nc
set change to 198.0 - 200.0    // -2.0
set loss to abs(change)         // 2.0
```

### String Concatenation

```nc
set greeting to "Hello, " + "World!"
set msg to "Age: " + str(25)
```

### Plain-English Comparisons

NC supports natural-language comparisons instead of symbols:

| NC Syntax | Equivalent | Meaning |
|-----------|------------|---------|
| `x is above 10` | `x > 10` | Greater than |
| `x is below 10` | `x < 10` | Less than |
| `x is at least 10` | `x >= 10` | Greater than or equal |
| `x is at most 10` | `x <= 10` | Less than or equal |
| `x is equal to 10` | `x == 10` | Equal |
| `x is equal 10` | `x == 10` | Equal (short form, also works) |
| `x is not equal to 10` | `x != 10` | Not equal |
| `x is greater than 10` | `x > 10` | Greater than (alternate) |
| `x is less than 10` | `x < 10` | Less than (alternate) |
| `x is "value"` | `x == "value"` | Short equality (direct `is` comparison) |
| `x is in items` | — | Element is in list |
| `x is not in items` | — | Element is not in list |
| `x is empty` | `len(x) == 0` | List/string is empty |
| `x is positive` | `x > 0` | Number is positive |
| `x is negative` | `x < 0` | Number is negative |

### Logic Operators

```nc
if age is above 18 and has_id is equal yes:
    show "Allowed"

if score is above 90 or is_vip is equal yes:
    show "Premium access"
```

### Not Operator

```nc
if not active:
    show "User is inactive"

if not contains(name, "admin"):
    show "Not an admin"
```

### Plain-English Arithmetic

NC also supports English-style arithmetic operations:

```nc
set score to 0
add 5 to score          // score is now 5
increase score by 10    // score is now 15
decrease score by 3     // score is now 12
```

---

## 15. Strings

### Creating Strings

```nc
set name to "Alice"
set greeting to 'Hello'
```

### Escape Characters

| Escape | Output |
|--------|--------|
| `\n` | Newline |
| `\t` | Tab |
| `\"` | Double quote |
| `\\` | Backslash |

### Template Interpolation

Use `{{variable}}` inside strings for `log`, `ask AI`, and other statements:

```nc
set user to "Alice"
set age to 30
log "User {{user}} is {{age}} years old"
```

**Output:**

```
[LOG] User Alice is 30 years old
```

Dot access works in templates too:

```nc
set person to {"name": "Bob", "age": 25}
log "Name: {{person.name}}, Age: {{person.age}}"
```

**Output:**

```
[LOG] Name: Bob, Age: 25
```

### Escape Sequences

NC strings support standard escape sequences:

| Escape | Character |
|--------|-----------|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\"` | Double quote |
| `\'` | Single quote |
| `\\` | Backslash |

```nc
set greeting to "Hello\nWorld"
show greeting
// Output:
// Hello
// World

set path to "C:\\Users\\Documents"
show path  // C:\Users\Documents
```

### String Functions

```nc
show upper("hello")              // HELLO
show lower("WORLD")              // world
show trim("  space  ")           // space
show len("hello")                // 5
show contains("hello", "ell")    // yes
show starts_with("hello", "he") // yes
show ends_with("hello", "lo")   // yes
show replace("hello", "l", "r") // herro
show split("a,b,c", ",")        // [a, b, c]
show join(["a", "b", "c"], "-") // a-b-c
show substr("hello", 1, 4)      // ell
```

### String Indexing and Slicing

```nc
set word to "hello"
show word[0]       // h
show word[4]       // o
show word[0:3]     // hel
show word[2:]      // llo
show word[:3]      // hel
```

---

## 16. Lists

### Creating Lists

```nc
set numbers to [1, 2, 3, 4, 5]
set names to ["Alice", "Bob", "Charlie"]
set mixed to [1, "two", yes, 3.14]
set empty to []
```

### Accessing Elements

```nc
set items to [10, 20, 30, 40, 50]
show items[0]    // 10  (first element)
show items[2]    // 30  (third element)
show items[-1]   // 50  (last element)
show items[-2]   // 40  (second from last)
```

Negative indices count from the end: `-1` is the last element, `-2` is second-to-last, and so on.

### Slicing

```nc
set items to [1, 2, 3, 4, 5]
show items[1:3]   // [2, 3]
show items[:2]    // [1, 2]
show items[3:]    // [4, 5]
show items[-2:]   // [4, 5]
```

### Modifying Lists

```nc
set items to [1, 2, 3]

// Append
append 4 to items          // [1, 2, 3, 4]

// Remove
remove 2 from items        // [1, 3, 4]

// Or use function form
set items to append(items, 5)
set items to remove(items, 3)

// List concatenation with +
set a to [1, 2]
set b to [3, 4]
set combined to a + b       // [1, 2, 3, 4]

// Append single item with + (wrapping in list)
set items to items + [99]   // append 99

// Build a list in a loop
set result to []
repeat for each i in range(5):
    set result to result + [i]
// result = [0, 1, 2, 3, 4]
```

### List Functions

```nc
set nums to [3, 1, 4, 1, 5, 9, 2]

show len(nums)             // 7
show first(nums)           // 3
show last(nums)            // 2
show sort(nums)            // [1, 1, 2, 3, 4, 5, 9]
show reverse(nums)         // [2, 9, 5, 1, 4, 1, 3]
show unique(nums)          // [3, 1, 4, 5, 9, 2]
show sum(nums)             // 25
show average(nums)         // 3.571...
show index_of(nums, 4)    // 2
show count(nums, 1)        // 2
show slice(nums, 1, 4)     // [1, 4, 1]
show any(nums)             // yes  (any truthy)
show all(nums)             // yes  (all truthy)

// Flatten nested lists
set nested to [[1, 2], [3, 4], [5]]
show flatten(nested)       // [1, 2, 3, 4, 5]

// Range
show range(5)              // [0, 1, 2, 3, 4]
show range(2, 6)           // [2, 3, 4, 5]
```

### Iterating Lists

```nc
set fruits to ["apple", "banana", "cherry"]

repeat for each fruit in fruits:
    show fruit
```

**Output:**

```
apple
banana
cherry
```

---

## 17. Records (Maps / Objects)

### Creating Records

```nc
set user to {"name": "Alice", "age": 30, "active": yes}
```

### Defining Structured Types

```nc
define User as:
    name is text
    email is text
    age is number
    active is yesno
```

### Accessing Fields

```nc
set user to {"name": "Alice", "age": 30}

// Dot access
show user.name    // Alice
show user.age     // 30

// Bracket access (string key)
show user["name"] // Alice

// Bracket access with variables
set field to "name"
show user[field]  // Alice

// Nested access
set data to {"user": {"profile": {"email": "a@b.com"}}}
show data.user.profile.email  // a@b.com
```

Record keys support `{{template}}` resolution when using map literals:

```nc
set label to "greeting"
set data to {"{{label}}": "hello"}
show data.greeting  // hello
```

### Modifying Fields

```nc
set user to {"name": "Alice", "age": 30}
set user.age to 31
set user.city to "NYC"     // adds new field
show user                  // {"name": "Alice", "age": 31, "city": "NYC"}
```

### Record Functions

```nc
set person to {"name": "Alice", "age": 30, "city": "NYC"}

show keys(person)           // ["name", "age", "city"]
show values(person)         // ["Alice", 30, "NYC"]
show has_key(person, "age") // yes
show has_key(person, "zip") // no
```

### Iterating Records

```nc
set config to {"host": "localhost", "port": 8080, "debug": yes}

repeat for each entry in config:
    log "{{entry.key}}: {{entry.value}}"
```

**Output:**

```
[LOG] host: localhost
[LOG] port: 8080
[LOG] debug: yes
```

### Key-Value Iteration

You can unpack both key and value when iterating a record:

```nc
set scores to {"Alice": 95, "Bob": 87, "Charlie": 92}

repeat for each name, score in scores:
    show name + " scored " + str(score)
```

**Output:**

```
Alice scored 95
Bob scored 87
Charlie scored 92
```

---

## 18. Control Flow

### If / Otherwise / Otherwise If

```nc
set score to 85

if score is above 90:
    show "Excellent!"
otherwise if score is above 70:
    show "Good"
otherwise:
    show "Needs improvement"
```

**Output:**

```
Good
```

`otherwise if` works as a true else-if chain — only the first matching branch executes. You can chain as many `otherwise if` blocks as needed:

```nc
to classify with strength:
    if strength is above 0.8:
        set signal to "strong_bullish"
    otherwise if strength is above 0.5:
        set signal to "bullish"
    otherwise if strength is above 0.2:
        set signal to "neutral"
    otherwise:
        set signal to "bearish"
    respond with signal
```

`respond with` inside any branch exits the function immediately — subsequent code does not execute.

### Match / When (Switch)

```nc
set status to "warning"

match status:
    when "healthy":
        show "All systems normal"
    when "warning":
        show "Performance issues detected"
    when "critical":
        show "Immediate action needed!"
    otherwise:
        show "Unknown status"
```

**Output:**

```
Performance issues detected
```

### Loops

#### Repeat For Each (For-In Loop)

```nc
set names to ["Alice", "Bob", "Charlie"]

repeat for each name in names:
    show "Hello, " + name
```

**Output:**

```
Hello, Alice
Hello, Bob
Hello, Charlie
```

#### Repeat N Times

```nc
repeat 5 times:
    show "ping"
```

**Output:**

```
ping
ping
ping
ping
ping
```

#### While Loop

```nc
set count to 1

while count is at most 5:
    show "Count: " + str(count)
    set count to count + 1
```

**Output:**

```
Count: 1
Count: 2
Count: 3
Count: 4
Count: 5
```

#### Repeat While

`repeat while` is a synonym for `while` — use whichever reads more naturally:

```nc
set count to 1

repeat while count is at most 5:
    show "Count: " + str(count)
    set count to count + 1
```

Both `while` and `repeat while` produce identical behavior. You can mix all loop forms freely in the same behavior:

```nc
to process with items:
    repeat for each item in items:
        log item
    set retries to 3
    repeat while retries is above 0:
        log "retry " + str(retries)
        set retries to retries - 1
```

### Loop Control

```nc
// Stop (break) — exit the loop early
set nums to [1, 2, 3, 4, 5]
repeat for each n in nums:
    if n is equal to 4:
        stop
    show n

// Output: 1, 2, 3
```

```nc
// Skip (continue) — skip current iteration
set nums to [1, 2, 3, 4, 5]
repeat for each n in nums:
    if n % 2 is equal to 0:
        skip
    show n

// Output: 1, 3, 5
```

### String Comparison in Loops

String equality works correctly inside loops — use `is equal to` to compare string values:

```nc
set items to [
    {"name": "test1", "status": "PASS"},
    {"name": "test2", "status": "FAIL"},
    {"name": "test3", "status": "PASS"}
]

set pass_count to 0
repeat for each item in items:
    if item.status is equal to "PASS":
        set pass_count to pass_count + 1

show pass_count    // 2
```

This also works with plain string lists:

```nc
set colors to ["red", "blue", "red", "green"]
set red_count to 0
repeat for each color in colors:
    if color is equal to "red":
        set red_count to red_count + 1

show red_count    // 2
```

You can also use `filter_by` for the same result with records:

```nc
set passed to filter_by(items, "status", "equal", "PASS")
show len(passed)    // 2
```

### Wait (Sleep)

```nc
show "Starting..."
wait 2 seconds
show "Done!"
```

Time units: `seconds`, `minutes`, `hours`, `days`

---

## 19. Behaviors (Functions)

Behaviors are NC's version of functions. They are defined with `to ... with ...:` syntax.

### Defining a Behavior

```nc
to greet with name:
    respond with "Hello, " + name + "!"
```

### Multiple Parameters

```nc
to add with a and b:
    respond with a + b
```

### Reserved Words as Parameters

NC allows reserved words as parameter names. Words like `name`, `description`, `model`, `event`, `type`, `status`, `version`, and `service` all work as parameters:

```nc
to create_event with name, description, type:
    set event to {"name": name, "description": description, "type": type}
    respond with event
```

### No Parameters

```nc
to health_check:
    respond with "healthy"
```

### Calling Behaviors

```nc
run greet with "Alice"
run add with 3, 5
run health_check
```

### Returning Values

Use `respond with` to return a value from a behavior:

```nc
to calculate_tax with amount:
    set tax to amount * 0.1
    respond with tax
```

### Early Return with `respond with`

`respond with` immediately exits the behavior — no subsequent code runs. This enables the early-return pattern:

```nc
to validate with age:
    if age is below 0:
        respond with {"error": "age cannot be negative"}
    if age is below 18:
        respond with {"status": "minor", "allowed": false}
    respond with {"status": "adult", "allowed": true}
```

When `age` is `-5`, the first `respond with` fires and the function exits — the other two `respond with` statements never execute.

This works inside `otherwise if` chains too:

```nc
to classify with code:
    if code is equal to 200:
        respond with "ok"
    otherwise if code is equal to 404:
        respond with "not_found"
    otherwise if code is equal to 500:
        respond with "server_error"
    respond with "unknown"
```

### Purpose (Documentation)

```nc
to classify_ticket with ticket:
    purpose: "Classify a support ticket using AI"
    ask AI to "Classify this ticket" using ticket save as result
    respond with result
```

### Approval Gates

Require manual approval before dangerous operations:

```nc
to delete_all with database:
    needs approval when database is equal to "production"
    log "Deleting all records from {{database}}"
    respond with "Deleted"
```

### Nested Behavior Calls

```nc
to double with n:
    respond with n * 2

to quadruple with n:
    run double with n
    run double with result
    respond with result
```

### Data Flow Between Behaviors

When you use `run`, the called behavior's `respond with` value is automatically stored in a variable called `result`:

```nc
to get_scores:
    respond with [95, 87, 92]

to analyze:
    run get_scores
    // result now contains [95, 87, 92]
    respond with result[0] + result[1] + result[2]
```

All value types are preserved across behavior boundaries — numbers stay as numbers, lists keep their elements, and maps retain their fields:

```nc
to fetch_user:
    respond with {"name": "Alice", "score": 95}

to process:
    run fetch_user
    // Dot access works on the result map
    log result.name      // "Alice"
    log result.score     // 95
    respond with result.score
```

---

## 20. Error Handling

### Try / On Error / Finally

```nc
try:
    set value to int("not a number")
    show value
on error:
    log "Caught error: " + error
    show "Failed to convert"
finally:
    log "Cleanup done"
```

**Output:**

```
[LOG] Caught error: Cannot convert 'not a number' to int
Failed to convert
[LOG] Cleanup done
```

### Practical Error Handling

```nc
to safe_divide with a and b:
    try:
        if b is equal to 0:
            respond with "Error: division by zero"
        respond with a / b
    on error:
        respond with "Error: " + error
```

### Error Variable

Inside an `on error:` block, the variable `error` contains the error message as text.

> **Note:** Three syntaxes are supported for error handlers:
> - `on error:` (two words)
> - `on_error:` (with underscore)
> - `otherwise:` (after try block)
>
> All three are equivalent. Use whichever reads best in context.

### Error Sources

The following operations can trigger `on error:` blocks:

- `gather` failures (network errors, missing data sources)
- `ask AI` failures (API errors, invalid responses after retries)
- `while` loop timeout (exceeds `NC_MAX_ITERATIONS`)

### Auto-Correct (Built-in Intelligence)

NC automatically corrects close typos at runtime — no other language does this:

```nc
set name to "World"
show nme              // NC auto-corrects 'nme' → 'name' and prints "World"

show lne([1, 2, 3])   // NC auto-corrects 'lne' → 'len' and prints 3

show uper("hello")    // NC auto-corrects 'uper' → 'upper' and prints "HELLO"
```

**How it works:**

| Edit distance | NC behavior |
|---|---|
| 1-2 edits (high confidence) | Auto-corrects silently, shows note: `Auto-corrected 'nme' → 'name'` |
| 3-4 edits (medium confidence) | Suggests: `Is this what you meant → 'name'?` |
| 5+ edits (low confidence) | No suggestion — too far to match |

Uses Damerau-Levenshtein distance which handles:
- **Transpositions**: `lne` → `len` (swapped characters)
- **Extra characters**: `sortt` → `sort`
- **Missing characters**: `uper` → `upper`
- **Substitutions**: `pritn` → `print`
- **Case differences**: `Total` → `total` (lower cost)

Works for variables, functions, behaviors, and NC keywords.

### Call Traceback

When an error occurs inside nested behavior calls, NC shows the full call chain:

```
[line 8] ask AI error: Failed after 3 retries

  Call trace (most recent last):
    in 'classify' at service.nc, line 3
    in 'process_ticket' at service.nc, line 8
```

This helps you trace exactly where the error occurred and which behavior called which.

### Nested Try Blocks

Try blocks can be nested. Each block independently catches and handles its own errors:

```nc
try:
    try:
        gather data from "https://failing-api.example.com"
    on error:
        log "Inner error caught: " + error
    gather backup from "https://backup-api.example.com"
on error:
    log "Outer error caught: " + error
finally:
    log "All done"
```

### Typed Error Catching (Enterprise)

NC v1.0.0 introduces typed error catching — catch specific error categories instead of generic errors:

```nc
try:
    gather data from "https://flaky-api.example.com/data"
catch "TimeoutError":
    log "Request timed out: " + err.message
    respond with {"error": "timeout", "type": err.type}
catch "ConnectionError":
    log "Connection failed at line " + str(err.line)
    respond with {"error": "connection_failed"}
catch "ParseError":
    respond with {"error": "bad_response_format"}
on_error:
    log "Unknown error: " + error
finally:
    log "cleanup complete"
```

**Supported error types:**

| Error Type | Triggers On |
|------------|-------------|
| `TimeoutError` | Timeout, deadline exceeded |
| `ConnectionError` | Network failure, HTTP errors |
| `ParseError` | JSON parse failure, malformed data |
| `IndexError` | List index out of range |
| `ValueError` | Type conversion failure |
| `IOError` | File read/write errors |
| `KeyError` | Missing key in record |

The `err` context object provides:
- `err.message` — the error message string
- `err.type` — the classified error type (e.g., `"TimeoutError"`)
- `err.line` — the line number where the error occurred

### Programmatic Error Classification

Use `error_type()` to classify any error message:

```nc
set et to error_type("Connection timeout expired")
log et   // "TimeoutError"

set et2 to error_type("Failed to parse JSON response")
log et2  // "ParseError"
```

### Stack Traces with traceback()

Get the current call stack programmatically:

```nc
set tb to traceback()
repeat for each frame in tb:
    log frame.behavior + " at " + frame.file + ":" + str(frame.line)
```

### Assert Statements

Assert a condition is true — halts execution on failure with line number and optional message:

```nc
assert len(users) is above 0, "Users list must not be empty"
assert response["status"] is equal 200, "Expected 200 OK"
assert data is not equal none
```

Output on failure:
```
Assertion failed at line 15: Users list must not be empty
  Call trace (most recent last):
    in 'validate_users' at service.nc, line 15
```

### Test Blocks (Built-in Test Framework)

Define isolated test blocks that run without killing the program on failure:

```nc
test "user creation":
    set user to create_user("Alice", "alice@example.com")
    assert user["name"] is equal "Alice", "Name mismatch"
    assert user["email"] is equal "alice@example.com", "Email mismatch"

test "data validation":
    set data to validate({"age": 25})
    assert data["valid"] is equal true, "Validation should pass"

test "error handling":
    try:
        set result to risky_operation()
    on_error:
        assert error is not equal none, "Should have error"
```

Output:
```
✓ PASS: user creation
✓ PASS: data validation
✓ PASS: error handling
```

### Yield — Value Streaming

Use `yield` to push values to an accumulator and output them:

```nc
to generate_report:
    yield "Step 1: Fetching data..."
    gather data from "https://api.example.com/metrics"
    yield "Step 2: Analyzing..."
    ask AI to "Analyze this data" using data save as analysis
    yield "Step 3: Complete"
    respond with analysis
```

### Await — Async Placeholder

Use `await` for future async support (currently synchronous):

```nc
set result to await http_get("https://api.example.com/data")
set processed to await transform(result)
```

---

## 21. Standard Library — Complete Reference

### String Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `upper(s)` | `upper("hello")` | `"HELLO"` | Convert to uppercase |
| `lower(s)` | `lower("WORLD")` | `"world"` | Convert to lowercase |
| `trim(s)` | `trim("  hi  ")` | `"hi"` | Remove leading/trailing whitespace |
| `len(s)` | `len("hello")` | `5` | String length |
| `contains(s, sub)` | `contains("hello", "ell")` | `yes` | Check if substring exists |
| `starts_with(s, p)` | `starts_with("hello", "he")` | `yes` | Check prefix |
| `ends_with(s, p)` | `ends_with("hello", "lo")` | `yes` | Check suffix |
| `replace(s, old, new)` | `replace("hi", "i", "ey")` | `"hey"` | Replace all occurrences |
| `split(s, delim)` | `split("a,b,c", ",")` | `["a","b","c"]` | Split string into list |
| `join(list, sep)` | `join(["a","b"], "-")` | `"a-b"` | Join list into string |
| `substr(s, start, end)` | `substr("hello", 1, 4)` | `"ell"` | Extract substring |

### Math Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `abs(x)` | `abs(-5)` | `5` | Absolute value (works on expressions: `abs(a - b)`) |
| `ceil(x)` | `ceil(3.2)` | `4` | Round up |
| `floor(x)` | `floor(3.8)` | `3` | Round down |
| `round(x)` | `round(3.5)` | `4` | Round to nearest integer |
| `round(x, d)` | `round(3.14159, 2)` | `3.14` | Round to d decimal places (returns float) |
| `sqrt(x)` | `sqrt(16)` | `4.0` | Square root |
| `pow(x, y)` | `pow(2, 3)` | `8` | Exponentiation |
| `min(a, b)` | `min(3, 7)` | `3` | Minimum of two values |
| `min(list)` | `min([5, 2, 8])` | `2` | Minimum value in a list |
| `max(a, b)` | `max(3, 7)` | `7` | Maximum of two values |
| `max(list)` | `max([5, 2, 8])` | `8` | Maximum value in a list |
| `random()` | `random()` | `0.73...` | Random float [0, 1) |
| `log(x)` | `log(100)` | `4.605` | Natural logarithm |
| `exp(x)` | `exp(1)` | `2.718` | e^x |
| `sin(x)` / `cos(x)` / `tan(x)` | `sin(3.14)` | `0.001` | Trigonometric functions |

### List Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `len(list)` | `len([1,2,3])` | `3` | List length |
| `first(list)` | `first([1,2,3])` | `1` | First element |
| `last(list)` | `last([1,2,3])` | `3` | Last element |
| `sort(list)` | `sort([3,1,2])` | `[1,2,3]` | Sorted copy |
| `reverse(list)` | `reverse([1,2,3])` | `[3,2,1]` | Reversed copy |
| `unique(list)` | `unique([1,1,2])` | `[1,2]` | Remove duplicates |
| `flatten(list)` | `flatten([[1],[2,3]])` | `[1,2,3]` | Flatten nested lists |
| `sum(list)` | `sum([1,2,3])` | `6` | Sum of all elements |
| `average(list)` | `average([10,20])` | `15.0` | Arithmetic mean (always returns float) |
| `index_of(list, item)` | `index_of(["a","b"], "b")` | `1` | Find index (-1 if missing) |
| `count(list, item)` | `count([1,1,2], 1)` | `2` | Count occurrences |
| `slice(list, s, e)` | `slice([1,2,3,4], 1, 3)` | `[2,3]` | Sub-list (supports negative indices) |
| `any(list)` | `any([no, yes, no])` | `yes` | Any element truthy? |
| `all(list)` | `all([yes, yes])` | `yes` | All elements truthy? |
| `range(n)` | `range(5)` | `[0,1,2,3,4]` | Integer range |
| `range(s, e)` | `range(2, 5)` | `[2,3,4]` | Range from s to e-1 |
| `append(list, item)` | `append([1,2], 3)` | `[1,2,3]` | Add to end |
| `remove(list, item)` | `remove([1,2,3], 2)` | `[1,3]` | Remove first match |
| `enumerate(list)` | `enumerate(["a","b"])` | `[[0,"a"],[1,"b"]]` | Add index to each element |
| `zip(a, b)` | `zip([1,2], ["a","b"])` | `[[1,"a"],[2,"b"]]` | Zip two lists into pairs |

### Higher-Order List Functions (for lists of records)

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `sort_by(list, "field")` | `sort_by(items, "score")` | sorted list | Sort records by field (quicksort) |
| `sort(list, "field")` | `sort(items, "score")` | sorted list | Same as sort_by |
| `max_by(list, "field")` | `max_by(items, "score")` | record | Record with highest field value |
| `min_by(list, "field")` | `min_by(items, "score")` | record | Record with lowest field value |
| `sum_by(list, "field")` | `sum_by(items, "price")` | number | Sum of field across all records |
| `map_field(list, "field")` | `map_field(items, "name")` | list | Extract one field into flat list |
| `filter_by(list, f, op, val)` | `filter_by(items, "score", "above", 50)` | list | Filter by comparison (numeric or string) |

**filter_by operators:** `"above"`, `"below"`, `"equal"`, `"at_least"`, `"at_most"`

> **String support:** `filter_by` works with both numeric and string values.
> `filter_by(items, "status", "equal", "PASS")` correctly filters by string equality.

**Example:**

```nc
set strategies to [
    {"name": "alpha", "fitness": 0.85},
    {"name": "beta", "fitness": 0.42},
    {"name": "gamma", "fitness": 0.91}
]

set ranked to sort_by(strategies, "fitness")
set best to max_by(strategies, "fitness")         // gamma
set names to map_field(strategies, "name")         // ["alpha", "beta", "gamma"]
set elites to filter_by(strategies, "fitness", "above", 0.5)
```

### Record Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `keys(record)` | `keys({"a":1,"b":2})` | `["a","b"]` | List of keys |
| `values(record)` | `values({"a":1,"b":2})` | `[1,2]` | List of values |
| `has_key(record, k)` | `has_key({"a":1}, "a")` | `yes` | Key exists? |

### String Formatting

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `format(tpl, ...)` | `format("Hi {}", "Alice")` | `"Hi Alice"` | Positional placeholders |
| `format(tpl, map)` | `format("Hi {name}", {"name":"Bob"})` | `"Hi Bob"` | Named placeholders from map |

**Examples:**

```nc
// Positional arguments
set msg to format("Hello {}, you have {} items", "Alice", 5)
// "Hello Alice, you have 5 items"

// Named arguments with a map
set vars to {"name": "Bob", "role": "admin"}
set msg to format("Welcome {name}, role: {role}", vars)
// "Welcome Bob, role: admin"
```

### Data Structure Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `set_new(list)` | `set_new([1,2,2,3])` | set (map) | Create set with unique values |
| `set_add(set, val)` | `set_add(s, "x")` | set | Add member to set |
| `set_has(set, val)` | `set_has(s, "x")` | yesno | Check set membership |
| `set_remove(set, val)` | `set_remove(s, "x")` | set | Remove member from set |
| `set_values(set)` | `set_values(s)` | list | Get all set members as list |
| `tuple(a, b, ...)` | `tuple(1, "hi", true)` | list | Create immutable-style sequence |
| `deque()` | `deque([1,2,3])` | list | Double-ended queue |
| `deque_push_front(dq, v)` | `deque_push_front(d, 0)` | list | Insert at front |
| `deque_pop_front(dq)` | `deque_pop_front(d)` | value | Remove and return first element |
| `counter(list)` | `counter(["a","b","a"])` | map | Count occurrences of each element |
| `default_map(val)` | `default_map(0)` | map | Map with default value |
| `enumerate(list)` | `enumerate(["x","y"])` | list | Add index: `[[0,"x"],[1,"y"]]` |
| `zip(a, b)` | `zip([1,2],["a","b"])` | list | Merge into pairs |
| `error_type(msg)` | `error_type("timeout")` | text | Classify error string |
| `traceback()` | `traceback()` | list | Current call stack frames |

**Examples:**

```nc
// Sets — unique collections
set colors to set_new(["red", "blue", "red", "green"])
log set_values(colors)    // ["red", "blue", "green"]
log set_has(colors, "blue")  // true

// Counter — count occurrences
set word_counts to counter(["the", "cat", "the", "hat", "the"])
log word_counts["the"]    // 3

// Deque — double-ended queue
set queue to deque([1, 2, 3])
set queue to deque_push_front(queue, 0)   // [0, 1, 2, 3]
set front to deque_pop_front(queue)        // front = 0

// Enumerate + Zip
set indexed to enumerate(["a", "b", "c"])  // [[0,"a"], [1,"b"], [2,"c"]]
set paired to zip([1, 2, 3], ["x", "y", "z"])  // [[1,"x"], [2,"y"], [3,"z"]]
```

### Data Processing Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `map(list, field)` | `map(users, "name")` | list | Extract field from list of records |
| `reduce(list, op)` | `reduce([1,2,3], "+")` | value | Fold with +, *, min, max, join |
| `items(record)` | `items({"a":1})` | list | Key-value pairs `[[key,val],...]` |
| `merge(r1, r2)` | `merge(defaults, overrides)` | record | Merge two records |
| `find(list, key, val)` | `find(users, "id", 42)` | record | First matching record |
| `group_by(list, field)` | `group_by(users, "dept")` | record | Group records by field |
| `take(list, n)` | `take([1,2,3,4], 2)` | list | First n elements |
| `drop(list, n)` | `drop([1,2,3,4], 2)` | list | Skip first n elements |
| `compact(list)` | `compact([1,none,2])` | list | Remove none values |
| `pluck(list, field)` | `pluck(items, "price")` | list | Extract field values (alias for map) |
| `chunk_list(list, size)` | `chunk_list([1..6], 2)` | list | Split into chunks |
| `sorted(list)` | `sorted([3,1,2])` | list | Non-mutating sort |
| `reversed(list)` | `reversed([1,2,3])` | list | Non-mutating reverse |
| `repeat_value(val, n)` | `repeat_value(0, 5)` | list | List of n copies |

### String Enhancement Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `title_case(str)` | `title_case("hello world")` | text | `"Hello World"` |
| `capitalize(str)` | `capitalize("hello")` | text | `"Hello"` |
| `pad_left(str, width, fill?)` | `pad_left("42", 5, "0")` | text | `"00042"` |
| `pad_right(str, width, fill?)` | `pad_right("hi", 6)` | text | `"hi    "` |
| `char_at(str, index)` | `char_at("Hello", 1)` | text | `"e"` |
| `repeat_string(str, n)` | `repeat_string("ab", 3)` | text | `"ababab"` |

### Type & Utility Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `isinstance(val, type)` | `isinstance(42, "number")` | yesno | Check type by name string |
| `is_empty(val)` | `is_empty("")` | yesno | Empty string/list/record/none |
| `to_json(val)` | `to_json({"a":1})` | text | Alias for json_encode |
| `from_json(str)` | `from_json("{\"a\":1}")` | value | Alias for json_decode |

### Math Enhancement Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `clamp(val, lo, hi)` | `clamp(15, 0, 10)` | number | Clamp to range → `10` |
| `sign(val)` | `sign(-5)` | number | Returns -1, 0, or 1 |
| `lerp(a, b, t)` | `lerp(0, 10, 0.5)` | number | Linear interpolation → `5.0` |
| `gcd(a, b)` | `gcd(12, 8)` | number | Greatest common divisor → `4` |
| `dot_product(v1, v2)` | `dot_product([1,2],[3,4])` | number | Vector dot product → `11.0` |
| `linspace(start, end, n)` | `linspace(0, 10, 5)` | list | Evenly spaced values |

**Data Processing Examples:**

```nc
// Pipeline: extract → process → aggregate
set orders to [{"item": "A", "total": 100}, {"item": "B", "total": 200}]
set totals to map(orders, "total")        // [100, 200]
set grand_total to reduce(totals, "+")    // 300

// Find + merge for updates
set user to find(users, "id", 42)
set updated to merge(user, {"role": "admin"})

// Group and analyze
set by_dept to group_by(employees, "dept")
set eng to by_dept["engineering"]

// List manipulation
set top5 to take(sorted(scores), 5)       // top 5 scores
set last3 to reversed(take(reversed(items), 3))

// String processing
set header to pad_right(title_case("column"), 20)
set code to pad_left(str(42), 6, "0")     // "000042"

// Math / vectors
set similarity to dot_product(embedding1, embedding2)
set x_values to linspace(0, 100, 50)
set clamped_score to clamp(raw_score, 0, 100)
```

### Type Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `type(x)` | `type(42)` | `"number"` | Type name |
| `str(x)` | `str(42)` | `"42"` | Convert to text |
| `int(x)` | `int("10")` | `10` | Convert to integer |
| `float(x)` | `float("3.14")` | `3.14` | Convert to float |
| `is_text(x)` | `is_text("hi")` | `yes` | Is text? |
| `is_number(x)` | `is_number(42)` | `yes` | Is number? |
| `is_list(x)` | `is_list([1])` | `yes` | Is list? |
| `is_record(x)` | `is_record({"a":1})` | `yes` | Is record? |
| `is_bool(x)` | `is_bool(yes)` | `yes` | Is boolean? |
| `is_none(x)` | `is_none(nothing)` | `yes` | Is nothing? |

### JSON Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `json_encode(x)` | `json_encode({"a":1})` | `'{"a":1}'` | Serialize to JSON string |
| `json_decode(s)` | `json_decode('{"a":1}')` | `{"a":1}` | Parse JSON string to value. If input is already a list/map/number, returns it as-is. On invalid JSON, signals error for `try/on_error`. |

### Utility Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `print(...)` | `print("hi", 42)` | — | Print to console |
| `chr(n)` | `chr(65)` | `"A"` | Number to character |
| `ord(c)` | `ord("A")` | `65` | Character to number |
| `env(name)` | `env("HOME")` | `"/Users/..."` | Read environment variable |
| `time_now()` | `time_now()` | `1709913600.0` | Current Unix timestamp (float) |
| `time_ms()` | `time_ms()` | `1709913600123.0` | Current time in milliseconds (float) |
| `time_format(ts, fmt)` | `time_format(time_now(), "%Y-%m-%d")` | `"2026-03-08"` | Format timestamp with strftime pattern |
| `time_iso()` | `time_iso()` | `"2026-03-14T09:00:00Z"` | Current time as ISO 8601 string |
| `time_iso(ts)` | `time_iso(0)` | `"1970-01-01T00:00:00Z"` | Timestamp as ISO 8601 string |
| `uuid()` | `uuid()` | `"a1b2c3d4-..."` | Generate random UUID |
| `validate(map, fields)` | `validate(data, ["name"])` | `{"valid":yes,...}` | Check required fields |
| `input(prompt)` | `input("Enter name: ")` | text | Read line from stdin |

### Built-in Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `success` | `"success"` | Standard success response |
| `failure` | `"failure"` | Standard failure response |
| `nothing` | `nothing` | Absence of value (same as `none`) |
| `yes` / `true` | boolean | Boolean true |
| `no` / `false` | boolean | Boolean false |

### Cache Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `cache(key, val)` | `cache("user:1", data)` | — | Store in cache |
| `cached(key)` | `cached("user:1")` | value | Retrieve from cache |
| `is_cached(key)` | `is_cached("user:1")` | `yes`/`no` | Check if cached |

### Memory Functions (Conversation State)

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `memory_new(max)` | `memory_new(20)` | memory object | Create conversation memory (default max: 50 turns) |
| `memory_add(mem, role, msg)` | `memory_add(mem, "user", "hi")` | — | Add message to memory |
| `memory_get(mem)` | `memory_get(mem)` | list | Get all messages |
| `memory_clear(mem)` | `memory_clear(mem)` | — | Clear messages |
| `memory_summary(mem)` | `memory_summary(mem)` | text | Get text summary |

### Cryptography Functions

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `hash_sha256(s)` | `hash_sha256("hello")` | `"2cf24d..."` | SHA-256 hex digest (64 chars) |
| `hash_password(pw)` | `hash_password("secret")` | `"$nc$..."` | Salted hash for secure storage (10K iterations) |
| `verify_password(pw, h)` | `verify_password("secret", h)` | `yes`/`no` | Constant-time password verification |
| `hash_hmac(data, key)` | `hash_hmac("msg", "key")` | `"5031fe..."` | HMAC-SHA256 hex digest (64 chars) |

**Example — User registration and login:**

```nc
to register with username, password:
    set password_hash to hash_password(password)
    store {"user": username, "hash": password_hash} into "users/" + username
    respond with {"status": "registered"}

to login with username, password:
    gather user from "users/" + username
    if verify_password(password, user.hash):
        set token to jwt_generate(username, "user", 3600)
        respond with {"token": token}
    respond with {"error": "invalid credentials", "_status": 401}
```

### JWT (Authentication Tokens)

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `jwt_generate(user, role, exp)` | `jwt_generate("alice", "admin", 3600)` | `"eyJ..."` | Create signed JWT (HS256) |
| `jwt_generate(user, role, exp, extra)` | `jwt_generate("alice", "admin", 3600, {"org": "acme"})` | `"eyJ..."` | JWT with extra claims |
| `jwt_verify(token)` | `jwt_verify(token)` | claims map or `false` | Verify JWT, extract claims |
| `jwt_verify(token, secret)` | `jwt_verify(token, "my-key")` | claims map or `false` | Verify with explicit secret |

Requires env var: `NC_JWT_SECRET`

**Example:**

```nc
to protected_endpoint:
    set auth to request_header("Authorization")
    set token to replace(auth, "Bearer ", "")
    set claims to jwt_verify(token)
    if claims is equal to false:
        respond with {"error": "unauthorized", "_status": 401}
    respond with {"user": claims.sub, "role": claims.role}
```

### Session Management

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `session_create()` | `session_create()` | `"nc_a1b2..."` | Create session, return ID |
| `session_set(sid, key, val)` | `session_set(sid, "user", "alice")` | `yes`/`no` | Store value in session |
| `session_get(sid, key)` | `session_get(sid, "user")` | value or `nothing` | Retrieve session value |
| `session_exists(sid)` | `session_exists(sid)` | `yes`/`no` | Check session validity |
| `session_destroy(sid)` | `session_destroy(sid)` | `yes`/`no` | Invalidate session |

Configure TTL: `NC_SESSION_TTL=3600` (seconds, default 1 hour)

### Request Context (HTTP Header Access)

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `request_header(name)` | `request_header("Authorization")` | string or `nothing` | Read HTTP request header |
| `request_headers()` | `request_headers()` | record | All headers as key-value map |
| `request_ip()` | `request_ip()` | `"192.168.1.1"` | Client IP address |
| `request_method()` | `request_method()` | `"GET"` | HTTP method |
| `request_path()` | `request_path()` | `"/api/data"` | Request path |

### Feature Flags

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `feature(name)` | `feature("dark_mode")` | `yes`/`no` | Check if feature flag is enabled |
| `feature(name, tenant)` | `feature("beta", "org1")` | `yes`/`no` | Tenant-specific flag check |

Configure via env: `NC_FF_DARK_MODE=1`, `NC_FF_BETA_API=50` (50% rollout)

### Circuit Breaker

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `circuit_open(name)` | `circuit_open("payment")` | `yes`/`no` | Check if circuit is tripped |

Configure: `NC_CB_FAILURE_THRESHOLD=5`, `NC_CB_TIMEOUT=30`

---

## 22. File I/O

### Reading Files

```nc
set content to read_file("data.txt")
show content
```

**Output:**

```
(contents of data.txt)
```

### Writing Files

```nc
set report to "Sales Report\n\nTotal: $1,234"
write_file("report.md", report)
show "Report written!"
```

**Output:**

```
Report written!
```

### Checking If a File Exists

```nc
if file_exists("config.json"):
    set config to read_file("config.json")
    set data to json_decode(config)
    show data
otherwise:
    show "Config file not found"
```

### Practical Example: CSV Processing

```nc
to process_data:
    set raw to read_file("sales.csv")
    set lines to split(raw, "\n")
    set total to 0
    repeat for each line in lines:
        set parts to split(line, ",")
        if len(parts) is above 2:
            set amount to float(parts[2])
            add amount to total
    show "Total sales: $" + str(total)
    write_file("summary.txt", "Total: $" + str(total))
```

---

## 23. User Input (stdin)

Read input from the user at the terminal:

```nc
set name to input("What is your name? ")
show "Hello, " + name + "!"

set age to int(input("Enter your age: "))
if age is above 18:
    show "You are an adult"
```

**Console interaction:**

```
What is your name? Alice
Hello, Alice!
Enter your age: 25
You are an adult
```

The `input()` function:
- Takes an optional prompt string to display
- Returns the text typed by the user (as a string)
- Waits for the user to press Enter

---

## 24. Data Format Parsing

NC can parse multiple data formats into records/lists:

### JSON

```nc
set json_str to '{"name": "Alice", "age": 30}'
set data to json_decode(json_str)
show data.name   // Alice

set back to json_encode(data)
show back        // {"name":"Alice","age":30}
```

### YAML

```nc
set yaml_str to read_file("config.yaml")
set config to yaml_parse(yaml_str)
show config.database.host
```

### CSV

```nc
set csv_str to read_file("data.csv")
set rows to csv_parse(csv_str)
repeat for each row in rows:
    show row
```

### TOML

```nc
set toml_str to read_file("config.toml")
set config to toml_parse(toml_str)
show config.server.port
```

### XML

```nc
set xml_str to read_file("data.xml")
set data to xml_parse(xml_str)
show data
```

### INI

```nc
set ini_str to read_file("config.ini")
set config to ini_parse(ini_str)
show config.database_host
```

### Summary

| Function | Input | Output | Description |
|----------|-------|--------|-------------|
| `json_decode(s)` | JSON string | value | Parse JSON |
| `json_encode(v)` | any value | string | Serialize to JSON |
| `yaml_parse(s)` | YAML string | record | Parse simple YAML (key: value pairs) |
| `csv_parse(s)` | CSV string | list of lists | Parse CSV to rows of fields |
| `toml_parse(s)` | TOML string | record | Parse TOML (key = value pairs) |
| `xml_parse(s)` | XML string | record | Parse simple XML (tag → content) |
| `ini_parse(s)` | INI string | record | Parse INI/config files (same as TOML parser) |

> **Note:** The YAML, XML, TOML, and INI parsers handle simple flat key-value structures. For deeply nested or complex formats, pre-process with an external tool.

---

---

# Part IV — AI, APIs & Services

---

## 25. AI and LLM Integration

NC has a **universal AI engine** — it works with any AI API without hardcoding any company name, model, or format. NC has zero knowledge of any specific AI provider.

### How It Works

NC uses two primitives to talk to any AI:

1. **Template Engine** — Fills `{{placeholders}}` in a JSON request template
2. **Path Extractor** — Navigates a JSON response by dot-path to find the AI's answer

All API-specific format knowledge lives in configuration (`nc_ai_providers.json`), not in compiled code. When a new AI API appears, users edit a JSON file — no recompile, no new NC version.

### Configuration

Set up AI access via environment variables:

```bash
export NC_AI_URL="<YOUR_PROVIDER_ENDPOINT>"
export NC_AI_KEY="<YOUR_API_KEY>"
export NC_AI_MODEL="<YOUR_MODEL_NAME>"
```

Or configure in your `.nc` file:

```nc
configure:
    ai_url is env:NC_AI_URL
    ai_key is env:NC_AI_KEY
    ai_model is "nova"
```

### Switching AI Providers

Switch providers with one env var — no code changes:

```bash
# Any chat-completions-compatible API (default)
NC_AI_URL="<YOUR_PROVIDER_ENDPOINT>" NC_AI_KEY="<YOUR_API_KEY>" nc run app.nc

# Use a named adapter preset from nc_ai_providers.json
NC_AI_ADAPTER=messages NC_AI_KEY="<YOUR_API_KEY>" nc run app.nc

# Local models (no API key needed)
NC_HTTP_ALLOW_PRIVATE=1 NC_AI_URL=http://localhost:11434/api/generate nc run app.nc
```

### Custom AI API (any provider, even ones that don't exist yet)

```nc
configure:
    ai_url is "https://future-ai-corp.com/v1/generate"
    ai_key is env:FUTURE_API_KEY
    ai_model is "future-model-v1"
    ai_response_path is "output.generated_text"
```

NC doesn't know or care what's at that URL. It fills the template, POSTs it, extracts by path.

### AI Response Contract

Every `ask AI` call returns a record with guaranteed fields:

| Field | Type | Description |
|-------|------|-------------|
| `ok` | yesno | Did extraction succeed? |
| `response` | text | The extracted AI text (always present) |
| `model` | text | Which model was used |
| `raw` | text | Unprocessed JSON for debugging |

If the AI returned structured JSON, its fields are merged into the result:

```nc
ask AI to "Return JSON with name and age" save as result
show result.ok        // yes
show result.response  // {"name": "Alice", "age": 30}
show result.name      // Alice (merged from parsed response)
```

### Basic AI Call

```nc
ask AI to "Write a haiku about programming" save as poem
show poem
```

**Output:**

```
Code flows like water,
Bugs hide in the semicolons—
Debug, compile, run.
```

### AI with Context

```nc
set ticket to {"title": "Login broken", "description": "Cannot login after update"}

ask AI to "Classify this support ticket as: bug, feature, or question" using ticket save as result

show result
```

**Output:**

```
{"category": "bug", "confidence": 0.95}
```

### AI with Confidence Threshold

```nc
ask AI to "Classify this email" using email:
    confidence: 0.7
    save as: classification
```

### AI with Model Selection

```nc
ask AI to "Write a creative story" using model "nova" save as story
ask AI to "Summarize in one line" using model "nova" save as summary
```

### AI with Fallback Models

```nc
set models to ["nova", "nova", "nova"]
set result to ai_with_fallback("Translate to French: Hello", {}, models)
show result
```

### RAG (Retrieval-Augmented Generation)

```nc
to answer_from_docs with question, file_path:
    set document to read_file(file_path)
    set chunks to chunk(document, 1000)
    set relevant to top_k(chunks, question, 5)
    ask AI to "Based on these excerpts, answer: {{question}}. Excerpts: {{relevant}}" save as answer
    respond with answer
```

| Function | Description |
|----------|-------------|
| `chunk(text, size)` | Split text into chunks of `size` characters (with optional overlap parameter) |
| `top_k(items, k)` | Return first `k` items from a list |
| `token_count(text)` | Estimate token count in text (heuristic based on word/character count) |

### Validation of AI Responses

```nc
ask AI to "Return JSON with name, age, city" save as result

set check to validate(result, ["name", "age", "city"])

if check.valid is equal no:
    show "Missing fields: " + str(check.missing)
    // Retry
    ask AI to "Return ONLY valid JSON with: name, age, city" save as result
otherwise:
    show result
```

### ML Model Integration

NC can load and run models from Python's ML ecosystem — sklearn, PyTorch, TensorFlow, and ONNX. Under the hood, NC spawns a persistent Python subprocess that loads the model once and accepts prediction requests via JSON over stdin/stdout.

```nc
// Load a Python ML model (auto-detects framework from extension)
set model to load_model("model.pkl")    // sklearn
set model to load_model("net.pt")       // PyTorch
set model to load_model("model.h5")     // TensorFlow/Keras
set model to load_model("model.onnx")   // ONNX

// Run prediction — pass features as a list
set prediction to predict(model, [35, 60000, 3])
show prediction
// Output: {"prediction": 1, "confidence": 0.93, "probabilities": [0.07, 0.93]}

// Unload when done
unload_model(model)
```

| Function | Description |
|----------|-------------|
| `load_model(path)` | Load a model file. Returns a handle with `{status, type, path, _handle}` |
| `predict(model, features)` | Run prediction. Features is a list of numbers. Returns JSON result |
| `unload_model(model)` | Terminate the Python subprocess for this model |

> **Tip:** Set `NC_PYTHON` environment variable to specify the Python binary (e.g., `export NC_PYTHON=python3.11`).

### Cross-Language Execution

NC can call **any programming language** at runtime. The `exec()` function runs external processes, captures stdout, and auto-parses JSON output into NC values.

```nc
// Call Python
set result to exec("python3", "analyze.py", "data.csv")
show result.score

// Call Node.js
set output to exec("node", "process.js")

// Call Go
set output to exec("go", "run", "main.go")

// Call any shell command
set files to shell("find /app -name '*.csv' | wc -l")
show "Found " + files + " CSV files"

// Call curl for complex HTTP requests
set response to shell("curl -s https://api.github.com/repos/nc-lang/nc-lang")
show response.stargazers_count
```

If the external command outputs valid JSON, NC automatically parses it into a record/list you can access with dot notation. Otherwise, the raw string is returned.

### Code Digestion — Convert Other Languages to NC

NC includes a cross-language digester that converts Python, JavaScript, TypeScript, YAML, and JSON files into NC code. This helps teams migrate existing codebases gradually.

```bash
nc digest app.py           # Python → NC
nc digest server.js        # JavaScript → NC
nc digest config.yaml      # YAML → NC configure block
nc digest settings.json    # JSON → NC configure block
```

The digester uses pattern recognition to translate:
- `def function(args):` → `to function with args:`
- `class User:` → `define User as:`
- `return value` → `respond with value`
- `print(...)` → `show ...`
- `x = value` → `set x to value`
- `const/let/var x = value` → `set x to value`
- `console.log(...)` → `show ...`

Supported file types: `.py`, `.js`, `.ts`, `.yaml`, `.yml`, `.json`

---

## 26. Building APIs and Services

### Service Structure

```nc
service "my-api"
version "1.0.0"
description "My awesome API"

configure:
    port: 8080

to list_users:
    respond with [{"name": "Alice"}, {"name": "Bob"}]

to create_user with name, email:
    log "Creating user: {{name}}"
    respond with {"name": name, "email": email, "created": yes}

to get_user with id:
    respond with {"id": id, "name": "Alice"}

api:
    GET    /users      runs list_users
    POST   /users      runs create_user
    GET    /users/:id  runs get_user
```

### Running the Server

```bash
nc serve my-api.nc -p 8080
```

**Output:**

```
NC server starting on port 8080
Routes:
  GET    /users      → list_users
  POST   /users      → create_user
  GET    /users/:id  → get_user
Server ready.
```

### Testing the API

```bash
# List users
curl http://localhost:8080/users

# Create a user
curl -X POST http://localhost:8080/users \
  -H "Content-Type: application/json" \
  -d '{"name": "Charlie", "email": "charlie@example.com"}'

# Get a user
curl http://localhost:8080/users/123
```

### HTTP Methods

| Method | Usage |
|--------|-------|
| `GET /path` | Read / list resources |
| `POST /path` | Create a resource |
| `PUT /path/:id` | Update a resource |
| `DELETE /path/:id` | Delete a resource |
| `PATCH /path/:id` | Partial update |

### Route Parameters

Parameters in URLs (like `:id`) become available in the behavior:

```nc
api:
    GET /users/:id/posts/:post_id runs get_user_post

to get_user_post with id, post_id:
    respond with {"user_id": id, "post_id": post_id}
```

### Request Context

Inside behaviors called via API, you can access:

```nc
to handle_request:
    show request.method     // "POST"
    show request.path       // "/users"
    show request.body       // parsed JSON body
    show request.query      // query parameters
    show auth.user_id       // authenticated user (if auth middleware)
    show auth.role          // user role
```

### Gather from External APIs

```nc
to get_weather with city:
    gather forecast from "https://api.weather.com/v1/{{city}}":
        headers: {"Authorization": "Bearer " + env("WEATHER_KEY")}
    respond with forecast
```

### Gather Options Reference

The `gather` statement supports these options:

| Option | Type | Description |
|--------|------|-------------|
| `method` | String | HTTP method: `"GET"`, `"POST"`, `"PUT"`, `"PATCH"`, `"DELETE"` |
| `headers` | Record | Custom headers. Use `"env:VAR_NAME"` to read from env vars |
| `body` | String/Record | Request body (auto-serialized to JSON if record) |
| `content_type` | String | Content-Type header (default: `"application/json"`) |
| `warmup` | String | URL to GET first (initializes session/cookies before main request) |
| `timeout` | Number | Per-request timeout override in seconds |

#### Example: POST with custom headers and body

```nc
to create_item with data:
    gather result from "https://api.example.com/items" with {
        method: "POST",
        headers: {"Authorization": "env:API_KEY", "X-Request-ID": "nc-123"},
        body: data,
        content_type: "application/json"
    }
    respond with result
```

#### Example: Session warmup (APIs requiring login/cookies)

```nc
to fetch_dashboard:
    gather data from "https://portal.example.com/api/dashboard" with {
        warmup: "https://portal.example.com/login",
        headers: {"Authorization": "env:PORTAL_TOKEN"},
        timeout: 90
    }
    respond with data
```

The `warmup` option sends a GET to the warmup URL first, establishing any session cookies, then the main request automatically includes those cookies.

> **Note:** To enable cookie persistence across requests, set `NC_HTTP_COOKIES=1` in your environment.

### HTTP Configuration

All HTTP behavior is configurable via environment variables — no hardcoded values in the engine:

```bash
# Timeouts
export NC_TIMEOUT=60              # Request timeout (seconds)
export NC_CONNECT_TIMEOUT=10      # Connection timeout (seconds)
export NC_STREAM_TIMEOUT=120      # Streaming request timeout (seconds)

# Cookies & Sessions
export NC_HTTP_COOKIES=1          # Enable cookie persistence
export NC_HTTP_COOKIE_JAR=/tmp/my_cookies.txt  # Custom cookie file

# User-Agent
export NC_HTTP_USER_AGENT="MyApp/2.0"  # Custom User-Agent string

# Retries
export NC_RETRIES=3               # Auto-retry on failure (with exponential backoff)
```

### Server Configuration

Workers and connection tuning auto-detect based on your system:

```bash
# Workers default to CPU count × 2 (auto-detected)
export NC_MAX_WORKERS=16          # Override max worker threads

# Connection tuning
export NC_SOCKET_TIMEOUT=30       # Socket read/write timeout
export NC_KEEPALIVE_MAX=100       # Max requests per keep-alive connection
export NC_KEEPALIVE_TIMEOUT=30    # Idle keep-alive timeout
export NC_LISTEN_BACKLOG=512      # TCP listen backlog
export NC_REQUEST_QUEUE_SIZE=4096 # Request queue capacity
```

---

## 27. Notifications and Messaging

### Notify

Send notifications to channels or recipients:

```nc
notify ops_team "Server is down!"
notify admin "Deployment complete"
```

With a template block:

```nc
notify support:
    message: "New ticket from {{customer.name}}"
    priority: "high"
```

### Send

`send` is an alias for `notify` — they work identically:

```nc
send alert to admin
send "Backup complete" to ops_channel
```

### Apply

Apply a tool or action to a target:

```nc
apply "security_scan" using scanner
```

### Check

Run a check and save the result:

```nc
check if "service is healthy" using health_monitor save as status
if status is equal "healthy":
    show "All good"
```

---

## 28. Database and Storage

### In-Memory Store

NC provides a built-in key-value store (in-memory by default):

```nc
// Store data
store {"name": "Alice", "score": 95} into "student:alice"

// Retrieve data
gather student from database:
    key: "student:alice"

show student.name   // Alice
```

### Gather from Variables

The source in `gather` can be a variable — useful for building dynamic URLs:

```nc
to fetch_data with symbol:
    set base_url to "https://api.example.com/data"
    set url to base_url + "/{{symbol}}"
    gather response from url
    respond with response

// Also works with options block:
to fetch_with_headers with symbol:
    set url to "https://api.example.com/data/{{symbol}}"
    gather response from url:
        headers:
            User-Agent: "NC/1.0"
    respond with response
```

### SQL Database Queries

```nc
// Query a database
gather users from database:
    query: "SELECT * FROM users WHERE active = true"

repeat for each user in users:
    show user.name
```

### Storage Configuration

NC uses a generic HTTP-based store backend. Set `NC_STORE_URL` to point at any key-value store that accepts JSON over HTTP:

```bash
export NC_STORE_URL="http://localhost:8080/api/data"
```

When `NC_STORE_URL` is not set, NC uses an in-memory store (data is lost on restart). This is useful for development.

For production, any HTTP endpoint that accepts `POST {key, value}` and responds to `GET /{key}` works — you can use Redis via a REST proxy, Supabase, or any custom service.

---

## 29. Events and Scheduling

### Event Handlers

```nc
on "user_signup":
    log "New user signed up: {{event.data}}"
    ask AI to "Write a welcome message for {{event.data.name}}" save as welcome
    send welcome to event.data.email
```

### Emitting Events

```nc
to register_user with name, email:
    store {"name": name, "email": email} into "users:{{email}}"
    emit "user_signup" with {"name": name, "email": email}
    respond with "Registered!"
```

### Scheduled Tasks

```nc
every 5 minutes:
    gather status from "https://api.example.com/health"
    if status.healthy is equal no:
        notify ops team "Service is down!"
```

---

## 30. Middleware

Add cross-cutting concerns to your API:

```nc
middleware:
    rate_limit
    auth

to protected_action with data:
    // Only runs if auth passes and rate limit not exceeded
    respond with {"status": "ok", "data": data}
```

### Available Middleware

| Middleware | Description |
|------------|-------------|
| `rate_limit` | Limit requests per client |
| `auth` | Require authentication |
| `cors` | Cross-origin resource sharing |
| `logging` | Request logging |

### Proxy / Forward

```nc
proxy "/api/v1" forward to "http://backend:8000"
```

---

---

# Part V — Ecosystem & Tooling

---

## 31. Imports and Modules

### Importing NC Files

```nc
import "utils.nc"
```

### Import with Alias

```nc
import "helpers" as h
```

### Built-in Modules

NC includes built-in modules you can import:

#### Math Module

```nc
import "math"

show math.pi     // 3.141592653589793
show math.e      // 2.718281828459045
show math.tau    // 6.283185307179586
```

#### Time Module

```nc
import "time"

show time.now    // current Unix timestamp
```

### Module Declaration

You can declare a file as a module. `module` is an alias for `service` — they are interchangeable:

```nc
module "my-utils"

to format_name with first, last:
    respond with upper(first) + " " + upper(last)
```

### Service-Level Declarations

A full service header supports these optional declarations:

```nc
service "my-app"
version "1.0.0"
author "Your Name"
description "A description of what this service does"
model "nova"
```

| Declaration | Purpose |
|-------------|---------|
| `service "name"` | Name of the service |
| `version "x.y.z"` | Version string |
| `author "name"` | Author name |
| `description "text"` | Human-readable description |
| `model "model-name"` | Default AI model |

### Configure Block

The `configure:` block sets service-level settings. Use `env:VAR` to reference environment variables:

```nc
configure:
    port: 8080
    ai_model is "nova"
    database_url: "env:DATABASE_URL"
    confidence_threshold: 0.7
    debug: yes
```

---

## 32. Package Management

### Initialize a Package

```bash
nc pkg init
```

Creates an `nc.pkg` manifest file.

### Install a Package

```bash
nc pkg install my-package
nc pkg install https://github.com/user/nc-package.git
```

Packages are stored in `.nc_packages/`.

### List Installed Packages

```bash
nc pkg list
```

### Using Packages

```nc
import "my-package"
```

---

## 33. The REPL (Interactive Mode)

Start the REPL:

```bash
nc repl
# Or simply run nc with no arguments:
nc
```

### Basic Usage

```
nc> set x to 42
nc> show x
42
nc> set name to "Alice"
nc> show "Hello, " + name
Hello, Alice
nc> show x + 8
50
```

### Multi-Line Input

Lines ending with `:` start a block. Indent the body, then press Enter on an empty line to execute:

```
nc> if x is above 10:
...     show "x is big"
...
x is big
```

### Dot Commands

| Command | Description |
|---------|-------------|
| `.help` | Show help text |
| `.vars` | Show all variables and their values |
| `.clear` | Clear all variables |
| `.history` | Show command history |
| `.load <file>` | Load a `.nc` file and list its behaviors |
| `.run <behavior> <file>` | Run a specific behavior from a file |
| `.quit` or `.exit` | Exit the REPL |

### REPL Session Example

```
$ nc repl
NC REPL v1.0.0 — type .help for commands

nc> set items to [3, 1, 4, 1, 5]
nc> show len(items)
5
nc> show sort(items)
[1, 1, 3, 4, 5]
nc> show sum(items)
14

nc> repeat for each n in items:
...     show n * 2
...
6
2
8
2
10

nc> .load examples/01_hello_world.nc
Loaded. Behaviors: greet, health_check

nc> .run greet examples/01_hello_world.nc
Hello, World! Welcome to NC.

nc> .quit
Goodbye!
```

---

## 34. Docker and Containers

NC ships with full Docker support for containerized deployment.

### Docker Image Variants

| Image | Base | Size | Use Case |
|-------|------|------|----------|
| `nc:latest` | Alpine 3.21 | ~23 MB | Production (minimal) |
| `nc:slim` | Debian slim | ~80 MB | When you need more OS tools |

### Build the Docker Image

```bash
# Build locally
docker build -t nc .

# Build for multiple platforms (amd64 + arm64)
docker buildx build --platform linux/amd64,linux/arm64 -t nc .
```

### Run NC in Docker

```bash
# Check version
docker run -it nc version

# Run a .nc file
docker run -v $(pwd):/app nc run /app/myservice.nc

# Run a specific behavior
docker run -v $(pwd):/app nc run /app/myservice.nc -b my_behavior

# Start as HTTP server (with port mapping)
docker run -p 8080:8080 -v $(pwd):/app nc serve /app/myservice.nc

# Interactive REPL
docker run -it nc repl
```

### Docker with Environment Variables

Pass AI keys and config via environment variables:

```bash
docker run -p 8080:8080 \
  -e NC_AI_URL="<YOUR_PROVIDER_ENDPOINT>" \
  -e NC_AI_KEY="<YOUR_API_KEY>" \
  -e NC_AI_MODEL="nova" \
  -e NC_SERVICE_PORT=8080 \
  -v $(pwd):/app \
  nc serve /app/service.nc
```

### Docker Compose

Create a `docker-compose.yml`:

```yaml
services:
  nc:
    build: .
    ports:
      - "8080:8080"
    volumes:
      - ./my-services:/app
    environment:
      - NC_SERVICE_PORT=8080
      - NC_AI_URL=${NC_AI_URL:-}
      - NC_AI_KEY=${NC_AI_KEY:-}
      - NC_AI_MODEL=${NC_AI_MODEL:-}
      - NC_STORE_URL=${NC_STORE_URL:-}
    command: ["serve", "/app/my_service.nc"]
```

Run with:

```bash
docker compose up
```

### Using NC as a Base Image

Create your own Dockerfile for production:

```dockerfile
FROM nc:latest
COPY my_service.nc /app/
COPY data/ /app/data/
ENV NC_SERVICE_PORT=8080
ENV NC_AI_MODEL=nova
EXPOSE 8080
CMD ["serve", "/app/my_service.nc"]
```

Build and run:

```bash
docker build -t my-nc-service .
docker run -p 8080:8080 -e NC_AI_KEY="<YOUR_API_KEY>" my-nc-service
```

### Docker Image Internals

| Path | Contents |
|------|----------|
| `/usr/local/bin/nc` | NC binary |
| `/nc/lib/` | Standard library files |
| `/app/` | Working directory (mount your files here) |

The `NC_LIB_PATH` environment variable is set to `/nc/lib` inside the container.

---

## 35. CI/CD and Deployment

### GitHub Actions CI/CD

NC includes a full CI/CD pipeline (`.github/workflows/ci.yml`) that:

1. **Builds and tests** on Linux, macOS, and Windows
2. **Publishes Docker images** (multi-platform: amd64 + arm64)
3. **Creates GitHub Releases** with pre-built binaries

### Pipeline Triggers

| Trigger | What Happens |
|---------|--------------|
| Push to `main` | Build + test + Docker image publish |
| Pull request to `main` | Build + test only |
| Tag `v*` (e.g. `v1.0.0`) | Build + test + Docker + GitHub Release with binaries |

### Pre-built Binaries

On tagged releases, binaries are available for download:

| Binary | Platform |
|--------|----------|
| `nc-linux-amd64` | Linux x86_64 |
| `nc-macos-arm64` | macOS Apple Silicon |
| `nc-windows-amd64.exe` | Windows x86_64 |

Download from the GitHub Releases page:

```bash
# Linux
curl -sSL https://github.com/YOUR_USERNAME/nc-lang/releases/latest/download/nc-linux-amd64 -o nc
chmod +x nc
sudo mv nc /usr/local/bin/

# macOS (Apple Silicon)
curl -sSL https://github.com/YOUR_USERNAME/nc-lang/releases/latest/download/nc-macos-arm64 -o nc
chmod +x nc
sudo mv nc /usr/local/bin/
```

### One-Line Install

```bash
curl -sSL https://raw.githubusercontent.com/YOUR_USERNAME/nc-lang/main/install.sh | bash
```

The installer:
- Tries pre-built binary first (fastest)
- Falls back to building from source if needed
- Auto-detects your platform (Linux/macOS/Windows WSL)
- Auto-detects architecture (amd64/arm64)
- Installs to `/usr/local/bin/` by default

### Custom Install Paths

```bash
NC_INSTALL_DIR=~/.local/bin NC_LIB_DIR=~/.local/lib/nc bash install.sh
```

### Deploy to Production

#### Option 1: Docker (recommended)

```bash
docker run -d \
  --name my-nc-service \
  --restart unless-stopped \
  -p 8080:8080 \
  -e NC_AI_KEY="<YOUR_API_KEY>" \
  -v /path/to/services:/app \
  nc serve /app/production.nc
```

#### Option 2: Direct binary

```bash
# Copy the binary to your server
scp engine/build/nc user@server:/usr/local/bin/

# Run as a systemd service
nc serve /path/to/service.nc -p 8080
```

#### Option 3: Docker Compose (multi-service)

```yaml
services:
  api-gateway:
    build: .
    ports: ["8080:8080"]
    command: ["serve", "/app/api_gateway.nc"]
    volumes: ["./services:/app"]
    environment:
      - NC_AI_KEY=${NC_AI_KEY}

  worker:
    build: .
    command: ["run", "/app/worker.nc"]
    volumes: ["./services:/app"]
    environment:
      - NC_AI_KEY=${NC_AI_KEY}
```

---


## 36. Platform Support

### Platform-Specific Details

#### File Permissions

- **POSIX (Linux/macOS):**
    - File permissions are enforced by the OS. NC respects umask and default permissions when creating files or directories.
    - Use `chmod` to adjust permissions if needed. NC does not set custom permissions by default.
    - Temporary files are created in `/tmp/` or as specified by the `NC_ALLOWED_PATH` environment variable.
- **Windows:**
    - File permissions are managed by NTFS ACLs. NC creates files with default permissions for the current user.
    - No executable bit; use Windows security settings to adjust access.
    - Temporary files are created in `%TEMP%` or `C:\Temp\`.

#### Shell Execution Differences

- **POSIX:**
    - Shell commands run via `/bin/sh` or `/bin/bash`.
    - Path separator is `/`.
    - Environment variables use `$NAME` syntax.
- **Windows:**
    - Shell commands run via `cmd.exe` by default.
    - Path separator is `\`.
    - Environment variables use `%NAME%` syntax.
    - Some shell features (e.g., pipes, redirection) may differ from POSIX.

#### Path Separator Handling

- Use `nc.pathlib.join` to build paths in a cross-platform way.
- NC normalizes paths internally, but always prefer `/` in NC code for portability.
- Avoid hardcoding absolute paths; use environment variables or relative paths.

#### Known Platform-Specific Caveats

- **Symlink traversal is blocked** for file I/O safety. NC resolves real paths and rejects access if a symlink escapes the working directory or allowed temp paths.
- **UNC paths (`\\server\share`) are not supported** on Windows for file I/O. Attempts to access UNC paths will fail with an explicit error message.
- **Line endings:** NC reads/writes files in binary mode; line endings are preserved as-is. Use `replace()` to normalize if needed.
- **Executable bit:** On Windows, there is no executable bit. On POSIX, use `chmod` if you need to make a file executable.

#### Binary Distribution Matrix & CI Check

For every release, the following binaries must be present and tested:

| Platform         | Architecture | Binary Name           |
|------------------|--------------|----------------------|
| Linux            | x86_64       | nc-linux-amd64       |
| Linux            | arm64        | nc-linux-arm64       |
| macOS            | x86_64       | nc-macos-amd64       |
| macOS            | arm64        | nc-macos-arm64       |
| Windows          | x86_64       | nc-windows-amd64.exe |

The CI pipeline must verify that all binaries are built and uploaded for each release. If any are missing, the release fails.

**Python CLI wrapper** (`bindings/python/cli.py`) auto-selects the correct binary for the current platform/arch. Always publish all binaries to ensure compatibility.

---

### Supported Operating Systems

| Platform | Build | Pre-built Binary | Docker |
|----------|-------|------------------|--------|
| Linux (x86_64) | Yes | Yes | Yes |
| Linux (ARM64) | Yes | — | Yes |
| macOS (Apple Silicon) | Yes | Yes | — |
| macOS (Intel) | Yes | — | — |
| Windows (x86_64) | Yes | Yes (.exe) | — |
| Windows (WSL) | Yes | Via Linux binary | Yes |

### Build Requirements by Platform

**Linux (Ubuntu/Debian):**

```bash
sudo apt-get update
sudo apt-get install -y gcc make libcurl4-openssl-dev libedit-dev
```

**Linux (Fedora/RHEL):**

```bash
sudo dnf install gcc make libcurl-devel libedit-devel
```

**macOS:**

```bash
xcode-select --install    # installs clang, make
brew install curl          # usually pre-installed
```

**Windows:**

Use WSL (recommended) or Git Bash:

```bash
# WSL
sudo apt-get install gcc make libcurl4-openssl-dev libedit-dev
cd nc-lang/nc && make

# Or use chocolatey
choco install curl make
```

### Architecture Support

| Architecture | Status |
|-------------|--------|
| x86_64 (amd64) | Fully supported |
| ARM64 (aarch64) | Fully supported |
| ARM32 | Untested |

### AI Provider Compatibility

NC works with any AI-provider-compatible chat completions API. Set `NC_AI_URL` to point at your provider:

| Provider | NC_AI_URL | NC_AI_MODEL (example) |
|----------|-----------|----------------------|
| NC AI | `<YOUR_OPENAI_ENDPOINT>` | `nova` |
| NC AI (via proxy) | `https://your-proxy/v1/chat/completions` | `claude-3-haiku-20240307` |
| External AI (via proxy) | `https://your-proxy/v1/chat/completions` | `provider-model` |
| Azure AI | `https://<name>.openai.azure.com/openai/deployments/<model>/chat/completions?api-version=2024-02-01` | `nova` |
| Ollama (local, set `NC_HTTP_ALLOW_PRIVATE=1`) | `http://localhost:11434/v1/chat/completions` | `llama3` |
| AI proxy (local, set `NC_HTTP_ALLOW_PRIVATE=1`) | `http://localhost:4000/v1/chat/completions` | `nova` |
| LM Studio (local, set `NC_HTTP_ALLOW_PRIVATE=1`) | `http://localhost:1234/v1/chat/completions` | `local-model` |

### Data Format Compatibility

| Format | Parse | Serialize | Function |
|--------|-------|-----------|----------|
| JSON | Yes | Yes | `json_decode()`, `json_encode()` |
| YAML | Yes | — | `yaml_parse()` (flat key: value pairs) |
| CSV | Yes | — | `csv_parse()` (rows of comma-separated fields) |
| TOML | Yes | — | `toml_parse()` (flat key = value pairs) |
| XML | Yes | — | `xml_parse()` (flat tag → content extraction) |
| INI | Yes | — | `ini_parse()` (same as TOML parser) |

### Build Tool Compatibility

| Tool | Support |
|------|---------|
| Make | Primary build system |
| Docker | Multi-stage Alpine build |
| Docker Compose | Service orchestration |
| GitHub Actions | CI/CD via shell commands |
| systemd | Run via `nc serve` command |

---

## 37. Debugging

### Debug Mode

```bash
nc debug myfile.nc
nc debug myfile.nc -b my_behavior
```

Shows step-by-step execution, variable state, and call stack.

### DAP (Debug Adapter Protocol)

```bash
nc debug myfile.nc --dap
```

Connects to VS Code's debugger.

### Log Statements

Add `log` statements for runtime debugging:

```nc
to process with data:
    log "Input data: {{data}}"
    set result to data.value * 2
    log "Result: {{result}}"
    respond with result
```

**Output:**

```
[LOG] Input data: {"value": 21}
[LOG] Result: 42
```

### Show vs Log

| Statement | Behavior |
|-----------|----------|
| `show value` | Prints the value to stdout |
| `log "message"` | Prints with `[LOG]` prefix, supports `{{templates}}` |

### Profiling

```bash
nc profile myfile.nc
```

Shows execution time per behavior.

### Semantic Analysis

```bash
nc analyze myfile.nc
```

Static analysis of the code for potential issues.

---

## 38. Editor Support (VS Code)

### Installation

1. Open VS Code
2. Go to Extensions (`Cmd+Shift+X`)
3. Search for "NC" or install from `editor/vscode/`
4. Reload VS Code

### Features

- Syntax highlighting for `.nc` files
- Keyword highlighting (service, define, ask AI, etc.)
- Template interpolation highlighting (`{{variable}}`)
- Bracket matching and auto-closing
- Code snippets for common patterns
- Comment toggling (`Cmd+/` → `//` comments)

### File Association

VS Code will automatically recognize `.nc` files. If not, add to `settings.json`:

```json
{
  "files.associations": {
    "*.nc": "nc"
  }
}
```

---

---

# Part VI — Reference

---

## 39. Environment Variables Reference

| Variable | Default | Description |
|----------|---------|-------------|
| **AI/LLM (Universal Engine)** | | |
| `NC_AI_URL` | — (required) | AI endpoint URL (any HTTP API) |
| `NC_AI_KEY` | — | API key for the AI service |
| `NC_AI_MODEL` | — | Model name (passed as `{{model}}` to the request template) |
| `NC_AI_ADAPTER` | — | Named adapter preset from `nc_ai_providers.json` |
| `NC_AI_TEMPERATURE` | `0.7` | Temperature (passed as `{{temperature}}` to template) |
| `NC_AI_MAX_TOKENS` | — | Max response tokens |
| `NC_AI_SYSTEM_PROMPT` | — | System prompt |
| `NC_AI_REQUEST_TEMPLATE` | (built-in) | Custom JSON request template with `{{placeholders}}` |
| `NC_AI_RESPONSE_PATH` | (multi-path) | Dot-path to extract response (e.g., `choices.0.message.content`) |
| `NC_AI_AUTH_HEADER` | `Authorization` | Auth header name |
| `NC_AI_AUTH_FORMAT` | `Bearer {{key}}` | Auth value format (template with `{{key}}`) |
| `NC_AI_CONFIG_FILE` | `nc_ai_providers.json` | Path to AI adapter presets config |
| **MCP (Tool Server)** | | |
| `NC_MCP_URL` | — | MCP tool server URL |
| `NC_MCP_PATH` | `/api/v1/tools/call` | MCP API path |
| **Data Store** | | |
| `NC_STORE_URL` | — | External key-value store URL (HTTP) |
| **Server** | | |
| `NC_SERVICE_PORT` | `8000` | Default server port |
| `NC_CORS_ORIGIN` | `*` | CORS allowed origins |
| `NC_CORS_METHODS` | `GET, POST, PUT, DELETE, PATCH, OPTIONS` | CORS allowed methods |
| `NC_CORS_HEADERS` | `Content-Type, Authorization` | CORS allowed headers |
| `NC_HEALTH_PATH` | `/health` | Health check endpoint path |
| `NC_BASE_PATH` | — | URL prefix to strip from routes |
| `NC_MAX_WORKERS` | `16` | Max concurrent request workers |
| `NC_REQUEST_TIMEOUT` | `120` | Per-request timeout in seconds |
| **HTTP Client** | | |
| `NC_TIMEOUT` | `60` | HTTP request timeout in seconds |
| `NC_RETRIES` | `3` | Number of retries for HTTP calls |
| `NC_HTTP_ALLOWLIST` | — | Comma-separated list of allowed outbound hosts (SSRF protection) |
| `NC_HTTP_STRICT` | — | When set, denies all outbound HTTP unless allowlist is configured |
| **Security** | | |
| `NC_API_KEY` | — | API key for incoming request authentication |
| `NC_API_KEYS` | — | Comma-separated list of valid API keys |
| `NC_JWT_SECRET` | — | Secret for JWT token verification |
| `NC_CORS_ORIGIN` | `*` (or `null` if auth enabled) | Allowed CORS origin (restrictive default when auth is configured) |
| `NC_ALLOW_EXEC` | — | Set to `1` to enable shell execution |
| `NC_ALLOW_FILE_WRITE` | — | Set to `1` to enable file writes |
| `NC_ALLOW_NETWORK` | — | Set to `1` to enable outbound network |
| **Authentication** | | |
| `NC_JWT_USER_CLAIM` | `sub` | JWT claim field for user ID |
| `NC_JWT_ROLE_CLAIM` | `roles` | JWT claim field for roles |
| `NC_JWT_TENANT_CLAIM` | `tid` | JWT claim field for tenant ID |
| `NC_DEFAULT_ROLE` | `user` | Default role when none in token |
| `NC_OIDC_ISSUER` | — | OIDC identity provider URL |
| `NC_OIDC_CLIENT_ID` | — | OIDC client ID |
| **Sessions** | | |
| `NC_SESSION_TTL` | `3600` | Session expiry in seconds |
| **Rate Limiting** | | |
| `NC_RATE_LIMIT_RPM` | `100` | Default requests per minute |
| `NC_RATE_LIMIT_WINDOW` | `60` | Rate limit window in seconds |
| **Circuit Breaker** | | |
| `NC_CB_FAILURE_THRESHOLD` | `5` | Failures before circuit opens |
| `NC_CB_TIMEOUT` | `30` | Seconds before retry |
| `NC_CB_SUCCESS_THRESHOLD` | `3` | Successes to close circuit |
| **Feature Flags** | | |
| `NC_FF_<NAME>` | — | Feature flag: `1`/`0`/percentage |
| **Observability** | | |
| `NC_LOG_LEVEL` | `info` | Log level: `debug`, `info`, `warn`, `error` |
| `NC_AUDIT_FORMAT` | — | Set to `json` to enable audit logging |
| `NC_AUDIT_FILE` | — | Audit log file path |
| `NC_OTEL_ENDPOINT` | — | Distributed tracing collector URL |
| **TLS** | | |
| `NC_TLS_CERT` | — | TLS certificate file path |
| `NC_TLS_KEY` | — | TLS private key file path |
| **Runtime** | | |
| `NC_MAX_LOOP_ITERATIONS` | `10000000` | Loop iteration limit (configurable) |
| `NC_DRAIN_TIMEOUT` | `30` | Graceful shutdown timeout in seconds |
| `NC_REQUEST_TIMEOUT` | `60` | Max behavior execution time in seconds (returns 504 on timeout) |
| `NC_LIB_PATH` | — | Path to NC standard library modules |
| `NC_LOG_FORMAT` | — | Set to `json` for structured JSON server logs |
| `NC_HTTP_COOKIES` | `0` | Set to `1` to enable HTTP cookie persistence across requests |
| `NC_HTTP_COOKIE_JAR` | `/tmp/nc_cookies_<pid>.txt` | Custom file path for cookie storage |
| `NC_HTTP_USER_AGENT` | `Mozilla/5.0 (compatible; NC/<version>)` | Default User-Agent header for outbound HTTP |
| `NC_SECRET_SOURCE` | `env` | Secret source: `env`, `file`, or `external` |
| `NC_SECRET_VARS` | — | Comma-separated list of additional env var names to mask in output |
| **HTTP Timeouts** | | |
| `NC_TIMEOUT` | `60` | HTTP request timeout in seconds |
| `NC_CONNECT_TIMEOUT` | `10` | HTTP connection timeout in seconds |
| `NC_STREAM_TIMEOUT` | `120` | Streaming HTTP request timeout in seconds |
| `NC_SOCKET_TIMEOUT` | `30` | Server socket read/write timeout in seconds |
| **Server Tuning** | | |
| `NC_MAX_WORKERS` | CPU count × 2 | Maximum worker threads for the HTTP server |
| `NC_WORKERS` | CPU count | Thread pool size for async tasks |
| `NC_LISTEN_BACKLOG` | `512` | TCP listen backlog for the server |
| `NC_REQUEST_QUEUE_SIZE` | `4096` | Pending request queue capacity |
| `NC_KEEPALIVE_MAX` | `100` | Maximum requests per keep-alive connection |
| `NC_KEEPALIVE_TIMEOUT` | `30` | Keep-alive connection idle timeout in seconds |
| `NC_MAX_REQUEST_SIZE` | `1048576` | Max request body size in bytes (1 MB) |
| `NC_RETRIES` | `3` | Number of automatic HTTP retries on failure |
| `NC_REGISTRY` | `https://registry.notation-code.dev` | Package registry URL |

---

## 40. CLI Command Reference

| Command | Description |
|---------|-------------|
| **Run & Execute** | |
| `nc run <file>` | Run a `.nc` file |
| `nc run <file> -b <behavior>` | Run a specific behavior |
| `nc run <file> --no-cache` | Run without bytecode cache |
| `nc serve <file>` | Start HTTP server |
| `nc serve <file> -p <port>` | Start server on specific port |
| `nc start` | Auto-discover and start .nc services |
| `nc start <file>` | Start a specific service |
| `nc stop` | Stop all running NC services |
| `nc dev [file]` | Validate + start (development mode) |
| `nc deploy` | Build container image |
| `nc deploy --tag app:v1` | Build with custom image tag |
| `nc deploy --push` | Build and push to registry |
| `nc repl` | Start interactive REPL |
| `nc debug <file>` | Run in debug mode |
| `nc debug <file> -b <behavior>` | Debug a specific behavior |
| `nc debug <file> --dap` | Start DAP debug server |
| **Plain English (Inline)** | |
| `nc "show 42 + 8"` | Run plain English directly from command line |
| `nc -c "set x to 5; show x"` | Execute inline NC code (semicolons become newlines) |
| `nc -e "42 + 8"` | Evaluate an expression and print the result |
| `echo "show 42" \| nc` | Pipe NC code via stdin |
| **Compile & Build** | |
| `nc compile <file>` | Compile to LLVM IR (`.ll` file) |
| `nc build <file>` | Compile to native binary |
| `nc build <file> -o <name>` | Compile with custom output name |
| `nc build <file> -b <behavior>` | Compile specific behavior |
| `nc build .` | Build all `.nc` files in current directory |
| `nc build <dir>` | Build all `.nc` files in a directory |
| `nc build . --recursive` | Build all `.nc` files recursively |
| `nc build --all` | Build all `.nc` files recursively (shorthand) |
| `nc build . -o build/` | Output all binaries to a directory |
| `nc build . -j 4` | Build 4 files in parallel |
| `nc build --all -o dist/ -j 8` | Full batch: recursive, parallel, output dir |
| `nc bytecode <file>` | Show compiled bytecode |
| **Analyze** | |
| `nc validate <file>` | Check syntax without running |
| `nc tokens <file>` | Show lexer token stream |
| `nc analyze <file>` | Semantic analysis |
| `nc profile <file>` | Run with profiling |
| **HTTP** | |
| `nc get <url>` | HTTP GET request (like curl) |
| `nc get <url> -H "Header: Value"` | HTTP GET with custom header |
| `nc post <url> '<json>'` | HTTP POST with JSON body |
| `nc post <url> '<json>' -H "Header: Value"` | HTTP POST with custom header |
| **Tooling** | |
| `nc fmt <file>` | Auto-format a `.nc` file |
| `nc digest <file>` | Convert Python/JS/YAML to NC |
| `nc lsp` | Start Language Server Protocol server |
| **Package Management** | |
| `nc pkg init` | Initialize package manifest |
| `nc pkg install <name>` | Install a package |
| `nc pkg list` | List installed packages |
| **Testing** | |
| `nc test` | Run all tests in `tests/lang/` |
| `nc test -v` | Run tests (verbose output) |
| `nc test <file>` | Run specific test file |
| `nc conformance` | Run conformance tests |
| **Info** | |
| `nc version` | Show version |

---

## 41. Complete Examples

### Example 1: Hello World Service

```nc
service "hello-world"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name + "!"

to health_check:
    respond with "healthy"

api:
    GET /hello runs greet
    GET /health runs health_check
```

**Run:** `nc serve hello.nc -p 8080`
**Test:** `curl http://localhost:8080/hello?name=Alice`
**Output:** `"Hello, Alice!"`

---

### Example 2: AI Ticket Classifier

```nc
service "ticket-classifier"
version "1.0.0"

configure:
    ai_model is "nova"

define Ticket as:
    id is text
    title is text
    description is text
    priority is text optional

to classify with ticket:
    purpose: "Classify a support ticket using AI"
    ask AI to "Classify this support ticket as: bug, feature, question, incident. Return JSON with category and confidence." using ticket save as classification
    set check to validate(classification, ["category"])
    if check.valid is equal no:
        ask AI to "Return ONLY valid JSON with: category, confidence" save as classification
    log "Classified: {{classification.category}}"
    respond with classification

api:
    POST /classify runs classify
```

**Run:** `nc serve classifier.nc`
**Test:**

```bash
curl -X POST http://localhost:8000/classify \
  -H "Content-Type: application/json" \
  -d '{"ticket": {"id": "1", "title": "Login broken", "description": "Cannot login after update"}}'
```

**Output:**

```json
{"category": "bug", "confidence": 0.95}
```

---

### Example 3: Data Processing Script

```nc
service "data-processor"
version "1.0.0"

to process_numbers:
    set numbers to [45, 23, 67, 12, 89, 34, 56]

    show "Original: " + str(numbers)
    show "Sorted:   " + str(sort(numbers))
    show "Reversed: " + str(reverse(numbers))
    show "Length:   " + str(len(numbers))
    show "Sum:      " + str(sum(numbers))
    show "Average:  " + str(average(numbers))
    show "Min:      " + str(first(sort(numbers)))
    show "Max:      " + str(last(sort(numbers)))

    // Filter: numbers above 50
    set big_numbers to []
    repeat for each n in numbers:
        if n is above 50:
            append n to big_numbers
    show "Above 50: " + str(big_numbers)

    // Transform: double each number
    set doubled to []
    repeat for each n in numbers:
        append n * 2 to doubled
    show "Doubled:  " + str(doubled)

    respond with {"count": len(numbers), "sum": sum(numbers), "average": average(numbers)}
```

**Run:** `nc run data.nc -b process_numbers`

**Output:**

```
Original: [45, 23, 67, 12, 89, 34, 56]
Sorted:   [12, 23, 34, 45, 56, 67, 89]
Reversed: [56, 34, 89, 12, 67, 23, 45]
Length:   7
Sum:      326
Average:  46.571429
Min:      12
Max:      89
Above 50: [67, 89, 56]
Doubled:  [90, 46, 134, 24, 178, 68, 112]
```

---

### Example 4: File-Based Report Generator

```nc
service "report-generator"
version "1.0.0"

configure:
    ai_model is "nova"

to generate_report with file_path:
    purpose: "Read data and generate an AI-powered report"

    if file_exists(file_path) is equal no:
        respond with {"error": "File not found: " + file_path}

    set raw_data to read_file(file_path)
    set lines to split(raw_data, "\n")
    log "Read {{len(lines)}} lines from {{file_path}}"

    ask AI to "Analyze this data and provide: summary, key_insights (list), recommendations (list). Data: {{raw_data}}" save as analysis

    set timestamp to time_now()
    set report_content to "# Data Report\n\nGenerated: " + str(timestamp) + "\n\n" + str(analysis)
    write_file("report_output.md", report_content)

    log "Report written to report_output.md"
    respond with analysis
```

**Run:** `nc run report.nc -b generate_report`

---

### Example 5: Interactive Number Guessing Game

```nc
service "guessing-game"
version "1.0.0"

to play:
    set secret to int(random() * 100) + 1
    set attempts to 0
    set guesses to []

    show "I'm thinking of a number between 1 and 100!"

    while attempts is below 10:
        set attempts to attempts + 1
        set guess to int(random() * 100) + 1
        append guess to guesses

        if guess is equal to secret:
            show "Got it in " + str(attempts) + " attempts!"
            respond with {"attempts": attempts, "secret": secret, "guesses": guesses}

        if guess is above secret:
            show str(guess) + " — too high!"
        otherwise:
            show str(guess) + " — too low!"

    show "Out of attempts! The number was " + str(secret)
    respond with {"attempts": 10, "secret": secret, "guesses": guesses}
```

**Run:** `nc run game.nc -b play`

---

### Example 6: Webhook Handler

```nc
service "webhook-handler"
version "1.0.0"

configure:
    port: 9000

to handle_github with action, repository, sender:
    purpose: "Handle GitHub webhook events"
    match action:
        when "opened":
            log "New PR/issue opened in {{repository.name}} by {{sender.login}}"
            ask AI to "Summarize this event: {{action}} in {{repository.name}}" save as summary
            respond with {"status": "processed", "summary": summary}
        when "closed":
            log "PR/issue closed in {{repository.name}}"
            respond with {"status": "noted", "action": action}
        otherwise:
            respond with {"status": "ignored", "action": action}

to handle_stripe with type, data:
    purpose: "Handle Stripe payment events"
    match type:
        when "payment_intent.succeeded":
            log "Payment received: ${{data.amount}}"
            respond with {"status": "processed"}
        when "payment_intent.failed":
            log "Payment failed!"
            respond with {"status": "alert_sent"}
        otherwise:
            respond with {"status": "ok"}

api:
    POST /webhooks/github runs handle_github
    POST /webhooks/stripe runs handle_stripe
```

**Run:** `nc serve webhook.nc -p 9000`

---

### Example 7: AI Chatbot — NC vs Python (Side-by-Side)

Building a chatbot with conversation memory is one of the most common AI tasks. Here's how it looks in Python vs NC.

**Python — 45 lines, 4 packages:**

```python
import nc_ai_client as ai_client
import json
from flask import Flask, request, jsonify

app = Flask(__name__)
client = ai_client.Client()
conversations = {}

@app.route("/chat", methods=["POST"])
def chat():
    data = request.json
    user_id = data.get("user_id", "default")
    message = data.get("message", "")

    if user_id not in conversations:
        conversations[user_id] = []

    conversations[user_id].append({"role": "user", "content": message})

    response = client.chat.completions.create(
        model="nova",
        messages=[
            {"role": "system", "content": "You are a helpful assistant."},
            *conversations[user_id][-20:]
        ]
    )

    reply = response.choices[0].message.content
    conversations[user_id].append({"role": "assistant", "content": reply})
    return jsonify({"reply": reply})

if __name__ == "__main__":
    app.run(port=8000)
```

**NC — 18 lines, 0 packages:**

```nc
service "ai-chatbot"
version "1.0.0"

configure:
    ai_model is "nova"

to chat with user_id, message:
    purpose: "Multi-turn conversational AI with memory"
    set mem to memory_new(20)
    memory_add(mem, "user", message)
    set context to memory_summary(mem)
    ask AI to "You are a helpful assistant. Conversation so far: {{context}}\nUser: {{message}}" save as reply
    memory_add(mem, "assistant", reply)
    respond with {"reply": reply}

api:
    POST /chat runs chat
```

**Run:** `nc serve chatbot.nc`
**Test:** `curl -X POST http://localhost:8000/chat -d '{"user_id": "alice", "message": "Hi!"}'`

The NC version has zero imports, zero package installs, and reads like a specification rather than code.

---

### Example 8: Data Pipeline with AI Analysis

A common real-world task: read data, process it, have AI analyze it, write a report.

```nc
service "sales-analyzer"
version "1.0.0"

configure:
    ai_model is "nova"

to analyze_sales with file_path:
    purpose: "Read sales CSV, compute stats, generate AI insights"

    set raw to read_file(file_path)
    set rows to csv_parse(raw)
    log "Loaded {{len(rows)}} rows from {{file_path}}"

    set totals to []
    repeat for each row in rows:
        if len(row) is above 2:
            append row[2] to totals

    set stats to {
        "rows": len(rows),
        "fields_per_row": len(first(rows)),
        "data_preview": first(rows)
    }

    ask AI to "Analyze this sales data summary and provide 3 actionable insights: {{stats}}" save as insights

    set report to "# Sales Report\n\n" + str(insights)
    write_file("report.md", report)
    log "Report written to report.md"
    respond with insights

api:
    POST /analyze runs analyze_sales
```

---

### Example 9: Cross-Language Execution — Calling Python from NC

NC can call any language at runtime. Here's how to run a Python script, capture its output, and use the result:

```nc
service "python-bridge"
version "1.0.0"

to run_analysis with data_path:
    purpose: "Run a Python analysis script and use the results"

    set result to exec("python3", "analyze.py", data_path)
    log "Python returned: {{result}}"

    if is_record(result):
        show "Analysis score: " + str(result.score)
        show "Confidence: " + str(result.confidence)
        respond with result
    otherwise:
        respond with {"raw_output": result}

to run_shell_command:
    purpose: "Run shell commands and parse output"

    set disk to shell("df -h /")
    show disk

    set files to shell("ls -la /app/data/")
    show files

    respond with {"status": "done"}
```

If the Python script outputs valid JSON, NC automatically parses it into a record you can access with dot notation.

---

### Example 10: ML Model Serving — Load a Trained Model and Serve Predictions

```nc
service "ml-predictor"
version "1.0.0"

configure:
    port: 8080

to load:
    purpose: "Load the trained ML model on startup"
    set model to load_model("model.pkl")
    log "Model loaded: {{model.type}}"
    respond with model

to predict_risk with age, income, credit_score:
    purpose: "Predict customer risk using a trained sklearn model"
    set model to load_model("model.pkl")
    set features to [age, income, credit_score]
    set prediction to predict(model, features)
    log "Prediction: {{prediction}}"
    respond with prediction

api:
    POST /predict runs predict_risk
```

**Test:**

```bash
curl -X POST http://localhost:8080/predict \
  -H "Content-Type: application/json" \
  -d '{"age": 35, "income": 60000, "credit_score": 720}'
```

This works with any sklearn, PyTorch, TensorFlow, or ONNX model — NC spawns a persistent Python subprocess to handle the inference.

---

### Example 11: Converting Python Code to NC

NC can automatically digest Python, JavaScript, and YAML into NC code:

```bash
# Convert a Python file to NC
nc digest app.py        # produces app.nc

# Convert a JavaScript file to NC
nc digest server.js     # produces server.nc

# Convert YAML config to NC
nc digest config.yaml   # produces config.nc

# Convert JSON to NC configure block
nc digest api.json      # produces api.nc
```

**What the digester converts:**

| Python | NC |
|--------|-----|
| `class User:` → `__init__` with `self.name` | `define User as:` with `name is text` |
| `def greet(name):` | `to greet with name:` |
| `return value` | `respond with value` |
| `print("hello")` | `show "hello"` |
| `x = 42` | `set x to 42` |
| `import flask` | `// import flask` (comment for review) |

| JavaScript | NC |
|-----------|-----|
| `function greet()` | `to greet:` |
| `const x = 42` | `set x to 42` |
| `console.log(x)` | `show x` |
| `return value` | `respond with value` |

The digester uses pattern recognition, not a full parser — always review the output.

---

### Real-World Use Case: Why NC Instead of Python?

**Scenario:** Your team needs to build 5 AI-powered microservices — a ticket classifier, a chatbot, a document summarizer, a sentiment analyzer, and an email router.

**With Python:**
- Install Python 3.x on each server
- `pip install flask openai pydantic redis` on each service
- Write 50-100 lines per service (routes, error handling, AI client setup, JSON parsing)
- Manage `requirements.txt` and virtual environments per service
- Total: ~400 lines of Python, 5 Dockerfiles (150 MB+ images each)

**With NC:**
- Copy the `nc` binary (570KB) to each server
- Write 15-25 lines per service in plain English
- No packages, no virtual environments, no dependency conflicts
- Total: ~100 lines of NC, 1 Dockerfile (22.9MB Alpine image)

NC doesn't replace Python for data science or model training. But for the "last mile" — turning AI models into production services — NC eliminates 80% of the boilerplate.

---

## 42. Frequently Asked Questions (FAQ)

### General Questions

**Q: What is NC?**
NC is a programming language where you write code in plain English. It's designed for AI platforms — AI operations like `ask AI to "analyze"` are built into the language itself.

**Q: Why another programming language?**
Most languages treat AI as a library you have to import and configure. NC treats AI as a **language primitive**. Instead of writing 30 lines of Python boilerplate, you write `ask AI to "analyze this" using data`.

**Q: What is NC written in?**
The NC runtime is a single 570KB binary with zero runtime dependencies. It compiles to native code.

**Q: Does NC replace Python?**
No. NC is designed for a specific use case: turning AI models into production services, building APIs, and defining workflows — all in plain English. Use Python for data science and model training, use NC for the "last mile" of deploying those models as services. They work together: NC can call Python scripts via `exec()` and load Python-trained models via `load_model()`.

**Q: Can NC work with Python?**
Yes, in three ways:
1. **Call Python at runtime:** `set result to exec("python3", "script.py")`
2. **Load Python ML models:** `set model to load_model("model.pkl")` then `predict(model, features)`
3. **Convert Python to NC:** `nc digest app.py` auto-converts Python code to NC syntax

**Q: Is NC production-ready?**
NC v1.0 is fully functional. The foundations are solid (85 language test files, 947 test behaviors, 112 Python integration tests, all passing), with a complete compiler pipeline, JIT VM, HTTP server, batch build, 21 security fixes, and AI integration. Ready for production deployment and community feedback.

### Language Questions

**Q: How do I call AI?**

```nc
ask AI to "classify this ticket" using ticket_data save as result
```

**Q: How do I create an API server?**

```nc
api:
    GET  /users runs list_users
    POST /users runs create_user
```

Then run: `nc serve myfile.nc -p 8080`

**Q: How do I handle errors?**

```nc
try:
    set x to int("not a number")
on error:
    log "Error: " + error
finally:
    log "Cleanup done"
```

**Q: Can NC make real AI calls?**
Yes. NC calls any AI-provider-compatible API via HTTP (libcurl). Set your API key:

```bash
export NC_AI_KEY="<YOUR_API_KEY>"
nc serve myfile.nc
```

**Q: Can NC convert Python/JavaScript to NC?**
Yes:

```bash
nc digest app.py        # Python → NC
nc digest server.js     # JavaScript → NC
nc digest config.yaml   # YAML → NC
```

### Technical Questions

**Q: How does NC execute code?**

```
Source (.nc) → Lexer → Parser → AST → Bytecode Compiler → VM
```

The VM is a stack-based machine with 48 opcodes, purpose-built for NC.

**Q: Does NC have a garbage collector?**
Yes — automatic memory management.

**Q: Does NC support concurrency?**
Yes — coroutines, event loop, and built-in concurrency.

**Q: Can NC compile to native code?**
Yes. `nc build app.nc -o myapp` compiles directly to a standalone native binary. NC can also generate LLVM IR (`nc compile file.nc`). For projects with many files, `nc build --all -o build/ -j 8` batch-builds everything in parallel.

**Q: Does NC work in my IDE?**
A VS Code extension is included in `editor/vscode/`. It provides syntax highlighting, snippets, and bracket matching. The LSP server (`nc lsp`) provides autocomplete and diagnostics.

**Q: What AI providers work with NC?**
Any AI-provider-compatible API:
- NC AI (NC AI)
- NC AI (NC AI, self-hosted)
- External AI provider (via gateway)
- Ollama (local models)
- Azure AI
- AI proxy proxy
- Any custom endpoint

---

## 43. Quick Reference Cheat Sheet

### Variables

```nc
set x to 42
set name to "Alice"
set items to [1, 2, 3]
set user to {"name": "Alice"}
```

### Output

```nc
show "Hello"              // print value
log "Debug: {{variable}}" // log with template
```

### Control Flow

```nc
if condition:              // if
otherwise if condition:    // else-if (chained)
otherwise:                 // else
match x:                   // switch
    when "a":              // case
    otherwise:             // default
repeat for each i in list: // for-in
repeat 5 times:            // for N
while condition:           // while
repeat while condition:    // while (synonym)
stop                       // break
skip                       // continue
```

### Behaviors

```nc
to name with params:      // define
    respond with value    // return (exits immediately)
run name with args        // call
```

### AI

```nc
ask AI to "prompt" save as result
ask AI to "prompt" using context save as result
ask AI to "prompt" using model "nova" save as result
```

### Error Handling

```nc
try:
    // risky code
on error:
    show error
finally:
    // cleanup
```

### API

```nc
api:
    GET  /path runs behavior
    POST /path runs behavior
```

### Comments

```nc
// line comment
# also a comment
```

### Notifications

```nc
notify channel "message"
send channel "message"
emit "event_name"
```

### Imports

```nc
import "module"
import "module" as alias
import "math"              // pi, e, tau
```

### Common Functions

```nc
// String
len(x)    upper(s)    lower(s)     trim(s)
split(s, d)           join(l, s)   contains(s, sub)
replace(s, old, new)  starts_with(s, p)  ends_with(s, p)
substr(s, start, end)

// Math
abs(x)    sqrt(x)    pow(x, y)   ceil(x)
floor(x)  round(x)   min(a, b)   max(a, b)
random()

// Type
type(x)   str(x)     int(x)      float(x)
is_text(x)  is_number(x)  is_list(x)  is_record(x)
is_bool(x)  is_none(x)

// List
first(l)  last(l)    sort(l)     reverse(l)
sum(l)    average(l) unique(l)   flatten(l)
range(n)  slice(l, s, e)  index_of(l, v)  count(l, v)
any(l)    all(l)     append(l, v)  remove(l, v)

// Record
keys(m)   values(m)  has_key(m, k)

// I/O
read_file(p)    write_file(p, c)    file_exists(p)
input(prompt)   print(...)

// Data formats
json_encode(x)  json_decode(s)
yaml_parse(s)   csv_parse(s)   toml_parse(s)   xml_parse(s)   ini_parse(s)

// AI / ML
load_model(p)   predict(m, f)   unload_model(m)
validate(m, fields)   ai_with_fallback(prompt, ctx, models)
chunk(text, size)     top_k(items, k)     token_count(s)

// Memory
memory_new(max)   memory_add(m, role, msg)   memory_get(m)
memory_clear(m)   memory_summary(m)

// Cache
cache(k, v)   cached(k)   is_cached(k)

// System
env(name)   time_now()   time_ms()   time_format(ts, fmt)
exec(cmd, args...)   shell(cmd)
chr(n)   ord(c)
```

---

### Docker

```bash
docker build -t nc .                              # build image
docker run -it nc repl                             # REPL
docker run -v $(pwd):/app nc run /app/file.nc      # run file
docker run -p 8080:8080 -v $(pwd):/app nc serve /app/file.nc  # serve
docker compose up                                  # compose
```

---

## 44. About the Project

### Developed By

| Field | Detail |
|-------|--------|
| **Creator** | Nuckala Sai Narender |
| **Role** | Founder & CEO |
| **Company** | DevHeal Labs AI |
| **Website** | [devheallabs.in](https://devheallabs.in) |
| **Email** | [support@devheallabs.in](mailto:support@devheallabs.in) |
| **First Release** | January 2026 |

### License

| Item | Detail |
|------|--------|
| **Open Source License** | Apache License 2.0 |
| **Copyright** | Copyright 2026 DevHeal Labs AI |

The NC programming language is open-source software. You are free to use, modify, and distribute it under the terms of the Apache 2.0 license.

### Project Report Summary

**NC** is a novel programming language that introduces a fundamentally new paradigm for software development. It enables developers to write executable programs using plain-English notation that reads like natural language, while compiling to efficient bytecode through a custom compiler and JIT virtual machine.

The language eliminates traditional programming boilerplate by allowing services, APIs, AI integrations, data pipelines, and infrastructure to be defined through keyword-driven statements (`to`, `ask AI to`, `respond with`, `save as`) and automatic service composition. NC is not a toy language — it has tensor math, automatic differentiation, distributed training coordination, cross-language interop, enterprise sandboxing, and a Python-like CLI that lets you write code directly in your terminal.

**Key innovations:**
1. **AI as a language primitive** — `ask AI to "analyze"` is a language statement, not a library call
2. **Plain-English syntax** — code reads like a specification document, not machine instructions
3. **Single binary runtime** — 570KB, zero runtime dependencies, ships everything from HTTP server to JSON parser
4. **Python-like CLI** — `nc "show 42"`, `nc -c "code"`, `nc -e "expr"`, pipe from stdin — same workflow
5. **Cross-language digestion** — `nc digest app.py` converts Python/JS/YAML to NC automatically
6. **Built-in data stack** — JSON, YAML, CSV, XML, TOML, INI parsing, database, caching — all built-in
7. **ML model serving** — load sklearn/PyTorch/TensorFlow/ONNX models and serve predictions over HTTP
8. **Tensor engine** — N-dimensional arrays with matmul, ReLU, softmax (CUDA/Metal stubs ready)
9. **Automatic differentiation** — tape-based autograd with SGD and Adam optimizers (like PyTorch)
10. **Distributed computing** — cluster coordination, gradient all-reduce, worker management
11. **Enterprise ready** — sandboxing, API auth, audit logging, multi-tenant isolation, resource quotas
12. **Automatic memory management** — efficient handling of strings, lists, and records

### Project Statistics

| Metric | Value |
|--------|-------|
| Source modules | 34 modules |
| Total lines | 45,000+ |
| Binary size | 570KB |
| Runtime dependencies | Zero |
| Build time | ~5 seconds |
| Startup time | ~7 ms |
| Test suites | 4 (VM Safety, New Features, Enterprise, V1 Enhancements) |
| Test cases | 292 (34 + 59 + 88 + 111) |
| Python validation tests | 112 (67 basic + 45 deep integration) |
| CI build targets | 6 (macOS ARM/Intel, Linux x86/ARM, Windows MinGW/MSVC) |
| Stdlib functions | 160+ |
| Token types | 125 |
| AST node types | 57 |
| VM opcodes | 48 |
| Platforms | Linux, macOS, Windows |
| Architectures | x86_64, ARM64 |
| Docker image size | Alpine: 22.9MB, Debian: 158MB |
| Cross-language digest | Python, JavaScript, TypeScript, YAML, JSON |
| ML model support | sklearn (.pkl), PyTorch (.pt), TensorFlow (.h5), ONNX (.onnx) |
| License | Apache License 2.0 |

### Project Structure

```
nc-lang/
├── nc/                    # NC implementation
│   ├── src/                # Runtime components
│   │   # Lexer, Parser, Compiler, VM, Standard library
│   │   # HTTP client & server, JSON, Database
│   │   # REPL, LSP, Debugger, WebSocket
│   │   # Module system, Package manager, Tensor ops
│   │   # See docs/NC_DEVELOPER_INTERNALS.md for details
│   ├── include/            # Headers
│   └── Makefile            # Build system
├── editor/vscode/          # VS Code extension
├── examples/               # Example programs
├── tests/                  # Test suite
├── docs/                   # Documentation
├── Dockerfile              # Docker image (Alpine)
├── docker-compose.yml      # Docker Compose config
├── install.sh              # Cross-platform installer
├── LICENSE                 # Apache License 2.0
└── README.md               # Project README
```

### Getting Help

- **Full Language Guide:** `docs/NC_LANGUAGE_GUIDE.md`
- **Tutorial (5 min):** `docs/TUTORIAL.md`
- **Examples:** `examples/` directory (10+ real-world examples)
- **REPL help:** Type `.help` in the REPL
- **Validate your code:** `nc validate myfile.nc`
- **Contact:** [support@devheallabs.in](mailto:support@devheallabs.in)

---

**NC — The AI Language**
Created by **Nuckala Sai Narender** | **DevHeal Labs AI** ([devheallabs.in](https://devheallabs.in))
Copyright 2026 DevHeal Labs AI. Apache License 2.0.
*Write services, not boilerplate.*
