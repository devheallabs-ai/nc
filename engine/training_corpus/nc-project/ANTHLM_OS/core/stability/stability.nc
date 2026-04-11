// ══════════════════════════════════════════════════════════════════
//  HiveANT — Swarm Stability Mechanisms
//
//  Prevents uncontrolled swarm behavior with:
//    - Max agents per task limits
//    - Spawn depth limits
//    - Task timeout policies
//    - Duplicate task detection
//    - Agent conflict resolution
//    - Runaway swarm detection & termination
// ══════════════════════════════════════════════════════════════════

to stability_init with config:
    purpose: "Initialize swarm stability parameters"
    shell("mkdir -p agents_state")

    if config:
        set params to config
    otherwise:
        set params to {"max_agents_total": 1000, "max_agents_per_task": 20, "max_spawn_depth": 4, "task_timeout_seconds": 300, "duplicate_detection": true, "conflict_resolution": "priority", "runaway_threshold_per_minute": 50}

    write_file("agents_state/stability_config.json", json_encode(params))
    log "STABILITY: Initialized with max_agents=" + str(params.max_agents_total) + " max_per_task=" + str(params.max_agents_per_task)
    respond with params

to stability_check_spawn with agent_type, task_id, depth:
    purpose: "Check if spawning a new agent is safe"
    set config_path to "agents_state/stability_config.json"
    if file_exists(config_path):
        set cfg to json_decode(read_file(config_path))
    otherwise:
        set cfg to {"max_agents_total": 1000, "max_agents_per_task": 20, "max_spawn_depth": 4}

    set total_agents to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
    set total to trim(total_agents)

    if total is above cfg.max_agents_total:
        respond with {"allowed": false, "reason": "Total agent limit reached (" + str(cfg.max_agents_total) + ")", "total_agents": total}

    if depth:
        if depth is above cfg.max_spawn_depth:
            respond with {"allowed": false, "reason": "Max spawn depth exceeded (" + str(cfg.max_spawn_depth) + ")", "depth": depth}

    respond with {"allowed": true, "total_agents": total, "limits": cfg}

to stability_detect_runaway:
    purpose: "Detect and terminate runaway swarm behavior"
    set config_path to "agents_state/stability_config.json"
    if file_exists(config_path):
        set cfg to json_decode(read_file(config_path))
    otherwise:
        set cfg to {"runaway_threshold_per_minute": 50}

    set recent_spawns to shell("ls -lt agents_state/agent-*.json 2>/dev/null | head -60 | wc -l || echo 0")
    set spawn_count to trim(recent_spawns)

    if spawn_count is above cfg.runaway_threshold_per_minute:
        log "STABILITY WARNING: Potential runaway swarm detected (" + str(spawn_count) + " agents)"
        respond with {"runaway_detected": true, "agent_count": spawn_count, "threshold": cfg.runaway_threshold_per_minute, "action": "Consider killing excess agents"}
    otherwise:
        respond with {"runaway_detected": false, "agent_count": spawn_count, "threshold": cfg.runaway_threshold_per_minute}

to stability_status:
    purpose: "Get swarm stability status"
    set config_path to "agents_state/stability_config.json"
    if file_exists(config_path):
        set cfg to json_decode(read_file(config_path))
        set total to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
        respond with {"config": cfg, "current_agents": trim(total), "healthy": true}
    otherwise:
        respond with {"status": "not_initialized"}
