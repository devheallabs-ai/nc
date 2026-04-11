#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./tests/run_ai_semantic_smoke.sh [--prompt TEXT] [--output-dir DIR] [--nc-bin PATH] [--model-path PATH] [--tokenizer-path PATH] [--artifacts-dir DIR] [--copy-model] [--force] [--verbose]
EOF
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SMOKE_SCRIPT="$REPO_ROOT/tests/run_ai_smoke.sh"

PROMPT="multi-tenant operations dashboard with role based access analytics alert center approvals and dark theme"
OUTPUT_DIR="$REPO_ROOT/engine/build/ai_create_semantic_ops_dashboard_unix"
NC_BIN=""
MODEL_PATH=""
TOKENIZER_PATH=""
ARTIFACTS_DIR=""
COPY_MODEL=0
FORCE=0
VERBOSE=0

resolve_path() {
    local path="$1"
    if [[ -z "$path" ]]; then
        return 1
    fi
    case "$path" in
        /*) printf '%s\n' "$path" ;;
        [A-Za-z]:[\\/]*) printf '%s\n' "$path" ;;
        *) printf '%s\n' "$REPO_ROOT/$path" ;;
    esac
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prompt)
            PROMPT="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$(resolve_path "$2")"
            shift 2
            ;;
        --nc-bin)
            NC_BIN="$(resolve_path "$2")"
            shift 2
            ;;
        --model-path)
            MODEL_PATH="$(resolve_path "$2")"
            shift 2
            ;;
        --tokenizer-path)
            TOKENIZER_PATH="$(resolve_path "$2")"
            shift 2
            ;;
        --artifacts-dir)
            ARTIFACTS_DIR="$(resolve_path "$2")"
            shift 2
            ;;
        --copy-model)
            COPY_MODEL=1
            shift
            ;;
        --force)
            FORCE=1
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

smoke_args=(--prompt "$PROMPT" --output-dir "$OUTPUT_DIR" --template-fallback)
[[ -n "$NC_BIN" ]] && smoke_args+=(--nc-bin "$NC_BIN")
[[ -n "$MODEL_PATH" ]] && smoke_args+=(--model-path "$MODEL_PATH")
[[ -n "$TOKENIZER_PATH" ]] && smoke_args+=(--tokenizer-path "$TOKENIZER_PATH")
[[ -n "$ARTIFACTS_DIR" ]] && smoke_args+=(--artifacts-dir "$ARTIFACTS_DIR")
[[ $COPY_MODEL -eq 1 ]] && smoke_args+=(--copy-model)
[[ $FORCE -eq 1 ]] && smoke_args+=(--force)
[[ $VERBOSE -eq 1 ]] && smoke_args+=(--verbose)

"$SMOKE_SCRIPT" "${smoke_args[@]}"

SERVICE_FILE="$OUTPUT_DIR/service.nc"
PAGE_FILE="$OUTPUT_DIR/app.ncui"

assert_patterns() {
    local file="$1"
    local name="$2"
    shift 2
    local pattern
    for pattern in "$@"; do
        if ! grep -Eiq "$pattern" "$file"; then
            echo "Semantic smoke failed: $name is missing pattern '$pattern'" >&2
            exit 1
        fi
    done
    echo "  [PASS] $name"
}

echo "  Semantic checks"
assert_patterns "$SERVICE_FILE" "tenant scope in service" 'list_tenants' '/api/v1/tenants'
assert_patterns "$SERVICE_FILE" "role access in service" 'list_roles' '/api/v1/roles' 'permission'
assert_patterns "$SERVICE_FILE" "analytics in service" 'analytics_overview' '/api/v1/analytics/overview'
assert_patterns "$SERVICE_FILE" "approval workflow in service" 'list_approvals' 'approve_request' 'reject_request'
assert_patterns "$SERVICE_FILE" "alert center in service" 'list_alerts' '/api/v1/alerts'

assert_patterns "$PAGE_FILE" "dashboard section in page" 'Dashboard'
assert_patterns "$PAGE_FILE" "tenant section in page" 'Tenants'
assert_patterns "$PAGE_FILE" "roles section in page" 'Roles'
assert_patterns "$PAGE_FILE" "analytics section in page" 'Analytics'
assert_patterns "$PAGE_FILE" "approvals section in page" 'Approvals'
assert_patterns "$PAGE_FILE" "alerts section in page" 'Alerts'

echo ""
echo "  Semantic smoke passed."
echo "  Output: $OUTPUT_DIR"
echo ""