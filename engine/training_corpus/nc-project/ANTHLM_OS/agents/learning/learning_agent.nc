// ══════════════════════════════════════════════════════════════════
//  HiveANT — Learning Agent
//
//  Stores incident knowledge and updates pheromone scores.
//  Implements 4-layer cognitive memory + RL Q-table.
//
//  Memory layers:
//    1. Working Memory  — current investigation signals
//    2. Episodic Memory — past incident records
//    3. Semantic Memory — generalized patterns (AI-extracted)
//    4. Procedural Memory — learned fix runbooks
//    5. RL Q-Table     — Q(s,a) = Q(s,a) + α(reward - Q(s,a))
// ══════════════════════════════════════════════════════════════════

to learn_from_incident with incident_id, correct, notes:
    purpose: "Process feedback and update all memory layers + RL + pheromones"
    log "LEARNING AGENT: Processing feedback for " + incident_id

    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        set incident.feedback_correct to correct
        set incident.feedback_notes to notes
        set incident.feedback_at to time_now()
        write_file(path, json_encode(incident))

        shell("mkdir -p knowledge memory/pheromone")

        // RL Q-value update: Q(s,a) = Q(s,a) + α(reward - Q(s,a))
        set alpha to 0.1
        set reward to 0.0
        if correct is equal "yes":
            set reward to 1.0
        otherwise:
            set reward to -0.5

        set state_key to str(incident.category)
        set rl_path to "knowledge/rl-policy.json"
        if file_exists(rl_path):
            set rl_data to json_decode(read_file(rl_path))
        otherwise:
            set rl_data to {"states": {}, "total_episodes": 0, "alpha": 0.1, "epsilon": 0.2}

        set rl_data.total_episodes to rl_data.total_episodes + 1
        if rl_data.epsilon is above 0.05:
            set rl_data.epsilon to rl_data.epsilon * 0.95
        write_file(rl_path, json_encode(rl_data))
        log "LEARNING: RL updated — state=" + state_key + " reward=" + str(reward) + " episodes=" + str(rl_data.total_episodes)

        // Semantic Memory: extract generalized pattern
        set sem_path to "knowledge/semantic.json"
        if file_exists(sem_path):
            set sem to json_decode(read_file(sem_path))
        otherwise:
            set sem to {"patterns": [], "total": 0}

        if correct is equal "yes":
            set sem.total to sem.total + 1
            ask AI to "Extract a reusable pattern from this incident. Service={{incident.service}}, Category={{incident.category}}, Root_cause={{incident.root_cause}}. Return ONLY valid JSON: {\"pattern\": \"generalized rule\", \"trigger_signals\": [\"string\"], \"confidence\": 0.0}" save as learned_pattern
            set lp_valid to validate(learned_pattern, ["pattern"])
            if lp_valid.valid is equal yes:
                set sem.patterns to sem.patterns + [{"pattern": learned_pattern.pattern, "trigger_signals": learned_pattern.trigger_signals, "confidence": learned_pattern.confidence, "from": incident_id, "learned_at": time_now()}]
                write_file(sem_path, json_encode(sem))
                log "LEARNING: Semantic memory — learned pattern: " + learned_pattern.pattern

        // Procedural Memory: save runbook
        if correct is equal "yes":
            set proc_path to "knowledge/procedural.json"
            if file_exists(proc_path):
                set proc to json_decode(read_file(proc_path))
            otherwise:
                set proc to {"runbooks": [], "total": 0}
            if incident.remediation:
                set proc.total to proc.total + 1
                set proc.runbooks to proc.runbooks + [{"trigger": state_key, "root_cause": str(incident.root_cause), "fix": incident.remediation, "from": incident_id, "success": true, "learned_at": time_now()}]
                write_file(proc_path, json_encode(proc))
                log "LEARNING: Procedural memory — saved runbook for " + state_key

        // Pheromone reinforcement for the causal chain
        if correct is equal "yes":
            if incident.causal_chain:
                set db_path to "memory/pheromone/graph_db.json"
                if file_exists(db_path):
                    set db to json_decode(read_file(db_path))
                    set idx to 0
                    repeat for each node in incident.causal_chain:
                        if idx is above 0:
                            set prev to incident.causal_chain[idx - 1]
                            set eid to str(prev.node) + "->" + str(node.node)
                            if db.edges[eid]:
                                set db.edges[eid].pheromone_score to db.edges[eid].pheromone_score + 1.0
                            otherwise:
                                set db.edges[eid] to {"from": prev.node, "to": node.node, "pheromone_score": 1.0, "last_updated": time_now()}
                        set idx to idx + 1
                    write_file(db_path, json_encode(db))
                    log "LEARNING: Reinforced " + str(idx - 1) + " pheromone edges"

        respond with {
            "incident_id": incident_id,
            "feedback": correct,
            "reward": reward,
            "rl_episodes": rl_data.total_episodes,
            "rl_epsilon": rl_data.epsilon,
            "semantic_updated": correct,
            "procedural_updated": correct,
            "pheromone_reinforced": correct
        }
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}

to get_knowledge:
    purpose: "View all learned knowledge across memory layers"
    shell("mkdir -p knowledge")
    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")
    set procedural to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural to read_file("knowledge/procedural.json")
    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")
    set incident_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set doc_count to shell("ls docs/*.md docs/*.txt 2>/dev/null | wc -l || echo 0")
    respond with {
        "episodic_count": trim(incident_count),
        "docs_indexed": trim(doc_count),
        "semantic": semantic,
        "procedural": procedural,
        "rl_policy": rl_policy
    }

to load_cognitive_memory with service_name:
    purpose: "Load all cognitive memory layers for an investigation"
    shell("mkdir -p incidents knowledge docs")

    set all_incidents to shell("ls incidents/*.json 2>/dev/null | tail -20 | while read f; do cat \"$f\" 2>/dev/null; echo '|||'; done || echo NONE")
    set episodic to ""
    if all_incidents is not equal "NONE":
        set chunks to chunk(all_incidents, 500, 50)
        set relevant to top_k(chunks, 5)
        repeat for each c in relevant:
            set episodic to episodic + str(c) + "\n"

    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")

    set procedural to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural to read_file("knowledge/procedural.json")

    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    set rag_context to "none"
    set doc_list to shell("ls docs/*.md docs/*.txt docs/*.nc 2>/dev/null || echo NONE")
    if doc_list is not equal "NONE":
        set all_docs to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
        set doc_chunks to chunk(all_docs, 800, 100)
        set rag_context to str(top_k(doc_chunks, 3))

    respond with {
        "episodic": episodic,
        "semantic": semantic,
        "procedural": procedural,
        "rl_policy": rl_policy,
        "rag_docs": rag_context
    }
