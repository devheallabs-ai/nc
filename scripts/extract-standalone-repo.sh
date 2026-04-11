#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: $0 <destination> [--exclude-proprietary-ai]"
  exit 1
fi

DEST="$1"
EXCLUDE_PROPRIETARY_AI="${2:-}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SOURCE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

mkdir -p "$DEST"

rsync -a \
  --exclude '.git' \
  --exclude 'engine/build' \
  --exclude 'engine/build_local' \
  --exclude 'editor/vscode/node_modules' \
  --exclude '__pycache__' \
  --exclude '*.o' \
  --exclude '*.obj' \
  --exclude '*.exe' \
  --exclude '*.dll' \
  --exclude '*.so' \
  --exclude '*.dylib' \
  --exclude '*.bin' \
  "$SOURCE_ROOT"/ "$DEST"/

if [ "$EXCLUDE_PROPRIETARY_AI" = "--exclude-proprietary-ai" ]; then
  rm -f \
    "$DEST/engine/src/nc_nova.c" \
    "$DEST/engine/src/nc_nova.h" \
    "$DEST/engine/src/nc_cortex.c" \
    "$DEST/engine/src/nc_cortex.h" \
    "$DEST/engine/src/nc_training.c" \
    "$DEST/engine/src/nc_training.h" \
    "$DEST/engine/src/nc_model.h" \
    "$DEST/engine/src/nc_tokenizer.c" \
    "$DEST/engine/src/nc_ai_enterprise.c" \
    "$DEST/engine/nc_ai_model.bin" \
    "$DEST/engine/nc_ai_model_prod.bin" \
    "$DEST/engine/nc_ai_tokenizer.bin"
fi

echo ""
echo "Standalone nc-lang repo copy created at:"
echo "  $DEST"
echo ""
echo "Next steps:"
echo "  1. cd $DEST"
echo "  2. git init"
echo "  3. review OPEN_SOURCE_SCOPE.md and STANDALONE_REPO_BLUEPRINT.md"
echo "  4. push to https://github.com/devheallabs-ai/nc-lang"
