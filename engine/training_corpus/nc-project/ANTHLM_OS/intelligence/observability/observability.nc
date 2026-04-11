// ══════════════════════════════════════════════════════════════════
//  HiveANT — Observability Intelligence Layer
//
//  Processes logs, metrics, distributed traces, and deployment
//  events. Provides anomaly detection, trend analysis, correlation
//  detection, and event clustering for investigation agents.
// ══════════════════════════════════════════════════════════════════

to observe_anomalies with service_name, time_window:
    purpose: "Detect anomalies across all telemetry signals"
    set prom_url to env("PROMETHEUS_URL")
    set es_url to env("ELASTICSEARCH_URL")
    set metrics_data to "not configured"
    set log_data to "not configured"

    if prom_url:
        set eq to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + service_name + "\"}[5m])"
        gather metrics_data from eq
    if es_url:
        set lurl to es_url + "/logs-*/_search?q=service:" + service_name + "+AND+level:ERROR&size=50&sort=@timestamp:desc"
        gather log_data from lurl

    ask AI to "You are the Observability Intelligence Agent. Analyze all telemetry for anomalies. SERVICE: {{service_name}}. TIME WINDOW: {{time_window}}. METRICS: {{metrics_data}}. LOGS: {{log_data}}. Perform: 1) Statistical anomaly detection (z-score, IQR). 2) Trend analysis (increasing/decreasing/stable). 3) Change point detection. 4) Event clustering (group related events). Return ONLY valid JSON: {\"anomalies\": [{\"type\": \"string\", \"metric\": \"string\", \"severity\": \"critical|high|medium|low\", \"current_value\": \"string\", \"expected_range\": \"string\", \"z_score\": 0.0, \"description\": \"string\"}], \"trends\": [{\"metric\": \"string\", \"direction\": \"increasing|decreasing|stable\", \"rate_of_change\": 0.0}], \"change_points\": [{\"time\": \"string\", \"metric\": \"string\", \"before\": \"string\", \"after\": \"string\"}], \"event_clusters\": [{\"cluster_id\": 0, \"theme\": \"string\", \"event_count\": 0, \"first_seen\": \"string\"}], \"health_score\": 0.0}" save as obs_result

    set obs_result.service to service_name
    set obs_result.observed_at to time_now()
    respond with obs_result

to observe_correlations with signals:
    purpose: "Find correlations between different telemetry signals"

    ask AI to "You are a signal correlation engine. Find patterns across these signals: {{signals}}. Look for: 1) Temporal correlations (events happening together). 2) Lagged correlations (A happens, then B follows). 3) Inverse correlations (A goes up, B goes down). 4) Seasonal patterns. Return ONLY valid JSON: {\"correlations\": [{\"signal_a\": \"string\", \"signal_b\": \"string\", \"type\": \"temporal|lagged|inverse|seasonal\", \"strength\": 0.0, \"lag_seconds\": 0, \"description\": \"string\"}], \"patterns\": [{\"pattern\": \"string\", \"confidence\": 0.0}]}" save as corr_result

    respond with corr_result

to observe_baseline with service_name:
    purpose: "Establish baseline metrics for a service"

    set prom_url to env("PROMETHEUS_URL")
    set baseline_data to "not configured"
    if prom_url:
        set bq to prom_url + "/api/v1/query?query=avg_over_time(http_request_duration_seconds{service=\"" + service_name + "\"}[24h])"
        gather baseline_data from bq

    ask AI to "Establish performance baselines for {{service_name}}. CURRENT METRICS: {{baseline_data}}. Return ONLY valid JSON: {\"baselines\": [{\"metric\": \"string\", \"mean\": 0.0, \"p50\": 0.0, \"p95\": 0.0, \"p99\": 0.0, \"stddev\": 0.0}], \"thresholds\": [{\"metric\": \"string\", \"warning\": 0.0, \"critical\": 0.0}]}" save as baselines

    shell("mkdir -p knowledge")
    write_file("knowledge/baseline-" + service_name + ".json", json_encode(baselines))
    respond with baselines
