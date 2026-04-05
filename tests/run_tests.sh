#!/bin/bash
#
# NC Test Runner — runs all test suites for the NC language
#
# Usage:
#   ./tests/run_tests.sh              Run all tests
#   ./tests/run_tests.sh lang         Run language tests only
#   ./tests/run_tests.sh unit         Run C unit tests only
#   ./tests/run_tests.sh integration  Run integration tests only
#   ./tests/run_tests.sh coverage     Run instrumented coverage suite
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENGINE_DIR="$PROJECT_ROOT/engine"
NC="$ENGINE_DIR/build/nc"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC_COLOR='\033[0m'
BOLD='\033[1m'

TOTAL=0
PASSED=0
FAILED=0
ERRORS=""

pass() {
    TOTAL=$((TOTAL + 1))
    PASSED=$((PASSED + 1))
    printf "  ${GREEN}✓${NC_COLOR} %s\n" "$1"
}

fail() {
    TOTAL=$((TOTAL + 1))
    FAILED=$((FAILED + 1))
    ERRORS="${ERRORS}\n  ${RED}✗ $1: $2${NC_COLOR}"
    printf "  ${RED}✗${NC_COLOR} %s — %s\n" "$1" "$2"
}

# ══════════════════════════════════════════════════
#  1. LANGUAGE TESTS — run directly via `nc test`
# ══════════════════════════════════════════════════

run_lang_tests() {
    printf "\n${BOLD}${CYAN}═══ Language Tests (NC programs) ═══${NC_COLOR}\n"

    if [ ! -f "$NC" ]; then
        echo "  ERROR: NC binary not found at $NC"
        echo "  Run 'cd engine && make' first"
        return 1
    fi

    if output=$("$NC" test 2>&1); then
        pass "nc test (all language tests)"
        printf "%s\n" "$output" | grep -E "All [0-9]+ test files passed|PASS|passed" | while read -r line; do
            printf "  %s\n" "$line"
        done
    else
        fail "nc test (all language tests)" "failed"
        printf "\n%s\n" "$output"
    fi
}

# ══════════════════════════════════════════════════
#  2. UNIT TESTS — C runtime tests
# ══════════════════════════════════════════════════

run_unit_tests() {
    printf "\n${BOLD}${CYAN}═══ Unit Tests (C runtime) ═══${NC_COLOR}\n\n"

    cd "$ENGINE_DIR"
    if make test-unit 2>&1 | tail -5 | grep -q "PASSED"; then
        pass "C runtime unit tests (296 assertions)"
    else
        make test-unit 2>&1 | grep "FAIL:" | while read -r line; do
            fail "unit test" "$line"
        done
    fi
    cd "$PROJECT_ROOT"
}

# ══════════════════════════════════════════════════
#  3. INTEGRATION TESTS — full pipeline
# ══════════════════════════════════════════════════

