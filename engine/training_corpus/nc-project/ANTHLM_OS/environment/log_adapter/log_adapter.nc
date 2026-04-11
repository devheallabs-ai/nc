// ══════════════════════════════════════════════════════════════════
//  HiveANT — Log Adapter (Environment Interface)
//
//  Connects to log ingestion systems:
//    - Elasticsearch / OpenSearch
//    - File-based logs
//    - Kubernetes pod logs
// ══════════════════════════════════════════════════════════════════

to pull_logs with search_query, service_name, time_range:
    purpose: "Search logs from configured log backend"
    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set query to search_query
        if service_name:
            set query to "service:" + service_name + "+AND+" + search_query
        set search_url to es_url + "/logs-*/_search?q=" + query + "&size=50&sort=@timestamp:desc"
        gather result from search_url
        respond with {"source": "elasticsearch", "query": query, "data": result}
    otherwise:
        respond with {"source": "elasticsearch", "error": "ELASTICSEARCH_URL not configured"}

to pull_k8s_logs with pod_name, namespace, lines:
    purpose: "Get logs from a Kubernetes pod"
    if namespace:
        set ns to namespace
    otherwise:
        set ns to "default"
    if lines:
        set tail to str(lines)
    otherwise:
        set tail to "100"
    set result to shell("kubectl logs " + pod_name + " -n " + ns + " --tail=" + tail + " 2>&1")
    respond with {"pod": pod_name, "namespace": ns, "logs": result}

to pull_file_logs with log_path, pattern, lines:
    purpose: "Search local log files"
    if lines:
        set tail to str(lines)
    otherwise:
        set tail to "100"
    if pattern:
        set result to shell("grep -i '" + pattern + "' " + log_path + " | tail -" + tail + " 2>&1")
    otherwise:
        set result to shell("tail -" + tail + " " + log_path + " 2>&1")
    respond with {"file": log_path, "pattern": pattern, "logs": result}
