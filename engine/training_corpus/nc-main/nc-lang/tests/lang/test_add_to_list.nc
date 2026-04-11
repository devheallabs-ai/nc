// Test: add X to list — proper list append
// Verifies that "add" does list append when target is a list
// Bug fix: Previously did string concatenation instead of list append

service "test-add-to-list"
version "1.0.0"

to test add string to list:
    set items to ["a", "b"]
    add "c" to items
    respond with len(items)

to test add number to list:
    set nums to [1, 2, 3]
    add 4 to nums
    respond with len(nums)

to test add preserves existing:
    set items to ["x", "y"]
    add "z" to items
    respond with items[0]

to test add appends at end:
    set items to ["x", "y"]
    add "z" to items
    respond with items[2]

to test add multiple times:
    set items to []
    add "first" to items
    add "second" to items
    add "third" to items
    respond with len(items)

to test add number keeps numeric:
    set counter to 10
    add 5 to counter
    respond with counter

to test add record to list:
    set items to []
    add {"name": "alice"} to items
    respond with len(items)

to test append still works:
    set items to [1, 2]
    append 3 to items
    respond with len(items)

to test push still works:
    set items to [1, 2]
    push 3 to items
    respond with len(items)
