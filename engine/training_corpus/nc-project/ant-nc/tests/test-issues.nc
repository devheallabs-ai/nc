// NC Issue Validation Test Suite
// Run: NC_ALLOW_EXEC=1 nc serve test-issues.nc
// Then: nc run test-runner.nc -b run_tests

service "nc-issue-tests"
version "1.0.0"

configure:
    port: 9877

// ═══ Issue #1: HTML serving ═══
to test_html_inline:
    respond with "<html><body><h1>Inline HTML</h1></body></html>"

to test_html_readfile:
    set html to read_file("test-page.html")
    respond with html

// ═══ Issue #2: GET / route ═══
to test_root:
    respond with {"route": "root", "status": "ok"}

// ═══ Issue #3: cache across requests ═══
to test_cache_set with key, value:
    cache(key, value)
    respond with {"action": "set", "key": key, "value": value}

to test_cache_get with key:
    set v to cached(key)
    set exists to is_cached(key)
    respond with {"key": key, "value": v, "exists": exists}

// ═══ Issue #4: store into ═══
to test_store_set with key, value:
    store value into key
    respond with {"action": "stored", "key": key, "value": value}

to test_store_get with key:
    gather v from key
    respond with {"key": key, "value": v}

// ═══ Issue #5: behavior calling behavior ═══
to helper_behavior:
    respond with {"from_helper": true, "number": 42}

to test_call_behavior:
    set result to helper_behavior()
    respond with {"called": true, "result": result}

// ═══ Issue #7: ai as variable name ═══
to test_ai_var:
    set ai_val to "test"
    respond with {"ai_val": ai_val}

// ═══ Issue #8: write_file without mkdir ═══
to test_write_no_dir:
    set ok to write_file("test-output/deep/nested/file.txt", "hello")
    respond with {"write_result": ok}

to test_write_flat:
    set ok to write_file("test-output-flat.txt", "hello")
    respond with {"write_result": ok}

// ═══ Issue #11: store 256 limit ═══
to test_store_many with count:
    set stored to 0
    repeat for each i in range(count):
        store "val" into "key" + str(i)
        set stored to stored + 1
    respond with {"attempted": count, "stored": stored}

// ═══ Issue #14: respond with in if doesn't exit ═══
to test_respond_if with flag:
    if flag is equal "yes":
        respond with {"branch": "if", "flag": flag}
    respond with {"branch": "fallthrough", "flag": flag}

to test_respond_if_otherwise with flag:
    if flag is equal "yes":
        respond with {"branch": "if", "flag": flag}
    otherwise:
        respond with {"branch": "otherwise", "flag": flag}

// ═══ Issue #16: time_now works? ═══
to test_time:
    set t to time_now()
    set formatted to time_format(t, "%Y-%m-%d %H:%M:%S")
    respond with {"timestamp": t, "formatted": formatted}

// ═══ Issue #20: read_file type ═══
to test_readfile_type:
    set content to read_file("test-page.html")
    set t to type(content)
    set l to len(content)
    respond with {"type": t, "length": l, "starts_with_lt": starts_with(content, "<")}

// ═══ Issue #19: gather POST ═══
to test_echo with message:
    respond with {"echo": message}

// ═══ Issue: env() ═══
to test_env:
    set port to env("NC_PORT")
    set missing to env("NONEXISTENT_VAR_XYZ")
    respond with {"NC_PORT": port, "missing_var": missing}

// ═══ Issue: shell() ═══
to test_shell:
    set result to shell("echo hello_from_shell")
    respond with {"shell_output": result}

// ═══ Issue: json_encode/decode ═══
to test_json:
    set obj to {"name": "test", "count": 42, "items": [1, 2, 3]}
    set encoded to json_encode(obj)
    set decoded to json_decode(encoded)
    respond with {"original": obj, "encoded_type": type(encoded), "decoded": decoded}

api:
    GET / runs test_root
    GET /html/inline runs test_html_inline
    GET /html/file runs test_html_readfile
    POST /cache/set runs test_cache_set
    POST /cache/get runs test_cache_get
    POST /store/set runs test_store_set
    POST /store/get runs test_store_get
    GET /call runs test_call_behavior
    GET /ai-var runs test_ai_var
    GET /write/nodir runs test_write_no_dir
    GET /write/flat runs test_write_flat
    POST /respond-if runs test_respond_if
    POST /respond-otherwise runs test_respond_if_otherwise
    GET /time runs test_time
    GET /readfile-type runs test_readfile_type
    POST /echo runs test_echo
    GET /env runs test_env
    GET /shell runs test_shell
    GET /json runs test_json
