// Test: REPL-style interactive patterns
// Verifies one-liner expressions and interactive usage patterns

service "test-repl"
version "1.0.0"

// Simple expression
to test expression:
    set x to 2 + 3 * 4
    respond with x

// Variable reassignment chain
to test reassignment:
    set x to 1
    set x to x + 1
    set x to x * 2
    set x to x - 1
    respond with x

// String interpolation style
to test string building:
    set name to "world"
    set greeting to "Hello, " + name + "!"
    respond with greeting

// Quick math
to test math chain:
    set result to sqrt(pow(3, 2) + pow(4, 2))
    respond with result

// Type conversion
to test type conversion:
    set num to 42
    set text to str(num)
    set back to int(text)
    respond with back

// Inline list operations
to test inline ops:
    set data to [5, 3, 1, 4, 2]
    set s to sort(data)
    set r to reverse(s)
    respond with first(r)
