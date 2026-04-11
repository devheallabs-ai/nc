<|begin|>
// Description: a multi-tenant operations dashboard with role based access analytics alert center approvals and dark theme
// Type: full app
service "ops-center"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

configure:
    rate_limit is 100
    port is 8080

to list_tenants:
    set tenants to load("tenants.json")
    respond with tenants

to list_roles:
    set roles to load("roles.json")
    respond with roles

to permission_matrix:
    set matrix to load("permissions.json")
    respond with matrix

to analytics_overview:
    set metrics to load("metrics.json")
    set pending to length(load("approvals.json"))
    respond with {"active_tenants": length(load("tenants.json")), "pending_approvals": pending, "active_alerts": 3, "automation_success": 98, "metrics": metrics}

to list_approvals:
    set approvals to load("approvals.json")
    respond with approvals

to approve_request with id:
    set approvals to load("approvals.json")
    set index to find_index(approvals, id)
    set approvals[index].status to "approved"
    save approvals to "approvals.json"
    respond with approvals[index]

to reject_request with id:
    set approvals to load("approvals.json")
    set index to find_index(approvals, id)
    set approvals[index].status to "rejected"
    save approvals to "approvals.json"
    respond with approvals[index]

to list_alerts:
    set alerts to load("alerts.json")
    respond with alerts

api:
    GET /api/v1/tenants runs list_tenants
    GET /api/v1/roles runs list_roles
    GET /api/v1/permissions runs permission_matrix
    GET /api/v1/analytics/overview runs analytics_overview
    GET /api/v1/approvals runs list_approvals
    POST /api/v1/approvals/:id/approve runs approve_request
    POST /api/v1/approvals/:id/reject runs reject_request
    GET /api/v1/alerts runs list_alerts
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "ops-center"}

// === NC_FILE_SEPARATOR ===

page "Ops Center"
title "Ops Center | NC App"

theme:
    primary is "#22d3ee"
    background is "#0a0a0f"
    foreground is "#e0e0e8"
    font is "Inter, sans-serif"

nav:
    brand "Ops Center"
    link "Dashboard" to "/dashboard"
    link "Tenants" to "/tenants"
    link "Roles" to "/roles"
    link "Analytics" to "/analytics"
    link "Approvals" to "/approvals"
    link "Alerts" to "/alerts"

section "hero":
    heading "Ops Center"
    text "Multi-tenant operations control plane"

section "kpis":
    grid columns 4:
        card:
            heading "Active Tenants"
            text from "/api/v1/analytics/overview" as data: data.active_tenants
        card:
            heading "Pending Approvals"
            text from "/api/v1/analytics/overview" as data: data.pending_approvals
        card:
            heading "Active Alerts"
            text from "/api/v1/analytics/overview" as data: data.active_alerts
        card:
            heading "Automation Success"
            text from "/api/v1/analytics/overview" as data: data.automation_success

section "roles":
    heading "Role-Based Access"
    list from "/api/v1/roles" as roles:
        card:
            heading roles.name
            text "Permissions managed centrally"

section "analytics":
    heading "Analytics Overview"
    grid columns 3:
        card:
            heading "Approval SLA"
            text "98% within target"
        card:
            heading "Escalations"
            text "2 active"
        card:
            heading "Throughput"
            text "42 ops/min"

section "approvals":
    heading "Approval Queue"
    list from "/api/v1/approvals" as approvals:
        card:
            heading approvals.title
            text approvals.status
            button "Approve" action "approveRequest" style primary
            button "Reject" action "rejectRequest" style danger

section "alerts":
    heading "Alert Center"
    list from "/api/v1/alerts" as alerts:
        card:
            heading alerts.severity
            text alerts.message

// === NC_AGENT_SEPARATOR ===

// Ops Center AI Agent
service "ops-center-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to summarize approvals with approvals:
    ask AI to "Summarize approval risk and blockers: {{approvals}}" save as summary
    respond with {"summary": summary}

to recommend action with alert:
    ask AI to "Recommend the best next action for this alert: {{alert}}" save as action
    respond with {"action": action}

to handle with prompt:
    purpose: "Handle user request for ops-center"
    ask AI to "You are a helpful ops-center assistant. {{prompt}}" save as response
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
