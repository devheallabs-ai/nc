// Test: Repeat loops
// Verifies for-each iteration and accumulation

service "test-loops"
version "1.0.0"

to test repeat sum:
    set total to 0
    set nums to [1, 2, 3, 4, 5]
    repeat for each n in nums:
        set total to total + n
    respond with total

to test repeat strings:
    set result to ""
    set words to ["hello", " ", "world"]
    repeat for each w in words:
        set result to result + w
    respond with result

to test repeat count:
    set count to 0
    set items to [10, 20, 30, 40, 50]
    repeat for each item in items:
        set count to count + 1
    respond with count

to test fibonacci:
    set a to 0
    set b to 1
    set steps to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each i in steps:
        set temp to b
        set b to a + b
        set a to temp
    respond with b
