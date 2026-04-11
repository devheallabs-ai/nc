// Test: Advanced standard library — every function documented in the language guide

service "test-advanced-stdlib"

to test first and last:
    set items to [10, 20, 30]
    set f to first(items)
    set l to last(items)
    respond with {"first": f, "last": l}

to test sum and average:
    set nums to [10, 20, 30, 40]
    set s to sum(nums)
    set a to average(nums)
    respond with {"sum": s, "avg": a}

to test unique:
    set items to [1, 2, 2, 3, 3, 3]
    respond with unique(items)

to test flatten:
    set nested to [[1, 2], [3, 4], [5]]
    respond with flatten(nested)

to test slice:
    set items to [10, 20, 30, 40, 50]
    respond with slice(items, 1, 3)

to test index of:
    set items to ["apple", "banana", "cherry"]
    respond with index_of(items, "banana")

to test any and all:
    set mixed to [false, true, false]
    set results to {"any": any(mixed), "all": all(mixed)}
    respond with results

to test type checks:
    set t to is_text("hello")
    set n to is_number(42)
    set l to is_list([1, 2])
    set z to is_none(nothing)
    respond with {"is_text": t, "is_number": n, "is_list": l, "is_none": z}

to test has key:
    set user to {"name": "Alice", "age": 30}
    respond with has_key(user, "name")

to test substr:
    respond with substr("hello world", 6, 11)

to test chr and ord:
    set c to chr(65)
    set n to ord("A")
    respond with {"chr": c, "ord": n}

to test cache:
    cache("greeting", "hello")
    set val to cached("greeting")
    set exists to is_cached("greeting")
    respond with {"value": val, "exists": exists}

to test modulo:
    set evens to []
    set nums to range(10)
    repeat for each n in nums:
        if n % 2 is equal to 0:
            append n to evens
    respond with evens

to test json roundtrip:
    set data to {"name": "NC", "fast": true}
    set json to json_encode(data)
    set back to json_decode(json)
    respond with back

to test file exists:
    set tmp to env("TMPDIR")
    if tmp is nothing:
        set tmp to env("TEMP")
    if tmp is nothing:
        set tmp to "/tmp"
    respond with file_exists(tmp)

to test env:
    set home to env("HOME")
    if home is nothing:
        set home to env("USERPROFILE")
    respond with home

to test token counting:
    set msg to "This is a sample for counting tokens"
    set cnt to token_count(msg)
    respond with cnt

to test count function:
    set items to [1, 2, 2, 3, 3, 3]
    respond with count(items, 3)
