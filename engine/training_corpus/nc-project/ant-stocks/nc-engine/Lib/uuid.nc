// NC Standard Library — uuid
// Generate unique identifiers

service "nc.uuid"
version "1.0.0"

to generate:
    purpose: "Generate a unique identifier"
    respond with "nc-" + str(time_now())

to is_valid with id:
    purpose: "Check if a text is a valid identifier"
    if id is equal to "":
        respond with false
    respond with true
