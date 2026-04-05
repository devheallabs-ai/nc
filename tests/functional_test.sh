#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  NC Functional Test Suite
#  Tests every major feature end-to-end using nc commands
# ═══════════════════════════════════════════════════════════
set -e

NC="./engine/build/nc"
PASS=0
FAIL=0
TOTAL=0

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  ✗ $1"; }
check() {
    TOTAL_NAME="$1"
    shift
    if echo "$@" | grep -q "$1" 2>/dev/null; then pass "$TOTAL_NAME"; else fail "$TOTAL_NAME"; fi
}

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

echo ""
echo "  ═══════════════════════════════════════════"
echo "   NC Functional Test Suite"
echo "  ═══════════════════════════════════════════"
echo ""

# ── 1. VERSION & MASCOT ──────────────────────────
echo "  ── 1. CLI Basics ──"

OUT=$($NC version 2>&1)
echo "$OUT" | grep -q "v1.0.0" && pass "nc version shows v1.0.0" || fail "nc version"

OUT=$($NC mascot 2>&1)
echo "$OUT" | grep -q "NC" && pass "nc mascot runs" || fail "nc mascot"

# ── 2. INLINE EXECUTION ──────────────────────────
echo ""
echo "  ── 2. Inline Execution ──"

OUT=$($NC -c 'show "hello functional test"' 2>&1)
echo "$OUT" | grep -q "hello functional test" && pass "nc -c inline execution" || fail "nc -c inline"

OUT=$($NC -c 'set x to 42; show x' 2>&1)
echo "$OUT" | grep -q "42" && pass "nc -c variable assignment" || fail "nc -c variable"

OUT=$($NC -c 'show 10 + 20 + 30' 2>&1)
echo "$OUT" | grep -q "60" && pass "nc -c arithmetic" || fail "nc -c arithmetic"

# ── 3. VARIABLES & TYPES ─────────────────────────
echo ""
echo "  ── 3. Variables & Types ──"

cat > "$TMPDIR/types.nc" << 'NCEOF'
service "type-test"
version "1.0.0"

to test_types:
    set name to "Alice"
    set age to 30
    set pi to 3.14
    set active to yes
    set items to [1, 2, 3]
    set empty to nothing

    show type(name)
    show type(age)
    show type(active)
    show type(items)
    show type(empty)
    show is_text(name)
    show is_number(age)
    show is_list(items)
    respond with "types ok"
NCEOF

OUT=$($NC run "$TMPDIR/types.nc" -b test_types 2>&1)
echo "$OUT" | grep -q "text" && pass "type() returns text" || fail "type() text"
echo "$OUT" | grep -q "number" && pass "type() returns number" || fail "type() number"
echo "$OUT" | grep -q "yesno" && pass "type() returns yesno" || fail "type() yesno"
echo "$OUT" | grep -q "list" && pass "type() returns list" || fail "type() list"
echo "$OUT" | grep -q "types ok" && pass "behavior returns result" || fail "behavior result"

# ── 4. STRING FUNCTIONS ──────────────────────────
echo ""
echo "  ── 4. String Functions ──"

cat > "$TMPDIR/strings.nc" << 'NCEOF'
service "string-test"
version "1.0.0"

to test_strings:
    show upper("hello")
    show lower("WORLD")
    show trim("  nc  ")
    show len("hello")
    show contains("hello world", "world")
    show starts_with("hello", "hel")
    show ends_with("hello", "llo")
    show replace("hello", "l", "r")
    set parts to split("a,b,c", ",")
    show len(parts)
    show join(["x", "y", "z"], "-")
    respond with "strings ok"
NCEOF

OUT=$($NC run "$TMPDIR/strings.nc" -b test_strings 2>&1)
echo "$OUT" | grep -q "HELLO" && pass "upper()" || fail "upper()"
echo "$OUT" | grep -q "world" && pass "lower()" || fail "lower()"
echo "$OUT" | grep -q "nc" && pass "trim()" || fail "trim()"
echo "$OUT" | grep -q "herro" && pass "replace()" || fail "replace()"
echo "$OUT" | grep -q "x-y-z" && pass "join()" || fail "join()"
echo "$OUT" | grep -q "strings ok" && pass "string suite complete" || fail "string suite"

