<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="250">
</p>

# Changelog

All notable changes to NC are documented here.

## [Unreleased]

---

## [1.3.0] - 2026-04-11

### Standard Library — Python parity

NC now ships a complete general-purpose standard library. Every module listed below
is built-in (zero dependencies, zero installs).

#### Math module (extended)
- Constants: `math_pi()`, `math_e()`, `math_inf()`, `math_nan()`
- Trig: `asin`, `acos`, `atan`, `atan2`, `hypot`, `degrees`, `radians`
- Logarithms: `log2`, `log10` (in addition to existing `log`, `exp`)
- Utilities: `trunc`, `sign`, `isnan`, `isinf`, `isfinite`
- Number theory: `gcd(a,b)`, `factorial(n)`
- Interpolation: `clamp(x, lo, hi)`, `lerp(a, b, t)`

#### Random module
- `rand_float()` — float in [0.0, 1.0) (xorshift64, uniform distribution)
- `randint(a, b)` — integer in [a, b] inclusive
- `rand_range(stop)` / `rand_range(start, stop)` — Python `range`-style
- `rand_choice(list)` — random element from a list
- `rand_shuffle(list)` — shuffled copy (Fisher-Yates)
- `rand_sample(list, k)` — k elements without replacement
- `rand_seed(n)` — reproducible random sequences for testing

#### String module (extended)
- `str_count(s, sub)` — count occurrences
- `str_pad_left(s, width)`, `str_pad_right(s, width)`, `str_center(s, width)` — padding with optional fill char
- `str_repeat(s, n)` — repeat a string n times
- `str_lstrip(s)`, `str_rstrip(s)` — strip leading/trailing characters
- `str_title(s)` — Title Case conversion
- `str_is_digit(s)`, `str_is_alpha(s)`, `str_is_alnum(s)` — character class checks
- `str_format(template, arg1, arg2, ...)` — positional `{0}`, `{1}` substitution

#### URL module
- `url_encode(s)` — percent-encode for HTTP (spaces as `+`)
- `url_decode(s)` — decode percent-encoded strings
- `url_parse(s)` → map with `scheme`, `host`, `port`, `path`, `query`, `fragment`, `params`

#### Log module
- `log_debug(msg)`, `log_info(msg)`, `log_warn(msg)`, `log_error(msg)`
- `log_set_level("debug" | "info" | "warn" | "error" | "off")`
- ISO 8601 timestamps; warn/error goes to stderr

#### Collections module
- `counter(list)` → map of `{item: count}` (like Python `Counter`)
- `unique(list)` → deduplicated list preserving order
- `flatten(list)` → one-level deep flatten

#### Type module
- `type_of(v)` → `"int"`, `"float"`, `"string"`, `"bool"`, `"list"`, `"map"`, `"nothing"`
- `to_int(v)`, `to_float(v)`, `to_bool(v)` — explicit type coercions

### Database — file-backed persistent store

NC programs can now persist data to a local JSON file without any external
service or environment variable setup.

```
configure:
    store_url is "file://./mydata.json"

store "hello world" into "greeting"
gather greeting from "greeting"          # → "hello world"
```

- **Direct API**: `db_put(url, key, value)`, `db_get(url, key)`, `db_delete(url, key)`,
  `db_list(url)`, `db_all(url)`
- **Transparent via `store`/`gather`**: set `NC_STORE_URL=file://./data.json`
- Atomic writes (write to `.tmp`, then rename) — crash-safe
- Thread-safe per-file mutex
- Flat JSON format — human-readable, version-control friendly

### Package manager — improved install

- **GitHub shorthand**: `nc pkg install github:user/repo` (or `gh:user/repo`)
- **GitLab shorthand**: `nc pkg install gitlab:user/repo` (or `gl:user/repo`)
- **Bitbucket shorthand**: `nc pkg install bitbucket:user/repo` (or `bb:user/repo`)
- **Automatic GitHub fallback**: if the registry is unavailable, tries
  `github.com/devheallabs-ai/nc-pkg-{name}` before failing
- **Better error messages**: shows all install options when a package is not found

---

## [1.2.1] - 2026-04-11

### Bug fixes — Windows compatibility

- **`nc_async.c`**: Fixed crash on Windows when using `async`/`gather`. The fiber-based
  coroutine runtime was calling `GetCurrentFiber()` without first converting the thread
  to a fiber via `ConvertThreadToFiber()`, returning garbage and crashing on
  `SwitchToFiber()`. Added per-thread `tl_fiber_converted` flag; `ConvertThreadToFiber()`
  is called once on first coroutine resume.
