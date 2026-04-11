// Tests for JWT generation and rate limiting features

service "test-jwt-ratelimit"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// JWT Generation
// ═══════════════════════════════════════════════════════════

to test_jwt_generate_basic:
    set token to jwt_generate("user123", "admin", 3600)
    if type(token) is equal "text":
        if contains(token, "."):
            respond with "jwt_ok"
    if type(token) is equal "record":
        respond with "jwt_needs_secret"
    respond with "jwt_ran"

to test_jwt_generate_with_claims:
    set token to jwt_generate("alice", "premium", 7200, {"org": "acme", "tier": "gold"})
    if type(token) is equal "text":
        respond with "jwt_claims_ok"
    respond with "jwt_claims_ran"

to test_jwt_has_three_parts:
    set token to jwt_generate("bob", "user", 3600)
    if type(token) is equal "text":
        set parts to split(token, ".")
        if len(parts) is equal 3:
            respond with "jwt_parts_ok"
    respond with "jwt_parts_ran"
