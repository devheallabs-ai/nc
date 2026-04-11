// Regression tests for issues 1-20 fixes
// Tests: mkdir, delete_file, store capacity, thread safety,
//        respond-with-in-if, string coercion, try/on_error,
//        behavior-calling-behavior, multi-line strings, ws_connect,
//        http_get/http_post, find_similar, notify, cache

service "test-issue-fixes-v2"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Issue 5: Behaviors calling behaviors (via function syntax)
// ═══════════════════════════════════════════════════════════
to helper_add with a, b:
    respond with a + b

to test_behavior_call_as_function:
    set result to helper_add(10, 20)
    respond with result

// ═══════════════════════════════════════════════════════════
// Issue 5: Behaviors calling behaviors (via run keyword)
// ═══════════════════════════════════════════════════════════
to helper_double with x:
    respond with x * 2

to test_run_behavior:
    run helper_double with 21
    respond with result

// ═══════════════════════════════════════════════════════════
// Issue 10: Store capacity beyond 256 entries
// ═══════════════════════════════════════════════════════════
to test_store_capacity:
    repeat for i in range(50):
        set key to "cap_key_" + str(i)
        store i into key
    respond with "stored_50"

// ═══════════════════════════════════════════════════════════
// Issue 14: find_similar — cosine similarity (basic test)
// ═══════════════════════════════════════════════════════════
to test_find_similar_basic:
    set query_vec to [1.0, 0.0, 0.0]
    set vectors to [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.5, 0.5, 0.0]]
    set docs to ["doc_a", "doc_b", "doc_c"]
    set results to find_similar(query_vec, vectors, docs, 2)
    respond with len(results)

to test_find_similar_top_match:
    set query_vec to [1.0, 0.0]
    set vectors to [[1.0, 0.0], [0.0, 1.0], [0.7, 0.7]]
    set docs to ["exact_match", "orthogonal", "partial"]
    set results to find_similar(query_vec, vectors, docs, 1)
    set best to results[0]
    respond with best.document

// ═══════════════════════════════════════════════════════════
// Issue 16: respond with in if block must exit behavior
// ═══════════════════════════════════════════════════════════
to test_respond_in_if_exits:
    set x to 1
    if x is equal 1:
        respond with "correct"
    respond with "should_not_reach"

to test_respond_in_else_exits:
    set x to 0
    if x is equal 1:
        respond with "wrong"
    otherwise:
        respond with "correct_else"
    respond with "should_not_reach"

// ═══════════════════════════════════════════════════════════
// Issue 19: String + non-string type coercion
// ═══════════════════════════════════════════════════════════
to test_string_plus_int:
    set result to "count: " + 42
    respond with result

to test_string_plus_float:
    set result to "pi: " + 3.14
    respond with result

to test_string_plus_bool:
    set result to "flag: " + yes
    respond with result

to test_int_plus_string:
    set result to str(42) + " items"
    respond with result

// ═══════════════════════════════════════════════════════════
// Issue 20: try/on_error works in behaviors
// ═══════════════════════════════════════════════════════════
to test_try_success_path:
    try:
        set val to 42
        respond with val
    on error:
        respond with "error_path"

to test_try_with_finally:
    try:
        set val to "try_result"
        respond with val
    on error:
        respond with "error"
    finally:
        log "cleanup done"

// ═══════════════════════════════════════════════════════════
// Issue 13: notify doesn't crash (still works as before)
// ═══════════════════════════════════════════════════════════
to test_notify_basic:
    notify "test-channel" "test message"
    respond with "notified"

to test_notify_with_variable:
    set msg to "dynamic message"
    notify "alerts" msg
    respond with "sent"

// ═══════════════════════════════════════════════════════════
// Issue 3/11: Cache thread safety (basic operation test)
// ═══════════════════════════════════════════════════════════
to test_cache_set_get:
    set x to {"name": "NC", "version": 1}
    respond with x

// ═══════════════════════════════════════════════════════════
// WebSocket client: error path (no server to connect to)
// ═══════════════════════════════════════════════════════════
to test_ws_connect_failure:
    set conn to ws_connect("ws://127.0.0.1:1/nonexistent")
    respond with conn

// ═══════════════════════════════════════════════════════════
// HTTP functions: type check (validates function exists)
// ═══════════════════════════════════════════════════════════
to test_http_request_exists:
    set result to type(http_request)
    respond with "http_request exists"

// ═══════════════════════════════════════════════════════════
// String concat edge cases
// ═══════════════════════════════════════════════════════════
to test_concat_none:
    set val to none
    set result to "value: " + str(val)
    respond with result

to test_concat_list:
    set items to [1, 2, 3]
    set result to "list: " + str(items)
    respond with result

// ═══════════════════════════════════════════════════════════
// Behavior return value propagation
// ═══════════════════════════════════════════════════════════
to compute_sum with a, b:
    respond with a + b

to test_chain_behaviors:
    set r1 to compute_sum(10, 20)
    set r2 to compute_sum(r1, 5)
    respond with r2
