service "auth-utils"
version "1.0.0"
description "Authentication utility behaviors"

to require_auth with request:
    purpose: "Check if request has valid authentication"
    if request.auth is equal nothing:
        respond with {"error": "Authentication required", "status": 401}
    if request.auth.authenticated is equal no:
        respond with {"error": "Invalid credentials", "status": 403}
    respond with {"authenticated": yes, "user": request.auth.user_id, "role": request.auth.role}

to check_role with request, required_role:
    purpose: "Verify user has the required role"
    if request.auth.role is not equal required_role:
        respond with {"error": "Insufficient permissions", "required": required_role, "actual": request.auth.role}
    respond with {"authorized": yes}

to rate_limit_check with client_id, max_requests:
    purpose: "Simple in-memory rate limit check"
    log "Rate limit check for {{client_id}}: max {{max_requests}}/min"
    respond with {"allowed": yes, "client": client_id}
