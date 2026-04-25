# NC AI Training Guide
## Train a self-contained model that generates full NC projects from plain English

---

## How It Works

When you run `nc generate "build me a todo app"`, the NC engine:
1. Looks for a trained model at `~/.nc/model/nc_model.bin`
2. If found: runs local inference (zero external API)
3. If not found: falls back to deterministic templates

Training teaches the model to map:
```
"build me a todo app with authentication"
  →  todo_service.nc  (nc-lang backend)
  +  todo.ncui        (nc-ui frontend)
  +  todo_agent.nc    (nc-ai agent)
```

No cloud AI. No external APIs. No internet. Fully local.

---

## Step 0 — Build the NC binary

You must build `nc` before training.

### Windows (MSYS2 / MinGW)
```bash
# Install MSYS2 from https://www.msys2.org then:
pacman -S --noconfirm mingw-w64-x86_64-gcc make

cd nc-lang/engine
make
# Output: build/nc.exe
```

### macOS
```bash
xcode-select --install       # installs clang
cd nc-lang/engine
make
# Output: build/nc
```

### Linux
```bash
sudo apt install build-essential libcurl4-openssl-dev
# or: sudo dnf install gcc make libcurl-devel

cd nc-lang/engine
make
# Output: build/nc
```

### Verify the build
```bash
./build/nc --version
./build/nc train --help
./build/nc generate --help
```

---

## Step 1 — Generate Training Data

```bash
cd nc-lang
python3 scripts/generate_training_data.py
```

This creates **30 complete NC project files** in:
```
nc-lang/engine/training_corpus/instruction-pairs/
  todo_full_app.nc
  blog_full_app.nc
  ecommerce_full_app.nc
  chatbot_full_app.nc
  auth_full_app.nc
  dashboard_full_app.nc
  files_full_app.nc
  support_full_app.nc
  inventory_full_app.nc
  booking_full_app.nc
  notes_full_app.nc
  job-queue_full_app.nc
  polls_full_app.nc
  notifications_full_app.nc
  recipes_full_app.nc
  api-gateway_full_app.nc
  newsletter_full_app.nc
  url-shortener_full_app.nc
  survey_full_app.nc
  monitoring_full_app.nc
  + 10 API-only variations
```

Each file is one training example in instruction format:
```
<|begin|>
// Description: a todo app with task management and AI assistance
// Type: full app
service "todo"
version "1.0.0"
...complete backend code...
// === NC_FILE_SEPARATOR ===
page "Todo App"
...complete frontend code...
// === NC_AGENT_SEPARATOR ===
service "todo-agent"
...ai agent code...
<|end|>
```

---

## Step 2 — Train the Model

### Quick commands

```bash
cd nc-lang/engine
```

#### Minimal test (fastest, ~2-5 min on CPU)
```bash
./build/nc train \
  --data ../training_corpus/instruction-pairs \
  --dim 128 \
  --layers 4 \
  --vocab 4096 \
  --epochs 10
```

#### Recommended for release (good quality, ~30-60 min on CPU)
```bash
./build/nc train \
  --data ../training_corpus/nc-corpus \
  --data ../training_corpus/instruction-pairs \
  --dim 512 \
  --layers 8 \
  --vocab 16384 \
  --epochs 15 \
  --lr 0.001 \
  --eval-interval 100 \
  --eval-split 0.1
```

> Run both `--data` dirs: first the corpus (teaches NC syntax), then instruction pairs (teaches project generation).

#### Fine-tune only (if model already exists)
```bash
./build/nc train \
  --data ../training_corpus/instruction-pairs \
  --epochs 50 \
  --lr 0.0001 \
  --eval-interval 50 \
  --eval-split 0.15
```

> Uses lower `--lr` when continuing from an existing model checkpoint.

#### High quality (best output, needs ~4GB RAM, several hours)
```bash
./build/nc train \
  --data ../training_corpus/nc-corpus \
  --data ../training_corpus/instruction-pairs \
  --dim 1024 \
  --layers 16 \
  --vocab 32768 \
  --epochs 30 \
  --lr 0.0005 \
  --eval-interval 100 \
  --eval-split 0.1 \
  --max-eval-ppl 30 \
  --report build/train_report.txt
```

---

## All Training Flags

| Flag | Default | Description |
|------|---------|-------------|
| `--data DIR` | built-in corpus | Directory of `.nc`/`.txt` files. Can repeat for multiple dirs. |
| `--epochs N` | 10 | Full passes over the dataset |
| `--steps N` | auto | Override total steps (replaces epochs) |
| `--lr RATE` | 0.001 | Learning rate. Use 0.0001 when fine-tuning. |
| `--dim N` | 256 | Model embedding dimension |
| `--layers N` | 6 | Number of transformer layers |
| `--vocab N` | 4096 | BPE vocabulary size |
| `--eval-interval N` | 100 | Run validation every N steps |
| `--eval-split R` | 0.10 | Fraction of data held out for eval |
| `--eval-max N` | 256 | Max sequences evaluated per pass |
| `--release-gate` | off | Stop only when perplexity target is hit |
| `--max-eval-ppl N` | 20 | Max allowed eval perplexity (release gate) |
| `--gate-evals N` | 2 | Consecutive passing evals needed |
| `--output PATH` | `~/.nc/model/nc_model.bin` | Where to save the model |
| `--report PATH` | none | Save training summary to file |
| `--preset v1-release` | none | Preset for v1 release quality |

---

## Model Size Guide

