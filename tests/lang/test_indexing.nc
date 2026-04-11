// Test: Indexing — list[i], map["key"], string[i]

service "test-indexing"

to test list index:
    set items to [10, 20, 30]
    respond with items[1]

to test string index:
    set word to "hello"
    respond with word[0]

to test map bracket:
    set user to {"name": "Alice", "age": 30}
    respond with user["name"]
