# NC Quick Reference

A cheat sheet for NC developers. For the full guide, see [NC_LANGUAGE_GUIDE.md](NC_LANGUAGE_GUIDE.md).

---

## Program Structure

```
service "name"              // required
version "1.0.0"             // required
model "nova"              // optional — default AI model
description "text"          // optional
author "name"               // optional

import "module"             // imports

configure:                  // configuration block
    key: value

define TypeName as:         // type definitions
    field is type

to behavior_name:           // behavior (function) definitions
    ...

api:                        // API route declarations
    METHOD /path runs handler
```

---

## Keywords and Synonyms

### Core Keywords

| Keyword | Purpose | Example |
|---------|---------|---------|
| `service` | Declare service name | `service "my-app"` |
| `version` | Declare version | `version "1.0.0"` |
| `model` | Set default AI model | `model "nova"` |
| `import` | Import a module | `import "math"` |
| `configure` | Configuration block | `configure:` |
| `define` | Define a type | `define User as:` |
| `to` | Define a behavior | `to greet with name:` |
| `with` | Behavior parameter | `to greet with name:` |
| `and` | Additional parameter | `to add with a and b:` |
| `api` | API route block | `api:` |
| `middleware` | Middleware block | `middleware:` |

### Variable and Assignment

| Keyword | Purpose | Example |
|---------|---------|---------|
| `set ... to` | Assign a variable | `set x to 42` |
| `respond with` | Return a value | `respond with result` |

### Control Flow

| Keyword | Synonym | Example |
|---------|---------|---------|
| `if` | — | `if x is above 10:` |
| `otherwise` | `else` | `otherwise:` |
| `match` | — | `match status:` |
| `when` | — | `when "active":` |
| `repeat for each` | — | `repeat for each item in items:` |
| `repeat N times` | — | `repeat 5 times:` |
| `while` | `repeat while` | `while count is below 10:` |
| `stop` | `break` | `stop` |
| `skip` | `continue` | `skip` |

### Comparison Operators (Plain English)

| NC Syntax | Meaning | Traditional |
|-----------|---------|-------------|
| `is above` | Greater than | `>` |
| `is below` | Less than | `<` |
| `is equal to` | Equals | `==` |
| `is not equal to` | Not equals | `!=` |
| `is at least` | Greater or equal | `>=` |
| `is at most` | Less or equal | `<=` |
| `is greater than` | Greater than | `>` |
| `is less than` | Less than | `<` |
| `is in` | Membership | `in` |
| `is not in` | Non-membership | `not in` |

### Logic

| Keyword | Example |
|---------|---------|
| `and` | `if a and b:` |
| `or` | `if a or b:` |
| `not` | `if not active:` |

### Actions

| Keyword | Purpose | Example |
|---------|---------|---------|
| `log` | Print/log a message | `log "processing"` |
| `show` | Display a value | `show result` |
| `notify` | Send notification | `notify ops "alert"` |
| `wait` | Pause execution | `wait 5 seconds` |
| `run` | Call another behavior | `run validate with data` |
| `store` | Persist data | `store result into "db"` |
| `emit` | Fire an event | `emit "done" with data` |
| `fail` | Raise an error | `fail "invalid input"` |
| `append` | Add to list | `append item to list` |
| `remove` | Remove from list | `remove item from list` |

### AI and Data

| Keyword | Purpose | Example |
|---------|---------|---------|
| `ask AI to` | Invoke an LLM | `ask AI to "classify" using data` |
| `gather ... from` | Fetch data from source | `gather data from "https://..."` |
| `save as` | Store AI result | `ask AI to "..." save as result` |

### Error Handling

| Keyword | Purpose | Example |
|---------|---------|---------|
| `try` | Begin try block | `try:` |
| `on error` | Error handler | `on error:` |

### Events and Scheduling

| Keyword | Purpose | Example |
|---------|---------|---------|
| `on event` | Event handler | `on event "alert.firing":` |
| `every` | Scheduled task | `every 5 minutes:` |

### Approval

