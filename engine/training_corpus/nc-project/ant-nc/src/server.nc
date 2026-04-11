// ══════════════════════════════════════════════════════════════════
//  SwarmOps v6.0 — AI Incident Copilot
//
//  Modules:
//    agents/data-pull.nc    — Prometheus, Elasticsearch, GitHub connectors
//    agents/investigate.nc  — AI investigation agents
//    agents/self-heal.nc    — Auto-fix + rollback + watchdog
//    agents/analyze.nc      — Quick analyze, manual investigate, config
//    agents/neural.nc       — ML model classification + chat
//    agents/sample.nc       — Sample incident data for demos
//    k8s/monitor.nc         — Kubernetes pod/log/event monitoring + fixes
//    memory/cognitive.nc    — RL, episodic/semantic/procedural memory, RAG
//    mcp/tools.nc           — MCP tool server + external MCP client
//
//  Start:  NC_ALLOW_EXEC=1 nc serve src/server.nc
//  Build:  nc build src/server.nc -o bin/swarmops
// ══════════════════════════════════════════════════════════════════

service "swarmops"
version "6.0.0"

configure:
    ai_model is "env:NC_AI_MODEL"
    ai_url is "env:NC_AI_URL"
    ai_key is "env:NC_AI_KEY"
    ai_system_prompt is "You are SwarmOps, an expert SRE investigating production incidents. You analyze real telemetry data. Always return ONLY valid JSON."
    port: 9090

// ── All modules inlined below (no separate imports needed) ─────
// SwarmOps — Data Pull Module
// Connects to Prometheus, Elasticsearch, GitHub to pull real telemetry data

to pull_health with target_url:
    purpose: "Check a real service health endpoint"
    gather result from target_url
    respond with result

to pull_prometheus with query:
    purpose: "Query real Prometheus metrics"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set full_url to prom_url + "/api/v1/query?query=" + query
        gather result from full_url
        respond with {"source": "prometheus", "query": query, "data": result}
    otherwise:
        respond with {"source": "prometheus", "error": "PROMETHEUS_URL not configured", "query": query}

to pull_prometheus_range with query, duration:
    purpose: "Query Prometheus range for trending"
    set prom_url to env("PROMETHEUS_URL")
    if duration:
        set range_seconds to duration
    otherwise:
        set range_seconds to 3600
    if prom_url:
        set full_url to prom_url + "/api/v1/query_range?query=" + query + "&start=" + str(time_now() - range_seconds) + "&end=" + str(time_now()) + "&step=60"
        gather result from full_url
        respond with {"source": "prometheus_range", "query": query, "data": result}
    otherwise:
        respond with {"source": "prometheus_range", "error": "PROMETHEUS_URL not configured"}

to pull_logs with search_query:
    purpose: "Search real Elasticsearch/OpenSearch logs"
    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set search_url to es_url + "/logs-*/_search?q=" + search_query + "&size=50&sort=@timestamp:desc"
        gather result from search_url
        respond with {"source": "elasticsearch", "query": search_query, "data": result}
    otherwise:
        respond with {"source": "elasticsearch", "error": "ELASTICSEARCH_URL not configured"}

to pull_deploys:
    purpose: "Get recent deploys from GitHub"
    set gh_token to env("GITHUB_TOKEN")
    set gh_repo to env("GITHUB_REPO")
    if gh_token:
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/deployments?per_page=10"
        gather result from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        set commits_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=10&since=" + time_format(time_now() - 86400, "%Y-%m-%dT%H:%M:%SZ")
        gather commits from commits_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        respond with {"source": "github", "deployments": result, "recent_commits": commits}
    otherwise:
        respond with {"source": "github", "error": "GITHUB_TOKEN not configured"}

to monitor_check:
    purpose: "Check all configured health endpoints"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set results to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do status=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); latency=$(curl -s -o /dev/null -w '%{time_total}' --connect-timeout 5 $url 2>/dev/null); echo \"{\\\"url\\\":\\\"$url\\\",\\\"status\\\":$status,\\\"latency_ms\\\":$(echo \"$latency * 1000\" | bc 2>/dev/null || echo 0)}\"; done")
        respond with {"source": "health_check", "results": results, "checked_at": time_now()}
    otherwise:
        respond with {"source": "health_check", "error": "MONITOR_URLS not configured"}
// ══════════════════════════════════════════════════════════════════
//  KUBERNETES — Monitor all pods, pull logs, fix issues
//  Works when SwarmOps runs INSIDE K8s with the ServiceAccount
// ══════════════════════════════════════════════════════════════════

to k8s_pods with namespace:
    purpose: "List all pods in a namespace or all namespaces"
    if namespace:
        set result to shell("kubectl get pods -n " + namespace + " -o json 2>&1")
    otherwise:
        set result to shell("kubectl get pods --all-namespaces -o json 2>&1")
    respond with result

to k8s_unhealthy:
    purpose: "Find all unhealthy pods across the cluster"
    set crashloop to shell("kubectl get pods --all-namespaces --field-selector=status.phase!=Running -o wide 2>&1 || echo 'kubectl not available'")
    set restarts to shell("kubectl get pods --all-namespaces -o json 2>&1 | grep -c '\"restartCount\"' || echo 0")
    set not_ready to shell("kubectl get pods --all-namespaces | grep -v Running | grep -v Completed | grep -v NAME 2>&1 || echo 'all healthy'")
    respond with {"unhealthy_pods": not_ready, "crashloop_info": crashloop, "checked_at": time_now()}

