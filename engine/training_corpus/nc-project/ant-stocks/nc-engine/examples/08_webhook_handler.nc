service "webhook-handler"
version "1.0.0"

configure:
    ai_model is "env:NC_AI_MODEL"
    metrics_url is "env:NC_METRICS_URL"
    search_url is "env:NC_SEARCH_URL"
    alert_url is "env:NC_ALERT_URL"

to handle_webhook with event_type, payload:
    purpose: "Generic webhook processor — works with any service"

    match event_type:
        when "push":
            ask AI to "Summarize this code push event: {{payload}}" save as summary
            log "Push event processed"
            respond with {"status": "processed", "summary": summary}

        when "alert":
            set severity to payload.severity
            if severity is equal "critical":
                notify ops "Critical alert: {{payload.message}}"
            log "Alert received: {{payload.message}}"
            respond with {"status": "acknowledged", "severity": severity}

        when "payment":
            log "Payment event: {{payload.amount}}"
            respond with {"processed": yes, "type": event_type}

    respond with {"status": "ok", "event": event_type}

to query_metrics with query, time_range:
    purpose: "Query any metrics endpoint via HTTP"

    set endpoint to env("NC_METRICS_URL")
    if endpoint is equal none:
        respond with {"error": "NC_METRICS_URL not configured"}

    set auth_headers to {"Authorization": "Bearer " + env("NC_METRICS_TOKEN")}
    set result to http_get(endpoint + "/api/v1/query", auth_headers)
    respond with result

to search_data with query, index_name:
    purpose: "Search any data store via HTTP"

    set endpoint to env("NC_SEARCH_URL")
    if endpoint is equal none:
        respond with {"error": "NC_SEARCH_URL not configured"}

    set search_body to {"query": {"match": {"_all": query}}, "size": 10}
    set url to endpoint + "/" + index_name + "/_search"
    set headers to {"Content-Type": "application/json"}
    set result to http_post(url, search_body, headers)
    respond with result

to send_alert with channel, message, severity:
    purpose: "Send alert to any notification endpoint"

    set endpoint to env("NC_ALERT_URL")
    if endpoint is equal none:
        notify channel message
        respond with {"sent_to": "stdout", "channel": channel}

    set alert_body to {"channel": channel, "text": message, "severity": severity}
    set headers to {"Authorization": "Bearer " + env("NC_ALERT_TOKEN"), "Content-Type": "application/json"}
    set result to http_post(endpoint, alert_body, headers)
    respond with {"sent_to": "webhook", "result": result}

to stream_events with source_url:
    purpose: "Connect to a WebSocket event stream"

    set conn to ws_connect(source_url)
    if conn.connected:
        ws_send(conn, json_encode({"type": "subscribe", "channel": "events"}))
        set msg to ws_receive(conn)
        ws_close(conn)
        respond with {"message": msg, "source": source_url}

    respond with {"error": "Could not connect to " + source_url}

to health:
    respond with {"status": "healthy", "service": "webhook-handler"}

api:
    POST /webhook runs handle_webhook
    GET  /metrics runs query_metrics
    POST /search runs search_data
    POST /alert runs send_alert
    POST /stream runs stream_events
    GET  /health runs health
