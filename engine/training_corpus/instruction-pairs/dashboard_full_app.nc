<|begin|>
// Description: an analytics dashboard with metrics, charts, and AI insights
// Type: full app
service "dashboard"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to get metrics:
    set data to load("metrics.json")
    respond with data

to get summary:
    set metrics to load("metrics.json")
    set total to sum(metrics, "value")
    set average to avg(metrics, "value")
    respond with {"total": total, "average": average, "count": length(metrics)}

to add metric with data:
    set data.id to generate_id()
    set data.timestamp to now()
    set metrics to load("metrics.json")
    add data to metrics
    save metrics to "metrics.json"
    respond with data

to get trends with period:
    set metrics to load("metrics.json")
    set filtered to filter_by_period(metrics, period)
    respond with filtered

api:
    GET /metrics runs get_metrics
    GET /metrics/summary runs get_summary
    POST /metrics runs add_metric
    GET /metrics/trends runs get_trends
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "dashboard"}

// === NC_FILE_SEPARATOR ===

page "Dashboard"
title "Dashboard | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "header":
    heading "Analytics Dashboard"
    text "Real-time insights for your business"

section "kpis":
    grid columns 4:
        card:
            heading "Total Users"
            text from "/metrics/summary" as data: data.total
        card:
            heading "Revenue"
            text "$0"
        card:
            heading "Conversions"
            text "0%"
        card:
            heading "Active Sessions"
            text "0"

section "charts":
    grid columns 2:
        card:
            heading "User Growth"
            text "Chart placeholder"
        card:
            heading "Revenue Trend"
            text "Chart placeholder"

// === NC_AGENT_SEPARATOR ===

// Dashboard AI Agent
service "dashboard-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to analyze trends with data:
    ask AI to "Analyze these metrics and identify trends: {{data}}" save as analysis
    respond with {"analysis": analysis}

to predict next period with history:
    ask AI to "Based on this historical data, predict next period values: {{history}}" save as prediction
    respond with {"prediction": prediction}

to generate report with metrics:
    ask AI to "Write an executive summary of these metrics: {{metrics}}" save as report
    respond with {"report": report}

to handle with prompt:
    purpose: "Handle user request for dashboard"
    ask AI to "You are a helpful dashboard assistant. {{prompt}}" save as response
    respond with {"reply": response}

to classify with input:
    ask AI to "Classify as: create, read, update, delete, help. Input: {{input}}" save as intent
    respond with {"intent": intent}

api:
    POST /agent          runs handle
    POST /agent/classify  runs classify
    GET  /agent/health    runs health_check

to health check:
    respond with {"status": "ok", "ai": "local"}
<|end|>
