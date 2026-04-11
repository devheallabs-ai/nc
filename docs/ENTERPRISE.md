# NC Enterprise Features Guide

This document covers all enterprise-grade features added to the NC engine.

## Table of Contents

- [Authentication & Security](#authentication--security)
- [Cryptographic Functions](#cryptographic-functions)
- [Session Management](#session-management)
- [Request Context](#request-context)
- [Feature Flags](#feature-flags)
- [Circuit Breaker](#circuit-breaker)
- [Audit Logging](#audit-logging)
- [API Versioning](#api-versioning)
- [Higher-Order List Functions](#higher-order-list-functions)
- [TLS / HTTPS](#tls--https)
- [Observability](#observability)
- [Configuration Reference](#configuration-reference)

---

## Authentication & Security

### JWT Authentication (HS256)

NC has built-in JWT support. No external libraries needed.

```nc
configure:
    auth: "bearer"

to login with username, password:
    set stored to hash_password(password)
    set token to jwt_generate(username, "admin", 3600)
    respond with {"token": token}

to protected_route:
    set token to request_header("Authorization")
    set claims to jwt_verify(token)
    if claims is equal to false:
        respond with {"error": "unauthorized", "_status": 401}
    respond with {"user": claims.sub}
```

**Environment variables:**
- `NC_JWT_SECRET` — signing key (required)
- `NC_JWT_USER_CLAIM` — claim for user ID (default: `sub`)
- `NC_JWT_ROLE_CLAIM` — claim for roles (default: `roles`)

### API Key Authentication

```nc
middleware:
    auth:
        type: "bearer"
```

Set `NC_API_KEYS=key1,key2,key3` for comma-separated valid keys.

### OIDC / SSO

```
NC_OIDC_ISSUER=https://your-idp.example.com
NC_OIDC_CLIENT_ID=your-client-id
NC_OIDC_SCOPES=openid profile email
```

---

## Cryptographic Functions

All implemented in pure C — zero external dependencies, cross-platform.

| Function | Description | Example |
|----------|-------------|---------|
| `hash_sha256(data)` | SHA-256 hex digest | `hash_sha256("hello")` → 64-char hex |
| `hash_password(pw)` | Salted + 10K iterations | `hash_password("secret")` → `$nc$...` |
| `verify_password(pw, hash)` | Constant-time verify | `verify_password("secret", stored)` → true/false |
| `hash_hmac(data, key)` | HMAC-SHA256 | `hash_hmac("msg", "key")` → 64-char hex |

### Password Storage Pattern

```nc
to register with username, password:
    set password_hash to hash_password(password)
    store {"username": username, "hash": password_hash} into "users/" + username
    respond with {"status": "registered"}

to login with username, password:
    gather user from "users/" + username
    if verify_password(password, user.hash):
        set token to jwt_generate(username, "user", 3600)
        respond with {"token": token}
    respond with {"error": "invalid credentials", "_status": 401}
```

---

## Session Management

Server-side sessions with automatic expiry.

```nc
to login with username, password:
    set sid to session_create()
    session_set(sid, "user", username)
    session_set(sid, "role", "admin")
    respond with {"session_id": sid}

to dashboard with session_id:
    if not session_exists(session_id):
        respond with {"error": "session expired", "_status": 401}
    set user to session_get(session_id, "user")
    respond with {"welcome": user}

to logout with session_id:
    session_destroy(session_id)
    respond with {"status": "logged out"}
```

**Configuration:** `NC_SESSION_TTL=3600` (seconds, default 1 hour)

---

## Request Context

Access HTTP request details inside any behavior:

```nc
to process_request:
    set auth to request_header("Authorization")
    set ip to request_ip()
    set method to request_method()
    set path to request_path()
    set all_headers to request_headers()

    log "Request from " + ip + ": " + method + " " + path
    respond with {"client_ip": ip}
```

| Function | Returns |
|----------|---------|
| `request_header("Name")` | Header value or nothing |
| `request_headers()` | Map of all headers |
| `request_ip()` | Client IP address |
| `request_method()` | HTTP method |
| `request_path()` | Request path |

---

## Feature Flags

Runtime toggles without redeployment.

```nc
to get_dashboard:
    if feature("new_ui"):
        respond with new_dashboard()
    otherwise:
        respond with old_dashboard()
```

**Configuration:**
- `NC_FF_NEW_UI=1` — enable flag
- `NC_FF_BETA_API=50` — 50% rollout
- `NC_FF_DARK_MODE=0` — disable flag

---

## Circuit Breaker

Protect against cascading failures from downstream services.

```nc
to call_external_api:
    if circuit_open("external_api"):
        respond with {"error": "service temporarily unavailable", "_status": 503}
    try:
        gather result from "https://api.example.com/data"
        respond with result
    otherwise:
        respond with {"error": "external service failed", "_status": 502}
```

**Configuration:**
- `NC_CB_FAILURE_THRESHOLD=5` — failures before opening
- `NC_CB_TIMEOUT=30` — seconds before retrying
- `NC_CB_SUCCESS_THRESHOLD=3` — successes to close

---

## Audit Logging

SOC 2 / HIPAA compliant audit trail.

```
NC_AUDIT_FORMAT=json
NC_AUDIT_FILE=/var/log/nc/audit.jsonl
```

Events are automatically logged for auth, access control, and rate limiting.
All entries include timestamp, user, action, IP, tenant_id, and trace_id.

---

## API Versioning

URL-based version routing.

```
NC_API_VERSIONS=v1,v2
NC_API_DEFAULT_VERSION=v1
NC_API_V1_SUNSET=2025-12-31
```

Routes: `/api/v1/users`, `/api/v2/users`, `/v1/users`

---

## Higher-Order List Functions

```nc
set strategies to [{...}, {...}, {...}]

set sorted to sort_by(strategies, "fitness")
set best to max_by(strategies, "fitness")
set worst to min_by(strategies, "fitness")
set total_pnl to sum_by(strategies, "pnl")
set names to map_field(strategies, "name")
set winners to filter_by(strategies, "fitness", "above", 0)

set highest to max([3, 7, 1, 9])
set lowest to min([3, 7, 1, 9])
set precise to round(3.14159, 2)
```

| Function | Description |
|----------|-------------|
| `sort_by(list, "field")` | Sort maps by field |
| `max_by(list, "field")` | Map with highest field value |
| `min_by(list, "field")` | Map with lowest field value |
| `sum_by(list, "field")` | Sum numeric field |
| `map_field(list, "field")` | Extract field into list |
| `filter_by(list, "field", "op", val)` | Filter by comparison |
| `max(list)` | Maximum value in list |
| `min(list)` | Minimum value in list |
| `round(val, decimals)` | Round to N decimal places |

---

## TLS / HTTPS

```nc
configure:
    tls_cert: "/path/to/cert.pem"
    tls_key: "/path/to/key.pem"
    port: 8443
```

Or via environment: `NC_TLS_CERT`, `NC_TLS_KEY`

---

## Observability

### Prometheus Metrics

Built-in at `/metrics`:
- `nc_requests_total` — total HTTP requests
- `nc_errors_total` — total errors
- `nc_active_connections` — current connections
- `nc_active_workers` — busy worker threads
- `nc_queue_depth` — current request queue depth
- `nc_keepalive_reuse` — total keep-alive connection reuses
- `nc_queue_full_rejects` — requests rejected due to full queue
- `nc_requests_latency_sum` — cumulative request latency (seconds)
- `nc_info` — runtime version info

### Health Endpoints

- `/health` — liveness check with stats
- `/ready` — readiness probe (checks worker capacity)
- `/live` — simple alive check
- `/openapi.json` — auto-generated API specification

### Structured Logging

```
NC_LOG_FORMAT=json
NC_LOG_LEVEL=info
```

### Distributed Tracing

```
NC_OTEL_ENDPOINT=http://collector:4318
```

Propagates `traceparent` headers automatically.

---

## Production Hardening

These features bring NC's HTTP server to production grade, comparable to
Uvicorn (Python), Go net/http, or Node.js cluster mode.

### Thread Pool with Bounded Request Queue

NC uses a **persistent thread pool** (not thread-per-request). Workers are
created at startup and reused across requests. When all workers are busy,
incoming connections are held in a bounded queue with configurable timeout.

```bash
# 64 persistent worker threads, 4096-slot request queue
export NC_MAX_WORKERS=64
export NC_REQUEST_QUEUE_SIZE=4096
export NC_QUEUE_TIMEOUT=5000   # ms to wait in queue before 503
```

If the queue is full, the server returns **503 Service Unavailable** with a
`Retry-After: 5` header, enabling load balancers to retry elsewhere.

### HTTP/1.1 Keep-Alive

Connections are reused by default. After processing a request, the worker
reads additional requests on the same TCP connection until the keep-alive
limit or timeout is reached.

```bash
export NC_KEEPALIVE_MAX=100     # max requests per connection
export NC_KEEPALIVE_TIMEOUT=30  # idle timeout in seconds
```

Response headers include:
```
Connection: keep-alive
Keep-Alive: timeout=30, max=100
```

### I/O Multiplexing

The accept loop uses the best available system call:
- **epoll** on Linux (no fd limit)
- **kqueue** on macOS/BSD (no fd limit)
- **select()** fallback (1024 fd limit)

### Chunked Transfer Encoding

Large responses (> 64 KB by default) are sent using HTTP/1.1 chunked
transfer encoding, reducing memory pressure and enabling streaming.

```bash
export NC_CHUNK_THRESHOLD=65536  # bytes, switch to chunked above this
export NC_CHUNK_SIZE=16384       # bytes per chunk
```

### Outbound Connection Pool

Outbound HTTP requests (to AI providers, webhooks, etc.) reuse connections
from a shared pool of 32 handles. Each handle maintains its own TCP
connection cache with keep-alive and TLS session reuse.

```bash
export NC_HTTP_POOL_CONNECTIONS=8  # connections cached per handle
```

### Sliding Window Rate Limiting

Rate limits use a **sliding window** algorithm (not fixed windows). This
eliminates the 2x burst problem at window boundaries that fixed-window
counters have.

```bash
export NC_RATE_LIMIT_RPM=100      # requests per window
export NC_RATE_LIMIT_WINDOW=60    # window duration in seconds
```

### TCP Listen Backlog

```bash
export NC_LISTEN_BACKLOG=512  # kernel TCP backlog queue depth
```

### Graceful Shutdown

On `SIGINT`/`SIGTERM`, the server:
1. Stops accepting new connections
2. Wakes all queued threads
3. Waits up to `NC_DRAIN_TIMEOUT` seconds for in-flight requests
4. Joins all thread pool workers
5. Exits cleanly

```bash
export NC_DRAIN_TIMEOUT=30  # seconds to drain in-flight requests
```

### Security Headers

Every response includes enterprise security headers:
```
X-Content-Type-Options: nosniff
X-Frame-Options: DENY
X-XSS-Protection: 1; mode=block
Referrer-Policy: strict-origin-when-cross-origin
Permissions-Policy: camera=(), microphone=(), geolocation=()
Strict-Transport-Security: max-age=31536000; includeSubDomains
```

---

## Configuration Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `NC_JWT_SECRET` | — | JWT signing secret |
| `NC_API_KEYS` | — | Comma-separated valid API keys |
| `NC_OIDC_ISSUER` | — | OIDC identity provider URL |
| `NC_CORS_ORIGIN` | — | Allowed CORS origin |
| `NC_RATE_LIMIT_RPM` | 100 | Requests per minute |
| `NC_RATE_LIMIT_WINDOW` | 60 | Sliding window duration (seconds) |
| `NC_LOG_FORMAT` | text | `text` or `json` |
| `NC_LOG_LEVEL` | info | `debug`, `info`, `warn`, `error` |
| `NC_AUDIT_FORMAT` | — | Set to `json` to enable |
| `NC_AUDIT_FILE` | — | Path for audit log file |
| `NC_SESSION_TTL` | 3600 | Session timeout (seconds) |
| `NC_MAX_LOOP_ITERATIONS` | 10000000 | Max loop iterations |
| `NC_MAX_WORKERS` | 64 | Thread pool size (persistent workers) |
| `NC_LISTEN_BACKLOG` | 512 | TCP listen backlog queue depth |
| `NC_REQUEST_QUEUE_SIZE` | 4096 | Bounded request queue capacity |
| `NC_KEEPALIVE_MAX` | 100 | Max requests per keep-alive connection |
| `NC_KEEPALIVE_TIMEOUT` | 30 | Keep-alive idle timeout (seconds) |
| `NC_QUEUE_TIMEOUT` | 5000 | Request queue wait timeout (ms) |
| `NC_DRAIN_TIMEOUT` | 30 | Graceful shutdown timeout (seconds) |
| `NC_CHUNK_THRESHOLD` | 65536 | Body size threshold for chunked transfer (bytes) |
| `NC_CHUNK_SIZE` | 16384 | Chunk size for chunked transfer (bytes) |
| `NC_HTTP_POOL_CONNECTIONS` | 8 | Outbound connection pool size per handle |
| `NC_TLS_CERT` | — | TLS certificate path |
| `NC_TLS_KEY` | — | TLS private key path |
| `NC_HTTP_COOKIES` | 0 | Enable cookie engine (`1`) |
| `NC_CB_FAILURE_THRESHOLD` | 5 | Circuit breaker failure threshold |
| `NC_CB_TIMEOUT` | 30 | Circuit breaker timeout (seconds) |
| `NC_FF_<NAME>` | — | Feature flag (`1`/`0`/percentage) |
| `NC_API_VERSIONS` | — | Comma-separated API versions |
| `NC_SECRET_SOURCE` | env | `env`, `file`, or `vault` |
| `NC_OTEL_ENDPOINT` | — | Tracing collector URL |

---

## Test Results

```
Conformance:  204/204 passed
Unit Tests:   411/412 passed (1 pre-existing)
New Features: 59/59 passed
VM Safety:    34/34 passed
HTTP Live:    11/11 endpoints verified via nc serve
```

All enterprise features have tests at four levels:
1. **C unit tests** (`make test-unit`) — direct function testing with assertions
2. **Conformance tests** (`nc conformance`) — NC code execution with expected output
3. **NC language test files** (`nc validate`) — full .nc program validation
4. **HTTP live tests** (`nc serve` + curl) — real server endpoint verification

Every function is registered in all 3 execution paths (interpreter, VM, JIT).

---

## v1.0.0 Language Features (Enterprise AI)

### Typed Error Catching

Catch specific error categories instead of generic errors:

```nc
try:
    gather data from "https://api.example.com/data"
catch "TimeoutError":
    respond with {"error": "timeout", "type": err.type, "line": err.line}
catch "ConnectionError":
    respond with {"error": "connection_failed", "message": err.message}
on_error:
    respond with {"error": error}
```

**7 error types:** `TimeoutError`, `ConnectionError`, `ParseError`, `IndexError`, `ValueError`, `IOError`, `KeyError`

### Assert Statements

Production-grade assertions with line numbers and stack traces:

```nc
assert len(users) is above 0, "Users list must not be empty"
assert response["status"] is equal 200, "Expected 200 OK"
```

### Built-in Test Framework

Test blocks run in isolation — one test failure doesn't kill the program:

```nc
test "user creation":
    set user to create_user("Alice", "alice@example.com")
    assert user["name"] is equal "Alice"

test "API health":
    set r to http_get("http://localhost:8080/health")
    assert r["status"] is equal "healthy"
```

### String Formatting

Python-style `format()` with positional and named placeholders:

```nc
set msg to format("Hello {}, you have {} items", "Alice", 5)
set welcome to format("Welcome {name}, role: {role}", {"name": "Bob", "role": "admin"})
```

### Rich Data Structures

| Function | Description |
|----------|-------------|
| `set_new()` / `set_add()` / `set_has()` | Set (unique values backed by map) |
| `tuple()` | Immutable-style sequence |
| `deque()` / `deque_push_front()` / `deque_pop_front()` | Double-ended queue |
| `counter()` | Count occurrences |
| `default_map()` | Map with default value |
| `enumerate()` / `zip()` | Functional list utilities |
| `error_type()` / `traceback()` | Error classification and stack inspection |

### Async & Streaming Syntax

```nc
set result to await http_get("https://api.example.com/data")
yield "Processing step 1..."
yield "Processing step 2..."
respond with result
```

---

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)** | support@devheallabs.in
