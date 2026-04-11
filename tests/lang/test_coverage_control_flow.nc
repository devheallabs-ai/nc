// Coverage: Control flow — if, repeat, match, try, run

service "test-coverage-control-flow"
version "1.0.0"

to test_if_true:
    if true:
        respond with "yes"

to test_if_false_otherwise:
    if false:
        respond with "wrong"
    otherwise:
        respond with "correct"

to test_if_above:
    set x to 10
    if x is above 5:
        respond with "big"

to test_nested_if:
    set x to 15
    if x is above 10:
        if x is below 20:
            respond with "mid"

to test_if_not:
    if not false:
        respond with "negated"

to test_if_and:
    set a to 5
    if a is above 3 and a is below 10:
        respond with "in range"

to test_if_or:
    set x to 15
    if x is below 5 or x is above 10:
        respond with "out"

to test_repeat_list:
    set r to ""
    repeat for each item in ["a", "b", "c"]:
        set r to r + item
    respond with r

to test_repeat_range:
    set s to 0
    repeat for each i in range(5):
        set s to s + i
    respond with s

to test_repeat_nested:
    set c to 0
    repeat for each i in [1, 2]:
        repeat for each j in [10, 20]:
            set c to c + 1
    respond with c

to test_repeat_accumulate:
    set s to 0
    repeat for each i in range(1, 11):
        set s to s + i
    respond with s

to test_repeat_find:
    set found to "none"
    repeat for each x in [1, 2, 3, 4, 5]:
        if x is equal 3:
            set found to x
    respond with found

to test_match_string:
    set c to "green"
    match c:
        when "red":
            respond with "stop"
        when "green":
            respond with "go"

to test_match_number:
    set code to 200
    match code:
        when 200:
            respond with "ok"
        when 404:
            respond with "missing"

to test_match_otherwise:
    set v to "unknown"
    match v:
        when "a":
            respond with "a"
        otherwise:
            respond with "default"

to test_match_dot_access:
    set action to {"type": "deploy"}
    match action.type:
        when "deploy":
            respond with "deploying"
        otherwise:
            respond with "other"

to test_try_success:
    try:
        set x to 42
        respond with x
    on error:
        respond with "err"

to test_try_underscore:
    try:
        respond with 100
    on_error:
        respond with "err"

to helper_add with a and b:
    respond with a + b

to test_run_basic:
    run helper_add with 10, 20
    respond with result

to helper_greet with name:
    respond with "hi " + name

to test_run_chain:
    run helper_add with 5, 3
    set r1 to result
    run helper_add with r1, 10
    respond with result

// ── while ────────────────────────────────────
to test_while_basic:
    set i to 0
    while i is below 5:
        set i to i + 1
    respond with i

to test_while_accumulate:
    set sum to 0
    set i to 1
    while i is at most 10:
        set sum to sum + i
        set i to i + 1
    respond with sum

to test_while_respond_exits:
    set i to 0
    while i is below 100:
        set i to i + 1
        if i is equal 3:
            respond with i
    respond with i

// ── repeat N times ───────────────────────────
to test_repeat_times:
    set count to 0
    repeat 5 times:
        set count to count + 1
    respond with count

to test_repeat_times_ten:
    set sum to 0
    repeat 10 times:
        set sum to sum + 1
    respond with sum