| Keyword | Purpose | Example |
|---------|---------|---------|
| `needs approval when` | Gate a behavior | `needs approval when env is equal to "prod"` |

---

## Data Types

| Type | NC Name | Examples |
|------|---------|---------|
| String | `text` | `"hello"`, `"world"` |
| Integer | `number` | `42`, `-7` |
| Float | `number` | `3.14`, `0.5` |
| Boolean | `yesno` | `yes`, `no`, `true`, `false` |
| List | `list` | `[1, 2, 3]` |
| Record (Map) | `record` | `{"key": "value"}` |
| Null | `nothing` | `nothing`, `none` |

---

## Built-in Functions

### Core

| Function | Description | Example |
|----------|-------------|---------|
| `len(x)` | Length of string, list, or map | `len("hello")` -> 5 |
| `str(x)` | Convert to string | `str(42)` -> "42" |
| `int(x)` | Convert to integer | `int("10")` -> 10 |
| `float(x)` | Convert to float | `float("3.14")` -> 3.14 |
| `bool(x)` | Convert to boolean | `bool(1)` -> yes |
| `type(x)` | Get type name | `type("hi")` -> "text" |
| `print(x)` | Print to output | `print("hello")` |
| `keys(map)` | Get map keys as list | `keys(record)` |
| `values(map)` | Get map values as list | `values(record)` |

### Type Checking

| Function | Returns |
|----------|---------|
| `is_text(x)` | `yes` if x is text |
| `is_number(x)` | `yes` if x is a number |
| `is_list(x)` | `yes` if x is a list |
| `is_none(x)` | `yes` if x is nothing |

### String Functions

| Function | Description | Example |
|----------|-------------|---------|
| `upper(s)` | Uppercase | `upper("hi")` -> "HI" |
| `lower(s)` | Lowercase | `lower("HI")` -> "hi" |
| `trim(s)` | Strip whitespace | `trim("  hi  ")` -> "hi" |
| `contains(s, sub)` | Substring check | `contains("hello", "ell")` -> yes |
| `starts_with(s, pre)` | Prefix check | `starts_with("hello", "hel")` -> yes |
| `ends_with(s, suf)` | Suffix check | `ends_with("hello", "llo")` -> yes |
| `replace(s, old, new)` | Replace substring | `replace("hello", "l", "r")` |
| `split(s, sep)` | Split into list | `split("a,b", ",")` -> ["a","b"] |
| `join(list, sep)` | Join list into string | `join(["a","b"], "-")` -> "a-b" |
| `slice(s, start, end)` | Substring | `slice("hello", 0, 3)` -> "hel" |

### Math Functions

| Function | Description |
|----------|-------------|
| `min(a, b)` / `min(list)` | Minimum value |
| `max(a, b)` / `max(list)` | Maximum value |
| `round(x)` / `round(x, d)` | Round to d decimal places |
| `abs(x)` | Absolute value |
| `floor(x)` | Floor |
| `ceil(x)` | Ceiling |

### List Functions

| Function | Description | Example |
|----------|-------------|---------|
| `append item to list` | Add to end | `append "x" to items` |
| `remove item from list` | Remove first match | `remove "x" from items` |
| `sort(list)` | Sort ascending | `sort([3,1,2])` -> [1,2,3] |
| `reverse(list)` | Reverse order | `reverse([1,2,3])` -> [3,2,1] |
| `unique(list)` | Remove duplicates | `unique([1,1,2])` -> [1,2] |
| `flatten(list)` | Flatten nested | `flatten([[1],[2]])` -> [1,2] |
| `range(n)` | Generate 0..n-1 | `range(5)` -> [0,1,2,3,4] |
| `range(a, b)` | Generate a..b-1 | `range(2, 5)` -> [2,3,4] |

### Higher-Order Functions

