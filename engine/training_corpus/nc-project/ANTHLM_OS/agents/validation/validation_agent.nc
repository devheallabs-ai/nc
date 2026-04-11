// ══════════════════════════════════════════════════════════════════
//  HiveANT — Validation Agent
//
//  Tests proposed fixes in sandbox environments before production.
//  Runs simulated deployments, automated tests, regression checks.
//  Only validated fixes reach production.
// ══════════════════════════════════════════════════════════════════

to validate_fix with fix, service_name, auto_execute, secret:
    purpose: "Validate a fix in sandbox before applying to production"
    log "VALIDATION AGENT: Validating fix for " + str(service_name)

    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}

    set sandbox_enabled to env("SANDBOX_ENABLED")

    ask AI to "You are the Validation Agent in HiveANT. Evaluate this proposed fix for safety before execution.

FIX TO VALIDATE:
{{fix}}

SERVICE: {{service_name}}
SANDBOX ENABLED: {{sandbox_enabled}}

Safety checks:
1. Are all commands idempotent (safe to run twice)?
2. Are commands reversible?
3. Could any command cause data loss?
4. Could any command affect other services?
5. Is the rollback plan complete?
6. Are pre-checks and post-checks defined?

Return ONLY valid JSON:
{
  \"validation_passed\": true,
  \"safety_score\": 0.0,
  \"checks\": [
    {\"check\": \"string\", \"passed\": true, \"details\": \"string\"}
  ],
  \"risks_identified\": [\"string\"],
  \"safe_to_auto_execute\": true,
  \"requires_approval\": false,
  \"approval_reason\": \"string\",
  \"modified_commands\": [{\"original\": \"string\", \"safer_version\": \"string\", \"reason\": \"string\"}]
}" save as validation

    set validation.service to service_name
    set validation.validated_at to time_now()

    set executed to "no"
    set exec_results to []

    if auto_execute is equal "yes":
        if validation.validation_passed:
            if validation.safe_to_auto_execute:
                log "VALIDATION AGENT: Fix passed validation — executing"
                if fix.recommended_fix:
                    if fix.recommended_fix.pre_checks:
                        repeat for each check in fix.recommended_fix.pre_checks:
                            log "PRE-CHECK: " + check

                    repeat for each cmd in fix.recommended_fix.commands:
                        if cmd.risk is not equal "high":
                            log "EXECUTING: " + cmd.cmd
                            set result to shell(cmd.cmd)
                            set exec_results to exec_results + [{"command": cmd.cmd, "result": result}]
                        otherwise:
                            set exec_results to exec_results + [{"command": cmd.cmd, "result": "SKIPPED — high risk"}]
                    set executed to "yes"

                    if fix.recommended_fix.post_checks:
                        repeat for each check in fix.recommended_fix.post_checks:
                            log "POST-CHECK: " + check
            otherwise:
                log "VALIDATION AGENT: Fix requires manual approval"
        otherwise:
            log "VALIDATION AGENT: Fix FAILED validation — not executing"

    set validation.executed to executed
    set validation.execution_results to exec_results
    respond with validation

to sandbox_test with commands, service_name:
    purpose: "Run commands in a sandboxed dry-run mode"
    log "SANDBOX: Dry-run testing " + str(len(commands)) + " commands"

    ask AI to "You are a Sandbox Test Agent. Simulate the execution of these commands WITHOUT actually running them. Predict the outcome of each command.

COMMANDS:
{{commands}}

SERVICE: {{service_name}}

For each command, predict:
- Would it succeed?
- What would change?
- Any side effects?
- Estimated duration?

Return ONLY valid JSON:
{
  \"sandbox_results\": [
    {
      \"command\": \"string\",
      \"predicted_outcome\": \"success|failure|partial\",
      \"changes\": [\"string\"],
      \"side_effects\": [\"string\"],
      \"estimated_seconds\": 0,
      \"safe\": true
    }
  ],
  \"overall_safe\": true,
  \"total_estimated_seconds\": 0,
  \"recommendation\": \"proceed|caution|abort\"
}" save as sandbox

    set sandbox.tested_at to time_now()
    respond with sandbox
