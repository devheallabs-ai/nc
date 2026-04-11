// Test: repeat while — synonym for while
// Verifies that 'repeat while' works identically to 'while'

service "test-repeat-while"
version "1.0.0"

to test basic counter:
    set n to 0
    repeat while n is below 5:
        set n to n + 1
    respond with n

to test accumulate:
    set total to 0
    set i to 1
    repeat while i is below 11:
        set total to total + i
        set i to i + 1
    respond with total

to test stop in repeat while:
    set n to 0
    repeat while n is below 100:
        set n to n + 1
        if n is equal to 7:
            stop
    respond with n

to test skip in repeat while:
    set total to 0
    set i to 0
    repeat while i is below 10:
        set i to i + 1
        if i % 2 is equal to 0:
            skip
        set total to total + i
    respond with total

to test zero iterations:
    set x to 99
    repeat while x is below 0:
        set x to x + 1
    respond with x

to test nested repeat while:
    set total to 0
    set i to 0
    repeat while i is below 3:
        set j to 0
        repeat while j is below 3:
            set total to total + 1
            set j to j + 1
        set i to i + 1
    respond with total

to test string building:
    set result to ""
    set i to 0
    repeat while i is below 3:
        set result to result + str(i)
        set i to i + 1
    respond with result