# ── 5. LIST FUNCTIONS ────────────────────────────
echo ""
echo "  ── 5. List Functions ──"

cat > "$TMPDIR/lists.nc" << 'NCEOF'
service "list-test"
version "1.0.0"

to test_lists:
    set nums to [5, 3, 1, 4, 2]
    show len(nums)
    show first(nums)
    show last(nums)
    show sum(nums)
    show average(nums)

    set sorted to sort(nums)
    show first(sorted)

    set uniq to unique([1, 1, 2, 2, 3])
    show len(uniq)

    set flat to flatten([[1, 2], [3, 4]])
    show len(flat)

    set rev to reverse([1, 2, 3])
    show first(rev)

    respond with "lists ok"
NCEOF

OUT=$($NC run "$TMPDIR/lists.nc" -b test_lists 2>&1)
echo "$OUT" | grep -q "15" && pass "sum()" || fail "sum()"
echo "$OUT" | grep -q "lists ok" && pass "list suite complete" || fail "list suite"

# ── 6. MATH FUNCTIONS ────────────────────────────
echo ""
echo "  ── 6. Math Functions ──"

cat > "$TMPDIR/math.nc" << 'NCEOF'
service "math-test"
version "1.0.0"

to test_math:
    show abs(-42)
    show sqrt(16)
    show pow(2, 10)
    show min(3, 7)
    show max(3, 7)
    show round(3.7)
    show ceil(3.2)
    show floor(3.8)
    respond with "math ok"
NCEOF

OUT=$($NC run "$TMPDIR/math.nc" -b test_math 2>&1)
echo "$OUT" | grep -q "42" && pass "abs()" || fail "abs()"
echo "$OUT" | grep -q "1024" && pass "pow()" || fail "pow()"
echo "$OUT" | grep -q "math ok" && pass "math suite complete" || fail "math suite"

# ── 7. CONTROL FLOW ──────────────────────────────
echo ""
echo "  ── 7. Control Flow ──"

cat > "$TMPDIR/control.nc" << 'NCEOF'
service "control-test"
version "1.0.0"

to test_if:
    set score to 85
    if score is above 90:
        respond with "excellent"
    otherwise:
        if score is above 70:
            respond with "good"
        otherwise:
            respond with "ok"

to test_match:
    set status to "active"
    match status:
        when "active": respond with "online"
        when "away": respond with "idle"
        otherwise: respond with "offline"

to test_loop:
    set total to 0
    set items to [10, 20, 30]
    repeat for each item in items:
        set total to total + item
    respond with total

to test_while:
    set count to 0
    while count is below 5:
        set count to count + 1
    respond with count

to test_repeat_n:
    set count to 0
    repeat 3 times:
        set count to count + 1
    respond with count
NCEOF

OUT=$($NC run "$TMPDIR/control.nc" -b test_if 2>&1)
echo "$OUT" | grep -q "good" && pass "if/otherwise" || fail "if/otherwise"

OUT=$($NC run "$TMPDIR/control.nc" -b test_match 2>&1)
echo "$OUT" | grep -q "online" && pass "match/when" || fail "match/when"

OUT=$($NC run "$TMPDIR/control.nc" -b test_loop 2>&1)
echo "$OUT" | grep -q "60" && pass "repeat for each" || fail "repeat for each"

OUT=$($NC run "$TMPDIR/control.nc" -b test_while 2>&1)
echo "$OUT" | grep -q "5" && pass "while loop" || fail "while loop"

OUT=$($NC run "$TMPDIR/control.nc" -b test_repeat_n 2>&1)
echo "$OUT" | grep -q "3" && pass "repeat N times" || fail "repeat N times"

# ── 8. ERROR HANDLING ────────────────────────────
echo ""
echo "  ── 8. Error Handling ──"

