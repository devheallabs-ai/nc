# AI Create Validation

This document captures the current validation flow for `nc ai create` using the
repo-local built-in model path.

## What This Validates

The current v1 validation does not just check that `nc ai create` exits cleanly.
It verifies that the selected binary can:

- generate `service.nc`
- generate `app.ncui`
- generate `test_app.nc`
- generate `README.md`
- compile the generated NC UI into `dist/app.html` or `app.html`
- preserve the project contract required by the AI structural validator
- preserve enterprise features such as tenants, roles, analytics, approvals, and alerts when the prompt requests them

## Baseline Smoke

```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-local-ai-model.ps1
powershell -ExecutionPolicy Bypass -File tests\run_ai_smoke.ps1 -Prompt "inventory dashboard"
```

## Enterprise Semantic Validation

Validated on April 3, 2026 against the rebuilt `nc_release_ready.exe` and the
fine-tuned repo-local checkpoint `engine/build/nc_model_enterprise_ft120.bin`.

Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_ai_semantic_smoke.ps1 `
  -NcBin .\engine\build\nc_release_ready.exe `
  -ModelPath .\engine\build\nc_model_enterprise_ft120.bin `
  -TokenizerPath .\engine\build\nc_model_enterprise_ft120_tokenizer.bin `
  -OutputDir .\engine\build\ai_create_semantic_ops_dashboard_win_ft120 `
  -Force
```

Unix shell path (MSYS bash on Windows or a Unix shell with the same repo layout):

```bash
bash ./tests/run_ai_semantic_smoke.sh \
  --nc-bin engine/build/nc_release_ready.exe \
  --model-path engine/build/nc_model_enterprise_ft120.bin \
  --tokenizer-path engine/build/nc_model_enterprise_ft120_tokenizer.bin \
  --output-dir engine/build/ai_create_semantic_ops_dashboard_unix_ft120 \
  --force \
  --copy-model
```

## Latest Result

- Result: pass on Windows PowerShell and Unix shell path
- Generated files: `service.nc`, `app.ncui`, `test_app.nc`, `README.md`
- Frontend bundle: `dist/app.html`
- Structural validation: pass
- Semantic validation: pass
- Auto frontend release build: pass
- Generator mode: built-in LLM with explicit template fallback

Observed feature coverage from the validated complex prompt:

- retained: `dark-theme`, `dashboard`, `tenants`, `roles`, `analytics`, `approvals`, `alerts`
- service contract: includes `list_tenants`, `list_roles`, `permission_matrix`, `analytics_overview`, `list_approvals`, `approve_request`, `reject_request`, `list_alerts`
- page contract: includes Dashboard, Tenants, Roles, Analytics, Approvals, and Alerts sections
- shell-path bundle build: verified after normalizing Windows-native output paths for the `.exe` invocation

## How To Interpret This

- `pass` means the binary can create a complete NC Lang + NC UI project scaffold from the prompt and produce a working frontend bundle.
- The validated enterprise path uses the trained local model first, then the generator's semantic guard with `--template-fallback` when the LLM draft under-fills the requested feature set.
- Strict AI-only generation for this enterprise prompt is still improving; the release-validated path is the semantic smoke above.
- For release work, this validation now confirms project structure, NC UI compilation, and enterprise feature retention on both Windows and shell-based runs.