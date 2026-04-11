<|begin|>
// Description: an API gateway with routing, rate limiting, and authentication
// Type: full app
service "api-gateway"
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

to proxy request with service and path and method:
    set services to load("services.json")
    set target to find_by(services, "name", service)
    if target is empty:
        respond with error "Service not found" status 404
    set response to forward(target.url + path, method)
    respond with response

to list services:
    set services to load("services.json")
    respond with services

to register service with data:
    set data.id to generate_id()
    set data.registered_at to now()
    set services to load("services.json")
    add data to services
    save services to "services.json"
    respond with data

to health check all:
    set services to load("services.json")
    set results to []
    repeat for each service in services:
        set health to ping(service.url + "/health")
        set result to {"service": service.name, "status": health.status}
        add result to results
    respond with results

api:
    ANY /:service/:path runs proxy_request
    GET /gateway/services runs list_services
    POST /gateway/services runs register_service
    GET /gateway/health runs health_check_all
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "api-gateway"}

// === NC_FILE_SEPARATOR ===

page "Api Gateway"
title "Api Gateway | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "gateway":
    heading "API Gateway"
    text "Route, secure, and monitor your APIs"

    grid columns 3:
        card:
            heading "Services"
            text from "/gateway/services" as s: s.length
        card:
            heading "Requests/min"
            text "0"
        card:
            heading "Uptime"
            text "100%"

    list from "/gateway/services" as services:
        card:
            heading services.name
            text services.url style mono
            badge services.status

// === NC_AGENT_SEPARATOR ===

// Api Gateway AI Agent
service "api-gateway-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to analyze traffic with logs:
    ask AI to "Analyze this API traffic and identify anomalies: {{logs}}" save as analysis
    respond with {"analysis": analysis}

to handle with prompt:
    purpose: "Handle user request for api-gateway"
    ask AI to "You are a helpful api-gateway assistant. {{prompt}}" save as response
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
