#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  NC Full Functional Test Suite
#  Tests every feature using nc commands only
# ═══════════════════════════════════════════════════════════

NC="./engine/build/nc"
P=0; F=0; T=0
pass() { P=$((P+1)); T=$((T+1)); printf "  \033[32mPASS\033[0m  %s\n" "$1"; }
fail() { F=$((F+1)); T=$((T+1)); printf "  \033[31mFAIL\033[0m  %s\n" "$1"; }
run_check() {
    local name="$1"; shift
    local expect="$1"; shift
    local out
    out=$(eval "$@" 2>&1)
    echo "$out" | grep -q "$expect" && pass "$name" || fail "$name"
}

TMP=$(mktemp -d)
trap "rm -rf $TMP; lsof -ti:6789 2>/dev/null | xargs kill -9 2>/dev/null" EXIT

echo ""
echo "  ══════════════════════════════════════════════════"
echo "   NC Full Functional Test Suite"
echo "  ══════════════════════════════════════════════════"

# ═══════════════════════════════════════════════════════════
#  1. CLI
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 1. CLI Commands ──"
run_check "nc version" "v1.0.0" "$NC version"
run_check "nc -c show" "hello" "$NC -c 'show \"hello\"'"
run_check "nc -c math" "42" "$NC -c 'show 40 + 2'"
run_check "nc -c variable" "world" "$NC -c 'set x to \"world\"; show x'"

# ═══════════════════════════════════════════════════════════
#  2. DATA TYPES
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 2. Data Types ──"
cat > "$TMP/types.nc" << 'EOF'
service "types"
version "1.0.0"
to test_text:
    show type("hello")
    respond with type("hello")
to test_number:
    show type(42)
    respond with type(42)
to test_float:
    show type(3.14)
    respond with type(3.14)
to test_bool:
    show type(yes)
    respond with type(yes)
to test_list:
    show type([1, 2])
    respond with type([1, 2])
to test_nothing:
    show type(nothing)
    respond with type(nothing)
EOF
run_check "type: text" "text" "$NC run $TMP/types.nc -b test_text"
run_check "type: number" "number" "$NC run $TMP/types.nc -b test_number"
run_check "type: float" "number" "$NC run $TMP/types.nc -b test_float"
run_check "type: boolean" "yesno" "$NC run $TMP/types.nc -b test_bool"
run_check "type: list" "list" "$NC run $TMP/types.nc -b test_list"
run_check "type: nothing" "nothing" "$NC run $TMP/types.nc -b test_nothing"

# ═══════════════════════════════════════════════════════════
#  3. STRING FUNCTIONS
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 3. String Functions ──"
run_check "upper()" "HELLO" "$NC -c 'show upper(\"hello\")'"
run_check "lower()" "world" "$NC -c 'show lower(\"WORLD\")'"
run_check "trim()" "nc" "$NC -c 'show trim(\"  nc  \")'"
run_check "len() string" "5" "$NC -c 'show len(\"hello\")'"
run_check "contains()" "yes" "$NC -c 'show contains(\"hello world\", \"world\")'"
run_check "starts_with()" "yes" "$NC -c 'show starts_with(\"hello\", \"hel\")'"
run_check "ends_with()" "yes" "$NC -c 'show ends_with(\"hello\", \"llo\")'"
run_check "replace()" "herro" "$NC -c 'show replace(\"hello\", \"l\", \"r\")'"
run_check "join()" "a-b-c" "$NC -c 'show join([\"a\", \"b\", \"c\"], \"-\")'"
run_check "split()" "3" "$NC -c 'set p to split(\"a,b,c\", \",\"); show len(p)'"