| Function | Description | Example |
|----------|-------------|---------|
| `sort_by(list, "field")` | Sort records by field | `sort_by(users, "age")` |
| `max_by(list, "field")` | Record with max field | `max_by(users, "score")` |
| `min_by(list, "field")` | Record with min field | `min_by(users, "score")` |
| `sum_by(list, "field")` | Sum a numeric field | `sum_by(orders, "total")` |
| `map_field(list, "field")` | Extract field values | `map_field(users, "name")` |
| `filter_by(list, "f", "op", val)` | Filter by comparison | `filter_by(users, "age", ">", 18)` |

### JSON Functions

| Function | Description |
|----------|-------------|
| `json_encode(value)` | Value to JSON string |
| `json_decode(string)` | JSON string to value |

### File I/O

| Function | Description |
|----------|-------------|
| `read_file(path)` | Read file contents |
| `write_file(path, data)` | Write string to file |
| `file_exists(path)` | Check if file exists |

### Security Functions

| Function | Description |
|----------|-------------|
| `hash_sha256(s)` | SHA-256 hex digest |
| `hash_password(pw)` | Salted hash for storage |
| `verify_password(pw, hash)` | Constant-time verify |
| `hash_hmac(data, key)` | HMAC-SHA256 |
| `jwt_generate(user, role, exp)` | Create signed JWT |
| `jwt_verify(token)` | Verify JWT, return claims |
| `session_create()` | Create server session |
| `session_set(s, k, v)` | Set session value |
| `session_get(s, k)` | Get session value |
| `session_destroy(s)` | Invalidate session |
| `request_header(name)` | Read HTTP request header |
| `request_ip()` | Client IP address |
| `feature(flag)` | Check feature flag |
| `circuit_open(name)` | Circuit breaker state |

---

## Common Patterns

### Service Template

```
service "my-service"
version "1.0.0"

to health:
    respond with {"status": "healthy"}

to process with body:
    purpose: "Process incoming data"
    set input to body["data"]
    // ... your logic ...
    respond with {"result": result}

api:
    GET  /health  runs health
    POST /process runs process
```

### CRUD Pattern

```
service "crud-api"
version "1.0.0"

set items to []
set next_id to 1

to create with body:
    set item to {}
    set item["id"] to next_id
    set next_id to next_id + 1
    set item["name"] to body["name"]
    append item to items
    respond with item

to list_all:
    respond with items

to get_one with id:
    repeat for each item in items:
        if item["id"] is equal to int(id):
            respond with item
    respond with {"error": "not found"}

to update with id and body:
    repeat for each item in items:
        if item["id"] is equal to int(id):
            set item["name"] to body["name"]
            respond with item
    respond with {"error": "not found"}

to delete_one with id:
    set new_items to []
    repeat for each item in items:
        if item["id"] is not equal to int(id):
            append item to new_items
    set items to new_items
    respond with {"deleted": id}

api:
    POST   /items      runs create
    GET    /items      runs list_all
    GET    /items/:id  runs get_one
    PUT    /items/:id  runs update
    DELETE /items/:id  runs delete_one
```

### AI Classification Pattern

```
service "classifier"
version "1.0.0"
model "nova"

to classify with body:
    set text to body["text"]
    ask AI to "Classify this text as: positive, negative, or neutral. Return only the label." using text save as label
    respond with {"label": trim(label), "input": text}

api:
    POST /classify runs classify
```

### API Routing with Parameters

```
api:
    GET    /users          runs list_users
    GET    /users/:id      runs get_user
    POST   /users          runs create_user
    PUT    /users/:id      runs update_user
    DELETE /users/:id      runs delete_user
    GET    /users/:id/posts runs get_user_posts
```

Route parameters (`:id`, `:name`) are passed as behavior arguments in order.

### Auth Pattern (JWT)

```
service "secure-api"
version "1.0.0"

configure:
    jwt_secret: "your-secret-key"

to login with body:
    set username to body["username"]
    set password to body["password"]
    // validate credentials ...
    set token to jwt_generate(username, "user", 3600)
    respond with {"token": token}

to protected with body:
    set auth to request_header("Authorization")
    if auth is equal to nothing:
        respond with {"error": "no token"}
    set token to replace(auth, "Bearer ", "")
    set claims to jwt_verify(token)
    if claims is equal to false:
        respond with {"error": "invalid token"}
    respond with {"user": claims["sub"], "data": "secret stuff"}

api:
    POST /login     runs login
    GET  /protected runs protected
```

