// Test: configure block — service configuration
// Verifies configure block parsing and key-value pairs

service "test-configure"
version "1.0.0"

configure:
    port is 8080
    debug is true
    ai_model is "gpt-4o"
    max_retries is 3

to test configure exists:
    set result to "configured"
    respond with result

to test with configuration:
    set x to 42
    if x is equal 42:
        respond with "config ok"
    otherwise:
        respond with "config fail"
