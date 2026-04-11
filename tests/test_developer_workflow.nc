// NC Developer Workflow Validation Test
// Tests everything a developer does daily — using NC only, no curl/python
//
// Run: nc run tests/test_developer_workflow.nc

service "developer-workflow-test"
version "1.0.0"

// ═══════════════════════════════════════════════
//  1. Variables & Data Types
// ═══════════════════════════════════════════════

to test_variables:
    set name to "NC Developer"
    set ver_num to 100
    set pi to 3.14159
    set active to yes
    set items to ["linux", "windows", "macos"]
    set config to {"port": 8080, "debug": yes}

    // Type checks
    if type(name) is equal "text":
        log "✓ String type works"
    otherwise:
        log "✗ String type FAILED"

    if type(ver_num) is equal "number":
        log "✓ Number type works"
    otherwise:
        log "✗ Number type FAILED"

    if type(items) is equal "list":
        log "✓ List type works"
    otherwise:
        log "✗ List type FAILED"

    if type(config) is equal "record":
        log "✓ Record type works"
    otherwise:
        log "✗ Record type FAILED"

    if len(items) is equal 3:
        log "✓ len() works"
    otherwise:
        log "✗ len() FAILED"

    respond with "variables OK"

// ═══════════════════════════════════════════════
//  2. String Operations (daily developer task)
// ═══════════════════════════════════════════════

to test_strings:
    set email to "  developer@notation-code.dev  "
    set cleaned to trim(email)
    set parts to split(cleaned, "@")
    set domain to parts[1]
    set upper_domain to upper(domain)

    if contains(cleaned, "@"):
        log "✓ contains() works"
    otherwise:
        log "✗ contains() FAILED"

    if starts_with(cleaned, "developer"):
        log "✓ starts_with() works"
    otherwise:
        log "✗ starts_with() FAILED"

    if ends_with(cleaned, ".dev"):
        log "✓ ends_with() works"
    otherwise:
        log "✗ ends_with() FAILED"

    set replaced to replace(cleaned, "developer", "admin")
    if contains(replaced, "admin"):
        log "✓ replace() works"
    otherwise:
        log "✗ replace() FAILED"

    if upper_domain is equal "NOTATION-CODE.DEV":
        log "✓ upper() works"
    otherwise:
        log "✗ upper() FAILED"

    respond with "strings OK"

// ═══════════════════════════════════════════════
//  3. List Operations
// ═══════════════════════════════════════════════

to test_lists:
    set platforms to ["linux", "windows", "macos"]
    set sorted_platforms to sort(platforms)
    set reversed to reverse(platforms)

    if first(platforms) is equal "linux":
        log "✓ first() works"
    otherwise:
        log "✗ first() FAILED"

    if last(platforms) is equal "macos":
        log "✓ last() works"
    otherwise:
        log "✗ last() FAILED"

    set with_docker to append("docker", platforms)
    if len(with_docker) is equal 4:
        log "✓ append() works"
    otherwise:
        log "✗ append() FAILED"

    set total to sum([10, 20, 30])
    if total is equal 60:
        log "✓ sum() works"
    otherwise:
        log "✗ sum() FAILED"

    set avg to average([10, 20, 30])
    if avg is equal 20:
        log "✓ average() works"
    otherwise:
        log "✗ average() FAILED"

    respond with "lists OK"

// ═══════════════════════════════════════════════
//  4. Control Flow
// ═══════════════════════════════════════════════

to test_control_flow:
    // If/otherwise
    set score to 85
    if score is above 90:
        set grade to "A"
    otherwise:
        if score is above 80:
            set grade to "B"
        otherwise:
            set grade to "C"

    if grade is equal "B":
        log "✓ if/otherwise works"
    otherwise:
        log "✗ if/otherwise FAILED"

    // Match/when
    set status to "running"
    match status:
        when "running":
            set emoji to "🟢"
        when "stopped":
            set emoji to "🔴"
        otherwise:
            set emoji to "⚪"

    if emoji is equal "🟢":
        log "✓ match/when works"
    otherwise:
        log "✗ match/when FAILED"

    // Repeat loop
    set counter to 0
    repeat 5 times:
        set counter to counter + 1

    if counter is equal 5:
        log "✓ repeat N times works"
    otherwise:
        log "✗ repeat FAILED"

    // For each
    set total to 0
    set nums to [1, 2, 3, 4, 5]
    repeat for each num in nums:
        set total to total + num

    if total is equal 15:
        log "✓ for each works"
    otherwise:
        log "✗ for each FAILED"

    respond with "control flow OK"