- **`nc_async.c`**: Fixed fiber handle leak — `nc_coro_free()` now calls `DeleteFiber()`
  on Windows so fiber handles are released when coroutines complete.
- **`nc_platform.h`**: Added `socklen_t` typedef for MSVC (not defined in MSVC headers
  by default, causing compilation failures).
- **`nc_server.c`**: Fixed `select()` first-argument truncation on 64-bit Windows.
  `SOCKET` is `UINT_PTR` (64-bit); casting to `(int)server_fd + 1` silently truncated
  the value. Windows ignores the first argument to `select()` so `0` is passed instead.
- **`nc_nova.c`**: Fixed compilation failure on MSVC — `#include <pthread.h>` was
  unguarded. Guarded behind `#ifndef NC_WINDOWS`. Replaced raw `pthread_t` /
  `pthread_create` / `pthread_join` with `nc_thread_t` / `nc_thread_create` /
  `nc_thread_join` abstractions. Fixed `batch_worker_fn` calling convention to
  `unsigned __stdcall` on Windows.
- **`main.c`**, **`nc_ai_benchmark.c`**, **`nc_ai_enterprise.c`**, **`nc_nova_reasoning.c`**:
  Replaced unguarded `clock_gettime(CLOCK_MONOTONIC, ...)` calls (POSIX-only, compile
  error on MSVC) with the cross-platform `nc_clock_ms()` from `nc_platform.h`.
  Added missing `#include "../include/nc_platform.h"` to the three `.c` files that
  lacked it.

---

## [1.2.0] - 2026-04-03

### v1.2.0 — Production AI Release with Enterprise Semantic Validation

NC v1.2.0 is the first production-quality AI release. The built-in model is frozen at the best quality-gated checkpoint, enterprise code generation passes semantic validation, and the full release pipeline is automated and tested end-to-end.

#### AI Model — Production Freeze
- **Model frozen at best checkpoint** — eval perplexity 77.70 (quality gate threshold: 280.0)
- **63,750 training steps** completed across 76,976 sequences from 549-file NC corpus
- **Best checkpoint promoted** — `nc_model_v1_release_best.bin` is now the canonical v1 release model
- **Release gate passed** — 609-eval pass streak, automated quality gate enforced
- **Training state**: `completed=1`, `release_gate_passed=1`, frozen at step 63,750

#### Enterprise Semantic Enforcement
- **`nc_ai_project_enterprise_semantic_valid()`** — New C function validates enterprise features in AI-generated code
- **Service validation** — Verifies tenant management (`list_tenants`, `/api/v1/tenants`), role-based access (`list_roles`, `permission_matrix`), analytics (`analytics_overview`), approval workflows (`approve_request`, `reject_request`), and alert center (`list_alerts`)
- **UI validation** — Verifies Dashboard, Tenants, Roles, Analytics, Approvals, and Alerts sections
- **Two-gate architecture** — Structural validation + semantic validation before accepting generated code
- **Template fallback** — When AI output fails semantic checks, deterministic templates ensure correct enterprise scaffolds
- **11/11 semantic checks pass** on both Windows and Unix platforms

#### Release Pipeline
- **`prepare_release.ps1`/`.sh`** — Now includes enterprise semantic smoke as a release gate
- **14 automated checks** — artifacts, binary, local tests, AI smoke, semantic smoke, memory/policy smoke, release gate, training completion
- **Readiness levels** — `blocked` → `candidate` → `final`, with automated manifest generation
- **SHA-256 checksums** for all release artifacts in `v1_release_manifest.json`

#### Quality Validation
- **C unit tests**: 485/485 PASS
- **Language tests**: 8/8 PASS (version, syntax, terminal UX, examples, inline code, eval, build)
- **AI project smoke**: PASS (service.nc + app.ncui + test_app.nc + README.md + dist/app.html)
- **Enterprise semantic smoke**: 11/11 PASS (tenants, roles, analytics, approvals, alerts)
- **Memory/policy smoke**: PASS (nc_long_term_memory + nc_policy_memory validated)

#### Cross-Platform
- **Windows**: PowerShell test harness with MSYS2/MinGW build chain
- **Unix/macOS**: Bash test harness with `cygpath` path conversion for mixed environments
- **Metal GPU**: Apple Silicon acceleration path preserved
- **Docker**: Multi-stage build support via Dockerfile

### Repository Housekeeping

#### HiveAnt Backend Removed from Open Repository (March 2026)

HiveAnt was an enterprise multi-agent swarm intelligence platform built on NC.
In March 2026, the HiveAnt core agent logic, swarm coordination algorithms, and
intelligence modules were moved out of the open-source monorepo and into the
private enterprise distribution.

