service "test-nested"
version "1.0.0"

to test_nested_if:
    set x to 10
    set result to "none"
    if x is above 5:
        if x is above 8:
            if x is equal 10:
                set result to "deep"
    respond with result

to test_nested_loops:
    set total to 0
    repeat for each i in [1, 2, 3]:
        repeat for each j in [10, 20]:
            set total to total + i * j
    respond with total

to test_if_in_loop:
    set evens to 0
    repeat for each n in [1, 2, 3, 4, 5, 6]:
        if n % 2 is equal 0:
            set evens to evens + 1
    respond with evens

to test_match_in_if:
    set x to 5
    set result to "unknown"
    if x is above 3:
        match x:
            when 5:
                set result to "five"
    respond with result
