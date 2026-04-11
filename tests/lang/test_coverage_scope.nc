// Coverage: Scope, behavior calls, parameter binding
// Tests variable scope isolation and behavior composition

service "test-coverage-scope"
version "1.0.0"

// ── Basic parameter binding ──────────────────
to echo with msg:
    respond with msg

to test_single_param:
    run echo with "hello"
    respond with result

// ── Multiple parameters ──────────────────────
to format_name with first and last:
    respond with first + " " + last

to test_multi_params:
    run format_name with "John", "Doe"
    respond with result

// ── Run with variable args ───────────────────
to add with a and b:
    respond with a + b

to test_run_variable_args:
    set x to 10
    set y to 20
    run add with x, y
    respond with result

// ── Behavior using set + respond ─────────────
to compute_square with n:
    set sq to n * n
    respond with sq

to test_run_with_internal_state:
    run compute_square with 7
    respond with result

// ── Chained behavior calls ───────────────────
to increment with n:
    respond with n + 1

to test_chain_runs:
    run increment with 0
    set r1 to result
    run increment with r1
    set r2 to result
    run increment with r2
    respond with result

// ── Behavior returning map ───────────────────
to make_user with name and age:
    respond with {"name": name, "age": age}

to test_run_returns_map:
    run make_user with "alice", 30
    respond with result.name

// ── Behavior with conditional ────────────────
to classify with score:
    if score is at least 90:
        respond with "A"
    if score is at least 80:
        respond with "B"
    if score is at least 70:
        respond with "C"
    respond with "F"

to test_run_with_conditional:
    run classify with 85
    respond with result

// ── Scope isolation ──────────────────────────
to inner_behavior:
    set local_var to "inside"
    respond with local_var

to test_scope_isolation:
    set local_var to "outside"
    run inner_behavior
    respond with local_var

// ── Behavior with loop ───────────────────────
to sum_list with numbers:
    set total to 0
    repeat for each n in numbers:
        set total to total + n
    respond with total

to test_run_with_loop:
    run sum_list with [1, 2, 3, 4, 5]
    respond with result

// ── Behavior calling behavior ────────────────
to double with n:
    respond with n * 2

to quadruple with n:
    run double with n
    set doubled to result
    run double with doubled
    respond with result

to test_nested_run:
    run quadruple with 5
    respond with result

// ── Define types ─────────────────────────────
define User as:
    name is text
    age is number
    active is yesno optional

to test_define_exists:
    respond with "types defined"
