// Test: Variable assignment and access
// Verifies set/to and variable scoping

service "test-variables"
version "1.0.0"

to test set and respond:
    set x to 42
    respond with x

to test overwrite:
    set x to 1
    set x to 2
    set x to 3
    respond with x

to test multiple vars:
    set a to 10
    set b to 20
    set c to 30
    respond with a + b + c
