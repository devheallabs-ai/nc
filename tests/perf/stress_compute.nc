service "stress-test"
version "1.0.0"

to bench_math:
    set result to 0
    set i to 0
    while i is below 10000:
        set result to result + i * 2
        set i to i + 1
    respond with result

to bench_strings:
    set result to ""
    set i to 0
    while i is below 500:
        set result to result + "x"
        set i to i + 1
    respond with len(result)

to bench_list:
    set items to []
    set i to 0
    while i is below 1000:
        append i to items
        set i to i + 1
    respond with len(items)

to bench_map:
    set data to {}
    set i to 0
    while i is below 100:
        set key to str(i)
        set data to {"count": i}
        set i to i + 1
    respond with i

to bench_nested_loops:
    set total to 0
    set i to 0
    while i is below 100:
        set j to 0
        while j is below 100:
            set total to total + 1
            set j to j + 1
        set i to i + 1
    respond with total

to bench_conditionals:
    set count to 0
    set i to 0
    while i is below 5000:
        if i % 2 is equal 0:
            set count to count + 1
        set i to i + 1
    respond with count

to bench_function_calls:
    set total to 0
    set i to 0
    while i is below 1000:
        set total to total + len("hello")
        set i to i + 1
    respond with total
