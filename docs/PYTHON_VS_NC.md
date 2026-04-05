# Python vs NC — Feature-by-Feature Comparison

> **NC is NOT a Python replacement.** NC is a purpose-built, plain-English programming language designed specifically for building AI-powered services at enterprise scale. Python is a general-purpose language. NC is a domain-specific AI language.

---

## Philosophy

| | Python | NC |
|---|---|---|
| **Purpose** | General-purpose programming | AI-first, plain-English language |
| **Paradigm** | Multi-paradigm (OOP, functional, imperative) | Declarative + imperative, English-native |
| **AI integration** | Library (import nc_ai_client as ai_client) | **Language primitive** (`ask AI to "..."`) |
| **Target** | Everything — web, ML, scripting, data science | AI APIs, AI services, AI pipelines |
| **Syntax** | Python syntax (indentation, keywords) | Plain English (reads like instructions) |
| **Deployment** | Python runtime + pip + virtualenv + gunicorn | One binary (570 KB, zero dependencies) |

---

## Side-by-Side Code Comparison

### AI Email Classifier

**Python (30 lines, 3 imports, 5 packages):**
```python
import nc_ai_client as ai_client, json, os
from flask import Flask, request, jsonify
app = Flask(__name__)
client = ai_client.Client(api_key=os.getenv("NC_AI_KEY"))

@app.route("/classify", methods=["POST"])
def classify():
    email = request.json["email"]
    response = client.chat.completions.create(
        model="nova",
        messages=[{"role": "user", "content": f"Classify: {email}"}]
    )
    result = json.loads(response.choices[0].message.content)
    return jsonify(result)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8080)
```

**NC (10 lines, 0 imports, 0 packages):**
```nc
service "email-classifier"
version "1.0.0"

to classify with email:
    ask AI to "Classify this email into category" using email save as result
    respond with result

api:
    POST /classify runs classify
```

### Error Handling

**Python:**
```python
try:
    response = requests.get("https://api.example.com/data", timeout=5)
    response.raise_for_status()
    data = response.json()
except requests.exceptions.Timeout:
    return {"error": "timeout"}
except requests.exceptions.ConnectionError:
    return {"error": "connection_failed"}
except json.JSONDecodeError:
    return {"error": "bad_format"}
except Exception as e:
    return {"error": str(e)}
finally:
    logging.info("request complete")
```

**NC:**
```nc
try:
    gather data from "https://api.example.com/data"
catch "TimeoutError":
    respond with {"error": "timeout", "type": err.type}
catch "ConnectionError":
    respond with {"error": "connection_failed"}
catch "ParseError":
    respond with {"error": "bad_format"}
on_error:
    respond with {"error": error}
finally:
    log "request complete"
```

### Testing

**Python (requires pytest):**
```python
import pytest

def test_user_creation():
    user = create_user("Alice", "alice@example.com")
    assert user["name"] == "Alice"
    assert user["email"] == "alice@example.com"

def test_data_validation():
    data = validate({"age": 25})
    assert data["valid"] == True
```

**NC (built-in, no framework needed):**
```nc
test "user creation":
    set user to create_user("Alice", "alice@example.com")
    assert user["name"] is equal "Alice", "Name mismatch"
    assert user["email"] is equal "alice@example.com"

test "data validation":
    set data to validate({"age": 25})
    assert data["valid"] is equal true
```

### String Formatting

**Python:**
```python
msg = f"Hello {name}, you have {count} items"
msg2 = "Welcome {name}, role: {role}".format(**vars)
```

**NC:**
```nc
set msg to format("Hello {}, you have {} items", name, count)
set msg2 to format("Welcome {name}, role: {role}", vars)
```

### Data Structures

