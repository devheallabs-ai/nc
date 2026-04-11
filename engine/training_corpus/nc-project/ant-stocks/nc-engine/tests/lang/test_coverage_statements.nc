// Coverage: All statement types
// Tests every NC statement node the compiler/VM handles

service "test-coverage-statements"
version "1.0.0"

// ── set / respond ────────────────────────────
to test_set_and_respond:
    set x to 42
    respond with x

to test_set_string:
    set name to "hello"
    respond with name

to test_set_field:
    set person to {"name": "alice", "age": 25}
    set person.age to 30
    respond with person.age

// ── log / show ───────────────────────────────
to test_log:
    set val to "logged"
    log "this is {{val}}"
    respond with "ok"

to test_show:
    show "visible"
    respond with "ok"

// ── emit ─────────────────────────────────────
to test_emit:
    emit "test_event"
    respond with "emitted"

// ── store ────────────────────────────────────
to test_store:
    set data to {"key": "value"}
    store data into "test_store_key"
    respond with "stored"

// ── notify ───────────────────────────────────
to test_notify:
    notify "channel" "hello there"
    respond with "notified"

// ── append ───────────────────────────────────
to test_append:
    set items to []
    append "first" to items
    append "second" to items
    append "third" to items
    respond with len(items)

// ── find in loop ─────────────────────────────
to test_find_in_loop:
    set found to "none"
    repeat for each i in [1, 2, 3, 4, 5]:
        if i is equal 3:
            set found to i
    respond with found

// ── conditional accumulate ───────────────────
to test_conditional_accumulate:
    set count to 0
    repeat for each i in range(1, 6):
        if i is not equal 3:
            set count to count + 1
    respond with count

// ── expression statement (function call as statement) ──
to test_expr_stmt:
    set items to [3, 1, 2]
    sort(items)
    respond with "ok"
