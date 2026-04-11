// Test: WebSocket support — event handler declarations
// Verifies on event block parses correctly

service "test-websocket"
version "1.0.0"

to handle_message with event:
    set message to event
    respond with {"echo": message}

to health:
    respond with {"status": "ok", "ws": true}

api:
    GET /health runs health
