// Test: while loops

service "test-while"

to test basic while:
    set x to 0
    while x is below 5:
        set x to x + 1
    respond with x

to test while accumulate:
    set sum to 0
    set i to 1
    while i is below 11:
        set sum to sum + i
        set i to i + 1
    respond with sum
