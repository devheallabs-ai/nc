// Test: Match/when pattern matching
// Verifies match with string and otherwise

service "test-match"
version "1.0.0"

to test match first:
    set status to "healthy"
    match status:
        when "healthy":
            respond with "ok"
        when "degraded":
            respond with "warning"
        when "critical":
            respond with "alert"

to test match middle:
    set status to "degraded"
    match status:
        when "healthy":
            respond with "ok"
        when "degraded":
            respond with "warning"
        when "critical":
            respond with "alert"

to test match otherwise:
    set status to "unknown"
    match status:
        when "healthy":
            respond with "ok"
        when "critical":
            respond with "alert"
        otherwise:
            respond with "unrecognized"
