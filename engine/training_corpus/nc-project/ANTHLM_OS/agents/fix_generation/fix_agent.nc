// ══════════════════════════════════════════════════════════════════
//  HiveANT — Fix Generation Agent
//
//  Uses the Bee Colony algorithm to discover and optimize
//  remediation strategies. Generates executable fix commands.
// ══════════════════════════════════════════════════════════════════

to generate_fix with root_cause, system_context, constraints:
    purpose: "Use bee colony optimization to find the best remediation"
    log "FIX AGENT: Generating remediation for: " + str(root_cause.root_cause)

    set past_fixes to "none"
    if file_exists("knowledge/procedural.json"):
        set past_fixes to read_file("knowledge/procedural.json")

    set success_criteria to "Service returns to healthy state with normal error rates and latency"
    if constraints:
        set success_criteria to constraints

    ask AI to "You are the Fix Generation Agent in HiveANT, using Artificial Bee Colony optimization.

ROOT CAUSE:
{{root_cause}}

SYSTEM CONTEXT:
{{system_context}}

PAST SUCCESSFUL FIXES:
{{past_fixes}}

SUCCESS CRITERIA:
{{success_criteria}}

Run the full ABC algorithm:
PHASE 1 (Scout): Find 5+ diverse remediation strategies across categories:
  - Service restart, Container scaling, Config update, Rollback, Code patch, Infra change
PHASE 2 (Worker): Evaluate each strategy for effectiveness, safety, speed, reversibility
PHASE 3 (Evaluator): Select the optimal fix with full execution plan

Return ONLY valid JSON:
{
  \"algorithm\": \"Artificial Bee Colony\",
  \"scout_solutions\": [
    {\"id\": \"FIX-001\", \"category\": \"string\", \"description\": \"string\", \"fitness\": 0.0}
  ],
  \"recommended_fix\": {
    \"solution_id\": \"string\",
    \"category\": \"string\",
    \"description\": \"string\",
    \"commands\": [{\"step\": 0, \"cmd\": \"string\", \"description\": \"string\", \"risk\": \"low|medium|high\"}],
    \"pre_checks\": [\"string\"],
    \"post_checks\": [\"string\"],
    \"rollback_commands\": [{\"cmd\": \"string\", \"description\": \"string\"}],
    \"estimated_recovery_minutes\": 0,
    \"confidence_percent\": 0,
    \"risk_level\": \"low|medium|high\",
    \"fitness\": {\"effectiveness\": 0.0, \"safety\": 0.0, \"speed\": 0.0, \"reversibility\": 0.0, \"total\": 0.0}
  },
  \"alternative_fixes\": [
    {\"id\": \"string\", \"description\": \"string\", \"when_to_use\": \"string\", \"fitness\": 0.0}
  ],
  \"verification_steps\": [\"string\"],
  \"prevention_recommendations\": [\"string\"]
}" save as fix_result

    set fix_valid to validate(fix_result, ["recommended_fix"])
    if fix_valid.valid is not equal yes:
        set fix_result to {
            "algorithm": "Artificial Bee Colony",
            "recommended_fix": {
                "solution_id": "FIX-MANUAL",
                "category": "manual",
                "description": "Manual investigation required — AI could not generate fix",
                "commands": [{"step": 1, "cmd": "kubectl get pods -A | grep -v Running", "description": "Check pod health", "risk": "low"}],
                "confidence_percent": 0,
                "risk_level": "low"
            }
        }

    log "FIX AGENT: Recommended fix — " + fix_result.recommended_fix.description
    set fix_result.generated_at to time_now()
    respond with fix_result
