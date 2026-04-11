# NOVA Architecture

> **NOVA** is the AI engine powering NC's local inference, code generation, training, and reasoning capabilities.

---

## Open Source vs. Proprietary Split

NC is an open-source programming language. The **runtime, compiler, VM, stdlib, HTTP server, and all language features** are fully open source under the Apache 2.0 license.

The **NOVA engine** — the inference backend, model architecture, tokenizer, training pipeline, and reasoning system — is a proprietary component that ships as a **precompiled binary** in official NC releases.

| Component | License | Source |
|-----------|---------|--------|
| NC language runtime (`nc_lexer`, `nc_parser`, `nc_vm`, `nc_compiler`, `nc_gc`, etc.) | Apache 2.0 | This repo |
| NC stdlib, HTTP server, REPL, debugger, LSP | Apache 2.0 | This repo |
| NC UI compiler and runtime | Apache 2.0 | This repo |
| NC AI router (cloud provider adapters) | Apache 2.0 | This repo |
| **NOVA inference engine** | Proprietary | Pre-built binary |
| **NOVA tokenizer** | Proprietary | Pre-built binary |
| **NOVA model architecture** | Proprietary | Pre-built binary |
| **NOVA training pipeline** | Proprietary | Pre-built binary |
| **NOVA reasoning engine** | Proprietary | Pre-built binary |
| **Tensor operations** | Proprietary | Pre-built binary |

---

## What the Open-Source Build Provides

When you build NC from source, all AI commands work in one of two modes:

### 1. Template Engine (always available)

The open-source build includes a rule-based intent parser and template engine that generates working NC code from natural language descriptions without any model.

```bash
nc ai generate "create a REST API with authentication"
# Works immediately — no model required
```

Seven template types are supported: `service`, `ncui-page`, `full-app`, `crud`, `ai-service`, `middleware`, `test`.

### 2. NOVA Neural Mode (requires pre-built binary)

When the pre-built NC binary is installed, `nc ai generate` upgrades to full neural inference:

- Transformer-based code generation trained on NC corpora
- BPE tokenization tuned for NC syntax
- Chain-of-thought reasoning for complex generation tasks
- Auto-repair: up to 3 retry attempts on weak drafts
- Local inference — no internet, no API keys, no cloud

---

## How to Get NOVA

Install the pre-built NC binary (includes NOVA):

```bash
# macOS / Linux
curl -sSL https://nc.devheallabs.in/install.sh | bash

# Windows (PowerShell)
irm https://nc.devheallabs.in/install.ps1 | iex
```

The installer places:
```
~/.nc/bin/nc                          # NC binary with NOVA embedded
~/.nc/model/nc_model.bin              # NOVA model weights (optional download)
~/.nc/model/nc_model_tokenizer.bin    # NOVA tokenizer
```

Verify NOVA is active:
```bash
nc ai info
# NOVA engine: active
# Model: ~/.nc/model/nc_model.bin (512-dim, 8-layer, 16K vocab)
# Inference: local (no internet required)
```

---

## Stub System (Open-Source Build)

To allow the open-source build to compile cleanly, all NOVA-dependent functions have no-op stubs provided in two files:

| File | Purpose |
|------|---------|
| `engine/src/nc_nova_stubs.h` | Inline stubs for all NOVA types and functions |
| `engine/src/nc_generate_stubs.c` | Compiled stubs for generation, training, tensor, and reasoning functions |

When a NOVA function is called in the open-source build, it prints:

```
This NC AI feature requires the pre-built NC binary.
Install: curl -sSL https://nc.devheallabs.in/install.sh | bash
```

The shim headers (`nc_model.h`, `nc_tokenizer.h`, `nc_nova.h`, `nc_training.h`, `nc_nova_reasoning.h`, `nc_generate.h`) all delegate to `nc_nova_stubs.h` and compile cleanly in both the open-source and proprietary builds.

---

## NOVA Components

### Inference Engine (`nc_nova.c`)

The core transformer inference loop. Implements multi-head self-attention, feedforward layers, positional encoding, and KV-cache for efficient generation.

Sizes available:

| Size | Params | Use Case |
|------|--------|----------|
| micro | ~1M | Fast testing |
| small | ~7M | Development |
| base | ~85M | Standard generation |
| large | ~350M | High-quality output |
| 1B | ~1B | Production (recommended) |
| 7B | ~7B | Best quality |

### Tokenizer (`nc_tokenizer.c`)

BPE (Byte Pair Encoding) tokenizer trained on NC syntax, English descriptions, and code. Vocabulary sizes from 2K to 32K tokens. Handles NC keywords as single tokens for efficient encoding.

### Training Pipeline (`nc_training.c`)

Two-phase training:
1. **Pretrain** on NC corpus (teaches language syntax and patterns)
2. **Fine-tune** on instruction pairs (teaches project generation from descriptions)

Supports: Hebbian learning, NCE loss, contrastive graph regularization (CGR), predictive action-perception training (PAPT), HRL warmup.

### Reasoning Engine (`nc_nova_reasoning.c`)

Chain-of-thought reasoning for complex tasks. Builds structured prompts for agent-mode generation and grounded generation from URLs.

### Tensor Operations (`nc_tensor.c`)

Dense float32 tensor math with platform-accelerated backends:
- macOS: Accelerate framework (BLAS/vDSP)
- Linux: OpenBLAS (when available)
- All platforms: Fallback scalar implementation

### Auto-differentiation (`nc_autograd.c`)

Reverse-mode automatic differentiation for training. Builds a computation graph and propagates gradients for Adam optimizer updates.

---

## Training Your Own Model

See [NC_AI_TRAINING.md](NC_AI_TRAINING.md) for a step-by-step guide to training a local NOVA model using the NC training corpus.

The open-source repo includes:
- `engine/training_corpus/` — 30+ NC instruction pairs and corpus files
- `scripts/generate_training_data.py` — generates additional training examples
- `scripts/train_nc_ai.sh` — one-command training script

Training requires the pre-built NC binary (for the `nc train` command).

---

## Security

NOVA inference runs entirely locally. No data is sent to external servers. Model weights and tokenizer files are stored in `~/.nc/model/` and are never transmitted.

Cloud AI providers (OpenAI, Anthropic, Google, etc.) are accessed only when explicitly invoked via `ask AI to` in NC programs, and only with user-provided API keys.

---

## Further Reading

- [NC AI Training Guide](NC_AI_TRAINING.md) — train a local model
- [NC Language Guide](docs/NC_LANGUAGE_GUIDE.md) — using `ask AI to` and AI commands
- [NC AI SDK](../nc-ai-sdk/README.md) — building AI-native services with NC
- [Developer Guide](docs/DEVELOPER_GUIDE.md) — building NC from source