**Python:**
```python
from collections import Counter, deque

s = set([1, 2, 2, 3])          # {1, 2, 3}
t = (1, "hello", True)         # tuple
d = deque([1, 2, 3])           # deque
d.appendleft(0)                # deque push front
c = Counter(["a", "b", "a"])   # Counter({'a': 2, 'b': 1})
pairs = list(enumerate(items)) # [(0, item), ...]
zipped = list(zip(a, b))       # [(a0, b0), ...]
```

**NC:**
```nc
set s to set_new([1, 2, 2, 3])           // {1, 2, 3}
set t to tuple(1, "hello", true)         // (1, "hello", true)
set d to deque([1, 2, 3])               // deque
set d to deque_push_front(d, 0)          // push front
set c to counter(["a", "b", "a"])        // {"a": 2, "b": 1}
set pairs to enumerate(items)            // [[0, item], ...]
set zipped to zip(a, b)                  // [[a0, b0], ...]
```

### Data Processing

**Python:**
```python
from functools import reduce
import math

# Map + reduce
names = [d["name"] for d in users]
total = reduce(lambda a, b: a + b, prices)

# Find, group, merge
user = next(u for u in users if u["id"] == 42)
groups = {}
for u in users: groups.setdefault(u["dept"], []).append(u)
merged = {**defaults, **overrides}

# List manipulation
first_3 = items[:3]
rest = items[3:]
clean = [x for x in data if x is not None]
chunks = [lst[i:i+3] for i in range(0, len(lst), 3)]
s = sorted(numbers)
r = list(reversed(numbers))

# String manipulation
title = "hello world".title()
padded = "42".rjust(5, "0")
char = "hello"[2]
repeated = "ab" * 3

# Type + math
is_str = isinstance(x, str)
clamped = max(0, min(10, x))
g = math.gcd(12, 8)
dot = sum(a*b for a, b in zip(v1, v2))
```

**NC:**
```nc
// Map + reduce
set names to map(users, "name")
set total_price to reduce(prices, "+")

// Find, group, merge
set user to find(users, "id", 42)
set groups to group_by(users, "dept")
set merged to merge(defaults, overrides)

// List manipulation
set first_3 to take(items_list, 3)
set rest to drop(items_list, 3)
set clean to compact(data)
set chunks to chunk_list(lst, 3)
set s to sorted(numbers)
set r to reversed(numbers)

// String manipulation
set title to title_case("hello world")
set padded to pad_left("42", 5, "0")
set char to char_at("hello", 2)
set repeated to repeat_string("ab", 3)

// Type + math
set is_str to isinstance(x, "text")
set clamped to clamp(x, 0, 10)
set g to gcd(12, 8)
set dot to dot_product(v1, v2)
```

---

## Feature Comparison Table

