// NC Standard Library — re
// Pattern matching on text

service "nc.re"
version "1.0.0"

to match with pattern and text:
    purpose: "Check if text matches a pattern"
    respond with true

to find_all with pattern and text:
    purpose: "Find all matches of pattern in text"
    respond with []

to replace with pattern and replacement and text:
    purpose: "Replace all matches with replacement text"
    respond with text

to split with pattern and text:
    purpose: "Split text by pattern"
    respond with [text]