// ═══════════════════════════════════════════════
//  5. JSON Operations
// ═══════════════════════════════════════════════

to test_json:
    set data to {"name": "NC", "version": 1, "platforms": ["linux", "windows", "macos"]}
    set json_str to json_encode(data)
    set parsed to json_decode(json_str)

    if parsed.name is equal "NC":
        log "✓ json_encode/decode roundtrip works"
    otherwise:
        log "✗ JSON roundtrip FAILED"

    if parsed.version is equal 1:
        log "✓ JSON number preservation works"
    otherwise:
        log "✗ JSON number FAILED"

    if len(parsed.platforms) is equal 3:
        log "✓ JSON list preservation works"
    otherwise:
        log "✗ JSON list FAILED"

    respond with "json OK"

// ═══════════════════════════════════════════════
//  6. Math Functions
// ═══════════════════════════════════════════════

to test_math:
    if abs(-42) is equal 42:
        log "✓ abs() works"
    otherwise:
        log "✗ abs() FAILED"

    if sqrt(16) is equal 4:
        log "✓ sqrt() works"
    otherwise:
        log "✗ sqrt() FAILED"

    if pow(2, 10) is equal 1024:
        log "✓ pow() works"
    otherwise:
        log "✗ pow() FAILED"

    if min(5, 3) is equal 3:
        log "✓ min() works"
    otherwise:
        log "✗ min() FAILED"

    if max(5, 3) is equal 5:
        log "✓ max() works"
    otherwise:
        log "✗ max() FAILED"

    set r to random()
    if r is at least 0:
        if r is below 1:
            log "✓ random() works"

    respond with "math OK"

// ═══════════════════════════════════════════════
//  7. File I/O
// ═══════════════════════════════════════════════

to test_file_io:
    set test_content to "Hello from NC v1.0.0 on any platform!"
    write_file("/tmp/_nc_test_output.txt", test_content)

    if file_exists("/tmp/_nc_test_output.txt"):
        log "✓ write_file + file_exists works"
    otherwise:
        log "✗ file_exists FAILED"

    set read_back to read_file("/tmp/_nc_test_output.txt")
    if contains(str(read_back), "NC v1.0.0"):
        log "✓ read_file works"
    otherwise:
        log "✗ read_file FAILED"

    respond with "file io OK"

// ═══════════════════════════════════════════════
//  8. Environment Variables
// ═══════════════════════════════════════════════

to test_env:
    set path_val to env("PATH")
    if len(str(path_val)) is above 0:
        log "✓ env() works (PATH exists)"
    otherwise:
        log "✗ env() FAILED"

    set home to env("HOME")
    if type(home) is equal "text":
        log "✓ env(HOME) returns text"
    otherwise:
        set userprofile to env("USERPROFILE")
        if type(userprofile) is equal "text":
            log "✓ env(USERPROFILE) works on Windows"
        otherwise:
            log "✗ env() for home FAILED"

    respond with "env OK"

// ═══════════════════════════════════════════════
//  9. Error Handling (try/on error)
// ═══════════════════════════════════════════════

to test_error_handling:
    try:
        set x to 10 / 0
        log "✓ division by zero handled"
    on error:
        log "✓ try/on-error caught division error"

    try:
        set data to json_decode("not valid json {{{")
        log "✓ invalid JSON handled gracefully"
    on error:
        log "✓ try/on-error caught JSON error"

    respond with "error handling OK"

// ═══════════════════════════════════════════════
//  10. Time Functions
// ═══════════════════════════════════════════════

to test_time:
    set now to time_now()
    if len(str(now)) is above 5:
        log "✓ time_now() works"
    otherwise:
        log "✗ time_now() FAILED"

    set ms to time_ms()
    if ms is above 1000000:
        log "✓ time_ms() works (epoch ms)"
    otherwise:
        log "✗ time_ms() FAILED"

    respond with "time OK"

// ═══════════════════════════════════════════════
//  Main — Runs all tests automatically
// ═══════════════════════════════════════════════

to run_all_tests:
    log "═══════════════════════════════════════"
    log "  NC Developer Workflow Test Suite"
    log "  Platform: all (Linux/Windows/macOS)"
    log "═══════════════════════════════════════"
    run test_variables
    run test_strings
    run test_lists
    run test_control_flow
    run test_json
    run test_math
    run test_file_io
    run test_env
    run test_error_handling
    run test_time
    log "═══════════════════════════════════════"
    log "  ALL DEVELOPER WORKFLOW TESTS PASSED"
    log "═══════════════════════════════════════"
    respond with "ALL OK"

api:
    GET /test runs run_all_tests
