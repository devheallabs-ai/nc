// Coverage: All expression types
// Tests every expression node in the AST

service "test-coverage-expressions"
version "1.0.0"

// ── Literals ─────────────────────────────────
to test_int_literal:
    respond with 42

to test_float_literal:
    respond with 3.14

to test_string_literal:
    respond with "hello"

to test_bool_true:
    respond with true

to test_bool_false:
    respond with false

to test_none_literal:
    respond with nothing

to test_list_literal:
    respond with [1, 2, 3]

to test_map_literal:
    respond with {"a": 1, "b": 2}

to test_empty_list:
    respond with []

to test_empty_map:
    respond with {}

// ── Math operations ──────────────────────────
to test_add_int:
    respond with 10 + 20

to test_subtract:
    respond with 50 - 8

to test_multiply:
    respond with 6 * 7

to test_divide:
    respond with 100 / 4

to test_modulo:
    respond with 17 % 5

to test_negate:
    set x to 10
    respond with 0 - x

to test_float_math:
    respond with 1.5 + 2.5

to test_mixed_math:
    respond with 10 + 3.14

to test_string_concat:
    respond with "hello" + " " + "world"

to test_string_plus_number:
    respond with "count: " + 42

to test_math_precedence:
    respond with 2 + 3 * 4

// ── Comparisons ──────────────────────────────
to test_equal:
    respond with 5 is equal 5

to test_not_equal:
    respond with 5 is not equal 3

to test_above:
    respond with 10 is above 5

to test_below:
    respond with 3 is below 7

to test_at_least:
    respond with 5 is at least 5

to test_at_most:
    respond with 5 is at most 5

to test_string_equal:
    respond with "abc" is equal "abc"

to test_string_not_equal:
    respond with "abc" is not equal "xyz"

to test_is_in:
    set fruits to ["apple", "banana", "cherry"]
    respond with "banana" is in fruits

to test_is_not_in:
    set fruits to ["apple", "banana"]
    respond with "grape" is not in fruits

// ── Logic ────────────────────────────────────
to test_and_true:
    respond with true and true

to test_and_false:
    respond with true and false

to test_or_true:
    respond with false or true

to test_or_false:
    respond with false or false

to test_not:
    respond with not false

to test_complex_logic:
    set x to 5
    respond with x is above 3 and x is below 10

// ── Dot access ───────────────────────────────
to test_dot_simple:
    set obj to {"name": "alice"}
    respond with obj.name

to test_dot_nested:
    set obj to {"a": {"b": {"c": 99}}}
    respond with obj.a.b.c

to test_dot_on_none:
    set obj to nothing
    respond with obj.name

// ── Index access ─────────────────────────────
to test_list_index_first:
    set items to ["a", "b", "c"]
    respond with items[0]

to test_list_index_last:
    set items to ["a", "b", "c"]
    respond with items[2]

to test_map_index:
    set config to {"port": 8080}
    set k to "port"
    respond with config[k]

to test_string_index:
    set s to "hello"
    respond with s[0]

// ── Slice ────────────────────────────────────
to test_string_slice:
    set s to "hello world"
    respond with s[0:5]

to test_list_slice:
    set items to [10, 20, 30, 40, 50]
    respond with items[1:4]

to test_slice_from_start:
    set s to "abcdef"
    respond with s[:3]

to test_slice_to_end:
    set s to "abcdef"
    respond with s[3:]

// ── Template expressions ─────────────────────
to test_template_simple:
    set name to "world"
    set msg to "hello {{name}}"
    respond with msg

to test_template_dot:
    set user to {"name": "bob"}
    set msg to "hi {{user.name}}"
    respond with msg

to test_template_multiple:
    set a to "foo"
    set b to "bar"
    set msg to "{{a}} and {{b}}"
    respond with msg

// ── Function calls ───────────────────────────
to test_call_len_list:
    respond with len([1, 2, 3])

to test_call_len_string:
    respond with len("hello")

to test_call_len_map:
    respond with len({"a": 1, "b": 2})

to test_call_str:
    respond with str(42)

to test_call_int:
    respond with int(3.9)

to test_call_type:
    respond with type("hello")

to test_call_keys:
    set m to {"x": 1, "y": 2}
    respond with keys(m)

to test_call_values:
    set m to {"x": 10, "y": 20}
    respond with values(m)

// ── Parenthesized expressions ────────────────
to test_parens:
    respond with (2 + 3) * 4

// ── Identifier as variable ───────────────────
to test_ident:
    set my_var to 99
    respond with my_var
