
  


# NC Language Guide

### The Complete Reference for NC

Welcome to the NC Language Guide â€” your one-stop reference for learning and using NC, the plain-English programming language for AI. This guide is structured like a Python tutorial: start from the basics, build up to advanced features.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [Your First Program](#2-your-first-program)
3. [Variables and Data Types](#3-variables-and-data-types)
4. [Operators](#4-operators)
5. [Strings and Interpolation](#5-strings-and-interpolation)
6. [Control Flow](#6-control-flow)
7. [Functions (Behaviors)](#7-functions-behaviors)
8. [Lists and Records](#8-lists-and-records)
9. [Built-in Functions](#9-built-in-functions)
10. [Error Handling](#10-error-handling)
11. [Services and APIs](#11-services-and-apis)
12. [Type Definitions](#12-type-definitions)
13. [Imports and Modules](#13-imports-and-modules)
14. [AI Operations](#14-ai-operations)
15. [Data Gathering and Storage](#15-data-gathering-and-storage)
16. [Events and Scheduling](#16-events-and-scheduling)
17. [Middleware](#17-middleware)
18. [WebSockets](#18-websockets)
19. [Database Operations](#19-database-operations)
20. [Async and Concurrency](#20-async-and-concurrency)
21. [Debugging](#21-debugging)
22. [REPL (Interactive Mode)](#22-repl-interactive-mode)
23. [CLI Reference](#23-cli-reference)
24. [Package Management](#24-package-management)
25. [Environment Variables](#25-environment-variables)
26. [Enterprise Features](#26-enterprise-features)
27. [Complete Examples](#27-complete-examples)

---

## 1. Getting Started

### Installation

```bash
git clone https://github.com/devheallabs-ai/nc-lang.git
cd nc
./install.sh
```

**Requirements:** A C compiler (`gcc` or `clang`). That's it.
**Binary size:** 570KB. **Dependencies:** Zero (statically linked).

### Running a Program

```bash
nc run myfile.nc
```

### Validating Syntax

```bash
nc validate myfile.nc
```

---

## 2. Your First Program

Create a file called `hello.nc`:

```
service "hello-world"
version "1.0.0"

to greet with name:
    purpose: "Say hello to someone"
    respond with "Hello, " + name + "! Welcome to NC."

to health_check:
    respond with "healthy"

api:
    GET /hello runs greet
    GET /health runs health_check
```

Run it:

```bash
nc run hello.nc
```

Or start a server:

```bash
nc serve hello.nc
```

That's it â€” a working API in 12 lines of English.

---

## 3. Variables and Data Types

### Setting Variables

Use `set ... to ...` to assign a value:

```
set name to "Alice"
set age to 30
set pi to 3.14
set active to yes
set items to [1, 2, 3]
set nothing_here to nothing
```

### Data Types

| Type | Syntax | Description |
|------|--------|-------------|
| **Text** (string) | `"hello"` | Text wrapped in double quotes |
| **Number** (integer) | `42` | Whole numbers |
| **Number** (float) | `3.14` | Decimal numbers |
| **Boolean** | `yes` / `no` or `true` / `false` | True/false values |
| **List** | `[1, 2, 3]` | Ordered collection |
| **Record** (map) | Key-value structure | Dictionary-like structure |
| **Nothing** | `nothing` or `none` | Null / absence of value |

### Type Conversion

```
set x to str(42)          // "42"
set y to int("10")        // 10
set z to float("3.14")    // 3.14
set b to bool(1)          // yes
```

### Type Checking

```
set t to type(name)       // "text"
set check to is_text(name)    // yes
set check to is_number(age)   // yes
set check to is_list(items)   // yes
set check to is_none(x)       // no
```

---

## 4. Operators

### Arithmetic

```
set sum to 10 + 5         // 15
set diff to 10 - 5        // 5
set product to 10 * 5     // 50
set quotient to 10 / 3    // 3.33...
```

### String Concatenation

```
set greeting to "Hello, " + name + "!"
```

### Comparison (Plain English)

NC uses natural language for comparisons:

```
if score is above 90:           // score > 90
if score is below 50:           // score < 50
if score is equal to 100:       // score == 100
if score is not equal to 0:     // score != 0
if score is at least 70:        // score >= 70
if score is at most 100:        // score <= 100
if score is greater than 80:    // score > 80
if score is less than 20:       // score < 20
```

### Logical Operators

```
if age is above 18 and active is true:
    log "Eligible"

if role is equal to "admin" or role is equal to "owner":
    log "Has access"

if active is not true:
    log "Inactive"
```

### Membership

```
if item is in allowed_list:
    log "Allowed"

if tool is not in config.allowed_tools:
    log "Blocked"
```

---

## 5. Strings and Interpolation

### Basic Strings

```
set message to "Hello, World!"
```

### String Concatenation

```
set full to "Hello, " + name + "!"
```

### Template Interpolation with `{{}}`

Inside strings used with `log`, `notify`, `gather`, and other statements, use `{{}}` for variable interpolation:

```
log "User logged in: {{username}}"
log "Order #{{order.id}} placed by {{order.customer}}"
log "Status: {{service.status}} â€” latency: {{service.latency_ms}}ms"
```

**Dot access** works inside templates:

```
log "{{incident.type}} â€” {{classification.severity}}"
```

### String Functions

```
set upper_name to upper("hello")         // "HELLO"
set lower_name to lower("HELLO")         // "hello"
set trimmed to trim("  hello  ")         // "hello"
set found to contains("hello world", "world")  // yes
set starts to starts_with("hello", "hel")      // yes
set ends to ends_with("hello", "llo")          // yes
set replaced to replace("hello", "l", "r")     // "herro"
set parts to split("a,b,c", ",")               // ["a", "b", "c"]
set joined to join(["a", "b", "c"], "-")        // "a-b-c"
```

---

## 6. Control Flow

### If / Otherwise

```
if temperature is above 30:
    log "It's hot!"
otherwise:
    log "It's comfortable."
```

**Nested conditions:**

```
if score is above 90:
    respond with "excellent"
otherwise:
    if score is above 70:
        respond with "good"
    otherwise:
        if score is above 50:
            respond with "average"
        otherwise:
            respond with "needs improvement"
```

### Match / When (Switch)

```
match status:
    when "healthy":
        respond with "System is running normally"
    when "degraded":
        respond with "Performance issues detected"
    when "critical":
        respond with "Immediate attention required"
    otherwise:
        respond with "Unknown status: " + status
```

### Repeat For Each (For Loop)

```
set fruits to ["apple", "banana", "cherry"]
repeat for each fruit in fruits:
    log "I like {{fruit}}"
```

### Repeat N Times

```
repeat 5 times:
    log "Hello!"
```

### While Loop

```
set count to 0
while count is below 10:
    log "Count: {{count}}"
    set count to count + 1
```

### Repeat While

`repeat while` is a synonym for `while`:

```
set count to 0
repeat while count is below 10:
    log "Count: {{count}}"
    set count to count + 1
```

All loop forms (`repeat for each`, `repeat N times`, `while`, `repeat while`) can be mixed freely in the same behavior.

### Loop Control

- `stop` â€” break out of the current loop
- `skip` â€” skip to the next iteration

```
repeat for each item in items:
    if item is equal to "skip_me":
        skip
    if item is equal to "done":
        stop
    log "Processing: {{item}}"
```

### Wait (Sleep)

```
wait 5 seconds
wait 2 minutes
wait 1 hours
```

---

## 7. Functions (Behaviors)

In NC, functions are called **behaviors**. They are defined with the `to` keyword.

### Defining a Behavior

```
to greet:
    respond with "Hello, World!"
```

### With Parameters

Use `with` for a single parameter, and `and` to chain multiple parameters:

```
to greet with name:
    respond with "Hello, " + name + "!"

to add with a and b:
    respond with a + b

to create_user with name and email and role:
    log "Creating user: {{name}}"
    respond with "User created"
```

### Purpose Annotation

Describe what a behavior does:

```
to classify with ticket:
    purpose: "Classify a support ticket using AI"
    ask AI to "classify this ticket" using ticket
    respond with result
```

### Returning Values

Use `respond with` to return a value:

```
to calculate_total with items:
    set total to 0
    repeat for each item in items:
        set total to total + item
    respond with total
```

### Calling Behaviors

Use `run` to call another behavior:

```
to process with data:
    run validate with data
    run transform with data
    run save with data
    respond with "done"
```

### Approval Gates

Require human approval for sensitive operations:

```
to delete_database with db_name:
    purpose: "Delete a database"
    needs approval when db_name is equal to "production"
    log "Deleting {{db_name}}..."
    respond with "deleted"

to scale_deployment with namespace and deployment and replicas:
    needs approval when replicas is above 10
    log "Scaling {{deployment}} to {{replicas}}"
    respond with "scaled"
```

---

## 8. Lists and Records

### Lists

```
set colors to ["red", "green", "blue"]
set numbers to [1, 2, 3, 4, 5]
set empty to []
```

**Operations:**

```
append(colors, "yellow")            // adds "yellow" to the end
set length to len(colors)           // 4
set first_item to first(colors)     // "red"
set last_item to last(colors)       // "yellow"
set sub to slice(colors, 1, 3)      // ["green", "blue"]
set idx to index_of(colors, "blue") // 2
set rev to reverse(colors)
set srt to sort(numbers)
set uniq to unique([1, 1, 2, 3])    // [1, 2, 3]
set flat to flatten([[1, 2], [3]])   // [1, 2, 3]

// List concatenation with +
set combined to [1, 2] + [3, 4]     // [1, 2, 3, 4]
set extended to colors + ["orange"] // append item

// Build a list in a loop
set result to []
repeat for each i in range(5):
    set result to result + [i]      // [0, 1, 2, 3, 4]
```

**Aggregate functions:**

```
set total to sum(numbers)           // 15
set avg to average(numbers)         // 3.0
set has_any to any(checks)
set has_all to all(checks)
```

### Records (Maps)

Records are key-value structures created via `define` blocks or `gather` results:

```
define User as:
    name is text
    email is text
    age is number

// Access fields with dot notation:
log "Name: {{user.name}}"
log "Email: {{user.email}}"
```

**Map utility functions:**

```
set k to keys(record)
set v to values(record)
set exists to has_key(record, "name")
```

---

## 9. Built-in Functions

### Math

| Function | Description | Example |
|----------|-------------|---------|
| `abs(x)` | Absolute value | `abs(-5)` â†’ `5` |
| `ceil(x)` | Round up | `ceil(3.2)` â†’ `4` |
| `floor(x)` | Round down | `floor(3.8)` â†’ `3` |
| `round(x)` | Round nearest | `round(3.5)` â†’ `4` |
| `sqrt(x)` | Square root | `sqrt(16)` â†’ `4` |
| `pow(x, y)` | Power | `pow(2, 3)` â†’ `8` |
| `min(a, b)` | Minimum | `min(3, 7)` â†’ `3` |
| `max(a, b)` | Maximum | `max(3, 7)` â†’ `7` |
| `random()` | Random 0-1 | `random()` â†’ `0.42` |
| `cos(x)` | Cosine | `cos(0)` â†’ `1.0` |
| `sin(x)` | Sine | `sin(0)` â†’ `0.0` |
| `log(x)` | Natural log | `log(2.718)` â†’ `1.0` |

### String

| Function | Description | Example |
|----------|-------------|---------|
| `upper(s)` | Uppercase | `upper("hi")` â†’ `"HI"` |
| `lower(s)` | Lowercase | `lower("HI")` â†’ `"hi"` |
| `trim(s)` | Strip whitespace | `trim("  hi  ")` â†’ `"hi"` |
| `len(s)` | Length | `len("hello")` â†’ `5` |
| `contains(s, sub)` | Contains check | `contains("hello", "ell")` â†’ `yes` |
| `starts_with(s, pre)` | Starts with | `starts_with("hello", "he")` â†’ `yes` |
| `ends_with(s, suf)` | Ends with | `ends_with("hello", "lo")` â†’ `yes` |
| `replace(s, old, new)` | Replace | `replace("hi", "i", "ello")` â†’ `"hello"` |
| `split(s, delim)` | Split to list | `split("a,b", ",")` â†’ `["a", "b"]` |
| `join(list, sep)` | Join from list | `join(["a", "b"], "-")` â†’ `"a-b"` |

### Type

| Function | Description | Example |
|----------|-------------|---------|
| `str(x)` | Convert to string | `str(42)` â†’ `"42"` |
| `int(x)` | Convert to integer | `int("10")` â†’ `10` |
| `float(x)` | Convert to float | `float("3.14")` â†’ `3.14` |
| `bool(x)` | Convert to boolean | `bool(1)` â†’ `yes` |
| `type(x)` | Get type name | `type("hi")` â†’ `"text"` |
| `is_text(x)` | Check if text | `is_text("hi")` â†’ `yes` |
| `is_number(x)` | Check if number | `is_number(42)` â†’ `yes` |
| `is_list(x)` | Check if list | `is_list([1])` â†’ `yes` |
| `is_record(x)` | Check if record | â€” |
| `is_none(x)` | Check if nothing | `is_none(nothing)` â†’ `yes` |

### List

| Function | Description |
|----------|-------------|
| `len(list)` | Number of items |
| `append(list, item)` | Add item to end |
| `first(list)` | First element |
| `last(list)` | Last element |
| `slice(list, start, end)` | Sublist |
| `index_of(list, item)` | Position of item |
| `reverse(list)` | Reversed copy |
| `sort(list)` | Sorted copy |
| `unique(list)` | Unique elements |
| `flatten(list)` | Flatten nested lists |
| `sum(list)` | Sum of numbers |
| `average(list)` | Mean of numbers |
| `any(list)` | Any truthy? |
| `all(list)` | All truthy? |
| `count(list, item)` | Count occurrences |
| `range(n)` | `[0, 1, ..., n-1]` |
| `range(start, end)` | `[start, ..., end-1]` |
| `range(start, end, step)` | With step |
| `enumerate(list)` | `[[0, item], ...]` |
| `zip(a, b)` | Pair items |
| `map_list(list, behavior)` | Apply behavior to each |

### Data Encoding / Decoding

| Function | Description |
|----------|-------------|
| `json_encode(x)` | Object â†’ JSON string |
| `json_decode(s)` | JSON string â†’ object |
| `yaml_parse(s)` | YAML string â†’ object |
| `yaml_encode(x)` | Object â†’ YAML string |
| `xml_parse(s)` | XML string â†’ object |
| `xml_encode(x)` | Object â†’ XML string |
| `csv_parse(s)` | CSV string â†’ list |
| `csv_encode(x)` | List â†’ CSV string |
| `toml_parse(s)` | TOML string â†’ object |
| `ini_parse(s)` | INI string â†’ object |
| `parse(s)` | Auto-detect format |
| `encode(x, fmt)` | Encode to `"json"`, `"yaml"`, `"xml"`, or `"csv"` |

### Utility

| Function | Description |
|----------|-------------|
| `uuid()` | Generate a UUID |
| `hash(s)` | Hash a string |
| `base64_encode(s)` | Base64 encode |
| `base64_decode(s)` | Base64 decode |
| `time_now()` | Current timestamp |
| `time_ms()` | Current time in milliseconds |
| `time_format(ts, fmt)` | Format a timestamp |
| `env(name)` | Get environment variable |
| `read_file(path)` | Read file contents |
| `write_file(path, content)` | Write to a file |
| `file_exists(path)` | Check if file exists |
| `print(x)` | Print to console |
| `input(prompt)` | Read user input |
| `chr(n)` | Number â†’ character |
| `ord(c)` | Character â†’ number |
| `hex(n)` | Number â†’ hex string |
| `bin(n)` | Number â†’ binary string |
| `format(fmt, ...)` | String formatting |

### Data Structures

| Function | Description |
|----------|-------------|
| `set_new(list)` | Create set (unique values) from list |
| `set_add(set, val)` | Add member to set |
| `set_has(set, val)` | Check set membership |
| `set_remove(set, val)` | Remove member from set |
| `set_values(set)` | Get set members as list |
| `tuple(a, b, ...)` | Create immutable-style sequence |
| `deque()` / `deque(list)` | Double-ended queue |
| `deque_push_front(dq, v)` | Push to front of deque |
| `deque_pop_front(dq)` | Pop from front of deque |
| `counter(list)` | Count occurrences of each element |
| `default_map(val)` | Map with default value |
| `enumerate(list)` | Index-value pairs: `[[0,a],[1,b]]` |
| `zip(a, b)` | Merge two lists into pairs |

### Data Processing Functions

| Function | Description |
|----------|-------------|
| `map(list, field)` | Extract field from list of records |
| `reduce(list, op)` | Fold list with "+", "*", "min", "max", "join" |
| `items(record)` | Key-value pairs as `[[key, val], ...]` |
| `merge(r1, r2)` | Merge two records (r2 overwrites) |
| `find(list, key, val)` | First record where field = value |
| `group_by(list, field)` | Group records by field |
| `take(list, n)` | First n elements |
| `drop(list, n)` | Skip first n elements |
| `compact(list)` | Remove none values |
| `pluck(list, field)` | Extract field values from records |
| `chunk_list(list, size)` | Split list into chunks |
| `sorted(list)` | Non-mutating sort (returns new list) |
| `reversed(list)` | Non-mutating reverse (returns new list) |
| `repeat_value(val, n)` | List of n copies of val |

### String Enhancement Functions

| Function | Description |
|----------|-------------|
| `title_case(str)` | Convert to Title Case |
| `capitalize(str)` | Capitalize first letter |
| `pad_left(str, width, fill?)` | Left-pad string with fill char |
| `pad_right(str, width, fill?)` | Right-pad string with fill char |
| `char_at(str, index)` | Character at position |
| `repeat_string(str, n)` | Repeat string n times |

### Type & Math Enhancement Functions

| Function | Description |
|----------|-------------|
| `isinstance(val, type)` | Check type by name ("text", "number", "list", etc.) |
| `is_empty(val)` | True if empty string, list, record, or none |
| `clamp(val, lo, hi)` | Clamp value to range |
| `sign(val)` | Returns -1, 0, or 1 |
| `lerp(a, b, t)` | Linear interpolation |
| `gcd(a, b)` | Greatest common divisor |
| `dot_product(v1, v2)` | Vector dot product |
| `linspace(start, end, n)` | Evenly spaced values |
| `to_json(val)` / `from_json(str)` | Aliases for json_encode / json_decode |

### Error & Debug Helpers

| Function | Description |
|----------|-------------|
| `error_type(msg)` | Classify error string â†’ `"TimeoutError"` etc. |
| `traceback()` | Get current call stack as list of frames |

### AI / RAG Helpers

| Function | Description |
|----------|-------------|
| `cosine_similarity(a, b)` | Similarity between vectors |
| `chunk(text, size, overlap)` | Split text into chunks |
| `top_k(items, k)` | Top K items |
| `find_similar(query, vectors, docs, k)` | Nearest neighbor search |
| `token_count(s)` | Count tokens in text |
| `cache(key, val)` | Store in cache |
| `cached(key)` | Retrieve from cache |
| `is_cached(key)` | Check if cached |

---

## 10. Error Handling

### Try / On Error

```
try:
    set result to int("not a number")
    respond with result
on error:
    log "Something went wrong: " + error
    respond with "Error: invalid input"
```

### Try / On Error / Finally

```
try:
    run risky_operation with data
on error:
    log "Failed: " + error
    notify ops_team "Operation failed"
finally:
    log "Cleanup complete"
```

### Typed Catch (Enterprise)

Catch specific error categories with the `catch "ErrorType":` syntax:

```
try:
    gather data from "https://flaky-api.example.com/data"
catch "TimeoutError":
    log "Timed out: " + err.message
    respond with {"error": "timeout", "type": err.type}
catch "ConnectionError":
    respond with {"error": "connection_failed"}
catch "ParseError":
    respond with {"error": "bad_format"}
on_error:
    respond with {"error": error}
finally:
    log "done"
```

**Error types:** `TimeoutError`, `ConnectionError`, `ParseError`, `IndexError`, `ValueError`, `IOError`, `KeyError`

The `err` context object: `err.message`, `err.type`, `err.line`

### Assert

```
assert len(users) is above 0, "Users list should not be empty"
assert response["status"] is equal 200
```

On failure: `Assertion failed at line 15: Users list should not be empty`

### Test Blocks

Isolated test blocks â€” failure doesn't kill the program:

```
test "user creation":
    set user to create_user("Alice", "alice@example.com")
    assert user["name"] is equal "Alice", "Name mismatch"

test "data validation":
    set data to validate({"age": 25})
    assert data["valid"] is equal true
```

Output: `âœ“ PASS: user creation` or `âœ— FAIL: user creation`

### Error Classification

```
set et to error_type("Connection timeout expired")
// Returns "TimeoutError"
```

### Early Return on Error

```
to process with input:
    if input is equal to "":
        respond with "Error: empty input"
    set result to "Processed: " + input
    respond with result
```

---

## 11. Services and APIs

### Service Declaration

Every `.nc` file can declare a service with metadata:

```
service "my-service"
version "1.0.0"
model "nova"
author "Your Name"
description "A service that does amazing things"
```

| Field | Required | Description |
|-------|----------|-------------|
| `service` | Yes | Service name (used in logs and routes) |
| `version` | No | Semantic version |
| `model` | No | Default AI model for `ask AI` |
| `author` | No | Author name |
| `description` | No | Service description |

### Configuration Block

```
configure:
    port: 8080
    confidence_threshold: 0.7
    max_retries: 3
    database_url: "env:DATABASE_URL"
    nested_config:
        timeout: 30
        debug: false
```

Access config values in behaviors: `config.port`, `config.nested_config.timeout`.

Use `"env:VARIABLE_NAME"` to read from environment variables.

### API Routes

Define HTTP endpoints with the `api:` block:

```
api:
    GET    /users      runs list_users
    POST   /users      runs create_user
    GET    /users/:id  runs get_user
    PUT    /users/:id  runs update_user
    DELETE /users/:id  runs delete_user
    GET    /health     runs health_check
```

**Supported methods:** `GET`, `POST`, `PUT`, `DELETE`, `PATCH`, `OPTIONS`

### Starting a Server

```bash
nc serve myservice.nc              # default port 8000
nc serve myservice.nc -p 3000      # custom port
```

### Request Context

Inside behaviors triggered by API routes, you can access:

- `request.method` â€” HTTP method
- `request.path` â€” Request path
- `request.body` â€” Request body
- `request.query` â€” Query parameters
- `auth.user_id` â€” Authenticated user ID
- `auth.tenant_id` â€” Tenant ID
- `auth.role` â€” User role

---

## 12. Type Definitions

Define custom data structures with `define ... as:`:

```
define Ticket as:
    id is text
    title is text
    description is text
    priority is text optional
    created_at is text optional
```

### Field Types

| Type | Description |
|------|-------------|
| `text` | String |
| `number` | Integer or float |
| `yesno` | Boolean (yes/no) |
| `list` | Ordered collection |
| `record` | Nested key-value |

Add `optional` after the type to mark a field as not required:

```
define User as:
    name is text
    email is text
    phone is text optional
    bio is text optional
```

---

## 13. Imports and Modules

### Importing Modules

```
import "logging"
import "json"
import "http/client"
import "http/server"
```

### Import with Alias

```
import "http/client" as http
```

---

## 14. AI Operations

NC treats AI as a first-class language feature. No SDKs, no boilerplate.

### Ask AI

The core AI operation â€” send a prompt with context:

```
ask AI to "classify this support ticket" using ticket:
    confidence: 0.8
    save as: classification
```

```
ask AI to "summarize this document" using document
```

```
ask AI to "analyze the sentiment" using review:
    save as: sentiment
```

**With multiple context variables:**

```
ask AI to "compare these two approaches" using option_a, option_b:
    save as: comparison
```

### Multi-Turn Chat

```
chat with AI:
    say "You are a helpful coding assistant"
    say "How do I sort a list in NC?"
    save as: response
```

### Streaming Responses

```
stream AI "Explain quantum computing in simple terms" save as output
```

### Embeddings

```
embed text save as vector
```

### Image Generation

```
generate image "A sunset over mountains, watercolor style" save as img
```

### Audio Transcription

```
transcribe "recording.mp3" save as text
```

### Content Moderation

```
moderate user_input save as safety
if safety.flagged is true:
    respond with "Content not allowed"
```

### Vision

```
vision "Describe what's in this image" using image_data save as description
```

### Environment Variables for AI

| Variable | Description |
|----------|-------------|
| `NC_AI_URL` | AI API endpoint |
| `NC_AI_KEY` | API key |
| `NC_AI_MODEL` | Default model name |
| `NC_AI_AUTH` | Auth type (`bearer` or `x-api-key`) |

---

## 15. Data Gathering and Storage

### Gather (Read Data)

`gather` retrieves data from external sources:

**From a database:**

```
gather users from database:
    query: "SELECT * FROM users WHERE active = true"
```

**From Kubernetes:**

```
gather pods from kubernetes:
    resource: "pods"
    namespace: "{{config.namespace}}"
```

**From monitoring tools:**

```
gather metrics from prometheus:
    query: "up{job='myapp'}"
    range: "1h"

gather logs from loki:
    query: "{app='myservice'}"
    limit: 100
```

**From MCP (Model Context Protocol):**

```
gather tools from mcp:
    action: "tools/list"
```

**From cache:**

```
gather value from cache:
    key: "session:{{user_id}}"
```

**From a URL:**

```
gather result from "https://api.example.com/data":
    method: "GET"
```

### Store (Write Data)

`store` persists data:

```
store user into "users"
store entry into "audit_log"
store value into "cache:{{key}}"
store nothing into "cache:{{key}}"          // delete from cache
store "open" into "circuit:{{service_name}}" // store a raw value
```

### Apply (Execute Actions)

`apply` executes actions on external systems:

```
apply instruction using kubernetes:
    action: "restart"
    target: "{{pod_name}}"
    namespace: "{{namespace}}"
```

### Check (Verify State)

```
check if deployment is ready using kubernetes:
    save as: verification
```

---

## 16. Events and Scheduling

### Event Handlers

React to events with `on event`:

```
on event "user_signup":
    log "New user: {{event.email}}"
    run send_welcome_email with event

on event "alert.firing":
    run handle_alert with event

on event "circuit_opened":
    notify ops_team "Circuit breaker opened for {{event.service}}"
```

### Emit Events

Trigger events from behaviors:

```
to create_order with order:
    store order into "orders"
    emit "order_created" with order
    respond with "created"
```

### Scheduled Tasks

Run behaviors on a schedule with `every`:

```
every 10 seconds:
    run health_check

every 5 minutes:
    run sync_data

every 1 hours:
    run cleanup_expired_sessions
```

### Notifications

Send notifications to teams or channels:

```
notify ops_team "Deployment complete: {{service.name}}"
notify security_team "Anomaly detected: {{anomaly.summary}}"
notify approvers "Approval needed: {{request.action}}"
```

---

## 17. Middleware

Configure middleware for your API service:

```
middleware:
    auth:
        type: "bearer"
        secret: "env:JWT_SECRET"
    rate_limit:
        requests_per_minute: 100
    cors:
        origins: "*"
    log_requests:
        enabled: true
```

### Supported Middleware

| Middleware | Description |
|-----------|-------------|
| `auth` | JWT, API key, or Bearer authentication |
| `rate_limit` | Per-IP rate limiting |
| `cors` | Cross-Origin Resource Sharing |
| `log_requests` | Request/response logging |
| `webhook_verification` | Verify webhook signatures |
| `tenant` | Multi-tenant isolation via `X-Tenant-ID` |

---

## 18. WebSockets

Enable WebSocket support for real-time communication:

```
service "realtime-chat"
version "1.0.0"

configure:
    websocket_enabled: true

to on_connect with client:
    store client into "active_connections"
    log "Client connected: {{client.id}}"
    emit "client_connected" with client

to on_disconnect with client:
    log "Client disconnected: {{client.id}}"

to broadcast_message with message:
    gather connections from websocket:
        type: "active"
    repeat for each conn in connections:
        send message to conn

to send_to_client with client_id and message:
    gather client from active_connections:
        id: "{{client_id}}"
    send message to client
```

---

## 19. Database Operations

### Environment Setup

| Variable | Description |
|----------|-------------|
| `NC_DATABASE_URL` | Database HTTP endpoint |
| `NC_DATABASE_KEY` | Database API key |
| `NC_DATABASE_AUTH` | Auth type (`bearer` or `x-api-key`) |
| `NC_SQL_URL` | SQL database URL |

### SQL Queries

```
gather users from database:
    query: "SELECT * FROM users WHERE role = 'admin'"

gather count from database:
    query: "SELECT COUNT(*) FROM orders WHERE status = 'pending'"
```

### Key-Value Operations

```
store user into "users:{{user.id}}"
gather user from database:
    key: "users:{{user_id}}"
```

### Search

```
gather results from database:
    collection: "products"
    index: "name"
    query: "laptop"
```

---

## 20. Async, Concurrency & Streaming

NC supports parallel execution, async syntax, and value streaming:

### Parallel Gather

Multiple data sources fetched concurrently:

```
to get_dashboard:
    gather pods from kubernetes:
        resource: "pods"
    gather metrics from prometheus:
        query: "up"
    gather alerts from alertmanager:
        state: "active"
    respond with pods, metrics, alerts
```

### Await (Async Syntax)

Use `await` for future async operations (currently synchronous passthrough):

```
set result to await http_get("https://api.example.com/data")
set processed to await transform(result)
respond with processed
```

### Yield (Value Streaming)

Use `yield` to push intermediate values and output them:

```
to generate_report:
    yield "Step 1: Fetching data..."
    gather data from "https://api.example.com/metrics"
    yield "Step 2: Analyzing..."
    ask AI to "Analyze this data" using data save as analysis
    yield "Step 3: Complete"
    respond with analysis
```

### Configuration

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_WORKERS` | Number of worker threads | CPU cores |

---

## 21. Debugging

### Step-Through Debugger

```bash
nc debug myfile.nc
nc debug myfile.nc -b my_behavior
```

### Debugger Commands

| Command | Description |
|---------|-------------|
| `s` or `step` | Step into next instruction |
| `n` or `next` | Step over |
| `c` or `continue` | Run until next breakpoint |
| `b <line>` | Set breakpoint at line |
| `p <var>` | Print variable value |
| `vars` | Show all variables |
| `bt` | Show call stack |
| `src` or `list` | Show source code around current line |
| `q` or `quit` | Exit debugger |

### Other Analysis Tools

```bash
nc validate myfile.nc    # syntax check
nc analyze myfile.nc     # semantic analysis
nc tokens myfile.nc      # show token stream
nc bytecode myfile.nc    # show compiled bytecodes
nc profile myfile.nc     # run with profiling
```

---

## 22. REPL (Interactive Mode)

Start an interactive session:

```bash
nc repl
```

### REPL Commands

| Command | Description |
|---------|-------------|
| `.help` | Show help |
| `.vars` | Show all variables |
| `.clear` | Clear variables |
| `.history` | Show command history |
| `.quit` / `.exit` | Exit REPL |

### Example Session

```
nc> set name to "Alice"
nc> set greeting to "Hello, " + name + "!"
nc> log greeting
Hello, Alice!
nc> set numbers to [1, 2, 3, 4, 5]
nc> set total to sum(numbers)
nc> log total
15
nc> .vars
name = "Alice"
greeting = "Hello, Alice!"
numbers = [1, 2, 3, 4, 5]
total = 15
```

Multi-line input is supported â€” just end a line with `:` and indent:

```
nc> if total is above 10:
...     log "Big number!"
Big number!
```

---

## 23. CLI Reference

```bash
nc run <file>                    # Run a program
nc run <file> -b <behavior>      # Run a specific behavior
nc run <file> --no-cache         # Run without bytecode cache
nc serve <file>                  # Start HTTP server
nc serve <file> -p <port>        # Start on specific port
nc validate <file>               # Check syntax, show behavior/type/route counts
nc analyze <file>                # Semantic analysis
nc debug <file>                  # Step-through debugger
nc debug <file> -b <behavior>    # Debug specific behavior
nc debug <file> --dap            # Start DAP server for IDE integration
nc bytecode <file>               # Show compiled bytecodes
nc compile <file>                # Generate LLVM IR (.ll)
nc build <file> -o app           # Compile to native binary
nc build .                       # Build all .nc files in current directory
nc build --all -o build/ -j 4    # Parallel batch build to output directory
nc init <name>                   # Scaffold a new NC project
nc setup <name>                  # Create + configure + start server
nc doctor                        # Diagnose project setup
nc tokens <file>                 # Show token stream
nc fmt <file>                    # Auto-format code
nc profile <file>                # Run with profiling
nc test                          # Run all tests in tests/lang/
nc test <file> -v                # Run specific test (verbose)
nc repl                          # Interactive mode
nc lsp                           # Start language server (for IDEs)
nc digest <file.py|.js|.yaml>    # Convert Python/JS/YAML/JSON â†’ NC (offline)
nc migrate <file|dir>            # AI-powered code migration to NC
nc get <url>                     # HTTP GET (curl-like, optional -H)
nc post <url> <body>             # HTTP POST with JSON body
nc conformance                   # Run language conformance suite
nc pkg init                      # Create package manifest
nc pkg install <name>            # Install a package
nc pkg list                      # List installed packages
nc pkg publish                   # Publish to registry
nc -c "code"                     # Execute inline NC code
nc -e "expression"               # Evaluate and print expression
nc version                       # Show version
nc mascot                        # Show the NC mascot
```

---

## 24. Package Management

### Initialize a Package

```bash
nc pkg init
```

This creates an `nc.pkg` manifest with `name`, `version`, `description`, `author`, and `requires`.

### Install Packages

```bash
nc pkg install <package-name>
```

Packages are stored in `.nc_packages/`.

### List Installed Packages

```bash
nc pkg list
```

---

## 25. Environment Variables

### Core

| Variable | Description | Default |
|----------|-------------|---------|
| `NC_SERVICE_PORT` | Server port | `8000` |
| `NC_BASE_PATH` | URL prefix for routes | â€” |
| `NC_LOG_FORMAT` | Log format (`json` for structured) | text |
| `NC_MAX_ITERATIONS` | Max loop iterations | 10,000,000 |
| `NC_WORKERS` | Worker thread count | CPU cores |

### AI

| Variable | Description |
|----------|-------------|
| `NC_AI_URL` | AI API endpoint |
| `NC_AI_KEY` | AI API key |
| `NC_AI_MODEL` | Default model |
| `NC_AI_AUTH` | Auth type |
| `NC_AI_VERSION` | API version |

### Database

| Variable | Description |
|----------|-------------|
| `NC_DATABASE_URL` | Database endpoint |
| `NC_DATABASE_KEY` | Database key |
| `NC_DATABASE_AUTH` | Auth type |
| `NC_SQL_URL` | SQL connection URL |

### MCP

| Variable | Description |
|----------|-------------|
| `NC_MCP_URL` | MCP server URL |

### Observability

| Variable | Description |
|----------|-------------|
| `NC_OTEL_ENDPOINT` | OpenTelemetry collector endpoint |
| `NC_TIMEOUT` | HTTP timeout |
| `NC_RETRIES` | HTTP retry count |

---

## 26. Enterprise Features

### Cryptographic Hashing

```
set digest to hash_sha256("data to hash")            // 64-char hex SHA-256
set stored to hash_password("user_password")          // salted, 10K iterations
set ok to verify_password("user_password", stored)    // constant-time compare
set mac to hash_hmac("payload", "secret_key")         // HMAC-SHA256
```

All implemented in pure C â€” no external dependencies, cross-platform.

### JWT Authentication

```
// Generate a signed token (HS256)
set token to jwt_generate("user_id", "admin", 3600)

// Verify and extract claims
set claims to jwt_verify(token)
if claims is equal to false:
    respond with {"error": "unauthorized", "_status": 401}
log "User: " + claims.sub
```

Requires: `NC_JWT_SECRET` environment variable.

### Session Management

```
set sid to session_create()
session_set(sid, "user", "alice")
set user to session_get(sid, "user")       // "alice"
set valid to session_exists(sid)           // yes
session_destroy(sid)                       // invalidate
```

Sessions auto-expire after `NC_SESSION_TTL` seconds (default 3600).

### Request Context

Access HTTP request details from any behavior:

```
set auth to request_header("Authorization")
set ip to request_ip()
set all to request_headers()               // map of all headers
```

### Feature Flags

```
if feature("new_dashboard"):
    respond with new_ui()
```

Configure: `NC_FF_NEW_DASHBOARD=1` or `NC_FF_BETA=50` (50% rollout).

### Circuit Breaker

```
if circuit_open("external_api"):
    respond with {"error": "service unavailable", "_status": 503}
```

Configure: `NC_CB_FAILURE_THRESHOLD=5`, `NC_CB_TIMEOUT=30`.

### Higher-Order List Operations

```
set sorted to sort_by(items, "score")      // sort records by field
set best to max_by(items, "score")         // record with highest value
set total to sum_by(items, "amount")       // sum a numeric field
set names to map_field(items, "name")      // extract field into list
set winners to filter_by(items, "score", "above", 50)
```

### Sandboxing

Restrict what behaviors can do:

- File read/write access (`NC_ALLOW_FILE_WRITE=1`)
- Network access (`NC_ALLOW_NETWORK=1`)
- Process execution (`NC_ALLOW_EXEC=1`)
- Loop iteration limits (`NC_MAX_LOOP_ITERATIONS`)
- Allowed hosts list (`NC_HTTP_ALLOWLIST`)

### Audit Logging

All operations are logged with: user, action, target, result, IP, tenant, and trace ID.

```
NC_AUDIT_FORMAT=json NC_AUDIT_FILE=/var/log/nc/audit.jsonl nc serve app.nc
```

### Built-in Server Endpoints

| Endpoint | Description |
|----------|-------------|
| `/health` | Liveness check with connection stats |
| `/ready` | Readiness probe (checks worker capacity) |
| `/live` | Simple alive check |
| `/metrics` | Prometheus-compatible metrics |
| `/openapi.json` | Auto-generated API specification |

### Wait with Milliseconds

```
wait 500 milliseconds          // also: ms, millisecond
wait 2 seconds
wait 1 minute
```

### Security Headers

All responses include: `X-Content-Type-Options`, `X-Frame-Options`,
`X-XSS-Protection`, `Referrer-Policy`, `Strict-Transport-Security`.

---

## 27. Complete Examples

### Example 1: Hello World API

```
service "hello-world"
version "1.0.0"

to greet with name:
    purpose: "Say hello to someone"
    respond with "Hello, " + name + "!"

to health_check:
    respond with "healthy"

api:
    GET /hello runs greet
    GET /health runs health_check
```

### Example 2: AI Ticket Classifier

```
service "ticket-classifier"
version "1.0.0"
model "nova"

define Ticket as:
    id is text
    title is text
    description is text
    priority is text optional

to classify with ticket:
    purpose: "Classify a support ticket using AI"
    ask AI to "classify this support ticket into: bug, feature, question" using ticket:
        confidence: 0.7
        save as: classification
    ask AI to "assess the priority as low, medium, high, or critical" using ticket, classification:
        save as: priority
    log "Classified: {{classification.category}} / {{priority.level}}"
    respond with classification

to classify_batch with tickets:
    purpose: "Classify multiple tickets"
    set results to []
    repeat for each ticket in tickets:
        run classify with ticket
        append(results, result)
    respond with results

api:
    POST /classify runs classify
    POST /classify/batch runs classify_batch
    GET /health runs health_check
```

### Example 3: Infrastructure Monitor

```
service "infra-monitor"
version "2.0.0"
model "nova"
description "AI-powered infrastructure monitoring"

configure:
    port: 8090
    namespace: "env:K8S_NAMESPACE"
    check_interval: "30s"

to watch_namespace:
    purpose: "Watch Kubernetes namespace for issues"
    gather pods from kubernetes:
        resource: "pods"
        namespace: "{{config.namespace}}"
    repeat for each pod in pods:
        if pod.status is not equal to "Running":
            set incident to pod
            run diagnose with incident

to diagnose with incident:
    purpose: "Diagnose an infrastructure incident"
    gather logs from kubernetes:
        resource: "logs"
        pod: "{{incident.name}}"
        tail: 100
    ask AI to "diagnose this infrastructure issue" using incident, logs:
        save as: diagnosis
    log "Diagnosis: {{diagnosis.summary}}"
    if diagnosis.confidence is above 0.85:
        run auto_heal with incident and diagnosis
    otherwise:
        notify ops_team "Manual review needed: {{incident.name}}"

to auto_heal with incident and diagnosis:
    purpose: "Automatically heal infrastructure issues"
    needs approval when diagnosis.risk is equal to "high"
    apply diagnosis.fix using kubernetes:
        namespace: "{{config.namespace}}"
        target: "{{incident.name}}"
    check if incident.name is healthy using kubernetes:
        save as: verification
    log "Healed: {{incident.name}} â€” {{verification.status}}"
    respond with verification

every 30 seconds:
    run watch_namespace

on event "alert.firing":
    run diagnose with event

api:
    GET /status runs watch_namespace
    POST /diagnose runs diagnose
    GET /health runs health_check
```

### Example 4: Circuit Breaker

```
service "resilience"
version "1.0.0"

define CircuitBreaker as:
    service is text
    state is text
    failure_count is number
    last_failure is text optional
    reset_at is text optional

to check_circuit with service_name:
    purpose: "Check circuit breaker state"
    gather breaker from cache:
        key: "circuit:{{service_name}}"
    if breaker is equal to nothing:
        respond with "closed"
    if breaker.state is equal to "open":
        if breaker.reset_at is below time_now():
            log "Circuit breaker half-open for {{service_name}}"
            respond with "half-open"
        respond with "open"
    respond with breaker.state

to record_failure with service_name:
    purpose: "Record a service failure"
    gather breaker from cache:
        key: "circuit:{{service_name}}"
    set failures to breaker.failure_count + 1
    if failures is above 5:
        store "open" into "circuit:{{service_name}}"
        log "Circuit breaker OPENED for {{service_name}}"
        emit "circuit_opened" with service_name
        respond with "opened"
    respond with "recorded"

to reset_circuit with service_name:
    purpose: "Reset a circuit breaker"
    store "closed" into "circuit:{{service_name}}"
    respond with "reset"

on event "circuit_opened":
    notify ops_team "Circuit breaker opened for {{event.service}}"

api:
    GET /circuit/:service runs check_circuit
    POST /circuit/:service/fail runs record_failure
    POST /circuit/:service/reset runs reset_circuit
    GET /health runs health_check
```

### Example 5: Simple Calculator (No AI)

```
service "calculator"
version "1.0.0"

to add with a and b:
    respond with a + b

to subtract with a and b:
    respond with a - b

to multiply with a and b:
    respond with a * b

to divide with a and b:
    if b is equal to 0:
        respond with "Error: division by zero"
    respond with a / b

to factorial with n:
    if n is at most 1:
        respond with 1
    set result to 1
    set i to 2
    while i is at most n:
        set result to result * i
        set i to i + 1
    respond with result

api:
    GET /add runs add
    GET /subtract runs subtract
    GET /multiply runs multiply
    GET /divide runs divide
    GET /factorial runs factorial
    GET /health runs health_check
```

---

## Cross-Language Synonym Engine

NC accepts keywords from other programming languages and silently maps them to NC equivalents. This means developers from any background can start writing NC immediately without memorizing new keywords.

### Supported Synonyms

| You write | NC runs | From |
|-----------|---------|------|
| `print "hello"` | `show "hello"` | Python |
| `return x` | `respond with x` | Every language |
| `def greet:` | `to greet:` | Python |
| `function greet:` | `to greet:` | JavaScript |
| `func greet:` | `to greet:` | Go |
| `fn greet:` | `to greet:` | Rust |
| `else:` | `otherwise:` | Every language |
| `elif x:` | `otherwise if x:` | Python |
| `let x to 5` | `set x to 5` | JavaScript |
| `var x to 5` | `set x to 5` | Go, JS |
| `const x to 5` | `set x to 5` | JS |
| `null` | `nothing` | Java, JS, C |
| `nil` | `nothing` | Ruby, Go |
| `None` | `nothing` | Python |
| `catch e:` | `on error e:` | Java, JS |
| `except e:` | `on error e:` | Python |
| `switch val:` | `match val:` | Java, C, Go |
| `case "x":` | `when "x":` | Java, C |
| `break` | `stop` | Every language |
| `continue` | `skip` | Every language |
| `loop:` | `repeat:` | General |
| `class Name:` | `define Name:` | Python, Java |
| `struct Name:` | `define Name:` | Go, Rust, C |
| `require "x"` | `import "x"` | Node.js |
| `include "x"` | `import "x"` | C, PHP |
| `True` | `yes` | Python |
| `False` | `no` | Python |

### Synonym Notices

Set `NC_SYNONYM_NOTICES=1` to see what NC maps:

```bash
$ NC_SYNONYM_NOTICES=1 nc -c 'print "hello"; let x to 10; return x'
  (NC: "print" -> "show")
  (NC: "let" -> "set")
  (NC: "return" -> "respond")
hello
10
```

### Natural English Aliases (Parser Level)

Beyond keyword synonyms, NC also accepts natural English phrasing:

```
add 5 to counter          // same as: set counter to counter + 5
increase counter by 5     // same as: set counter to counter + 5
decrease counter by 3     // same as: set counter to counter - 3
subtract 3 from counter   // same as: set counter to counter - 3
multiply counter by 2     // same as: set counter to counter * 2
divide counter by 2       // same as: set counter to counter / 2
push item to list         // same as: append item to list
insert item into list     // same as: append item to list
delete item from list     // same as: remove item from list
```

---

## Quick Reference Card

```
// Variables
set x to 42
set name to "Alice"
set items to [1, 2, 3]

// Output
log "Hello, {{name}}"
respond with result

// Conditions
if x is above 10:
    log "big"
otherwise:
    log "small"

// Loops
repeat for each item in items:
    log "{{item}}"
repeat 5 times:
    log "again"
while x is above 0:
    set x to x - 1
repeat while x is above 0:
    set x to x - 1

// Functions
to greet with name:
    respond with "Hi, " + name

// Call functions
run greet with "Alice"

// AI
ask AI to "summarize" using data
gather info from source
store data into "key"

// Error handling
try:
    ...
on error:
    log error

// API
api:
    GET /path runs handler

// Events
on event "name":
    ...
every 10 seconds:
    ...

// Comments
// This is a comment
# This is also a comment
```

---

*NC â€” Write services in English, not boilerplate.*

Created by **Nuckala Sai Narender** | **[DevHeal Labs AI](https://devheallabs.in)** | support@devheallabs.in

