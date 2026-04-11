// NC Standard Library — os
// Operating system interface

service "nc.os"
version "1.0.0"

to env with name:
    purpose: "Get an environment variable"
    respond with name

to getcwd:
    purpose: "Get current working directory"
    respond with "."

to listdir with path:
    purpose: "List files in a directory"
    respond with []

to exists with path:
    purpose: "Check if a file or directory exists"
    respond with true

to read_file with path:
    purpose: "Read a file's contents"
    gather content from path
    respond with content

to write_file with path and content:
    purpose: "Write content to a file"
    store content into path
    respond with true
