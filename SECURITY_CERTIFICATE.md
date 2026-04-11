<p align="center">
  <img src="docs/assets/nc_mascot.png" alt="NC Mascot" width="200">
</p>

<h1 align="center">NC Security Certificate</h1>
<h4 align="center">v1.0.0 — March 20, 2026 (Updated)</h4>

<p align="center">
  <img src="https://img.shields.io/badge/security-audited-brightgreen" alt="Security Audited">
  <img src="https://img.shields.io/badge/version-v1.0.0-orange" alt="v1.0.0">
  <img src="https://img.shields.io/badge/license-Apache%202.0-green" alt="Apache 2.0">
</p>

---

This document certifies the security posture of **NC v1.0.0**. It details every hardening measure, mitigation, and test implemented in the runtime, HTTP server, AI engine, and toolchain.

**Maintainer:** DevHeal Labs AI  
**Contact:** support@devheallabs.in  
**Vulnerability Reports:** See [SECURITY.md](SECURITY.md)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Path Traversal Prevention](#2-path-traversal-prevention)
3. [Sandbox & Default-Deny Execution](#3-sandbox--default-deny-execution)
4. [Injection Mitigation](#4-injection-mitigation)
5. [HTTP Server Hardening](#5-http-server-hardening)
6. [Authentication & Authorization](#6-authentication--authorization)
7. [Rate Limiting](#7-rate-limiting)
8. [Secret & Credential Protection](#8-secret--credential-protection)
9. [AI Engine Security](#9-ai-engine-security)
10. [Buffer Safety & Memory](#10-buffer-safety--memory)
11. [Docker Security](#11-docker-security)
12. [Supply Chain & Package Manager](#12-supply-chain--package-manager)
13. [Security Test Coverage](#13-security-test-coverage)
14. [Known Limitations](#14-known-limitations)
15. [Compliance Matrix](#15-compliance-matrix)

---

## 1. Executive Summary

| Area | Status | Severity Mitigated |
|------|--------|--------------------|
| Path traversal | **Mitigated** | Critical |
| Code injection (shell/exec) | **Mitigated** | Critical |
| Template/embed injection | **Mitigated** | High |
| AI prompt injection | **Mitigated** | High |
| Timing attacks (auth) | **Mitigated** | High |
| Secret leakage in logs | **Mitigated** | High |
| CORS misconfiguration | **Mitigated** | Medium |
| Rate limiting (DoS) | **Mitigated** | Medium |
| Buffer overflow | **Mitigated** | Critical |
| Integer overflow (arithmetic) | **Mitigated** | Critical |
| Stack underflow (VM) | **Mitigated** | Critical |
| CORS header injection | **Mitigated** | Critical |
| SSRF via redirects | **Mitigated** | High |
| URL-encoded path traversal | **Mitigated** | Medium |
| Parser recursion DoS | **Mitigated** | High |
| JIT dispatch NULL jump | **Mitigated** | Critical |
| SQL-style injection | **Mitigated** | High |
| Non-root containers | **Mitigated** | Medium |
| Package manager injection | **Mitigated** | High |

**Total hardening measures:** 43
**Security test assertions:** 50+
**Penetration test checks:** 27  

---

## 2. Path Traversal Prevention

**Status: MITIGATED**  
**Files:** `engine/src/nc_stdlib.c`, `engine/src/nc_database.c`, `engine/src/nc_server.c`, `engine/src/nc_pkg.c`

### Implementation

`nc_path_is_safe()` in `nc_stdlib.c` enforces the following rules for all file operations (`read_file`, `write_file`, `file_exists`):

| Check | Blocks |
|-------|--------|
| `..` in path | Directory traversal (`../../etc/passwd`) |
| URL-encoded `..` | `%2e%2e`, `%2E%2E`, `.%2e`, `%2e.` variants |
| Null bytes | Null-byte bypass (`file.nc\0.txt`) |
| Absolute paths to system dirs | `/etc/passwd`, `/etc/shadow`, `/etc/sudoers` |
| Proc/sys/dev filesystems | `/proc/self/environ`, `/sys/`, `/dev/` |
| `realpath()` resolution | Symlink-based traversal |

### Additional path checks

- **Database keys** (`nc_database.c`): rejects `..`, `/`, `\` in store keys
- **HTTP server** (`nc_server.c`): rejects `..` in URL paths, returns 400
- **Package manager** (`nc_pkg.c`): validates package names against `[a-zA-Z0-9\-_./:@]`

### Test coverage

- `tests/security/test_security.nc`: `../../etc/passwd`, `/etc/passwd`, null-byte bypass
- `tests/security/test_pentest.sh`: 7 path traversal vectors (1a–1g), URL-encoded traversal

---

## 3. Sandbox & Default-Deny Execution

**Status: MITIGATED**  
**File:** `engine/src/nc_enterprise.c`

### Default sandbox policy

| Capability | Default | Override |
|------------|---------|----------|
| File read | Allowed | — |
| File write | **Denied** | `NC_ALLOW_FILE_WRITE=1` |
| Network | **Denied** | `NC_ALLOW_NETWORK=1` |
| Exec/shell | **Denied** | `NC_ALLOW_EXEC=1` |
| Env access | **Denied** | (sandbox API) |

The `nc_sandbox_check()` function is called before every privileged operation. When denied, the operation returns an error value instead of executing.

### Shell/exec hardening

- `exec()` and `shell()` builtins refuse to run without `NC_ALLOW_EXEC=1`
- All `system()` calls replaced with `execvp` (POSIX) / `CreateProcess` (Windows) via `safe_run()`
- Arguments are shell-escaped with single-quote wrapping; embedded `'` becomes `'\''`

### Test coverage

- `tests/security/test_pentest.sh`: 6 exec sandbox vectors (2a–2f)
- `tests/security/test_pentest.sh`: 2 write sandbox vectors (3a–3b)

---

## 4. Injection Mitigation

**Status: MITIGATED**  
**Files:** `engine/src/nc_embed.c`, `engine/src/nc_http.c`, `engine/src/nc_database.c`, `engine/src/nc_stdlib.c`

### 4.1 Embed Code Injection

The embed API (`nc_embed.c`) escapes all user-supplied values before interpolation into NC source. Escaped characters: `"`, `\`, newline, tab. Variable names are restricted to `[a-zA-Z0-9_]`.

### 4.2 JSON / Template Escaping

`nc_dbuf_append_escaped()` in `nc_http.c` escapes all values inserted into JSON templates: `"`, `\`, `\n`, `\r`, `\t`, and control characters. Used in AI request construction and HTTP responses.

### 4.3 Database Key / Value Injection

`nc_database.c` validates keys (rejects `..`, `/`, `\`) and JSON-escapes all stored values via `json_escape_into()`.

### 4.4 Shell Command Injection

`exec()` in `nc_stdlib.c` wraps arguments in single quotes with proper escaping, calculates safe buffer lengths, and enforces a minimum 8192-byte cap with bounds checking.

### Test coverage

- `tests/security/test_security.nc`: template injection (`{{}}`), SQL-style injection, special characters in store/gather

---

## 5. HTTP Server Hardening

**Status: MITIGATED**  
**File:** `engine/src/nc_server.c`

| Measure | Detail |
|---------|--------|
| **Request size limit** | `NC_MAX_REQUEST_SIZE` — default 1 MB |
| **Response size limit** | `NC_MAX_RESPONSE_SIZE` — default 4 MB |
| **Path length limit** | Paths > 500 characters rejected (400) |
| **Empty path rejection** | Empty or missing path → 400 |
| **Path traversal** | `..` in URL path → 400 |
| **No path reflection in 404** | Response is `{"error":"Not Found"}` — no user input echoed |
| **Query string redaction** | Query parameters logged as `?[REDACTED]` |
| **Worker pool limit** | `NC_MAX_WORKERS` — default 16, returns 503 when full |
| **Accept timeout** | 30-second `select()` timeout prevents hanging |
| **Invalid JSON body** | Malformed request body → 400 |

### CORS

| Setting | Default | Override |
|---------|---------|----------|
| `NC_CORS_ORIGIN` | `NULL` (no wildcard) | Set to specific origin |
| `NC_CORS_METHODS` | `GET, POST, PUT, DELETE, OPTIONS` | Customizable |
| `NC_CORS_HEADERS` | `Content-Type, Authorization` | Customizable |

CORS headers are only sent when `NC_CORS_ORIGIN` is explicitly set. No default `Access-Control-Allow-Origin: *`.

---

## 6. Authentication & Authorization

**Status: MITIGATED**  
**Files:** `engine/src/nc_middleware.c`, `engine/src/nc_server.c`, `engine/src/nc_enterprise.c`

### Supported auth methods

| Method | Header | Implementation |
|--------|--------|----------------|
| Bearer token | `Authorization: Bearer <token>` | JWT basic validation (expiry, signature check) |
| API key | `X-Api-Key: <key>` or `Authorization: ApiKey <key>` | Validated against `NC_API_KEYS` env var |

### Security measures

- **Constant-time comparison** (`ct_string_equal`) for API key validation — prevents timing attacks
- Missing credentials → **401 Unauthorized**
- Invalid credentials → **403 Forbidden**
- Malformed/unsigned JWT tokens → rejected
- Expired JWT tokens → rejected
- Per-key rate limiting in enterprise module

### Role-based access

`nc_enterprise.c` provides `nc_auth_add_key()`, `nc_auth_validate()`, and `nc_auth_get_role()` for multi-tenant isolation.

---

## 7. Rate Limiting

**Status: MITIGATED**  
**Files:** `engine/src/nc_middleware.c`, `engine/src/nc_server.c`

| Setting | Default | Override |
|---------|---------|----------|
| Requests per window | 100 | `NC_RATE_LIMIT_RPM` |
| Window duration | 60 seconds | `NC_RATE_LIMIT_WINDOW` |
| Max tracked clients | 1024 | Compile-time constant |

Rate limiting is per-IP. When exceeded, the server returns **429 Too Many Requests**. Enterprise module adds per-API-key rate limiting.

---

## 8. Secret & Credential Protection

**Status: MITIGATED**  
**Files:** `engine/include/nc.h`, `engine/src/nc_server.c`

### Automatic log redaction

`nc_redact_secret()` is called by `nc_log()` before every log write. The following patterns are detected and masked:

| Pattern | Example | Redacted as |
|---------|---------|-------------|
| `sk-*` | `sk-abc123...` | `sk-[REDACTED]` |
| `Bearer *` | `Bearer eyJ...` | `Bearer [REDACTED]` |
| `token=*` | `token=abc123` | `token=[REDACTED]` |
| `key=*` | `key=secret` | `key=[REDACTED]` |
| `password=*` | `password=hunter2` | `password=[REDACTED]` |
| `secret=*` | `secret=xyz` | `secret=[REDACTED]` |
| `apikey=*` | `apikey=abc` | `apikey=[REDACTED]` |
| `api_key=*` | `api_key=abc` | `api_key=[REDACTED]` |

### Additional protections

- Query strings in HTTP access logs are replaced with `?[REDACTED]`
- `.gitignore` blocks `.env`, `*.key`, `*.pem`, `credentials.json`
- API keys passed exclusively via environment variables (`NC_AI_KEY`)
- `.env.example` provided for safe onboarding (no real values)

---

## 9. AI Engine Security

**Status: MITIGATED**  
**File:** `engine/src/nc_http.c`

### Prompt injection boundary

AI requests include system/user content separation:

```
[System prompt from NC_AI_SYSTEM_PROMPT]
--- BEGIN USER INPUT ---
[User-provided prompt and context]
--- END USER INPUT ---
```

This boundary makes it harder for user input to override system instructions.

### Template safety

All user-supplied values inserted into AI request JSON templates are escaped via `nc_dbuf_append_escaped()` — prevents JSON injection in API calls.

### Provider isolation

NC's C runtime has zero knowledge of any AI provider. All format knowledge lives in `nc_ai_providers.json`. The runtime only fills templates and extracts responses by dot-path. No provider-specific code exists in the binary.

### Outbound HTTP restriction

`NC_HTTP_ALLOWLIST` restricts outbound HTTP requests to approved hosts only. When set, `gather` and AI calls to unlisted hosts are blocked.

---

## 10. Buffer Safety & Memory

**Status: MITIGATED**  
**Files:** `engine/src/nc_vm.c`, `engine/src/nc_jit.c`, `engine/src/nc_stdlib.c`

| Measure | Location |
|---------|----------|
| Stack overflow detection | `nc_vm.c` — checked on every push |
| Call stack depth limit | `nc_jit.c` — `NC_FRAMES_MAX` enforced |
| Dynamic buffers | `NcDynBuf` replaces all fixed-size char arrays |
| NULL checks on allocation | All `malloc`/`realloc` in hot paths |
| Safe buffer sizing in exec | Calculated length with min 8192 cap |
| Mark & sweep GC | Prevents memory leaks and use-after-free |

### Test coverage

- `tests/security/test_security.nc`: buffer overflow (long strings, nested lists), large payloads (large list, many map keys)

---

## 11. Docker Security

**Status: MITIGATED**  
**Files:** `Dockerfile`, `Dockerfile.slim`

| Measure | Production (`Dockerfile`) | Slim (`Dockerfile.slim`) |
|---------|---------------------------|--------------------------|
| Base image | Alpine 3.21 (minimal) | Debian bookworm-slim |
| Non-root user | `nc` user/group | `nc` user/group |
| `USER nc` directive | Yes | Yes |
| HEALTHCHECK | Yes | Yes |
| Multi-stage build | Yes (builder + runtime) | Yes |
| Build flags | `-Wall -Wextra -O2 -DNDEBUG` | `-Wall -Wextra -O2 -DNDEBUG` |
| Minimal runtime deps | libcurl, ca-certificates | libcurl, ca-certificates |

**Note:** `Dockerfile.dev` runs as root for development convenience and is not intended for production.

---

## 12. Supply Chain & Package Manager

**Status: MITIGATED**  
**File:** `engine/src/nc_pkg.c`

| Measure | Detail |
|---------|--------|
| Package name validation | `is_safe_pkg_name()` — only `[a-zA-Z0-9\-_./:@]` |
| No `system()` calls | All subprocess execution uses `execvp` / `CreateProcess` |
| Path traversal in names | `..` rejected |
| `.gitignore` protection | `.nc_packages/` ignored by default |

---

## 13. Security Test Coverage

### Unit & Language Tests

| Suite | Count | Location |
|-------|-------|----------|
| C unit test assertions | 485 | `nc/tests/test_nc.c` |
| Language test files | 85 (947 behaviors) | `tests/lang/*.nc` |
| Python basic validation | 67 tests | `tests/validate_nc.py` |
| Python deep integration | 45 tests | `tests/validate_nc_deep.py` |
| Security behavior tests | 12 categories | `tests/security/test_security.nc` |
| Windows PowerShell runner | 6 tests | `tests/run_tests.ps1` |
| Docker Linux validation | Full suite | `Dockerfile.test` |

### Penetration Test Suite

`tests/security/test_pentest.sh` — automated penetration test with 27 checks:

| # | Category | Vectors |
|---|----------|---------|
| 1 | Path traversal | 7 vectors (relative, absolute, URL-encoded, proc/sys) |
| 2 | Exec/shell sandbox | 6 vectors (command injection, pipe, backtick, allow-list) |
| 3 | Write sandbox | 2 vectors (default deny, explicit allow) |
| 4 | Security suite | Runs `test_security.nc` |
| 5 | Conformance | Runs `nc test` |
| 6 | Static analysis | 12 checks (hardcoded secrets, CORS, path reflection, log redaction, etc.) |

### Security behavior test categories (`test_security.nc`)

| Category | What it tests |
|----------|---------------|
| Buffer overflow | Long strings, deeply nested lists |
| Path traversal | `../../etc/passwd`, absolute paths, null-byte bypass |
| Template injection | `{{}}` in user input, literal template strings |
| SQL-style injection | Special characters in store/gather operations |
| Bounded loops | `repeat` with limits, guarded `while` |
| Bounded recursion | Shallow and bounded recursive calls |
| Large payloads | Oversized lists, excessive map keys |
| Type confusion | String-as-number, `nothing` values, wrong argument types |

---

## 14. Known Limitations

| Item | Status | Mitigation |
|------|--------|------------|
| No fuzzing harness | Not yet implemented | Planned: AFL/libFuzzer integration for lexer and parser |
| Tensor engine stubs | CUDA/Metal stubs present, not production | No security impact — stubs do not execute GPU code |
| `Dockerfile.dev` runs as root | By design for development | Not used in production; production images use `USER nc` |
| `nc digest` loop conversion | `for` loops emit TODO comments | Use `nc migrate` for AI-powered conversion |
| JWT validation is basic | Checks expiry and signature presence | Full JWKS/RS256 validation planned for v1.1 |

---

## 15. Compliance Matrix

### OWASP Top 10 (2021) Coverage

| # | Risk | NC Mitigation | Status |
|---|------|---------------|--------|
| A01 | Broken Access Control | Sandbox default-deny, Bearer/API key auth, role-based access | Mitigated |
| A02 | Cryptographic Failures | Constant-time key comparison, secrets via env vars only | Mitigated |
| A03 | Injection | Shell escaping, JSON escaping, embed escaping, template escaping, path validation | Mitigated |
| A04 | Insecure Design | Default-deny sandbox, no `system()`, `safe_run()` | Mitigated |
| A05 | Security Misconfiguration | No default CORS `*`, explicit env var overrides, secure defaults | Mitigated |
| A06 | Vulnerable Components | Minimal dependencies (libc, libcurl, pthreads), no npm/pip supply chain | Mitigated |
| A07 | Auth Failures | JWT expiry checks, constant-time comparison, 401/403 responses | Mitigated |
| A08 | Data Integrity Failures | Package name validation, no `system()` calls, `execvp` only | Mitigated |
| A09 | Logging & Monitoring | `nc_redact_secret()`, structured JSON logs, OTEL trace export, audit logging | Mitigated |
| A10 | SSRF | `NC_HTTP_ALLOWLIST` for outbound HTTP, URL validation in `gather` | Mitigated |

---

## Certificate Statement

This document certifies that **NC v1.0.0** has undergone internal security review covering path traversal, injection, authentication, authorization, rate limiting, secret protection, buffer safety, container security, and supply chain integrity.

All identified vulnerabilities have been mitigated with corresponding test coverage. The runtime follows a **default-deny** security model where dangerous operations (exec, file write, network) must be explicitly enabled.

**Issued:** March 11, 2026  
**Valid for:** NC v1.0.0  
**Next review:** Prior to v1.1.0 release  

---

<p align="center">
  <em>For vulnerability reports, contact <a href="mailto:support@devheallabs.in">support@devheallabs.in</a></em><br>
  <em>See <a href="SECURITY.md">SECURITY.md</a> for the disclosure process</em>
</p>