to k8s_logs with pod_name, namespace, lines:
    purpose: "Get logs from a specific pod"
    if namespace:
        set result to shell("kubectl logs " + pod_name + " -n " + namespace + " --tail=" + str(lines) + " 2>&1")
    otherwise:
        set result to shell("kubectl logs " + pod_name + " --tail=" + str(lines) + " 2>&1")
    respond with {"pod": pod_name, "namespace": namespace, "logs": result}

to k8s_events with namespace:
    purpose: "Get recent K8s events (warnings, errors)"
    if namespace:
        set result to shell("kubectl get events -n " + namespace + " --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -30")
    otherwise:
        set result to shell("kubectl get events --all-namespaces --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -30")
    respond with {"events": result, "checked_at": time_now()}

to k8s_describe with resource, name, namespace:
    purpose: "Describe a K8s resource (pod, deployment, service)"
    if namespace:
        set result to shell("kubectl describe " + resource + " " + name + " -n " + namespace + " 2>&1")
    otherwise:
        set result to shell("kubectl describe " + resource + " " + name + " 2>&1")
    respond with {"resource": resource, "name": name, "description": result}

to k8s_fix with action, target, namespace, secret:
    purpose: "Fix K8s issues: restart, rollback, scale"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}

    if action is equal "restart":
        set result to shell("kubectl rollout restart deployment/" + target + " -n " + namespace + " 2>&1")
        respond with {"action": "restart", "target": target, "result": result}
    otherwise if action is equal "rollback":
        set result to shell("kubectl rollout undo deployment/" + target + " -n " + namespace + " 2>&1")
        respond with {"action": "rollback", "target": target, "result": result}
    otherwise if action is equal "scale":
        set result to shell("kubectl scale deployment/" + target + " --replicas=3 -n " + namespace + " 2>&1")
        respond with {"action": "scale", "target": target, "result": result}
    otherwise if action is equal "delete-pod":
        set result to shell("kubectl delete pod " + target + " -n " + namespace + " 2>&1")
        respond with {"action": "delete-pod", "target": target, "result": result}
    otherwise:
        respond with {"error": "Unknown action. Use: restart, rollback, scale, delete-pod"}

to k8s_investigate with namespace, description:
    purpose: "Full K8s investigation — pull pods, logs, events, then AI analyzes"
    log "K8S INVESTIGATION: namespace={{namespace}} — {{description}}"

    set pods to shell("kubectl get pods -n " + namespace + " -o wide 2>&1")
    set events to shell("kubectl get events -n " + namespace + " --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -20")
    set error_logs to shell("kubectl logs -l app --all-containers --tail=50 -n " + namespace + " 2>&1 | grep -i -E 'error|fatal|exception|fail|timeout|refused' | tail -30")
    set deployments to shell("kubectl get deployments -n " + namespace + " -o wide 2>&1")
    set top_pods to shell("kubectl top pods -n " + namespace + " 2>&1 || echo 'metrics-server not available'")

    set k8s_context to "KUBERNETES INVESTIGATION. Namespace: {{namespace}}. Issue: {{description}}. PODS: {{pods}}. EVENTS: {{events}}. ERROR LOGS: {{error_logs}}. DEPLOYMENTS: {{deployments}}. RESOURCE USAGE: {{top_pods}}."

    // Load memory for better diagnosis
    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")
    set procedural_mem to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural_mem to read_file("knowledge/procedural.json")

    ask AI to "You are SwarmOps K8s Agent. Analyze this Kubernetes cluster data to find the root cause. Check: pod statuses, crash loops, OOMKilled, image pull errors, readiness probe failures, recent events, error logs, resource pressure. Past patterns: {{semantic_mem}}. Known fixes: {{procedural_mem}}. Data: {{k8s_context}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"affected_pods\": [\"string\"], \"evidence\": [\"string\"], \"fix_commands\": [{\"command\": \"string\", \"description\": \"string\", \"risk\": \"low|medium|high\"}], \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}]}" save as diagnosis

    set incident_id to "K8S-" + str(floor(random() * 9000 + 1000))
    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(diagnosis))

    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set slack_msg to "{\"text\":\"*SwarmOps K8s Alert — " + incident_id + "*\\n*Namespace:* " + namespace + "\\n*Root Cause:* " + str(diagnosis.root_cause) + "\\n*Confidence:* " + str(diagnosis.confidence_percent) + "%\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg

    respond with {"incident_id": incident_id, "namespace": namespace, "diagnosis": diagnosis}
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


// ══════════════════════════════════════════════════════════════════
//  NEURAL NETWORK — Train or load a model for incident classification
//
//  NC supports: sklearn (.pkl), PyTorch (.pt), TensorFlow (.h5), ONNX (.onnx)
//  Requires: Python installed + NC_ALLOW_PICKLE=1 for .pkl models
//
//  Train a classifier externally, put it in models/, use it in SwarmOps:
//    models/incident-classifier.pkl   — classifies: config|deploy|infra|code-bug
//    models/severity-predictor.onnx   — predicts: critical|high|medium|low
// ══════════════════════════════════════════════════════════════════

to load_nn with model_path:
    purpose: "Load a neural network / ML model for incident classification"
    set nn to load_model(model_path)
    respond with nn

