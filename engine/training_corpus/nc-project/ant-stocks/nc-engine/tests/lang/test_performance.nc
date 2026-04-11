// Performance Test Suite for NC
// Validates that core operations complete within expected bounds.
// Uses time_ms() to measure actual elapsed time — no shell needed.
// All tests are self-contained and cross-platform.

service "test-performance"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Arithmetic — 10,000 iterations
// ═══════════════════════════════════════════════════════════

to test_perf_arithmetic:
    set start to time_ms()
    set result to 0
    set i to 0
    while i is below 10000:
        set result to result + i * 2
        set i to i + 1
    set elapsed to time_ms() - start
    if elapsed is below 5000:
        respond with "arith_ok"
    respond with "arith_slow"

// ═══════════════════════════════════════════════════════════
// String concatenation — 500 iterations
// ═══════════════════════════════════════════════════════════

to test_perf_string_concat:
    set start to time_ms()
    set result to ""
    set i to 0
    while i is below 500:
        set result to result + "x"
        set i to i + 1
    set elapsed to time_ms() - start
    if len(result) is equal 500 and elapsed is below 5000:
        respond with "string_ok"
    respond with "string_slow"

// ═══════════════════════════════════════════════════════════
// List append — 1,000 items
// ═══════════════════════════════════════════════════════════

to test_perf_list_append:
    set start to time_ms()
    set items to []
    set i to 0
    while i is below 1000:
        append i to items
        set i to i + 1
    set elapsed to time_ms() - start
    if len(items) is equal 1000 and elapsed is below 5000:
        respond with "list_ok"
    respond with "list_slow"

// ═══════════════════════════════════════════════════════════
// Map operations — 200 key-value pairs
// ═══════════════════════════════════════════════════════════

to test_perf_map_ops:
    set start to time_ms()
    set i to 0
    while i is below 200:
        set data to {"index": i, "value": i * 10, "label": "item"}
        set v to data.index + data.value
        set i to i + 1
    set elapsed to time_ms() - start
    if i is equal 200 and elapsed is below 5000:
        respond with "map_ok"
    respond with "map_slow"

// ═══════════════════════════════════════════════════════════
// Nested loops — 100 x 100
// ═══════════════════════════════════════════════════════════

to test_perf_nested_loops:
    set start to time_ms()
    set total to 0
    set i to 0
    while i is below 100:
        set j to 0
        while j is below 100:
            set total to total + 1
            set j to j + 1
        set i to i + 1
    set elapsed to time_ms() - start
    if total is equal 10000 and elapsed is below 5000:
        respond with "nested_ok"
    respond with "nested_slow"

// ═══════════════════════════════════════════════════════════
// Conditionals — 5,000 if-checks
// ═══════════════════════════════════════════════════════════

to test_perf_conditionals:
    set start to time_ms()
    set evens to 0
    set i to 0
    while i is below 5000:
        if i % 2 is equal 0:
            set evens to evens + 1
        set i to i + 1
    set elapsed to time_ms() - start
    if evens is equal 2500 and elapsed is below 5000:
        respond with "cond_ok"
    respond with "cond_slow"

// ═══════════════════════════════════════════════════════════
// Function calls — 1,000 behavior invocations
// ═══════════════════════════════════════════════════════════

to perf_helper with x:
    respond with x + 1

to test_perf_function_calls:
    set start to time_ms()
    set total to 0
    set i to 0
    while i is below 1000:
        set total to perf_helper(total)
        set i to i + 1
    set elapsed to time_ms() - start
    if total is equal 1000 and elapsed is below 5000:
        respond with "func_ok"
    respond with "func_slow"

// ═══════════════════════════════════════════════════════════
// JSON encode/decode — 500 roundtrips
// ═══════════════════════════════════════════════════════════

to test_perf_json:
    set start to time_ms()
    set i to 0
    while i is below 500:
        set data to {"index": i, "name": "item", "active": true}
        set encoded to json_encode(data)
        set decoded to json_decode(encoded)
        set i to i + 1
    set elapsed to time_ms() - start
    if elapsed is below 5000:
        respond with "json_ok"
    respond with "json_slow"

// ═══════════════════════════════════════════════════════════
// String split/join — 500 iterations
// ═══════════════════════════════════════════════════════════

to test_perf_split_join:
    set start to time_ms()
    set i to 0
    while i is below 500:
        set parts to split("one,two,three,four,five", ",")
        set joined to join(parts, "-")
        set i to i + 1
    set elapsed to time_ms() - start
    if elapsed is below 5000:
        respond with "split_join_ok"
    respond with "split_join_slow"

// ═══════════════════════════════════════════════════════════
// File I/O — 100 write+read cycles
// ═══════════════════════════════════════════════════════════

to test_perf_file_io:
    set start to time_ms()
    set path to "perf_test_tmp.txt"
    set i to 0
    while i is below 100:
        write_file(path, "data_" + str(i))
        set content to read_file(path)
        set i to i + 1
    delete_file(path)
    set elapsed to time_ms() - start
    if elapsed is below 10000:
        respond with "io_ok"
    respond with "io_slow"

// ═══════════════════════════════════════════════════════════
// Cache — 500 set+get cycles
// ═══════════════════════════════════════════════════════════

to test_perf_cache:
    set start to time_ms()
    set i to 0
    while i is below 500:
        cache("perf_" + str(i), i * 10)
        set val to cached("perf_" + str(i))
        set i to i + 1
    set elapsed to time_ms() - start
    if elapsed is below 5000:
        respond with "cache_ok"
    respond with "cache_slow"
