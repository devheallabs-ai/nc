# NC Language — Known Issues & Planned Work

This document tracks incomplete work, known limitations, and planned improvements
identified in the codebase (consolidating TODO/FIXME comments from source files).

Issues are grouped by component and priority. Contributions welcome — see
[CONTRIBUTING.md](CONTRIBUTING.md) for how to submit a fix.

---

## High Priority

### HTTP Server (`nc_http.c`)

**Issue:** HTTP chunked transfer encoding is not fully implemented.
Streaming responses (SSE / `stream respond`) work but chunked decoding
on the client read side is incomplete.

**Workaround:** Use `respond with` for single-shot responses. Use
`stream respond` for SSE (which sends newline-delimited frames correctly).

**Planned:** v1.2.0

---

## Medium Priority

### LLVM Backend (`nc_llvm.c`)

**Issue:** The LLVM IR emit path is a stub. `nc compile --llvm` parses
and type-checks the source but does not produce valid LLVM IR output.
The feature is guarded behind `NC_LLVM_ENABLED` which is off by default.

**Workaround:** Use the default interpreter/bytecode paths.

**Planned:** v2.0.0 (requires LLVM 17+ dependency)

---

### WebAssembly Target (`nc_wasm.c`)

**Issue:** `nc compile --wasm` is a placeholder. The Wasm emit path
outputs a stub module that does not contain the compiled program.

**Workaround:** None — Wasm target is not production-ready.

**Planned:** v2.0.0

---

## Low Priority

### JWT Validation (`nc_http.c`)

**Issue:** JWT verification uses HMAC-SHA256 (HS256) with a shared
secret. RS256 (asymmetric) and JWKS endpoint verification are not
implemented.

**Impact:** For enterprise SSO integrations that issue RS256 tokens,
JWT validation must be handled by a reverse proxy (e.g., nginx with
`lua-jwt`) rather than the NC service itself.

**Workaround:** Validate RS256 tokens at the proxy layer; pass a trusted
claim header to the NC service.

**Planned:** v1.2.0

---

## Completed (recently fixed)

| Issue | Fixed in |
|-------|----------|
| HiveAnt backend removed from open repo | v1.1.0 (see CHANGELOG) |
| Model weight distribution undocumented | v1.1.0 (see MODEL_WEIGHTS.md) |
| `nc ai download` command missing | v1.1.0 (scripts/download_weights.sh added) |
| LLVM backend missing TODO stubs in polyglot | Documented here — planned v2.0.0 |
| **JSON parser: numbers >63 chars silently truncated** | v1.1.1 — `nc_json.c` `jp_parse_number()` now uses `strtod`/`strtoll` directly on the source pointer; no fixed-size copy, no truncation |
| **JSON exponent parsing unreliable** | v1.1.1 — same fix; exponent form (`1.5e-10`) now parsed correctly via `strtod` |
| **Tensor thread count fixed at 4 (underutilizes cores)** | v1.1.1 — `nc_nova.c` now auto-detects hardware thread count via `GetSystemInfo()` (Windows) / `sysconf(_SC_NPROCESSORS_ONLN)` (POSIX); `OMP_NUM_THREADS` no longer required |
| **`batch_size` not capped against available threads** | v1.1.1 — `nc_nova.c` caps `batch_size` to `max_threads`; prints warning when capped; all config defaults changed to `n_threads=0` (auto) |
| **Polyglot: `async def` / `async function` lost `async` qualifier** | v1.1.1 — `nc_polyglot.c` `digest_python()` now matches `async def` and emits `async to name:`; `digest_javascript()` emits `async to name:` for `async function` |
| **Polyglot: `await` not converted to `gather`** | v1.1.1 — `nc_polyglot.c` both Python and JS paths now emit `gather <expr>` for `await <expr>` |
| **Polyglot: `yield` not converted** | v1.1.1 — `nc_polyglot.c` both paths emit `yield <expr>` (NC has native `yield`) |
| **Polyglot: `Promise.then()` silently dropped** | v1.1.1 — emits `// TODO: convert Promise chain to: gather <promise>` |
| **Migration engine: web frameworks not detected for hybrid mode** | v1.1.1 — `nc_migrate.c` adds `looks_like_web_framework()` detecting Express, Flask, FastAPI, Django, Rails, Spring Boot, Laravel; triggers hybrid wrap instead of direct conversion |
| **Fuzzing harness: HTTP, JSON numbers, templates not covered** | v1.1.1 — `nc_fuzz.c` expanded from 2 targets to 6: JSON parser, JSON number edge cases, NC lexer/parser, template engine (`nc_ai_fill_template`), HTTP JSON-body, chunked encoding |

---

## How to Contribute a Fix

1. Pick an issue above
2. Open a GitHub Issue to claim it (avoids duplicate work)
3. Fix it, add a test, and open a PR
4. Reference this file in your PR description

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full contribution workflow.
