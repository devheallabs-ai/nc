// Test: store keyword — data persistence
// Verifies store...into and in-memory store behavior

service "test-store"
version "1.0.0"

to test store and retrieve:
    store "hello world" into "test_key"
    set result to "stored"
    respond with result

to test store record:
    set data to {"name": "NC", "version": 1}
    store data into "config_key"
    respond with "ok"

to test store overwrites:
    store "first" into "ow_key"
    store "second" into "ow_key"
    respond with "overwritten"
