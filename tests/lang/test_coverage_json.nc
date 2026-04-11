// Coverage: JSON parsing and serialization
// Tests the nc_json.c module through json_encode/json_decode

service "test-coverage-json"
version "1.0.0"

// ── Encode basic types ───────────────────────
to test_encode_map:
    set data to {"name": "test", "value": 42}
    respond with json_encode(data)

to test_encode_list:
    respond with json_encode([1, 2, 3])

to test_encode_string:
    respond with json_encode("hello")

to test_encode_number:
    respond with json_encode(42)

to test_encode_bool:
    respond with json_encode(true)

to test_encode_null:
    respond with json_encode(nothing)

to test_encode_nested:
    set data to {"users": [{"name": "alice"}, {"name": "bob"}], "count": 2}
    respond with json_encode(data)

// ── Decode basic types ───────────────────────
to test_decode_object:
    set data to json_decode("{\"x\": 10, \"y\": 20}")
    respond with data.x

to test_decode_array:
    set data to json_decode("[1, 2, 3]")
    respond with len(data)

to test_decode_string:
    set data to json_decode("\"hello\"")
    respond with data

to test_decode_number:
    set data to json_decode("42")
    respond with data

to test_decode_float:
    set data to json_decode("3.14")
    respond with type(data)

to test_decode_bool_true:
    set data to json_decode("true")
    respond with data

to test_decode_bool_false:
    set data to json_decode("false")
    respond with data

to test_decode_null:
    set data to json_decode("null")
    respond with type(data)

to test_decode_nested:
    set data to json_decode("{\"a\": {\"b\": [1, 2, 3]}}")
    respond with data.a.b[1]

// ── Roundtrip ────────────────────────────────
to test_roundtrip:
    set original to {"name": "test", "items": [1, 2, 3], "active": true}
    set encoded to json_encode(original)
    set decoded to json_decode(encoded)
    respond with decoded.name

// ── Escape handling ──────────────────────────
to test_decode_escaped_string:
    set data to json_decode("{\"msg\": \"hello\\nworld\"}")
    respond with type(data.msg)

// ── Invalid JSON ─────────────────────────────
to test_decode_invalid:
    set data to json_decode("not json at all")
    respond with type(data)

to test_decode_empty:
    set data to json_decode("")
    respond with type(data)
