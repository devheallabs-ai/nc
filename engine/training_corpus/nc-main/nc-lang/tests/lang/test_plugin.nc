// Test: plugin system — extensibility
// Verifies that plugin-related code paths don't crash
// (actual plugin loading requires .so/.dll files)

service "test-plugin"
version "1.0.0"

to test without plugins:
    set x to 42
    set y to 58
    respond with x + y

to test plugin fallback:
    set result to "no plugin loaded"
    respond with result

to test builtin works without plugin:
    set data to {"key": "value"}
    set json_str to json_encode(data)
    set parsed to json_decode(json_str)
    respond with parsed.key
