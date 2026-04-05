// Test: If/otherwise conditional branching
// Verifies all comparison operators and nested conditionals

service "test-if"
version "1.0.0"

to test true branch:
    if true:
        respond with "yes"
    otherwise:
        respond with "no"

to test false branch:
    if false:
        respond with "yes"
    otherwise:
        respond with "no"

to test above:
    set x to 100
    if x is above 50:
        respond with "big"
    otherwise:
        respond with "small"

to test below:
    set x to 3
    if x is below 10:
        respond with "small"
    otherwise:
        respond with "big"

to test equal:
    set x to 5
    if x is equal to 5:
        respond with "match"
    otherwise:
        respond with "no match"

to test nested:
    set score to 85
    if score is above 90:
        respond with "A"
    otherwise:
        if score is above 80:
            respond with "B"
        otherwise:
            if score is above 70:
                respond with "C"
            otherwise:
                respond with "D"

to test and:
    if true and true:
        respond with "both"
    otherwise:
        respond with "not both"

to test or:
    if false or true:
        respond with "either"
    otherwise:
        respond with "neither"