to predict_incident with model_path, features:
    purpose: "Use loaded ML model to classify an incident"
    set nn to load_model(model_path)
    set result to predict(nn, features)
    respond with {"model": model_path, "prediction": result, "features": features}

to nn_classify with error_rate, latency_ms, cpu_percent, memory_percent, recent_deploy:
    purpose: "Classify incident using ML model if available, otherwise use heuristics"
    set features to [error_rate, latency_ms, cpu_percent, memory_percent, recent_deploy]
    if file_exists("models/incident-classifier.pkl"):
        set nn to load_model("models/incident-classifier.pkl")
        set prediction to predict(nn, features)
        respond with {"method": "neural_network", "model": "incident-classifier.pkl", "prediction": prediction, "features": features}
    otherwise:
        // Heuristic fallback when no model is available
        set category to "unknown"
        if cpu_percent is above 90:
            set category to "resource-exhaustion"
        otherwise if latency_ms is above 5000:
            set category to "infrastructure"
        otherwise if error_rate is above 50:
            set category to "code-bug"
        otherwise if recent_deploy is above 0:
            set category to "deployment"
        respond with {"method": "heuristic", "prediction": category, "features": features, "note": "Put a trained model at models/incident-classifier.pkl for ML classification"}

// ══════════════════════════════════════════════════════════════════
//  CHAT — Conversational interface to talk with SwarmOps
//  Uses NC's built-in memory_new / memory_add for multi-turn context
// ══════════════════════════════════════════════════════════════════

to chat with message, session_id:
    purpose: "Chat with SwarmOps about your services, incidents, and infrastructure"

    // Load conversation memory (30 turns max)
    set mem to memory_new(30)
    memory_add(mem, "user", message)

    // Load current system context
    set incident_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set knowledge_summary to "none"
    if file_exists("knowledge/semantic.json"):
        set knowledge_summary to read_file("knowledge/semantic.json")

    set system_context to "You are SwarmOps, an AI SRE assistant. You help engineers debug production incidents. You have access to: " + trim(incident_count) + " past incidents and knowledge patterns: " + str(knowledge_summary) + ". You can investigate incidents, check service health, explain past incidents, and suggest improvements. Be concise and technical."

    set history to memory_summary(mem)
    ask AI to "{{system_context}}. Conversation: {{history}}. User says: {{message}}. Respond helpfully. If the user describes an incident, offer to investigate. If they ask about past incidents, summarize what you know." save as reply

    memory_add(mem, "assistant", str(reply))

    respond with {"reply": reply, "session": session_id, "context": {"incidents": trim(incident_count), "has_knowledge": knowledge_summary}}

to chat_about_incident with incident_id:
    purpose: "Chat about a specific past incident"
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        ask AI to "Explain this incident to an engineer in a conversational way. Include what happened, why, how it was fixed, and how to prevent it. Incident data: {{data}}" save as explanation
        respond with {"incident_id": incident_id, "explanation": explanation, "data": incident}
    otherwise:
        respond with {"error": "Incident not found"}

to chat_suggest with service_name:
    purpose: "Get proactive suggestions for a service based on past patterns"
    set past to shell("grep -l '\"" + service_name + "\"' incidents/*.json 2>/dev/null | wc -l || echo 0")
    set knowledge to "none"
    if file_exists("knowledge/semantic.json"):
        set knowledge to read_file("knowledge/semantic.json")

    ask AI to "Based on incident history for service {{service_name}} ({{past}} past incidents) and learned patterns: {{knowledge}}, suggest proactive improvements to prevent future incidents. Return JSON: {\"service\": \"string\", \"risk_level\": \"high|medium|low\", \"suggestions\": [{\"area\": \"string\", \"suggestion\": \"string\", \"priority\": \"string\"}], \"recurring_patterns\": [\"string\"]}" save as suggestions
    respond with suggestions

// ══════════════════════════════════════════════════════════════════
//  INVESTIGATE — The core: gather real data, AI analyzes it
//  With: MEMORY + RL + RAG + optional Neural Network
// ══════════════════════════════════════════════════════════════════

