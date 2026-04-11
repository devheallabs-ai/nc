// Test: Cryptographic hashing functions
// Verifies SHA-256, password hashing, HMAC

service "test-hashing"
version "1.0.0"

to test sha256 basic:
    set h to hash_sha256("hello")
    respond with len(h)

to test sha256 deterministic:
    set a to hash_sha256("test data")
    set b to hash_sha256("test data")
    if a is equal to b:
        respond with "deterministic"

to test sha256 different inputs:
    set a to hash_sha256("input1")
    set b to hash_sha256("input2")
    if a is not equal to b:
        respond with "different"

to test password hash format:
    set stored to hash_password("my_password")
    respond with contains(stored, "$nc$")

to test hmac basic:
    set mac to hash_hmac("message", "secret_key")
    respond with len(mac)

to test hmac deterministic:
    set a to hash_hmac("data", "key")
    set b to hash_hmac("data", "key")
    if a is equal to b:
        respond with "match"
