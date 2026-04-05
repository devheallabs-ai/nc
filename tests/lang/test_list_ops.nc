// Test: List operations
// Verifies list creation, access, append, sort, reverse, slicing, len

service "test-list-ops"
version "1.0.0"

to test create list:
    set items to [1, 2, 3, 4, 5]
    respond with len(items)

to test index access:
    set colors to ["red", "green", "blue"]
    respond with colors[1]

to test negative index:
    set nums to [10, 20, 30, 40, 50]
    respond with nums[-1]

to test append:
    set items to [1, 2, 3]
    append 4 to items
    respond with len(items)

to test append value:
    set items to [10, 20]
    append 30 to items
    respond with items[2]

to test sort:
    set nums to [5, 3, 1, 4, 2]
    set sorted to sort(nums)
    respond with sorted[0]

to test reverse:
    set items to [1, 2, 3]
    set rev to reverse(items)
    respond with rev[0]

to test slice:
    set nums to [10, 20, 30, 40, 50]
    set sub to nums[1:3]
    respond with len(sub)

to test slice values:
    set nums to [10, 20, 30, 40, 50]
    set sub to nums[1:4]
    respond with sub[0]

to test empty list:
    set empty to []
    respond with len(empty)

to test mixed type list:
    set mixed to [1, "two", true, 4.0]
    respond with len(mixed)

to test list contains string:
    set fruits to ["apple", "banana", "cherry"]
    respond with contains("apple,banana,cherry", "banana")

to test nested list:
    set matrix to [[1, 2], [3, 4], [5, 6]]
    respond with matrix[1][0]

to test list in loop:
    set total to 0
    set nums to [10, 20, 30]
    repeat for each n in nums:
        set total to total + n
    respond with total

to test range function:
    set nums to range(5)
    respond with len(nums)

to test range values:
    set nums to range(3)
    respond with nums[2]

to test first and last:
    set items to ["a", "b", "c", "d"]
    set f to items[0]
    set l to items[-1]
    respond with f + l
