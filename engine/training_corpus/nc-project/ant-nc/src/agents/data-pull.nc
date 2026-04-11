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