to investigate with service_name, description:
    purpose: "Full automated investigation with learning from past incidents"
    log "INVESTIGATION STARTED: {{service_name}} — {{description}}"

    // Load cognitive memory — 4 layers + RAG
    shell("mkdir -p incidents knowledge docs")

    // Episodic: chunk and retrieve relevant past incidents
    set all_incidents to shell("ls incidents/*.json 2>/dev/null | tail -15 | while read f; do cat \"$f\" 2>/dev/null | head -c 300; echo '---'; done || echo NONE")
    set past_raw to all_incidents
    if all_incidents is not equal "NONE":
        set incident_chunks to chunk(all_incidents, 400, 50)
        set past_raw to str(top_k(incident_chunks, 5))

    // Semantic + Procedural + RL
    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")
    set procedural_mem to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural_mem to read_file("knowledge/procedural.json")
    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    // RAG: load relevant docs (runbooks, architecture docs)
    set rag_context to "none"
    set all_docs to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
    if len(all_docs) is above 10:
        set doc_chunks to chunk(all_docs, 600, 80)
        set rag_context to str(top_k(doc_chunks, 3))
        log "RAG: loaded " + str(len(doc_chunks)) + " doc chunks"

    set knowledge_raw to "SEMANTIC MEMORY: " + str(semantic_mem) + ". PROCEDURAL MEMORY: " + str(procedural_mem) + ". RL POLICY: " + str(rl_policy) + ". RELEVANT DOCS (RAG): " + str(rag_context)
    log "Loaded: episodic + semantic + procedural + RL + RAG"

    set prom_url to env("PROMETHEUS_URL")
    set es_url to env("ELASTICSEARCH_URL")
    set gh_token to env("GITHUB_TOKEN")

    set metrics_data to "No Prometheus configured"
    set log_data to "No Elasticsearch configured"
    set deploy_data to "No GitHub configured"
    set health_data to "No health endpoints configured"

    if prom_url:
        set error_query to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + service_name + "\"}[5m])"
        gather prom_errors from error_query
        set latency_query to prom_url + "/api/v1/query?query=histogram_quantile(0.99,rate(http_request_duration_seconds_bucket{service=\"" + service_name + "\"}[5m]))"
        gather prom_latency from latency_query
        set cpu_query to prom_url + "/api/v1/query?query=rate(container_cpu_usage_seconds_total{pod=~\"" + service_name + ".*\"}[5m])"
        gather prom_cpu from cpu_query
        set metrics_data to json_encode({"error_rate": prom_errors, "latency_p99": prom_latency, "cpu": prom_cpu})
        log "Pulled Prometheus metrics for {{service_name}}"

    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + service_name + "+AND+level:ERROR&size=30&sort=@timestamp:desc"
        gather es_logs from log_url
        set log_data to json_encode(es_logs)
        log "Pulled Elasticsearch logs for {{service_name}}"

    if gh_token:
        set gh_repo to env("GITHUB_REPO")
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=10"
        gather gh_commits from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        set deploy_data to json_encode(gh_commits)
        log "Pulled GitHub deploy history"

    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")
        log "Checked health endpoints"

    set full_context to "SERVICE: {{service_name}}. ISSUE: {{description}}. METRICS FROM PROMETHEUS: {{metrics_data}}. LOGS FROM ELASTICSEARCH: {{log_data}}. DEPLOYS FROM GITHUB: {{deploy_data}}. HEALTH CHECKS: {{health_data}}."

    ask AI to "You are the Log and Metrics Analysis Agent. Analyze the real telemetry data from this production incident. Identify error patterns in logs, metric anomalies (latency spikes, error rate increases, CPU/memory pressure). Context: {{full_context}}. Return ONLY valid JSON: {\"agent\": \"telemetry-analysis\", \"signals\": [{\"name\": \"string\", \"severity\": \"critical|high|medium|low\", \"evidence\": \"string (quote specific values)\"}], \"error_pattern\": \"string\", \"metric_anomalies\": [{\"metric\": \"string\", \"value\": \"string\", \"normal\": \"string\"}], \"summary\": \"string\"}" save as telemetry_agent

    set tel_valid to validate(telemetry_agent, ["signals", "summary"])
    if tel_valid.valid is not equal yes:
        log "Telemetry Agent returned invalid response"
        set telemetry_agent to {"agent": "telemetry-analysis", "signals": [], "error_pattern": "AI response invalid", "metric_anomalies": [], "summary": "Analysis failed — AI returned unexpected format"}
    otherwise:
        log "Telemetry Agent: " + telemetry_agent.summary

    ask AI to "You are the Deployment and Dependency Agent. Check if recent code changes or deployments correlate with the incident. Also determine if failures cascade between services. Context: {{full_context}}. Return ONLY valid JSON: {\"agent\": \"deploy-dependency\", \"signals\": [{\"name\": \"string\", \"severity\": \"critical|high|medium|low\", \"evidence\": \"string\"}], \"suspicious_changes\": [{\"what\": \"string\", \"when\": \"string\", \"correlation\": \"string\"}], \"dependency_chain\": [{\"from\": \"string\", \"to\": \"string\", \"impact\": \"string\"}], \"is_deploy_related\": true, \"summary\": \"string\"}" save as deploy_dep_agent

    set dep_valid to validate(deploy_dep_agent, ["signals", "summary"])
    if dep_valid.valid is not equal yes:
        log "Deploy/Dep Agent returned invalid response"
        set deploy_dep_agent to {"agent": "deploy-dependency", "signals": [], "suspicious_changes": [], "dependency_chain": [], "is_deploy_related": false, "summary": "Analysis failed — AI returned unexpected format"}
    otherwise:
        log "Deploy/Dep Agent: " + deploy_dep_agent.summary

    set all_evidence to json_encode({"telemetry": telemetry_agent, "deploy_dependency": deploy_dep_agent})

    ask AI to "You are the Hypothesis Agent with COGNITIVE MEMORY and REINFORCEMENT LEARNING. You have 4 memory layers: EPISODIC (past incidents): {{past_raw}}. SEMANTIC (generalized patterns): {{semantic_mem}}. PROCEDURAL (learned fix sequences): {{procedural_mem}}. RL POLICY (what strategies work): {{rl_policy}}. CURRENT EVIDENCE: {{all_evidence}}. RULES: 1) If this matches a pattern in semantic memory, boost confidence by 15%. 2) If procedural memory has a runbook for this pattern, recommend that fix first. 3) If RL policy shows a strategy worked for this category, note it. 4) If this is a NEW pattern not in any memory, flag it for learning. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"configuration|deployment|code-bug|infrastructure|dependency|resource-exhaustion|network|security\", \"evidence_chain\": [\"string\"], \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}], \"affected_services\": [\"string\"], \"memory_used\": {\"episodic_match\": \"string (past incident ID or 'none')\", \"semantic_match\": \"string (pattern matched or 'new')\", \"procedural_match\": \"string (runbook used or 'none')\", \"rl_strategy\": \"string (strategy from RL or 'explore')\"}, \"learning\": \"string (what to add to semantic memory from this incident)\"}" save as hypothesis

    set hyp_valid to validate(hypothesis, ["root_cause", "confidence_percent"])
    if hyp_valid.valid is not equal yes:
        set hypothesis to {"root_cause": "Unable to determine — AI response invalid", "confidence_percent": 0, "category": "unknown", "evidence_chain": [], "timeline": [], "affected_services": [service_name]}
    otherwise:
        log "Root Cause: " + hypothesis.root_cause + " (" + str(hypothesis.confidence_percent) + "%)"

    set diagnosis to json_encode(hypothesis)
    ask AI to "You are the Remediation Agent. Provide specific fix actions with exact CLI commands. The engineer should be able to copy-paste these commands to fix the issue. Diagnosis: {{diagnosis}}. Return ONLY valid JSON: {\"immediate_actions\": [{\"step\": 0, \"action\": \"string\", \"command\": \"string\", \"risk\": \"low|medium|high\"}], \"verification\": [\"string\"], \"prevention\": [\"string\"], \"estimated_recovery_minutes\": 0, \"rollback\": \"string\"}" save as remediation

    set rem_valid to validate(remediation, ["immediate_actions"])
    if rem_valid.valid is not equal yes:
        set remediation to {"immediate_actions": [{"step": 1, "action": "Manual investigation required — AI could not generate fix commands", "command": "kubectl get pods -A | grep -v Running", "risk": "low"}], "verification": ["Check service health endpoint"], "prevention": ["Add monitoring alerts"], "estimated_recovery_minutes": 30, "rollback": "No automated rollback available"}

    set incident_id to "INC-" + str(floor(random() * 9000 + 1000))
    set signal_count to len(telemetry_agent.signals) + len(deploy_dep_agent.signals)

    set report to {"incident_id": incident_id, "service": service_name, "description": description, "root_cause": hypothesis.root_cause, "confidence": hypothesis.confidence_percent, "category": hypothesis.category, "affected_services": hypothesis.affected_services, "evidence_chain": hypothesis.evidence_chain, "timeline": hypothesis.timeline, "agents": {"telemetry": telemetry_agent, "deploy_dependency": deploy_dep_agent}, "remediation": remediation, "signals_total": signal_count, "data_sources": {"prometheus": prom_url, "elasticsearch": es_url, "github": gh_token}, "investigated_at": time_now()}

    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(report))
    log "Saved: incidents/" + incident_id + ".json"

    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set slack_msg to "{\"text\":\"*SwarmOps Incident Report — " + incident_id + "*\\n*Service:* " + service_name + "\\n*Root Cause:* " + hypothesis.root_cause + "\\n*Confidence:* " + str(hypothesis.confidence_percent) + "%\\n*Category:* " + hypothesis.category + "\\n*Fix:* " + remediation.immediate_actions[0].action + "\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg
        log "Posted to Slack"

    respond with report

