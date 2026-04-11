service "ticket-classifier"
version "1.0.0"

to classify with ticket:
    ask AI to "classify this support ticket" using ticket
    save as category
    respond with category

to health_check:
    respond with {"status": "healthy"}

api:
    POST /classify runs classify
    GET /health runs health_check
