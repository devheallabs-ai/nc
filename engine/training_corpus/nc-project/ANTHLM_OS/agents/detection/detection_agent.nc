// ══════════════════════════════════════════════════════════════════
//  HiveANT — Detection Agent
//
//  Monitors system health and detects anomalies.
//  Triggers investigation swarms when issues are found.
// ══════════════════════════════════════════════════════════════════

to detect_anomalies with service_name, thresholds:
    purpose: "Detect anomalies across all monitored data sources"
    log "DETECTION AGENT: Scanning for anomalies in " + str(service_name)

    set health_data to "unknown"
    set metrics_data to "not configured"
    set log_data to "not configured"

    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); latency=$(curl -s -o /dev/null -w '%{time_total}' --connect-timeout 5 $url 2>/dev/null); echo \"{\\\"url\\\":\\\"$url\\\",\\\"status\\\":$code,\\\"latency\\\":$latency}\"; done")

    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set error_q to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\"}[5m])"
        gather error_data from error_q
        set latency_q to prom_url + "/api/v1/query?query=histogram_quantile(0.99,rate(http_request_duration_seconds_bucket[5m]))"
        gather latency_data from latency_q
        set cpu_q to prom_url + "/api/v1/query?query=rate(container_cpu_usage_seconds_total[5m])"
        gather cpu_data from cpu_q
        set metrics_data to json_encode({"errors": error_data, "latency_p99": latency_data, "cpu": cpu_data})

    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set log_url to es_url + "/logs-*/_search?q=level:ERROR&size=20&sort=@timestamp:desc"
        gather log_data from log_url

    if thresholds:
        set thresh to thresholds
    otherwise:
        set thresh to {"error_rate_pct": 5, "latency_ms": 1000, "cpu_pct": 80, "http_5xx_count": 10}

    ask AI to "You are a Detection Agent in the HiveANT swarm. Analyze the following system telemetry to detect anomalies.

SERVICE: {{service_name}}
HEALTH CHECKS: {{health_data}}
METRICS: {{metrics_data}}
LOGS: {{log_data}}
THRESHOLDS: {{thresh}}

Rules:
1. Any metric exceeding its threshold is an anomaly
2. Any HTTP 5xx response is an anomaly
3. Error log spikes are anomalies
4. Latency spikes are anomalies
5. Rate of change matters (sudden spike vs gradual drift)

Return ONLY valid JSON:
{
  \"anomalies_detected\": true,
  \"anomaly_count\": 0,
  \"severity\": \"critical|high|medium|low|none\",
  \"anomalies\": [
    {
      \"type\": \"error_rate|latency|cpu|memory|availability|log_spike\",
      \"service\": \"string\",
      \"current_value\": \"string\",
      \"threshold\": \"string\",
      \"severity\": \"critical|high|medium|low\",
      \"description\": \"string\",
      \"first_seen\": \"string\"
    }
  ],
  \"recommendation\": \"investigate|monitor|ignore\",
  \"confidence\": 0.0
}" save as detection

    set detection.detected_at to time_now()
    set detection.service to service_name

    if detection.anomalies_detected:
        shell("mkdir -p incidents")
        set alert_id to "ALERT-" + str(floor(random() * 9000 + 1000))
        write_file("incidents/" + alert_id + ".json", json_encode(detection))
        set detection.alert_id to alert_id
        log "DETECTION AGENT: " + str(detection.anomaly_count) + " anomalies detected (severity=" + detection.severity + ")"
    otherwise:
        log "DETECTION AGENT: No anomalies detected"

    respond with detection

to watchdog_scan:
    purpose: "Quick health check across all monitored endpoints"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set results to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); if [ \"$code\" != \"200\" ]; then echo \"UNHEALTHY:$url:$code\"; else echo \"HEALTHY:$url:$code\"; fi; done")
        set unhealthy to shell("echo '" + results + "' | grep UNHEALTHY | wc -l")
        respond with {
            "scan_time": time_now(),
            "results": results,
            "unhealthy_count": trim(unhealthy),
            "action": "Call POST /investigate if unhealthy services found"
        }
    otherwise:
        respond with {"error": "MONITOR_URLS not configured"}
