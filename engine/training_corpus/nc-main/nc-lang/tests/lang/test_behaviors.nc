// Test: Behaviors (functions in NC)
// Verifies behavior definitions, parameters, calling, and nesting

service "test-behaviors"
version "1.0.0"

to greet with name:
    respond with "Hello, " + name

to test basic behavior:
    run greet with "World"
    respond with result

to add with a, b:
    respond with a + b

to test params:
    run add with 10, 20
    respond with result

to double with x:
    respond with x * 2

to quadruple with x:
    run double with x
    set d to result
    run double with d
    respond with result

to test nested behavior calls:
    run quadruple with 5
    respond with result

to factorial with n:
    if n is below 2:
        respond with 1
    otherwise:
        set prev to n - 1
        run factorial with prev
        respond with n * result

to test recursive behavior:
    run factorial with 6
    respond with result

to classify with score:
    if score is above 90:
        respond with "A"
    otherwise:
        if score is above 80:
            respond with "B"
        otherwise:
            if score is above 70:
                respond with "C"
            otherwise:
                respond with "F"

to test classify A:
    run classify with 95
    respond with result

to test classify B:
    run classify with 85
    respond with result

to test classify F:
    run classify with 50
    respond with result
