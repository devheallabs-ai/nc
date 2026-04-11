#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/prepare_release.sh [version] [--allow-incomplete-training] [--nc-bin PATH]

Examples:
  ./scripts/prepare_release.sh v1.0.0 --allow-incomplete-training
  ./scripts/prepare_release.sh v1.0.0 --nc-bin engine/build/nc
EOF
}

VERSION="v1.0.0"
ALLOW_INCOMPLETE=0
NC_BIN=""
POSITIONAL_VERSION_SET=0
SKIP_SEMANTIC=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --allow-incomplete-training)
            ALLOW_INCOMPLETE=1
            shift
            ;;
        --skip-semantic-smoke)
            SKIP_SEMANTIC=1
            shift
            ;;
        --nc-bin)
            [[ $# -ge 2 ]] || { echo "Missing value for --nc-bin" >&2; exit 2; }
            NC_BIN="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            if [[ $POSITIONAL_VERSION_SET -eq 0 ]]; then
                VERSION="$1"
                POSITIONAL_VERSION_SET=1
                shift
            else
                echo "Unknown argument: $1" >&2
                usage
                exit 2
            fi
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENGINE_BUILD="$PROJECT_ROOT/engine/build"
REPORT_PATH="$ENGINE_BUILD/nc_model_v1_release_report.txt"
STATE_PATH="$ENGINE_BUILD/nc_model_v1_release_training_state.json"
MODEL_PATH="$ENGINE_BUILD/nc_model_v1_release.bin"
TOKENIZER_PATH="$ENGINE_BUILD/nc_model_v1_release_tokenizer.bin"
OPTIMIZER_PATH="$ENGINE_BUILD/nc_model_v1_release_optimizer.bin"
BEST_MODEL_PATH="$ENGINE_BUILD/nc_model_v1_release_best.bin"
BEST_TOKENIZER_PATH="$ENGINE_BUILD/nc_model_v1_release_best_tokenizer.bin"
SMOKE_DIR="$ENGINE_BUILD/release_ai_smoke_inventory_dashboard"
MEMORY_STORE_PATH="$ENGINE_BUILD/release_memory_policy_smoke_store.json"
POLICY_STORE_PATH="$ENGINE_BUILD/release_policy_smoke_store.json"
TRAINING_DIR="$PROJECT_ROOT/training_data"
TRAINING_MODEL_PATH="$TRAINING_DIR/nova_model.bin"
TRAINING_TOKENIZER_PATH="$TRAINING_DIR/nc_ai_tokenizer.bin"

if [[ -z "$NC_BIN" ]]; then
    for candidate in \
        "$ENGINE_BUILD/nc" \
        "$ENGINE_BUILD/nc_ready.exe" \
        "$ENGINE_BUILD/nc_new.exe" \
        "$ENGINE_BUILD/nc.exe"; do
        if [[ -f "$candidate" ]]; then
            NC_BIN="$candidate"
            break
        fi
    done
fi

CHECKS_PASSED=0
CHECKS_FAILED=0
BLOCKERS=()

pass() {
    CHECKS_PASSED=$((CHECKS_PASSED + 1))
    printf "  [PASS] %s\n" "$1"
}

fail() {
    CHECKS_FAILED=$((CHECKS_FAILED + 1))
    BLOCKERS+=("$2")
    printf "  [FAIL] %s\n" "$1"
}

sha256_file() {
    local path="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$path" | awk '{print $1}'
    else
        shasum -a 256 "$path" | awk '{print $1}'
    fi
}

extract_report_value() {
    local key="$1"
    local value
    value="$(grep -E "^${key}=" "$REPORT_PATH" | tail -1 | cut -d= -f2-)"
    printf "%s" "$value"
}

extract_state_value() {
    local key="$1"
    local value
    value="$(grep -E "\"${key}\"[[:space:]]*:" "$STATE_PATH" | head -1 | sed -E 's/.*:[[:space:]]*//; s/[",]//g')"
    printf "%s" "$value"
}

echo ""
echo "  NC v1 Release Preparation — ${VERSION}"
echo "  Binary: ${NC_BIN:-not found}"
echo "  Project: $PROJECT_ROOT"
echo ""

for required in \
    "$REPORT_PATH" \
    "$STATE_PATH" \
    "$MODEL_PATH" \
    "$TOKENIZER_PATH" \
    "$OPTIMIZER_PATH" \
    "$BEST_MODEL_PATH" \
    "$BEST_TOKENIZER_PATH"; do
    if [[ -f "$required" ]]; then
        pass "artifact present: $(basename "$required")"
    else
        fail "artifact missing: $(basename "$required")" "Missing required release artifact: $required"
    fi
done

if [[ -n "$NC_BIN" && -f "$NC_BIN" ]]; then
    pass "release binary located"
else
    fail "release binary located" "NC binary not found. Build the release binary first."
fi

if [[ -f "$MODEL_PATH" && ! -f "$TRAINING_MODEL_PATH" ]]; then
    mkdir -p "$TRAINING_DIR"
    cp "$MODEL_PATH" "$TRAINING_MODEL_PATH"
fi
if [[ -f "$TOKENIZER_PATH" && ! -f "$TRAINING_TOKENIZER_PATH" ]]; then
    mkdir -p "$TRAINING_DIR"
    cp "$TOKENIZER_PATH" "$TRAINING_TOKENIZER_PATH"
fi

if [[ -n "$NC_BIN" && -f "$NC_BIN" ]]; then
    if (cd "$PROJECT_ROOT" && ./tests/run_tests.sh) >/tmp/nc_release_tests.log 2>&1; then
        pass "local test suite"
    else
        fail "local test suite" "Local test suite failed. See /tmp/nc_release_tests.log"
    fi

    rm -rf "$SMOKE_DIR"
    if (cd "$PROJECT_ROOT" && "$NC_BIN" ai create "inventory dashboard" -o "$SMOKE_DIR") >/tmp/nc_release_ai_smoke.log 2>&1; then
        missing=0
        for rel in service.nc app.ncui test_app.nc README.md; do
            if [[ ! -f "$SMOKE_DIR/$rel" ]]; then
                missing=1
                break
            fi
        done
        if [[ $missing -eq 0 && ( -f "$SMOKE_DIR/dist/app.html" || -f "$SMOKE_DIR/app.html" ) ]]; then
            pass "AI create smoke"
        else
            fail "AI create smoke" "AI smoke output is incomplete under $SMOKE_DIR"
        fi
    else
        fail "AI create smoke" "AI smoke command failed. See /tmp/nc_release_ai_smoke.log"
    fi

    if [[ $SKIP_SEMANTIC -eq 0 ]]; then
        SEMANTIC_DIR="$ENGINE_BUILD/release_semantic_smoke_enterprise"
        if (cd "$PROJECT_ROOT" && ./tests/run_ai_semantic_smoke.sh \
            --output-dir "$SEMANTIC_DIR" \
            --nc-bin "$NC_BIN" \
            --artifacts-dir "$ENGINE_BUILD" \
            --model-path "$MODEL_PATH" \
            --tokenizer-path "$TOKENIZER_PATH" \
            --force) >/tmp/nc_release_semantic_smoke.log 2>&1; then
            pass "enterprise semantic smoke"
        else
            fail "enterprise semantic smoke" "Enterprise semantic smoke failed. See /tmp/nc_release_semantic_smoke.log"
        fi
    fi

    rm -f "$MEMORY_STORE_PATH" "$POLICY_STORE_PATH"
    if (cd "$PROJECT_ROOT" && NC_ALLOW_FILE_WRITE=1 NC_ALLOW_FILE_READ=1 "$NC_BIN" run tests/release_memory_policy_smoke.nc -b smoke) >/tmp/nc_release_memory_smoke.log 2>&1; then
        if [[ -f "$MEMORY_STORE_PATH" && -f "$POLICY_STORE_PATH" ]] && \
           grep -q 'nc_long_term_memory' "$MEMORY_STORE_PATH" && \
           grep -q 'nc_policy_memory' "$POLICY_STORE_PATH"; then
            pass "core memory/policy smoke"
        else
            fail "core memory/policy smoke" "Memory smoke files were not created or are malformed"
        fi
    else
        fail "core memory/policy smoke" "Memory smoke command failed. See /tmp/nc_release_memory_smoke.log"
    fi
fi

RELEASE_GATE_PASSED="0"
BEST_EVAL_PPL=""
CURRENT_STEP=""
TARGET_STEP=""
COMPLETED="0"

if [[ -f "$REPORT_PATH" ]]; then
    RELEASE_GATE_PASSED="$(extract_report_value release_gate_passed)"
    BEST_EVAL_PPL="$(extract_report_value best_eval_ppl)"
fi
if [[ -f "$STATE_PATH" ]]; then
    CURRENT_STEP="$(extract_state_value current_step)"
    TARGET_STEP="$(extract_state_value target_step)"
    COMPLETED="$(extract_state_value completed)"
fi

if [[ "$RELEASE_GATE_PASSED" == "1" ]]; then
    pass "release gate passed"
else
    fail "release gate passed" "Release gate is not passing in $REPORT_PATH"
fi

if [[ "$COMPLETED" == "1" ]]; then
    pass "training run completed"
else
    if [[ $ALLOW_INCOMPLETE -eq 1 ]]; then
        pass "training run still in progress (candidate mode allowed)"
    else
        fail "training run completed" "Training state is incomplete: current_step=${CURRENT_STEP:-unknown}, target_step=${TARGET_STEP:-unknown}"
    fi
fi

echo ""
echo "  Checksums"
for checksum_target in \
    "$NC_BIN" \
    "$MODEL_PATH" \
    "$TOKENIZER_PATH" \
    "$OPTIMIZER_PATH" \
    "$BEST_MODEL_PATH" \
    "$BEST_TOKENIZER_PATH"; do
    if [[ -n "$checksum_target" && -f "$checksum_target" ]]; then
        printf "  %s  %s\n" "$(sha256_file "$checksum_target")" "$(basename "$checksum_target")"
    fi
done

READINESS="blocked"
if [[ ${#BLOCKERS[@]} -eq 0 ]]; then
    if [[ "$COMPLETED" == "1" ]]; then
        READINESS="final"
    else
        READINESS="candidate"
    fi
fi

echo ""
echo "  Summary"
printf "  readiness: %s\n" "$READINESS"
printf "  best_eval_ppl: %s\n" "${BEST_EVAL_PPL:-unknown}"
printf "  current_step: %s\n" "${CURRENT_STEP:-unknown}"
printf "  target_step: %s\n" "${TARGET_STEP:-unknown}"

if [[ ${#BLOCKERS[@]} -gt 0 ]]; then
    echo ""
    echo "  Blockers"
    for blocker in "${BLOCKERS[@]}"; do
        printf "  - %s\n" "$blocker"
    done
fi

echo ""
echo "  Next steps"
echo "  1. Update CHANGELOG.md and release notes."
echo "  2. Tag and push once readiness is acceptable."
echo "  3. Upload platform binaries plus the versioned v1 model assets."
echo ""

if [[ "$READINESS" == "blocked" ]]; then
    exit 1
fi

if [[ "$READINESS" == "candidate" && $ALLOW_INCOMPLETE -eq 0 ]]; then
    exit 1
fi

exit 0
