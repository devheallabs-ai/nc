// ══════════════════════════════════════════════════════════════════
//  HiveANT — Artificial Bee Colony (ABC) for Remediation Optimization
//
//  Three agent roles:
//    Scout Agents   — Search for potential fixes
//    Worker Agents  — Test candidate solutions
//    Evaluator Agents — Measure success metrics
//
//  Possible fixes: restart, scale, config update, rollback, code patch
//  The highest-scoring solution becomes the recommended fix.
// ══════════════════════════════════════════════════════════════════

to bee_colony_init with config:
    purpose: "Initialize the bee colony optimization parameters"
    shell("mkdir -p memory/pheromone")

    if config:
        set params to config
    otherwise:
        set params to {
            "scout_count": 10,
            "worker_count": 30,
            "evaluator_count": 10,
            "max_cycles": 50,
            "abandonment_limit": 5,
            "fitness_weights": {
                "effectiveness": 0.4,
                "safety": 0.3,
                "speed": 0.2,
                "reversibility": 0.1
            }
        }

    write_file("memory/pheromone/abc_config.json", json_encode(params))
    log "ABC: Bee colony initialized — scouts=" + str(params.scout_count) + " workers=" + str(params.worker_count) + " evaluators=" + str(params.evaluator_count)
    respond with params

to bee_scout_phase with root_cause, system_context, constraints:
    purpose: "Scout bees explore potential remediation strategies"
    shell("mkdir -p memory/pheromone")

    set past_fixes to "none"
    if file_exists("knowledge/procedural.json"):
        set past_fixes to read_file("knowledge/procedural.json")

    ask AI to "You are a Scout Bee in an Artificial Bee Colony algorithm.

ROOT CAUSE IDENTIFIED:
{{root_cause}}

SYSTEM CONTEXT:
{{system_context}}

CONSTRAINTS:
{{constraints}}

PAST SUCCESSFUL FIXES (procedural memory):
{{past_fixes}}

TASK: As {{config.scout_count}} scout bees, search the solution space for potential remediation strategies.
Explore diverse fix categories:
1. Service restart (low risk)
2. Container scaling (low risk)
3. Configuration update (medium risk)
4. Deployment rollback (medium risk)
5. Code patch (high risk)
6. Infrastructure change (high risk)

For each strategy, estimate a fitness score based on:
- effectiveness (0-1): how likely to fix the root cause
- safety (0-1): how safe to execute
- speed (0-1): how fast to apply
- reversibility (0-1): how easy to roll back

Return ONLY valid JSON:
{
  \"phase\": \"scout\",
  \"food_sources\": [
    {
      \"solution_id\": \"FIX-001\",
      \"category\": \"restart|scale|config|rollback|patch|infrastructure\",
      \"description\": \"string\",
      \"commands\": [{\"cmd\": \"string\", \"description\": \"string\"}],
      \"fitness\": {
        \"effectiveness\": 0.0,
        \"safety\": 0.0,
        \"speed\": 0.0,
        \"reversibility\": 0.0,
        \"total\": 0.0
      },
      \"risk_level\": \"low|medium|high\",
      \"estimated_time_minutes\": 0,
      \"rollback_plan\": \"string\",
      \"trial_count\": 0,
      \"abandonment_counter\": 0
    }
  ],
  \"scout_count\": 0,
  \"diversity_score\": 0.0
}" save as scout_result

    write_file("memory/pheromone/scout_results.json", json_encode(scout_result))
    log "ABC Scout: Found " + str(len(scout_result.food_sources)) + " remediation strategies"
    respond with scout_result

to bee_worker_phase with food_sources, system_context:
    purpose: "Worker bees evaluate and refine candidate solutions"

    ask AI to "You are Worker Bees in an Artificial Bee Colony algorithm.

CANDIDATE SOLUTIONS (food sources from scouts):
{{food_sources}}

SYSTEM CONTEXT:
{{system_context}}

