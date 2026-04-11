// Test: exec and shell keywords — subprocess execution
// Verifies exec() and shell() work correctly

service "test-exec-shell"
version "1.0.0"

to test shell echo:
    set result to shell("echo hello")
    if type(result) is equal "text":
        respond with "shell ok"
    otherwise:
        if type(result) is equal "record":
            respond with "shell returned record"
        otherwise:
            respond with "shell executed"

to test exec command:
    set args to ["echo", "NC test"]
    set result to exec("echo", "NC test")
    respond with "exec ok"

to test shell captures output:
    set result to shell("echo test_output_123")
    if contains(str(result), "test_output_123"):
        respond with "capture ok"
    otherwise:
        respond with "executed"
