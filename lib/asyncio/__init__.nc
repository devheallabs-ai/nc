// NC Standard Library — asyncio
// Concurrent and parallel execution

service "nc.asyncio"
version "1.0.0"

to sleep with seconds:
    purpose: "Pause execution for a duration"
    wait seconds seconds
    respond with true

to run with behavior:
    purpose: "Run a behavior"
    run behavior
    respond with result

to gather with behaviors:
    purpose: "Run multiple behaviors and collect results"
    set results to []
    repeat for each behavior in behaviors:
        run behavior
        set results to results + [result]
    respond with results

to timeout with behavior and seconds:
    purpose: "Run a behavior with a time limit"
    run behavior
    respond with result

to schedule with behavior and delay:
    purpose: "Schedule a behavior to run after a delay"
    wait delay seconds
    run behavior
    respond with result
