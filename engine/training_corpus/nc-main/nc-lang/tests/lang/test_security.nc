service "test-security"
version "1.0.0"

to test_long_string:
    set x to "AAAAAAAAAA"
    set i to 0
    while i is below 50:
        set x to x + "AAAAAAAAAA"
        set i to i + 1
    respond with len(x)

to test_nested_list:
    set inner to [1, 2, 3]
    set outer to [inner, inner, inner]
    set deep to [outer, outer]
    respond with len(deep)

to test_path_traversal:
    set result to read_file("../../etc/passwd")
    respond with type(result)

to test_special_chars_in_string:
    set s to "it's a \"test\" with \\ backslash"
    respond with len(s)

to test_empty_inputs:
    set a to ""
    set b to []
    set c to {}
    respond with len(a) + len(b) + len(keys(c))

to test_large_list:
    set items to []
    set i to 0
    while i is below 500:
        append i to items
        set i to i + 1
    respond with len(items)

to test_division_by_zero:
    set x to 10
    set y to 0
    set result to x / y
    respond with result

to test_type_mismatch:
    set x to "hello"
    set y to 42
    set result to x + y
    respond with result

to test_none_operations:
    set x to nothing
    set result to type(x)
    respond with result

to test_negative_index:
    set items to [1, 2, 3]
    set result to items[-1]
    respond with type(result)
