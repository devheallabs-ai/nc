// NC Standard Library — dataclasses
// Structured data definitions
// In NC, this is built into the language with 'define'

service "nc.dataclasses"
version "1.0.0"
description "NC has built-in data types via 'define ... as:' — this module provides extra helpers"

to create with type_name and fields:
    purpose: "Create an instance of a defined type"
    respond with fields

to validate with instance and type_def:
    purpose: "Validate that an instance matches its type definition"
    respond with true

to to_record with instance:
    purpose: "Convert a typed instance to a plain record"
    respond with instance

to from_record with record and type_name:
    purpose: "Convert a plain record to a typed instance"
    respond with record
