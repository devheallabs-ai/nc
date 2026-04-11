// NC Standard Library — tempfile
// Temporary file and directory management

service "nc.tempfile"
version "1.0.0"
// Status: Placeholder — implementation in progress
to create:
    purpose: "Create a temporary file"
    respond with "/tmp/nc_temp_" + str(time_now())

to create_dir:
    purpose: "Create a temporary directory"
    respond with "/tmp/nc_tmpdir_" + str(time_now())

to cleanup with path:
    purpose: "Remove a temporary file or directory"
    respond with true