# ═══════════════════════════════════════════════════════════
#  4. LIST FUNCTIONS
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 4. List Functions ──"
run_check "len() list" "5" "$NC -c 'show len([1,2,3,4,5])'"
run_check "first()" "10" "$NC -c 'show first([10,20,30])'"
run_check "last()" "30" "$NC -c 'show last([10,20,30])'"
run_check "sum()" "15" "$NC -c 'show sum([1,2,3,4,5])'"
run_check "average()" "3" "$NC -c 'show average([1,2,3,4,5])'"
run_check "reverse()" "3" "$NC -c 'show first(reverse([1,2,3]))'"
run_check "unique()" "3" "$NC -c 'show len(unique([1,1,2,2,3]))'"
run_check "flatten()" "4" "$NC -c 'show len(flatten([[1,2],[3,4]]))'"

# ═══════════════════════════════════════════════════════════
#  5. MATH FUNCTIONS
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 5. Math Functions ──"
run_check "abs()" "42" "$NC -c 'show abs(-42)'"
run_check "sqrt()" "4" "$NC -c 'show sqrt(16)'"
run_check "pow()" "1024" "$NC -c 'show pow(2, 10)'"
run_check "min()" "3" "$NC -c 'show min(3, 7)'"
run_check "max()" "7" "$NC -c 'show max(3, 7)'"
run_check "round()" "4" "$NC -c 'show round(3.7)'"
run_check "ceil()" "4" "$NC -c 'show ceil(3.1)'"
run_check "floor()" "3" "$NC -c 'show floor(3.9)'"

# ═══════════════════════════════════════════════════════════
#  6. TYPE CONVERSION
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 6. Type Conversion ──"
run_check "str()" "42" "$NC -c 'show str(42)'"
run_check "int()" "10" "$NC -c 'show int(\"10\")'"
run_check "is_text()" "yes" "$NC -c 'show is_text(\"hi\")'"
run_check "is_number()" "yes" "$NC -c 'show is_number(42)'"
run_check "is_list()" "yes" "$NC -c 'show is_list([1,2])'"

# ═══════════════════════════════════════════════════════════
#  7. CONTROL FLOW
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 7. Control Flow ──"
cat > "$TMP/control.nc" << 'EOF'
service "control"
version "1.0.0"
to test_if_true:
    set x to 10
    if x is above 5:
        respond with "above"
    otherwise:
        respond with "below"
to test_if_false:
    set x to 3
    if x is above 5:
        respond with "above"
    otherwise:
        respond with "below"
to test_loop:
    set total to 0
    repeat for each n in [10, 20, 30]:
        set total to total + n
    respond with total
to test_while:
    set i to 0
    while i is below 5:
        set i to i + 1
    respond with i
to test_repeat_n:
    set count to 0
    repeat 4 times:
        set count to count + 1
    respond with count
to test_nested_if:
    set score to 85
    if score is above 90:
        respond with "A"
    otherwise:
        if score is above 80:
            respond with "B"
        otherwise:
            respond with "C"
EOF
run_check "if true branch" "above" "$NC run $TMP/control.nc -b test_if_true"
run_check "if false branch" "below" "$NC run $TMP/control.nc -b test_if_false"
run_check "repeat for each" "60" "$NC run $TMP/control.nc -b test_loop"
run_check "while loop" "5" "$NC run $TMP/control.nc -b test_while"
run_check "repeat N times" "4" "$NC run $TMP/control.nc -b test_repeat_n"
run_check "nested if/otherwise" "B" "$NC run $TMP/control.nc -b test_nested_if"

# ═══════════════════════════════════════════════════════════
#  8. BEHAVIORS (FUNCTIONS)
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 8. Behaviors ──"
cat > "$TMP/behaviors.nc" << 'EOF'
service "behaviors"
version "1.0.0"
to add with a and b:
    respond with a + b
to greet with name:
    respond with "Hello, " + name
to factorial with n:
    if n is at most 1:
        respond with 1
    set r to 1
    set i to 2
    while i is at most n:
        set r to r * i
        set i to i + 1
    respond with r
to no_args:
    respond with "works"
