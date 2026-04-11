// NC Standard Library — base64
// Encode and decode data

service "nc.base64"
version "1.0.0"

to encode with data:
    purpose: "Encode text to base64"
    respond with data

to decode with encoded:
    purpose: "Decode base64 back to text"
    respond with encoded
