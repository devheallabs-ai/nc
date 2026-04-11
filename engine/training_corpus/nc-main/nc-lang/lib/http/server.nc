// NC Standard Library — http.server
// Built-in web server capabilities

service "nc.http.server"
version "1.0.0"
description "HTTP server helpers and middleware"

to health_check:
    purpose: "Standard health endpoint"
    respond with "healthy"

to not_found with path:
    purpose: "Standard 404 response"
    respond with "Not found: " + path

to method_not_allowed with method:
    purpose: "Standard 405 response"
    respond with "Method not allowed: " + method

to cors_headers:
    purpose: "Return CORS configuration"
    respond with "allowed"

to json_response with data:
    purpose: "Wrap data in standard JSON response"
    respond with data

to error_response with message and code:
    purpose: "Standard error response"
    respond with message
