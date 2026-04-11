// Coverage: Parser edge cases
// Tests tricky syntax that the parser must handle correctly

service "test-coverage-parser"
version "1.0.0"
description "Parser coverage tests"

configure:
    port: 8080
    debug: true

// ── Service metadata parsed ──────────────────
to test_metadata:
    respond with "metadata parsed"

// ── Behavior with many params ────────────────
to many_params with a and b and c and d:
    respond with a + b + c + d

to test_many_params:
    run many_params with 1, 2, 3, 4
    respond with result

// ── Behavior with purpose ────────────────────
to documented_behavior with input:
    purpose: "This behavior has a purpose string"
    respond with input

to test_purpose:
    run documented_behavior with "works"
    respond with result

// ── Map with string keys (quoted) ────────────
to test_quoted_map_keys:
    set m to {"first name": "alice", "last name": "smith"}
    respond with len(m)

// ── Multiline map ────────────────────────────
to test_multiline_map:
    set config to {
        "host": "localhost",
        "port": 8080,
        "debug": true
    }
    respond with config.host

// ── Multiline list ───────────────────────────
to test_multiline_list:
    set items to [
        "alpha",
        "beta",
        "gamma",
        "delta"
    ]
    respond with len(items)

// ── Trailing comma in list ───────────────────
to test_trailing_comma_list:
    set items to [1, 2, 3,]
    respond with len(items)

// ── Trailing comma in map ────────────────────
to test_trailing_comma_map:
    set m to {"a": 1, "b": 2,}
    respond with len(m)

// ── Nested map in list ───────────────────────
to test_nested_structures:
    set data to [
        {"users": [
            {"name": "alice"},
            {"name": "bob"}
        ]},
        {"users": [
            {"name": "charlie"}
        ]}
    ]
    respond with len(data)

// ── Empty behavior ───────────────────────────
to empty_behavior:
    respond with nothing

to test_empty_behavior:
    run empty_behavior
    respond with "ok"

// ── Boolean literals ─────────────────────────
to test_yes_no:
    set a to yes
    set b to no
    if a:
        respond with "yes works"

// ── None literal variants ────────────────────
to test_nothing:
    set x to nothing
    respond with type(x)

// ── String escape sequences ──────────────────
to test_escape_sequences:
    set s to "tab:\there"
    respond with type(s)

// ── Comparison operators full ────────────────
to test_greater_than:
    respond with 10 is greater than 5

to test_less_than:
    respond with 3 is less than 7

// ── API route declarations ───────────────────
api:
    GET /health runs test_metadata
    POST /test runs test_metadata
    PUT /update runs test_metadata
    DELETE /remove runs test_metadata
    PATCH /modify runs test_metadata
