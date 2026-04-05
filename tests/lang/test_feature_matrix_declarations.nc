// Functional coverage: declaration matrix and routing blocks
// Ensures parser/runtime exercise top-level language declarations.

service "test-feature-matrix"
version "1.0.0"
model "openai/gpt-4o-mini"
author "nc-tests"
description "Covers declarations, events, schedules, middleware, and routes"

import "json"

configure:
    env: "test"
    retries: 2
    timeout_ms: 5000

define Job as:
    id is text
    priority is number
    owner is text optional

middleware:
    auth:
        type: "bearer"
    cors:
        origin: "*"

on event "deploy.completed":
    log "event handled"

every 5 minutes:
    log "scheduler tick"

to health_check:
    respond with {"status": "ok", "service": "test-feature-matrix"}

to decide_with_match with status:
    match status:
        when "ready":
            respond with "ship"
        otherwise:
            respond with "hold"

to test_import_and_config:
    set settings to {"env": "test", "retries": 2}
    respond with settings

api:
    GET /health runs health_check
    POST /decide runs decide_with_match
