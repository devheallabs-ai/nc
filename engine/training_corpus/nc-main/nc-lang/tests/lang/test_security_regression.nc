service "test-security-regression"
version "1.0.0"

# Security regression tests for vulnerabilities fixed in v1.0.0
# Tests integer overflow, stack safety, recursion limits, and edge cases

# --- Integer Overflow Protection ---

to test_integer_overflow_add:
    set x to 9223372036854775807
    set result to x + 1
    respond with type(result)

to test_integer_overflow_subtract:
    set x to -9223372036854775807
    set result to x - 1
    respond with type(result)

to test_integer_overflow_multiply:
    set x to 9223372036854775807
    set result to x * 2
    respond with type(result)

to test_large_multiply:
    set a to 3000000000
    set b to 3000000000
    set result to a * b
    respond with type(result)

# --- Division Safety ---

to test_division_by_zero_int:
    set x to 42
    set y to 0
    set result to x / y
    respond with type(result)

to test_division_by_zero_float:
    set x to 3.14
    set y to 0
    set result to x / y
    respond with type(result)

to test_modulo_by_zero:
    set x to 42
    set y to 0
    set result to x % y
    respond with type(result)

# --- Large Collection Safety ---

to test_large_list_creation:
    set items to []
    set i to 0
    while i is below 1000:
        append i to items
        set i to i + 1
    respond with len(items)

to test_large_map_creation:
    set data to {}
    set i to 0
    while i is below 100:
        set key to "key_" + str(i)
        set data[key] to i
        set i to i + 1
    respond with len(keys(data))

# --- String Safety ---

to test_very_long_string:
    set s to ""
    set i to 0
    while i is below 100:
        set s to s + "ABCDEFGHIJ"
        set i to i + 1
    respond with len(s)

to test_string_with_null_chars:
    set s to "hello\0world"
    respond with type(s)

to test_string_escape_sequences:
    set s to "tab\there\nnewline\\backslash\"quote"
    respond with type(s)

# --- Nested Data Safety ---

to test_deeply_nested_list:
    set a to [1]
    set b to [a]
    set c to [b]
    set d to [c]
    set e to [d]
    respond with type(e)

to test_deeply_nested_map:
    set a to {"x": 1}
    set b to {"inner": a}
    set c to {"inner": b}
    set d to {"inner": c}
    respond with type(d)

# --- Type Coercion Safety ---

to test_mixed_type_addition:
    set x to "hello"
    set y to 42
    set result to x + y
    respond with type(result)

to test_none_arithmetic:
    set x to nothing
    set result to type(x)
    respond with result

to test_bool_arithmetic:
    set x to yes
    set y to 1
    set result to x + y
    respond with type(result)

# --- Index Safety ---

to test_out_of_bounds_index:
    set items to [1, 2, 3]
    set result to items[99]
    respond with type(result)

to test_negative_index_list:
    set items to [1, 2, 3]
    set result to items[-1]
    respond with type(result)

to test_empty_list_index:
    set items to []
    set result to items[0]
    respond with type(result)

to test_map_missing_key:
    set data to {"a": 1}
    set result to data["missing"]
    respond with type(result)

# --- Loop Safety ---

to test_while_zero_iterations:
    set count to 0
    set i to 10
    while i is below 0:
        set count to count + 1
        set i to i + 1
    respond with count

to test_repeat_empty_list:
    set count to 0
    repeat for each item in []:
        set count to count + 1
    respond with count

# --- Path Traversal Prevention ---

to test_path_traversal_dotdot:
    set result to read_file("../../etc/passwd")
    respond with type(result)

to test_path_traversal_absolute:
    set result to read_file("/etc/shadow")
    respond with type(result)

# --- Error Handling Safety ---

to test_try_catch_division:
    try:
        set result to 1 / 0
    on error:
        respond with "caught"

to test_try_catch_type_error:
    try:
        set x to "abc" - 1
    on error:
        respond with "caught"

to test_nested_try:
    try:
        try:
            set x to 1 / 0
        on error:
            respond with "inner caught"
    on error:
        respond with "outer caught"
