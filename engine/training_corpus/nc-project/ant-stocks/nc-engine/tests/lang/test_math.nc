// Test: Arithmetic operations
// Verifies +, -, *, / on integers and floats

service "test-math"
version "1.0.0"

to test addition:
    respond with 10 + 20

to test subtraction:
    respond with 50 - 15

to test multiplication:
    respond with 6 * 7

to test division:
    respond with 100 / 4

to test compound:
    set x to 10
    set y to 20
    set z to x + y
    respond with z

to test precedence:
    respond with 2 + 3 * 4

to test multi step:
    set a to 5
    set b to a * 2
    set c to b + 3
    respond with c
