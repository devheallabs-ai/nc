// Functional coverage: synonym engine and plain-language interoperability
// Uses lexer aliases like def/function/fn/else/print/return/null.

service "test-synonym-interop"
version "1.0.0"

def greet with name:
    print "hello " + name
    return "hello " + name

function add with a, b:
    return a + b

fn classify with score:
    if score is above 90:
        return "A"
    else:
        return "B"

to test_def_alias:
    run greet with "nc"
    respond with result

to test_function_alias:
    run add with 8, 5
    respond with result

to test_else_alias:
    run classify with 77
    respond with result

to test_null_alias:
    set value to null
    if value is equal nothing:
        respond with "none"
    otherwise:
        respond with "unexpected"
