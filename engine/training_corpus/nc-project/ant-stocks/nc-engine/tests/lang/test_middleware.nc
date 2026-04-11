// Test: middleware block — server middleware
// Verifies middleware declarations parse correctly

service "test-middleware"
version "1.0.0"

configure:
    auth is "bearer"
    rate_limit is 100
    cors is true
    log_requests is true

middleware:
    auth
    rate_limit
    cors

to health:
    respond with {"status": "ok"}

to protected_action:
    respond with {"message": "authenticated action"}

api:
    GET /health runs health
    POST /action runs protected_action
