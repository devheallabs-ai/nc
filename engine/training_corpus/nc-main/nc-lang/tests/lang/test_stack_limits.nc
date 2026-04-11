// Test: Stack limits, deep recursion, large lists, stress tests
// Covers: Deep nesting, large data structures, sequential run calls,
//         multiple behavior calls, loop stress
// Mirrors C test sections: Stack Limits, Sequential Run Calls,
//   Multiple nc_call_behavior calls, nested repeat while

service "test-stack-limits"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Large List Building
// ═══════════════════════════════════════════════════════════

to test build list of 500 items:
    set items to []
    repeat for each i in range(500):
        set items to items + [i]
    if len(items) is equal to 500:
        respond with "pass"

to test build list via append 1000:
    set items to []
    set i to 0
    while i is below 1000:
        append i to items
        set i to i + 1
    if len(items) is equal to 1000:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Large Arithmetic Loops
// ═══════════════════════════════════════════════════════════

to test arithmetic loop 10000:
    set result to 0
    set i to 0
    while i is below 10000:
        set result to result + 1
        set i to i + 1
    if result is equal to 10000:
        respond with "pass"

to test sum loop large:
    set total to 0
    set i to 1
    while i is below 101:
        set total to total + i
        set i to i + 1
    // sum 1..100 = 5050
    if total is equal to 5050:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Nested Loops (matrix pattern from C tests)
// ═══════════════════════════════════════════════════════════

to test nested repeat while 3x3:
    set total to 0
    set i to 0
    repeat while i is below 3:
        set j to 0
        repeat while j is below 3:
            set total to total + 1
            set j to j + 1
        set i to i + 1
    if total is equal to 9:
        respond with "pass"

to test nested repeat while 5x5:
    set total to 0
    set i to 0
    repeat while i is below 5:
        set j to 0
        repeat while j is below 5:
            set total to total + 1
            set j to j + 1
        set i to i + 1
    if total is equal to 25:
        respond with "pass"

to test triple nested loop:
    set count to 0
    set i to 0
    while i is below 3:
        set j to 0
        while j is below 3:
            set k to 0
            while k is below 3:
                set count to count + 1
                set k to k + 1
            set j to j + 1
        set i to i + 1
    if count is equal to 27:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Sequential Run Calls (stack reset test from C tests)
// ═══════════════════════════════════════════════════════════

to doubler with x:
    respond with x * 2

to test sequential run calls:
    set total to 0
    repeat for each i in range(12):
        run doubler with i
        set total to total + result
    // sum of i*2 for i=0..11 = 2*(0+1+...+11) = 2*66 = 132
    if total is equal to 132:
        respond with "pass"

to tripler with x:
    respond with x * 3

to test many sequential run calls:
    set total to 0
    repeat for each i in range(20):
        run tripler with i
        set total to total + result
    // sum of i*3 for i=0..19 = 3*(0+1+...+19) = 3*190 = 570
    if total is equal to 570:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Deep If/Otherwise Nesting
// ═══════════════════════════════════════════════════════════

to test deep if nesting:
    set score to 75
    if score is above 90:
        respond with "A"
    otherwise:
        if score is above 80:
            respond with "B"
        otherwise:
            if score is above 70:
                respond with "C"
            otherwise:
                if score is above 60:
                    respond with "D"
                otherwise:
                    respond with "F"

to test deep comparison chain:
    set score to 75
    if score is above 90:
        respond with "A"
    otherwise if score is above 80:
        respond with "B"
    otherwise if score is above 70:
        respond with "C"
    otherwise if score is above 60:
        respond with "D"
    otherwise:
        respond with "F"

// ═══════════════════════════════════════════════════════════
// Fibonacci Accumulation (end-to-end from C tests)
// ═══════════════════════════════════════════════════════════

to test fibonacci 10:
    set a to 0
    set b to 1
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each i in nums:
        set temp to b
        set b to a + b
        set a to temp
    if b is equal to 89:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// String Building in Loop (end-to-end from C tests)