EOF
run_check "behavior with 2 params" "Result:" "$NC run $TMP/behaviors.nc -b add"
run_check "behavior with string" "Hello" "$NC run $TMP/behaviors.nc -b greet"
run_check "factorial" "Result:" "$NC run $TMP/behaviors.nc -b factorial"
run_check "behavior no args" "works" "$NC run $TMP/behaviors.nc -b no_args"

# ═══════════════════════════════════════════════════════════
#  9. ERROR HANDLING
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 9. Error Handling ──"
cat > "$TMP/errors.nc" << 'EOF'
service "errors"
version "1.0.0"
to test_try_catch:
    try:
        set x to 1 / 0
    on error:
        respond with "caught"
to test_finally:
    set result to "start"
    try:
        set result to "tried"
    on error:
        set result to "error"
    finally:
        set result to result + "_done"
    respond with result
EOF
run_check "try/on error" "caught" "$NC run $TMP/errors.nc -b test_try_catch"
run_check "try/finally" "done" "$NC run $TMP/errors.nc -b test_finally"

# ═══════════════════════════════════════════════════════════
#  10. JSON
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 10. JSON Operations ──"
cat > "$TMP/json.nc" << 'EOF'
service "json"
version "1.0.0"
to test_encode:
    set data to {"name": "NC", "version": 1}
    respond with json_encode(data)
to test_decode:
    set raw to "{\"lang\":\"NC\"}"
    set parsed to json_decode(raw)
    respond with parsed.lang
EOF
run_check "json_encode()" "NC" "$NC run $TMP/json.nc -b test_encode"
run_check "json_decode()" "NC" "$NC run $TMP/json.nc -b test_decode"

# ═══════════════════════════════════════════════════════════
#  11. FILE I/O
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 11. File I/O ──"
cat > "$TMP/filetest.nc" << 'EOF'
service "filetest"
version "1.0.0"
to test_exists:
    respond with file_exists("filetest.nc")
to test_read:
    set content to read_file("filetest.nc")
    respond with contains(content, "filetest")
EOF
run_check "file_exists()" "yes" "cd $TMP && $NC run filetest.nc -b test_exists"
run_check "read_file()" "yes" "cd $TMP && $NC run filetest.nc -b test_read"

# ═══════════════════════════════════════════════════════════
#  12. SYNONYMS
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 12. Synonym Engine ──"
run_check "def -> to" "synonym" "$NC -c 'def test: respond with \"synonym\"'"
run_check "print -> show" "hello syn" "$NC -c 'print \"hello syn\"'"
run_check "let -> set" "77" "$NC -c 'let x to 77; show x'"
run_check "null -> nothing" "nothing" "$NC -c 'set x to null; show type(x)'"
run_check "return -> respond" "returned" "$NC -c 'def t: return \"returned\"'"

# ═══════════════════════════════════════════════════════════
#  13. CACHE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 13. Cache ──"
run_check "cache/cached" "myval" "$NC -c 'cache(\"k1\", \"myval\"); show cached(\"k1\")'"
run_check "is_cached" "yes" "$NC -c 'cache(\"k2\", \"v\"); show is_cached(\"k2\")'"

# ═══════════════════════════════════════════════════════════
#  14. MEMORY
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 14. Conversation Memory ──"
cat > "$TMP/memory.nc" << 'EOF'
service "memory"
version "1.0.0"
to test_mem:
    set mem to memory_new()
    memory_add(mem, "user", "hello")
    memory_add(mem, "assistant", "hi")
    memory_add(mem, "user", "bye")
    set h to memory_get(mem)
    respond with len(h)
EOF
run_check "memory_new/add/get" "3" "$NC run $TMP/memory.nc -b test_mem"

# ═══════════════════════════════════════════════════════════
#  15. VALIDATE & ANALYZE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 15. Validate & Analyze ──"
cat > "$TMP/valid.nc" << 'EOF'
service "valid"
version "1.0.0"
define User as:
    name is text
    email is text
