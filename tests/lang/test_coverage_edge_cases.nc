// Coverage: Edge cases, error paths, boundary conditions
// Tests every corner case a real language must handle

service "test-coverage-edge-cases"
version "1.0.0"

// ── Division by zero ─────────────────────────
to test_divide_by_zero:
    respond with 10 / 0

to test_modulo_by_zero:
    respond with 10 % 0

// ── Unset variables ──────────────────────────
to test_unset_is_nothing:
    respond with type(unset_var)

to test_unset_in_if:
    if unset_var is equal nothing:
        respond with "correctly nothing"
    otherwise:
        respond with "bug"

// ── Empty collections ────────────────────────
to test_empty_list_len:
    respond with len([])

to test_empty_map_len:
    respond with len({})

to test_empty_string_len:
    respond with len("")

// ── Negative indexing edge ───────────────────
to test_out_of_bounds_index:
    set items to [1, 2, 3]
    respond with type(items[99])

// ── Deeply nested maps ──────────────────────
to test_deep_nesting:
    set data to {"a": {"b": {"c": {"d": {"e": 42}}}}}
    respond with data.a.b.c.d.e

// ── Large list ───────────────────────────────
to test_large_list:
    set items to range(100)
    respond with len(items)

// ── String with special chars ────────────────
to test_string_with_newline:
    set s to "line1\nline2"
    respond with len(s)

to test_string_with_quotes:
    set s to "she said \"hello\""
    respond with type(s)

// ── Map with many keys ──────────────────────
to test_map_many_keys:
    set m to {"a": 1, "b": 2, "c": 3, "d": 4, "e": 5, "f": 6, "g": 7, "h": 8}
    respond with len(m)

// ── Boolean edge cases ──────────────────────
to test_truthy_string:
    set s to "hello"
    if s:
        respond with "truthy"

to test_truthy_zero:
    set x to 0
    if not x:
        respond with "zero is falsy"

to test_truthy_empty_string:
    set s to ""
    if not s:
        respond with "empty string is falsy"

// ── Chained operations ──────────────────────
to test_chained_math:
    respond with 1 + 2 + 3 + 4 + 5

to test_chained_concat:
    respond with "a" + "b" + "c" + "d"

// ── Map with variable values ────────────────
to test_map_all_types:
    set m to {"int": 1, "float": 3.14, "str": "hello", "bool": true, "none": nothing, "list": [1, 2]}
    respond with len(m)

// ── Nested loops ────────────────────────────
to test_nested_loop_count:
    set total to 0
    repeat for each i in range(5):
        repeat for each j in range(5):
            set total to total + 1
    respond with total

// ── Overwrite variable ──────────────────────
to test_variable_overwrite:
    set x to "first"
    set x to "second"
    set x to "third"
    respond with x

// ── Set field creates map key ───────────────
to test_set_field_update:
    set obj to {"count": 0}
    set obj.count to 10
    set obj.count to obj.count + 5
    respond with obj.count

// ── Respond from inside if ──────────────────
to test_respond_from_if:
    set x to 42
    if x is above 0:
        respond with "positive"

// ── Find in loop ─────────────────────────────
to test_find_in_loop:
    set found to "none"
    repeat for each i in [10, 20, 30]:
        if i is equal 20:
            set found to i
    respond with found

// ── Type coercion in comparison ─────────────
to test_int_float_comparison:
    respond with 5 is equal 5.0

// ── Multiline list with maps ────────────────
to test_multiline_list:
    set items to [
        {"id": 1, "name": "first"},
        {"id": 2, "name": "second"},
        {"id": 3, "name": "third"}
    ]
    respond with items[1].name

// ── Using result from run ───────────────────
to double with n:
    respond with n * 2

to test_run_result_in_map:
    run double with 21
    respond with {"answer": result}