TASK: As worker bees, evaluate each candidate solution:
1. Simulate executing each fix mentally
2. Identify potential side effects
3. Check for conflicts between fixes
4. Refine the fitness scores based on deeper analysis
5. Rank solutions by total fitness

Apply the neighborhood search: for each solution, try small variations to improve fitness.
If a solution has been tried too many times without improvement, increment its abandonment counter.

Return ONLY valid JSON:
{
  \"phase\": \"worker\",
  \"evaluated_solutions\": [
    {
      \"solution_id\": \"string\",
      \"category\": \"string\",
      \"description\": \"string\",
      \"commands\": [{\"cmd\": \"string\", \"description\": \"string\"}],
      \"fitness\": {
        \"effectiveness\": 0.0,
        \"safety\": 0.0,
        \"speed\": 0.0,
        \"reversibility\": 0.0,
        \"total\": 0.0
      },
      \"side_effects\": [\"string\"],
      \"conflicts_with\": [\"string\"],
      \"refined\": true,
      \"risk_level\": \"low|medium|high\",
      \"rollback_plan\": \"string\",
      \"trial_count\": 0,
      \"abandonment_counter\": 0
    }
  ],
  \"best_solution\": {
    \"solution_id\": \"string\",
    \"description\": \"string\",
    \"fitness_total\": 0.0
  },
  \"solutions_abandoned\": 0
}" save as worker_result

    write_file("memory/pheromone/worker_results.json", json_encode(worker_result))
    log "ABC Worker: Evaluated " + str(len(worker_result.evaluated_solutions)) + " solutions, best=" + worker_result.best_solution.solution_id
    respond with worker_result

to bee_evaluator_phase with evaluated_solutions, success_criteria:
    purpose: "Evaluator bees select the optimal solution"

    ask AI to "You are Evaluator Bees in an Artificial Bee Colony algorithm.

EVALUATED SOLUTIONS:
{{evaluated_solutions}}

SUCCESS CRITERIA:
{{success_criteria}}

TASK: Final evaluation phase.
1. Apply tournament selection between top candidates
2. Consider real-world constraints (downtime, user impact, team availability)
3. Produce a final ranked list with the recommended fix
4. Generate a detailed execution plan for the winning solution
5. Define verification steps to confirm the fix worked

Return ONLY valid JSON:
{
  \"phase\": \"evaluator\",
  \"final_ranking\": [
    {
      \"rank\": 1,
      \"solution_id\": \"string\",
      \"category\": \"string\",
      \"description\": \"string\",
      \"fitness_total\": 0.0,
      \"risk_level\": \"string\",
      \"recommended\": true
    }
  ],
  \"recommended_fix\": {
    \"solution_id\": \"string\",
    \"description\": \"string\",
    \"category\": \"string\",
    \"commands\": [{\"cmd\": \"string\", \"description\": \"string\", \"order\": 0}],
    \"pre_checks\": [\"string\"],
    \"post_checks\": [\"string\"],
    \"rollback_commands\": [{\"cmd\": \"string\", \"description\": \"string\"}],
    \"estimated_recovery_minutes\": 0,
    \"confidence_percent\": 0,
    \"risk_level\": \"string\"
  },
  \"alternative_fixes\": [
    {
      \"solution_id\": \"string\",
      \"description\": \"string\",
      \"when_to_use\": \"string\"
    }
  ]
}" save as eval_result

    write_file("memory/pheromone/evaluator_results.json", json_encode(eval_result))
    log "ABC Evaluator: Recommended fix = " + eval_result.recommended_fix.solution_id
    respond with eval_result

