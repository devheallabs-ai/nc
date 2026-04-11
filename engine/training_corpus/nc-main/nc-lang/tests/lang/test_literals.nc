// Test: Literal values and types
// Verifies that integers, floats, strings, booleans, and nothing work correctly

service "test-literals"
version "1.0.0"

to test integer:
    respond with 42

to test negative integer:
    respond with 0 - 100

to test float:
    respond with 3.14

to test string:
    respond with "hello world"

to test empty string:
    respond with ""

to test boolean true:
    respond with true

to test boolean false:
    respond with false

to test nothing:
    respond with nothing
