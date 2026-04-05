// NC Standard Library — threading
// Concurrent execution

service "nc.threading"
version "1.0.0"
// Status: Implemented
to spawn with behavior:
    purpose: "Run a behavior in a separate thread"
    run behavior
    respond with result

to parallel with behaviors:
    purpose: "Run multiple behaviors at the same time"
    set results to []
    repeat for each behavior in behaviors:
        run behavior
        set results to results + [result]
    respond with results

to lock:
    purpose: "Create a mutual exclusion lock"
    respond with true

to wait_all with tasks:
    purpose: "Wait for all tasks to complete"
    set results to []
    repeat for each task in tasks:
        set results to results + [task]
    respond with results
