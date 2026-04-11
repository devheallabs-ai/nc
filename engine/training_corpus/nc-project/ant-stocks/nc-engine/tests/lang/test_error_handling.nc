// Test: Error handling — try/on_error/finally
// Verifies error catching, recovery, and finally blocks

service "test-error-handling"
version "1.0.0"

to test try success:
    try:
        set x to 42
        respond with x
    on_error:
        respond with "error"

to test try with finally:
    set trace to "start"
    try:
        set x to 10
    finally:
        set trace to trace + "-done"
    respond with trace

to test error recovery:
    set result to "ok"
    try:
        gather data from "nonexistent-source"
    on_error:
        set result to "recovered"
    respond with result

to test nested try:
    set result to "none"
    try:
        try:
            set x to 1
        on_error:
            set result to "inner error"
        set result to "outer ok"
    on_error:
        set result to "outer error"
    respond with result

to test finally always runs:
    set cleaned to false
    try:
        set x to 42
    finally:
        set cleaned to true
    respond with cleaned