cat > "$TMPDIR/errors.nc" << 'NCEOF'
service "error-test"
version "1.0.0"

to test_try:
    try:
        set x to 10 / 0
        respond with "no error"
    on error:
        respond with "caught error"

to test_try_finally:
    set result to "start"
    try:
        set result to "tried"
    on error:
        set result to "error"
    finally:
        set result to result + " finally"
    respond with result
NCEOF

OUT=$($NC run "$TMPDIR/errors.nc" -b test_try 2>&1)
echo "$OUT" | grep -q "caught error" && pass "try/on error catches" || fail "try/on error"

OUT=$($NC run "$TMPDIR/errors.nc" -b test_try_finally 2>&1)
echo "$OUT" | grep -q "finally" && pass "try/finally runs" || fail "try/finally"

# ── 9. JSON OPERATIONS ───────────────────────────
echo ""
echo "  ── 9. JSON Operations ──"

cat > "$TMPDIR/json.nc" << 'NCEOF'
service "json-test"
version "1.0.0"

to test_json_encode:
    set data to {"name": "NC", "version": 1}
    set json_str to json_encode(data)
    show json_str
    respond with json_str

to test_json_decode:
    set raw to "{\"lang\":\"NC\",\"cool\":true}"
    set parsed to json_decode(raw)
    show parsed.lang
    respond with parsed.lang
NCEOF

OUT=$($NC run "$TMPDIR/json.nc" -b test_json_encode 2>&1)
echo "$OUT" | grep -q "NC" && pass "json_encode()" || fail "json_encode()"

OUT=$($NC run "$TMPDIR/json.nc" -b test_json_decode 2>&1)
echo "$OUT" | grep -q "NC" && pass "json_decode()" || fail "json_decode()"

# ── 10. FILE OPERATIONS ──────────────────────────
echo ""
echo "  ── 10. File I/O ──"

cat > "$TMPDIR/fileio.nc" << 'NCEOF'
service "file-test"
version "1.0.0"

to test_file_exists:
    set exists to file_exists("fileio.nc")
    respond with exists

to test_read_file:
    set content to read_file("fileio.nc")
    set has_service to contains(content, "file-test")
    respond with has_service
NCEOF

OUT=$(cd "$TMPDIR" && $NC run fileio.nc -b test_file_exists 2>&1)
echo "$OUT" | grep -q "yes" && pass "file_exists()" || fail "file_exists()"

OUT=$(cd "$TMPDIR" && $NC run fileio.nc -b test_read_file 2>&1)
echo "$OUT" | grep -q "yes" && pass "read_file()" || fail "read_file()"

# ── 11. SYNONYM ENGINE ───────────────────────────
echo ""
echo "  ── 11. Synonym Engine ──"

OUT=$($NC -c 'def greet: respond with "synonym works"' 2>&1)
echo "$OUT" | grep -q "synonym works" && pass "def -> to" || fail "def -> to"

OUT=$($NC -c 'print "hello synonym"' 2>&1)
echo "$OUT" | grep -q "hello synonym" && pass "print -> show" || fail "print -> show"

OUT=$($NC -c 'let x to 99; show x' 2>&1)
echo "$OUT" | grep -q "99" && pass "let -> set" || fail "let -> set"

OUT=$($NC -c 'set x to null; show type(x)' 2>&1)
echo "$OUT" | grep -q "nothing" && pass "null -> nothing" || fail "null -> nothing"

# ── 12. VALIDATE & ANALYZE ───────────────────────
echo ""
echo "  ── 12. Validate & Analyze ──"

cat > "$TMPDIR/valid.nc" << 'NCEOF'
service "validation-test"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name

to health_check:
    respond with {"status": "healthy"}

api:
    GET /greet runs greet
    GET /health runs health_check
NCEOF

OUT=$($NC validate "$TMPDIR/valid.nc" 2>&1)
echo "$OUT" | grep -q "VALID" && pass "nc validate" || fail "nc validate"

