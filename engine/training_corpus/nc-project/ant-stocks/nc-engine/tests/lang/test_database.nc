// Test: database operations — store and retrieve
// Verifies store into and data retrieval

service "test-database"
version "1.0.0"

to test store string:
    store "test value" into "db_key_1"
    respond with "stored"

to test store record:
    set user to {"name": "Alice", "age": 30}
    store user into "user_alice"
    respond with "record stored"

to test store list:
    set items to [1, 2, 3, 4, 5]
    store items into "numbers"
    respond with "list stored"

to test multiple stores:
    store "a" into "key_a"
    store "b" into "key_b"
    store "c" into "key_c"
    respond with "multi stored"
