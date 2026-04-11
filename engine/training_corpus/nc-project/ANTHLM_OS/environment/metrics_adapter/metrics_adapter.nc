// ══════════════════════════════════════════════════════════════════
//  HiveANT — Metrics Adapter (Environment Interface)
//
//  Connects to metrics platforms:
//    - Prometheus
//    - Custom metric endpoints
//    - Kubernetes metrics-server
// ══════════════════════════════════════════════════════════════════

to pull_prometheus with query:
    purpose: "Query Prometheus metrics"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set full_url to prom_url + "/api/v1/query?query=" + query
        gather result from full_url
        respond with {"source": "prometheus", "query": query, "data": result}
    otherwise:
        respond with {"source": "prometheus", "error": "PROMETHEUS_URL not configured"}

to pull_prometheus_range with query, duration:
    purpose: "Query Prometheus time-series range"
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

to pull_health with target_url:
    purpose: "Check a service health endpoint"
    gather result from target_url
    respond with result

to pull_k8s_metrics with namespace:
    purpose: "Get Kubernetes resource usage metrics"
    if namespace:
        set pods to shell("kubectl top pods -n " + namespace + " 2>&1 || echo 'metrics-server not available'")
        set nodes to shell("kubectl top nodes 2>&1 || echo 'metrics-server not available'")
    otherwise:
        set pods to shell("kubectl top pods --all-namespaces 2>&1 || echo 'metrics-server not available'")
        set nodes to shell("kubectl top nodes 2>&1 || echo 'metrics-server not available'")
    respond with {"pods": pods, "nodes": nodes}

to monitor_endpoints:
    purpose: "Check all configured monitoring endpoints"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set results to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do status=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); latency=$(curl -s -o /dev/null -w '%{time_total}' --connect-timeout 5 $url 2>/dev/null); echo \"{\\\"url\\\":\\\"$url\\\",\\\"status\\\":$status,\\\"latency_ms\\\":$(echo \"$latency * 1000\" | bc 2>/dev/null || echo 0)}\"; done")
        respond with {"source": "health_check", "results": results, "checked_at": time_now()}
    otherwise:
        respond with {"source": "health_check", "error": "MONITOR_URLS not configured"}