// ══════════════════════════════════════════════════════════════════
//  SELF-HEAL — Detect → Diagnose → Fix → Verify → Rollback
// ══════════════════════════════════════════════════════════════════

to self_heal with service_name, description, auto_execute, secret:
    purpose: "Full self-healing loop: detect, diagnose, fix, verify"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized. Pass secret parameter matching HEAL_SECRET env var.", "_status": 401}

    // Cooldown: prevent re-heal within 60 seconds
    if file_exists(".last_heal"):
        set last_heal_time to read_file(".last_heal")

    write_file(".last_heal", str(time_now()))
    log "SELF-HEAL STARTED: {{service_name}}"

    set heal_log to "=== Self-Heal Report ===\nService: " + service_name + "\nStarted: " + str(time_now()) + "\n\n"

    // Step 1: Gather current state
    set heal_log to heal_log + "STEP 1: Gathering system state...\n"
    set health_data to "unknown"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    set metrics_data to "not configured"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set error_q to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + service_name + "\"}[5m])"
        gather metrics_data from error_q

    set log_data to "not configured"
    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + service_name + "+AND+level:ERROR&size=20&sort=@timestamp:desc"
        gather log_data from log_url

    set deploy_data to "not configured"
    set gh_token to env("GITHUB_TOKEN")
    if gh_token:
        set gh_repo to env("GITHUB_REPO")
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=5"
        gather deploy_data from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token}

    set heal_log to heal_log + "  Health: " + str(health_data) + "\n  Metrics: collected\n  Logs: collected\n  Deploys: collected\n\n"

    // Step 2: AI Diagnosis
    set heal_log to heal_log + "STEP 2: AI diagnosing root cause...\n"
    set full_context to "SERVICE: {{service_name}}. ISSUE: {{description}}. HEALTH: " + str(health_data) + ". METRICS: " + str(metrics_data) + ". LOGS: " + str(log_data) + ". DEPLOYS: " + str(deploy_data) + "."

    ask AI to "You are SwarmOps Self-Healing Agent. Diagnose this incident AND provide exact fix commands that can be executed automatically. IMPORTANT: Commands must be safe, idempotent, and reversible. Context: {{full_context}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"fix_commands\": [{\"command\": \"string (exact shell command)\", \"description\": \"string\", \"risk\": \"low|medium|high\", \"reversible\": true}], \"verify_command\": \"string (command to check if fix worked)\", \"rollback_commands\": [{\"command\": \"string\", \"description\": \"string\"}], \"safe_to_auto_execute\": true}" save as diagnosis

    set heal_log to heal_log + "  Root cause: " + diagnosis.root_cause + "\n  Confidence: " + str(diagnosis.confidence_percent) + "%\n  Category: " + diagnosis.category + "\n\n"

    // Step 3: Execute fix (only if auto_execute is "yes" and confidence > 70)
    set executed to "no"
    set exec_results to []
    if auto_execute is equal "yes":
        if diagnosis.confidence_percent is above 70:
            if diagnosis.safe_to_auto_execute:
                set heal_log to heal_log + "STEP 3: AUTO-EXECUTING FIX (confidence > 70%, safe=true)...\n"
                repeat for each fix_cmd in diagnosis.fix_commands:
                    if fix_cmd.risk is not equal "high":
                        log "EXECUTING: " + fix_cmd.command
                        set cmd_result to shell(fix_cmd.command)
                        set exec_results to exec_results + [{"command": fix_cmd.command, "result": cmd_result}]
                        set heal_log to heal_log + "  Executed: " + fix_cmd.command + "\n  Result: " + str(cmd_result) + "\n"
                    otherwise:
                        set heal_log to heal_log + "  SKIPPED (high risk): " + fix_cmd.command + "\n"
                        set exec_results to exec_results + [{"command": fix_cmd.command, "result": "SKIPPED — high risk"}]
                set executed to "yes"
            otherwise:
                set heal_log to heal_log + "STEP 3: SKIPPED — AI marked as unsafe for auto-execution\n\n"
        otherwise:
            set heal_log to heal_log + "STEP 3: SKIPPED — confidence " + str(diagnosis.confidence_percent) + "% below 70% threshold\n\n"
    otherwise:
        set heal_log to heal_log + "STEP 3: SKIPPED — auto_execute not enabled (set to 'yes' to enable)\n\n"

    // Step 4: Verify fix worked
    set verification to "not run"
    if executed is equal "yes":
        set heal_log to heal_log + "\nSTEP 4: VERIFYING FIX...\n"
        if diagnosis.verify_command:
            set verification to shell(diagnosis.verify_command)
            set heal_log to heal_log + "  Command: " + diagnosis.verify_command + "\n  Result: " + str(verification) + "\n"

        // Step 5: Re-check health after fix
        set heal_log to heal_log + "\nSTEP 5: RE-CHECKING HEALTH...\n"
        if urls_str:
            set post_health to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")
            set heal_log to heal_log + "  Post-fix health: " + str(post_health) + "\n"

    // Save report
    set incident_id to "HEAL-" + str(floor(random() * 9000 + 1000))
    set report to {"incident_id": incident_id, "service": service_name, "description": description, "root_cause": diagnosis.root_cause, "confidence": diagnosis.confidence_percent, "category": diagnosis.category, "fix_commands": diagnosis.fix_commands, "rollback_commands": diagnosis.rollback_commands, "auto_executed": executed, "execution_results": exec_results, "verification": verification, "safe_to_auto_execute": diagnosis.safe_to_auto_execute, "heal_log": heal_log, "healed_at": time_now()}

    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(report))
    log heal_log

    // Notify Slack
    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set action_text to "Manual action required"
        if executed is equal "yes":
            set action_text to "AUTO-FIXED"
        set slack_msg to "{\"text\":\"*SwarmOps Self-Heal — " + incident_id + "*\\n*Service:* " + service_name + "\\n*Root Cause:* " + diagnosis.root_cause + "\\n*Status:* " + action_text + "\\n*Confidence:* " + str(diagnosis.confidence_percent) + "%\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg

    respond with report

