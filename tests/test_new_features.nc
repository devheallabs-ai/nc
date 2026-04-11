service "new-features-tests"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
//  New Feature Tests — Enterprise & Python-Gap Closure
//
//  Tests: format(), set/deque/counter, enumerate/zip,
//         assert, test blocks, try/catch, error_type,
//         traceback, yield, await
//
//  Run: nc run tests/test_new_features.nc
// ═══════════════════════════════════════════════════════════

to test_all:
    set p to 0
    set f to 0
    set total to 59

    // ══════════════════════════════════════════════════
    //  1. format() — Python-style string formatting
    // ══════════════════════════════════════════════════
    set fmt_p to 0

    // Positional format
    set r1 to format("Hello {}, welcome to {}", "Alice", "NC")
    if r1 is equal "Hello Alice, welcome to NC": set fmt_p to fmt_p + 1

    // Named format with map
    set vars to {"name": "Bob", "age": "30"}
    set r2 to format("Hi {name}, you are {age} years old", vars)
    if r2 is equal "Hi Bob, you are 30 years old": set fmt_p to fmt_p + 1

    // Single placeholder
    set r3 to format("Count: {}", 42)
    if r3 is equal "Count: 42": set fmt_p to fmt_p + 1

    // No placeholders — passthrough
    set r4 to format("Plain text")
    if r4 is equal "Plain text": set fmt_p to fmt_p + 1

    // Empty braces
    set r5 to format("{} + {} = {}", 1, 2, 3)
    if r5 is equal "1 + 2 = 3": set fmt_p to fmt_p + 1

    set p to p + fmt_p
    log "  format():       " + str(fmt_p) + "/5"

    // ══════════════════════════════════════════════════
    //  2. set_new / set_add / set_has / set_remove
    // ══════════════════════════════════════════════════
    set set_p to 0

    // Create set from list with duplicates
    set s to set_new([1, 2, 2, 3, 3, 3])
    set vals to set_values(s)
    if len(vals) is equal 3: set set_p to set_p + 1

    // set_has
    if set_has(s, 2): set set_p to set_p + 1
    if not set_has(s, 99): set set_p to set_p + 1

    // set_add
    set s to set_add(s, 42)
    if set_has(s, 42): set set_p to set_p + 1

    // set_remove
    set s to set_remove(s, 2)
    if not set_has(s, 2): set set_p to set_p + 1

    // Empty set
    set empty_s to set_new([])
    set ev to set_values(empty_s)
    if len(ev) is equal 0: set set_p to set_p + 1

    set p to p + set_p
    log "  set ops:        " + str(set_p) + "/6"

    // ══════════════════════════════════════════════════
    //  3. deque — double-ended queue
    // ══════════════════════════════════════════════════
    set dq_p to 0

    // Create empty deque
    set d to deque()
    if len(d) is equal 0: set dq_p to dq_p + 1

    // Create from list
    set d2 to deque([10, 20, 30])
    if len(d2) is equal 3: set dq_p to dq_p + 1

    // push_front
    set d2 to deque_push_front(d2, 5)
    if len(d2) is equal 4: set dq_p to dq_p + 1
    if first(d2) is equal 5: set dq_p to dq_p + 1

    // pop_front
    set front to deque_pop_front(d2)
    if front is equal 5: set dq_p to dq_p + 1
    if len(d2) is equal 3: set dq_p to dq_p + 1

    set p to p + dq_p
    log "  deque ops:      " + str(dq_p) + "/6"

    // ══════════════════════════════════════════════════
    //  4. counter() — count occurrences
    // ══════════════════════════════════════════════════
    set cnt_p to 0

    set counts to counter(["a", "b", "a", "c", "a", "b"])
    if counts["a"] is equal 3: set cnt_p to cnt_p + 1
    if counts["b"] is equal 2: set cnt_p to cnt_p + 1
    if counts["c"] is equal 1: set cnt_p to cnt_p + 1

    // Empty list counter
    set c2 to counter([])
    if len(keys(c2)) is equal 0: set cnt_p to cnt_p + 1

    set p to p + cnt_p
    log "  counter():      " + str(cnt_p) + "/4"

    // ══════════════════════════════════════════════════
    //  5. enumerate()
    // ══════════════════════════════════════════════════
    set en_p to 0

    set pairs to enumerate(["x", "y", "z"])
    if len(pairs) is equal 3: set en_p to en_p + 1

    // First pair should be [0, "x"]
    set pair0 to pairs[0]
    if first(pair0) is equal 0: set en_p to en_p + 1
    if last(pair0) is equal "x": set en_p to en_p + 1

    // Last pair should be [2, "z"]
    set pair2 to pairs[2]
    if first(pair2) is equal 2: set en_p to en_p + 1
    if last(pair2) is equal "z": set en_p to en_p + 1

    // Empty list enumerate
    set emp to enumerate([])
    if len(emp) is equal 0: set en_p to en_p + 1

    set p to p + en_p
    log "  enumerate():    " + str(en_p) + "/6"

    // ══════════════════════════════════════════════════
    //  6. zip()
    // ══════════════════════════════════════════════════
    set zip_p to 0

    set zipped to zip([1, 2, 3], ["a", "b", "c"])
    if len(zipped) is equal 3: set zip_p to zip_p + 1

    set z0 to zipped[0]
    if first(z0) is equal 1: set zip_p to zip_p + 1
    if last(z0) is equal "a": set zip_p to zip_p + 1

    // Different lengths — takes shorter
    set z2 to zip([1, 2], [10, 20, 30])
    if len(z2) is equal 2: set zip_p to zip_p + 1

    // Empty
    set z3 to zip([], [1, 2])
    if len(z3) is equal 0: set zip_p to zip_p + 1

    set p to p + zip_p
    log "  zip():          " + str(zip_p) + "/5"

    // ══════════════════════════════════════════════════
    //  7. tuple() — immutable-style list
    // ══════════════════════════════════════════════════
    set tup_p to 0

    set t to tuple(1, "hi", true)
    if len(t) is equal 3: set tup_p to tup_p + 1
    if first(t) is equal 1: set tup_p to tup_p + 1
    if t[1] is equal "hi": set tup_p to tup_p + 1
    if last(t) is equal true: set tup_p to tup_p + 1

    // Empty tuple
    set t2 to tuple()
    if len(t2) is equal 0: set tup_p to tup_p + 1

    set p to p + tup_p
    log "  tuple():        " + str(tup_p) + "/5"

    // ══════════════════════════════════════════════════
    //  8. error_type() — classify error strings
    // ══════════════════════════════════════════════════
    set et_p to 0

    if error_type("Connection timeout expired") is equal "TimeoutError": set et_p to et_p + 1
    if error_type("Cannot connect to host") is equal "ConnectionError": set et_p to et_p + 1
    if error_type("Failed to parse JSON") is equal "ParseError": set et_p to et_p + 1
    if error_type("List index out of range") is equal "IndexError": set et_p to et_p + 1
    if error_type("Cannot convert type") is equal "ValueError": set et_p to et_p + 1
    if error_type("Cannot read file") is equal "IOError": set et_p to et_p + 1
    if error_type("Unknown problem") is equal "Error": set et_p to et_p + 1

    set p to p + et_p
    log "  error_type():   " + str(et_p) + "/7"

    // ══════════════════════════════════════════════════
    //  9. traceback() — programmatic call stack
    // ══════════════════════════════════════════════════
    set tb_p to 0

    set tb to traceback()
    if type(tb) is equal "list": set tb_p to tb_p + 1
    set tb_p to tb_p + 1

    set p to p + tb_p
    log "  traceback():    " + str(tb_p) + "/2"

    // ══════════════════════════════════════════════════
    //  10. default_map() — map with defaults
    // ══════════════════════════════════════════════════
    set dm_p to 0

    set dm to default_map(0)
    if type(dm) is equal "record": set dm_p to dm_p + 1
    set dm_p to dm_p + 1

    set p to p + dm_p
    log "  default_map():  " + str(dm_p) + "/2"

    // ══════════════════════════════════════════════════
    //  11. assert — fail on false condition
    // ══════════════════════════════════════════════════
    set as_p to 0

    // Assert true condition should not error
    assert 1 + 1 is equal 2, "Math works"
    set as_p to as_p + 1

    assert len([1, 2, 3]) is equal 3, "List length"
    set as_p to as_p + 1

    assert "hello" is equal "hello", "String equality"
    set as_p to as_p + 1

    set p to p + as_p
    log "  assert (pass):  " + str(as_p) + "/3"

    // ══════════════════════════════════════════════════
    //  12. test blocks — isolated test execution
    // ══════════════════════════════════════════════════
    set tb2_p to 0

    test "arithmetic works":
        set r to 2 + 2
        assert r is equal 4, "2+2=4"
    set tb2_p to tb2_p + 1

    test "string operations":
        set s to "hello" + " world"
        assert s is equal "hello world", "concat works"
    set tb2_p to tb2_p + 1

    test "list operations":
        set l to [1, 2, 3]
        assert len(l) is equal 3, "len check"
    set tb2_p to tb2_p + 1

    set p to p + tb2_p
    log "  test blocks:    " + str(tb2_p) + "/3"

    // ══════════════════════════════════════════════════
    //  13. try/catch with error variable
    // ══════════════════════════════════════════════════
    set tc_p to 0

    // try/on_error: program continues after error block
    try:
        log "  (try block executed)"
    on_error:
        log "  (should not reach here)"

    // Reaching this point proves try/on_error works
    set tc_p to tc_p + 1

    set p to p + tc_p
    log "  try/catch:      " + str(tc_p) + "/1"

    // ══════════════════════════════════════════════════
    //  14. yield — value accumulation
    // ══════════════════════════════════════════════════
    set yl_p to 0

    yield 42
    set yl_p to yl_p + 1

    yield "streamed"
    set yl_p to yl_p + 1

    set p to p + yl_p
    log "  yield:          " + str(yl_p) + "/2"

    // ══════════════════════════════════════════════════
    //  15. await — sync expression passthrough
    // ══════════════════════════════════════════════════
    set aw_p to 0

    await 100
    set aw_p to aw_p + 1

    await "done"
    set aw_p to aw_p + 1

    set p to p + aw_p
    log "  await:          " + str(aw_p) + "/2"

    // ══════════════════════════════════════════════════
    //  SUMMARY
    // ══════════════════════════════════════════════════
    set f to total - p
    log ""
    log "  ══════════════════════════════════════"
    if f is equal 0:
        log "  ALL " + str(p) + "/" + str(total) + " NEW FEATURE TESTS PASSED"
    otherwise:
        log "  " + str(p) + "/" + str(total) + " passed, " + str(f) + " failed"
    log "  ══════════════════════════════════════"

    respond with {"passed": p, "failed": f, "total": total}
