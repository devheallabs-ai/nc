#!/bin/bash
# NC Performance & Stress Test Suite
# Run from project root: bash tests/perf/test_perf.sh

NC="./engine/build/nc"
PERF_DIR="tests/perf"
PASS=0
FAIL=0

echo ""
echo "  ╔══════════════════════════════════════════╗"
echo "  ║  NC Performance & Stress Test Suite       ║"
echo "  ╚══════════════════════════════════════════╝"
echo ""

# Check binary exists
if [ ! -f "$NC" ]; then
    echo "  ERROR: $NC not found. Run 'make' first."
    exit 1
fi

bench() {
    local name="$1"
    local behavior="$2"
    local file="$3"
    local start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
    local result=$($NC run "$file" -b "$behavior" --no-cache 2>/dev/null | grep "Result:" | sed 's/.*Result: //')
    local end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
    local ms=$(( (end - start) / 1000000 ))
    if [ -n "$result" ] && [ "$result" != "nothing" ]; then
        printf "  %-30s %6dms  result=%s\n" "$name" "$ms" "$result"
        PASS=$((PASS + 1))
    else
        printf "  %-30s %6dms  FAIL\n" "$name" "$ms"
        FAIL=$((FAIL + 1))
    fi
}

# ── 1. Compilation Speed ──────────────────────
echo "  ── Compilation Speed ──"
start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
for i in $(seq 1 10); do
    $NC bytecode "$PERF_DIR/stress_compute.nc" > /dev/null 2>&1
done
end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
ms=$(( (end - start) / 1000000 ))
avg=$(( ms / 10 ))
printf "  %-30s %6dms  (10 runs, avg %dms)\n" "Compile stress_compute.nc" "$ms" "$avg"
echo ""

# ── 2. VM Execution Benchmarks ────────────────
echo "  ── VM Execution Benchmarks ──"
bench "Math (10K iterations)" "bench_math" "$PERF_DIR/stress_compute.nc"
bench "String concat (500x)" "bench_strings" "$PERF_DIR/stress_compute.nc"
bench "List append (1000x)" "bench_list" "$PERF_DIR/stress_compute.nc"
bench "Map operations (100x)" "bench_map" "$PERF_DIR/stress_compute.nc"
bench "Nested loops (100x100)" "bench_nested_loops" "$PERF_DIR/stress_compute.nc"
bench "Conditionals (5000x)" "bench_conditionals" "$PERF_DIR/stress_compute.nc"
bench "Function calls (1000x)" "bench_function_calls" "$PERF_DIR/stress_compute.nc"
echo ""

# ── 3. Test Suite Speed ───────────────────────
echo "  ── Test Suite Speed ──"
start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
$NC test > /dev/null 2>&1
end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
ms=$(( (end - start) / 1000000 ))
printf "  %-30s %6dms\n" "Full test suite (20 files)" "$ms"

start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
$NC conformance > /dev/null 2>&1
end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
ms=$(( (end - start) / 1000000 ))
printf "  %-30s %6dms\n" "Conformance (71 tests)" "$ms"
echo ""

# ── 4. Memory Check ──────────────────────────
echo "  ── Memory Baseline ──"
binary_size=$(ls -l "$NC" | awk '{print $5}')
printf "  %-30s %s bytes (%s)\n" "Binary size" "$binary_size" "$(ls -lh $NC | awk '{print $5}')"
echo ""

# ── 5. Stress: Rapid Compile-Execute Cycles ──
echo "  ── Stress: 50 Rapid Executions ──"
start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
for i in $(seq 1 50); do
    $NC run "$PERF_DIR/stress_compute.nc" -b bench_math --no-cache > /dev/null 2>&1
done
end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")
ms=$(( (end - start) / 1000000 ))
avg=$(( ms / 50 ))
printf "  %-30s %6dms total, %dms avg\n" "50x bench_math" "$ms" "$avg"
echo ""

# ── Results ───────────────────────────────────
echo "  ╔══════════════════════════════════════════╗"
if [ $FAIL -eq 0 ]; then
    echo "  ║  ALL $PASS BENCHMARKS PASSED                  ║"
else
    echo "  ║  $PASS PASSED, $FAIL FAILED                       ║"
fi
echo "  ╚══════════════════════════════════════════╝"
echo ""

exit $FAIL