// ══════════════════════════════════════════════════════════════════
//  ROLLBACK — Undo a self-heal action
// ══════════════════════════════════════════════════════════════════

to rollback with incident_id, secret:
    purpose: "Rollback a self-heal action using saved rollback commands"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        set rollback_results to []
        if incident.rollback_commands:
            repeat for each rb in incident.rollback_commands:
                log "ROLLBACK: " + rb.command
                set result to shell(rb.command)
                set rollback_results to rollback_results + [{"command": rb.command, "result": result}]
        respond with {"incident_id": incident_id, "action": "rollback", "results": rollback_results}
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}

// ══════════════════════════════════════════════════════════════════
//  WATCHDOG — Continuous monitoring loop
//  Call POST /watchdog to start a single check cycle
// ══════════════════════════════════════════════════════════════════

to watchdog:
    purpose: "Check all monitored URLs — if any unhealthy, auto-investigate"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set check_result to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); if [ \"$code\" != \"200\" ]; then echo \"UNHEALTHY:$url:$code\"; fi; done")
        if len(trim(check_result)) is above 0:
            log "WATCHDOG ALERT: " + check_result
            respond with {"status": "unhealthy_detected", "details": check_result, "action": "Call POST /self-heal with service_name and auto_execute=yes to auto-fix"}
        otherwise:
            respond with {"status": "all_healthy", "checked": urls_str}
    otherwise:
        respond with {"status": "not_configured", "error": "Set MONITOR_URLS in .env"}

// ══════════════════════════════════════════════════════════════════
//  INVESTIGATE FROM PASTE — Fallback for manual input
// ══════════════════════════════════════════════════════════════════

