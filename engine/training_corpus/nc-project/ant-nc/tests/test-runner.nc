// NC Issue Test Runner — ONLY NC
// Run: NC_ALLOW_EXEC=1 nc run test-runner.nc -b run_tests

to run_tests:
    show ""
    show "NC Issue Validation"
    show "==================="

    gather r1 from "http://localhost:9877/html/inline"
    show "#1a HTML inline:   " + str(r1)

    gather r2 from "http://localhost:9877/html/file"
    show "#1b HTML file:     " + str(r2)

    gather r3 from "http://localhost:9877/time"
    show "#23 time_format:   " + str(r3)

    gather r4 from "http://localhost:9877/call"
    show "#5  behavior call: " + str(r4)

    gather r5 from "http://localhost:9877/shell"
    show "    shell:         " + str(r5)

    gather r6 from "http://localhost:9877/env"
    show "    env:           " + str(r6)

    gather r7 from "http://localhost:9877/json"
    show "    json:          " + str(r7)

    show ""
    show "Done."
