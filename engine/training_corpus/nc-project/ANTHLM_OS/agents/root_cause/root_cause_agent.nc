// ══════════════════════════════════════════════════════════════════
//  HiveANT — Root Cause Agent
//
//  Uses the Ant Colony algorithm to identify causal paths.
//  Queries the pheromone graph for historical patterns.
//  Combines evidence from investigation agents with learned knowledge.
// ══════════════════════════════════════════════════════════════════

to find_root_cause with signals, system_context, service_name:
    purpose: "Run ant colony root cause analysis on collected signals"
    log "ROOT CAUSE AGENT: Analyzing signals for " + str(service_name)

    shell("mkdir -p memory/pheromone knowledge")

    set existing_graph to "none"
    if file_exists("memory/pheromone/graph_db.json"):
        set existing_graph to read_file("memory/pheromone/graph_db.json")

    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")

    set procedural_mem to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural_mem to read_file("knowledge/procedural.json")

    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    set all_incidents to shell("ls incidents/*.json 2>/dev/null | tail -15 | while read f; do cat \"$f\" 2>/dev/null | head -c 300; echo '---'; done || echo NONE")
    set past_raw to all_incidents
    if all_incidents is not equal "NONE":
        set incident_chunks to chunk(all_incidents, 400, 50)
        set past_raw to str(top_k(incident_chunks, 5))

    ask AI to "You are the Root Cause Agent using Ant Colony Optimization.

SIGNALS FROM INVESTIGATION:
{{signals}}

SYSTEM CONTEXT:
{{system_context}}

PHEROMONE GRAPH (historical causal paths):
{{existing_graph}}

COGNITIVE MEMORY:
- Episodic (past incidents): {{past_raw}}
- Semantic (patterns): {{semantic_mem}}
- Procedural (runbooks): {{procedural_mem}}
- RL Policy (what works): {{rl_policy}}

TASK: Simulate ant colony exploration across the causal graph.
1. Deploy ants at each symptom node
2. Ants explore paths toward potential root causes
3. Paths with higher pheromone get more ant traffic (positive feedback)
4. Score each path by evidence strength * pheromone score
5. If a semantic memory pattern matches, boost confidence by 15%
6. If procedural memory has a runbook, recommend it
7. If RL policy shows a strategy worked, note it

Return ONLY valid JSON:
{
  \"root_cause\": \"string\",
  \"confidence_percent\": 0,
  \"category\": \"configuration|deployment|code-bug|infrastructure|dependency|resource-exhaustion|network|security\",
  \"causal_chain\": [
    {\"node\": \"string\", \"type\": \"symptom|intermediate|root_cause\", \"evidence\": \"string\", \"pheromone_score\": 0.0}
  ],
  \"evidence_chain\": [\"string\"],
  \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}],
  \"affected_services\": [\"string\"],
  \"alternative_causes\": [
    {\"cause\": \"string\", \"probability\": 0.0, \"evidence\": \"string\"}
  ],
  \"memory_used\": {
    \"episodic_match\": \"string\",
    \"semantic_match\": \"string\",
    \"procedural_match\": \"string\",
    \"rl_strategy\": \"string\"
  },
  \"pheromone_updates\": [
    {\"from\": \"string\", \"to\": \"string\", \"new_score\": 0.0}
  ],
  \"learning\": \"string\"
}" save as root_cause

    set rc_valid to validate(root_cause, ["root_cause", "confidence_percent"])
    if rc_valid.valid is not equal yes:
        set root_cause to {
            "root_cause": "Unable to determine — analysis returned unexpected format",
            "confidence_percent": 0,
            "category": "unknown",
            "causal_chain": [],
            "evidence_chain": [],
            "affected_services": [service_name]
        }
    otherwise:
        log "ROOT CAUSE AGENT: " + root_cause.root_cause + " (confidence=" + str(root_cause.confidence_percent) + "%)"

    if root_cause.pheromone_updates:
        set db_path to "memory/pheromone/graph_db.json"
        if file_exists(db_path):
            set db to json_decode(read_file(db_path))
            repeat for each update in root_cause.pheromone_updates:
                set eid to str(update.from) + "->" + str(update.to)
                set db.edges[eid] to {
                    "from": update.from,
                    "to": update.to,
                    "pheromone_score": update.new_score,
                    "last_updated": time_now()
                }
            set db.updated_at to time_now()
            write_file(db_path, json_encode(db))
            log "ROOT CAUSE AGENT: Updated " + str(len(root_cause.pheromone_updates)) + " pheromone edges"

    set root_cause.service to service_name
    set root_cause.analyzed_at to time_now()
    respond with root_cause
