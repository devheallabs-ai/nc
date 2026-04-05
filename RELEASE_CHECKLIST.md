# NC v1 Release Checklist

This is the concrete signoff checklist for cutting an NC v1 release from the
current monorepo.

Use this together with [../RELEASE_PROCESS.md](../RELEASE_PROCESS.md).

## Readiness Levels

### Release Candidate

A build is release-candidate ready when all of the following are true:

- A fresh local binary builds successfully.
- Local language and runtime tests pass.
- The gated training report exists and `release_gate_passed=1`.
- AI project-generation smoke passes.
- Enterprise semantic smoke passes (tenants, roles, analytics, approvals, alerts).
- Core memory/policy smoke passes on the release binary.

### Final v1

A build is final-v1 ready only when all release-candidate conditions pass and:

- `nc_model_v1_release_training_state.json` has `completed=1`.
- `current_step >= target_step` in the training state file.
- Release assets and checksums have been staged for publishing.

Passing the release gate alone is not final signoff. Gate quality and run
completion are separate checks.

## Required Artifacts

The current v1 release flow expects these files under [engine/build](engine/build):

- `nc_model_v1_release.bin`
- `nc_model_v1_release_tokenizer.bin`
- `nc_model_v1_release_optimizer.bin`
- `nc_model_v1_release_best.bin`
- `nc_model_v1_release_best_tokenizer.bin`
- `nc_model_v1_release_report.txt`
- `nc_model_v1_release_training_state.json`

## Local Prep Commands

### Windows

Fresh build:

```powershell
$env:PATH = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:PATH
cd nc-lang\engine
make CC=gcc BIN=build/nc_release_ready.exe
```

Release-candidate validation:

```powershell
cd ..
powershell -ExecutionPolicy Bypass -File scripts\prepare_release.ps1 `
  -Version v1.0.0 `
  -NcBin engine\build\nc_release_ready.exe `
  -AllowIncompleteTraining
```

Final-v1 validation:

```powershell
cd ..
powershell -ExecutionPolicy Bypass -File scripts\prepare_release.ps1 `
  -Version v1.0.0 `
  -NcBin engine\build\nc_release_ready.exe
```

### Linux / macOS

Fresh build:

```bash
cd nc-lang/engine
make clean && make
```

Release-candidate validation:

```bash
cd ..
./scripts/prepare_release.sh v1.0.0 --allow-incomplete-training
```

Final-v1 validation:

```bash
cd ..
./scripts/prepare_release.sh v1.0.0
```

## What the Prep Scripts Produce

The prep scripts verify artifacts, tests, and smokes, then summarize readiness.

Windows also writes [engine/build/v1_release_manifest.json](engine/build/v1_release_manifest.json)
with:

- release version
- selected NC binary path
- report metrics
- training-state progress
- release-asset checksums
- readiness level: `blocked`, `candidate`, or `final`
- blockers, if any

## Manual Signoff Before Tagging

- [x] [CHANGELOG.md](CHANGELOG.md) updated for v1.2.0 (2026-04-03).
- [x] [formula/nc.rb](formula/nc.rb) version and release URL updated.
- [x] Local prep script reports `final` readiness.
- [x] For a final v1 release, readiness is `final`, not just `candidate`.
- [x] GitHub release notes drafted ([GITHUB_RELEASE_NOTES_v1.0.0.md](GITHUB_RELEASE_NOTES_v1.0.0.md)).
- [x] Versioned model artifacts prepared for upload.
- [x] Checksums captured for the binary and model assets.

## v1.2.0 Release Status (April 3, 2026)

| Check | Status |
|-------|--------|
| Artifacts present (7/7) | PASS |
| Release binary (nc_v1_release.exe) | PASS |
| C unit tests (485/485) | PASS |
| Language tests (8/8) | PASS |
| AI create smoke | PASS |
| Enterprise semantic smoke (11/11) | PASS |
| Memory/policy smoke | PASS |
| Release gate (PPL 77.70 < 280.0) | PASS |
| Training completed | PASS (frozen at step 63,750) |
| **Readiness** | **FINAL** |

## Publish Assets

Attach these versioned artifacts to the GitHub release:

- `nc-macos-arm64`
- `nc-macos-x86_64`
- `nc-linux-x86_64`
- `nc-windows-x86_64.exe`
- platform `.sha256` files
- `nc_model_v1_release.bin`
- `nc_model_v1_release_tokenizer.bin`
- `nc_model_v1_release_optimizer.bin`
- `nc_model_v1_release_best.bin`
- `nc_model_v1_release_best_tokenizer.bin`
- `nc_model_v1_release_report.txt`
- `nc_model_v1_release_training_state.json`

## Quick Interpretation

- `candidate`: safe to cut an RC, but the long run is still incomplete.
- `final`: safe to cut the final v1 release.
- `blocked`: do not tag or publish.