// Test: Control flow — comprehensive
// Verifies if/otherwise, match/when, while, repeat, stop, skip

service "test-control-flow"
version "1.0.0"

// --- If/Otherwise ---

to test if true:
    if true:
        respond with "yes"

to test if false:
    if false:
        respond with "yes"
    otherwise:
        respond with "no"

to test if number comparison:
    set temp to 72
    if temp is above 80:
        respond with "hot"
    otherwise:
        if temp is above 60:
            respond with "warm"
        otherwise:
            respond with "cold"

to test at least:
    set age to 18
    if age is at least 18:
        respond with "adult"
    otherwise:
        respond with "minor"

to test at most:
    set speed to 55
    if speed is at most 60:
        respond with "legal"
    otherwise:
        respond with "speeding"

to test not equal:
    set status to "active"
    if status is not equal to "banned":
        respond with "allowed"
    otherwise:
        respond with "denied"

to test and operator:
    set x to 5
    if x is above 0 and x is below 10:
        respond with "single digit"
    otherwise:
        respond with "other"

to test or operator:
    set role to "admin"
    if role is equal to "admin" or role is equal to "superadmin":
        respond with "access granted"
    otherwise:
        respond with "denied"

to test not operator:
    set done to false
    if not done:
        respond with "still working"
    otherwise:
        respond with "finished"

// --- Match/When ---

to test match basic:
    set color to "red"
    match color:
        when "red":
            respond with "stop"
        when "green":
            respond with "go"
        when "yellow":
            respond with "caution"

to test match otherwise:
    set code to 404
    match code:
        when 200:
            respond with "ok"
        when 404:
            respond with "not found"

to test match number:
    set day to 3
    match day:
        when 1:
            respond with "Monday"
        when 2:
            respond with "Tuesday"
        when 3:
            respond with "Wednesday"

// --- While ---

to test while countdown:
    set n to 5
    set result to 0
    while n is above 0:
        set result to result + n
        set n to n - 1
    respond with result

to test while power:
    set base to 2
    set result to 1
    set i to 0
    while i is below 10:
        set result to result * base
        set i to i + 1
    respond with result

// --- Repeat with stop/skip ---

to test stop in loop:
    set found to "none"
    set items to ["a", "b", "target", "d"]
    repeat for each item in items:
        if item is equal to "target":
            set found to item
            stop
    respond with found

to test skip in loop:
    set total to 0
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each n in nums:
        if n is below 5:
            skip
        set total to total + n
    respond with total

to test nested loops:
    set count to 0
    set outer to [1, 2, 3]
    set inner to [10, 20]
    repeat for each a in outer:
        repeat for each b in inner:
            set count to count + 1
    respond with count
