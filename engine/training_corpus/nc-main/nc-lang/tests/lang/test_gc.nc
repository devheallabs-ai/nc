// Test: Garbage Collection stress tests
// Covers: Creating many objects, verifying cleanup, memory pressure
// Mirrors C test section: test_gc (section 16)

service "test-gc"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// String Allocation Stress
// ═══════════════════════════════════════════════════════════

to test gc string allocation:
    // Allocate many strings and verify they remain valid
    set result to ""
    set i to 0
    while i is below 200:
        set s to "item_" + str(i)
        set result to s
        set i to i + 1
    // The last string should still be valid
    if contains(result, "item_199"):
        respond with "pass"

to test gc string concatenation pressure:
    // Many concatenations create intermediate string objects
    set s to ""
    set i to 0
    while i is below 100:
        set s to s + "x"
        set i to i + 1
    if len(s) is equal to 100:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// List Allocation Stress
// ═══════════════════════════════════════════════════════════

to test gc list allocation:
    // Allocate many lists to stress the GC
    set items to []
    set i to 0
    while i is below 500:
        append i to items
        set i to i + 1
    if len(items) is equal to 500:
        respond with "pass"

to test gc list of lists:
    // Create lists of lists — stresses reference tracking
    set outer to []
    set i to 0
    while i is below 50:
        set inner to [i, i + 1, i + 2]
        append inner to outer
        set i to i + 1
    if len(outer) is equal to 50:
        respond with "pass"

to test gc list replace stress:
    // Repeatedly replace list contents — old lists should be collected
    set data to [1, 2, 3]
    set i to 0
    while i is below 100:
        set data to [i, i + 1, i + 2]
        set i to i + 1
    if data[0] is equal to 99:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Map Allocation Stress
// ═══════════════════════════════════════════════════════════

to test gc map allocation:
    // Create many maps
    set m to {}
    set i to 0
    while i is below 200:
        set key to "key_" + str(i)
        set m[key] to i
        set i to i + 1
    if len(keys(m)) is above 100:
        respond with "pass"

to test gc map replace stress:
    // Repeatedly create and replace maps
    set data to {"a": 1}
    set i to 0
    while i is below 100:
        set data to {"value": i, "index": i + 1}
        set i to i + 1
    if data.value is equal to 99:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Mixed Type Allocation
// ═══════════════════════════════════════════════════════════

to test gc mixed allocations:
    // Allocate strings, lists, and maps together
    set results to []
    set i to 0
    while i is below 100:
        set s to "str_" + str(i)
        set m to {"key": s, "val": i}
        append m to results
        set i to i + 1
    if len(results) is equal to 100:
        set last to results[99]
        if last.val is equal to 99:
            respond with "pass"

// ═══════════════════════════════════════════════════════════
// Scope Cleanup
// ═══════════════════════════════════════════════════════════

to test gc loop scope cleanup:
    // Variables created inside loops should be eligible for GC after loop
    set total to 0
    repeat for each n in [1, 2, 3, 4, 5]:
        set temp to [n, n * 2, n * 3]
        set total to total + len(temp)
    // All temp lists from previous iterations should be collectable
    if total is equal to 15:
        respond with "pass"

to test gc nested loop cleanup:
    set count to 0
    set i to 0
    while i is below 10:
        set j to 0
        while j is below 10:
            set temp to "cell_" + str(i) + "_" + str(j)
            set count to count + 1
            set j to j + 1
        set i to i + 1
    if count is equal to 100:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Multiple Behavior Calls (leak test pattern from C tests)
// ═══════════════════════════════════════════════════════════

to helper with x:
    respond with x * 3

to test gc sequential run calls:
    set total to 0
    repeat for each i in range(20):
        run helper with i
        set total to total + result
    // sum of i*3 for i=0..19 = 3*(0+1+...+19) = 3*190 = 570
    if total is equal to 570:
        respond with "pass"
