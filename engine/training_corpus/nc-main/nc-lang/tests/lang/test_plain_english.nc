// Test: Plain English syntax — write code like you talk
// These are the natural language features that make NC unique

service "test-plain-english"

to test add to:
    set score to 10
    add 5 to score
    respond with score

to test increase by:
    set count to 0
    increase count by 3
    increase count by 7
    respond with count

to test decrease by:
    set lives to 5
    decrease lives by 2
    respond with lives

to test is empty:
    set items to []
    if items is empty:
        respond with "empty"
    otherwise:
        respond with "not empty"

to test is not empty:
    set items to [1, 2, 3]
    if items is not empty:
        respond with "has items"
    otherwise:
        respond with "nothing"

to test short comparison:
    set name to "Alice"
    if name is "Alice":
        respond with "found Alice"
    otherwise:
        respond with "not Alice"

to test is with number:
    set x to 42
    if x is 42:
        respond with "match"
    otherwise:
        respond with "no match"

to test keyword as variable:
    set second to "hello"
    set text to "world"
    respond with second + " " + text

to test build list:
    set results to []
    set nums to [1, 2, 3, 4, 5]
    repeat for each n in nums:
        if n is above 2:
            append n to results
    respond with results

to test accumulate:
    set total to 0
    set prices to [10, 20, 30, 40]
    repeat for each price in prices:
        add price to total
    respond with total
