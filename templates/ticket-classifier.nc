// Ticket Classifier — AI-powered support ticket routing
//
// Classifies incoming support tickets into categories using AI.
// Deploy: NC_AI_KEY=sk-xxx nc serve templates/ticket-classifier.nc

service "ticket-classifier"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"
    ai_key is "env:NC_AI_KEY"
    port: 8000

to classify with ticket:
    purpose: "Classify a support ticket into a category"
    ask AI to "Classify this support ticket into exactly one category: technical, billing, account, or general. Respond with only the category name. Ticket: {{ticket}}" save as category
    respond with {"category": category, "ticket": ticket}

to health:
    respond with {"status": "healthy", "service": "ticket-classifier"}

middleware:
    rate_limit: 100
    cors: true
    log_requests: true

api:
    POST /classify runs classify
    GET /health runs health
