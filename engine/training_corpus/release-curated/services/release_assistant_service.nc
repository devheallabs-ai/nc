service "release-assistant"
version "1.0.0"
description "AI service for release notes, migration guidance, and support classification."

configure:
    port: 8450
    ai_model is "gpt-4o-mini"
    docs_service is "http://localhost:8451"
    changelog_service is "http://localhost:8452"

middleware:
    cors: true
    log_requests: true

to draft_release_notes with version, highlights, breaking_changes:
    purpose: "Draft release notes for SDK and platform consumers"
    if version is equal "":
        respond with {"error": "version is required", "_status": 400}
    gather previous from "{{config.changelog_service}}/latest"
    ask AI to """You are a release manager for an enterprise SDK.

CURRENT VERSION: {{version}}
PREVIOUS SUMMARY: {{previous.summary}}
HIGHLIGHTS: {{highlights}}
BREAKING CHANGES: {{breaking_changes}}

Return valid JSON with:
- title
- summary
- highlights: list
- breaking_changes: list
- upgrade_steps: list of 4 concise steps
- sdk_message: one sentence for SDK consumers""" save as notes
    respond with {
        "version": version,
        "title": notes.title,
        "summary": notes.summary,
        "highlights": notes.highlights,
        "breaking_changes": notes.breaking_changes,
        "upgrade_steps": notes.upgrade_steps,
        "sdk_message": notes.sdk_message
    }

to build_sdk_migration_plan with current_version, target_version, changed_modules:
    purpose: "Create a migration plan for SDK consumers moving between versions"
    if current_version is equal "":
        respond with {"error": "current_version is required", "_status": 400}
    if target_version is equal "":
        respond with {"error": "target_version is required", "_status": 400}
    ask AI to """You are an SDK migration planner.

CURRENT VERSION: {{current_version}}
TARGET VERSION: {{target_version}}
CHANGED MODULES: {{changed_modules}}

Return valid JSON with:
- risk_level: \"low\", \"medium\", or \"high\"
- summary
- required_code_changes: list
- rollout_order: list
- verification_steps: list""" save as plan
    respond with {
        "current_version": current_version,
        "target_version": target_version,
        "risk_level": plan.risk_level,
        "summary": plan.summary,
        "required_code_changes": plan.required_code_changes,
        "rollout_order": plan.rollout_order,
        "verification_steps": plan.verification_steps
    }

to classify_support_ticket with ticket_text, customer_tier:
    purpose: "Classify incoming support tickets for the release operations queue"
    if ticket_text is equal "":
        respond with {"error": "ticket_text is required", "_status": 400}
    ask AI to """You are a support triage assistant for an enterprise release team.

CUSTOMER TIER: {{customer_tier}}
TICKET: {{ticket_text}}

Return valid JSON with:
- label: one of \"release-blocker\", \"migration-help\", \"usage-question\", or \"feature-request\"
- priority: one of \"low\", \"medium\", \"high\", or \"urgent\"
- summary: one sentence
- next_team: one of \"sdk\", \"platform\", or \"support\""" save as result
    respond with {
        "label": result.label,
        "priority": result.priority,
        "summary": result.summary,
        "next_team": result.next_team,
        "customer_tier": customer_tier
    }

to validate_release_readiness with candidate_version, open_incidents, failed_checks:
    purpose: "Score whether a candidate build is ready for production rollout"
    if candidate_version is equal "":
        respond with {"error": "candidate_version is required", "_status": 400}
    gather template from "{{config.docs_service}}/templates/release-gate"
    ask AI to """You are a release gate reviewer.

VERSION: {{candidate_version}}
OPEN INCIDENTS: {{open_incidents}}
FAILED CHECKS: {{failed_checks}}
GATE TEMPLATE: {{template}}

Return valid JSON with:
- decision: \"pass\", \"review\", or \"block\"
- confidence: number 0-100
- summary
- must_fix: list
- owner_actions: list""" save as gate
    respond with {
        "candidate_version": candidate_version,
        "decision": gate.decision,
        "confidence": gate.confidence,
        "summary": gate.summary,
        "must_fix": gate.must_fix,
        "owner_actions": gate.owner_actions
    }

to health_check:
    purpose: "Return health for SDK release workflows"
    respond with {
        "status": "healthy",
        "service": "release-assistant",
        "version": "1.0.0"
    }

api:
    GET /health runs health_check
    POST /release-notes runs draft_release_notes
    POST /sdk/migration-plan runs build_sdk_migration_plan
    POST /tickets/classify runs classify_support_ticket
    POST /release-readiness runs validate_release_readiness