// AI Ticket Classifier — No coding knowledge needed
// Run: nc run examples/02_ai_classifier.nc

service "ticket-classifier"
version "1.0.0"
model "gpt-4o"

define Ticket as:
    id is text
    title is text
    description is text
    priority is text optional

configure:
    port: 8081
    confidence_threshold: 0.7

to classify a ticket:
    purpose: "Classify a support ticket using AI"

    ask AI to "classify this support ticket into one of: bug, feature, question, incident, security" using ticket:
        confidence: 0.7
        save as: classification

    ask AI to "assess the priority as low, medium, high, or critical" using ticket, classification:
        save as: priority

    log "Classified ticket as {{classification.category}}"

    respond with classification

to classify many tickets with tickets:
    purpose: "Classify a batch of tickets"

    set results to []
    repeat for each ticket in tickets:
        run classify with ticket
        store result into "classifications"

    respond with results

api:
    POST /classify runs classify
    POST /classify/batch runs classify_many
    GET /health runs health_check
