// Test: Modulo operator (%)

service "test-modulo"

to test basic modulo:
    respond with 10 % 3

to test even check:
    set x to 4
    if x % 2 is equal to 0:
        respond with "even"
    otherwise:
        respond with "odd"

to test odd check:
    set x to 7
    if x % 2 is equal to 0:
        respond with "even"
    otherwise:
        respond with "odd"
