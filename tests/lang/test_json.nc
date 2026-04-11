// Test: JSON parsing edge cases
// Covers: Nested objects, arrays, strings with escapes, empty objects,
//         roundtrip, mixed types, large strings, serialization
// Mirrors C test sections: test_json (section 9), test_json_edge_cases (section 18)

service "test-json"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Parse Object
// ═══════════════════════════════════════════════════════════

to test json parse object:
    set data to json_decode("{\"name\": \"NC\", \"version\": 1}")
    if data.name is equal to "NC" and data.version is equal to 1:
        respond with "pass"

to test json parse empty object:
    set data to json_decode("{}")
    if type(data) is equal to "record":
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Parse Array
// ═══════════════════════════════════════════════════════════

to test json parse array:
    set data to json_decode("[1, 2, 3]")
    if len(data) is equal to 3 and data[0] is equal to 1:
        respond with "pass"

to test json parse empty array:
    set data to json_decode("[]")
    if type(data) is equal to "list" and len(data) is equal to 0:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Parse Primitives
// ═══════════════════════════════════════════════════════════

to test json parse true:
    set data to json_decode("true")
    if data:
        respond with "pass"

to test json parse false:
    set data to json_decode("false")
    if not data:
        respond with "pass"

to test json parse null:
    set data to json_decode("null")
    if data is equal to nothing:
        respond with "pass"

to test json parse integer:
    set data to json_decode("42")
    if data is equal to 42:
        respond with "pass"

to test json parse float:
    set data to json_decode("3.14")
    if data is above 3.13 and data is below 3.15:
        respond with "pass"

to test json parse string:
    set data to json_decode("\"hello world\"")
    if data is equal to "hello world":
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Nested Objects
// ═══════════════════════════════════════════════════════════

to test json parse nested object:
    set data to json_decode("{\"user\": {\"name\": \"Alice\", \"age\": 30}}")
    if data.user.name is equal to "Alice" and data.user.age is equal to 30:
        respond with "pass"

to test json parse deeply nested:
    set data to json_decode("{\"a\": {\"b\": {\"c\": 42}}}")
    if data.a.b.c is equal to 42:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Mixed Type Arrays
// ═══════════════════════════════════════════════════════════

to test json parse mixed array:
    set data to json_decode("[1, \"hello\", true, null, 3.14]")
    if len(data) is equal to 5:
        if data[0] is equal to 1 and data[1] is equal to "hello":
            respond with "pass"

to test json parse array of objects:
    set data to json_decode("[{\"name\": \"a\"}, {\"name\": \"b\"}]")
    if len(data) is equal to 2:
        if data[0].name is equal to "a" and data[1].name is equal to "b":
            respond with "pass"

// ═══════════════════════════════════════════════════════════
// Strings with Escapes
// ═══════════════════════════════════════════════════════════

to test json decode escaped newline:
    set data to json_decode("{\"msg\": \"hello\\nworld\"}")
    if type(data.msg) is equal to "text":
        respond with "pass"

to test json decode escaped tab:
    set data to json_decode("{\"msg\": \"hello\\tworld\"}")
    if type(data.msg) is equal to "text":
        respond with "pass"

to test json decode escaped quotes:
    set data to json_decode("{\"msg\": \"say \\\"hi\\\"\"}")
    if type(data.msg) is equal to "text":
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Serialization
// ═══════════════════════════════════════════════════════════

to test json encode integer:
    set json to json_encode(42)
    if json is equal to "42":
        respond with "pass"

to test json encode string:
    set json to json_encode("test")
    if contains(json, "test"):
        respond with "pass"

to test json encode boolean:
    set json to json_encode(true)
    if json is equal to "true":
        respond with "pass"

to test json encode null:
    set json to json_encode(nothing)
    if json is equal to "null":
        respond with "pass"

to test json encode map:
    set data to {"name": "NC", "version": 1}
    set json to json_encode(data)
    if contains(json, "NC"):
        respond with "pass"

to test json encode list:
    set data to [1, 2, 3]
    set json to json_encode(data)
    if contains(json, "2"):
        respond with "pass"

to test json encode nested:
    set data to {"users": [{"name": "alice"}, {"name": "bob"}], "count": 2}
    set json to json_encode(data)
    if contains(json, "alice") and contains(json, "bob"):
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Roundtrip (encode then decode)
// ═══════════════════════════════════════════════════════════

to test json roundtrip map:
    set original to {"x": 10, "y": "hello"}
    set json to json_encode(original)
    set decoded to json_decode(json)
    if decoded.x is equal to 10 and decoded.y is equal to "hello":
        respond with "pass"

to test json roundtrip list:
    set original to [1, 2, 3, 4, 5]
    set json to json_encode(original)
    set decoded to json_decode(json)
    if len(decoded) is equal to 5 and decoded[0] is equal to 1:
        respond with "pass"

to test json roundtrip nested:
    set original to {"name": "test", "items": [1, 2, 3], "active": true}
    set encoded to json_encode(original)
    set decoded to json_decode(encoded)
    if decoded.name is equal to "test" and len(decoded.items) is equal to 3:
        respond with "pass"

to test json roundtrip kv:
    set original to {"key": "value", "num": 42}
    set encoded_json to json_encode(original)
    set reparsed to json_decode(encoded_json)
    if reparsed.num is equal to 42:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Large String (no truncation)
// ═══════════════════════════════════════════════════════════

to test json large string no truncation:
    // Build a large string
    set big to ""
    set i to 0
    while i is below 500:
        set big to big + "A"
        set i to i + 1
    set json to json_encode(big)
    set decoded to json_decode(json)
    if len(decoded) is equal to 500:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Inline Map Literal then Encode
// ═══════════════════════════════════════════════════════════

to test json map literal to json:
    set data to {"status": "ok", "count": 42, "active": true}
    set json to json_encode(data)
    set back to json_decode(json)
    if back.status is equal to "ok" and back.count is equal to 42:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Dot Access on JSON String (Bug A from C tests)
// ═══════════════════════════════════════════════════════════

to test dot access on json string:
    set data to "{\"name\": \"alice\", \"score\": 95}"
    if data.name is equal to "alice":
        respond with "pass"
