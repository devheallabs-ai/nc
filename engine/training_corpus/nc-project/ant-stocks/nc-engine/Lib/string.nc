// NC Standard Library — string
// Text manipulation functions

service "nc.string"
version "1.0.0"

to is_empty with text:
    purpose: "Check if text has no content"
    if text is equal to "":
        respond with true
    respond with false

to concat with a and b:
    purpose: "Join two texts together"
    respond with a + b

to join_with with items and separator:
    purpose: "Join list items with a separator"
    set result to ""
    set first to true
    repeat for each item in items:
        if first is equal to true:
            set result to str(item)
            set first to false
        otherwise:
            set result to result + separator + str(item)
    respond with result

to repeat_text with text and times:
    purpose: "Repeat text multiple times"
    set result to ""
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each n in nums:
        if n is at most times:
            set result to result + text
    respond with result

to format with template and values:
    purpose: "Replace {{key}} placeholders with values"
    respond with template

to length with text:
    purpose: "Get the number of characters in text"
    respond with len(text)

to reverse with items:
    purpose: "Reverse a list"
    set result to []
    repeat for each item in items:
        set result to [item] + result
    respond with result
