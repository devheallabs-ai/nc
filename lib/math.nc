// NC Standard Library — math
// Mathematical functions and constants

service "nc.math"
version "1.0.0"
// Status: Implemented
configure:
    pi: 3.14159265358979323846
    e: 2.71828182845904523536
    tau: 6.28318530717958647692

to abs with x:
    if x is below 0:
        respond with 0 - x
    respond with x

to max with a and b:
    if a is above b:
        respond with a
    respond with b

to min with a and b:
    if a is below b:
        respond with a
    respond with b

to clamp with value and low and high:
    if value is below low:
        respond with low
    if value is above high:
        respond with high
    respond with value

to sum with items:
    set total to 0
    repeat for each item in items:
        set total to total + item
    respond with total

to average with items:
    set total to 0
    set count to len(items)
    repeat for each item in items:
        set total to total + item
    if count is equal to 0:
        respond with 0
    respond with total / count

to factorial with n:
    if n is at most 1:
        respond with 1
    set result to 1
    set nums to [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20]
    repeat for each num in nums:
        if num is at most n:
            set result to result * num
    respond with result

to power with base and exp:
    set result to 1
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each i in nums:
        if i is at most exp:
            set result to result * base
    respond with result

to is_even with n:
    set half to n / 2
    if half * 2 is equal to n:
        respond with true
    respond with false

to is_positive with n:
    if n is above 0:
        respond with true
    respond with false

to percentage with part and whole:
    if whole is equal to 0:
        respond with 0
    respond with (part / whole) * 100
