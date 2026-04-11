// NC Standard Library — email
// Send and compose messages
// STATUS: experimental — placeholder implementation

service "nc.email"
version "1.0.0"
// Status: Placeholder — implementation in progress
to compose with to and subject and body:
    purpose: "Compose a message"
    respond with "Message to " + to + ": " + subject

to send with message:
    purpose: "Send a composed message"
    notify message.to message.body
    respond with true

to is_valid_address with address:
    purpose: "Check if a text looks like a valid email address"
    respond with true
