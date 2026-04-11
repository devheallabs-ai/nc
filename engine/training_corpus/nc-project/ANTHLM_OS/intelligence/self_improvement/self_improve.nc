// ══════════════════════════════════════════════════════════════════
//  HiveANT — Self-Improving Swarm Engine
//
//  Continuously improves swarm performance through reinforcement
//  learning. After every investigation cycle, updates pheromone
//  scores, tunes algorithm parameters, and evolves strategies.
// ══════════════════════════════════════════════════════════════════

to improve_from_cycle with incident_id, outcome, metrics:
    purpose: "Update swarm parameters based on investigation outcome"
    shell("mkdir -p knowledge memory/pheromone")

    set perf_path to "knowledge/swarm_performance.json"
    if file_exists(perf_path):
        set perf to json_decode(read_file(perf_path))
    otherwise:
        set perf to {"total_cycles": 0, "success_count": 0, "avg_time_seconds": 0, "avg_confidence": 0, "parameter_history": []}

    set perf.total_cycles to perf.total_cycles + 1
    if outcome is equal "success":
        set perf.success_count to perf.success_count + 1

    set success_rate to 0
    if perf.total_cycles is above 0:
        set success_rate to perf.success_count / perf.total_cycles

    ask AI to "You are the Swarm Self-Improvement Engine. Based on performance data, suggest parameter adjustments. CURRENT PERFORMANCE: Total cycles={{perf.total_cycles}}, Success rate={{success_rate}}, Avg confidence={{perf.avg_confidence}}. LATEST OUTCOME: {{outcome}}. METRICS: {{metrics}}. Suggest tuning for: ACO parameters (alpha, beta, evaporation_rate), ABC parameters (scout_count, worker_count), Agent spawn limits, Task timeout values. Return ONLY valid JSON: {\"adjustments\": [{\"parameter\": \"string\", \"current_value\": 0.0, \"suggested_value\": 0.0, \"reason\": \"string\"}], \"strategy_notes\": \"string\", \"performance_trend\": \"improving|stable|declining\"}" save as improvements

    set perf.latest_adjustments to improvements.adjustments
    set perf.performance_trend to improvements.performance_trend
    write_file(perf_path, json_encode(perf))

    log "SELF-IMPROVE: Performance trend = " + improvements.performance_trend + " (success_rate=" + str(success_rate) + ")"
    respond with {"success_rate": success_rate, "total_cycles": perf.total_cycles, "improvements": improvements}

to get_swarm_performance:
    purpose: "Get swarm performance metrics"
    set perf_path to "knowledge/swarm_performance.json"
    if file_exists(perf_path):
        respond with json_decode(read_file(perf_path))
    otherwise:
        respond with {"total_cycles": 0, "success_count": 0, "message": "No cycles completed yet"}

to auto_tune_parameters:
    purpose: "Automatically tune swarm parameters based on history"
    set perf_path to "knowledge/swarm_performance.json"
    if file_exists(perf_path):
        set perf to json_decode(read_file(perf_path))
        ask AI to "Analyze swarm performance history and auto-tune parameters. PERFORMANCE: {{perf}}. Return ONLY valid JSON: {\"aco_params\": {\"alpha\": 0.0, \"beta\": 0.0, \"evaporation_rate\": 0.0, \"colony_size\": 0}, \"abc_params\": {\"scout_count\": 0, \"worker_count\": 0, \"evaluator_count\": 0}, \"kernel_params\": {\"max_agents_per_task\": 0, \"task_timeout_seconds\": 0, \"max_spawn_depth\": 0}, \"confidence\": 0.0}" save as tuned
        write_file("knowledge/tuned_params.json", json_encode(tuned))
        respond with tuned
    otherwise:
        respond with {"error": "No performance history available"}
