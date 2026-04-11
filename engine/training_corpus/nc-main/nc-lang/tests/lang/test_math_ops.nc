// Test: Math operations
// Verifies arithmetic, built-in math functions, type coercion

service "test-math-ops"
version "1.0.0"

to test addition:
    respond with 10 + 20

to test subtraction:
    respond with 100 - 37

to test multiplication:
    respond with 6 * 7

to test division:
    respond with 10 / 4

to test modulo:
    respond with 17 % 5

to test negative:
    respond with -42

to test order of ops:
    respond with 2 + 3 * 4

to test parentheses:
    respond with (2 + 3) * 4

to test abs negative:
    respond with abs(-99)

to test abs positive:
    respond with abs(42)

to test sqrt:
    respond with sqrt(144)

to test sqrt four:
    respond with sqrt(4)

to test pow:
    respond with pow(2, 10)

to test min:
    respond with min(5, 3)

to test max:
    respond with max(5, 3)

to test ceil:
    respond with ceil(3.2)

to test floor:
    respond with floor(3.9)

to test round:
    respond with round(3.5)

to test integer math:
    set a to 10
    set b to 3
    respond with a / b

to test float math:
    set a to 1.5
    set b to 2.5
    respond with a + b

to test accumulator:
    set sum to 0
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each n in nums:
        set sum to sum + n
    respond with sum

to test big numbers:
    respond with 1000000 * 1000000

to test zero division:
    respond with 10 / 0
