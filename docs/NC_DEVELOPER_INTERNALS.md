# NC Developer Internals

A deep dive into the NC runtime engine for contributors, embedders, and anyone building on top of NC.

For a higher-level onboarding guide, see [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md).

---

## Table of Contents

1. [Compilation Pipeline](#compilation-pipeline)
2. [Source File Map](#source-file-map)
3. [Value System](#value-system)
4. [Virtual Machine](#virtual-machine)
5. [Memory and Garbage Collection](#memory-and-garbage-collection)
6. [Platform Abstractions](#platform-abstractions)
7. [AI Integration Surface](#ai-integration-surface)
8. [Security Model](#security-model)
9. [Extension Points](#extension-points)
10. [JIT and LLVM](#jit-and-llvm)
11. [Bytecode Reference](#bytecode-reference)

---

## Compilation Pipeline

NC source code goes through six stages before execution:

```
Source (.nc file)
      |
      v
  [nc_lexer.c]
  Lexer — plain English tokens
  Keywords: "to", "set", "ask", "respond", "service", ...
  Produces: token stream (TK_IDENT, TK_STRING, TK_NUMBER, ...)
      |
      v
  [nc_parser.c]
  Parser — recursive descent
  Produces: AST (NcNode tree)
      |
      v
  [nc_semantic.c]
  Semantic analysis — type checking, scope resolution,
  undefined reference detection, synonym normalization
      |
      v
  [nc_compiler.c]
  Bytecode compiler — AST → opcode stream
  Constant folding, dead code elimination
  Produces: NcChunk (bytecode + constant pool)
      |
      v
  [nc_optimizer.c]
  Bytecode optimizer — peephole passes
  Collapses redundant loads, fuses push/pop pairs
      |
      v
  [nc_vm.c]
  Stack-based VM — computed-goto dispatch
  Executes NcChunk directly
```

The interpreter (`nc_interp.c`) provides a tree-walk fallback used in the REPL and for certain eval paths.

---

## Source File Map

### Core Pipeline

| File | Responsibility |
|------|---------------|
| `main.c` | Entry point, CLI argument parsing, command dispatch |
| `nc_lexer.c` | Tokenizer — converts plain English source to token stream |
| `nc_parser.c` | Recursive descent parser — token stream to AST |
| `nc_semantic.c` | Semantic analysis pass — scopes, types, synonyms |
| `nc_compiler.c` | Bytecode compiler — AST to NcChunk |
| `nc_optimizer.c` | Peephole bytecode optimizer |
| `nc_vm.c` | Stack-based virtual machine with computed-goto dispatch |
| `nc_interp.c` | Tree-walk interpreter (REPL fallback, eval) |
| `nc_value.c` | Value type implementations (text, number, list, record, function) |
| `nc_gc.c` | Mark-and-sweep garbage collector |

### Standard Library

| File | Responsibility |
|------|---------------|
| `nc_stdlib.c` | Built-in functions: `len`, `str`, `upper`, `lower`, `split`, `join`, `keys`, `values`, tensor ops |
| `nc_json.c` | JSON encode/decode — used by HTTP layer and built-in `parse json` |
| `nc_module.c` | `import` system — resolves `.nc` library files |

### Networking and Services

| File | Responsibility |
|------|---------------|
| `nc_http.c` | HTTP client — `gather data from`, `ask {url}` |
| `nc_server.c` | HTTP server — `service` blocks, `api:` routes, request handling |
| `nc_websocket.c` | WebSocket support — real-time service endpoints |
| `nc_database.c` | Database operations — `store`, `fetch`, `query` built-ins |
| `nc_middleware.c` | Middleware pipeline — auth, CORS, rate limiting, JWT |

### AI Integration

| File | Responsibility |
|------|---------------|
| `nc_ai_router.c` | Provider routing — dispatches `ask AI to` to OpenAI, Anthropic, Google, Ollama, or NOVA local |
| `nc_ai_enterprise.c` | Enterprise AI features — audit logging, usage limits, provider failover |
| `nc_ai_efficient.c` | Efficient AI request batching and caching |
| `nc_generate_stubs.c` | Open-source stubs for NOVA generation/training/reasoning functions |

### Platform and Runtime

| File | Responsibility |
|------|---------------|
| `nc_async.c` | Async/concurrent execution — coroutines, event loop |
| `nc_enterprise.c` | Enterprise features — multi-tenancy, RBAC, audit trails, compliance |
| `nc_pkg.c` | Package manager — `nc install`, `nc publish` |
| `nc_debug.c` | Debugger — breakpoints, step, inspect |
| `nc_lsp.c` | Language Server Protocol — IDE integration |
| `nc_repl.c` | Interactive REPL with history and syntax highlighting |
| `nc_suggestions.c` | Error recovery and did-you-mean suggestions |
| `nc_plugin.c` | Plugin system — `dlopen`/`LoadLibrary` FFI |
| `nc_embed.c` | Embedding API — host C application embeds NC runtime |
| `nc_polyglot.c` | Polyglot bridge — call Python/JavaScript from NC |
| `nc_migrate.c` | Python-to-NC migration helper |

### Build and Compilation Targets

| File | Responsibility |
|------|---------------|
| `nc_build.c` | `nc build` — AOT compilation to native binary |
| `nc_wasm.c` | WebAssembly target — compile NC to WASM |
| `nc_llvm.c` | LLVM IR generation backend |
| `nc_jit.c` | JIT compilation with trace caching |
| `nc_cuda.c` | CUDA device management stubs (for NOVA GPU inference) |

### UI Compiler

| File | Responsibility |
|------|---------------|
| `nc_ui_compiler.c` | NC UI (`.ncui`) compiler — plain English UI to AST |
| `nc_ui_vm.c` | NC UI component runtime |
| `nc_ui_html.c` | NC UI → HTML/CSS/JS code generation |
| `nc_terminal_ui.c` | Terminal UI renderer for `nc_ui` in CLI context |

---

## Value System

All NC values are represented as a tagged union `NcValue` (defined in `include/nc_value.h`):

```c
typedef enum {
    NC_VAL_NONE,
    NC_VAL_BOOL,
    NC_VAL_NUMBER,
    NC_VAL_TEXT,
    NC_VAL_LIST,
    NC_VAL_RECORD,
    NC_VAL_FUNCTION,
    NC_VAL_NATIVE,
    NC_VAL_TENSOR,
    NC_VAL_ERROR,
} NcValueType;

typedef struct NcValue {
    NcValueType type;
    union {
        bool        boolean;
        double      number;
        NcString   *text;
        NcList     *list;
        NcRecord   *record;
        NcFunction *function;
        NcNative   *native;
        NcTensor   *tensor;
        NcError    *error;
    };
} NcValue;
```

The `NC_NONE()` macro creates a zero-value (type = `NC_VAL_NONE`). Values on the VM stack are always `NcValue` structs — no boxing overhead for primitives.

**Strings** are interned heap objects with reference counts. Identical string literals share one allocation.

**Lists** are dynamic arrays of `NcValue` with amortized O(1) append.

**Records** are open hash maps (`string key → NcValue`). Used for JSON objects, service state, and named parameters.

---

## Virtual Machine

`nc_vm.c` implements a register-less stack machine with computed-goto dispatch:

```c
// Dispatch table — one label per opcode
static void *dispatch_table[] = {
    &&op_push, &&op_pop, &&op_add, &&op_sub, ...
};

// Main loop
DISPATCH:
    goto *dispatch_table[*ip++];

op_add:
    b = stack_pop();
    a = stack_pop();
    stack_push(nc_value_add(a, b));
    goto DISPATCH;
```

Computed-goto dispatch eliminates branch misprediction overhead from a `switch` statement. This is a C extension supported by GCC and Clang (and MinGW on Windows).

**Call frames** are heap-allocated `NcFrame` structs pushed onto a linked-list call stack. Each frame holds:
- Pointer to the `NcChunk` being executed
- Instruction pointer (`ip`)
- Base pointer into the value stack (`bp`)
- Local variable slots

**Tail calls** are optimized by reusing the current frame when the compiler detects a tail-position call.

---

## Memory and Garbage Collection

NC uses a **tri-color mark-and-sweep** GC (`nc_gc.c`):

1. **Mark** — starting from the root set (VM stack, call frames, global scope), mark all reachable `NcValue` objects gray, then black.
2. **Sweep** — walk the heap's object list. Free any object that is still white (unreachable).
3. **Compact** (optional) — not currently implemented; heap can fragment over long-running services.

GC is triggered when the heap allocation count exceeds a threshold (`gc_trigger`), which doubles after each cycle (adaptive pacing).

**Reference counting is not used** — all heap objects are GC-managed. The only exception is string interning, which uses an explicit retain/release API to keep interned strings alive across GC cycles.

**Arena allocator** (`nc_arena.c`) is used for short-lived allocations during compilation. The entire AST allocation arena is freed in one call after bytecode compilation completes.

---

## Platform Abstractions

`include/nc_platform.h` (~600 lines) abstracts OS differences for all 34 source files:

| Abstraction | Linux | macOS | Windows |
|-------------|-------|-------|---------|
| `nc_setenv(k, v)` | `setenv` | `setenv` | `_putenv_s` |
| `nc_unsetenv(k)` | `unsetenv` | `unsetenv` | `_putenv_s(k,"")` |
| `nc_mkdir_p(path)` | `mkdir -p` | `mkdir -p` | `_mkdir` loop |
| `nc_tempdir()` | `/tmp` | `/tmp` | `GetTempPath` |
| `nc_secrets_dir()` | `~/.nc/secrets` | `~/Library/NC/secrets` | `%APPDATA%\NC\secrets` |
| `nc_sleep_ms(ms)` | `nanosleep` | `nanosleep` | `Sleep` |
| `nc_thread_create` | `pthread_create` | `pthread_create` | `CreateThread` |
| Coroutines | `ucontext_t` (experimental) | `setjmp/longjmp` | Windows Fibers |
| Dynamic loading | `dlopen/dlsym` | `dlopen/dlsym` | `LoadLibrary/GetProcAddress` |
| Shared memory | `shm_open` | `shm_open` | `CreateFileMapping` |

All platform-specific `#ifdef` blocks are confined to `nc_platform.h`. The 34 `.c` files use only the `nc_*` abstraction functions.

---

## AI Integration Surface

NC's `ask AI to` statement routes through `nc_ai_router.c`. The router selects a provider in this priority order:

1. **NOVA local** — if `~/.nc/model/nc_model.bin` exists and NOVA binary is active
2. **Environment variable** — `NC_AI_PROVIDER` overrides the default (`openai`, `anthropic`, `google`, `ollama`)
3. **Config file** — `~/.nc/config.nc` `ai_provider:` key
4. **Auto-detect** — tries `NC_AI_KEY`, `OPENAI_API_KEY`, `ANTHROPIC_API_KEY` in order

The router calls a provider-specific adapter that formats the request, calls the HTTP endpoint, and returns a normalized `NcValue` response.

**NOVA local inference** bypasses the HTTP layer entirely — it calls the NOVA C API directly via the pre-built binary's exported symbols. In the open-source build, these calls hit the no-op stubs in `nc_generate_stubs.c` and print an install message.

---

## Security Model

22 CWE-class vulnerabilities were identified and fixed during the v1.0.0 security audit. Key mitigations:

| Threat | Mitigation |
|--------|-----------|
| Command injection in `nc_polyglot.c` | Argument array API, no shell expansion |
| Path traversal in file operations | `nc_path_sanitize()` — normalizes and checks against allowed roots |
| SSRF in `gather data from` | Blocklist for private IP ranges (RFC 1918, loopback, link-local) |
| Integer overflow in buffer sizing | `nc_safe_add()` / `nc_safe_mul()` — abort on overflow |
| Stack overflow in recursive evaluation | Depth counter with configurable limit (default: 10,000 frames) |
| Timing side-channel in auth | `nc_timing_safe_compare()` — constant-time string comparison |
| TLS certificate bypass | `CURLOPT_SSL_VERIFYPEER` always `1L`; no override in production mode |
| JWT secret exposure | Secrets loaded from `nc_secrets_dir()` — never from argv or environment in production |
| Uninitialized memory reads | All `NcValue` allocations zero-initialized via `nc_value_alloc()` |
| Format string injection | All `fprintf`/`snprintf` calls use literal format strings |

The full security audit is documented in [SECURITY_CERTIFICATE.md](../SECURITY_CERTIFICATE.md).

---

## Extension Points

### Plugin API (`nc_plugin.h`)

Native plugins are shared libraries (`.so` / `.dylib` / `.dll`) that export an `nc_plugin_init` symbol:

```c
// my_plugin.c
#include "nc_plugin.h"

static NcValue my_builtin(NcValue *args, int n_args) {
    // ... implementation
    return NC_TEXT("result");
}

void nc_plugin_init(NcPluginAPI *api) {
    api->register_builtin("my_function", my_builtin, 1);
}
```

Load in NC:
```nc
use plugin "my_plugin"
set result to my_function with "hello"
```

### Embedding API (`nc_embed.h`)

Embed the NC runtime in a C/C++ host application:

```c
#include "nc_embed.h"

NcRuntime *rt = nc_runtime_create();
nc_runtime_eval(rt, "service \"embedded\" ...");
NcValue result = nc_runtime_call(rt, "my_function", args, n_args);
nc_runtime_free(rt);
```

### Native Builtins Registration

To add a new built-in function to the NC standard library, add an entry in `nc_stdlib.c`:

```c
// In nc_stdlib_register() in nc_stdlib.c
nc_register_builtin(vm, "my_builtin", nc_ncfn_my_builtin, 2);
```

And implement it as:
```c
NcValue nc_ncfn_my_builtin(NcValue a, NcValue b) {
    // ... implementation
    return result;
}
```

Declare it in `include/nc_stdlib.h` for use in other source files.

---

## JIT and LLVM

### JIT (`nc_jit.c`)

The JIT compiler is a **tracing JIT** — it profiles hot loops during interpretation and compiles them to native code on demand.

Trace recording is triggered when a backward branch (loop back-edge) exceeds the hot threshold (default: 1,000 iterations). The trace compiler:
1. Records the bytecode sequence for one loop iteration
2. Emits x86-64 or ARM64 machine code via a lightweight code emitter
3. Caches the compiled trace keyed by the loop header PC
4. Replaces the interpreter dispatch with a direct call to the native trace

Traces are invalidated when a branch exits the expected path (guard failure) — the interpreter resumes and re-profiles.

### LLVM (`nc_llvm.c`)

`nc build` invokes the LLVM backend via `nc_llvm.c` to produce an optimized ahead-of-time binary. The NC bytecode is lowered to LLVM IR, then compiled with standard `-O2` optimizations.

This requires LLVM 14+ to be installed. The LLVM backend is optional — `nc build` falls back to embedding the NC interpreter if LLVM is not found.

---

## Bytecode Reference

NC bytecode (`NcOpcode` enum in `include/nc.h`) uses a simple variable-width encoding. Most instructions are 1 byte; instructions with operands are followed by 1–4 byte operand values.

| Opcode | Operand | Description |
|--------|---------|-------------|
| `OP_PUSH_CONST` | u16 index | Push constant[index] onto stack |
| `OP_PUSH_TRUE` | — | Push `true` |
| `OP_PUSH_FALSE` | — | Push `false` |
| `OP_PUSH_NONE` | — | Push `none` |
| `OP_POP` | — | Pop top of stack |
| `OP_DUP` | — | Duplicate top of stack |
| `OP_LOAD_LOCAL` | u8 slot | Push local[slot] |
| `OP_STORE_LOCAL` | u8 slot | Pop → local[slot] |
| `OP_LOAD_GLOBAL` | u16 index | Push global[index] |
| `OP_STORE_GLOBAL` | u16 index | Pop → global[index] |
| `OP_ADD` | — | Pop b, a; push a + b |
| `OP_SUB` | — | Pop b, a; push a - b |
| `OP_MUL` | — | Pop b, a; push a * b |
| `OP_DIV` | — | Pop b, a; push a / b |
| `OP_EQ` | — | Pop b, a; push a == b |
| `OP_NEQ` | — | Pop b, a; push a != b |
| `OP_LT` | — | Pop b, a; push a < b |
| `OP_GT` | — | Pop b, a; push a > b |
| `OP_AND` | — | Pop b, a; push a && b |
| `OP_OR` | — | Pop b, a; push a \|\| b |
| `OP_NOT` | — | Pop a; push !a |
| `OP_JUMP` | i16 offset | Unconditional jump by offset |
| `OP_JUMP_IF_FALSE` | i16 offset | Pop; jump if false |
| `OP_CALL` | u8 n_args | Call function with n_args args |
| `OP_RETURN` | — | Return top of stack to caller |
| `OP_MAKE_LIST` | u16 count | Pop count values; push list |
| `OP_MAKE_RECORD` | u16 count | Pop count key/value pairs; push record |
| `OP_GET_FIELD` | u16 name | Pop record; push record[name] |
| `OP_SET_FIELD` | u16 name | Pop value, record; record[name] = value |
| `OP_ASK_AI` | u16 prompt | Invoke AI router with prompt |
| `OP_RESPOND` | — | Emit HTTP response from top of stack |
| `OP_HALT` | — | Stop execution |

Use `nc disasm <file.nc>` to view the bytecode for any NC program.

---

## Further Reading

- [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) — building, testing, contributing
- [LANGUAGE_SPEC.md](LANGUAGE_SPEC.md) — formal grammar and specification
- [NOVA_ARCHITECTURE.md](../NOVA_ARCHITECTURE.md) — NOVA AI engine internals
- [SECURITY_CERTIFICATE.md](../SECURITY_CERTIFICATE.md) — security audit details
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) — language cheat sheet
