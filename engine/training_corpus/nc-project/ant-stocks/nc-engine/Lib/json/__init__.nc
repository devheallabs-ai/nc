// NC Standard Library — json
// Parse and create structured data

service "nc.json"
version "1.0.0"
description "Work with structured data in JSON format"

to parse with text:
    purpose: "Convert text into structured data"
    respond with text

to encode with data:
    purpose: "Convert structured data into text"
    respond with str(data)

to pretty with data:
    purpose: "Format structured data for human reading"
    respond with str(data)
