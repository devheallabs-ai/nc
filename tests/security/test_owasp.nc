// OWASP Top 10 Security Test Suite for NC
// Validates that NC services enforce OWASP-compliant protections.
// Each test verifies a specific OWASP 2021 category.
//
// Part of the NC language ecosystem by DevHeal Labs AI.

service "owasp-tests"
version "1.0.0"

// ─── OWASP A01: Broken Access Control ───────────────────────

to test_jwt_rejects_invalid_token:
    purpose: "A01 — JWT middleware rejects invalid tokens"
    set token to "invalid.jwt.token"
    set result to validate_jwt(token)
    if result is "invalid":
        respond with "pass"
    otherwise:
        respond with "fail: accepted invalid JWT"

to test_jwt_rejects_expired_token:
    purpose: "A01 — JWT middleware rejects expired tokens"
    set token to create_expired_jwt("user123")
    set result to validate_jwt(token)
    if result is "expired":
        respond with "pass"
    otherwise:
        respond with "fail: accepted expired JWT"

to test_rate_limiting_enforced:
    purpose: "A01 — Rate limiting blocks excessive requests"
    set count to 0
    set blocked to false
    repeat 200 times:
        set count to count + 1
        set result to check_rate_limit("test-client", 100, "per minute")
        if result is "blocked":
            set blocked to true
    if blocked is true:
        respond with "pass"
    otherwise:
        respond with "fail: rate limit never triggered"

to test_role_based_access:
    purpose: "A01 — Role-based access rejects unauthorized users"
    set user to create_test_user("viewer")
    set result to check_access(user, "admin_panel")
    if result is "denied":
        respond with "pass"
    otherwise:
        respond with "fail: viewer accessed admin panel"

// ─── OWASP A03: Injection ───────────────────────────────────

to test_sql_injection_blocked:
    purpose: "A03 — SQL injection patterns are rejected"
    set payload to "'; DROP TABLE users; --"
    set result to sanitize_input(payload)
    if result does not contain "DROP TABLE":
        respond with "pass"
    otherwise:
        respond with "fail: SQL injection not blocked"

to test_nosql_injection_blocked:
    purpose: "A03 — NoSQL injection patterns are rejected"
    set payload to '{"$gt": ""}'
    set result to sanitize_input(payload)
    if result does not contain "$gt":
        respond with "pass"
    otherwise:
        respond with "fail: NoSQL injection not blocked"

to test_path_traversal_blocked:
    purpose: "A03 — Path traversal is blocked"
    set path to "../../etc/passwd"
    set result to validate_path(path)
    if result is "blocked":
        respond with "pass"
    otherwise:
        respond with "fail: path traversal not blocked"

to test_command_injection_blocked:
    purpose: "A03 — Command injection is blocked"
    set input to "test; rm -rf /"
    set result to sanitize_input(input)
    if result does not contain ";":
        respond with "pass"
    otherwise:
        respond with "fail: command injection not blocked"

// ─── OWASP A07: XSS ────────────────────────────────────────

to test_html_entities_escaped:
    purpose: "A07 — HTML entities are escaped in responses"
    set input to "<script>alert('xss')</script>"
    set result to escape_html(input)
    if result does not contain "<script>":
        respond with "pass"
    otherwise:
        respond with "fail: script tag not escaped"

to test_attribute_injection_blocked:
    purpose: "A07 — HTML attribute injection is blocked"
    set input to '" onload="alert(1)'
    set result to escape_attribute(input)
    if result does not contain "onload":
        respond with "pass"
    otherwise:
        respond with "fail: attribute injection not blocked"

// ─── OWASP A09: Security Logging ────────────────────────────

to test_auth_failure_logged:
    purpose: "A09 — Authentication failures are logged"
    set token to "bad_token"
    set before_count to get_security_log_count()
    set result to validate_jwt(token)
    set after_count to get_security_log_count()
    if after_count is above before_count:
        respond with "pass"
    otherwise:
        respond with "fail: auth failure not logged"

to test_access_denied_logged:
    purpose: "A09 — Access denied events are logged"
    set user to create_test_user("guest")
    set before_count to get_security_log_count()
    set result to check_access(user, "admin_panel")
    set after_count to get_security_log_count()
    if after_count is above before_count:
        respond with "pass"
    otherwise:
        respond with "fail: access denial not logged"

to test_rate_limit_violation_logged:
    purpose: "A09 — Rate limit violations are logged"
    set before_count to get_security_log_count()
    repeat 200 times:
        set result to check_rate_limit("log-test-client", 50, "per minute")
    set after_count to get_security_log_count()
    if after_count is above before_count:
        respond with "pass"
    otherwise:
        respond with "fail: rate limit violation not logged"
