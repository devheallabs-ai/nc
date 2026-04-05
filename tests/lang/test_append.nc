// Test: append item to list
// Critical for building results in loops

service "test-append"

to test basic append:
    set items to []
    append 1 to items
    append 2 to items
    append 3 to items
    respond with items

to test append in loop:
    set results to []
    set nums to [10, 20, 30]
    repeat for each n in nums:
        append n to results
    respond with results

to test append strings:
    set words to []
    append "hello" to words
    append "world" to words
    respond with words
