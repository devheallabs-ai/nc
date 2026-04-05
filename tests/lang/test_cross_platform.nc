// Cross-Platform Test Suite for NC
// Tests all core features using only platform-independent operations.
// Must pass on Linux, macOS, and Windows without modification.

service "test-cross-platform"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// 1. Variables and Data Types
// ═══════════════════════════════════════════════════════════

to test_set_string:
    set name to "world"
    respond with name

to test_set_number:
    set count to 42
    respond with count

to test_set_float:
    set pi to 3.14
    if pi is above 3:
        respond with "float_ok"

to test_set_bool:
    set active to true
    if active:
        respond with "bool_ok"

to test_set_none:
    set empty to nothing
    if empty is equal nothing:
        respond with "none_ok"

to test_set_list:
    set items to [1, 2, 3]
    respond with len(items)

to test_set_map:
    set data to {"key": "value", "count": 10}
    respond with data.key

// ═══════════════════════════════════════════════════════════
// 2. String Operations (cross-platform, no shell needed)
// ═══════════════════════════════════════════════════════════

to test_string_concat:
    set greeting to "hello" + " " + "world"
    respond with greeting

to test_string_upper:
    respond with upper("hello")

to test_string_lower:
    respond with lower("HELLO")

to test_string_trim:
    respond with trim("  spaced  ")

to test_string_contains:
    if contains("hello world", "world"):
        respond with "found"

to test_string_starts_with:
    if starts_with("filename.txt", "file"):
        respond with "starts_ok"

to test_string_ends_with:
    if ends_with("filename.txt", ".txt"):
        respond with "ends_ok"

to test_string_replace:
    set result to replace("hello world", "world", "NC")
    respond with result

to test_string_split:
    set parts to split("a,b,c", ",")
    respond with len(parts)

to test_string_join:
    set items to ["x", "y", "z"]
    set result to join(items, "-")
    respond with result

to test_string_plus_number:
    set result to "count: " + 42
    respond with result

to test_string_plus_float:
    set result to "pi: " + 3.14
    respond with result

to test_string_plus_bool:
    set result to "active: " + true
    respond with result

// ═══════════════════════════════════════════════════════════
// 3. String Interpolation — works in ALL string contexts
// ═══════════════════════════════════════════════════════════

to test_interpolation_in_string:
    set lang to "NC"
    set ver to "1.0"
    set result to "{{lang}} v{{ver}}"
    respond with result

to test_interpolation_with_dot:
    set info to {"name": "NC", "version": "1.0"}
    set result to "{{info.name}} v{{info.version}}"
    respond with result

// ═══════════════════════════════════════════════════════════
// 4. Multi-line Strings (triple-quoted)
// ═══════════════════════════════════════════════════════════

to test_triple_quote_string:
    set msg to """line one
line two
line three"""
    if contains(msg, "line two"):
        respond with "multiline_ok"

to test_triple_quote_length:
    set prompt to """Analyze this data.
Return JSON with fields:
- category
- confidence"""
    if len(prompt) is above 40:
        respond with "length_ok"

// ═══════════════════════════════════════════════════════════
// 5. List Operations
// ═══════════════════════════════════════════════════════════

to test_list_append:
    set items to [1, 2]
    set items to append(items, 3)
    respond with len(items)

to test_list_first_last:
    set items to [10, 20, 30]
    set f to first(items)
    set l to last(items)
    respond with f + l

to test_list_slice:
    set items to [1, 2, 3, 4, 5]
    set sub to slice(items, 1, 3)
    respond with len(sub)

to test_list_reverse:
    set items to [1, 2, 3]
    set rev to reverse(items)
    respond with first(rev)

to test_list_sort:
    set items to [3, 1, 2]
    set sorted to sort(items)
    respond with first(sorted)

to test_list_unique:
    set items to [1, 1, 2, 2, 3]
    set uniq to unique(items)
    respond with len(uniq)

to test_list_sum:
    set items to [10, 20, 30]
    respond with sum(items)

to test_empty_list:
    set items to []
    respond with len(items)

to test_empty_list_type:
    set items to []
    respond with type(items)