**What changed:**
- `hiveant/` directory (agent definitions, swarm core, scheduler, kernel) — removed from this repo
- `hiveant/agents/`, `hiveant/core/`, `hiveant/intelligence/` — enterprise-only, not open source
- `nc-apps/hiveant-dashboard/` — **remains** as a pure NC UI frontend (open source)

**Why:**
HiveAnt's swarm coordination and pheromone-graph algorithms are patent-pending.
Open-sourcing them before patent grant would compromise IP protection.

**For users of HiveAnt:**
- The HiveAnt backend is distributed as a signed binary via the enterprise release
- API documentation: https://devheallabs.in/hiveant/docs
- Enterprise inquiries: enterprise@devheallabs.in

**Training corpus note:**
NC v1.1.0 training data includes NC syntax examples from HiveAnt (in
`nc-lang/engine/training_corpus/`) — these are syntax demonstrations only,
not the algorithm code.

---

## [1.1.0] - 2026-03-22

### v1.1.0 — Built-in AI & GPU Acceleration

NC v1.1.0 makes NC the world's first programming language with a built-in AI model. The NC binary now ships with a decoder-only transformer, BPE tokenizer, and full training pipeline — all hardware-accelerated with Metal GPU and Apple Accelerate BLAS.

#### Built-in AI Engine
- **`nc ai generate`** — Generate NC applications from plain English descriptions
- **`nc ai status`** — Display model architecture, parameters, and status
- **`nc ai train`** — Training instructions for custom models
- **`nc ai serve`** — AI generation API server (placeholder)
- **Decoder-only transformer** — 6 layers, 8 heads, 256 dim, 4096 BPE vocabulary (~5M params)
- **BPE tokenizer** — Byte Pair Encoding with NC-specific vocabulary, auto-training from corpus
- **Training pipeline** — Adam optimizer, cosine LR decay, gradient clipping, checkpointing
- **Binary model format** — NCM1 magic + config + weights, save/load support
- **Text generation** — `nc_model_generate_text()` end-to-end: encode → generate → decode

#### GPU & Hardware Acceleration
- **Metal Performance Shaders** — GPU-accelerated GEMM on Apple Silicon via Objective-C bridge (nc_metal.m)
  - MPSMatrixMultiplication for large matrix multiplies (10-50x speedup)
  - Zero-copy shared memory on Apple Silicon
  - Auto-threshold: GPU for matrices > 4096 elements
- **Apple Accelerate BLAS** — `cblas_sgemm` for matmul (50-100x), `vDSP_vadd`/`vDSP_vsmul` for element-wise ops
- **3-tier dispatch** — Metal GPU → Accelerate BLAS → Tiled CPU fallback (all platforms)
- **`-DACCELERATE_NEW_LAPACK`** flag for modern macOS headers

#### New Source Files (4 files)
- `nc_model.c` (992 lines) — Transformer neural network with `ncm_` prefixed functions
- `nc_model.h` — Model and tensor API declarations
- `nc_training.c` — Training loop, Adam optimizer, data loading, checkpointing
- `nc_tokenizer.c` — BPE tokenizer: train, encode, decode, save, load
- `nc_metal.m` — Metal GPU acceleration (Objective-C, macOS only)
- `nc_metal.h` — Metal API declarations

#### Build System
- Makefile updated: 38 source files (37 C + 1 Objective-C)
- macOS: `-framework Accelerate -framework Metal -framework MetalPerformanceShaders -framework Foundation`
- Objective-C compilation rule for nc_metal.m
- All 113 test files continue to pass

#### Training Infrastructure
- Training corpus: 1.3MB of NC code from 8 projects (nc-lang, hiveant, swarmops, nc-ui, nc-ai, nc-scripts, nc-apps, neuraledge)
- Training script: `nc-ai/train_nc_model.nc` (runs with `nc run`)
- Production config: dim=256, 6 layers, 8 heads, 4096 vocab, 512 max_seq
- Checkpoints saved every 500 steps to `training_data/checkpoints/`

---

## [1.0.0] - 2026-03-16

### v1.0.0 — Enterprise AI Language Release

NC v1.0.0 closes all major Python feature gaps, making NC enterprise-ready as a plain-English AI language. This release adds typed error handling, a built-in test framework, Python-style string formatting, rich data structures, async/await/yield syntax, data processing pipelines, and 44 new built-in functions across both execution engines.