| Config | `--dim` | `--layers` | `--vocab` | RAM | Speed | Quality |
|--------|---------|-----------|---------|-----|-------|---------|
| Tiny (test) | 128 | 4 | 2048 | ~200MB | Fast | Basic syntax |
| Small (default) | 256 | 6 | 4096 | ~500MB | Medium | Good patterns |
| Recommended | 512 | 8 | 16384 | ~1GB | Slower | Good output |
| Production | 1024 | 16 | 32768 | ~4GB | Hours | Best output |

Start with "Recommended" for release. Use "Production" when you have time and RAM.

---

## Step 3 — Test the Trained Model

After training completes, the model is at `~/.nc/model/nc_model.bin`.

```bash
cd nc-lang/engine

# Generate a full 3-file project
./build/nc generate --full "build me a todo app with user authentication"

# Generate a backend service only
./build/nc generate "a REST API for managing blog posts"

# Generate a UI page only
./build/nc generate --ui "a dark landing page for a SaaS product"

# Generate with a specific type
./build/nc generate --type crud "products"
./build/nc generate --type ai "a ticket classifier"

# Print to stdout (don't create files)
./build/nc generate --stdout "a simple hello world service"
```

### What gets created with `--full`

```
todo_service.nc     ← nc-lang backend (behaviors, routes, data)
todo.ncui           ← nc-ui frontend  (pages, forms, components)
todo_agent.nc       ← nc-ai agent     (AI behaviors, classify, route)
```

---

## Step 4 — Run the Generated Project

```bash
# Install nc first if not in PATH
# Then run any of the generated files:

nc run todo_service.nc
# Service starts on http://localhost:8080

nc run --ui todo.ncui
# Opens browser with the frontend

nc run todo_agent.nc
# AI agent starts on http://localhost:8081
```

---

## Using the One-Command Script

```bash
cd nc-lang

# Generate data + train (normal, ~1 hour)
./scripts/train_nc_ai.sh

# Quick test run (5 min)
./scripts/train_nc_ai.sh --fast

# Full quality run (several hours)
./scripts/train_nc_ai.sh --full
```

---

## Improving Model Quality Over Time

### Add more training examples
```bash
# Edit the generator to add more project types
nano scripts/generate_training_data.py

# Re-generate
python3 scripts/generate_training_data.py

# Re-train (fine-tune from existing model)
./build/nc train \
  --data engine/training_corpus/instruction-pairs \
  --epochs 30 \
  --lr 0.0001
```

### Use auto-distillation (collects real usage data)
Every time the model generates code with confidence > 0.5, it saves the pair to:
```
~/.nc/training_data/auto_distilled.txt
```

After collecting real usage data, retrain with it:
```bash
./build/nc train \
  --data ~/.nc/training_data \
  --data engine/training_corpus/instruction-pairs \
  --epochs 20 \
  --lr 0.00005
```

### Check training quality (perplexity)
Lower perplexity = better model. Target: < 20 for NC code generation.

```bash
# Training prints eval perplexity every --eval-interval steps:
# Step 100/1500 | loss=2.45 | ppl=11.6 | lr=0.001000
# Step 200/1500 | loss=2.12 | ppl=8.3  | lr=0.001000
```

---

## Troubleshooting

### "No training files found"
```bash
# Specify the data directory explicitly
./build/nc train --data engine/training_corpus/instruction-pairs
```

### "Out of memory" during training
Reduce model size:
```bash
./build/nc train --dim 256 --layers 6 --vocab 4096 --data ...
```

### Model generates gibberish
1. Train for more epochs: add `--epochs 50`
2. Add more training data: `python3 scripts/generate_training_data.py`
3. Lower learning rate for fine-tuning: `--lr 0.0001`

### Model exists but not being used by `nc generate`
Check the model path:
```bash
ls ~/.nc/model/
# Should show: nc_model.bin  nc_model_tokenizer.bin

# If missing, specify output path when training:
./build/nc train --data ... --output ~/.nc/model/nc_model.bin
```

### Training is too slow
- On macOS: Accelerate framework is auto-used (fast)
- On Linux: Install OpenBLAS for 10-100x speedup:
  ```bash
  sudo apt install libopenblas-dev
  make clean && make
  ```
- On Windows: Use MSYS2 MinGW GCC (not MSVC)

---

## File Locations Reference

| What | Where |
|------|-------|
| Trained model | `~/.nc/model/nc_model.bin` |
| Tokenizer | `~/.nc/model/nc_model_tokenizer.bin` |
| Optimizer state | `~/.nc/model/nc_model_optimizer.bin` |
| Auto-distilled data | `~/.nc/training_data/auto_distilled.txt` |
| Instruction pairs | `nc-lang/engine/training_corpus/instruction-pairs/` |
| NC corpus | `nc-lang/engine/training_corpus/nc-corpus/` |
| Training script | `nc-lang/scripts/train_nc_ai.sh` |
| Data generator | `nc-lang/scripts/generate_training_data.py` |

---

## Full Training Sequence (Copy-Paste)

```bash
# 1. Build
cd nc-lang/engine && make && cd ../..

# 2. Generate training data
python3 nc-lang/scripts/generate_training_data.py

# 3. Phase 1: Pretrain on NC corpus (teaches syntax)
./nc-lang/engine/build/nc train \
  --data nc-lang/engine/training_corpus/nc-corpus \
  --dim 512 --layers 8 --vocab 16384 \
  --epochs 15 --lr 0.001 \
  --eval-interval 100 --eval-split 0.1

# 4. Phase 2: Fine-tune on instruction pairs (teaches project generation)
./nc-lang/engine/build/nc train \
  --data nc-lang/engine/training_corpus/instruction-pairs \
  --epochs 50 --lr 0.0001 \
  --eval-interval 50 --eval-split 0.15

# 5. Test
./nc-lang/engine/build/nc generate --full "build me a todo app with login"
```
