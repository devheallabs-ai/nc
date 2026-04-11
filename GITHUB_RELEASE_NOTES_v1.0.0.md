# NC v1.0.0 — First Stable Release

**Release date:** 2026-04-03

NC v1.0.0 is the first stable, production-ready release of the NC language — a plain-English AI programming language with a built-in neural model, GPU acceleration, enterprise code generation, and a native UI compiler.

---

## Highlights

### NC Language Engine
- Plain-English syntax for APIs, services, and AI apps — no boilerplate
- 125 token types, 57 AST node types, 48 opcodes
- 160+ built-in functions: data structures, HTTP, async, streams, crypto, database, ML
- Typed error catching, assert, test blocks, yield, await, stream respond
- Compiler + interpreter + bytecode VM in a single zero-dependency binary

### Built-in AI Model
- **`nc ai generate`** — generate NC apps from plain English
- Decoder-only transformer: 6 layers, 8 heads, 256 dim, ~5M params
- BPE tokenizer with 32K vocabulary
- Model frozen at best quality-gated checkpoint — eval PPL **77.70** (gate threshold: 280.0)
- 63,750 training steps across 76,976 sequences from a 549-file NC corpus
- Enterprise code generation validated: 11/11 semantic checks pass (tenants, roles, analytics, approvals, alerts)
- `nc_model_v1_release_best.bin` is the canonical v1 release model

### GPU & Hardware Acceleration
- Metal GPU on Apple Silicon (10–50× GEMM speedup via MPSMatrixMultiplication)
- Apple Accelerate BLAS (50–100× matmul via `cblas_sgemm`)
- 3-tier dispatch: Metal GPU → Accelerate BLAS → portable CPU fallback

### NC UI — Native UI Compiler (Built In)
- **`nc ui build <file.ncui>`** — compile plain-English markup to production HTML
- Zero external dependencies — no Node.js, no npm
- Outputs HTML + CSP security manifests + Netlify / Vercel / Azure / Cloudflare configs
- Legacy Node.js path: `npm install -g nc-ui`

### World Model AI
- Formal decision intelligence: W = (S, A, T, R, M, Π)
- Probabilistic transitions, reward scoring, Hebbian memory consolidation
- Causal graph reasoning, swarm multi-agent voting

---

## Quality Validation

| Check | Result |
|-------|--------|
| C unit tests | 485 / 485 PASS |
| Language tests | 8 / 8 PASS |
| AI project smoke | PASS |
| Enterprise semantic smoke | 11 / 11 PASS |
| Memory / policy smoke | PASS |
| Release gate (eval PPL < 280.0) | PASS — 77.70 |
| Training completed | PASS — frozen at step 63,750 |
| **Overall readiness** | **FINAL** |

---

## NC AI Model Bundle (Separate Asset)

The standalone NC AI model bundle `release_bundle_v2_20260403.zip` is attached as a release asset.

| Property | Value |
|----------|-------|
| Architecture | Decoder-only transformer, 6 layers, 8 heads, 256 dim |
| Vocab | 4,096 BPE tokens, 512-token context window |
| Best eval PPL | 407.71 |
| Benchmark | 12 / 12 pass, avg 625 ms latency, 169 tok/s |
| Training | 8,000 steps, 63,585 sequences, `completed=1` |

---

## Release Assets

| Asset | Description |
|-------|-------------|
| `nc-linux-x86_64` + `.sha256` | Linux x86_64 binary |
| `nc-macos-arm64` + `.sha256` | macOS Apple Silicon binary |
| `nc-macos-x86_64` + `.sha256` | macOS Intel binary |
| `nc-windows-x86_64.exe` + `.sha256` | Windows x64 binary |
| `nc_model_v1_release.bin` | Built-in language model |
| `nc_model_v1_release_tokenizer.bin` | Tokenizer |
| `nc_model_v1_release_optimizer.bin` | Optimizer state |
| `nc_model_v1_release_best.bin` | Best checkpoint (canonical) |
| `nc_model_v1_release_best_tokenizer.bin` | Best checkpoint tokenizer |
| `nc_model_v1_release_report.txt` | Training report |
| `nc_model_v1_release_training_state.json` | Training state |
| `release_bundle_v2_20260403.zip` | NC AI standalone model bundle |

### SHA-256 Checksums

```
nc_v1_release.exe:                      1f20c4969096de38502d606a12bedd964f4efa23607baed605ce1e0d122a9eee
nc_model_v1_release.bin:                bc1b5f2267d4282f366b6e1d817bd2d49211b8dfff8e4110025985992b459ae0
nc_model_v1_release_tokenizer.bin:      87223d8addd11fc841b902a1d014adc584742eb9febebbda825e0e15d4a48c51
nc_model_v1_release_optimizer.bin:      f4bce46b47639c628217009f467a3f82cae2eb91618b0af5cb65beda03e6e09e
nc_model_v1_release_best.bin:           bc1b5f2267d4282f366b6e1d817bd2d49211b8dfff8e4110025985992b459ae0
nc_model_v1_release_best_tokenizer.bin: 87223d8addd11fc841b902a1d014adc584742eb9febebbda825e0e15d4a48c51
```

**NC AI Bundle SHA-256**
```
nc_ai_model_prod.bin:    f89d8e9f4a185caa52abeb0e9e32d4c923fb62f79acdd086176b3acc9ebd4a44
nc_ai_tokenizer.bin:     3337b48760c221303a755e98c7d478f480c13c8b8c5e5593698305acaf50831d
benchmark_report.txt:    8c1f3f748b47e35fcf04117e82b114498245060ef3ada2c10b4e85c5aee42902
training_report.txt:     37f33a11a4ff7629ee48d90cb02b31305f95ca6f929b14fd11e7b45faa9359a2
training_state.json:     f037c9b98b42219d3f0fb9e56b05ca2c095008cc315538e6c6b1f3d6044a07d7
smoke_service.nc:        4a3579abb974f655d7e1369e87acc2431c87bc6d5ed952c6ca2a5753428a257c
smoke_app.ncui:          ce55aeebdaa6c8e172d665ef3d567e235de18c2f7a5f1e2159d5636523008ffc
smoke_README.md:         baebac5dd98f5fb60a248082d5d46831a62069e9dd58f3b3b08b09babdb9775c
```

---

## Installation

**Homebrew (macOS / Linux)**
```bash
brew install devheallabs/nc/nc
```

**Direct download**
```bash
# macOS Apple Silicon
curl -Lo nc https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-macos-arm64
chmod +x nc && sudo mv nc /usr/local/bin/

# Linux
curl -Lo nc https://github.com/devheallabs-ai/nc-lang/releases/download/v1.0.0/nc-linux-x86_64
chmod +x nc && sudo mv nc /usr/local/bin/
```

**Quick start**
```bash
nc version
nc "show 42 + 8"
nc serve service.nc
nc ai generate "REST API with user auth"
nc ui build app.ncui
```

Documentation: https://nc.devheallabs.in/docs

---

## Full Changelog

See [CHANGELOG.md](CHANGELOG.md) for the complete history.
