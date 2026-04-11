// Test: String operations
// Verifies all string built-in functions

service "test-string-ops"
version "1.0.0"

to test upper:
    respond with upper("hello world")

to test lower:
    respond with lower("HELLO WORLD")

to test trim:
    respond with trim("   spaces   ")

to test trim newlines:
    respond with trim("\n\thello\n\t")

to test split basic:
    set parts to split("a,b,c,d", ",")
    respond with len(parts)

to test split space:
    set words to split("hello world nc", " ")
    respond with words[2]

to test join:
    respond with join(["one", "two", "three"], "-")

to test contains true:
    respond with contains("hello world", "world")

to test contains false:
    respond with contains("hello world", "xyz")

to test replace:
    respond with replace("hello world", "world", "NC")

to test starts_with true:
    respond with starts_with("hello", "hel")

to test starts_with false:
    respond with starts_with("hello", "xyz")

to test ends_with true:
    respond with ends_with("hello.nc", ".nc")

to test ends_with false:
    respond with ends_with("hello.nc", ".py")

to test string concat:
    set a to "hello"
    set b to " "
    set c to "world"
    respond with a + b + c

to test string length:
    respond with len("notation as code")

to test string index:
    set s to "hello"
    respond with s[0]

to test string last char:
    set s to "world"
    respond with s[-1]

to test string slice:
    set s to "notation"
    respond with s[0:4]

to test string to number:
    set s to "42"
    respond with int(s) + 8

to test number to string:
    set n to 100
    respond with str(n) + " points"

to test empty string falsy:
    set s to ""
    if s:
        respond with "truthy"
    otherwise:
        respond with "falsy"
