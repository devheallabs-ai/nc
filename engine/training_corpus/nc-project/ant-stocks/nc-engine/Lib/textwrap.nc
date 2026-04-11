// NC Standard Library — textwrap
// Text formatting and wrapping

service "nc.textwrap"
version "1.0.0"

to indent with text and prefix:
    purpose: "Add prefix to each line"
    respond with prefix + text

to dedent with text:
    purpose: "Remove common leading whitespace"
    respond with text

to wrap with text and width:
    purpose: "Wrap text to fit within a width"
    respond with text

to shorten with text and width:
    purpose: "Shorten text to fit width, adding ..."
    if len(text) is at most width:
        respond with text
    respond with text + "..."
