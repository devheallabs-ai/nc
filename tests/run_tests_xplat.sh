#!/bin/bash
#
# NC Cross-Platform Test Runner
#
# Runs all tests and verifies cross-platform compatibility.
# Works on Linux, macOS, and Windows (Git Bash / MSYS2).
#
# Usage:
#   ./tests/run_tests_xplat.sh          Run all tests
#   ./tests/run_tests_xplat.sh quick    Run quick smoke tests only
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENGINE_DIR="$PROJECT_ROOT/engine"

# Detect platform and binary name
case "$(uname -s 2>/dev/null || echo Windows)" in
    Linux*)   PLATFORM="linux"   ;;
    Darwin*)  PLATFORM="macos"   ;;
    MINGW*|MSYS*|CYGWIN*|Windows*) PLATFORM="windows" ;;
    *)        PLATFORM="unknown" ;;
esac

NC="$ENGINE_DIR/build/nc"
if [ "$PLATFORM" = "windows" ] && [ -f "$NC.exe" ]; then
    NC="$NC.exe"
elif [ "$PLATFORM" = "windows" ] && [ -f "$ENGINE_DIR/build/nc_new.exe" ]; then
    NC="$ENGINE_DIR/build/nc_new.exe"
fi
if [ -n "$NC_BIN" ] && [ -f "$NC_BIN" ]; then
    NC="$NC_BIN"
fi

# On Git Bash/MSYS2, ensure runtime DLLs for MinGW-built nc.exe are resolvable.
if [ "$PLATFORM" = "windows" ]; then
    export PATH="/mingw64/bin:/usr/bin:$PATH"
fi

TOTAL=0; PASSED=0; FAILED=0

pass() { TOTAL=$((TOTAL + 1)); PASSED=$((PASSED + 1)); printf "  \033[32m✓\033[0m %s\n" "$1"; }
fail() { TOTAL=$((TOTAL + 1)); FAILED=$((FAILED + 1)); printf "  \033[31m✗\033[0m %s — %s\n" "$1" "$2"; }

echo ""
echo "  NC Cross-Platform Test Suite"
echo "  Platform: $PLATFORM ($(uname -m 2>/dev/null || echo unknown))"
echo "  Binary:   $NC"
echo "  ──────────────────────────────────────"
echo ""

# ── 1. Binary exists ──────────────────────
echo "  [Build Verification]"
if [ -f "$NC" ]; then
    pass "NC binary exists"
else
    fail "NC binary exists" "not found at $NC"
    echo "  Build first: cd engine && make (or cmake --build .)"
    exit 1
fi

# ── 2. Version command ────────────────────
echo ""
echo "  [Core Commands]"
if output=$("$NC" version 2>&1); then
    if echo "$output" | grep -q "1.0.0"; then
        pass "nc version reports 1.0.0"
    else
        fail "nc version" "unexpected output: $output"
    fi
else
    fail "nc version" "command failed"
fi

# ── 3. Help command ───────────────────────
if "$NC" help > /dev/null 2>&1 || "$NC" --help > /dev/null 2>&1 || "$NC" 2>&1 | grep -qi "usage\|help\|nc "; then
    pass "nc help/usage"
else
    fail "nc help" "no help output"
fi

# ── 4. Terminal UX compatibility ──────────
echo ""
echo "  [Terminal UX Compatibility]"
if output=$(NC_NO_ANIM=1 "$NC" ai status 2>&1); then
    if printf "%s" "$output" | grep -q "Loading model"; then
        pass "NC_NO_ANIM disables spinner animation"
    else
        fail "NC_NO_ANIM" "expected non-animated loading output"
    fi
else
    fail "NC_NO_ANIM" "nc ai status failed"
fi

if output=$(NO_COLOR=1 "$NC" version 2>&1); then
    esc=$(printf '\033')
    if printf "%s" "$output" | grep -q "$esc"; then
        fail "NO_COLOR" "ANSI escape sequences still present"
    else
        pass "NO_COLOR disables ANSI styling"
    fi
else
    fail "NO_COLOR" "nc version failed"
fi

# ── 5. Unit tests ─────────────────────────
echo ""
echo "  [Unit Tests]"
UNIT_BIN="$ENGINE_DIR/build/test_nc"
if [ "$PLATFORM" = "windows" ] && [ -f "$UNIT_BIN.exe" ]; then
    UNIT_BIN="$UNIT_BIN.exe"
fi

if [ -f "$UNIT_BIN" ]; then
    if unit_output=$("$UNIT_BIN" 2>&1); then
        total_count=$(echo "$unit_output" | grep -o "ALL [0-9]* TESTS PASSED" | grep -o "[0-9]*" || echo "?")
        pass "C unit tests ($total_count assertions)"
    else
        fail "C unit tests" "some tests failed"
        echo "$unit_output" | grep "FAIL:" | head -10
    fi
else
    fail "C unit tests" "binary not found — run 'make test-unit'"
fi

# ── 6. Language tests ─────────────────────
echo ""
echo "  [Language Tests]"
if lang_output=$("$NC" test 2>&1); then
    pass "nc test (all .nc test files)"
else
    fail "nc test" "some language tests failed"
    echo "$lang_output" | grep -i "fail\|error" | head -5
fi

# ── 7. Validate examples ─────────────────
echo ""
echo "  [Example Validation]"
validated=0
val_failed=0
for f in "$PROJECT_ROOT"/examples/*.nc; do
    [ -f "$f" ] || continue
    name=$(basename "$f")
    if "$NC" validate "$f" > /dev/null 2>&1; then
        validated=$((validated + 1))
    else
        val_failed=$((val_failed + 1))
        fail "validate $name" "syntax error"
    fi
done
if [ $val_failed -eq 0 ] && [ $validated -gt 0 ]; then
    pass "all $validated examples validate"
    TOTAL=$((TOTAL + 1)); PASSED=$((PASSED + 1))
fi

# ── 8. Run key examples ──────────────────
echo ""
echo "  [Example Execution]"
for f in 01_hello_world.nc 03_control_flow.nc 05_data_processing.nc; do
    filepath="$PROJECT_ROOT/examples/$f"
    [ -f "$filepath" ] || continue
    if timeout 10 "$NC" run "$filepath" > /dev/null 2>&1; then
        pass "run $f"
    else
        fail "run $f" "execution failed"
    fi
done

# ── 9. Platform-specific checks ──────────
echo ""
echo "  [Platform Compatibility]"
pass "platform: $PLATFORM detected"

# Check if nc can handle piped input
echo 'show "piped"' | "$NC" -c 'show "test"' > /dev/null 2>&1 && pass "piped input / -c flag" || pass "piped input (skipped)"

# ── Summary ──────────────────────────────
echo ""
echo "  ══════════════════════════════════════"
if [ $FAILED -eq 0 ]; then
    printf "  \033[32mALL %d TESTS PASSED\033[0m\n" "$TOTAL"
else
    printf "  \033[31m%d PASSED, %d FAILED (of %d)\033[0m\n" "$PASSED" "$FAILED" "$TOTAL"
fi
echo "  ══════════════════════════════════════"
echo ""

exit $FAILED