| Feature | Python | NC | Notes |
|---------|--------|----|-------|
| **AI as language primitive** | No (library) | **Yes** | `ask AI to "..."` is a keyword |
| **HTTP server** | Flask/FastAPI (install) | **Built-in** | `api: POST /path runs handler` |
| **Error handling** | try/except with types | **try/catch with 7 types** | TimeoutError, ConnectionError, etc. |
| **Testing framework** | pytest (install) | **Built-in** | `test "name":` + `assert` |
| **String formatting** | f-strings, .format() | **format()** | Positional + named placeholders |
| **Set** | set() | **set_new()** | set_add, set_has, set_remove, set_values |
| **Tuple** | tuple() | **tuple()** | Immutable-style sequence |
| **Deque** | collections.deque | **deque()** | deque_push_front, deque_pop_front |
| **Counter** | collections.Counter | **counter()** | Count occurrences |
| **enumerate** | enumerate() | **enumerate()** | Index-value pairs |
| **zip** | zip() | **zip()** | Merge two lists |
| **Stack traces** | traceback module | **traceback()** | Built-in, no import |
| **Assert** | assert statement | **assert** | With line numbers + messages |
| **map** | map() + lambda | **map()** | Extract fields from records |
| **reduce** | functools.reduce | **reduce()** | Built-in: +, *, min, max, join |
| **sorted** | sorted() | **sorted()** | Non-mutating sort |
| **reversed** | reversed() | **reversed()** | Non-mutating reverse |
| **items** | dict.items() | **items()** | Returns [[key, val], ...] |
| **merge** | {**a, **b} | **merge()** | Merge two records |
| **find** | next(x for x in...) | **find()** | Find record by field value |
| **group_by** | itertools.groupby | **group_by()** | Group records by field |
| **take/drop** | islice / [n:] | **take() / drop()** | First/skip n elements |
| **compact** | [x for x if x] | **compact()** | Remove none values |
| **pluck** | [d["k"] for d in..] | **pluck()** | Extract field from list of records |
| **chunk** | — (manual) | **chunk_list()** | Split list into chunks |
| **isinstance** | isinstance() | **isinstance()** | Type check by name string |
| **is_empty** | not x / len(x)==0 | **is_empty()** | Check empty string/list/record |
| **title_case** | str.title() | **title_case()** | Title Case String |
| **capitalize** | str.capitalize() | **capitalize()** | Capitalize first letter |
| **pad_left/right** | str.ljust/rjust | **pad_left/pad_right()** | Pad with fill character |
| **char_at** | str[i] | **char_at()** | Character at index |
| **repeat_string** | str * n | **repeat_string()** | Repeat string n times |
| **repeat_value** | [x] * n | **repeat_value()** | List of n copies |
| **clamp** | max(lo,min(hi,x)) | **clamp()** | Clamp value to range |
| **sign** | math.copysign | **sign()** | Returns -1, 0, or 1 |
| **gcd** | math.gcd | **gcd()** | Greatest common divisor |
| **lerp** | — (manual) | **lerp()** | Linear interpolation |
| **dot_product** | numpy.dot | **dot_product()** | Vector dot product |
| **linspace** | numpy.linspace | **linspace()** | Evenly spaced values |
| **to_json/from_json** | json.dumps/loads | **to_json/from_json()** | Aliases for json_encode/decode |
| **JWT auth** | PyJWT (install) | **Built-in** | jwt_generate, jwt_verify |
| **Password hashing** | bcrypt (install) | **Built-in** | hash_password, verify_password |
| **Rate limiting** | flask-limiter (install) | **Built-in** | Sliding window, per-IP |
| **Sessions** | flask-session (install) | **Built-in** | session_create/get/set/destroy |
| **Feature flags** | LaunchDarkly SDK (install) | **Built-in** | feature("flag_name") |
| **Circuit breaker** | pybreaker (install) | **Built-in** | circuit_open("service") |
| **Connection pooling** | httpx/aiohttp | **Built-in** | 32-handle pool with TLS reuse |
| **Thread pool** | gunicorn workers | **Built-in** | 64 workers + 4096 request queue |
| **Async/await** | asyncio + await | **await** | Syntax ready (sync today) |
| **Yield/streaming** | yield (generators) | **yield** | Value accumulator + console |
| **JSON** | import json | **Built-in** | json_encode, json_decode |
| **HTTP client** | requests (install) | **Built-in** | http_get, http_post, http_request |
| **Regex** | import re | **Built-in** | re_match, re_find, re_replace |
| **File I/O** | open() / with | **Built-in** | read_file, write_file, file_exists |
| **Deploy** | Docker + Gunicorn + nginx | **nc deploy** | One command |
| **Binary size** | ~50 MB | **570 KB** | Zero dependencies |
| **Install** | Python + pip + virtualenv | **One binary** | curl install, 5 seconds |
| **Syntax** | Python keywords | **Plain English** | Reads like instructions |

---

## What NC Has That Python Doesn't (Built-in)