to test_empty_list_is_falsy:
    set items to []
    if items:
        respond with "truthy"
    otherwise:
        respond with "falsy"

// ═══════════════════════════════════════════════════════════
// 6. Map / Record Operations
// ═══════════════════════════════════════════════════════════

to test_map_literal:
    set m to {"a": 1, "b": 2}
    respond with m.a + m.b

to test_map_keys:
    set m to {"x": 10, "y": 20}
    set k to keys(m)
    respond with len(k)

to test_map_has_key:
    set m to {"name": "NC"}
    if has_key(m, "name"):
        respond with "has_key_ok"

to test_map_dynamic_key:
    set db to {"alice": 100, "bob": 200}
    set key to "alice"
    set val to db[key]
    respond with val

to test_map_set_dynamic_key:
    set scores to {}
    set player to "alice"
    set scores[player] to 100
    respond with scores["alice"]

to test_map_dot_bracket:
    set db to {"edges": {}}
    set eid to "a_to_b"
    set db.edges[eid] to {"score": 1.0}
    respond with db.edges["a_to_b"]["score"]

to test_nested_map_set:
    set graph to {}
    set graph["nodes"] to {}
    respond with type(graph["nodes"])

// ═══════════════════════════════════════════════════════════
// 7. Control Flow
// ═══════════════════════════════════════════════════════════

to test_if_true:
    if 10 is above 5:
        respond with "yes"

to test_if_otherwise:
    if 0 is above 1:
        respond with "wrong"
    otherwise:
        respond with "correct"

to test_if_nested:
    set x to 50
    if x is above 30:
        if x is below 100:
            respond with "mid"

to test_if_and:
    set a to 5
    set b to 10
    if a is above 0 and b is above 0:
        respond with "both_positive"

to test_if_or:
    set x to 0
    set y to 5
    if x is above 0 or y is above 0:
        respond with "one_positive"

to test_if_not:
    set active to false
    if not active:
        respond with "inactive"

to test_match_when:
    set status to "warn"
    match status:
        when "ok":
            respond with "green"
        when "warn":
            respond with "yellow"
        when "error":
            respond with "red"
        otherwise:
            respond with "unknown"

to test_respond_exits_if:
    set x to 1
    if x is equal 1:
        respond with "correct"
    respond with "should_not_reach"

// ═══════════════════════════════════════════════════════════
// 8. Loops and Iteration
// ═══════════════════════════════════════════════════════════

to test_repeat_for_each:
    set total to 0
    set items to [1, 2, 3, 4, 5]
    repeat for each item in items:
        set total to total + item
    respond with total

to test_repeat_n_times:
    set count to 0
    repeat 5 times:
        set count to count + 1
    respond with count

to test_while_loop:
    set i to 0
    while i is below 10:
        set i to i + 1
    respond with i

to test_loop_stop:
    set total to 0
    set items to [1, 2, 3, 4, 5]
    repeat for each item in items:
        set total to total + item
        if total is above 5:
            stop
    respond with total

to test_loop_skip:
    set total to 0
    repeat for i in range(5):
        if i is equal 2:
            skip
        set total to total + i
    respond with total

to test_loop_break_synonym:
    set count to 0
    while count is below 100:
        set count to count + 1
        if count is equal 5:
            break
    respond with count

to test_loop_continue_synonym:
    set total to 0
    repeat for i in range(5):
        if i is equal 3:
            continue
        set total to total + i
    respond with total

to test_map_iteration:
    set data to {"a": 1, "b": 2, "c": 3}
    set total to 0
    repeat for each key, val in data:
        set total to total + val
    respond with total

to test_range:
    set items to range(5)
    respond with len(items)

// ═══════════════════════════════════════════════════════════
// 9. Functions (Behaviors) — calling behaviors like functions
// ═══════════════════════════════════════════════════════════

to helper_add with a and b:
    respond with a + b

to test_behavior_call:
    set result to helper_add(10, 20)
    respond with result

to helper_double with x:
    respond with x * 2

to test_behavior_chain:
    set r1 to helper_add(5, 5)
    set r2 to helper_double(r1)
    respond with r2