// ═══════════════════════════════════════════════════════════

to test string build in loop:
    set result to ""
    set words to ["hello", " ", "world"]
    repeat for each w in words:
        set result to result + w
    if result is equal to "hello world":
        respond with "pass"

to test string build large:
    set result to ""
    set i to 0
    while i is below 200:
        set result to result + "a"
        set i to i + 1
    if len(result) is equal to 200:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Loop with Stop and Skip
// ═══════════════════════════════════════════════════════════

to test repeat while with stop:
    set n to 0
    repeat while n is below 100:
        set n to n + 1
        if n is equal to 7:
            stop
    if n is equal to 7:
        respond with "pass"

to test repeat while with skip:
    set total to 0
    set i to 0
    repeat while i is below 10:
        set i to i + 1
        if i % 2 is equal to 0:
            skip
        set total to total + i
    // Odd numbers 1..9: 1+3+5+7+9 = 25
    if total is equal to 25:
        respond with "pass"

to test for each with skip:
    set total to 0
    set items to [1, 2, 3, 4, 5]
    repeat for each item in items:
        if item is equal to 3:
            skip
        set total to total + item
    // 1+2+4+5 = 12
    if total is equal to 12:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Repeat While List Accumulation (from C tests)
// ═══════════════════════════════════════════════════════════

to test repeat while list accumulation:
    set result to []
    set i to 0
    repeat while i is below 4:
        set result to result + [i]
        set i to i + 1
    if len(result) is equal to 4:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// While After Repeat For Each (Bug C from C tests)
// ═══════════════════════════════════════════════════════════

to test while after repeat for each:
    set total to 0
    set items to [1, 2, 3]
    repeat for each item in items:
        set total to total + item
    set count to 3
    while count is above 0:
        set total to total + count
        set count to count - 1
    // 6 + 6 = 12
    if total is equal to 12:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// slice() Function (from C tests)
// ═══════════════════════════════════════════════════════════

to test slice basic:
    set vals to [100, 110, 120, 130, 140]
    set sliced to slice(vals, 2, 5)
    if len(sliced) is equal to 3:
        if sliced[0] is equal to 120 and sliced[2] is equal to 140:
            respond with "pass"

to test slice start only:
    set vals to [10, 20, 30, 40, 50]
    set sliced to slice(vals, 3)
    if len(sliced) is equal to 2:
        respond with "pass"

to test slice inline list:
    set sliced to slice([10, 20, 30, 40, 50], 1, 4)
    if len(sliced) is equal to 3:
        respond with "pass"

to test slice negative index:
    set vals to [10, 20, 30, 40, 50]
    set sliced to slice(vals, -3)
    if len(sliced) is equal to 3:
        respond with "pass"

to test slice rsi pattern:
    set prices to [100, 102, 99, 97, 101, 103, 98]
    set recent to slice(prices, 3)
    if len(recent) is equal to 4:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// sort_by on List of Maps (from C tests)
// ═══════════════════════════════════════════════════════════

to test sort by field:
    set items to [{"n": "c", "s": 3}, {"n": "a", "s": 1}, {"n": "b", "s": 2}]
    set sorted to sort_by(items, "s")
    if sorted[0].n is equal to "a":
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Numeric Types Across Run Boundary (Bug B from C tests)
// ═══════════════════════════════════════════════════════════

to make_list:
    respond with [10, 20, 30]

to test list nums survive run boundary:
    run make_list
    set total to result[0] + result[1] + result[2]
    if total is equal to 60:
        respond with "pass"

to get_data:
    respond with {"values": [1, 2, 3]}

to test map list run:
    run get_data
    if len(result.values) is equal to 3:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// try/otherwise Syntax (from C tests)
// ═══════════════════════════════════════════════════════════

to test try otherwise happy path:
    try:
        set x to 42
    otherwise:
        set x to -1
    if x is equal to 42:
        respond with "pass"
