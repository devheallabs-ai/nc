// Test: try/on error execution
// Critical — any external call failure must not crash the service

service "test-try-error"

to test try success:
    try:
        set x to 42
        respond with x
    on error:
        respond with "should not reach here"

to test try responds:
    try:
        respond with "success"
    on error:
        respond with "error"

// on_error (underscore) must also work
to test on_error keyword:
    try:
        set val to 100
        respond with val
    on_error:
        respond with "fallback"