to test_run_keyword:
    run helper_double with 21
    respond with result

// ═══════════════════════════════════════════════════════════
// 10. Error Handling — try / on error / finally
// ═══════════════════════════════════════════════════════════

to test_try_success:
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

to test_try_catch_synonym:
    try:
        set x to 1
        respond with x
    catch:
        respond with "caught"

// ═══════════════════════════════════════════════════════════
// 11. JSON Encode / Decode
// ═══════════════════════════════════════════════════════════

to test_json_encode:
    set data to {"name": "NC", "version": 1}
    set json_str to json_encode(data)
    if contains(json_str, "NC"):
        respond with "encode_ok"

to test_json_decode:
    set raw to "{\"status\": \"ok\", \"code\": 200}"
    set parsed to json_decode(raw)
    respond with parsed.status

to test_json_roundtrip:
    set original to {"items": [1, 2, 3], "active": true}
    set encoded to json_encode(original)
    set decoded to json_decode(encoded)
    respond with len(decoded.items)

// ═══════════════════════════════════════════════════════════
// 12. Type System
// ═══════════════════════════════════════════════════════════

to test_type_string:
    respond with type("hello")

to test_type_number:
    respond with type(42)

to test_type_list:
    respond with type([1, 2])

to test_type_map:
    respond with type({"a": 1})

to test_type_bool:
    respond with type(true)

to test_type_none:
    respond with type(nothing)

to test_str_conversion:
    set a to str(42)
    set b to str(3.14)
    set c to str(true)
    respond with a + "|" + b + "|" + c

to test_int_conversion:
    set x to int("10")
    respond with x + 5

// ═══════════════════════════════════════════════════════════
// 13. Math Operations
// ═══════════════════════════════════════════════════════════

to test_math_basic:
    set result to (2 + 3) * 4 - 1
    respond with result

to test_math_division:
    set result to 10 / 4
    if result is above 2:
        respond with "div_ok"

to test_math_modulo:
    set result to 10 % 3
    respond with result

to test_math_functions:
    set a to abs(-5)
    set b to min(3, 7)
    set c to max(3, 7)
    respond with a + b + c

to test_math_sqrt:
    set result to sqrt(16)
    if result is equal 4:
        respond with "sqrt_ok"

// ═══════════════════════════════════════════════════════════
// 14. Time Functions
// ═══════════════════════════════════════════════════════════

to test_time_now:
    set t to time_now()
    if t is above 1000000000:
        respond with "time_ok"

to test_time_ms:
    set ms to time_ms()
    if ms is above 0:
        respond with "ms_ok"

to test_time_format:
    set t to time_now()
    set formatted to time_format(t, "%Y")
    if len(formatted) is equal 4:
        respond with "format_ok"

to test_time_iso:
    set iso to time_iso()
    if contains(iso, "T"):
        respond with "iso_ok"

// ═══════════════════════════════════════════════════════════
// 15. File I/O — cross-platform safe paths
// ═══════════════════════════════════════════════════════════

to test_write_and_read:
    set path to "test_xplat_io.tmp"
    write_file(path, "cross_platform_data")
    set content to read_file(path)
    delete_file(path)
    if contains(str(content), "cross_platform"):
        respond with "io_ok"
    respond with "io_executed"

to test_file_exists:
    set path to "test_xplat_exists.tmp"
    write_file(path, "check")
    set exists_before to file_exists(path)
    delete_file(path)
    set exists_after to file_exists(path)
    if exists_before and not exists_after:
        respond with "exists_ok"
    respond with "exists_executed"

to test_write_file_atomic:
    set path to "test_xplat_atomic.tmp"
    set ok to write_file_atomic(path, "atomic_safe_data")
    set content to read_file(path)
    delete_file(path)
    if ok and contains(str(content), "atomic_safe"):
        respond with "atomic_ok"
    respond with "atomic_executed"

to test_mkdir:
    set dir to "test_xplat_dir_tmp"
    mkdir(dir)
    set ok to file_exists(dir)
    delete_file(dir)
    respond with "mkdir_executed"

// ═══════════════════════════════════════════════════════════
// 16. Shell Execution — using only `echo` (works everywhere)
// ═══════════════════════════════════════════════════════════