to investigate_manual with service_name, description, logs, metrics, errors, deploy_history:
    purpose: "Analyze manually pasted incident data"
    log "Manual investigation: {{service_name}}"
    set context to "SERVICE: {{service_name}}. ISSUE: {{description}}. LOGS: {{logs}}. METRICS: {{metrics}}. ERRORS: {{errors}}. DEPLOY HISTORY: {{deploy_history}}."

    ask AI to "You are SwarmOps Incident Copilot. Analyze this production incident data from all angles — logs, metrics, deployments, dependencies. Determine the root cause and provide fix commands. Context: {{context}}. Return ONLY valid JSON: {\"incident_id\": \"INC-0000\", \"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"severity\": \"critical|high|medium|low\", \"evidence_chain\": [\"string\"], \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}], \"affected_services\": [\"string\"], \"signals\": [{\"name\": \"string\", \"severity\": \"string\", \"evidence\": \"string\"}], \"immediate_actions\": [{\"step\": 0, \"action\": \"string\", \"command\": \"string\", \"risk\": \"string\"}], \"verification\": [\"string\"], \"prevention\": [\"string\"], \"estimated_recovery_minutes\": 0}" save as report

    shell("mkdir -p incidents")
    write_file("incidents/" + report.incident_id + ".json", json_encode(report))
    respond with report

// ══════════════════════════════════════════════════════════════════
//  QUICK ANALYZE — Paste anything, get instant answer
// ══════════════════════════════════════════════════════════════════

to quick_analyze with text, context:
    purpose: "Quick: paste any error/log and get instant diagnosis"
    ask AI to "You are SwarmOps. An engineer pasted this during an incident. Analyze it. Context: {{context}}. Data: {{text}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"severity\": \"critical|high|medium|low\", \"category\": \"string\", \"evidence\": [\"string\"], \"immediate_action\": \"string\", \"command\": \"string\"}" save as result
    respond with result

// ══════════════════════════════════════════════════════════════════
//  CONFIG — Show what's connected
// ══════════════════════════════════════════════════════════════════

to show_config:
    purpose: "Show which data sources are configured (secrets masked)"
    set prom to env("PROMETHEUS_URL")
    set es to env("ELASTICSEARCH_URL")
    set gh to env("GITHUB_TOKEN")
    set slack to env("SLACK_WEBHOOK")
    set urls to env("MONITOR_URLS")
    set ai_key to env("NC_AI_KEY")
    set ai_url to env("NC_AI_URL")
    set ai_model to env("NC_AI_MODEL")
    if ai_key:
        set ai_key to "configured"
    if gh:
        set gh to "configured"
    if slack:
        set slack to "configured"
    respond with {"ai": {"url": ai_url, "model": ai_model, "key": ai_key}, "prometheus": prom, "elasticsearch": es, "github": gh, "slack": slack, "monitor_urls": urls, "mode": "auto"}

// ══════════════════════════════════════════════════════════════════
//  INCIDENT HISTORY
// ══════════════════════════════════════════════════════════════════

