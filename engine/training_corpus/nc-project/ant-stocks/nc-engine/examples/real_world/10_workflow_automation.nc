// ═══════════════════════════════════════════════════════════
//  Workflow Automation
//
//  Replaces: 200+ lines of Python with task queues + multi-API integration
//
//  Automates multi-step business workflows with AI.
//
//  curl -X POST http://localhost:8000/onboard \
//    -d '{"employee": {"name": "Alice", "role": "Engineer", "team": "Platform"}}'
// ═══════════════════════════════════════════════════════════

service "workflow-automation"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o-mini"

to onboard_employee with employee:
    purpose: "Automate new employee onboarding"

    log "Starting onboarding for {{employee.name}}"

    ask AI to "Create a personalized onboarding checklist for a new {{employee.role}} joining the {{employee.team}} team. Return JSON with: welcome_message (string), first_week (list of tasks), first_month (list of tasks), tools_needed (list of strings), people_to_meet (list of strings)." save as plan

    log "Onboarding plan created for {{employee.name}}"

    respond with {"employee": employee.name, "plan": plan, "status": "onboarding_started"}

to process_invoice with invoice:
    purpose: "AI-powered invoice processing"

    ask AI to "Process this invoice data. Return JSON with: vendor (string), amount (number), currency (string), due_date (string), line_items (list), category (string), approved (boolean — approve if under 10000).\n\nInvoice: {{invoice}}" save as processed

    if processed.approved:
        log "Invoice approved: {{processed.vendor}} — {{processed.amount}} {{processed.currency}}"
    otherwise:
        log "Invoice needs manual review: {{processed.vendor}} — {{processed.amount}}"

    respond with processed

to daily_digest with data_sources:
    purpose: "Generate a daily digest from multiple data sources"

    set all_data to ""
    repeat for each source in data_sources:
        set content to read_file(source)
        if type(content) is not equal "none":
            set all_data to all_data + "\n--- " + source + " ---\n" + content

    ask AI to "Create a concise daily digest from these data sources. Include: top 5 highlights, any action items, any risks or alerts. Format as a clean report.\n\n{{all_data}}" save as digest

    respond with digest

to lead_scoring with lead:
    purpose: "Score and qualify sales leads"

    ask AI to "Score this sales lead. Return JSON with: score (1-100), qualification (hot/warm/cold), reasoning (string), next_action (string), estimated_value (string).\n\nLead info: {{lead}}" save as score

    if score.score is above 70:
        log "HOT LEAD: {{lead.company}} — score {{score.score}}"

    respond with score

to incident_response with alert:
    purpose: "Automated incident response — classify, prioritize, respond"

    ask AI to "Classify this alert and suggest response. Return JSON with: severity (critical/high/medium/low), category (infrastructure/security/performance/data), root_cause_guess (string), immediate_actions (list of strings), who_to_notify (list of strings), estimated_resolution_time (string).\n\nAlert: {{alert}}" save as response

    if response.severity is equal "critical":
        log "CRITICAL INCIDENT: {{response.category}} — {{response.root_cause_guess}}"

    respond with response

api:
    POST /onboard runs onboard_employee
    POST /invoice runs process_invoice
    POST /digest runs daily_digest
    POST /lead runs lead_scoring
    POST /incident runs incident_response
