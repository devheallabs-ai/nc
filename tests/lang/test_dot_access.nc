service "test-dot-access"
version "1.0.0"

to test_simple_dot:
    set person to {"name": "Bob", "age": 25}
    respond with person.name

to test_nested_dot:
    set data to {"user": {"profile": {"email": "bob@test.com"}}}
    respond with data.user

to test_dot_after_set:
    set config to {"port": 8080, "host": "localhost"}
    set port to config.port
    respond with port

to test_list_in_map:
    set data to {"items": [10, 20, 30]}
    set items to data.items
    respond with len(items)
