service "enterprise-accounts"
version "1.0.0"
description "Enterprise account operations service for provisioning, lifecycle updates, and internal summaries."

configure:
    port: 8400
    crm_service is "http://localhost:8401"
    audit_service is "http://localhost:8402"
    ai_model is "gpt-4o-mini"
    default_plan is "growth"

middleware:
    cors: true
    log_requests: true

to list_accounts with status, owner_email:
    purpose: "List customer accounts with optional lifecycle filters"
    set filters to {"status": status, "owner_email": owner_email}
    gather records from "{{config.crm_service}}/accounts/search":
        method: "POST"
        body: filters
        content_type: "application/json"
    set accounts to []
    repeat for each item in records.accounts:
        append {
            "account_id": item.account_id,
            "name": item.name,
            "status": item.status,
            "plan": item.plan,
            "owner_email": item.owner_email,
            "renewal_date": item.renewal_date
        } to accounts
    respond with {
        "accounts": accounts,
        "count": len(accounts),
        "filters": filters
    }

to get_account with account_id:
    purpose: "Load a single account for operations review"
    if account_id is equal "":
        respond with {"error": "account_id is required", "_status": 400}
    gather account from "{{config.crm_service}}/accounts/{{account_id}}"
    respond with {
        "account_id": account.account_id,
        "name": account.name,
        "status": account.status,
        "plan": account.plan,
        "owner_email": account.owner_email,
        "open_tickets": account.open_tickets,
        "modules": account.modules,
        "renewal_date": account.renewal_date
    }

to create_account with name, owner_email, plan:
    purpose: "Create a new enterprise account with a safe default plan"
    if name is equal "":
        respond with {"error": "name is required", "_status": 400}
    if owner_email is equal "":
        respond with {"error": "owner_email is required", "_status": 400}
    if plan is equal "":
        set selected_plan to config.default_plan
    otherwise:
        set selected_plan to plan
    set payload to {
        "name": name,
        "owner_email": owner_email,
        "plan": selected_plan,
        "status": "active"
    }
    gather created from "{{config.crm_service}}/accounts":
        method: "POST"
        body: payload
        content_type: "application/json"
    gather audit_result from "{{config.audit_service}}/events":
        method: "POST"
        body: {"category": "account_created", "account_id": created.account_id, "owner_email": owner_email}
        content_type: "application/json"
    respond with {
        "account": created,
        "audit_id": audit_result.event_id,
        "message": "Account created"
    }

to update_account with account_id, patch:
    purpose: "Apply a partial update to an account record"
    if account_id is equal "":
        respond with {"error": "account_id is required", "_status": 400}
    gather updated from "{{config.crm_service}}/accounts/{{account_id}}":
        method: "PUT"
        body: patch
        content_type: "application/json"
    respond with {
        "account": updated,
        "message": "Account updated"
    }

to archive_account with account_id, reason:
    purpose: "Archive an account without losing its renewal history"
    if account_id is equal "":
        respond with {"error": "account_id is required", "_status": 400}
    if reason is equal "":
        set archive_reason to "customer requested archive"
    otherwise:
        set archive_reason to reason
    gather archived from "{{config.crm_service}}/accounts/{{account_id}}/archive":
        method: "POST"
        body: {"reason": archive_reason}
        content_type: "application/json"
    respond with {
        "account_id": account_id,
        "status": archived.status,
        "reason": archive_reason,
        "archived_at": archived.archived_at
    }

to summarize_account with account_id:
    purpose: "Create an internal release-readiness summary for one account"
    if account_id is equal "":
        respond with {"error": "account_id is required", "_status": 400}
    gather account from "{{config.crm_service}}/accounts/{{account_id}}"
    ask AI to """You are an enterprise customer success assistant. Summarize this account for an internal release readiness meeting.

ACCOUNT:
- Name: {{account.name}}
- Plan: {{account.plan}}
- Status: {{account.status}}
- Open tickets: {{account.open_tickets}}
- Renewal date: {{account.renewal_date}}
- Product modules: {{account.modules}}

Return valid JSON with:
- headline: one short sentence
- health: \"green\", \"yellow\", or \"red\"
- summary: one paragraph
- next_actions: list of 3 actions""" save as summary
    respond with {
        "account_id": account_id,
        "headline": summary.headline,
        "health": summary.health,
        "summary": summary.summary,
        "next_actions": summary.next_actions
    }

to health_check:
    purpose: "Return the service health for platform checks"
    respond with {
        "status": "healthy",
        "service": "enterprise-accounts",
        "version": "1.0.0"
    }

api:
    GET /health runs health_check
    GET /accounts runs list_accounts
    GET /accounts/:account_id runs get_account
    POST /accounts runs create_account
    PUT /accounts/:account_id runs update_account
    DELETE /accounts/:account_id runs archive_account
    POST /accounts/:account_id/summary runs summarize_account