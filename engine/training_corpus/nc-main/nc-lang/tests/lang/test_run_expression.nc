// Regression test: run keyword inside expressions
// Ensures behaviors can be called in expression positions such as
// assignments, responses, and later chained use.

service "test-run-expression"
version "1.0.0"

to answer:
    respond with 21

to double with value:
    respond with value * 2

to make_status:
    respond with {"ready": true, "value": 42}

to test_run_expression_in_set:
    set value_out to run double with 21
    assert value_out is equal 42, "run expression should return a behavior value inside set"
    respond with value_out

to test_run_expression_in_respond:
    respond with run double with 21

to test_run_expression_with_map_result:
    set status to run make_status
    assert status.ready is equal true, "run expression should preserve returned map fields"
    respond with status.value

to test_run_expression_chaining:
    set base to run answer
    set doubled to run double with base
    assert doubled is equal 42, "run expression result should be reusable in later behavior calls"
    respond with doubled
