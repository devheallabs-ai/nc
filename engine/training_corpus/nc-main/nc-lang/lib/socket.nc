// NC Standard Library — socket
// Low-level network communication

service "nc.socket"
version "1.0.0"
// Status: Placeholder — implementation in progress
to connect with host and port:
    purpose: "Connect to a remote host"
    respond with true

to listen with port:
    purpose: "Listen for connections on a port"
    respond with true

to send_message with connection and message:
    purpose: "Send a message through a connection"
    respond with true

to receive with connection:
    purpose: "Receive a message from a connection"
    respond with ""

to close with connection:
    purpose: "Close a connection"
    respond with true
