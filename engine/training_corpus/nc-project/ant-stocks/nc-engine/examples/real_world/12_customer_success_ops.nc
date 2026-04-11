// ═══════════════════════════════════════════════════════════
//  Customer Success Operations
//
//  Real-world use case:
//  - Account health scoring
//  - AI-assisted success playbook generation
//  - Renewal risk watchlists
//
//  curl -X POST http://localhost:8000/renewal/watchlist \
//    -d '{"accounts":[{"name":"Acme","nps":6,"open_tickets":8,"usage_percent":35,"payment_status":"late"}]}'
// ═══════════════════════════════════════════════════════════

service "customer-success-ops"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o-mini"

to score_account_health with account:
    purpose: "Generate a deterministic account health score"

    set score to 100

    if account.nps is below 7:
        set score to score - 20

    if account.open_tickets is above 5:
        set score to score - 25

    if account.usage_percent is below 40:
        set score to score - 30

    if account.payment_status is equal "late":
        set score to score - 20

    if score is below 0:
        set score to 0

    if score is above 80:
        respond with {"account": account.name, "score": score, "segment": "healthy"}
    otherwise:
        if score is above 60:
            respond with {"account": account.name, "score": score, "segment": "watch"}
        otherwise:
            respond with {"account": account.name, "score": score, "segment": "at_risk"}

to build_success_playbook with account:
    purpose: "AI-generated playbook for customer success managers"

    ask AI to "Create a concise customer success playbook for this account. Return JSON with: top_risks (list), immediate_actions (list), 30_day_plan (list), executive_summary (string).\n\nAccount profile: {{account}}" save as playbook

    respond with {"account": account.name, "playbook": playbook}

to renewal_watchlist with accounts:
    purpose: "Collect accounts that need proactive renewal intervention"

    set watchlist to []

    repeat for each account in accounts:
        run score_account_health with account
        if result.score is below 60:
            append {"account": account.name, "score": result.score, "segment": result.segment} to watchlist

    respond with watchlist

api:
    POST /health/score runs score_account_health
    POST /playbook runs build_success_playbook
    POST /renewal/watchlist runs renewal_watchlist
