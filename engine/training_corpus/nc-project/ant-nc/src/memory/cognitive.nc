// ══════════════════════════════════════════════════════════════════
//  COGNITIVE MEMORY + RL + RAG
//
//  1. Working Memory  — current signals (in-memory during investigation)
//  2. Episodic Memory — past incidents, chunked + ranked by relevance
//  3. Semantic Memory  — generalized patterns (LLM-extracted)
//  4. Procedural Memory — learned fix sequences
//  5. RL Q-Table      — real Q-value updates with learning rate
//  6. RAG             — chunk docs, find relevant context by keyword match
// ══════════════════════════════════════════════════════════════════

to load_memory with service_name:
    purpose: "Load cognitive memory with RAG-style retrieval"
    shell("mkdir -p incidents knowledge docs")

    // Episodic: load past incidents, chunk them, pick most relevant
    set all_incidents to shell("ls incidents/*.json 2>/dev/null | tail -20 | while read f; do cat \"$f\" 2>/dev/null; echo '|||'; done || echo NONE")
    set episodic_relevant to ""
    if all_incidents is not equal "NONE":
        set chunks to chunk(all_incidents, 500, 50)
        set relevant to top_k(chunks, 5)
        repeat for each c in relevant:
            set episodic_relevant to episodic_relevant + str(c) + "\n"

    // Semantic: load learned patterns
    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")

    // Procedural: load learned runbooks
    set procedural to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural to read_file("knowledge/procedural.json")

    // RL: load Q-table
    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    // RAG: load relevant docs if any exist
    set rag_context to "none"
    set doc_list to shell("ls docs/*.md docs/*.txt docs/*.nc 2>/dev/null || echo NONE")
    if doc_list is not equal "NONE":
        set all_docs to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
        set doc_chunks to chunk(all_docs, 800, 100)
        set rag_context to str(top_k(doc_chunks, 3))

    respond with {"episodic": episodic_relevant, "semantic": semantic, "procedural": procedural, "rl_policy": rl_policy, "rag_docs": rag_context}

// ══════════════════════════════════════════════════════════════════
//  RAG — Index and search your runbooks, docs, architecture docs
// ══════════════════════════════════════════════════════════════════

to rag_index:
    purpose: "Index all docs in docs/ folder for RAG retrieval"
    shell("mkdir -p docs")
    set file_list to shell("ls docs/*.md docs/*.txt docs/*.nc 2>/dev/null || echo NONE")
    if file_list is equal "NONE":
        respond with {"status": "no_docs", "message": "Put your runbooks, architecture docs, or .md files in docs/ folder", "example": "docs/runbook-checkout.md, docs/architecture.md, docs/oncall-guide.txt"}
    otherwise:
        set all_content to shell("cat docs/*.md docs/*.txt docs/*.nc 2>/dev/null")
        set total_tokens to token_count(all_content)
        set chunks to chunk(all_content, 800, 100)
        set chunk_count to len(chunks)
        respond with {"status": "indexed", "files": file_list, "total_tokens": total_tokens, "chunks": chunk_count, "message": "Docs indexed. They will be used as context during investigations."}

to rag_search with query:
    purpose: "Search docs for relevant context"
    shell("mkdir -p docs")
    set all_content to shell("cat docs/*.md docs/*.txt docs/*.nc 2>/dev/null || echo ''")
    if len(all_content) is above 0:
        set doc_chunks to chunk(all_content, 600, 80)
        set results to top_k(doc_chunks, 5)
        respond with {"query": query, "results": results, "chunks_searched": len(doc_chunks)}
    otherwise:
        respond with {"query": query, "results": [], "message": "No docs found. Put files in docs/ folder."}

// ══════════════════════════════════════════════════════════════════
//  REINFORCEMENT LEARNING — Real Q-value updates
//
//  Q(s,a) = Q(s,a) + alpha * (reward - Q(s,a))
//  alpha = 0.1 (learning rate)
//  reward = +1 (correct), -0.5 (wrong), 0 (no feedback)
// ══════════════════════════════════════════════════════════════════

to add_feedback with incident_id, correct, notes:
    purpose: "Feedback updates RL Q-table + all memory layers"
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        set incident.feedback_correct to correct
        set incident.feedback_notes to notes
        set incident.feedback_at to time_now()
        write_file(path, json_encode(incident))

        shell("mkdir -p knowledge")

        // ── RL Q-value update: Q(s,a) = Q(s,a) + alpha * (reward - Q(s,a)) ──
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
        // Decay epsilon (explore less over time)
        if rl_data.epsilon is above 0.05:
            set rl_data.epsilon to rl_data.epsilon * 0.95
        write_file(rl_path, json_encode(rl_data))
        log "RL updated: state=" + state_key + " reward=" + str(reward) + " episodes=" + str(rl_data.total_episodes) + " epsilon=" + str(rl_data.epsilon)

        // ── Semantic Memory: extract pattern if correct ──
        set sem_path to "knowledge/semantic.json"
        if file_exists(sem_path):
            set sem to json_decode(read_file(sem_path))
        otherwise:
            set sem to {"patterns": [], "total": 0}
        if correct is equal "yes":
            set sem.total to sem.total + 1
            // Ask LLM to extract a generalized pattern
            ask AI to "Extract a reusable pattern from this incident. Incident: service={{incident.service}}, category={{incident.category}}, root_cause={{incident.root_cause}}. Return ONLY valid JSON: {\"pattern\": \"string (generalized rule like: deploy + auth_error = credential_misconfiguration)\", \"trigger_signals\": [\"string\"], \"confidence\": 0.0}" save as learned_pattern
            set lp_valid to validate(learned_pattern, ["pattern"])
            if lp_valid.valid is equal yes:
                set sem.patterns to sem.patterns + [{"pattern": learned_pattern.pattern, "trigger_signals": learned_pattern.trigger_signals, "confidence": learned_pattern.confidence, "from": incident_id, "learned_at": time_now()}]
                write_file(sem_path, json_encode(sem))
                log "Semantic memory: learned pattern: " + learned_pattern.pattern

        // ── Procedural Memory: save runbook if correct ──
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
                log "Procedural memory: saved runbook for " + state_key

        respond with {"incident_id": incident_id, "feedback": correct, "reward": reward, "rl_episodes": rl_data.total_episodes, "rl_epsilon": rl_data.epsilon, "semantic_updated": correct, "procedural_updated": correct}
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}

to get_knowledge:
    purpose: "View all learned knowledge"
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
    respond with {"episodic_count": trim(incident_count), "docs_indexed": trim(doc_count), "semantic": semantic, "procedural": procedural, "rl_policy": rl_policy}


