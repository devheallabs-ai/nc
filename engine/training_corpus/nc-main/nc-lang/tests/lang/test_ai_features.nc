// Test: AI-related features and constructs
// Verifies NC AI integration, model behaviors, and AI builtins

service "test-ai-features"
version "1.0.0"

to test str conversion:
    set x to 42
    set result to str(x)
    respond with result

to test str concat:
    set name to "world"
    set greeting to "hello " + name
    respond with greeting

to test number str concat:
    set count to 5
    set msg to "items: " + str(count)
    respond with msg

to test type check int:
    set x to 42
    respond with type(x)

to test type check string:
    set x to "hello"
    respond with type(x)

to test type check list:
    set x to [1, 2, 3]
    respond with type(x)

to test type check map:
    set x to {"a": 1}
    respond with type(x)

to test len string:
    set x to "hello"
    respond with len(x)

to test len list:
    set x to [1, 2, 3, 4]
    respond with len(x)

to test upper:
    set x to "hello"
    respond with upper(x)

to test lower:
    set x to "HELLO"
    respond with lower(x)

to test abs positive:
    respond with abs(42)

to test abs negative:
    respond with abs(-42)

to test min two:
    respond with min(3, 7)

to test max two:
    respond with max(3, 7)

to test round:
    respond with round(3.7)

to test int conversion:
    respond with int("42")

to test float conversion:
    respond with float("3.14")

to test contains true:
    set x to "hello world"
    respond with contains(x, "world")

to test contains false:
    set x to "hello world"
    respond with contains(x, "xyz")

to test split:
    set parts to split("a,b,c", ",")
    respond with len(parts)

to test join:
    set items to ["x", "y", "z"]
    respond with join(items, "-")

to test replace:
    set x to "hello world"
    respond with replace(x, "world", "NC")

to test trim:
    set x to "  hello  "
    respond with trim(x)

to test keys:
    set m to {"a": 1, "b": 2}
    respond with len(keys(m))

to test values:
    set m to {"x": 10, "y": 20}
    respond with len(values(m))
