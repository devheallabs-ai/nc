// ══════════════════════════════════════════════════════════════════
//  HiveANT — Investigation Agent
//
//  Collects logs, metrics, traces, and deployment data.
//  Correlates signals across multiple data sources.
//  Feeds findings to the Root Cause Agent.
// ══════════════════════════════════════════════════════════════════

to investigate_service with service_name, description:
    purpose: "Full investigation: pull all telemetry and analyze"
    log "INVESTIGATION AGENT: Starting investigation of " + service_name

    shell("mkdir -p incidents knowledge docs")

    set prom_url to env("PROMETHEUS_URL")
    set es_url to env("ELASTICSEARCH_URL")
    set gh_token to env("GITHUB_TOKEN")

    set metrics_data to "No Prometheus configured"
    set log_data to "No Elasticsearch configured"
    set deploy_data to "No GitHub configured"
    set health_data to "No health endpoints configured"

    if prom_url:
        set eq to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + service_name + "\"}[5m])"
        gather prom_errors from eq
        set lq to prom_url + "/api/v1/query?query=histogram_quantile(0.99,rate(http_request_duration_seconds_bucket{service=\"" + service_name + "\"}[5m]))"
        gather prom_latency from lq
        set cq to prom_url + "/api/v1/query?query=rate(container_cpu_usage_seconds_total{pod=~\"" + service_name + ".*\"}[5m])"
        gather prom_cpu from cq
        set metrics_data to json_encode({"error_rate": prom_errors, "latency_p99": prom_latency, "cpu": prom_cpu})

    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + service_name + "+AND+level:ERROR&size=30&sort=@timestamp:desc"
        gather es_logs from log_url
        set log_data to json_encode(es_logs)

    if gh_token:
        set gh_repo to env("GITHUB_REPO")
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=10"
        gather gh_commits from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        set deploy_data to json_encode(gh_commits)

    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    set past_incidents to "none"
    if file_exists("knowledge/semantic.json"):
        set past_incidents to read_file("knowledge/semantic.json")

    set rag_context to "none"
    set all_docs to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
    if len(all_docs) is above 10:
        set doc_chunks to chunk(all_docs, 600, 80)
        set rag_context to str(top_k(doc_chunks, 3))

    set full_context to "SERVICE: {{service_name}}. ISSUE: {{description}}. METRICS: {{metrics_data}}. LOGS: {{log_data}}. DEPLOYS: {{deploy_data}}. HEALTH: {{health_data}}."

    ask AI to "You are a Telemetry Analysis Agent in the HiveANT swarm. Analyze production telemetry data. Context: {{full_context}}. Past patterns: {{past_incidents}}. Relevant docs: {{rag_context}}.

Return ONLY valid JSON:
{
  \"agent\": \"telemetry-analysis\",
  \"signals\": [{\"name\": \"string\", \"severity\": \"critical|high|medium|low\", \"evidence\": \"string\"}],
  \"error_pattern\": \"string\",
  \"metric_anomalies\": [{\"metric\": \"string\", \"value\": \"string\", \"normal\": \"string\"}],
  \"deployment_correlation\": {\"is_deploy_related\": false, \"suspicious_changes\": []},
  \"dependency_impact\": [{\"from\": \"string\", \"to\": \"string\", \"impact\": \"string\"}],
  \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}],
  \"summary\": \"string\"
}" save as investigation

    set investigation.service to service_name
    set investigation.investigated_at to time_now()
    set investigation.data_sources to {"prometheus": prom_url, "elasticsearch": es_url, "github": gh_token}

    shell("mkdir -p incidents")
    set inv_id to "INV-" + str(floor(random() * 9000 + 1000))
    write_file("incidents/" + inv_id + ".json", json_encode(investigation))
    set investigation.investigation_id to inv_id

    log "INVESTIGATION AGENT: Complete — " + str(len(investigation.signals)) + " signals found"
    respond with investigation

to investigate_manual with service_name, description, logs, metrics, errors, deploy_history:
    purpose: "Investigate from manually pasted data"
    set context to "SERVICE: {{service_name}}. ISSUE: {{description}}. LOGS: {{logs}}. METRICS: {{metrics}}. ERRORS: {{errors}}. DEPLOYS: {{deploy_history}}."

    ask AI to "You are an Investigation Agent in HiveANT. Analyze this incident data. Context: {{context}}.
Return ONLY valid JSON:
{
  \"signals\": [{\"name\": \"string\", \"severity\": \"string\", \"evidence\": \"string\"}],
  \"error_pattern\": \"string\",
  \"summary\": \"string\",
  \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}]
}" save as result

    set result.service to service_name
    set result.investigated_at to time_now()
    shell("mkdir -p incidents")
    set inv_id to "INV-" + str(floor(random() * 9000 + 1000))
    write_file("incidents/" + inv_id + ".json", json_encode(result))
    set result.investigation_id to inv_id
    respond with result
