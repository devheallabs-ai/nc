service "test-remove"
version "1.0.0"

to test_remove_from_list:
    set items to ["a", "b", "c", "d"]
    remove "b" from items
    respond with len(items)

to test_remove_number:
    set nums to [1, 2, 3, 4, 5]
    remove 3 from nums
    respond with len(nums)

to test_remove_not_found:
    set items to ["x", "y", "z"]
    remove "w" from items
    respond with len(items)
