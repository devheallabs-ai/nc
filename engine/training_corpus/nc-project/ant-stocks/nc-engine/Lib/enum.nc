// NC Standard Library — enum
// Named constants

service "nc.enum"
version "1.0.0"

to is_valid with value and allowed:
    purpose: "Check if value is in the allowed list"
    repeat for each option in allowed:
        if value is equal to option:
            respond with true
    respond with false

to from_text with text and mapping:
    purpose: "Convert text to its mapped value"
    respond with text
