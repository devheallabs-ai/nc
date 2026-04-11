service "vm-enterprise-test"
version "1.0.0"

configure:
    port: 9199

to test_hash_sha256:
    set h to hash_sha256("hello")
    respond with {"hash": h, "len": len(h)}

to test_hash_password:
    set stored to hash_password("secret123")
    set ok to verify_password("secret123", stored)
    set bad to verify_password("wrong", stored)
    respond with {"ok": ok, "bad": bad}

to test_hmac:
    set mac to hash_hmac("data", "key")
    respond with {"mac": mac, "len": len(mac)}

to test_jwt:
    set token to jwt_generate("testuser", "admin", 3600)
    set claims to jwt_verify(token)
    respond with {"token_len": len(token), "user": claims.sub, "role": claims.role}

to test_session:
    set sid to session_create()
    session_set(sid, "user", "alice")
    set user to session_get(sid, "user")
    set exists to session_exists(sid)
    session_destroy(sid)
    set gone to session_exists(sid)
    respond with {"user": user, "existed": exists, "destroyed": gone}

to test_time:
    set iso to time_iso()
    respond with {"iso": iso}

to test_list_ops:
    set nums to [5, 2, 8, 1, 9]
    set items to [{"n": "c", "s": 3}, {"n": "a", "s": 1}, {"n": "b", "s": 2}]
    respond with {
        "max": max(nums),
        "min": min(nums),
        "round": round(3.14159, 2),
        "sort_by_first": sort_by(items, "s")[0].n,
        "max_by": max_by(items, "s").n,
        "sum_by": sum_by(items, "s")
    }

to test_feature:
    if feature("test_flag_xyz"):
        respond with "enabled"
    otherwise:
        respond with "disabled"

to test_try_otherwise:
    try:
        set x to 42
    otherwise:
        set x to -1
    respond with x

to test_request_info:
    set req_method to request_method()
    set req_path to request_path()
    set req_ip to request_ip()
    respond with {"req_method": req_method, "req_path": req_path, "has_ip": req_ip is not equal to nothing}

to test_wait_ms:
    wait 10 ms
    respond with "waited"

api:
    GET /hash runs test_hash_sha256
    GET /password runs test_hash_password
    GET /hmac runs test_hmac
    GET /jwt runs test_jwt
    GET /session runs test_session
    GET /time runs test_time
    GET /lists runs test_list_ops
    GET /feature runs test_feature
    GET /try runs test_try_otherwise
    GET /request runs test_request_info
    GET /wait runs test_wait_ms
