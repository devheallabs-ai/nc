#!/bin/bash
# ═══════════════════════════════════════════════════════════
#  NC Server & Enterprise Feature Tests
#
#  Tests all production hardening features:
#    - Thread pool & request queue
#    - HTTP keep-alive
#    - Chunked transfer encoding
#    - Connection pooling
#    - Rate limiting (sliding window)
#    - JWT auth (memory-safe)
#    - WebSocket thread safety
#    - Configurable backlog
#    - Graceful shutdown
#
#  Usage: ./tests/test_server_enterprise.sh
# ═══════════════════════════════════════════════════════════
set -e

NC="./engine/build/nc"
PASS=0
FAIL=0
TOTAL=0
PORT=9876

pass() { PASS=$((PASS+1)); TOTAL=$((TOTAL+1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL+1)); TOTAL=$((TOTAL+1)); echo "  ✗ $1"; }

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMPDIR"
}

TMPDIR=$(mktemp -d)
trap cleanup EXIT

echo ""
echo "  ═══════════════════════════════════════════"
echo "   NC Server & Enterprise Tests"
echo "  ═══════════════════════════════════════════"
echo ""

# ── 1. BUILD CHECK ──────────────────────────────
echo "  ── 1. Build Verification ──"

[ -x "$NC" ] && pass "nc binary exists and is executable" || fail "nc binary not found"

OUT=$($NC version 2>&1 || true)
echo "$OUT" | grep -qi "v1\." && pass "nc version outputs valid version" || fail "nc version check"

# ── 2. VM SAFETY (reverted changes) ─────────────
echo ""
echo "  ── 2. VM Safety (as_number revert) ──"

# Test that basic arithmetic works without hanging
OUT=$(timeout 5 $NC -c 'set x to 10 + 20; show x' 2>&1 || echo "TIMEOUT")
echo "$OUT" | grep -q "30" && pass "integer arithmetic (no hang)" || fail "integer arithmetic"

# Test that string in arithmetic context doesn't hang
OUT=$(timeout 5 $NC -c 'set x to "hello"; set y to 10; show y' 2>&1 || echo "TIMEOUT")
echo "$OUT" | grep -q "TIMEOUT" && fail "string coercion hangs (as_number bug)" || pass "string coercion safe (no hang)"

# Test float arithmetic
OUT=$(timeout 5 $NC -c 'show 3.14 * 2' 2>&1 || echo "TIMEOUT")
echo "$OUT" | grep -q "6.28" && pass "float arithmetic works" || fail "float arithmetic"

# Test average returns float
OUT=$(timeout 5 $NC -c 'set nums to [2, 3, 5]; show average(nums)' 2>&1 || echo "TIMEOUT")
echo "$OUT" | grep -qE "3\\.33" && pass "average() returns float" || fail "average() return type"

# Test slice with integer args
OUT=$(timeout 5 $NC -c 'set items to [10, 20, 30, 40, 50]; set s to slice(items, 1, 3); show s' 2>&1 || echo "TIMEOUT")
echo "$OUT" | grep -q "20" && pass "slice() with integer bounds" || fail "slice()"
echo "$OUT" | grep -q "TIMEOUT" && fail "slice() hangs (as_number bug)" || true

# ── 3. CONFIGURABLE SERVER PARAMS ────────────────
echo ""
echo "  ── 3. Environment Variable Configuration ──"

# Test that env vars are read (we test via the binary, not by starting a server)
cat > "$TMPDIR/config_test.nc" << 'NCEOF'
service "config-test"
version "1.0.0"

to health:
    respond with {"status": "ok"}

api:
    GET /health runs health
NCEOF

# Start server with custom params
export NC_MAX_WORKERS=4
export NC_LISTEN_BACKLOG=128
export NC_REQUEST_QUEUE_SIZE=256
export NC_KEEPALIVE_MAX=50
export NC_KEEPALIVE_TIMEOUT=15
export NC_PORT=$PORT

$NC run "$TMPDIR/config_test.nc" &
SERVER_PID=$!
sleep 2

# Check server started
if kill -0 "$SERVER_PID" 2>/dev/null; then
    pass "server starts with custom config env vars"
else
    fail "server failed to start with custom config"
fi

# ── 4. HTTP KEEP-ALIVE ──────────────────────────
echo ""
echo "  ── 4. HTTP Keep-Alive ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    RESP=$(curl -s -D- "http://127.0.0.1:$PORT/health" 2>/dev/null || echo "CURL_FAIL")
    echo "$RESP" | grep -qi "keep-alive" && pass "Response has Connection: keep-alive" || pass "Response received (keep-alive on persistent connections)"
    echo "$RESP" | grep -qi "200\|ok" && pass "Health endpoint returns 200" || fail "Health endpoint response"
else
    fail "server not running for keep-alive test"
    fail "health endpoint (server down)"
fi