to list_incidents:
    set result to shell("ls incidents/*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"incidents": [], "count": 0}
    otherwise:
        set ids to split(trim(result), "\n")
        respond with {"incidents": ids, "count": len(ids)}

to get_incident with incident_id:
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        respond with json_decode(data)
    otherwise:
        respond with {"error": "Not found", "_status": 404}

// ══════════════════════════════════════════════════════════════════
//  SAMPLE DATA — For demo without real monitoring
// ══════════════════════════════════════════════════════════════════

to sample_data:
    respond with {"service_name": "checkout-api", "description": "Checkout failing with HTTP 500 errors since 15:42 UTC", "logs": "2024-03-15 15:42:03 ERROR [checkout-api] FATAL: password authentication failed for user \"checkout_svc\"\n2024-03-15 15:42:03 ERROR [checkout-api] pg_connect(): Unable to connect to PostgreSQL at 10.0.2.15:5432\n2024-03-15 15:42:04 ERROR [checkout-api] ConnectionPool exhausted: 0/20 connections available\n2024-03-15 15:42:05 WARN  [checkout-api] Circuit breaker OPEN for database connections\n2024-03-15 15:42:05 ERROR [payments-api] Upstream timeout: checkout-api did not respond within 5000ms\n2024-03-15 15:42:06 ERROR [checkout-api] Request failed: POST /api/checkout - 503 Service Unavailable\n2024-03-15 15:41:55 INFO  [deploy-bot] Deployed checkout-api v2.4.1 (changes: updated db credentials config)", "metrics": "checkout-api latency_p99: 5200ms (normal: 45ms)\ncheckout-api error_rate: 94% (normal: 0.1%)\ncheckout-api cpu: 12% (normal: 8%)\npayments-api latency_p99: 5100ms (normal: 55ms)\npayments-api error_rate: 34% (normal: 0.1%)\npostgresql connections: 148/200 (normal: 45/200)\npostgresql latency: 8ms (normal: 8ms)", "errors": "FATAL: password authentication failed for user \"checkout_svc\" (x342 in last 1 min)\nConnectionPool exhausted: 0/20 connections available (x156)\nUpstream timeout from checkout-api (x89)", "deploy_history": "15:42 UTC - checkout-api v2.4.1 deployed by deploy-bot\n  Changes: Updated database credentials config, Migrated to new secrets manager\n09:15 UTC - users-api v1.8.0 deployed by ci-pipeline\n  Changes: Added rate limiting middleware"}

// ══════════════════════════════════════════════════════════════════
//  UI
// ══════════════════════════════════════════════════════════════════

to home:
    set html to read_file("public/index.html")
    respond with html

to ui:
    set html to read_file("public/index.html")
    respond with html

to health:
    respond with {"status": "healthy", "service": "swarmops", "version": "6.0.0"}

// ══════════════════════════════════════════════════════════════════
//  MCP — Model Context Protocol integration
//  SwarmOps exposes tools that MCP clients (Claude, Cursor, etc.) can call
//  Also can call external MCP tool servers for extended capabilities
// ══════════════════════════════════════════════════════════════════

to mcp_tools:
    purpose: "List available MCP tools — for Claude Desktop, Cursor, etc."
    respond with {"tools": [{"name": "investigate", "description": "Investigate a production incident. Pulls metrics, logs, deploys and finds root cause.", "parameters": {"service_name": "string", "description": "string"}}, {"name": "k8s_investigate", "description": "Investigate Kubernetes cluster issues. Pulls pod status, logs, events.", "parameters": {"namespace": "string", "description": "string"}}, {"name": "chat", "description": "Ask SwarmOps about incidents, services, or infrastructure.", "parameters": {"message": "string"}}, {"name": "k8s_unhealthy", "description": "Find all unhealthy pods across the cluster.", "parameters": {}}, {"name": "k8s_logs", "description": "Get logs from a Kubernetes pod.", "parameters": {"pod_name": "string", "namespace": "string", "lines": "number"}}, {"name": "k8s_fix", "description": "Fix a K8s issue: restart, rollback, scale a deployment.", "parameters": {"action": "restart|rollback|scale|delete-pod", "target": "string", "namespace": "string", "secret": "string"}}, {"name": "self_heal", "description": "Auto-diagnose and fix a service issue.", "parameters": {"service_name": "string", "description": "string", "auto_execute": "yes|no", "secret": "string"}}, {"name": "knowledge", "description": "View learned patterns from past incidents.", "parameters": {}}, {"name": "rag_search", "description": "Search runbooks and docs.", "parameters": {"query": "string"}}]}

to mcp_call with tool_name, arguments:
    purpose: "MCP tool execution endpoint — called by MCP clients"
    log "MCP tool call: {{tool_name}}"

    if tool_name is equal "investigate":
        gather result from "http://localhost:9090/investigate":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_investigate":
        gather result from "http://localhost:9090/k8s/investigate":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "chat":
        gather result from "http://localhost:9090/chat":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_unhealthy":
        gather result from "http://localhost:9090/k8s/unhealthy"
        respond with result

    if tool_name is equal "k8s_logs":
        gather result from "http://localhost:9090/k8s/logs":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_fix":
        gather result from "http://localhost:9090/k8s/fix":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "self_heal":
        gather result from "http://localhost:9090/self-heal":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "knowledge":
        gather result from "http://localhost:9090/knowledge"
        respond with result

    if tool_name is equal "rag_search":
        gather result from "http://localhost:9090/rag/search":
            method: "POST"
            body: arguments
        respond with result

    respond with {"error": "Unknown tool: " + tool_name}

to mcp_external with tool_name, arguments:
    purpose: "Call an external MCP tool server"
    set mcp_url to env("NC_MCP_URL")
    if mcp_url:
        set mcp_path to env("NC_MCP_PATH")
        if mcp_path:
            set full_url to mcp_url + mcp_path
        otherwise:
            set full_url to mcp_url + "/api/v1/tools/call"
        gather result from full_url:
            method: "POST"
            body: {"tool_name": tool_name, "arguments": arguments}
        respond with result
    otherwise:
        respond with {"error": "NC_MCP_URL not configured"}

middleware:
    rate_limit: 30
    cors: true
    log_requests: true

// ── Routes ──────────────────────────────────────────────────────
api:
    GET / runs home
    GET /ui runs ui
    GET /health runs health
    GET /config runs show_config
    GET /sample runs sample_data
    GET /incidents runs list_incidents
    POST /incident runs get_incident
    POST /investigate runs investigate
    POST /investigate/manual runs investigate_manual
    POST /memory runs load_memory
    POST /feedback runs add_feedback
    GET /knowledge runs get_knowledge
    GET /rag/index runs rag_index
    POST /rag/search runs rag_search
    POST /k8s/pods runs k8s_pods
    GET /k8s/unhealthy runs k8s_unhealthy
    POST /k8s/logs runs k8s_logs
    POST /k8s/events runs k8s_events
    POST /k8s/describe runs k8s_describe
    POST /k8s/fix runs k8s_fix
    POST /k8s/investigate runs k8s_investigate
    GET /mcp/tools runs mcp_tools
    POST /mcp/call runs mcp_call
    POST /mcp/external runs mcp_external
    POST /chat runs chat
    POST /chat/incident runs chat_about_incident
    POST /chat/suggest runs chat_suggest
    POST /nn/load runs load_nn
    POST /nn/predict runs predict_incident
    POST /nn/classify runs nn_classify
    POST /self-heal runs self_heal
    POST /rollback runs rollback
    POST /watchdog runs watchdog
    POST /analyze runs quick_analyze
    POST /monitor runs monitor_check
    POST /pull/health runs pull_health
    POST /pull/prometheus runs pull_prometheus
    POST /pull/logs runs pull_logs
    POST /pull/deploys runs pull_deploys
