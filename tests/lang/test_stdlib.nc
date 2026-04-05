// Test: Standard library functions callable from NC

service "test-stdlib"

to test upper:
    respond with upper("hello")

to test lower:
    respond with lower("HELLO")

to test trim:
    respond with trim("  hello  ")

to test split:
    set parts to split("a,b,c", ",")
    respond with len(parts)

to test join:
    set words to ["hello", "world"]
    respond with join(words, " ")

to test abs:
    respond with abs(-42)

to test sqrt:
    respond with sqrt(9)

to test ceil:
    respond with ceil(2.3)

to test floor:
    respond with floor(2.9)

to test round:
    respond with round(2.5)

to test min:
    respond with min(3, 7)

to test max:
    respond with max(3, 7)

to test random:
    set r to random()
    respond with r

to test time:
    set t to time_now()
    respond with t

to test json roundtrip:
    set data to {"name": "NC", "version": 1}
    set encoded to json_encode(data)
    set decoded to json_decode(encoded)
    respond with decoded

to test env:
    set home to env("HOME")
    if home is nothing:
        set home to env("USERPROFILE")
    respond with home

to test range:
    set nums to range(5)
    respond with nums

to test contains:
    respond with contains("hello world", "world")

to test replace:
    respond with replace("hello world", "world", "NC")

to test sort:
    set items to [3, 1, 2]
    respond with sort(items)

to test reverse:
    set items to [1, 2, 3]
    respond with reverse(items)