to greet with name:
    respond with "Hello, " + name
to health_check:
    respond with {"status": "healthy"}
configure:
    port: 8080
middleware:
    cors: true
    rate_limit: 100
api:
    GET /greet runs greet
    GET /health runs health_check
EOF
run_check "nc validate" "VALID" "$NC validate $TMP/valid.nc"
run_check "nc analyze" "greet" "$NC analyze $TMP/valid.nc"

# ═══════════════════════════════════════════════════════════
#  16. BYTECODE & TOKENS
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 16. Bytecode & Tokens ──"
run_check "nc bytecode" "OP_" "$NC bytecode $TMP/valid.nc"
run_check "nc tokens" "TOK_" "$NC tokens $TMP/valid.nc"

# ═══════════════════════════════════════════════════════════
#  17. FORMATTER
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 17. Formatter ──"
cp "$TMP/valid.nc" "$TMP/fmt_test.nc"
FMT_OUT=$($NC fmt "$TMP/fmt_test.nc" 2>&1)
FMT_EXIT=$?
[ $FMT_EXIT -eq 0 ] && pass "nc fmt exit code 0" || fail "nc fmt exit code"

# ═══════════════════════════════════════════════════════════
#  18. COMPILE (LLVM IR)
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 18. Compile ──"
run_check "nc compile" "define\|declare\|LLVM\|generated\|IR" "$NC compile $TMP/valid.nc"

# ═══════════════════════════════════════════════════════════
#  19. DIGEST (PYTHON -> NC)
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 19. Code Digestion ──"
cat > "$TMP/sample.py" << 'PYEOF'
def greet(name):
    return "Hello, " + name

def add(a, b):
    result = a + b
    return result

class User:
    def __init__(self, name, email):
        self.name = name
        self.email = email

print("done")
PYEOF
run_check "nc digest python" "Digested" "$NC digest $TMP/sample.py"
if [ -f "$TMP/sample.nc" ]; then
    run_check "digest: def -> to" "to greet" "cat $TMP/sample.nc"
    run_check "digest: return -> respond" "respond with" "cat $TMP/sample.nc"
    run_check "digest: print -> show" "show" "cat $TMP/sample.nc"
    run_check "digest: class -> define" "define User" "cat $TMP/sample.nc"
else
    fail "digest: output file"; fail "digest: def"; fail "digest: return"; fail "digest: class"
fi

cat > "$TMP/sample.js" << 'JSEOF'
const express = require("express");
function greet(name) {
    return "Hello, " + name;
}
let total = 0;
console.log("done");
JSEOF
run_check "nc digest javascript" "Digested" "$NC digest $TMP/sample.js"

# ═══════════════════════════════════════════════════════════
#  20. SERVICES (nc serve + nc get + nc post)
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 20. HTTP Server ──"
cat > "$TMP/server.nc" << 'EOF'
service "http-test"
version "1.0.0"

to home:
    respond with {"message": "hello from NC", "engine": "bytecode-vm"}

to echo_back with data:
    respond with data

to add_numbers with a and b:
    respond with {"result": a + b}

to greet with name:
    respond with "<html><body><h1>Hello, " + name + "!</h1></body></html>"

to health_check:
    respond with {"status": "healthy"}

api:
    GET / runs home
    POST /echo runs echo_back
    GET /add runs add_numbers
    GET /greet runs greet
    GET /health runs health_check
EOF

$NC serve "$TMP/server.nc" -p 6789 2>/dev/null &
SRV_PID=$!
sleep 3

run_check "serve: health" "healthy" "$NC get http://127.0.0.1:6789/health"
run_check "serve: GET /" "hello from NC" "$NC get http://127.0.0.1:6789/"
run_check "serve: POST echo" "test" "$NC post http://127.0.0.1:6789/echo '{\"test\":\"data\"}'"
run_check "serve: 404 route" "Not Found" "$NC get http://127.0.0.1:6789/nonexistent"
run_check "serve: HTML response" "html" "$NC get http://127.0.0.1:6789/greet"

