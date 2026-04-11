// Coverage: All standard library functions
// Tests every built-in function available in NC

service "test-coverage-stdlib"
version "1.0.0"

// ── String functions ─────────────────────────
to test_upper:
    respond with upper("hello")

to test_lower:
    respond with lower("HELLO")

to test_trim:
    respond with trim("  hello  ")

to test_contains:
    respond with contains("hello world", "world")

to test_contains_false:
    respond with contains("hello", "xyz")

to test_starts_with:
    respond with starts_with("hello world", "hello")

to test_ends_with:
    respond with ends_with("hello world", "world")

to test_replace:
    respond with replace("hello world", "world", "NC")

to test_split:
    set parts to split("a,b,c", ",")
    respond with len(parts)

to test_join:
    set items to ["x", "y", "z"]
    respond with join(items, "-")

to test_substr:
    respond with substr("hello", 1, 4)

// ── Math functions ───────────────────────────
to test_abs_positive:
    respond with abs(42)

to test_abs_negative:
    respond with abs(-7)

to test_sqrt:
    respond with sqrt(16)

to test_ceil:
    respond with ceil(3.2)

to test_floor:
    respond with floor(3.9)

to test_round:
    respond with round(3.5)

to test_pow:
    respond with pow(2, 10)

to test_min:
    respond with min(3, 7)

to test_max:
    respond with max(3, 7)

to test_random:
    set r to random()
    respond with type(r)

// ── List functions ───────────────────────────
to test_sort:
    set items to [3, 1, 4, 1, 5]
    respond with sort(items)

to test_reverse:
    set items to [1, 2, 3]
    respond with reverse(items)

to test_range_one_arg:
    respond with range(5)

to test_range_two_args:
    respond with range(2, 7)

to test_first:
    respond with first([10, 20, 30])

to test_last:
    respond with last([10, 20, 30])

to test_sum:
    respond with sum([1, 2, 3, 4, 5])

to test_average:
    respond with average([10, 20, 30])

to test_count:
    respond with count([1, 2, 2, 3, 2], 2)

to test_index_of:
    respond with index_of(["a", "b", "c"], "b")

to test_unique:
    respond with unique([1, 2, 2, 3, 3, 3])

to test_flatten:
    respond with flatten([[1, 2], [3, 4]])

to test_any_true:
    respond with any([false, false, true])

to test_any_false:
    respond with any([false, false, false])

to test_all_true:
    respond with all([true, true, true])

to test_all_false:
    respond with all([true, false, true])

to test_remove_from_list:
    set items to [1, 2, 3, 4]
    set items to remove(items, 3)
    respond with items

// ── Type functions ───────────────────────────
to test_type_number:
    respond with type(42)

to test_type_text:
    respond with type("hello")

to test_type_list:
    respond with type([1, 2])

to test_type_record:
    respond with type({"a": 1})

to test_type_bool:
    respond with type(true)

to test_type_none:
    respond with type(nothing)

to test_is_text:
    respond with is_text("hello")

to test_is_number:
    respond with is_number(42)

to test_is_list:
    respond with is_list([1])

to test_is_record:
    respond with is_record({"a": 1})

to test_is_none:
    respond with is_none(nothing)

to test_is_bool:
    respond with is_bool(true)

// ── Map functions ────────────────────────────
to test_has_key:
    set m to {"name": "alice"}
    respond with has_key(m, "name")

to test_has_key_false:
    set m to {"name": "alice"}
    respond with has_key(m, "age")

// ── Time functions ───────────────────────────
to test_time_now:
    set t to time_now()
    respond with type(t)

to test_time_ms:
    set t to time_ms()
    respond with type(t)

// ── Environment ──────────────────────────────
to test_env:
    set home to env("HOME")
    if home is nothing:
        set home to env("USERPROFILE")
    respond with type(home)

// ── JSON encode/decode ───────────────────────
to test_json_encode:
    set data to {"name": "test", "value": 42}
    set j to json_encode(data)
    respond with type(j)

to test_json_decode:
    set j to "{\"x\": 10}"
    set data to json_decode(j)
    respond with data.x

// ── Cache ────────────────────────────────────
to test_cache_set_get:
    cache("my_key", 42)
    set val to cached("my_key")
    respond with val

to test_is_cached:
    cache("test_key", "hello")
    respond with is_cached("test_key")

// ── String utility ───────────────────────────
to test_chr:
    respond with chr(65)

to test_ord:
    respond with ord("A")

// ── Token count ──────────────────────────────
to test_token_count:
    set count to token_count("hello world how are you")
    respond with type(count)

// ── Cross-language exec ──────────────────────
to test_shell:
    set result to shell("echo test_output")
    respond with result

to test_exec:
    set result to exec("echo", "from_exec")
    respond with result

// ── Validate ─────────────────────────────────
to test_validate_pass:
    set data to {"name": "alice", "age": 30}
    set v to validate(data, ["name", "age"])
    respond with v

to test_validate_fail:
    set data to {"name": "alice"}
    set v to validate(data, ["name", "age", "email"])
    respond with v

// ── Memory ───────────────────────────────────
to test_memory_lifecycle:
    set mem to memory_new(5)
    memory_add(mem, "user", "hello")
    memory_add(mem, "assistant", "hi there")
    set history to memory_get(mem)
    respond with len(history)

to test_memory_summary:
    set mem to memory_new(5)
    memory_add(mem, "user", "test message")
    set summary to memory_summary(mem)
    respond with type(summary)

to test_memory_clear:
    set mem to memory_new(5)
    memory_add(mem, "user", "msg")
    memory_clear(mem)
    set history to memory_get(mem)
    respond with len(history)
