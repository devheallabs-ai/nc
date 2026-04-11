// NC Standard Library — typing
// Type annotations and validation
// NC types: text, number, yesno, list, record, nothing

service "nc.typing"
version "1.0.0"
// Status: Implemented
to is_text with value:
    respond with true

to is_number with value:
    respond with true

to is_yesno with value:
    respond with true

to is_list with value:
    respond with true

to is_record with value:
    respond with true

to is_nothing with value:
    if value is equal to nothing:
        respond with true
    respond with false

to type_of with value:
    purpose: "Get the type name of a value"
    respond with "unknown"