kill $SRV_PID 2>/dev/null; wait $SRV_PID 2>/dev/null

# ═══════════════════════════════════════════════════════════
#  21. DOCS SITE PAGES
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 21. Website Pages ──"
DOCS_DIR="./docs"
$NC serve "$DOCS_DIR/serve_docs.nc" -p 6789 2>/dev/null &
DOCS_PID=$!
sleep 3

run_check "page: home" "DOCTYPE" "$NC get http://127.0.0.1:6789/"
run_check "page: install" "DOCTYPE" "$NC get http://127.0.0.1:6789/install"
run_check "page: playground" "DOCTYPE" "$NC get http://127.0.0.1:6789/playground"
run_check "page: convert" "DOCTYPE" "$NC get http://127.0.0.1:6789/convert"
run_check "page: about" "DOCTYPE" "$NC get http://127.0.0.1:6789/about"
run_check "page: 404" "DOCTYPE" "$NC get http://127.0.0.1:6789/404.html"
run_check "page: health (JSON)" "healthy" "$NC get http://127.0.0.1:6789/health"

run_check "home has DevHeal Labs" "DevHeal Labs" "$NC get http://127.0.0.1:6789/"
run_check "about has mission" "Our Mission" "$NC get http://127.0.0.1:6789/about"
run_check "convert has converter" "Convert" "$NC get http://127.0.0.1:6789/convert"

kill $DOCS_PID 2>/dev/null; wait $DOCS_PID 2>/dev/null

# ═══════════════════════════════════════════════════════════
#  22. DEFINE TYPES
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 22. Type Definitions ──"
cat > "$TMP/typedef.nc" << 'EOF'
service "typedef"
version "1.0.0"
define Ticket as:
    id is text
    title is text
    priority is number optional
to test_define:
    respond with "define works"
EOF
run_check "define validates" "VALID" "$NC validate $TMP/typedef.nc"
run_check "define runs" "define works" "$NC run $TMP/typedef.nc -b test_define"

# ═══════════════════════════════════════════════════════════
#  23. CONFIGURE BLOCK
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 23. Configure Block ──"
cat > "$TMP/config.nc" << 'EOF'
service "config-test"
version "1.0.0"
configure:
    port: 9999
    debug: yes
    app_name: "test-app"
    max_retries: 3
to test:
    respond with "config ok"
EOF
run_check "configure validates" "VALID" "$NC validate $TMP/config.nc"
run_check "configure runs" "config ok" "$NC run $TMP/config.nc -b test"

# ═══════════════════════════════════════════════════════════
#  24. MIDDLEWARE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 24. Middleware ──"
cat > "$TMP/mw.nc" << 'EOF'
service "mw-test"
version "1.0.0"
to home:
    respond with "ok"
middleware:
    cors: true
    rate_limit: 100
    log_requests: true
    auth: "bearer"
api:
    GET / runs home
EOF
run_check "middleware validates" "VALID" "$NC validate $TMP/mw.nc"

# ═══════════════════════════════════════════════════════════
#  25. REAL-WORLD SERVICE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 25. Real-World Service ──"
cat > "$TMP/realworld.nc" << 'EOF'
service "ticket-classifier"
version "1.0.0"

define Ticket as:
    id is text
    title is text
    description is text
    priority is text optional

configure:
    ai_model: "nova"
    confidence_threshold: 0.7

to classify with ticket:
    purpose: "Classify a support ticket"
    set category to "technical"
    respond with {"category": category, "ticket_id": ticket.id}

to list_tickets:
    set tickets to [
        {"id": "T001", "title": "Login broken"},
        {"id": "T002", "title": "Billing question"},
        {"id": "T003", "title": "Feature request"}
    ]
    respond with tickets

to health_check:
    respond with {"status": "healthy", "service": "ticket-classifier"}

