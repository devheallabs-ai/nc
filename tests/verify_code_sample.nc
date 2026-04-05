service "customer-support"
version "1.0.0"

to classify with ticket:
    ask AI to "classify this support ticket as: technical, billing, or general" using ticket
    save as category
    respond with {"category": category}

to health_check:
    respond with {"status": "healthy"}

middleware:
    auth: "bearer"
    rate_limit: 100

api:
    POST /classify runs classify
    GET /health runs health_check
