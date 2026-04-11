// ══════════════════════════════════════════════════════════════════
//  HiveANT — Swarm OS Kernel: Agent Runtime
//
//  The kernel manages agent lifecycle, scheduling, resource quotas,
//  and inter-agent communication. Agents behave like lightweight
//  OS processes with spawn/kill/suspend/resume semantics.
// ══════════════════════════════════════════════════════════════════

to kernel_init:
    purpose: "Initialize the HiveANT kernel and all subsystems"
    shell("mkdir -p incidents knowledge docs memory/pheromone memory/twins memory/tasks agents_state")

    set kernel_state to {
        "version": "1.0.0",
        "name": "HiveANT",
        "started_at": time_iso(time_now()),
        "agents": {},
        "agent_count": 0,
        "max_agents": 10000,
        "clusters": {},
        "cluster_count": 0,
        "tick": 0,
        "status": "running"
    }
    write_file("agents_state/kernel.json", json_encode(kernel_state))
    log "KERNEL: HiveANT initialized"
    respond with kernel_state

to kernel_status:
    purpose: "Return current kernel state and agent census"
    if file_exists("agents_state/kernel.json"):
        try:
            set state to json_decode(read_file("agents_state/kernel.json"))
            set state.agent_count to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
            set state.uptime_seconds to time_now() - state.started_at
            respond with state
        otherwise:
            respond with {"error": "Corrupt kernel state file"}
    otherwise:
        respond with {"error": "Kernel not initialized. Call POST /kernel/init first."}

to spawn_agent with agent_type, agent_id:
    purpose: "Spawn a new agent process in the swarm"
    if file_exists("agents_state/kernel.json") is not equal true:
        respond with {"error": "Kernel not initialized", "_status": 500}

    try:
        set k_state to json_decode(read_file("agents_state/kernel.json"))
    otherwise:
        respond with {"error": "Corrupt kernel state", "_status": 500}

    if agent_id is not equal "":
        set aid to agent_id
    otherwise:
        set aid to agent_type + "-" + str(floor(random() * 9000 + 1000))

    set agent to {
        "id": aid,
        "type": agent_type,
        "status": "running",
        "spawned_at": time_iso(time_now()),
        "last_heartbeat": time_iso(time_now()),
        "tasks_completed": 0,
        "tasks_failed": 0,
        "memory_usage_kb": 0,
        "cpu_ticks": 0,
        "cluster_id": "default"
    }

    write_file("agents_state/agent-" + aid + ".json", json_encode(agent))
    set k_state.agent_count to k_state.agent_count + 1
    write_file("agents_state/kernel.json", json_encode(k_state))

    log "KERNEL: Spawned agent " + aid + " (type=" + agent_type + ")"
    respond with agent

to kill_agent with agent_id:
    purpose: "Terminate an agent process"
    set path to "agents_state/agent-" + agent_id + ".json"
    if file_exists(path):
        try:
            set agent to json_decode(read_file(path))
            set agent.status to "terminated"
            set agent.terminated_at to time_iso(time_now())
            write_file(path, json_encode(agent))

            set agent_count to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
            try:
                set k_state to json_decode(read_file("agents_state/kernel.json"))
                set k_state.agent_count to agent_count
                write_file("agents_state/kernel.json", json_encode(k_state))
            otherwise:
                log "WARN: Could not update kernel state"

            log "KERNEL: Killed agent " + agent_id
            respond with {"agent_id": agent_id, "status": "terminated"}
        otherwise:
            respond with {"error": "Corrupt agent file", "_status": 500}
    otherwise:
        respond with {"error": "Agent not found", "_status": 404}

to suspend_agent with agent_id:
    purpose: "Suspend an agent (pause execution)"
    set path to "agents_state/agent-" + agent_id + ".json"
    if file_exists(path):
        try:
            set agent to json_decode(read_file(path))
            set agent.status to "suspended"
            set agent.suspended_at to time_iso(time_now())
            write_file(path, json_encode(agent))
            log "KERNEL: Suspended agent " + agent_id
            respond with {"agent_id": agent_id, "status": "suspended"}
        otherwise:
            respond with {"error": "Corrupt agent file", "_status": 500}
    otherwise:
        respond with {"error": "Agent not found", "_status": 404}

to resume_agent with agent_id:
    purpose: "Resume a suspended agent"
    set path to "agents_state/agent-" + agent_id + ".json"
    if file_exists(path):
        try:
            set agent to json_decode(read_file(path))
            set agent.status to "running"
            set agent.resumed_at to time_iso(time_now())
            write_file(path, json_encode(agent))
            log "KERNEL: Resumed agent " + agent_id
            respond with {"agent_id": agent_id, "status": "running"}
        otherwise:
            respond with {"error": "Corrupt agent file", "_status": 500}
    otherwise:
        respond with {"error": "Agent not found", "_status": 404}

to list_agents with status_filter:
    purpose: "List all agents, optionally filtered by status"
    set result to shell("ls agents_state/agent-*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"agents": [], "count": 0}

    set agents to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set data to json_decode(read_file(f))
                if status_filter:
                    if data.status is equal status_filter:
                        set agents to agents + [data]
                otherwise:
                    set agents to agents + [data]
            otherwise:
                log "WARN: Corrupt agent file " + f
                continue

    respond with {"agents": agents, "count": len(agents)}

to agent_heartbeat with agent_id, metrics:
    purpose: "Record agent heartbeat with resource metrics"
    set path to "agents_state/agent-" + agent_id + ".json"
    if file_exists(path):
        try:
            set agent to json_decode(read_file(path))
            set agent.last_heartbeat to time_iso(time_now())
            if metrics:
                set agent.memory_usage_kb to metrics.memory_kb
                set agent.cpu_ticks to metrics.cpu_ticks
            write_file(path, json_encode(agent))
            respond with {"agent_id": agent_id, "heartbeat": "ok"}
        otherwise:
            respond with {"error": "Corrupt agent file", "_status": 500}
    otherwise:
        respond with {"error": "Agent not found", "_status": 404}
