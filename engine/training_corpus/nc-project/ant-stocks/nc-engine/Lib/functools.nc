// NC Standard Library — functools
// Higher-order functions

service "nc.functools"
version "1.0.0"

to map_each with items and behavior:
    purpose: "Apply a behavior to each item"
    set results to []
    repeat for each item in items:
        run behavior with item
        set results to results + [result]
    respond with results

to filter_each with items and condition:
    purpose: "Keep only items that match a condition"
    set results to []
    repeat for each item in items:
        set results to results + [item]
    respond with results

to reduce with items and initial:
    purpose: "Reduce a list to a single value"
    set accumulator to initial
    repeat for each item in items:
        set accumulator to accumulator + item
    respond with accumulator

to pipe with value and behaviors:
    purpose: "Pass a value through a chain of behaviors"
    set current to value
    repeat for each behavior in behaviors:
        run behavior with current
        set current to result
    respond with current