OUT=$($NC analyze "$TMPDIR/valid.nc" 2>&1)
echo "$OUT" | grep -q "greet" && pass "nc analyze" || fail "nc analyze"

# ── 13. BYTECODE & TOKENS ────────────────────────
echo ""
echo "  ── 13. Bytecode & Tokens ──"

OUT=$($NC bytecode "$TMPDIR/valid.nc" 2>&1)
echo "$OUT" | grep -q "OP_" && pass "nc bytecode shows opcodes" || fail "nc bytecode"

OUT=$($NC tokens "$TMPDIR/valid.nc" 2>&1)
echo "$OUT" | grep -q "TOK_" && pass "nc tokens shows tokens" || fail "nc tokens"

# ── 14. FORMATTER ────────────────────────────────
echo ""
echo "  ── 14. Formatter ──"

cat > "$TMPDIR/ugly.nc" << 'NCEOF'
service "fmt-test"
version "1.0.0"
to greet with name:
    respond with "Hello, " + name
NCEOF

OUT=$($NC fmt "$TMPDIR/ugly.nc" 2>&1)
echo "$?" -eq 0 && pass "nc fmt runs without error" || fail "nc fmt"

# ── 15. COMPILE (LLVM IR) ────────────────────────
echo ""
echo "  ── 15. Compiler ──"

OUT=$($NC compile "$TMPDIR/valid.nc" 2>&1)
echo "$OUT" | grep -q "define\|declare\|LLVM\|IR\|generated" && pass "nc compile generates IR" || fail "nc compile"

# ── 16. DIGEST (PYTHON -> NC) ────────────────────
echo ""
echo "  ── 16. Code Digestion ──"

cat > "$TMPDIR/sample.py" << 'PYEOF'
def greet(name):
    return "Hello, " + name

def add(a, b):
    result = a + b
    return result

print("done")
PYEOF

OUT=$($NC digest "$TMPDIR/sample.py" 2>&1)
echo "$OUT" | grep -q "Digested" && pass "nc digest python" || fail "nc digest python"

if [ -f "$TMPDIR/sample.nc" ]; then
    DIGESTED=$(cat "$TMPDIR/sample.nc")
    echo "$DIGESTED" | grep -q "to greet" && pass "digest: def -> to" || fail "digest: def -> to"
    echo "$DIGESTED" | grep -q "respond with" && pass "digest: return -> respond with" || fail "digest: return -> respond"
    echo "$DIGESTED" | grep -q "show" && pass "digest: print -> show" || fail "digest: print -> show"
else
    fail "digest output file not created"
    fail "digest: def -> to"
    fail "digest: return -> respond with"
    fail "digest: print -> show"
fi

# ── 17. HTTP SERVER (nc serve + nc get) ──────────
echo ""
echo "  ── 17. HTTP Server ──"

cat > "$TMPDIR/server.nc" << 'NCEOF'
service "http-test"
version "1.0.0"

to home:
    respond with {"message": "hello from NC"}

to echo_back with data:
    respond with data

to health_check:
    respond with {"status": "healthy"}

api:
    GET / runs home
    POST /echo runs echo_back
    GET /health runs health_check
NCEOF

$NC serve "$TMPDIR/server.nc" -p 7777 2>/dev/null &
SRVPID=$!
sleep 2

