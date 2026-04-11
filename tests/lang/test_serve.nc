// Test: HTTP server routes — api block
// Verifies API route declarations parse correctly

service "test-serve"
version "1.0.0"

to health:
    respond with {"status": "healthy", "version": "1.0.0"}

to echo with data:
    respond with data

to greet with request:
    set name to "world"
    respond with "Hello, " + name

api:
    GET /health runs health
    POST /echo runs echo
    GET /greet runs greet
