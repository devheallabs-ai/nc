service "docs-test"
version "1.0.0"

to test_crypto:
    set digest to hash_sha256("hello world")
    set stored to hash_password("secret")
    set ok to verify_password("secret", stored)
    set mac to hash_hmac("data", "key")
    respond with {"digest_len": len(digest), "pw_ok": ok, "mac_len": len(mac)}

to test_sessions:
    set sid to session_create()
    session_set(sid, "user", "alice")
    set user to session_get(sid, "user")
    set exists to session_exists(sid)
    session_destroy(sid)
    set gone to session_exists(sid)
    respond with {"user": user, "exists_before": exists, "exists_after": gone}

to test_list_ops:
    set nums to [5, 2, 8, 1, 9]
    set lowest to min(nums)
    set highest to max(nums)
    set precise to round(3.14159, 2)
    respond with {"min": lowest, "max": highest, "round": precise}

to test_higher_order:
    set items to [{"n": "c", "s": 3}, {"n": "a", "s": 1}, {"n": "b", "s": 2}]
    set sorted to sort_by(items, "s")
    set best to max_by(items, "s")
    set total to sum_by(items, "s")
    set names to map_field(items, "n")
    set winners to filter_by(items, "s", "above", 1)
    respond with {"first": sorted[0].n, "best": best.n, "total": total, "names": len(names), "winners": len(winners)}

to test_time:
    set iso to time_iso()
    set now to time_now()
    set formatted to time_format(now)
    respond with {"has_year": contains(iso, "202"), "has_time": contains(formatted, ":")}

to test_try_otherwise:
    try:
        set x to 42
    otherwise:
        set x to -1
    respond with x

to test_try_on_error:
    try:
        set x to 100
    on_error:
        set x to -1
    respond with x

to test_wait_ms:
    wait 1 ms
    respond with "waited"

to test_skip:
    set total to 0
    set items to [1, 2, 3, 4, 5]
    repeat for each item in items:
        if item is equal to 3:
            skip
        set total to total + item
    respond with total

to test_feature_flag:
    if feature("nonexistent_xyz"):
        respond with "on"
    otherwise:
        respond with "off"

to test_request_context:
    set h to request_header("Authorization")
    if h is equal to nothing:
        respond with "no context"

to test_type_checks:
    set a to is_text("hello")
    set b to is_number(42)
    set c to is_list([1, 2])
    set d to is_record({"k": "v"})
    set e to is_none(nothing)
    respond with {"text": a, "number": b, "list": c, "record": d, "none": e}

api:
    GET /crypto runs test_crypto
    GET /sessions runs test_sessions
    GET /lists runs test_list_ops
    GET /higher runs test_higher_order
    GET /time runs test_time
    GET /try1 runs test_try_otherwise
    GET /try2 runs test_try_on_error
    GET /wait runs test_wait_ms
    GET /skip runs test_skip
    GET /feature runs test_feature_flag
    GET /context runs test_request_context
    GET /types runs test_type_checks
