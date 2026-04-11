service "test-edge-cases"
version "1.0.0"

to test_empty_string:
    set s to ""
    respond with len(s)

to test_zero_value:
    set x to 0
    if x is equal 0:
        respond with "zero"

to test_none_handling:
    set x to nothing
    respond with type(x)

to test_empty_list:
    set items to []
    respond with len(items)

to test_empty_map:
    set data to {}
    respond with len(keys(data))

to test_negative_number:
    set x to -42
    set y to abs(x)
    respond with y

to test_float_precision:
    set x to 0.1 + 0.2
    respond with x

to test_string_with_spaces:
    set s to "hello world foo bar"
    set parts to split(s, " ")
    respond with len(parts)

to test_bool_in_list:
    set items to [yes, no, yes]
    respond with len(items)

to test_mixed_list:
    set items to [1, "two", yes, nothing]
    respond with len(items)
