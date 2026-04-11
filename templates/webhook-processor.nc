// Webhook Processor — Handle GitHub and Stripe webhooks with AI
//
// Receives webhooks, uses AI to summarize events, logs them.
// Deploy: NC_AI_KEY=sk-xxx nc serve templates/webhook-processor.nc

service "webhook-processor"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"
    ai_key is "env:NC_AI_KEY"
    port: 8000

to handle_github with action, repository, sender:
    purpose: "Process GitHub webhook events"
    match action:
        when "opened":
            ask AI to "Write a one-line summary: GitHub {{action}} event in {{repository}} by {{sender}}" save as summary
            log summary
            respond with {"summary": summary, "action": action}
        when "closed":
            log "Closed event"
            respond with {"status": "noted", "action": action}
    respond with {"status": "ok", "action": action}

to handle_stripe with type, data:
    purpose: "Process Stripe payment webhooks"
    match type:
        when "payment_intent.succeeded":
            log "Payment received"
            respond with {"processed": yes, "type": type}
        when "payment_intent.failed":
            log "Payment FAILED"
            respond with {"processed": yes, "alert": "Payment failed"}
    respond with {"processed": yes, "type": type}

to health:
    respond with {"status": "healthy", "service": "webhook-processor"}

middleware:
    rate_limit: 200
    log_requests: true

api:
    POST /webhook/github runs handle_github
    POST /webhook/stripe runs handle_stripe
    GET /health runs health
