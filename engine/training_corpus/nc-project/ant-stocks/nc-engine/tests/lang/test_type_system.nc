// Test: Type system — type(), conversions, truthiness
// Verifies NC's dynamic type system

service "test-type-system"
version "1.0.0"

// --- type() function ---

to test type number:
    respond with type(42)

to test type float:
    respond with type(3.14)

to test type string:
    respond with type("hello")

to test type boolean:
    respond with type(true)

to test type list:
    respond with type([1, 2, 3])

to test type record:
    respond with type({"a": 1})

to test type none:
    respond with type(nothing)

// --- Conversions ---

to test int from string:
    respond with int("42")

to test int from float:
    respond with int(3.9)

to test str from number:
    respond with str(42)

to test str from bool:
    respond with str(true)

// --- Truthiness ---

to test zero falsy:
    if 0:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test nonzero truthy:
    if 42:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test empty string falsy:
    if "":
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test nonempty string truthy:
    if "hello":
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test empty list falsy:
    if []:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test nonempty list truthy:
    if [1]:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test true truthy:
    if true:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test false falsy:
    if false:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test nothing falsy:
    if nothing:
        respond with "truthy"
    otherwise:
        respond with "falsy"
