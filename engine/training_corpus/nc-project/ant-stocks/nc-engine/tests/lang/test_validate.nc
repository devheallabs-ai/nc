service "test-validate"
version "1.0.0"

to test_validate_all_present:
    set data to {"name": "John", "age": 30, "city": "NYC"}
    set result to validate(data, ["name", "age"])
    respond with result.valid

to test_validate_missing_field:
    set data to {"name": "John"}
    set result to validate(data, ["name", "age", "city"])
    set missing_count to len(result.missing)
    respond with missing_count