middleware:
    rate_limit: 200
    cors: true

api:
    POST /classify runs classify
    GET /tickets runs list_tickets
    GET /health runs health_check
EOF
run_check "real-world validates" "VALID" "$NC validate $TMP/realworld.nc"
run_check "real-world: classify" "category" "$NC run $TMP/realworld.nc -b classify"
run_check "real-world: list" "T001" "$NC run $TMP/realworld.nc -b list_tickets"
run_check "real-world: health" "healthy" "$NC run $TMP/realworld.nc -b health_check"

# ═══════════════════════════════════════════════════════════
#  26. DATA PIPELINE SERVICE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 26. Data Pipeline Service ──"
cat > "$TMP/pipeline.nc" << 'EOF'
service "data-pipeline"
version "1.0.0"

to process_records:
    set records to [
        {"name": "Alice", "score": 92},
        {"name": "Bob", "score": 78},
        {"name": "Carol", "score": 95},
        {"name": "Dave", "score": 61}
    ]
    set total to 0
    set count to 0
    set high_scorers to []
    repeat for each rec in records:
        set total to total + rec.score
        set count to count + 1
        if rec.score is above 80:
            append(high_scorers, rec.name)
    set avg to total / count
    respond with {
        "total_records": count,
        "average_score": avg,
        "high_scorers": high_scorers
    }

to transform:
    set items to ["hello", "WORLD", "  test  "]
    set result to []
    repeat for each item in items:
        set cleaned to trim(lower(item))
        append(result, cleaned)
    respond with result
EOF
run_check "pipeline validates" "VALID" "$NC validate $TMP/pipeline.nc"
run_check "pipeline: process" "high_scorers" "$NC run $TMP/pipeline.nc -b process_records"
run_check "pipeline: transform" "result" "$NC run $TMP/pipeline.nc -b transform"

# ═══════════════════════════════════════════════════════════
#  27. CALCULATOR SERVICE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 27. Calculator Service ──"
cat > "$TMP/calc.nc" << 'EOF'
service "calculator"
version "1.0.0"

to compute:
    set a to 100
    set b to 25
    set addition to a + b
    set subtraction to a - b
    set multiplication to a * b
    set division to a / b
    respond with {
        "add": addition,
        "sub": subtraction,
        "mul": multiplication,
        "div": division
    }

to stats:
    set data to [10, 20, 30, 40, 50]
    respond with {
        "sum": sum(data),
        "avg": average(data),
        "min": min(first(data), last(data)),
        "max": max(first(data), last(data)),
        "len": len(data)
    }

api:
    GET /compute runs compute
    GET /stats runs stats
EOF
run_check "calc validates" "VALID" "$NC validate $TMP/calc.nc"
run_check "calc: compute" "125" "$NC run $TMP/calc.nc -b compute"
run_check "calc: stats" "150" "$NC run $TMP/calc.nc -b stats"

# ═══════════════════════════════════════════════════════════
#  28. FULL LANGUAGE TEST SUITE
# ═══════════════════════════════════════════════════════════
echo ""; echo "  ── 28. Language Test Suite ──"
LANG_OUT=$($NC test 2>&1)
echo "$LANG_OUT" | grep -q "passed" && pass "nc test — all language tests pass" || fail "nc test"
LANG_COUNT=$(echo "$LANG_OUT" | grep -o "[0-9]* test files" | head -1)
echo "       ($LANG_COUNT)"

# ═══════════════════════════════════════════════════════════
#  RESULTS
# ═══════════════════════════════════════════════════════════
echo ""
echo "  ══════════════════════════════════════════════════"
if [ $F -eq 0 ]; then
    printf "  \033[32m  ALL %d TESTS PASSED\033[0m\n" $T
else
    printf "  \033[31m  %d/%d passed, %d FAILED\033[0m\n" $P $T $F
fi
echo "  ══════════════════════════════════════════════════"
echo ""
exit $F
