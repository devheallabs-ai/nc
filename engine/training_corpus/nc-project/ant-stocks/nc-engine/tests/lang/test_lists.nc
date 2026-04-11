// Test: List literals and operations
// Verifies list creation, len(), and iteration

service "test-lists"
version "1.0.0"

to test list literal:
    respond with [1, 2, 3]

to test empty list:
    respond with []

to test list length:
    set items to [10, 20, 30, 40, 50]
    respond with len(items)

to test mixed list:
    respond with [1, "hello", true, 3.14]