| Feature | Python Needs | NC Has |
|---------|-------------|--------|
| AI as keyword | `openai` SDK | `ask AI to "..."` |
| HTTP server | Flask + Gunicorn | `api:` block |
| Auto-correct typos | — | Damerau-Levenshtein distance |
| Keyword synonyms | — | `else`/`otherwise`, `def`/`to`, etc. |
| API key auth | middleware packages | Built into `middleware:` |
| Code digestion | — | `nc digest app.py` converts Python → NC |
| AI provider-agnostic | per-SDK code | JSON config file, no recompile |
| Template engine | Jinja2 | `{{placeholder}}` native |
| One-file deploy | — | One `.nc` file = complete service |
| Linear interpolation | Manual calculation | `lerp(a, b, t)` built-in |
| Vector dot product | numpy (install) | `dot_product()` built-in |
| Evenly spaced values | numpy.linspace (install) | `linspace()` built-in |
| Value clamping | Manual `max(min())` | `clamp(val, lo, hi)` built-in |

## What Python Has That NC Doesn't (Yet)

| Feature | Status in NC |
|---------|-------------|
| OOP (classes, inheritance) | Planned for v2.0 |
| Decorators | Not planned (English syntax replaces) |
| List comprehensions | **Covered**: map() + filter() + reduce() |
| Package ecosystem (PyPI) | Growing (nc pkg install) |
| NumPy/Pandas | **Partial**: dot_product, linspace, sum, average built-in |
| REPL auto-complete | Basic REPL exists, IDE via LSP |
| Generators (lazy) | yield exists (eager accumulator) |
| Multiple return values | **Covered**: use record or list |
| Lambda functions | **Covered**: map/filter/reduce with string ops |

---

## Enterprise Traffic Handling — NC vs Python

| Metric | Python (Flask + Gunicorn) | NC |
|--------|--------------------------|-----|
| **Startup time** | ~200ms | **~7ms** |
| **Memory footprint** | ~30 MB+ | **~2 MB** |
| **Binary/deploy size** | ~50 MB+ | **570 KB** |
| **Concurrent connections** | gunicorn workers (fixed) | **Thread pool (64) + request queue (4096)** |
| **Rate limiting** | flask-limiter package | **Built-in sliding window** |
| **Keep-alive** | gunicorn setting | **Built-in (100 req/conn, 30s idle)** |
| **I/O multiplexing** | asyncio/uvloop | **epoll/kqueue/select (built-in)** |
| **Connection pooling** | httpx pool | **32-handle pool + TLS reuse** |
| **Chunked transfer** | manual | **Auto for >64KB responses** |
| **Graceful shutdown** | gunicorn signal handlers | **Built-in (SIGINT/SIGTERM drain)** |
| **Security headers** | flask-talisman package | **Automatic (HSTS, CSP, X-Frame, etc.)** |
| **Health endpoints** | manual routes | **Automatic (/health, /ready, /metrics)** |
| **Docker image** | ~150 MB+ | **22.9 MB (Alpine)** |

---

## When to Use NC vs Python

### Use NC When:
- Building AI-powered APIs and services
- You want zero-dependency deployment
- Your team includes non-developers who need to read/write code
- Enterprise features (auth, rate limiting, sessions) are needed out-of-the-box
- Fast iteration on AI prompt engineering
- Microservices that talk to LLMs

### Use Python When:
- Data science and ML model training (NumPy, Pandas, scikit-learn)
- Large existing Python codebase
- Need the PyPI ecosystem (200,000+ packages)
- Desktop GUI applications
- System scripting and automation
- Academic/research work

### Use Both:
NC can call Python models via `load_model()` / `predict()` and `exec()` / `shell()`. Use Python to train your ML models, then deploy them via NC services.

```nc
// Use a Python-trained model in an NC service
to predict_sentiment with text:
    set model to load_model("sentiment_model.pkl")
    set score to predict(model, text)
    respond with {"sentiment": score}
```

---

*NC is created by **Nuckala Sai Narender**, Founder & CEO of **[DevHeal Labs AI](https://devheallabs.in)***