#### Language Features
- **Typed error catching** — `catch "TimeoutError":` with 7 error categories and rich `err.message`/`err.type`/`err.line` context
- **Assert statements** — `assert condition, "message"` with line numbers and stack traces
- **Test blocks** — `test "name":` with isolated pass/fail execution (✓ PASS / ✗ FAIL output)
- **Yield** — `yield value` for value accumulation and streaming output
- **Await** — `await expr` syntax placeholder for future async support
- **Stream respond** — SSE event frame generation for server-sent events
- **6 new tokens** — `catch`, `assert`, `test`, `async`, `await`, `yield`
- **6 new AST nodes** — NODE_ASSERT, NODE_TEST_BLOCK, NODE_ASYNC_CALL, NODE_AWAIT, NODE_YIELD, NODE_STREAM_RESPOND

#### New Built-in Functions — Data Structures (17 functions)
- **String formatting** — `format("Hello {name}", vars)` with positional and named placeholders
- **Set operations** — `set_new()`, `set_add()`, `set_has()`, `set_remove()`, `set_values()`
- **Tuple** — `tuple(a, b, ...)` for immutable-style sequences
- **Deque** — `deque()`, `deque_push_front()`, `deque_pop_front()` for double-ended queues
- **Counter** — `counter(list)` counts occurrences like Python's collections.Counter
- **Default map** — `default_map(val)` for maps with default values
- **Enumerate** — `enumerate(list)` returns `[[0,item],[1,item],...]`
- **Zip** — `zip(a, b)` merges two lists into pairs
- **Error classification** — `error_type(msg)` classifies error strings into 7 categories
- **Stack inspection** — `traceback()` returns current call stack as list of frames