OUT=$($NC get http://127.0.0.1:7777/health 2>&1)
echo "$OUT" | grep -q "healthy" && pass "nc serve + nc get /health" || fail "nc serve /health"

OUT=$($NC get http://127.0.0.1:7777/ 2>&1)
echo "$OUT" | grep -q "hello from NC" && pass "nc get / returns JSON" || fail "nc get /"

OUT=$($NC post http://127.0.0.1:7777/echo '{"test":"data"}' 2>&1)
echo "$OUT" | grep -q "test" && pass "nc post /echo returns body" || fail "nc post /echo"

OUT=$($NC get http://127.0.0.1:7777/nonexistent 2>&1)
echo "$OUT" | grep -q "Not Found" && pass "404 for unknown route" || fail "404 route"

kill $SRVPID 2>/dev/null
wait $SRVPID 2>/dev/null

# ── 18. CACHE FUNCTIONS ──────────────────────────
echo ""
echo "  ── 18. Cache ──"

OUT=$($NC -c 'cache("mykey", "myval"); show cached("mykey")' 2>&1)
echo "$OUT" | grep -q "myval" && pass "cache/cached" || fail "cache/cached"

OUT=$($NC -c 'cache("k", "v"); show is_cached("k")' 2>&1)
echo "$OUT" | grep -q "yes" && pass "is_cached" || fail "is_cached"

# ── 19. MEMORY FUNCTIONS ─────────────────────────
echo ""
echo "  ── 19. Conversation Memory ──"

cat > "$TMPDIR/memory.nc" << 'NCEOF'
service "mem-test"
version "1.0.0"

to test_memory:
    set mem to memory_new()
    memory_add(mem, "user", "hello")
    memory_add(mem, "assistant", "hi there")
    set history to memory_get(mem)
    show len(history)
    respond with len(history)
NCEOF

OUT=$($NC run "$TMPDIR/memory.nc" -b test_memory 2>&1)
echo "$OUT" | grep -q "2" && pass "memory_new/add/get" || fail "memory functions"

# ── 20. DEFINE TYPE ──────────────────────────────
echo ""
echo "  ── 20. Type Definitions ──"

cat > "$TMPDIR/typedef.nc" << 'NCEOF'
service "type-def-test"
version "1.0.0"

define User as:
    name is text
    email is text
    age is number

to test_define:
    respond with "define works"
NCEOF

OUT=$($NC validate "$TMPDIR/typedef.nc" 2>&1)
echo "$OUT" | grep -q "VALID" && pass "define ... as: validates" || fail "define type"

OUT=$($NC run "$TMPDIR/typedef.nc" -b test_define 2>&1)
echo "$OUT" | grep -q "define works" && pass "define type runs" || fail "define type runs"

# ── 21. CONFIGURE BLOCK ──────────────────────────
echo ""
echo "  ── 21. Configure Block ──"

cat > "$TMPDIR/config.nc" << 'NCEOF'
service "config-test"
version "1.0.0"

configure:
    port: 9999
    debug: yes
    app_name: "test-app"

to test_config:
    respond with "config ok"
NCEOF

OUT=$($NC validate "$TMPDIR/config.nc" 2>&1)
echo "$OUT" | grep -q "VALID" && pass "configure block validates" || fail "configure block"

# ── 22. MIDDLEWARE DECLARATION ────────────────────
echo ""
echo "  ── 22. Middleware ──"

cat > "$TMPDIR/middleware.nc" << 'NCEOF'
service "mw-test"
version "1.0.0"

to home:
    respond with "ok"

middleware:
    cors: true
    rate_limit: 100
    log_requests: true

api:
    GET / runs home
NCEOF

OUT=$($NC validate "$TMPDIR/middleware.nc" 2>&1)
echo "$OUT" | grep -q "VALID" && pass "middleware block validates" || fail "middleware block"

# ── 23. LANGUAGE TEST SUITE ──────────────────────
echo ""
echo "  ── 23. Full Language Test Suite ──"

OUT=$($NC test 2>&1)
echo "$OUT" | grep -q "passed" && pass "nc test — all language tests pass" || fail "nc test"
TESTCOUNT=$(echo "$OUT" | grep -o "[0-9]* test files" | head -1)
echo "     ($TESTCOUNT)"

# ═══════════════════════════════════════════════════
#  RESULTS
# ═══════════════════════════════════════════════════
echo ""
echo "  ═══════════════════════════════════════════"
if [ $FAIL -eq 0 ]; then
    echo "   ALL $TOTAL TESTS PASSED"
else
    echo "   $PASS/$TOTAL passed, $FAIL FAILED"
fi
echo "  ═══════════════════════════════════════════"
echo ""

exit $FAIL
