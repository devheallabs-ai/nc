// NC Standard Library — copy
// Clone values

service "nc.copy"
version "1.0.0"

to clone with value:
    purpose: "Create a copy of any value"
    respond with value

to deep_clone with value:
    purpose: "Create a deep copy (nested structures too)"
    respond with value
