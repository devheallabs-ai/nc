// Test: String operations
// Verifies concatenation, template resolution, and string + number

service "test-strings"
version "1.0.0"

to test concat:
    respond with "hello" + " " + "world"

to test string plus number:
    set x to 42
    respond with "value: " + x

to test multipart:
    set part1 to "NC"
    set part2 to " is "
    set part3 to "fast"
    respond with part1 + part2 + part3

to test empty concat:
    respond with "" + "hello" + ""
