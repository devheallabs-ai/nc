#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
    ./tests/run_ai_smoke.sh [--prompt TEXT] [--output-dir DIR] [--nc-bin PATH] [--model-path PATH] [--tokenizer-path PATH] [--artifacts-dir DIR] [--template-fallback] [--copy-model] [--force] [--verbose]
EOF
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SETUP_SCRIPT="$REPO_ROOT/scripts/setup-local-ai-model.sh"

PROMPT="inventory dashboard"
OUTPUT_DIR=""
NC_BIN=""
MODEL_PATH=""
TOKENIZER_PATH=""
ARTIFACTS_DIR=""
COPY_MODEL=0
FORCE=0
VERBOSE=0
TEMPLATE_FALLBACK=0

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

to_native_path() {
    local path="$1"
    if [[ -z "$path" ]]; then
        return 1
    fi
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$path"
        return 0
    fi
    printf '%s\n' "$path"
}

slugify() {
    printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+/_/g; s/^_+//; s/_+$//'
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
        --template-fallback)
            TEMPLATE_FALLBACK=1
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

if [[ -z "$NC_BIN" ]]; then
    for candidate in \
        "$REPO_ROOT/engine/build/nc" \
        "$REPO_ROOT/engine/build/nc_release_ready.exe" \
        "$REPO_ROOT/engine/build/nc_ready.exe" \
        "$REPO_ROOT/engine/build/nc_new.exe" \
        "$REPO_ROOT/engine/build/nc.exe"; do
        if [[ -f "$candidate" ]]; then
            NC_BIN="$candidate"
            break
        fi
    done
fi

if [[ -z "$NC_BIN" || ! -f "$NC_BIN" ]]; then
    echo "nc binary not found. Build the engine first or pass --nc-bin." >&2
    exit 1
fi

if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$REPO_ROOT/engine/build/ai_create_smoke_$(slugify "$PROMPT")"
fi

TRAINING_MODEL="$REPO_ROOT/training_data/nova_model.bin"
TRAINING_TOKENIZER="$REPO_ROOT/training_data/nc_ai_tokenizer.bin"

if [[ $FORCE -eq 1 || -n "$MODEL_PATH" || -n "$TOKENIZER_PATH" || -n "$ARTIFACTS_DIR" || ! -f "$TRAINING_MODEL" || ! -f "$TRAINING_TOKENIZER" ]]; then
    setup_args=()
    [[ -n "$MODEL_PATH" ]] && setup_args+=(--model-path "$MODEL_PATH")
    [[ -n "$TOKENIZER_PATH" ]] && setup_args+=(--tokenizer-path "$TOKENIZER_PATH")
    [[ -n "$ARTIFACTS_DIR" ]] && setup_args+=(--artifacts-dir "$ARTIFACTS_DIR")
    [[ $COPY_MODEL -eq 1 ]] && setup_args+=(--copy)
    [[ $FORCE -eq 1 ]] && setup_args+=(--force)
    "$SETUP_SCRIPT" "${setup_args[@]}"
fi

if [[ -d "$OUTPUT_DIR" ]]; then
    if [[ $FORCE -ne 1 ]]; then
        echo "Output directory already exists: $OUTPUT_DIR. Re-run with --force or pass --output-dir." >&2
        exit 1
    fi
    rm -rf "$OUTPUT_DIR"
fi

EXPECTED_FILES=(service.nc app.ncui test_app.nc README.md)
LOG_FILE="${TMPDIR:-/tmp}/nc_ai_smoke.log"

echo ""
echo "  NC AI Smoke Test (Unix)"
echo "  Binary: $NC_BIN"
echo "  Prompt: $PROMPT"
echo "  Output: $OUTPUT_DIR"
echo ""

set +e
create_output_dir="$OUTPUT_DIR"
if [[ "$NC_BIN" == *.exe ]]; then
    create_output_dir="$(to_native_path "$OUTPUT_DIR")"
fi
create_args=(ai create "$PROMPT" -o "$create_output_dir")
[[ $TEMPLATE_FALLBACK -eq 1 ]] && create_args+=(--template-fallback)
COMMAND_OUTPUT="$(cd "$REPO_ROOT" && "$NC_BIN" "${create_args[@]}" 2>&1)"
EXIT_CODE=$?
set -e

printf '%s\n' "$COMMAND_OUTPUT" > "$LOG_FILE"

if [[ $VERBOSE -eq 1 && -n "$COMMAND_OUTPUT" ]]; then
    printf '%s\n' "$COMMAND_OUTPUT"
fi

if [[ $EXIT_CODE -ne 0 ]]; then
    if [[ -n "$COMMAND_OUTPUT" ]]; then
        printf '%s\n' "$COMMAND_OUTPUT"
    fi
    echo "nc ai create failed with exit code $EXIT_CODE" >&2
    exit $EXIT_CODE
fi

MISSING=()
for rel in "${EXPECTED_FILES[@]}"; do
    if [[ ! -f "$OUTPUT_DIR/$rel" ]]; then
        MISSING+=("$rel")
    fi
done

if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo "Smoke output is incomplete. Missing: ${MISSING[*]}" >&2
    exit 1
fi

if [[ ! -f "$OUTPUT_DIR/dist/app.html" && ! -f "$OUTPUT_DIR/app.html" ]]; then
    echo "Smoke output is missing the frontend bundle (dist/app.html or app.html)." >&2
    exit 1
fi

echo "  Smoke passed."
while IFS= read -r line; do
    case "$line" in
        *"Validation:"*|*"Successfully compiled"*|*"Project created:"*|*"source files generated"*)
            echo "  $line"
            ;;
    esac
done < "$LOG_FILE"

echo ""
echo "  Verified files:"
for rel in "${EXPECTED_FILES[@]}"; do
    echo "    $rel"
done
echo "    dist/app.html or app.html"
echo ""