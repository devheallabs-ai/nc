// Test: Variable resolution inside map literals
// This is the #1 critical fix — without this, no service can return dynamic JSON

service "test-map-variables"

to test variable in map:
    set name to "Alice"
    set result to {"greeting": name}
    respond with result

to test multiple variables:
    set user to "Bob"
    set age to 25
    set active to true
    set result to {"user": user, "age": age, "active": active}
    respond with result

to test expression in map:
    set x to 10
    set y to 20
    set result to {"sum": x + y}
    respond with result

to test nested data:
    set status to "healthy"
    set code to 200
    set response to {"status": status, "code": code, "message": "OK"}
    respond with response