### Error Handling Pattern

```
to safe_fetch with url:
    try:
        gather data from url
        respond with data
    on error:
        log "Failed to fetch: " + url
        respond with {"error": "fetch failed", "url": url}
```

### Data Persistence Pattern

```
to save_data with data:
    set raw to json_encode(data)
    write_file("data/store.json", raw)
    log "Data saved"

to load_data:
    try:
        set raw to read_file("data/store.json")
        set data to json_decode(raw)
        respond with data
    on error:
        respond with []
```

### Middleware Pattern

```
middleware:
    use cors
    use rate_limit
    use auth jwt
    use logging
```

### WebSocket Pattern

```
on websocket "connect":
    log "Client connected"

on websocket "message":
    set reply to "Echo: " + message
    respond with reply

on websocket "disconnect":
    log "Client disconnected"
```

### Scheduled Task Pattern

```
every 5 minutes:
    gather metrics from "https://api.example.com/health"
    if metrics["status"] is not equal to "ok":
        notify ops_team "Service degraded"
```

---

## Environment Variables

### AI Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_AI_URL` | AI provider URL | — |
| `NC_AI_KEY` | AI API key | — |
| `NC_AI_MODEL` | Default model | — |

### Server Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_PORT` | HTTP listen port | `8080` |
| `NC_MAX_WORKERS` | Worker thread count | `64` |
| `NC_LISTEN_BACKLOG` | TCP backlog size | `512` |
| `NC_REQUEST_QUEUE_SIZE` | Request queue depth | `4096` |
| `NC_KEEPALIVE_MAX` | Max keepalive requests | `100` |
| `NC_KEEPALIVE_TIMEOUT` | Keepalive timeout (sec) | `30` |
| `NC_QUEUE_TIMEOUT` | Queue timeout (ms) | `5000` |
| `NC_DRAIN_TIMEOUT` | Graceful shutdown (sec) | `30` |

### Security

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_JWT_SECRET` | JWT signing secret | — |
| `NC_CORS_ORIGIN` | Allowed CORS origins | — |
| `NC_OIDC_ISSUER` | OIDC issuer URL | — |
| `NC_OIDC_CLIENT_ID` | OIDC client ID | — |

### Logging and Monitoring

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_LOG_FORMAT` | Log format (`json` or `text`) | `text` |
| `NC_LOG_LEVEL` | Log level (`debug`, `info`, `warn`, `error`) | `info` |
| `NC_AUDIT_FORMAT` | Audit log format | `json` |
| `NC_AUDIT_FILE` | Audit log file path | — |
| `NC_OTEL_ENDPOINT` | OpenTelemetry collector | — |

### Storage

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_STORE_URL` | External store URL | — |
| `NC_LIB_PATH` | Standard library path | `./lib` |

### API Versioning

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_API_VERSIONS` | Supported API versions | `v1` |

---

## CLI Reference

```bash
nc version                  # Show version info
nc run <file.nc>            # Run a script
nc serve <file.nc>          # Start HTTP server
nc validate <file.nc>       # Syntax check only
nc test                     # Run built-in tests
nc repl                     # Interactive REPL
nc migrate <file.py>        # Convert Python to NC
nc lsp                      # Start Language Server Protocol
nc pkg install <name>       # Install a package
nc pkg list                 # List installed packages
```

---

## String Interpolation

Use `{{variable}}` inside strings for template interpolation:

```
log "Hello, {{name}}!"
log "Order #{{order.id}} from {{order.customer}}"
log "Score: {{score}} / 100"
```

Dot access works inside templates: `{{record.field}}`.

---

## Truthiness

| Value | Truthy? |
|-------|---------|
| `nothing` / `none` | no |
| `false` / `no` | no |
| `0` | no |
| `""` (empty string) | no |
| `[]` (empty list) | no |
| `{}` (empty map) | no |
| Everything else | yes |

---

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)**
