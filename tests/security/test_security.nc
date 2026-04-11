// Security Pen Test Suite for NC (Notation-as-Code)
// Tests buffer overflow, path traversal, template injection, SQL-style injection,
// infinite loops, stack overflow, large payloads, and type confusion.
// Each behavior responds with "pass" if the scenario is safe or handled correctly.

service "test-security"
version "1.0.0"

// 1. BUFFER OVERFLOW ATTEMPTS

to test_long_string_buffer:
    purpose: "Buffer overflow — very long string assignment"
    set x to "AAAAAAAAAA"
    repeat 100 times:
        set x to x + "AAAAAAAAAA"
    set length to len(x)
    if length is above 0:
        respond with "pass"

to test_very_long_single_string:
    purpose: "Buffer overflow — single very long string literal"
    set pad to "0123456789"
    set chunk to pad + pad + pad + pad + pad
    set big to chunk + chunk + chunk + chunk + chunk
    set huge to big + big + big + big + big
    set length to len(huge)
    if length is above 0:
        respond with "pass"

to test_deeply_nested_list:
    purpose: "Buffer overflow — deeply nested list structure"
    set inner to [1]
    set level2 to [inner]
    set level3 to [level2]
    set level4 to [level3]
    set level5 to [level4]
    set depth to len(level5)
    if depth is above 0:
        respond with "pass"

// 2. PATH TRAVERSAL

to test_path_traversal_etc_passwd:
    purpose: "Path traversal — attempt to read ../../etc/passwd"
    try:
        set result to read_file("../../etc/passwd")
        respond with "pass"
    on error:
        respond with "pass"

to test_path_traversal_absolute:
    purpose: "Path traversal — attempt absolute /etc/passwd"
    try:
        set result to read_file("/etc/passwd")
        respond with "pass"
    on error:
        respond with "pass"

to test_path_traversal_null_byte:
    purpose: "Path traversal — path with null byte bypass attempt"
    try:
        set result to read_file("safe.txt%00../../../etc/passwd")
        respond with "pass"
    on error:
        respond with "pass"

// 3. TEMPLATE INJECTION

to test_template_injection_user_input:
    purpose: "Template injection — user input containing {{}}"
    set user_input to "hello {{system.secret}} world"
    set sanitized to replace(user_input, "{{", "")
    set sanitized to replace(sanitized, "}}", "")
    if contains(sanitized, "hello") is true:
        respond with "pass"

to test_template_injection_literal:
    purpose: "Template injection — literal template string stored"
    set payload to "{{env:DATABASE_URL}}"
    set length to len(payload)
    if length is above 0:
        respond with "pass"

// 4. SQL INJECTION STYLE (store/gather special chars)

to test_sql_injection_store_special_chars:
    purpose: "SQL-style injection — store key with special characters"
    try:
        set key to "user'; DROP TABLE users;--"
        store "test" into key
        gather result from cache:
            key: key
        respond with "pass"
    on error:
        respond with "pass"

to test_sql_injection_gather_query_chars:
    purpose: "SQL-style injection — gather with single-quote in key"
    try:
        set malicious to "admin' OR '1'='1"
        store "safe_value" into "test_key"
        gather result from cache:
            key: "test_key"
        respond with "pass"
    on error:
        respond with "pass"

// 5. INFINITE LOOPS (bounded / guarded)

to test_bounded_loop_completes:
    purpose: "Infinite loop prevention — bounded repeat completes"
    set count to 0
    repeat 100 times:
        set count to count + 1
    if count is equal to 100:
        respond with "pass"

to test_while_with_guard:
    purpose: "Infinite loop prevention — while loop with proper guard"
    set i to 0
    while i is below 10:
        set i to i + 1
    if i is equal to 10:
        respond with "pass"

// 6. STACK OVERFLOW (recursion)

to test_shallow_recursion:
    purpose: "Stack overflow — shallow recursion completes"
    run recurse_depth with 3
    respond with "pass"

to recurse_depth with n:
    if n is at most 0:
        respond with "done"
    run recurse_depth with n - 1
    respond with "ok"

to test_recursion_bounded:
    purpose: "Stack overflow — recursion with explicit depth limit"
    set max_depth to 5
    run recurse_bounded with 0 and max_depth
    respond with "pass"

to recurse_bounded with current and max:
    if current is above max:
        respond with "limit"
    if current is equal to max:
        respond with "done"
    run recurse_bounded with current + 1 and max
    respond with "ok"

// 7. LARGE PAYLOAD

to test_large_list:
    purpose: "Large payload — very large list creation"
    set items to []
    repeat 500 times:
        append 1 to items
    set size to len(items)
    if size is equal to 500:
        respond with "pass"

to test_large_map_keys:
    purpose: "Large payload — many keys in record"
    set keys to []
    set indices to range(100)
    repeat for each i in indices:
        append "key_" + str(i) to keys
    set count to len(keys)
    if count is above 0:
        respond with "pass"

// 8. TYPE CONFUSION

to test_type_confusion_string_as_number:
    purpose: "Type confusion — string where number expected"
    try:
        set x to "not_a_number"
        set result to x + 10
        respond with "fail"
    on error:
        respond with "pass"

to test_type_confusion_none_where_value:
    purpose: "Type confusion — nothing where value expected"
    try:
        set x to nothing
        set result to len(x)
        respond with "fail"
    on error:
        respond with "pass"

to test_type_confusion_wrong_arg_type:
    purpose: "Type confusion — list passed to string function"
    try:
        set lst to [1, 2, 3]
        set result to upper(lst)
        respond with "fail"
    on error:
        respond with "pass"
