// Test: Symbol-based comparison operators (>, <, >=, <=, ==, !=)
// Verifies that all C-style comparison operators work correctly
// Bug fix: These symbols were missing from the lexer

service "test-symbol-operators"
version "1.0.0"

// ── Greater Than (>) ──

to test gt true:
    set x to 10
    if x > 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test gt false:
    set x to 3
    if x > 5:
        respond with "fail"
    otherwise:
        respond with "pass"

to test gt equal:
    set x to 5
    if x > 5:
        respond with "fail"
    otherwise:
        respond with "pass"

// ── Less Than (<) ──

to test lt true:
    set x to 3
    if x < 10:
        respond with "pass"
    otherwise:
        respond with "fail"

to test lt false:
    set x to 20
    if x < 10:
        respond with "fail"
    otherwise:
        respond with "pass"

to test lt equal:
    set x to 10
    if x < 10:
        respond with "fail"
    otherwise:
        respond with "pass"

// ── Greater Than or Equal (>=) ──

to test gte above:
    set x to 10
    if x >= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test gte equal:
    set x to 5
    if x >= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test gte below:
    set x to 3
    if x >= 5:
        respond with "fail"
    otherwise:
        respond with "pass"

// ── Less Than or Equal (<=) ──

to test lte below:
    set x to 3
    if x <= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test lte equal:
    set x to 5
    if x <= 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test lte above:
    set x to 10
    if x <= 5:
        respond with "fail"
    otherwise:
        respond with "pass"

// ── Equal (==) ──

to test eqeq true:
    set x to 42
    if x == 42:
        respond with "pass"
    otherwise:
        respond with "fail"

to test eqeq false:
    set x to 42
    if x == 99:
        respond with "fail"
    otherwise:
        respond with "pass"

to test eqeq string:
    set name to "alice"
    if name == "alice":
        respond with "pass"
    otherwise:
        respond with "fail"

// ── Not Equal (!=) ──

to test neq true:
    set x to 10
    if x != 5:
        respond with "pass"
    otherwise:
        respond with "fail"

to test neq false:
    set x to 5
    if x != 5:
        respond with "fail"
    otherwise:
        respond with "pass"

to test neq string:
    set name to "bob"
    if name != "alice":
        respond with "pass"
    otherwise:
        respond with "fail"

// ── Mixed: Symbol operators with expressions ──

to test gt expression:
    set a to 10
    set b to 3
    if a + b > 12:
        respond with "pass"
    otherwise:
        respond with "fail"

to test lt expression:
    set a to 2
    set b to 3
    if a * b < 10:
        respond with "pass"
    otherwise:
        respond with "fail"

// ── Multiple if blocks (previously broken) ──

to test multi if blocks:
    set x to 10
    if x > 5:
        show "first"
    if x > 3:
        show "second"
    if x > 1:
        show "third"
    respond with "pass"

to test multi if mixed:
    set x to 10
    if x > 5:
        show "symbol gt"
    if x is above 3:
        show "keyword above"
    respond with "pass"
