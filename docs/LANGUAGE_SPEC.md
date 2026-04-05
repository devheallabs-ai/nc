
  


# NC Language Specification v1.0

## 1. Overview

NC is a plain English programming language designed for AI platforms.
Programs are written in natural language and compiled to bytecodes executed by a C-based VM.

**File extension:** `.nc`
**Encoding:** UTF-8
**Comments:** `//` for line comments
**Blocks:** Indentation-based (4 spaces recommended)

## 2. Program Structure

```
service "name"           // required
version "1.0.0"          // required
model "nova"           // optional
description "text"       // optional
author "name"            // optional

import "module"          // imports (v0.3+)

configure:               // optional configuration
    key: value

define TypeName as:      // type definitions
    field is type

to behavior_name:        // behavior definitions
    ...statements...

api:                     // API route declarations
    METHOD /path runs handler
```

## 3. Types

| NC Type  | Description              | Examples            |
|----------|--------------------------|---------------------|
| text     | UTF-8 string             | "hello", "world"    |
| number   | integer or float         | 42, 3.14            |
| yesno    | boolean                  | yes, no, true, false|
| list     | ordered collection       | [1, 2, 3]           |
| record   | key-value map            | (via gather/ask AI) |
| nothing  | null/none                | nothing, none       |

## 4. Variables

```
set x to 42
set name to "World"
set items to [1, 2, 3]
```

Variables are dynamically typed. Scope follows block structure.
Variables set inside `repeat` or `if` are visible in the enclosing scope.

## 5. Expressions

### Arithmetic
```
x + y       // addition (numbers), string concatenation, or list concatenation
x - y       // subtraction
x * y       // multiplication
x / y       // division
```

### Comparison (plain English)
```
x is above 10           // x > 10
x is below 5            // x < 5
x is equal to "ok"      // x == "ok"
x is not equal to 0     // x != 0
x is at least 10        // x >= 10
x is at most 100        // x <= 100
```

### Logic
```
a and b
a or b
not a
```

### Access
```
record.field             // dot access
list[0]                  // index access
len(items)               // function call
```

### Templates
```
"Hello, {{name}}!"      // string interpolation
```

## 6. Statements

### gather — fetch data from a source
```
gather metrics from prometheus
gather metrics from prometheus:
    query: "container_cpu"
    range: "1h"
```

### ask AI — invoke an LLM
```
ask AI to "analyze this data" using metrics, logs
ask AI to "classify" using ticket:
    confidence: 0.8
    save as: result
```

### set — assign a variable
```
set count to 0
set name to "World"
```

### respond — return a value from a behavior
```
respond with result
respond with "done"
respond with x + y
```

### if / otherwise — conditional
```
if score is above 90:
    respond with "excellent"
otherwise:
    respond with "ok"
```

### repeat — loop
```
repeat for each item in items:
    log item

repeat 5 times:
    log "hello"
```

### while — conditional loop
```
while count is above 0:
    set count to count - 1
```

### repeat while — synonym for while
```
repeat while count is above 0:
    set count to count - 1
```

### match / when — pattern matching
```
match status:
    when "healthy":
        respond with "ok"
    when "critical":
        respond with "alert"
    otherwise:
        respond with "unknown"
```

### log — output a message
```
log "processing started"
```

### show — display a value
```
show result
```

### notify — send a notification
```
notify ops_team "deployment failed"
```

### wait — pause execution
```
wait 30 seconds
wait 5 minutes
```

### run — call another behavior
```
run diagnose with issue
run heal with issue and diagnosis
```

### store — persist data
```
store result into "knowledge_base"
```

### emit — fire an event
```
emit "diagnosis_complete" with data
```

### try / on error — error handling
```
try:
    gather data from risky_source
on error:
    log "failed"
```

## 7. Behaviors

Behaviors are NC's functions. Defined with `to`:

```
to greet with name:
    purpose: "Say hello"
    respond with "Hello, " + name

to add with a and b:
    respond with a + b
```

### Approval gates
```
to heal:
    needs approval when blast_radius is equal to "high"
    ...
```

## 8. Types (define)

```
define Issue as:
    id is text
    severity is text
    pod is text optional
    count is number
```

## 9. API Routes

```
api:
    GET /health runs health_check
    POST /diagnose runs diagnose
    PUT /update runs update_config
```

## 10. Events and Schedules

```
on event "alert.firing":
    run diagnose with alert

every 5 minutes:
    log "health check"
```

## 11. Built-in Functions

### Core

| Function    | Description                    |
|-------------|--------------------------------|
| len(x)      | Length of list, string, or map |
| str(x)      | Convert to string              |
| int(x)      | Convert to integer             |
| float(x)    | Convert to float               |
| type(x)     | Type name as string            |
| print(x)    | Print to output                |
| keys(map)   | Get list of map keys           |
| values(map) | Get list of map values         |

### Math (updated v1.0)

| Function         | Description                     |
|------------------|---------------------------------|
| min(a, b)        | Minimum of two values           |
| min(list)        | Minimum value in a list         |
| max(a, b)        | Maximum of two values           |
| max(list)        | Maximum value in a list         |
| round(x)         | Round to nearest integer        |
| round(x, d)      | Round to d decimal places       |

### Higher-Order (v1.0)

| Function                         | Description                    |
|----------------------------------|--------------------------------|
| sort_by(list, "field")           | Sort records by field          |
| max_by(list, "field")            | Record with highest value      |
| min_by(list, "field")            | Record with lowest value       |
| sum_by(list, "field")            | Sum a numeric field            |
| map_field(list, "field")         | Extract field into list        |
| filter_by(list, "f", "op", val)  | Filter by comparison           |

### Security (v1.0)

| Function              | Description                       |
|-----------------------|-----------------------------------|
| hash_sha256(s)        | SHA-256 hex digest                |
| hash_password(pw)     | Salted hash for storage           |
| verify_password(pw,h) | Constant-time verify              |
| hash_hmac(data, key)  | HMAC-SHA256                       |
| jwt_generate(u,r,exp) | Create signed JWT                 |
| jwt_verify(token)     | Verify JWT, return claims or false|
| session_create()      | Create server-side session        |
| session_set(s, k, v)  | Set session value                 |
| session_get(s, k)     | Get session value                 |
| session_destroy(s)    | Invalidate session                |
| request_header(name)  | Read HTTP request header          |
| request_ip()          | Client IP address                 |
| feature(flag)         | Check feature flag                |
| circuit_open(name)    | Check circuit breaker state       |

## 12. Execution Model

```
Source (.nc) → Lexer → Parser → AST → Semantic Analysis
    → Bytecode Compiler → VM Execution
```

The VM is a stack-based machine with 30+ opcodes.
Each behavior compiles to a separate bytecode chunk.

## 13. Truthiness

| Value              | Truthy? |
|--------------------|---------|
| nothing / none     | no      |
| false / no         | no      |
| 0                  | no      |
| "" (empty string)  | no      |
| [] (empty list)    | no      |
| {} (empty map)     | no      |
| Everything else    | yes     |

---

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)** | support@devheallabs.in
