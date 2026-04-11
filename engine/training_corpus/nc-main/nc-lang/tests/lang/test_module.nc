// Test: module system — cross-file behavior
// Verifies module resolution and behavior calling

service "test-module"
version "1.0.0"

to test local behavior call:
    set x to 10
    set y to 20
    run add_numbers with x, y
    respond with x + y

to add_numbers with a, b:
    set sum to a + b
    respond with sum

to test behavior chain:
    run step_one
    respond with "chain ok"

to step_one:
    set x to 1
    respond with x
