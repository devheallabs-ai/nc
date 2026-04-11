// Test: gather keyword — data gathering
// Verifies gather from various sources

service "test-gather"
version "1.0.0"

to test gather from string:
    set source to "local_data"
    gather data from source
    set result to "gathered"
    respond with result

to test gather with fallback:
    try:
        gather data from "nonexistent_source_xyz"
    on_error:
        set data to {"fallback": true}
    respond with "ok"

to test gather result type:
    set items to [1, 2, 3]
    set count to len(items)
    respond with count
