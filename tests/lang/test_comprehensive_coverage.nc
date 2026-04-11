// Test: Comprehensive coverage for all NC language features
// Covers edge cases, combinations, and full feature matrix

service "test-comprehensive-coverage"
version "1.0.0"

// ── Arithmetic ──

to test add integers:
    respond with 2 + 3

to test subtract:
    respond with 10 - 4

to test multiply:
    respond with 3 * 4

to test divide:
    respond with 10 / 2

to test modulo:
    respond with 10 % 3

to test negative:
    respond with -5 + 10

to test float arithmetic:
    respond with 1.5 + 2.5

to test precedence:
    respond with 2 + 3 * 4

// ── String operations ──

to test string concat:
    respond with "hello" + " " + "world"

to test string index:
    set s to "hello"
    respond with s[0]

to test string length:
    respond with len("abc")

// ── Boolean logic ──

to test and true:
    if true and true:
        respond with "pass"
    otherwise:
        respond with "fail"

to test or false:
    if false or false:
        respond with "fail"
    otherwise:
        respond with "pass"

to test not:
    if not false:
        respond with "pass"
    otherwise:
        respond with "fail"

// ── Comparison combinations ──

to test keyword above:
    set x to 10
    if x is above 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test keyword below:
    set x to 3
    if x is below 10:
        respond with "pass"
    otherwise:
        respond with "fail"

to test keyword equal:
    set x to 5
    if x is equal to 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol gt:
    if 10 > 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol lt:
    if 3 < 10:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol gte:
    if 5 >= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol lte:
    if 5 <= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol eq:
    if 42 == 42:
        respond with "pass"
    otherwise:
        respond with "fail"

to test symbol neq:
    if 1 != 2:
        respond with "pass"
    otherwise:
        respond with "fail"

// ── List operations ──

to test list create:
    set x to [1, 2, 3]
    respond with len(x)

to test list add:
    set x to [1, 2]
    add 3 to x
    respond with len(x)

to test list append:
    set x to [10, 20]
    append 30 to x
    respond with x[2]

to test list push:
    set x to ["a"]
    push "b" to x
    respond with len(x)

to test list index:
    set x to [10, 20, 30]
    respond with x[1]

to test list negative index:
    set x to [10, 20, 30]
    respond with x[-1]

to test list slice:
    set x to [1, 2, 3, 4, 5]
    set s to x[1:3]
    respond with len(s)

// ── Map operations ──

to test map create:
    set m to {"key": "val"}
    respond with m["key"]

to test map dot:
    set m to {"name": "nc"}
    respond with m.name

to test map set:
    set m to {"x": 1}
    set m.x to 2
    respond with m.x

to test map deep set:
    set m to {"a": {"b": 10}}
    set m.a.b to 20
    set inner to m["a"]
    respond with inner["b"]

to test map bracket set:
    set m to {"key": "old"}
    set m["key"] to "new"
    respond with m["key"]

// ── Control flow ──

to test if true:
    if true:
        respond with "yes"
    otherwise:
        respond with "no"

to test if false:
    if false:
        respond with "yes"
    otherwise:
        respond with "no"

to test multi if:
    set x to 10
    if x > 5:
        show "a"
    if x > 3:
        show "b"
    respond with "pass"

to test repeat count:
    set sum to 0
    repeat 5 times:
        add 1 to sum
    respond with sum

to test repeat each:
    set result to ""
    set items to ["a", "b", "c"]
    repeat for each item in items:
        set result to result + item
    respond with result

to test while loop:
    set i to 0
    repeat while i is below 5:
        set i to i + 1
    respond with i

// ── Functions with params ──

to helper_add with a, b:
    respond with a + b

to test function call:
    set result to helper_add(3, 4)
    respond with result

to helper_greet with name:
    respond with "hello " + name

to test function string:
    set msg to helper_greet("NC")
    respond with msg

// ── Match/when ──

to test match:
    set x to "b"
    match x:
        when "a":
            respond with "first"
        when "b":
            respond with "second"
        when "c":
            respond with "third"

// ── Nested data ──

to test nested map access:
    set info to {"users": [{"name": "alice"}]}
    set users to info["users"]
    set first to users[0]
    respond with first["name"]
