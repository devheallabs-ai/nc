// Test: Variable scoping
// Verifies that scope works correctly across behaviors and blocks

service "test-scope"
version "1.0.0"

to test local scope:
    set x to 10
    respond with x

to test scope isolation:
    set a to 1
    set b to 2
    respond with a + b

to test scope in loop:
    set total to 0
    repeat for each i in [1, 2, 3]:
        set total to total + i
    respond with total

to test scope in if:
    set result to "none"
    if true:
        set result to "found"
    respond with result

to test overwrite:
    set x to 1
    set x to 2
    set x to 3
    respond with x

to test string accumulation:
    set s to ""
    repeat for each word in ["hello", " ", "world"]:
        set s to s + word
    respond with s

to test counter:
    set count to 0
    repeat for each item in [1, 2, 3, 4, 5]:
        set count to count + 1
    respond with count
