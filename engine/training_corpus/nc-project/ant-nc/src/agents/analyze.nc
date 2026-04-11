
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
    purpose: "Show which data sources are configured"
    set prom to env("PROMETHEUS_URL")
    set es to env("ELASTICSEARCH_URL")
    set gh to env("GITHUB_TOKEN")
    set slack to env("SLACK_WEBHOOK")
    set urls to env("MONITOR_URLS")
    set ai_key to env("NC_AI_KEY")
    respond with {"prometheus": prom, "elasticsearch": es, "github": gh, "slack": slack, "monitor_urls": urls, "ai_configured": ai_key, "mode": "auto"}

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