to bee_colony_run with root_cause, system_context, constraints, success_criteria:
    purpose: "Run full ABC cycle: scout -> worker -> evaluator"
    log "ABC: Starting full bee colony optimization cycle"

    if file_exists("memory/pheromone/abc_config.json"):
        set config to json_decode(read_file("memory/pheromone/abc_config.json"))
    otherwise:
        set config to {"scout_count": 10, "worker_count": 30, "evaluator_count": 10}

    set past_fixes to "none"
    if file_exists("knowledge/procedural.json"):
        set past_fixes to read_file("knowledge/procedural.json")

    ask AI to "You are a Scout Bee in an Artificial Bee Colony algorithm. ROOT CAUSE: {{root_cause}}. SYSTEM: {{system_context}}. CONSTRAINTS: {{constraints}}. PAST FIXES: {{past_fixes}}. Search for {{config.scout_count}} diverse remediation strategies covering: restart, scale, config update, rollback, code patch, infrastructure change. Return ONLY valid JSON: {\"food_sources\": [{\"solution_id\": \"FIX-001\", \"category\": \"string\", \"description\": \"string\", \"commands\": [{\"cmd\": \"string\", \"description\": \"string\"}], \"fitness\": {\"effectiveness\": 0.0, \"safety\": 0.0, \"speed\": 0.0, \"reversibility\": 0.0, \"total\": 0.0}, \"risk_level\": \"low|medium|high\", \"rollback_plan\": \"string\"}]}" save as scout_result

    ask AI to "You are Worker Bees evaluating these candidate solutions: {{scout_result}}. SYSTEM: {{system_context}}. Evaluate each: simulate execution, find side effects, refine fitness scores, rank by total fitness. Return ONLY valid JSON: {\"evaluated_solutions\": [{\"solution_id\": \"string\", \"description\": \"string\", \"fitness_total\": 0.0, \"side_effects\": [\"string\"], \"risk_level\": \"string\", \"commands\": [{\"cmd\": \"string\", \"description\": \"string\"}], \"rollback_plan\": \"string\"}], \"best_solution\": {\"solution_id\": \"string\", \"fitness_total\": 0.0}}" save as worker_result

    ask AI to "You are Evaluator Bees selecting the optimal fix. EVALUATED: {{worker_result}}. CRITERIA: {{success_criteria}}. Do tournament selection, produce final ranking, generate detailed execution plan for the winner. Return ONLY valid JSON: {\"recommended_fix\": {\"solution_id\": \"string\", \"description\": \"string\", \"category\": \"string\", \"commands\": [{\"cmd\": \"string\", \"description\": \"string\", \"order\": 0}], \"pre_checks\": [\"string\"], \"post_checks\": [\"string\"], \"rollback_commands\": [{\"cmd\": \"string\", \"description\": \"string\"}], \"estimated_recovery_minutes\": 0, \"confidence_percent\": 0, \"risk_level\": \"string\"}, \"final_ranking\": [{\"rank\": 1, \"solution_id\": \"string\", \"description\": \"string\", \"fitness_total\": 0.0}], \"alternative_fixes\": [{\"solution_id\": \"string\", \"description\": \"string\", \"when_to_use\": \"string\"}]}" save as eval_result

    set full_result to {
        "algorithm": "Artificial Bee Colony",
        "scout_phase": scout_result,
        "worker_phase": worker_result,
        "evaluator_phase": eval_result,
        "recommended_fix": eval_result.recommended_fix,
        "completed_at": time_now()
    }

    write_file("memory/pheromone/abc_full_result.json", json_encode(full_result))
    log "ABC: Full cycle complete — recommended fix: " + eval_result.recommended_fix.description
    respond with full_result

to bee_colony_status:
    purpose: "Get current bee colony status"
    set config to "not initialized"
    if file_exists("memory/pheromone/abc_config.json"):
        set config to json_decode(read_file("memory/pheromone/abc_config.json"))

    set has_scout to file_exists("memory/pheromone/scout_results.json")
    set has_worker to file_exists("memory/pheromone/worker_results.json")
    set has_eval to file_exists("memory/pheromone/evaluator_results.json")

    respond with {
        "algorithm": "Artificial Bee Colony",
        "config": config,
        "scout_complete": has_scout,
        "worker_complete": has_worker,
        "evaluator_complete": has_eval
    }
