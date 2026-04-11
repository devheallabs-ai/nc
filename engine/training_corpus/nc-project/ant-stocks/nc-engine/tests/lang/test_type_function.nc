service "test-type"
version "1.0.0"

to test_type_number:
    set t to type(42)
    respond with t

to test_type_text:
    set t to type("hello")
    respond with t

to test_type_list:
    set t to type([1, 2, 3])
    respond with t

to test_type_record:
    set t to type({"key": "val"})
    respond with t

to test_type_yesno:
    set t to type(yes)
    respond with t

to test_type_none:
    set t to type(nothing)
    respond with t
