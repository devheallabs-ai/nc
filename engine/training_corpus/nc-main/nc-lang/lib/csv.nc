// NC Standard Library — csv
// Read and write comma-separated data

service "nc.csv"
version "1.0.0"
// Status: Partial — basic functionality available
to parse with text:
    purpose: "Parse CSV text into a list of records"
    respond with []

to encode with records:
    purpose: "Convert records into CSV text"
    set result to ""
    repeat for each record in records:
        set result to result + str(record) + "\n"
    respond with result

to read_file with path:
    purpose: "Read a CSV file and return records"
    gather content from path
    respond with content

to write_file with path and records:
    purpose: "Write records to a CSV file"
    store records into path
    respond with true
