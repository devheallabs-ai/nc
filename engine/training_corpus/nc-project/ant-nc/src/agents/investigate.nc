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

