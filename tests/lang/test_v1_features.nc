// V1 Feature Tests — validates all new stdlib and VM features
// Must pass on the VM (compiled) path used by `nc test`

service "test-v1-features"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Dynamic map keys — OP_SET_INDEX in VM
// ═══════════════════════════════════════════════════════════

to test_vm_set_map_key:
    set data to {}
    set key to "name"
    set data[key] to "NC"
    respond with data["name"]

to test_vm_set_map_field_key:
    set db to {"edges": {}}
    set eid to "a_to_b"
    set db.edges[eid] to {"score": 1.0}
    respond with db.edges["a_to_b"]["score"]

to test_vm_map_multiple_keys:
    set scores to {}
    set scores["alice"] to 100
    set scores["bob"] to 200
    respond with scores["alice"] + scores["bob"]

// ═══════════════════════════════════════════════════════════
// Math: trig + log/exp
// ═══════════════════════════════════════════════════════════

to test_cos:
    set result to cos(0)
    if result is above 0.9:
        respond with "cos_ok"
    respond with "cos_fail"

to test_sin:
    set result to sin(0)
    if result is below 0.1:
        respond with "sin_ok"
    respond with "sin_fail"

to test_tan:
    set result to tan(0)
    if result is below 0.1:
        respond with "tan_ok"
    respond with "tan_fail"

to test_exp:
    set result to exp(0)
    if result is above 0.9:
        respond with "exp_ok"
    respond with "exp_fail"

to test_log:
    set result to log(1)
    if result is below 0.1:
        respond with "log_ok"
    respond with "log_fail"

// ═══════════════════════════════════════════════════════════
// Functional: enumerate, zip, filter
// ═══════════════════════════════════════════════════════════

to test_enumerate:
    set items to ["a", "b", "c"]
    set result to enumerate(items)
    respond with len(result)

to test_zip:
    set names to ["x", "y"]
    set vals to [10, 20]
    set result to zip(names, vals)
    respond with len(result)

to test_filter:
    set items to [0, 1, 0, 2, 0, 3]
    set result to filter(items)
    respond with len(result)

// ═══════════════════════════════════════════════════════════
// Hashing / encoding
// ═══════════════════════════════════════════════════════════

to test_hash:
    set h to hash("hello")
    if len(h) is equal 8:
        respond with "hash_ok"

to test_uuid:
    set u to uuid()
    if contains(u, "-"):
        respond with "uuid_ok"

to test_hex:
    set h to hex(255)
    if contains(h, "ff"):
        respond with "hex_ok"

to test_bin:
    set b to bin(5)
    if contains(b, "101"):
        respond with "bin_ok"

// ═══════════════════════════════════════════════════════════
// Shell exec + atomic writes (VM path)
// ═══════════════════════════════════════════════════════════

to test_shell_exec_vm:
    set r to shell_exec("echo vm_test")
    if r.ok:
        respond with "shell_exec_vm_ok"
    respond with "shell_exec_vm_ran"

to test_write_atomic_vm:
    set path to "test_v1_atomic.tmp"
    set ok to write_file_atomic(path, "vm_atomic")
    set content to read_file(path)
    delete_file(path)
    if ok and contains(str(content), "vm_atomic"):
        respond with "atomic_vm_ok"
    respond with "atomic_vm_ran"

// ═══════════════════════════════════════════════════════════
// Regex (POSIX — skipped on Windows)
// ═══════════════════════════════════════════════════════════

to test_re_match:
    set result to re_match("hello world", "wor")
    respond with result

to test_re_find:
    set result to re_find("abc 123 def", "[0-9]+")
    respond with result

to test_re_replace:
    set result to re_replace("hello 123 world 456", "[0-9]+", "NUM")
    respond with result

// ═══════════════════════════════════════════════════════════
// Time: time_iso in VM
// ═══════════════════════════════════════════════════════════

to test_time_iso_vm:
    set iso to time_iso()
    if contains(iso, "T"):
        respond with "iso_vm_ok"
