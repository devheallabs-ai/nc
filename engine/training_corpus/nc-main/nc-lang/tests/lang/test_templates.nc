service "test-templates"
version "1.0.0"

to test_simple_template:
    set name to "World"
    set msg to "Hello, {{name}}"
    respond with msg

to test_template_with_map:
    set user to {"name": "Alice", "age": 30}
    set msg to "Name: {{user.name}}, Age: {{user.age}}"
    respond with msg

to test_template_in_log:
    set count to 42
    log "The count is {{count}}"
    respond with count

to test_no_template:
    set msg to "No templates here"
    respond with msg
