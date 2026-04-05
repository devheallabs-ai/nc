// NC Standard Library — unittest
// Testing framework

service "nc.unittest"
version "1.0.0"
description "Write and run tests for NC programs"

to assert_equal with actual and expected:
    purpose: "Check that two values are equal"
    if actual is equal to expected:
        log "PASS"
        respond with true
    log "FAIL: expected " + str(expected) + " but got " + str(actual)
    respond with false

to assert_true with value:
    purpose: "Check that a value is true"
    if value is equal to true:
        log "PASS"
        respond with true
    log "FAIL: expected true"
    respond with false

to assert_false with value:
    purpose: "Check that a value is false"
    if value is equal to false:
        log "PASS"
        respond with true
    log "FAIL: expected false"
    respond with false

to assert_above with value and threshold:
    purpose: "Check that a value is above a threshold"
    if value is above threshold:
        log "PASS"
        respond with true
    log "FAIL: " + str(value) + " is not above " + str(threshold)
    respond with false

to assert_not_empty with value:
    purpose: "Check that a value is not empty"
    if value is equal to "":
        log "FAIL: value is empty"
        respond with false
    if value is equal to nothing:
        log "FAIL: value is nothing"
        respond with false
    log "PASS"
    respond with true

to run_test with name and result:
    purpose: "Record a test result"
    if result is equal to true:
        log "  PASS  " + name
    otherwise:
        log "  FAIL  " + name
    respond with result