#### New Built-in Functions — Data Processing (27 functions)
- **map(list, field)** — extract field from list of records (like Python's list comprehension)
- **reduce(list, op)** — fold list with "+", "*", "min", "max", "join"
- **items(record)** — return [[key, val], ...] pairs (like Python dict.items())
- **merge(record1, record2)** — merge two records (like Python {**a, **b})
- **find(list, key, value)** — find first record where field matches value
- **group_by(list, field)** — group records by field value (like itertools.groupby)
- **take(list, n)** — first n elements (like itertools.islice)
- **drop(list, n)** — skip first n elements (like list[n:])
- **compact(list)** — remove none values (like [x for x in list if x is not None])
- **pluck(list, field)** — extract field values from list of records
- **chunk_list(list, size)** — split list into chunks of given size
- **sorted(list)** — non-mutating sort (returns new sorted list)
- **reversed(list)** — non-mutating reverse (returns new reversed list)
- **repeat_value(val, n)** — create list of n copies
- **title_case(str)** — convert to Title Case (like Python str.title())
- **capitalize(str)** — capitalize first letter (like Python str.capitalize())
- **pad_left(str, width, fill?)** — left pad string (like Python str.rjust())
- **pad_right(str, width, fill?)** — right pad string (like Python str.ljust())
- **char_at(str, index)** — character at position (like Python str[i])
- **repeat_string(str, n)** — repeat string n times (like Python str * n)
- **isinstance(val, type_name)** — type check by name (like Python isinstance())
- **is_empty(val)** — check if empty string/list/record/none
- **clamp(val, lo, hi)** — clamp value to range (like max(lo, min(hi, val)))
- **sign(val)** — return -1, 0, or 1
- **lerp(a, b, t)** — linear interpolation
- **gcd(a, b)** — greatest common divisor (like math.gcd())
- **dot_product(list1, list2)** — vector dot product (like numpy.dot())
- **linspace(start, end, n)** — evenly spaced values (like numpy.linspace())
- **to_json(val)** / **from_json(str)** — aliases for json_encode/json_decode

#### Compiler & VM
- **Compiler support** — NODE_ASSERT, NODE_TEST_BLOCK, NODE_AWAIT, NODE_YIELD, NODE_STREAM_RESPOND all compile to bytecode
- **VM parity** — All 44 new functions registered in OP_CALL_NATIVE dispatch (VM + interpreter)
- **125 token types** (was 119), **57 AST node types** (was 51), **48 opcodes** (was 47)

#### Documentation
- **Python vs NC comparison** — comprehensive feature-by-feature comparison with 60-row table (docs/PYTHON_VS_NC.md)
- **User Manual** — updated §20 (error handling), §21 (stdlib), added typed catch, assert, test blocks, data structures
- **Language Guide** — updated §10 (error handling), §9 (built-in functions), §20 (async/streaming)
- **Enterprise docs** — updated test results, added v1.0.0 feature documentation
- **Website** — updated stats, added Python comparison table, new enterprise feature cards, 30 new function tags
- **Developer Reference** — updated §29 with typed catch, assert, test blocks, data structures, formatting, streaming

#### Tests
- **111/111 v1 enhancement tests** — items, merge, find, group_by, take, drop, compact, reduce, map, title_case, capitalize, pad_left/right, char_at, repeat_string, isinstance, is_empty, clamp, sign, gcd, sorted, reversed, pluck, chunk_list, repeat_value, dot_product, linspace, lerp, to_json, combined pipelines
- **88/88 enterprise feature tests** — format, set, deque, counter, tuple, enumerate, zip, error_type, traceback, assert, test blocks
- **59/59 new feature tests** — format, set, deque, counter, enumerate, zip, tuple, error_type, traceback, assert, test blocks
- **34/34 VM safety tests** — integer/float arithmetic, slicing, average, sum, math functions (regression)
- **Zero regressions** across all test suites

#### Release Stats
- Binary size: 570 KB (arm64)
- Total built-in functions: 160+
- Total test cases passing: 292 (34 + 59 + 88 + 111)
- Platforms: Linux, macOS, Windows (6 targets)
- Dependencies: 0

## [0.9.0] - 2026-03-14

### Enterprise Features & Bug Fixes

#### New Built-in Functions
- **Cryptography** — `hash_sha256()`, `hash_password()`, `verify_password()`, `hash_hmac()` (pure C, zero dependencies)
- **JWT** — `jwt_verify()` returns claims map or false; `jwt_generate()` now available in all execution paths
- **Sessions** — `session_create()`, `session_set()`, `session_get()`, `session_exists()`, `session_destroy()`
- **Request context** — `request_header()`, `request_headers()`, `request_ip()`, `request_method()`, `request_path()`
- **Feature flags** — `feature("flag_name")` with env-var and percentage rollout support
- **Circuit breaker** — `circuit_open("service")` for downstream failure protection
- **Higher-order lists** — `sort_by()`, `max_by()`, `min_by()`, `sum_by()`, `map_field()`, `filter_by()`
- **Time** — `time_iso()` now available in all execution paths

#### Bug Fixes
- **`max(list)` / `min(list)`** — now accepts a single list argument (previously required 2 scalar args)
- **`round(val, decimals)`** — new 2-argument overload returns float with correct precision
- **`sort()` on maps** — replaced O(n²) insertion sort with quicksort; maps now sort by field value
- **`wait milliseconds`** — added `TOK_MILLISECONDS` token; supports `ms`, `millisecond`, `milliseconds`
- **`try/otherwise`** — parser now accepts `otherwise:` as error handler after `try:` blocks
- **`continue` in loops** — confirmed working via `skip` synonym mapping
- **VM + JIT parity** — all enterprise functions registered in all 3 execution paths (interpreter, VM, JIT)
- **`port is "env:..."`** — server now resolves `env:` prefix for string port values
- **List concatenation** — `[1,2] + [3,4]` now returns `[1,2,3,4]`; `list + [item]` appends; works inside loops
- **Blank lines in behaviors** — blank/comment lines no longer emit spurious DEDENT tokens
- **Consecutive `gather` blocks** — multiple `gather` with options blocks in one behavior now parse correctly
- **`set map[key].field`** — chained bracket-then-dot access now supported in assignments
- **`method` keyword** — no longer treated as synonym for `to` (broke `method:` in gather options)
- **`http_get`/`http_post`** — now available in VM and JIT paths (were interpreter-only)
- **`sort(list, "field")`** — 2-arg sort now works in VM and JIT (not just `sort_by`)
- **Map iteration in compiler** — `OP_GET_INDEX` with integer on maps returns key at position
- **Schedule handler** — `every N:` now executes `run` statements from the handler body
- **`try/on_error` variable scope** — variables set inside `on_error` now propagate to the enclosing scope
- **`json_decode()` on parsed data** — returns value as-is when input is already a list/map/number/boolean (was returning null)
- **`json_decode()` error signaling** — invalid JSON now sets `had_error` flag so `try/on_error` catches it
- **`gather from variable`** — `gather x from url` now resolves `url` as a variable at runtime (was treating it as literal string)
- **HTTP crash protection** — added `CURLOPT_CONNECTTIMEOUT` (10s) and `CURLOPT_NOSIGNAL` to prevent server hangs on failed HTTP calls; 500+ responses now return body instead of retrying; retry backoff capped at 4s
- **Default User-Agent** — outbound HTTP sends generic `NC/1.1` User-Agent; customizable via `NC_HTTP_USER_AGENT` env var or per-request headers map
- **Accept header** — all outbound HTTP sends `Accept: application/json, text/plain, */*` by default
- **Compressed responses** — `CURLOPT_ACCEPT_ENCODING` enabled for auto gzip/deflate/br decompression
- **JSON string auto-parse** — dot access (`resp.field`) and bracket access (`resp["field"]`) now auto-parse JSON strings, fixing nested field access after `http_get` with concatenated URLs
- **Map hash index fallback** — `map_find_dense` falls back to linear scan if hash probe misses, preventing silent lookup failures on large maps
- **Response parsing** — strips BOM, leading/trailing whitespace before JSON parse

#### New CLI Commands
- **`nc start`** — auto-discover and start .nc services (like `npm start`)
- **`nc stop`** — stop all running NC services
- **`nc dev [file]`** — validate then start (development mode)
- **`nc deploy`** — build container image (auto-generates Dockerfile if missing)
- **`nc deploy --tag app:v1`** — build with custom image tag
- **`nc deploy --push`** — build and push to registry

#### Infrastructure
- **Security scanning** CI workflow (container scanning, dependency scanning, code analysis, secret detection)
- **Code quality** CI workflow (coverage with gcov/lcov, cppcheck static analysis, sanitizer builds)
- **Signed releases** with SHA-256 checksums and SBOM generation
- **Helm chart** with HPA, PDB, NetworkPolicy, ServiceMonitor
- **Terraform modules** for cloud deployment
- **Grafana dashboard** and alert rules
- **Load testing** configs (k6 load, stress, soak tests)
- **Pre-commit hooks** configuration
- **`.clang-format`** for C code formatting

#### Security Hardening
- Security headers on all responses (HSTS, X-Frame-Options, CSP, etc.)
- Graceful shutdown with connection draining (`NC_DRAIN_TIMEOUT`)
- Readiness (`/ready`) and liveness (`/live`) probe endpoints
- Auto-generated OpenAPI spec at `/openapi.json`
- Enhanced `/metrics` with worker stats and runtime info
- AST caching to avoid re-parsing source on every request
- Configurable loop guard via `NC_MAX_LOOP_ITERATIONS`
- Cookie engine support via `NC_HTTP_COOKIES=1`

#### Test Results
- **201/201** conformance tests passed
- **411/412** unit tests passed (1 pre-existing)
- **11/11** HTTP endpoint tests verified via `nc serve`

## [0.8.0] - 2026-03-12

### Production Release — Cross-Platform, Enterprise-Ready

#### New Features
- **Batch build** — `nc build .`, `nc build --all`, `nc build dir/` builds all `.nc` files at once
- **Parallel build** — `-j 4` flag uses `fork()` (macOS/Linux) or `CreateProcess` (Windows) for concurrent compilation
- **Output directory** — `nc build . -o build/` places all binaries in a folder
- **Windows REPL** — built-in readline fallback (`nc_basic_readline`) enables REPL on all platforms without readline library
- **macOS coroutines** — replaced deprecated `ucontext` with `setjmp/longjmp` + inline assembly (ARM64 + x86_64)
- **Windows subprocess I/O** — bidirectional pipes via `CreateProcess` + `CreatePipe` (was unidirectional `_popen`)
- **MSVC CI** — CMake + MSVC build job in GitHub Actions alongside MinGW
- **Python validation suite** — 67 basic tests + 45 deep integration tests (HTTP server, build pipeline, project scaffolding)
- **Windows PowerShell runner** — `tests/run_tests.ps1` for native Windows testing
- **Docker test image** — `Dockerfile.test` builds and runs both Python test suites on Linux
- **Cross-platform CI** — `.github/workflows/validate.yml` runs Python tests on macOS, Linux, and Windows

#### Cross-Platform Support (34/34 source files clean)
- **Platform abstraction layer** (`nc_platform.h`, 550+ lines) — sockets, threads, filesystem, time, signals, JIT memory all abstracted
- **Thread-per-request HTTP server** — replaced `fork()` model, works on Linux/macOS/Windows
- **Zero bare POSIX calls** — all OS-specific code goes through `nc_platform.h`
- **CMakeLists.txt** — builds with GCC, Clang, and MSVC
- **Windows installer** (`install.ps1`) — auto-detects CMake/MSYS2, builds from source or downloads binary
- **macOS installer** — `install.sh` correctly maps `darwin` to `macos` for binary downloads
- **nc_socket_t** used everywhere — no `int` sockets that truncate `HANDLE` on 64-bit Windows
- **SIGTERM** guarded with `#ifndef NC_WINDOWS` across all files
- **`select()` first arg** uses `0` on Windows (Winsock ignores nfds)
- **ETIMEDOUT fallback** defined for MSVC in `nc_platform.h`
- **`nc_embed.h`** documents both Unix and Windows build commands
- **Plugin paths** use `NC_PATH_SEP_STR` instead of hardcoded `/`
- **Makefile** includes `nc_embed.c`; **CMakeLists.txt** links `dl` for `test_nc`

#### Bug Fixes (15 critical/high)
- **VM bounds checks** — OP_CONSTANT, OP_GET_LOCAL, OP_SET_LOCAL, OP_MAKE_LIST, OP_MAKE_MAP all bounds-checked
- **GC map key marking** — map keys now marked during GC to prevent use-after-free
- **Parser EOF sentinel** — `cur()` returns static sentinel when `count == 0` instead of indexing `tokens[-1]`
- **Compiler null check** — `nc_compiler_compile` checks for NULL program
- **Compiler overflow** — `make_constant`/`make_var` set `had_error` when index > 255
- **Optimizer memmove** — `opt_dead_store` now properly shifts bytecode instead of corrupting it
- **JIT mmap fallback** — removed `malloc` fallback that produced non-executable memory
- **`replace("")` infinite loop** — early return when delimiter is empty
- **`ftell()` returns -1** — checked before `malloc`
- **Curl handle leak** — fallback handles now cleaned up; `reusable_curl` is thread-local
- **Cluster use-after-free** — `cluster.workers = NULL` after `free()`
- **LSP Content-Length DoS** — rejects values <= 0 or > 1MB
- **`ord("")` crash** — checks string length before accessing `chars[0]`
- **JSON realloc null** — uses temp variable; keeps old buffer on failure
- **Rate limiter race** — mutex added around shared state
- **Parallel build slot -1** — validates slot before array access
- **Recursion depth limit** — `eval_expr` capped at 1000 depth
- **Memory clear leak** — frees old message list before replacing

#### Security Fixes (10 vulnerabilities)
- **CWE-78** — `shell()` rejects metacharacters (`;|&$\`><!(){}`) unless `NC_ALLOW_EXEC=unsafe`
- **CWE-22** — module import path traversal blocked: rejects names with `/`, `\`, `..`, control chars
- **CWE-22** — `nc_path_is_safe()` null-byte check fixed: rejects all control characters (bytes < 0x20)
- **CWE-287** — `nc_enterprise.c` auth uses constant-time `nc_ct_string_equal()` instead of `strcmp`
- **CWE-918** — `NC_HTTP_STRICT` mode added: denies all outbound HTTP when set
- **CWE-295** — explicit `CURLOPT_SSL_VERIFYPEER=1` and `CURLOPT_SSL_VERIFYHOST=2` on all HTTPS requests
- **CWE-276** — CORS defaults to `"null"` (deny) when auth is enabled, not `"*"`
- **CWE-190** — LSP Content-Length rejects negative values
- **Supply chain** — `tar` extraction uses `--strip-components=1` and `--no-same-owner`

#### Reliability
- **Dynamic buffers** — request (1MB default), response (4MB default), configurable via env vars
- **JSON parser** — dynamic allocation, no 8KB truncation
- **NULL checks** — all malloc/realloc in hot paths
- **`intptr_t`** for process handles in parallel build (prevents 64-bit truncation)

#### Observability
- **`NC_OBS()`** — structured JSON events for Datadog/CloudWatch/Splunk
- **`NC_LOG_FORMAT=json`** — machine-readable log output
- **OTEL trace export** — W3C Trace Context propagation in HTTP server

#### Docker
- **Non-root USER** — containers run as `nc` user
- **HEALTHCHECK** — auto-detected by Docker/K8s
- **Multi-arch** — `linux/amd64` + `linux/arm64`
- **Test image** — `Dockerfile.test` for automated Linux validation

#### Testing
- **296 unit test assertions** — values, strings, lists, maps, lexer, parser, compiler, VM, JSON, stdlib, GC, middleware, platform, async
- **75 language test files** (648 behaviors) — all passing
- **67 Python basic validation tests** — CLI, stdlib, types, errors, security
- **45 Python deep integration tests** — HTTP server, build pipeline, REPL, project scaffolding, stress
- **Windows PowerShell runner** — `tests/run_tests.ps1`
- **Cross-platform CI** — macOS, Linux, Windows (MinGW + MSVC)
- **Docker Linux validation** — `Dockerfile.test`

#### Documentation
- **Batch build docs** in README and User Manual
- **Security features** table in README
- **Cross-platform matrix** in README
- **Python test suite** usage instructions
- **Updated CLI reference** with all batch build commands

---

## [0.7.0] - 2026-03-08

### Added — Universal AI Engine
- **Universal AI bridge** — NC's C code has zero knowledge of any AI company, model, or API format
- **Template engine** (`nc_ai_fill_template`) — fills `{{placeholders}}` in JSON request templates
- **Path extractor** (`nc_ai_extract_by_path`) — navigates JSON responses by dot-path (e.g. `choices.0.message.content`)
- **External AI config** (`nc_ai_providers.json`) — all API format knowledge lives in a JSON file, not C code
- **Named adapter presets** — switch AI providers by editing JSON, no recompile needed
- **Configurable auth** — auth header and format are template-driven (`Bearer {{key}}`, `{{key}}`, etc.)
- **Dynamic buffers** (`NcDynBuf`) — replaces all fixed-size char arrays, no more silent truncation
- **AI response contract** — every `ask AI` call returns `{ok, response, model, raw}` guaranteed

### Added — "Is this what you meant?" suggestions
- **Damerau-Levenshtein distance** (`nc_suggestions.c`) — suggests close matches for typos
- Integrated into **parser** — misspelled keywords get suggestions
- Integrated into **interpreter** — undefined variables and unknown functions get suggestions
- Handles **transpositions** (e.g. `lne` → `len`, `pritn` → `print`)

### Added — Call traceback
- **Frame chain** — behaviors push/pop trace entries on call
- **Traceback output** — errors show the full call chain with behavior names, file, and line numbers

### Changed
- **Renamed `cnc/` directory to `nc/`** — cleaner project structure
- **README** — fixed "zero dependencies" claim to "minimal dependencies" (libcurl, libm, pthreads, libedit)
- **README** — added AI Provider System documentation
- Updated ~55 references across 20+ files for the cnc → nc rename

### Removed
- Hardcoded AI provider functions (openai_build_request, anthropic_parse_response, etc.)
- Company names from C source code — NC is now fully provider-agnostic
- Fixed-size buffers in AI path (32KB body, 8KB prompt) — replaced with dynamic allocation

## [0.6.0] - 2026-03-07

### Added
- JIT compiler foundation with computed goto dispatch (2-3x faster VM)
- Hot path detection for JIT candidates
- Native code emission (x86-64 + ARM64 via mmap)
- Async coroutines and event loop
- Worker threads for parallel execution
- Generators (range with lazy evaluation)
- Distributed training via TCP message passing
- Gradient aggregation (all-reduce sum/average)
- Cluster management (connect, heartbeat, shutdown)
- Sandbox execution (restrict file/network/exec access)
- API key authentication with rate limiting
- Audit logging
- Multi-tenant isolation
- Conformance test suite (12 tests, 11 passing)

## [0.5.0] - 2026-03-07

### Added
- Tape-based autograd engine with backward pass
- Neural network layers (Linear/Dense with Xavier init)
- Optimizers: SGD, Adam (with momentum + bias correction)
- Loss functions: MSE, cross-entropy
- Bytecode optimizer: constant folding, dead store elimination, jump threading
- VM profiler with opcode frequency analysis
- Code formatter (`nc fmt`)
- Cross-language digestion: Python, JavaScript, YAML, JSON → NC
- Training loop helper

## [0.3.0] - 2026-03-07

### Added
- Module system: import resolution, caching, circular import detection
- NC_PATH environment variable for module search
- Built-in stdlib modules (math, time)
- Language specification document
- Complete roadmap (v0.1 → v4.0)

### Changed
- Python runtime moved to `deprecated/` — NC is now C-only

## [0.2.0] - 2026-03-07

### Added
- Full bytecode compiler (AST → opcodes for all statement types)
- Stack-based VM as production execution engine
- `nc run` now goes through bytecode compilation path
- Semantic analyzer with symbol table, scope tracking, type checking
- Mark & sweep garbage collector
- Interactive REPL (`nc repl`)
- Standard library: file I/O, math, strings, time
- VS Code extension with syntax highlighting

### Changed
- Engine label changed to "Bytecode VM (CNC v0.2)"

## [0.1.0] - 2026-03-07

### Added
- NC language design — plain English syntax
- C lexer with 120+ token types and indentation tracking
- Recursive descent parser with 30+ AST node types
- Tree-walking interpreter
- Bytecode VM with 30+ opcodes
- Value system: strings, lists, maps (reference-counted)
- JSON parser and serializer
- HTTP client (libcurl stubs for AI/MCP calls)
- LLVM IR code generator
- Tensor runtime: matmul, relu, softmax + CUDA/Metal stubs
- Package manager skeleton
- Step-through debugger with breakpoints
- LSP server for IDE integration
- 56 C unit tests
- 8 example .nc programs (real-world AI services)
- README, Makefile, .gitignore
- Git repository initialized
