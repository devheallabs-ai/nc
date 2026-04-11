// ══════════════════════════════════════════════════════════════════
//  HiveANT — Distributed Trace Adapter
//
//  Ingests and analyzes distributed traces from:
//    - Jaeger / Zipkin
//    - OpenTelemetry
//    - Custom trace systems
// ══════════════════════════════════════════════════════════════════

to pull_traces with service_name, trace_id, limit:
    purpose: "Pull distributed traces for a service"
    set jaeger_url to env("JAEGER_URL")
    if jaeger_url:
        if trace_id:
            set url to jaeger_url + "/api/traces/" + trace_id
        otherwise:
            set url to jaeger_url + "/api/traces?service=" + service_name + "&limit=" + str(limit)
        gather result from url
        respond with {"source": "jaeger", "service": service_name, "data": result}
    otherwise:
        respond with {"source": "jaeger", "error": "JAEGER_URL not configured"}

to analyze_trace with trace_data:
    purpose: "Analyze a distributed trace for bottlenecks"

    ask AI to "Analyze this distributed trace for performance issues. TRACE: {{trace_data}}. Find: 1) Slowest spans. 2) Error spans. 3) Bottleneck services. 4) Unnecessary sequential calls (could be parallel). Return ONLY valid JSON: {\"bottlenecks\": [{\"service\": \"string\", \"span\": \"string\", \"duration_ms\": 0, \"issue\": \"string\"}], \"errors\": [{\"service\": \"string\", \"error\": \"string\"}], \"optimization_suggestions\": [\"string\"], \"critical_path_ms\": 0}" save as analysis

    respond with analysis
