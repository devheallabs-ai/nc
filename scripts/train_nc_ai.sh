#!/bin/bash
# ─────────────────────────────────────────────────────────────
#  NC AI Self-Contained Training Script
#  Trains the NC model to generate full projects from prompts.
#  No external LLM required — 100% local.
#
#  Usage:
#    ./scripts/train_nc_ai.sh
#    ./scripts/train_nc_ai.sh --fast      (fewer epochs, test run)
#    ./scripts/train_nc_ai.sh --full      (full training, hours)
# ─────────────────────────────────────────────────────────────

set -e
cd "$(dirname "$0")/.."

INSTRUCTION_DATA="engine/training_corpus/instruction-pairs"
CORPUS_DATA="engine/training_corpus/nc-corpus"
MODEL_OUT="$HOME/.nc/model/nc_model.bin"
NC_BIN="./build/nc"

# Fallback binary locations
if [ ! -f "$NC_BIN" ]; then
    NC_BIN="nc"
fi

MODE="normal"
if [ "$1" = "--fast" ]; then MODE="fast"; fi
if [ "$1" = "--full" ]; then MODE="full"; fi

echo ""
echo "  NC AI Training — Self-Contained Code Generation"
echo "  ─────────────────────────────────────────────────"
echo "  Mode:   $MODE"
echo "  Output: $MODEL_OUT"
echo ""

# ── Step 1: Generate instruction-pairs if missing ───────────
if [ ! -d "$INSTRUCTION_DATA" ] || [ -z "$(ls -A "$INSTRUCTION_DATA" 2>/dev/null)" ]; then
    echo "  [1/3] Generating instruction-tuning training data..."
    python3 scripts/generate_training_data.py
else
    COUNT=$(ls "$INSTRUCTION_DATA"/*.nc 2>/dev/null | wc -l)
    echo "  [1/3] Found $COUNT instruction-pair files in $INSTRUCTION_DATA"
fi

echo ""

# ── Step 2: Phase 1 — Pretrain on NC corpus ─────────────────
echo "  [2/3] Phase 1: Pretraining on NC corpus..."
echo "        (teaches the model NC syntax and patterns)"
echo ""

if [ "$MODE" = "fast" ]; then
    PRETRAIN_EPOCHS=5
    PRETRAIN_STEPS=500
elif [ "$MODE" = "full" ]; then
    PRETRAIN_EPOCHS=30
    PRETRAIN_STEPS=0
else
    PRETRAIN_EPOCHS=15
    PRETRAIN_STEPS=0
fi

# Only pretrain if no existing model
if [ ! -f "$MODEL_OUT" ]; then
    "$NC_BIN" train \
        --data "$CORPUS_DATA" \
        --dim 512 \
        --layers 8 \
        --vocab 16384 \
        --epochs "$PRETRAIN_EPOCHS" \
        --eval-interval 100 \
        --eval-split 0.1 \
        --output "$MODEL_OUT"
    echo ""
    echo "  Phase 1 complete: NC corpus pretraining done."
else
    echo "  Existing model found at $MODEL_OUT — skipping Phase 1 pretraining."
fi

echo ""

# ── Step 3: Phase 2 — Instruction fine-tuning ───────────────
echo "  [3/3] Phase 2: Instruction fine-tuning..."
echo "        (teaches: prompt -> full NC project generation)"
echo ""

if [ "$MODE" = "fast" ]; then
    FINETUNE_EPOCHS=20
elif [ "$MODE" = "full" ]; then
    FINETUNE_EPOCHS=100
else
    FINETUNE_EPOCHS=50
fi

"$NC_BIN" train \
    --data "$INSTRUCTION_DATA" \
    --epochs "$FINETUNE_EPOCHS" \
    --lr 0.0001 \
    --eval-interval 50 \
    --eval-split 0.15 \
    --output "$MODEL_OUT"

echo ""
echo "  ─────────────────────────────────────────────────"
echo "  Training complete!"
echo "  Model saved: $MODEL_OUT"
echo ""
echo "  Test it:"
echo '  nc generate --full "build me a todo app with auth"'
echo '  nc generate --full "create a blog with AI content generation"'
echo '  nc generate "a REST API for user management"'
echo ""