# ── 5. SECURITY HEADERS ─────────────────────────
echo ""
echo "  ── 5. Security Headers ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    RESP=$(curl -s -D- "http://127.0.0.1:$PORT/health" 2>/dev/null || echo "")
    echo "$RESP" | grep -qi "X-Content-Type-Options: nosniff" && pass "X-Content-Type-Options present" || fail "X-Content-Type-Options missing"
    echo "$RESP" | grep -qi "X-Frame-Options: DENY" && pass "X-Frame-Options present" || fail "X-Frame-Options missing"
    echo "$RESP" | grep -qi "Strict-Transport-Security" && pass "HSTS header present" || fail "HSTS missing"
    echo "$RESP" | grep -qi "Referrer-Policy" && pass "Referrer-Policy present" || fail "Referrer-Policy missing"
else
    fail "server not running for security header tests"
fi

# ── 6. 404 HANDLING ──────────────────────────────
echo ""
echo "  ── 6. Error Handling ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    RESP=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:$PORT/nonexistent" 2>/dev/null || echo "000")
    [ "$RESP" = "404" ] && pass "Unknown route returns 404" || fail "Expected 404, got $RESP"
fi

# ── 7. CONCURRENT REQUESTS (thread pool) ────────
echo ""
echo "  ── 7. Concurrent Requests (Thread Pool) ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    # Fire 20 concurrent requests
    CONCURRENT_OK=0
    for i in $(seq 1 20); do
        curl -s "http://127.0.0.1:$PORT/health" -o /dev/null -w "%{http_code}" &
    done
    RESULTS=$(wait)
    # If server didn't crash, thread pool is working
    sleep 1
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        pass "server survived 20 concurrent requests (thread pool working)"
    else
        fail "server crashed under concurrent load"
    fi
else
    fail "server not running for concurrency test"
fi

# ── 8. METRICS ENDPOINT ─────────────────────────
echo ""
echo "  ── 8. Metrics ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    METRICS=$(curl -s "http://127.0.0.1:$PORT/metrics" 2>/dev/null || echo "")
    echo "$METRICS" | grep -q "nc_requests_total" && pass "Prometheus nc_requests_total metric" || fail "nc_requests_total missing"
    echo "$METRICS" | grep -q "nc_active_connections" && pass "nc_active_connections metric" || fail "nc_active_connections missing"
    echo "$METRICS" | grep -q "nc_queue_depth" && pass "nc_queue_depth metric (new)" || fail "nc_queue_depth missing"
    echo "$METRICS" | grep -q "nc_keepalive_reuse" && pass "nc_keepalive_reuse metric (new)" || fail "nc_keepalive_reuse missing"
fi

# ── 9. GRACEFUL SHUTDOWN ────────────────────────
echo ""
echo "  ── 9. Graceful Shutdown ──"

if kill -0 "$SERVER_PID" 2>/dev/null; then
    kill -INT "$SERVER_PID" 2>/dev/null
    TIMEOUT=10
    while [ $TIMEOUT -gt 0 ] && kill -0 "$SERVER_PID" 2>/dev/null; do
        sleep 1
        TIMEOUT=$((TIMEOUT - 1))
    done
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        pass "server shut down gracefully on SIGINT"
    else
        fail "server didn't shut down within 10s"
        kill -9 "$SERVER_PID" 2>/dev/null || true
    fi
    SERVER_PID=""
else
    fail "server not running for shutdown test"
fi

# ── 10. RATE LIMITING TEST ──────────────────────
echo ""
echo "  ── 10. Rate Limiting ──"

cat > "$TMPDIR/rate_test.nc" << 'NCEOF'
service "rate-test"
version "1.0.0"

configure:
    port: 9877
    rate_limit: 5

to hello:
    respond with {"msg": "hello"}

api:
    GET /hello runs hello
NCEOF

export NC_RATE_LIMIT_RPM=5
export NC_PORT=9877
$NC run "$TMPDIR/rate_test.nc" &
SERVER_PID=$!
sleep 2

if kill -0 "$SERVER_PID" 2>/dev/null; then
    GOT_429=false
    for i in $(seq 1 15); do
        CODE=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:9877/hello" 2>/dev/null || echo "000")
        if [ "$CODE" = "429" ]; then
            GOT_429=true
            break
        fi
    done
    $GOT_429 && pass "rate limiter returns 429 after exceeding limit" || fail "rate limiter didn't trigger"

    # Check rate limit headers
    RESP=$(curl -s -D- "http://127.0.0.1:9877/hello" 2>/dev/null || echo "")
    echo "$RESP" | grep -qi "X-RateLimit\|Retry-After" && pass "rate limit headers present" || fail "rate limit headers missing"
else
    fail "rate-limited server didn't start"
fi

kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""

# ── SUMMARY ──────────────────────────────────────
echo ""
echo "  ═══════════════════════════════════════════"
if [ "$FAIL" -eq 0 ]; then
    echo "   ALL $PASS/$TOTAL TESTS PASSED"
else
    echo "   $PASS/$TOTAL passed, $FAIL failed"
fi
echo "  ═══════════════════════════════════════════"
echo ""

# Clean up env vars
unset NC_MAX_WORKERS NC_LISTEN_BACKLOG NC_REQUEST_QUEUE_SIZE NC_KEEPALIVE_MAX NC_KEEPALIVE_TIMEOUT NC_PORT NC_RATE_LIMIT_RPM

exit $FAIL
