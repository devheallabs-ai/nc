// NC Standard Library — timeit
// Measure execution time

service "nc.timeit"
version "1.0.0"

to measure with behavior:
    purpose: "Measure how long a behavior takes to run"
    set start to time_now()
    run behavior
    set end to time_now()
    set elapsed to end - start
    log "Elapsed: " + str(elapsed) + " seconds"
    respond with elapsed

to benchmark with behavior and iterations:
    purpose: "Run a behavior many times and report timing"
    set start to time_now()
    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    repeat for each n in nums:
        if n is at most iterations:
            run behavior
    set end to time_now()
    set total to end - start
    set per_run to total / iterations
    log "Total: " + str(total) + "s, Per run: " + str(per_run) + "s"
    respond with per_run
