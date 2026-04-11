#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/setup-local-ai-model.sh [--model-path PATH] [--tokenizer-path PATH] [--artifacts-dir DIR] [--copy] [--force]
EOF
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ENGINE_DIR="$REPO_ROOT/engine"
BUILD_DIR="$ENGINE_DIR/build"
TARGET_DIR="$REPO_ROOT/training_data"
MODEL_TARGET="$TARGET_DIR/nova_model.bin"
TOKENIZER_TARGET="$TARGET_DIR/nc_ai_tokenizer.bin"

MODEL_PATH=""
TOKENIZER_PATH=""
ARTIFACTS_DIR=""
COPY_MODE=0
FORCE=0

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

resolve_first_existing() {
    local candidate
    for candidate in "$@"; do
        if [[ -n "$candidate" && -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

stage_artifact() {
    local source="$1"
    local target="$2"

    mkdir -p "$(dirname "$target")"
    rm -f "$target"
    if [[ $COPY_MODE -eq 1 ]]; then
        cp "$source" "$target"
        return 0
    fi

    if ln -sf "$source" "$target" 2>/dev/null; then
        return 0
    fi

    cp "$source" "$target"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
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
        --copy)
            COPY_MODE=1
            shift
            ;;
        --force)
            FORCE=1
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

if [[ -n "$ARTIFACTS_DIR" ]]; then
    BUILD_DIR="$ARTIFACTS_DIR"
fi

if [[ -z "$MODEL_PATH" ]]; then
    MODEL_PATH="$(resolve_first_existing \
        "$BUILD_DIR/nc_model_v1_release.bin" \
        "$BUILD_DIR/nc_model.bin" \
        "$BUILD_DIR/nova_model.bin" || true)"
fi

if [[ -z "$TOKENIZER_PATH" ]]; then
    TOKENIZER_PATH="$(resolve_first_existing \
        "$BUILD_DIR/nc_model_v1_release_tokenizer.bin" \
        "$BUILD_DIR/nc_model_tokenizer.bin" \
        "$BUILD_DIR/nc_ai_tokenizer.bin" || true)"
fi

if [[ -z "$MODEL_PATH" || ! -f "$MODEL_PATH" ]]; then
    echo "Could not find a local model artifact. Pass --model-path or point --artifacts-dir at a build directory that contains nc_model_v1_release.bin, nc_model.bin, or nova_model.bin." >&2
    exit 1
fi

if [[ -z "$TOKENIZER_PATH" || ! -f "$TOKENIZER_PATH" ]]; then
    echo "Could not find a tokenizer artifact. Pass --tokenizer-path or point --artifacts-dir at a build directory that contains nc_model_v1_release_tokenizer.bin, nc_model_tokenizer.bin, or nc_ai_tokenizer.bin." >&2
    exit 1
fi

if [[ $FORCE -eq 1 || ! -f "$MODEL_TARGET" || ! -f "$TOKENIZER_TARGET" ]]; then
    stage_artifact "$MODEL_PATH" "$MODEL_TARGET"
    stage_artifact "$TOKENIZER_PATH" "$TOKENIZER_TARGET"
fi

echo ""
echo "  Local AI Model Setup"
echo "  Repo Root:  $REPO_ROOT"
echo "  Model Src:  $MODEL_PATH"
echo "  Token Src:  $TOKENIZER_PATH"
echo "  Target Dir: $TARGET_DIR"
echo ""
echo "  Local built-in model is ready:"
echo "    training_data/nova_model.bin"
echo "    training_data/nc_ai_tokenizer.bin"
echo ""