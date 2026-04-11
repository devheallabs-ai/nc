// Test: wait keyword — timed pause
// Verifies wait does not crash and execution continues

service "test-wait"
version "1.0.0"

to test wait seconds:
    set before to "before"
    wait 0.01 seconds
    set after to "after"
    respond with before + "-" + after

to test wait zero:
    wait 0 seconds
    respond with "ok"
