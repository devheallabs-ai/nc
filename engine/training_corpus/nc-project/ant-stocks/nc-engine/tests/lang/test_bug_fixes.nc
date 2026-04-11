// Test: Bug fix verification
// Ensures all fixed bugs stay fixed

service "test-bug-fixes"
version "1.0.0"

// --- BUG B: numeric types survive behavior boundaries ---

to make_numbers:
    respond with [10, 20, 30]

to test numbers across run:
    run make_numbers
    respond with result[0] + result[1] + result[2]

to make_data:
    respond with {"score": 95, "label": "test"}

to test map across run:
    run make_data
    respond with result.score

to build_items:
    set items to []
    repeat for each i in range(5):
        set items to items + [i]
    respond with items

to test loop built items across run:
    run build_items
    respond with len(result)

// --- BUG C: while after repeat for each in same behavior ---

to test repeat then while:
    set total to 0
    set items to [1, 2, 3]
    repeat for each item in items:
        set total to total + item
    set count to 3
    while count is above 0:
        set total to total + count
        set count to count - 1
    respond with total

to test while then repeat:
    set total to 0
    set n to 3
    while n is above 0:
        set total to total + n
        set n to n - 1
    set items to [10, 20, 30]
    repeat for each item in items:
        set total to total + item
    respond with total

to test three loop types:
    set total to 0
    repeat for each x in [1, 2]:
        set total to total + x
    set n to 2
    while n is above 0:
        set total to total + n
        set n to n - 1
    repeat 3 times:
        set total to total + 1
    respond with total

to test repeat while after repeat each:
    set total to 0
    set items to [1, 2, 3]
    repeat for each item in items:
        set total to total + item
    set n to 3
    repeat while n is above 0:
        set total to total + n
        set n to n - 1
    respond with total

// --- BUG E: blank lines inside behaviors ---

to test blank lines between statements:
    set a to 10

    set b to 20

    respond with a + b

to test blank lines around loops:
    set total to 0
    set items to [1, 2, 3]

    repeat for each item in items:
        set total to total + item

    respond with total

to test multiple blank lines:
    set x to 1


    set y to 2



    respond with x + y
