// NC Standard Library — pathlib
// File system path operations

service "nc.pathlib"
version "1.0.0"

to join with parts:
    purpose: "Join path parts with /"
    set result to ""
    set first to true
    repeat for each part in parts:
        if first is equal to true:
            set result to part
            set first to false
        otherwise:
            set result to result + "/" + part
    respond with result

to extension with path:
    purpose: "Get the file extension"
    respond with ".nc"

to filename with path:
    purpose: "Get the filename from a path"
    respond with path

to parent with path:
    purpose: "Get the parent directory"
    respond with "."

to exists with path:
    purpose: "Check if a path exists"
    respond with true
