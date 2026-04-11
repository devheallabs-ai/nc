// Test: migration-related patterns — code conversion targets
// Verifies NC can express patterns that users migrate from Python/JS

service "test-migrate"
version "1.0.0"

// Python: for item in items: total += item
to test python loop pattern:
    set items to [10, 20, 30, 40]
    set total to 0
    repeat for each item in items:
        set total to total + item
    respond with total

// Python: result = x if condition else y
to test python ternary pattern:
    set x to 10
    if x is above 5:
        set result to "big"
    otherwise:
        set result to "small"
    respond with result

// Python: data = {"key": value, **defaults}
to test python dict pattern:
    set defaults to {"timeout": 30, "retries": 3}
    set config to {"url": "https://api.example.com"}
    respond with config.url

// Python: try/except/finally
to test python error pattern:
    set status to "init"
    try:
        set status to "running"
        set x to 42
    on_error:
        set status to "failed"
    finally:
        set status to status + "-complete"
    respond with status

// Python: list comprehension [x*2 for x in range(5)]
to test python list pattern:
    set doubled to []
    repeat for each n in [1, 2, 3, 4, 5]:
        append n * 2 to doubled
    respond with doubled
