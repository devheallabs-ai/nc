<|begin|>
// Description: a service monitoring and health check system with alerting
// Type: full app
service "monitoring"
version "1.0.0"

to add service with data:
    set data.id to generate_id()
    set data.status to "unknown"
    set data.registered_at to now()
    set services to load("monitored_services.json")
    add data to services
    save services to "monitored_services.json"
    respond with data

to check all services:
    set services to load("monitored_services.json")
    set results to []
    repeat for each service in services:
        set health to ping(service.url)
        set service.status to health.ok ? "up" : "down"
        set service.last_checked to now()
        set service.latency to health.latency
        add service to results
    save services to "monitored_services.json"
    respond with results

to get service status with id:
    set services to load("monitored_services.json")
    set service to find_by(services, "id", id)
    respond with service

to list down services:
    set services to load("monitored_services.json")
    set down to filter(services, "status", "down")
    respond with down

api:
    POST /services runs add_service
    GET /services/check runs check_all_services
    GET /services/:id runs get_service_status
    GET /services/alerts/down runs list_down_services
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "monitoring"}

// === NC_FILE_SEPARATOR ===

page "Monitoring"
title "Monitoring | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "monitoring":
    heading "Service Monitoring"
    button "Check All" action "checkAll" style primary

    grid columns 3:
        card:
            heading "Total Services"
            text from "/services" as s: s.length
        card:
            heading "Up"
            text style success
        card:
            heading "Down"
            text style danger

    list from "/services" as services:
        card:
            heading services.name
            text services.url style mono
            badge services.status "up" "down"
            text services.latency style meta
            text services.last_checked style meta

// === NC_AGENT_SEPARATOR ===

// Monitoring AI Agent
service "monitoring-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to diagnose outage with service:
    ask AI to "Diagnose this service outage and suggest fixes: {{service.name}} at {{service.url}} — error: {{service.error}}" save as diagnosis
    respond with {"diagnosis": diagnosis}

to generate incident report with services:
    ask AI to "Write an incident report for these down services: {{services}}" save as report
    respond with {"report": report}

to handle with prompt:
    purpose: "Handle user request for monitoring"
    ask AI to "You are a helpful monitoring assistant. {{prompt}}" save as response
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