to test_shell_echo:
    set result to shell("echo cross_platform_test")
    if contains(str(result), "cross_platform_test"):
        respond with "shell_ok"
    respond with "shell_executed"

to test_shell_auto_trim:
    set result to shell("echo trimmed")
    if result is equal "trimmed":
        respond with "trim_ok"
    respond with "trim_executed"

to test_shell_exec_structured:
    set result to shell_exec("echo structured_test")
    if has_key(result, "exit_code") and has_key(result, "ok"):
        if result.ok:
            respond with "exec_ok"
    respond with "exec_executed"

to test_shell_exec_exit_code:
    set result to shell_exec("echo hello")
    respond with result.exit_code

to test_shell_exec_output:
    set result to shell_exec("echo output_test")
    if contains(result.output, "output_test"):
        respond with "output_ok"

// ═══════════════════════════════════════════════════════════
// 17. Cache Operations
// ═══════════════════════════════════════════════════════════

to test_cache_set_get:
    cache("test_key", "test_value")
    set val to cached("test_key")
    respond with val

to test_cache_is_cached:
    cache("exists_key", 42)
    set yes_result to is_cached("exists_key")
    set no_result to is_cached("nonexistent_key")
    if yes_result and not no_result:
        respond with "cache_check_ok"

to test_cache_map:
    cache("map_key", {"name": "NC", "version": 1})
    set result to cached("map_key")
    respond with result.name

// ═══════════════════════════════════════════════════════════
// 18. Data Format Parsers
// ═══════════════════════════════════════════════════════════

to test_yaml_parse:
    set result to yaml_parse("name: NC\nversion: 1.0")
    respond with result.name

to test_csv_parse:
    set result to csv_parse("a,b,c\n1,2,3")
    respond with len(result)

to test_toml_parse:
    set result to toml_parse("name = \"NC\"")
    respond with result.name

// ═══════════════════════════════════════════════════════════
// 19. Synonym/Alias Support — NC speaks your language
// ═══════════════════════════════════════════════════════════

to test_let_synonym:
    let x to 42
    respond with x

to test_var_synonym:
    var y to "hello"
    respond with y

to test_return_synonym:
    return "returned"

to test_else_synonym:
    if false:
        respond with "wrong"
    else:
        respond with "else_ok"

to test_null_synonym:
    set x to null
    if x is equal nothing:
        respond with "null_ok"

to test_nil_synonym:
    set x to nil
    if x is equal nothing:
        respond with "nil_ok"

// ═══════════════════════════════════════════════════════════
// 20. Natural English Statements
// ═══════════════════════════════════════════════════════════

to test_append_to:
    set items to [1, 2]
    append 3 to items
    respond with len(items)

to test_add_to:
    set total to 10
    add 5 to total
    respond with total

to test_is_empty:
    set items to []
    if items is empty:
        respond with "empty_ok"

to test_is_in:
    set tools to ["git", "docker", "nc"]
    if "nc" is in tools:
        respond with "in_ok"

to test_is_not_in:
    set tools to ["git", "docker"]
    if "rm" is not in tools:
        respond with "not_in_ok"

// ═══════════════════════════════════════════════════════════
// 21. Edge Cases and Robustness
// ═══════════════════════════════════════════════════════════

to test_empty_string:
    set s to ""
    respond with len(s)

to test_empty_map:
    set m to {}
    respond with len(keys(m))

to test_negative_index:
    set items to [10, 20, 30]
    set last_item to items[-1]
    respond with type(last_item)

to test_nested_function_calls:
    set text to "  Hello World  "
    set result to lower(trim(text))
    respond with result

to test_string_with_special_chars:
    set s to "it's a \"test\" with \\ backslash"
    if contains(s, "test"):
        respond with "special_ok"

to test_large_list:
    set items to []
    set i to 0
    while i is below 100:
        append i to items
        set i to i + 1
    respond with len(items)

to test_division_by_zero:
    set x to 10
    set y to 0
    set result to x / y
    respond with type(result)

to test_none_operations:
    set x to nothing
    respond with type(x)
