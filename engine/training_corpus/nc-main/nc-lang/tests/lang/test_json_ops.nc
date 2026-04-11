// Test: JSON encode/decode operations
// Verifies json_encode, json_decode, round-tripping

service "test-json-ops"
version "1.0.0"

to test encode record:
    set data to {"name": "Alice", "age": 30}
    set json to json_encode(data)
    respond with contains(json, "Alice")

to test encode list:
    set items to [1, 2, 3]
    set json to json_encode(items)
    respond with contains(json, "2")

to test decode record:
    set json to "{\"name\": \"Bob\", \"score\": 95}"
    set data to json_decode(json)
    respond with data.name

to test decode score:
    set json to "{\"name\": \"Bob\", \"score\": 95}"
    set data to json_decode(json)
    respond with data.score

to test roundtrip:
    set original to {"status": "ok", "count": 42}
    set json to json_encode(original)
    set decoded to json_decode(json)
    respond with decoded.count

to test nested json:
    set json to "{\"user\": {\"name\": \"Charlie\", \"active\": true}}"
    set data to json_decode(json)
    respond with data.user.name

to test json array:
    set json to "[10, 20, 30]"
    set data to json_decode(json)
    respond with len(data)

to test encode string:
    set json to json_encode("hello world")
    respond with json

to test encode number:
    set json to json_encode(42)
    respond with json

to test encode boolean:
    set json to json_encode(true)
    respond with json
