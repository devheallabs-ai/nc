// Test: Error handling with try/on error
// Verifies try blocks execute normally

service "test-try"
version "1.0.0"

to test try normal:
    try:
        set x to 42
        respond with x
    on error:
        respond with "error occurred"

to test try responds:
    try:
        log "attempting operation"
        respond with "success"
