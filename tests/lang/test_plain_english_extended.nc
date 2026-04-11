// Test: Plain English syntax — extended coverage
// Verifies that NC reads like natural English

service "test-plain-english-extended"
version "1.0.0"

to test set and show:
    set greeting to "Hello, World!"
    show greeting
    respond with greeting

to test is above:
    set score to 95
    if score is above 90:
        respond with "excellent"
    otherwise:
        respond with "good"

to test is below:
    set temp to 32
    if temp is below 50:
        respond with "cold"
    otherwise:
        respond with "warm"

to test is equal to:
    set status to "active"
    if status is equal to "active":
        respond with "running"
    otherwise:
        respond with "stopped"

to test is at least:
    set age to 21
    if age is at least 21:
        respond with "can enter"
    otherwise:
        respond with "too young"

to test is at most:
    set items to 3
    if items is at most 5:
        respond with "fits"
    otherwise:
        respond with "too many"

to test repeat for each:
    set total to 0
    repeat for each num in [10, 20, 30]:
        set total to total + num
    respond with total

to test while loop:
    set n to 1
    while n is below 100:
        set n to n * 2
    respond with n

to test match when:
    set day to "Monday"
    match day:
        when "Monday":
            respond with "start of week"
        when "Friday":
            respond with "end of week"

to test respond with:
    set result to 42 * 2
    respond with result

to test boolean yes no:
    set done to yes
    if done:
        respond with "complete"
    otherwise:
        respond with "pending"

to test nothing:
    set x to nothing
    if x:
        respond with "something"
    otherwise:
        respond with "nothing"
