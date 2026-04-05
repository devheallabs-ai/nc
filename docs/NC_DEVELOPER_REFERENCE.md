# NC Developer Reference — Complete Built-in Function Guide

> **Version:** 1.1.0 | **Test Status:** 201/201 conformance, 411/412 unit tests
>
> Every function in this document is verified working in the NC engine.
> Each entry includes the exact function name, argument count, return type,
> and a runnable NC code example.

---

## Table of Contents

1. [Service Structure](#1-service-structure)
2. [Variables & Types](#2-variables--types)
3. [Control Flow](#3-control-flow)
4. [String Functions](#4-string-functions)
5. [Math Functions](#5-math-functions)
6. [List Functions](#6-list-functions)
7. [Map / Record Functions](#7-map--record-functions)
8. [Higher-Order List Functions](#8-higher-order-list-functions)
9. [File I/O](#9-file-io)
10. [HTTP Client](#10-http-client)
11. [JSON](#11-json)
12. [Time & Date](#12-time--date)
13. [Cryptography](#13-cryptography)
14. [Authentication (JWT)](#14-authentication-jwt)
15. [Session Management](#15-session-management)
16. [Request Context](#16-request-context)
17. [Feature Flags](#17-feature-flags)
18. [Circuit Breaker](#18-circuit-breaker)
19. [Caching](#19-caching)
20. [Shell & Process Execution](#20-shell--process-execution)
21. [AI Integration](#21-ai-integration)
22. [ML Model Integration](#22-ml-model-integration)
23. [Data Format Parsers](#23-data-format-parsers)
24. [WebSocket](#24-websocket)
25. [Conversation Memory](#25-conversation-memory)
26. [Type Checking](#26-type-checking)
27. [Regex](#27-regex)
28. [Environment Variables](#28-environment-variables)
29. [Error Handling](#29-error-handling)
30. [API Server](#30-api-server)
31. [Middleware](#31-middleware)
32. [Scheduled Tasks](#32-scheduled-tasks)
33. [Configuration Reference](#33-configuration-reference)
34. [Keyword Synonyms](#34-keyword-synonyms)

---

## 1. Service Structure

Every NC file follows this structure:

```nc
service "my-app"
version "1.0.0"

configure:
    port: 8080
    cors_origin: "https://myapp.example.com"
    auth: "bearer"
    rate_limit: 100
    log_requests: true

to health_check:
    respond with {"status": "healthy"}

to greet with name:
    respond with "Hello, " + name + "!"

api:
    GET /health runs health_check
    POST /greet runs greet
```

**Keywords available:** `service`, `version`, `configure`, `to`, `with`, `api`,
`import`, `module`, `define`, `middleware`, `on event`, `every`

---

## 2. Variables & Types

```nc
to examples:
    set name to "Alice"                  // string
    set age to 30                        // integer
    set pi to 3.14159                    // float
    set active to true                   // boolean
    set items to [1, 2, 3]              // list
    set user to {"name": "Bob", "age": 25}  // map/record
    set empty to nothing                 // none/null

    // Type conversion
    set s to str(42)                     // "42"
    set n to int("42")                   // 42
    set f to float("3.14")              // 3.14
    set t to type(name)                  // "string"
    set l to len(items)                  // 3
```

| Function | Args | Returns | Description |
|----------|------|---------|-------------|
| `str(val)` | 1 | string | Convert any value to string |
| `int(val)` | 1 | integer | Convert to integer |
| `float(val)` | 1 | float | Convert to float |
| `type(val)` | 1 | string | Type name: "string", "integer", "float", "boolean", "list", "map", "none" |
| `len(val)` | 1 | integer | Length of string, list, or map |

---

## 3. Control Flow

### If / Otherwise / Otherwise If

```nc
to check_age with age:
    if age is above 18:
        respond with "adult"
    otherwise:
        respond with "minor"
```

**Chained conditions** with `otherwise if` (works like `elif` / `else if`):

```nc
to classify with score:
    if score is above 90:
        respond with "excellent"
    otherwise if score is above 70:
        respond with "good"
    otherwise if score is above 50:
        respond with "average"
    otherwise:
        respond with "needs improvement"
```

`otherwise if` chains are true else-if branching — only the first matching branch
executes, and `respond with` inside any branch exits the function immediately.

### Comparisons

| Syntax | Meaning |
|--------|---------|
| `is equal to` | == |
| `is not equal to` | != |
| `is above` | > |
| `is below` | < |
| `is at least` | >= |
| `is at most` | <= |
| `is in` | membership test |
| `is not in` | non-membership |

### Logic

```nc
if age is above 18 and active:
    log "eligible"
if score is above 90 or is_vip:
    log "premium"
if not blocked:
    log "allowed"
```

### Match / When

```nc
to classify with grade:
    match grade:
        when "A":
            respond with "excellent"
        when "B":
            respond with "good"
        otherwise:
            respond with "other"
```

### Repeat Loops

```nc
to process_items with items:
    set total to 0
    repeat for each item in items:
        set total to total + item
    respond with total

// With stop (break) and skip (continue)
to find_first with items, target:
    repeat for each item in items:
        if item is equal to target:
            respond with item
            stop
        if item is below 0:
            skip
```

### While Loop

```nc
to countdown with n:
    set count to n
    while count is above 0:
        log count
        set count to count - 1
    respond with "done"
```

### Repeat While

`repeat while` is a synonym for `while` — use whichever reads more naturally:

```nc
to countdown with n:
    set count to n
    repeat while count is above 0:
        log count
        set count to count - 1
    respond with "done"
```

Both forms produce identical behavior and can be mixed freely with `repeat for each` and `repeat N times` in the same behavior.

### Range

```nc
to demo_range:
    repeat for each i in range(5):          // 0,1,2,3,4
        log i
    repeat for each i in range(1, 10):      // 1..9
        log i
    repeat for each i in range(0, 100, 10): // 0,10,20..90
        log i
```

---

## 4. String Functions

```nc
to string_demo:
    set s to "  Hello World  "

    set u to upper(s)                    // "  HELLO WORLD  "
    set l to lower(s)                    // "  hello world  "
    set t to trim(s)                     // "Hello World"

    set parts to split("a,b,c", ",")     // ["a", "b", "c"]
    set joined to join(parts, " - ")     // "a - b - c"

    set has to contains("hello", "ell")  // true
    set sw to starts_with("hello", "he") // true
    set ew to ends_with("hello", "lo")   // true

    set r to replace("hello", "l", "r")  // "herro"

    // String templates
    set name to "NC"
    set msg to "Welcome to {{name}}!"    // "Welcome to NC!"

    // Triple-quote multi-line strings
    set long_text to """
    This is a multi-line
    string that preserves
    line breaks.
    """
```

| Function | Args | Returns | Description |
|----------|------|---------|-------------|
| `upper(s)` | 1 | string | Uppercase |
| `lower(s)` | 1 | string | Lowercase |
| `trim(s)` | 1 | string | Strip whitespace |
| `split(s, delim)` | 2 | list | Split by delimiter |
| `join(list, sep)` | 2 | string | Join with separator |
| `contains(s, sub)` | 2 | boolean | Substring check |
| `starts_with(s, prefix)` | 2 | boolean | Prefix check |
| `ends_with(s, suffix)` | 2 | boolean | Suffix check |
| `replace(s, old, new)` | 3 | string | Replace all occurrences |

---

## 5. Math Functions

```nc
to math_demo:
    set a to abs(-5)                     // 5
    set d to abs(98.0 - 100.0)            // 2.0 (works on expressions)
    set r to sqrt(16)                    // 4.0
    set p to pow(2, 10)                  // 1024.0
    set c to ceil(3.2)                   // 4
    set f to floor(3.8)                  // 3

    // round — 1 arg returns integer, 2 args returns float
    set r1 to round(3.7)                 // 4 (integer)
    set r2 to round(3.14159, 2)          // 3.14 (float!)

    set mn to min(3, 7)                  // 3
    set mx to max(3, 7)                  // 7

    // IMPORTANT: min/max also accept a list!
    set lowest to min([5, 2, 8, 1, 9])   // 1
    set highest to max([5, 2, 8, 1, 9])  // 9

    set rnd to random()                  // 0.0 to 1.0
    set lg to log(100)                   // natural log
    set ex to exp(1)                     // 2.71828...
```

| Function | Args | Returns | Description |
|----------|------|---------|-------------|
| `abs(x)` | 1 | number | Absolute value (works on expressions: `abs(a - b)`) |
| `sqrt(x)` | 1 | float | Square root |
| `pow(base, exp)` | 2 | float | Exponentiation |
| `round(x)` | 1 | **integer** | Round to nearest integer |
| `round(x, decimals)` | 2 | **float** | Round to N decimal places |
| `ceil(x)` | 1 | integer | Ceiling |
| `floor(x)` | 1 | integer | Floor |
| `min(a, b)` | 2 | number | Minimum of two values |
| `min(list)` | 1 | number | Minimum value in list |
| `max(a, b)` | 2 | number | Maximum of two values |
| `max(list)` | 1 | number | Maximum value in list |
| `random()` | 0 | float | Random float [0, 1) |
| `log(x)` | 1 | float | Natural logarithm |
| `exp(x)` | 1 | float | e^x |
| `sin(x)` / `cos(x)` / `tan(x)` | 1 | float | Trigonometry |

---

## 6. List Functions

```nc
to list_demo:
    set items to [3, 1, 4, 1, 5, 9]

    set length to len(items)             // 6
    set sorted to sort(items)            // [1, 1, 3, 4, 5, 9]
    set rev to reverse(items)            // [9, 5, 1, 4, 1, 3]
    set uniq to unique(items)            // [3, 1, 4, 5, 9]
    set flat to flatten([[1,2],[3,4]])    // [1, 2, 3, 4]

    set f to first(items)               // 3
    set l to last(items)                // 9
    set s to sum(items)                 // 23
    set avg to average(items)           // 3.833... (always returns float)

    // Works on inline lists too
    set sma to average([10.0, 20.0, 30.0])  // 20.0
    set idx to index_of(items, 4)       // 2
    set cnt to count(items, 1)          // 2

    // Slice — extract sub-list
    set sub to slice(items, 2, 5)       // [4, 1, 5]
    set tail to slice(items, 3)         // [1, 5, 9] (from index 3 to end)
    set last3 to slice(items, -3)       // [1, 5, 9] (last 3 elements)

    // Append and remove
    append(items, 99)
    remove(items, 1)

    // Check conditions
    set a to any([false, true, false])   // true
    set al to all([true, true, true])    // true

    // Enumerate and zip
    set enum to enumerate(["a","b","c"]) // [[0,"a"],[1,"b"],[2,"c"]]
    set zipped to zip([1,2], ["a","b"])  // [[1,"a"],[2,"b"]]

    // List concatenation with +
    set combined to [1, 2] + [3, 4]      // [1, 2, 3, 4]
    set extended to items + [99]          // append item

    // Build list in a loop (works correctly)
    set result to []
    repeat for each i in range(5):
        set result to result + [i]       // [0, 1, 2, 3, 4]
```

---

## 7. Map / Record Functions

```nc
to map_demo:
    set user to {"name": "Alice", "age": 30, "role": "admin"}

    set k to keys(user)                  // ["name", "age", "role"]
    set v to values(user)                // ["Alice", 30, "admin"]
    set has to has_key(user, "name")     // true

    // Access
    set name to user.name                // "Alice"
    set age to user["age"]               // 30
```

---

## 8. Higher-Order List Functions

These work on lists of maps (records/objects):

```nc
to advanced_list_demo:
    set strategies to [
        {"name": "alpha", "fitness": 0.85, "pnl": 1200},
        {"name": "beta", "fitness": 0.42, "pnl": -300},
        {"name": "gamma", "fitness": 0.91, "pnl": 800}
    ]

    // Sort by field (uses quicksort internally)
    set ranked to sort_by(strategies, "fitness")
    // ranked[0].name = "beta" (lowest), ranked[2].name = "gamma" (highest)

    // Also works: sort(strategies, "fitness") — same as sort_by
    set also_sorted to sort(strategies, "fitness")

    // Find best/worst by field
    set best to max_by(strategies, "fitness")     // {name:"gamma",...}
    set worst to min_by(strategies, "fitness")    // {name:"beta",...}

    // Sum a numeric field
    set total_pnl to sum_by(strategies, "pnl")    // 1700

    // Extract one field into a flat list
    set names to map_field(strategies, "name")     // ["alpha","beta","gamma"]

    // Filter by numeric comparison
    set winners to filter_by(strategies, "pnl", "above", 0)
    // winners = [alpha, gamma]

    set elites to filter_by(strategies, "fitness", "at_least", 0.8)
    // elites = [alpha, gamma]

    // Filter by string equality
    set results to [
        {"name": "test1", "status": "PASS"},
        {"name": "test2", "status": "FAIL"},
        {"name": "test3", "status": "PASS"}
    ]
    set passed to filter_by(results, "status", "equal", "PASS")
    // passed = [test1, test3]
```

| Function | Args | Returns | Description |
|----------|------|---------|-------------|
| `sort_by(list, "field")` | 2 | list | Sort maps by field (ascending) |
| `max_by(list, "field")` | 2 | map | Map with highest field value |
| `min_by(list, "field")` | 2 | map | Map with lowest field value |
| `sum_by(list, "field")` | 2 | float | Sum of field values |
| `map_field(list, "field")` | 2 | list | Extract field into list |
| `filter_by(list, "field", "op", val)` | 4 | list | Filter by comparison (numeric or string) |

**`filter_by` operators:** `"above"` / `">"`, `"below"` / `"<"`, `"equal"` / `"=="`, `"at_least"` / `">="`, `"at_most"` / `"<="`

> **String support:** When the value argument is a string (e.g., `"PASS"`), `filter_by` compares field values as strings. When it's a number, comparison is numeric.

---

## 9. File I/O

```nc
to file_demo:
    // Write
    write_file("/tmp/data.txt", "hello world")

    // Atomic write (safe against crashes)
    write_file_atomic("/tmp/config.json", json_encode(config))

    // Read
    set content to read_file("/tmp/data.txt")      // "hello world"

    // Check & delete
    if file_exists("/tmp/data.txt"):
        delete_file("/tmp/data.txt")

    // Create directory
    mkdir("/tmp/my_app/data")
```

> **Security:** File access is sandboxed. Set `NC_ALLOW_FILE_WRITE=1` to enable writes.

---

## 10. HTTP Client

```nc
to http_demo:
    // Simple GET
    set response to http_get("https://api.example.com/data")

    // GET with headers
    set response to http_get("https://api.example.com/data",
        {"Authorization": "Bearer " + token})

    // POST with body
    set result to http_post("https://api.example.com/submit",
        {"key": "value"})

    // GET with custom headers (avoids rate limiting)
    set data to http_get("https://api.example.com/data",
        {"User-Agent": "MyApp/1.0", "Accept": "application/json"})

    // Generic request
    set result to http_request("PUT", "https://api.example.com/item/1",
        {"Content-Type": "application/json"},
        {"name": "updated"})
```

A default `User-Agent: NC/1.0 (compatible)` is sent automatically when none is provided.
Override globally with `NC_HTTP_USER_AGENT` env var, or per-request via headers map.

> **Cookie support:** Set `NC_HTTP_COOKIES=1` to enable session cookies.

---

## 11. JSON

```nc
to json_demo:
    set data to {"name": "Alice", "scores": [95, 87, 92]}

    // Encode to JSON string
    set json_str to json_encode(data)
    // '{"name":"Alice","scores":[95,87,92]}'

    // Decode from JSON string
    set parsed to json_decode(json_str)
    log parsed.name    // "Alice"

    // Safe: if input is already parsed (list/map/etc), returns as-is
    set already_list to [1, 2, 3]
    set same to json_decode(already_list)    // returns [1, 2, 3]

    // Error handling: invalid JSON triggers try/on_error
    try:
        set bad to json_decode("invalid{{{")
    on_error:
        set bad to {"error": "parse failed"}
```

---

## 12. Time & Date

```nc
to time_demo:
    set now to time_now()                          // 1710410400 (unix)
    set ms to time_ms()                            // 1710410400123.456

    set iso to time_iso()                          // "2026-03-14T09:00:00Z"
    set iso2 to time_iso(1710410400)               // specific timestamp

    set formatted to time_format(now, "%Y-%m-%d")  // "2026-03-14"
    set default_fmt to time_format(now)            // "2026-03-14 09:00:00"
```

### Wait

```nc
to rate_limited_call:
    gather data from "https://api.example.com/data"
    wait 500 milliseconds       // ← now supported!
    wait 2 seconds
    wait 1 minute
```

**Supported units:** `milliseconds` / `millisecond` / `ms`, `seconds` / `second`, `minutes` / `minute`, `hours` / `hour`, `days` / `day`

---

## 13. Cryptography

All pure C, zero external dependencies.

```nc
to crypto_demo:
    // SHA-256 digest (64-char hex string)
    set digest to hash_sha256("hello world")

    // Password hashing (salted, 10K iterations)
    set stored_hash to hash_password("user_password")
    // Returns: "$nc$<salt>$<hash>"

    // Password verification (constant-time)
    if verify_password("user_password", stored_hash):
        log "password correct"

    // HMAC-SHA256 (for webhook signatures, API auth)
    set signature to hash_hmac("request_body", "webhook_secret")
```

---

## 14. Authentication (JWT)

```nc
to create_token with user_id, role:
    // jwt_generate(user, role, expiry_seconds, extra_claims)
    set token to jwt_generate(user_id, role, 3600, {"org": "acme"})
    respond with {"token": token}

to verify_token:
    set auth to request_header("Authorization")
    if auth is equal to nothing:
        respond with {"error": "no token", "_status": 401}

    // Strip "Bearer " prefix
    set token to replace(auth, "Bearer ", "")

    // jwt_verify returns claims map or false
    set claims to jwt_verify(token)
    if claims is equal to false:
        respond with {"error": "invalid token", "_status": 401}

    respond with {"user": claims.sub, "role": claims.role}

    // Optionally pass the secret as second arg
    // set claims to jwt_verify(token, "my-secret-key")
```

**Required env:** `NC_JWT_SECRET` (for HS256 signing)

---

## 15. Session Management

```nc
service "session-demo"
version "1.0.0"

to login with username, password:
    // Verify credentials
    set stored to hash_password(password)
    // ... verify against stored hash ...

    // Create session
    set sid to session_create()
    session_set(sid, "user", username)
    session_set(sid, "role", "admin")
    session_set(sid, "login_time", time_iso())

    respond with {"session_id": sid}

to dashboard with session_id:
    if not session_exists(session_id):
        respond with {"error": "session expired", "_status": 401}

    set user to session_get(session_id, "user")
    set role to session_get(session_id, "role")
    respond with {"welcome": user, "role": role}

to logout with session_id:
    session_destroy(session_id)
    respond with {"status": "logged out"}

api:
    POST /login runs login
    GET /dashboard runs dashboard
    POST /logout runs logout
```

**Config:** `NC_SESSION_TTL=3600` (auto-expiry in seconds)

---

## 16. Request Context

Access the current HTTP request from any behavior:

```nc
to protected_endpoint:
    // Read any header
    set auth to request_header("Authorization")
    set content_type to request_header("Content-Type")
    set user_agent to request_header("User-Agent")

    // Get all headers as a map
    set all to request_headers()

    // Client info
    set ip to request_ip()
    set method to request_method()        // "GET", "POST", etc.
    set path to request_path()            // "/api/data"

    log method + " " + path + " from " + ip
```

---

## 17. Feature Flags

```nc
to get_ui:
    if feature("new_dashboard"):
        respond with new_dashboard_html()
    otherwise:
        respond with old_dashboard_html()

to get_feature_for_tenant with tenant_id:
    // Tenant-specific flag check
    if feature("beta_api", tenant_id):
        respond with beta_response()
```

**Config:**
```
NC_FF_NEW_DASHBOARD=1           # on
NC_FF_BETA_API=50               # 50% rollout
NC_FF_OLD_FEATURE=0             # off
```

---

## 18. Circuit Breaker

```nc
to call_downstream:
    if circuit_open("payment_service"):
        respond with {"error": "service temporarily unavailable", "_status": 503}

    try:
        gather result from "https://payment.internal/charge"
        respond with result
    otherwise:
        respond with {"error": "payment service failed", "_status": 502}
```

**Config:** `NC_CB_FAILURE_THRESHOLD=5`, `NC_CB_TIMEOUT=30`, `NC_CB_SUCCESS_THRESHOLD=3`

---

## 19. Caching

```nc
to get_data with key:
    // Check cache first
    if is_cached(key):
        respond with cache_get(key)

    // Fetch and cache
    gather data from "https://api.example.com/" + key
    cache(key, data)
    respond with data
```

---

## 20. Shell & Process Execution

```nc
to run_script:
    // Simple shell command
    set output to shell("ls -la /app")

    // Structured result with exit code
    set result to shell_exec("python3 train.py")
    log "exit code: " + result.exit_code
    log "output: " + result.output
    if result.ok:
        log "training succeeded"

    // Execute with arguments (auto-escaped)
    set r to exec("python3", "predict.py", "--input", data)
```

> **Security:** Requires `NC_ALLOW_EXEC=1`

---

## 21. AI Integration

```nc
to analyze with question:
    // Simple AI call
    ask AI to "analyze this data" using {"data": question}

    // With model fallback
    set result to ai_with_fallback("analyze", context, ["model-a", "model-b"])
```

**Config:** `NC_AI_URL`, `NC_AI_KEY`, `NC_AI_MODEL`

---

## 22. ML Model Integration

```nc
to predict_price with features:
    set model to load_model("models/price_model.pkl")
    set prediction to predict(model, features)
    unload_model(model)
    respond with {"predicted_price": prediction}
```

---

## 23. Data Format Parsers

```nc
to parse_formats:
    set yaml_data to yaml_parse("name: Alice\nage: 30")
    set csv_data to csv_parse("name,age\nAlice,30")
    set xml_data to xml_parse("<user><name>Alice</name></user>")
    set toml_data to toml_parse("[server]\nport = 8080")
    set ini_data to ini_parse("[section]\nkey=value")
```

---

## 24. WebSocket

```nc
to websocket_demo:
    set conn to ws_connect("ws://localhost:8080/ws")
    ws_send(conn, json_encode({"type": "subscribe"}))
    set msg to ws_receive(conn)
    ws_close(conn)
```

---

## 25. Conversation Memory

```nc
to chat with message:
    set mem to memory_new(100)
    memory_add(mem, "user", message)
    set context to memory_get(mem)
    ask AI to "respond" using {"history": context}
```

---

## 26. Type Checking

```nc
to validate_input with data:
    if is_text(data):
        log "got string"
    if is_number(data):
        log "got number"
    if is_list(data):
        log "got list"
    if is_record(data):
        log "got map/record"
    if is_none(data):
        log "got nothing"
```

---

## 27. Regex

```nc
to regex_demo:
    set matches to re_match("hello123", "[a-z]+[0-9]+")   // true
    set found to re_find("price: $42.50", "[0-9.]+")      // "42.50"
    set cleaned to re_replace("a1b2c3", "[0-9]", "")      // "abc"
```

---

## 28. Environment Variables

```nc
to config_demo:
    set api_key to env("NC_AI_KEY")
    set port to env("NC_PORT")

    // In configure block, use env: prefix
    // configure:
    //     port: "env:MY_PORT"
    //     api_key: "env:MY_API_KEY"
```

---

## 29. Error Handling

NC supports three error handling syntaxes:

```nc
// Syntax 1: try / on_error
to safe_call_v1:
    try:
        gather data from "https://api.example.com/data"
        respond with data
    on_error:
        respond with {"error": "failed to fetch data"}

// Syntax 2: try / on error (two words)
to safe_call_v2:
    try:
        set result to risky_operation()
        respond with result
    on error:
        respond with {"error": "operation failed"}

// Syntax 3: try / otherwise
to safe_call_v3:
    try:
        gather data from "https://unstable-api.example.com"
        respond with data
    otherwise:
        respond with {"error": "service unavailable", "_status": 503}

// With finally block
to safe_call_v4:
    try:
        set data to read_file("/tmp/important.json")
        respond with json_decode(data)
    on_error:
        respond with {"error": "file not found"}
    finally:
        log "cleanup complete"
```

### 29.1 Typed Error Catching (Enterprise)

Catch specific error categories using `catch "ErrorType":` syntax:

```nc
try:
    gather data from "https://flaky-api.example.com/data"
catch "TimeoutError":
    log "Request timed out: " + err.message
    respond with {"error": "timeout", "type": err.type}
catch "ConnectionError":
    log "Connection failed at line " + str(err.line)
    respond with {"error": "connection_failed"}
on_error:
    log "Unknown error: " + error
    respond with {"error": error}
finally:
    log "cleanup complete"
```

**Supported error types:** `TimeoutError`, `ConnectionError`, `ParseError`, `IndexError`, `ValueError`, `IOError`, `KeyError`

The `err` context object provides:
- `err.message` — the error message string
- `err.type` — the classified error type
- `err.line` — the line number where the error occurred

### 29.2 error_type(message)

Classify an arbitrary error message string into a standard error type.

```nc
set et to error_type("Connection timeout expired")
// Returns "TimeoutError"

set et2 to error_type("Failed to parse JSON")
// Returns "ParseError"
```

### 29.3 traceback()

Returns the current call stack as a list of frame records.

```nc
set tb to traceback()
// Returns: [{"behavior": "my_handler", "file": "service.nc", "line": 42}, ...]
```

### 29.4 assert

Assert that a condition is true. On failure, halts execution with a line number and optional message.

```nc
assert len(users) is above 0, "Users list should not be empty"
assert response["status"] is equal 200, "Expected 200 OK"
assert result is not equal none
```

### 29.5 test Blocks

Define isolated test blocks that run without killing the program on failure. Outputs ✓ PASS or ✗ FAIL for each test.

```nc
test "user creation":
    set user to create_user("Alice", "alice@example.com")
    assert user["name"] is equal "Alice", "Name mismatch"
    assert user["email"] is equal "alice@example.com", "Email mismatch"

test "data validation":
    set data to validate({"age": 25})
    assert data["valid"] is equal true, "Should be valid"
```

---

## 29b. String Formatting

### format(template, ...)

Python-style string formatting with positional or named placeholders.

```nc
// Positional arguments
set msg to format("Hello {}, welcome to {}", "Alice", "NC")
// "Hello Alice, welcome to NC"

// Named placeholders with a map
set vars to {"name": "Bob", "count": "5"}
set msg to format("Hi {name}, you have {count} items", vars)
// "Hi Bob, you have 5 items"

// Numeric values
set result to format("{} + {} = {}", 1, 2, 3)
// "1 + 2 = 3"
```

---

## 29c. Data Structure Functions

### set_new(list)

Create a set (unique values) from a list. Backed by a map.

```nc
set s to set_new([1, 2, 2, 3, 3, 3])
set vals to set_values(s)
// vals: ["1", "2", "3"]  (3 unique elements)
```

### set_add(set, value), set_has(set, value), set_remove(set, value)

```nc
set s to set_new([])
set s to set_add(s, "apple")
set s to set_add(s, "banana")
log set_has(s, "apple")    // true
set s to set_remove(s, "apple")
log set_has(s, "apple")    // false
```

### set_values(set)

Returns the members of a set as a list.

### tuple(values...)

Create an immutable-style list from arguments.

```nc
set point to tuple(10, 20, "label")
log first(point)   // 10
log point[1]       // 20
```

### deque(), deque(list)

Double-ended queue. Supports push/pop from both ends.

```nc
set q to deque([1, 2, 3])
set q to deque_push_front(q, 0)    // [0, 1, 2, 3]
set front to deque_pop_front(q)     // front = 0, q = [1, 2, 3]
```

### counter(list)

Count occurrences of each element.

```nc
set counts to counter(["a", "b", "a", "c", "a", "b"])
log counts["a"]   // 3
log counts["b"]   // 2
```

### default_map(default_value)

Create a map with a stored default value.

```nc
set dm to default_map(0)
```

### enumerate(list)

Returns `[[0, item], [1, item], ...]` pairs.

```nc
set pairs to enumerate(["x", "y", "z"])
// [[0, "x"], [1, "y"], [2, "z"]]
```

### zip(list1, list2)

Merge two lists into pairs, truncated to the shorter length.

```nc
set zipped to zip([1, 2, 3], ["a", "b", "c"])
// [[1, "a"], [2, "b"], [3, "c"]]
```

---

## 29d. Async & Streaming

### await expression

Syntax placeholder for async operations (currently synchronous passthrough).

```nc
set result to await http_get("https://api.example.com/data")
```

### yield value

Push a value to the yield accumulator and output it.

```nc
to generate_items:
    yield "item 1"
    yield "item 2"
    yield "item 3"
```

---

## 30. API Server

```nc
service "my-api"
version "1.0.0"

configure:
    port: 8080

to get_users:
    respond with [{"name": "Alice"}, {"name": "Bob"}]

to create_user with name, email:
    // request body is auto-parsed as args
    store {"name": name, "email": email} into "users/" + name
    respond with {"status": "created", "_status": 201}

api:
    GET /users runs get_users
    POST /users runs create_user
    GET /health runs health_check
```

### Built-in Endpoints (automatic)

| Endpoint | Description |
|----------|-------------|
| `/health` | Liveness check with stats |
| `/ready` | Readiness probe (checks workers) |
| `/live` | Simple alive check |
| `/metrics` | Prometheus-compatible metrics |
| `/openapi.json` | Auto-generated API spec |
| `/` | Route discovery |

### Custom HTTP Status Codes

```nc
to not_found:
    respond with {"error": "not found", "_status": 404}

to created with data:
    respond with {"id": 123, "_status": 201}
```

---

## 31. Middleware

```nc
middleware:
    auth:
        type: "bearer"
    rate_limit:
        rpm: 100
    cors:
        origin: "https://myapp.example.com"
    log_requests: true
```

---

## 32. Scheduled Tasks

```nc
every 5 minutes:
    log "running cleanup"
    // periodic task logic here

every 1 hour:
    log "hourly report"
```

---

## 33. Configuration Reference

| Variable | Default | Description |
|----------|---------|-------------|
| **Server** | | |
| `NC_SERVICE_PORT` | 8000 | Server port |
| `NC_MAX_WORKERS` | 16 | Max concurrent threads |
| `NC_DRAIN_TIMEOUT` | 30 | Graceful shutdown timeout (sec) |
| `NC_REQUEST_TIMEOUT` | 60 | Max behavior execution time (sec). Returns 504 on timeout |
| `NC_MAX_LOOP_ITERATIONS` | 10000000 | Max loop iterations |
| `NC_TLS_CERT` | — | TLS certificate path |
| `NC_TLS_KEY` | — | TLS private key path |
| **Auth** | | |
| `NC_JWT_SECRET` | — | JWT signing key |
| `NC_API_KEYS` | — | Comma-separated API keys |
| `NC_OIDC_ISSUER` | — | OIDC identity provider |
| **Security** | | |
| `NC_CORS_ORIGIN` | — | Allowed CORS origin |
| `NC_ALLOW_EXEC` | 0 | Enable shell execution |
| `NC_ALLOW_FILE_WRITE` | 0 | Enable file writes |
| `NC_ALLOW_NETWORK` | 0 | Enable outbound network |
| `NC_HTTP_ALLOWLIST` | — | Allowed outbound hosts |
| `NC_HTTP_COOKIES` | 0 | Enable cookie engine |
| **Rate Limiting** | | |
| `NC_RATE_LIMIT_RPM` | 100 | Default requests/minute |
| `NC_RATE_LIMIT_WINDOW` | 60 | Window in seconds |
| **Logging** | | |
| `NC_LOG_FORMAT` | text | `text` or `json` |
| `NC_LOG_LEVEL` | info | `debug`/`info`/`warn`/`error` |
| `NC_AUDIT_FORMAT` | — | Set `json` to enable audit |
| `NC_AUDIT_FILE` | — | Audit log file path |
| **Sessions** | | |
| `NC_SESSION_TTL` | 3600 | Session timeout (seconds) |
| **Circuit Breaker** | | |
| `NC_CB_FAILURE_THRESHOLD` | 5 | Failures before opening |
| `NC_CB_TIMEOUT` | 30 | Seconds before retry |
| **Feature Flags** | | |
| `NC_FF_<NAME>` | — | `1`/`0`/percentage |
| **AI** | | |
| `NC_AI_URL` | — | AI provider endpoint |
| `NC_AI_KEY` | — | AI API key |
| `NC_AI_MODEL` | — | Default model name |
| **Observability** | | |
| `NC_OTEL_ENDPOINT` | — | Tracing collector URL |

---

## 34. Keyword Synonyms

NC accepts keywords from other languages. They are silently mapped:

| You write | NC understands it as |
|-----------|---------------------|
| `else` | `otherwise` |
| `elif` | `otherwise if` |
| `break` | `stop` |
| `continue` | `skip` |
| `switch` | `match` |
| `case` | `when` |
| `def` / `func` / `function` / `fn` | `to` |
| `return` | `respond with` |
| `var` / `let` / `const` | `set` |
| `print` / `puts` / `echo` / `console.log` | `log` |
| `require` / `include` / `use` | `import` |
| `dict` / `map` / `object` | `record` |
| `array` | `list` |
| `struct` / `class` / `interface` | `define` |
| `null` / `nil` / `None` / `undefined` | `nothing` |
| `catch` / `except` / `rescue` | `on_error` |

---

## Complete Working Example

```nc
service "user-api"
version "1.0.0"

configure:
    port: 8080
    auth: "bearer"
    rate_limit: 100
    cors_origin: "env:FRONTEND_URL"
    log_requests: true

to register with username, password, email:
    purpose: "Create a new user account"
    if len(password) is below 8:
        respond with {"error": "password too short", "_status": 400}

    set password_hash to hash_password(password)
    set user to {
        "username": username,
        "email": email,
        "password_hash": password_hash,
        "created_at": time_iso(),
        "role": "user"
    }
    store user into "users/" + username
    set token to jwt_generate(username, "user", 86400)
    respond with {"token": token, "user": username, "_status": 201}

to login with username, password:
    purpose: "Authenticate and return JWT"
    try:
        gather user from "users/" + username
    otherwise:
        respond with {"error": "user not found", "_status": 404}

    if not verify_password(password, user.password_hash):
        respond with {"error": "invalid password", "_status": 401}

    set token to jwt_generate(username, user.role, 86400)
    set sid to session_create()
    session_set(sid, "user", username)
    respond with {"token": token, "session_id": sid}

to get_profile:
    purpose: "Get current user profile"
    set auth to request_header("Authorization")
    set token to replace(auth, "Bearer ", "")
    set claims to jwt_verify(token)
    if claims is equal to false:
        respond with {"error": "unauthorized", "_status": 401}

    gather user from "users/" + claims.sub
    respond with {
        "username": user.username,
        "email": user.email,
        "role": user.role,
        "created_at": user.created_at
    }

to health_check:
    respond with {
        "status": "healthy",
        "version": "1.0.0",
        "timestamp": time_iso()
    }

api:
    POST /register runs register
    POST /login runs login
    GET /profile runs get_profile
    GET /health runs health_check
```

## Project Lifecycle (Single-Command Workflow)

NC supports full project lifecycle from creation to deployment — like npm, pip, or cargo:

```bash
# 1. Create a new project (scaffolds service.nc, .env, Dockerfile, docker-compose)
nc init my-api
nc init my-api --all          # includes Kubernetes manifests too

# 2. Develop
cd my-api
nc dev                        # validates, then starts server
nc dev service.nc -p 3000     # custom port

# 3. Start / Stop
nc start                      # auto-discovers *.nc files, starts server
nc start service.nc            # start specific file
nc stop                        # stop all running NC services

# 4. Test
nc validate service.nc         # syntax check
nc test                        # run all test files
nc conformance                 # run conformance suite
nc doctor                      # check AI keys, .env, dependencies

# 5. Build native binary
nc build service.nc            # compile to native executable
nc build . -j 4                # compile all .nc files in parallel

# 6. Deploy
nc deploy                      # build container image (auto-generates Dockerfile)
nc deploy --tag my-app:v1      # custom image tag
nc deploy --push               # build and push to registry

# 7. Run in production
docker run -p 8080:8080 my-app:v1
# or
nc start -p 8080
```

### Comparison with Other Languages

| Task | Python/pip | Node/npm | NC |
|------|-----------|---------|------|
| Create project | `mkdir + venv + pip init` | `npm init` | `nc init my-api` |
| Install deps | `pip install -r requirements.txt` | `npm install` | Not needed (zero deps) |
| Dev server | `flask run` / `uvicorn` | `npm run dev` | `nc dev` |
| Start | `python app.py` | `npm start` | `nc start` |
| Stop | `kill PID` | `Ctrl+C` | `nc stop` |
| Validate | `mypy` / `pylint` | `eslint` | `nc validate` |
| Build | `pyinstaller` | `npm run build` | `nc build` |
| Deploy | `docker build` | `docker build` | `nc deploy` |
| Binary size | ~50MB (with Python) | ~100MB (with Node) | ~500KB |
| Dependencies | pip packages | node_modules | Zero |

---

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)** | support@devheallabs.in