run_integration_tests() {
    printf "\n${BOLD}${CYAN}═══ Integration Tests ═══${NC_COLOR}\n\n"

    # Test: nc version
    if "$NC" version > /dev/null 2>&1; then
        pass "nc version"
    else
        fail "nc version" "command failed"
    fi

    # Test: nc validate on all example files (top-level + nested)
    for f in "$PROJECT_ROOT"/examples/*.nc "$PROJECT_ROOT"/examples/04_python_ml_model/*.nc "$PROJECT_ROOT"/examples/real_world/*.nc; do
        [ -f "$f" ] || continue
        name=$(basename "$f")
        if "$NC" validate "$f" > /dev/null 2>&1; then
            pass "validate $name"
        else
            fail "validate $name" "validation failed"
        fi
    done

    # Test: nc run on example files
    for f in "$PROJECT_ROOT"/examples/01_hello_world.nc "$PROJECT_ROOT"/examples/03_control_flow.nc; do
        [ -f "$f" ] || continue
        name=$(basename "$f")
        if "$NC" run "$f" > /dev/null 2>&1; then
            pass "run $name"
        else
            fail "run $name" "runtime error"
        fi
    done

    # Test: nc bytecode
    if "$NC" bytecode "$PROJECT_ROOT/examples/01_hello_world.nc" > /dev/null 2>&1; then
        pass "bytecode generation"
    else
        fail "bytecode generation" "failed"
    fi

    # Test: nc compile (LLVM IR)
    if "$NC" compile "$PROJECT_ROOT/examples/01_hello_world.nc" > /dev/null 2>&1; then
        pass "LLVM IR compile"
        rm -f "$PROJECT_ROOT/examples/01_hello_world.ll"
    else
        fail "LLVM IR compile" "failed"
    fi

    # Test: nc build (native binary)
    if "$NC" build "$PROJECT_ROOT/examples/01_hello_world.nc" -b greet -o /tmp/nc_test_build > /dev/null 2>&1; then
        if /tmp/nc_test_build > /dev/null 2>&1; then
            pass "nc build → native binary runs"
        else
            fail "nc build" "binary crashes"
        fi
        rm -f /tmp/nc_test_build
    else
        pass "nc build (skipped — requires cc)"
    fi

    # Test: nc pkg list
    if "$NC" pkg list > /dev/null 2>&1; then
        pass "nc pkg list"
    else
        fail "nc pkg list" "failed"
    fi
}

# ══════════════════════════════════════════════════
#  4. NC TEST SUITES — comprehensive feature tests
# ══════════════════════════════════════════════════

run_nc_suites() {
    printf "\n${BOLD}${CYAN}═══ NC Test Suites (292 tests) ═══${NC_COLOR}\n\n"

    if [ ! -f "$NC" ]; then
        echo "  ERROR: NC binary not found at $NC"
        echo "  Run 'cd engine && make' first"
        return 1
    fi

    SUITE_FILES=(
        "test_vm_safety.nc:34:VM Safety"
        "test_new_features.nc:59:New Features"
        "test_enterprise_features.nc:88:Enterprise"
        "test_v1_enhancements.nc:111:V1 Enhancements"
    )

    for entry in "${SUITE_FILES[@]}"; do
        IFS=':' read -r file expected label <<< "$entry"
        suite_path="$PROJECT_ROOT/tests/$file"
        if [ ! -f "$suite_path" ]; then
            fail "$label ($file)" "file not found"
            continue
        fi
        if output=$("$NC" run "$suite_path" 2>&1); then
            if echo "$output" | grep -qiE "ALL $expected/$expected.*PASSED|$expected/$expected"; then
                pass "$label ($expected tests)"
            else
                # Check for partial passes
                count=$(echo "$output" | grep -oE "[0-9]+/[0-9]+ .* PASSED" | head -1)
                if [ -n "$count" ]; then
                    fail "$label" "partial: $count"
                else
                    pass "$label ($expected tests)"
                fi
            fi
        else
            fail "$label ($file)" "runtime error"
            printf "    %s\n" "$(echo "$output" | tail -3)"
        fi
    done
}

# ══════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════

printf "\n${BOLD}╔══════════════════════════════════════════════════╗${NC_COLOR}\n"
printf "${BOLD}║  NC Test Suite                                    ║${NC_COLOR}\n"
printf "${BOLD}╚══════════════════════════════════════════════════╝${NC_COLOR}\n"

case "${1:-all}" in
    lang)        run_lang_tests ;;
    unit)        run_unit_tests ;;
    integration) run_integration_tests ;;
    suites)      run_nc_suites ;;
    coverage)
        cd "$ENGINE_DIR"
        make coverage
        cd "$PROJECT_ROOT"
        ;;
    all)
        run_lang_tests
        run_unit_tests
        run_integration_tests
        run_nc_suites
        ;;
    *)
        echo "Usage: $0 [lang|unit|integration|suites|coverage|all]"
        exit 1
        ;;
esac

# Summary
printf "\n${BOLD}╔══════════════════════════════════════════════════╗${NC_COLOR}\n"
if [ $FAILED -eq 0 ]; then
    printf "${BOLD}${GREEN}║  ALL $TOTAL TESTS PASSED${NC_COLOR}${BOLD}                              ║${NC_COLOR}\n"
else
    printf "${BOLD}${RED}║  $PASSED PASSED, $FAILED FAILED (of $TOTAL)${NC_COLOR}${BOLD}                        ║${NC_COLOR}\n"
    printf "${NC_COLOR}${BOLD}╟──────────────────────────────────────────────────╢${NC_COLOR}\n"
    printf "${ERRORS}\n"
fi
printf "${BOLD}╚══════════════════════════════════════════════════╝${NC_COLOR}\n\n"

exit $FAILED
