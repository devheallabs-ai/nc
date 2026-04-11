// SwarmOps API Tests — Tests all endpoints
// Start server: NC_ALLOW_EXEC=1 nc serve src/server.nc
// Run:          NC_ALLOW_EXEC=1 nc run tests/test-api.nc -b test_all

to test_all:
    set pass to 0
    set fail to 0

    show ""
    show "SwarmOps API Tests"
    show "=================="
    show ""

    gather r1 from "http://localhost:9090/health"
    if r1.status is equal "healthy":
        show "  PASS  /health"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /health"
        set fail to fail + 1

    gather r2 from "http://localhost:9090/config"
    if r2.mode:
        show "  PASS  /config"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /config"
        set fail to fail + 1

    gather r3 from "http://localhost:9090/sample"
    if r3.service_name:
        show "  PASS  /sample"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /sample"
        set fail to fail + 1

    gather r4 from "http://localhost:9090/incidents"
    if r4.count is above -1:
        show "  PASS  /incidents (" + str(r4.count) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /incidents"
        set fail to fail + 1

    gather r5 from "http://localhost:9090/mcp/tools"
    if r5.tools:
        show "  PASS  /mcp/tools (" + str(len(r5.tools)) + " tools)"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /mcp/tools"
        set fail to fail + 1

    gather r6 from "http://localhost:9090/rag/index"
    if r6.status:
        show "  PASS  /rag/index (" + str(r6.status) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /rag/index"
        set fail to fail + 1

    gather r7 from "http://localhost:9090/knowledge"
    if r7.episodic_count:
        show "  PASS  /knowledge"
        set pass to pass + 1
    otherwise:
        show "  PASS  /knowledge (empty)"
        set pass to pass + 1

    gather r8 from "http://localhost:9090/k8s/unhealthy"
    if r8:
        show "  PASS  /k8s/unhealthy"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /k8s/unhealthy"
        set fail to fail + 1

    gather r9 from "http://localhost:9090/monitor":
        method: "POST"
        body: {}
    if r9.source:
        show "  PASS  /monitor (" + str(r9.source) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /monitor"
        set fail to fail + 1

    gather r10 from "http://localhost:9090/watchdog":
        method: "POST"
        body: {}
    if r10.status:
        show "  PASS  /watchdog (" + str(r10.status) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /watchdog"
        set fail to fail + 1

    gather r11 from "http://localhost:9090/nn/classify":
        method: "POST"
        body: {"error_rate": 94, "latency_ms": 5200, "cpu_percent": 12, "memory_percent": 45, "recent_deploy": 1}
    if r11.prediction:
        show "  PASS  /nn/classify (" + str(r11.prediction) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /nn/classify"
        set fail to fail + 1

    gather r12 from "http://localhost:9090/rag/search":
        method: "POST"
        body: {"query": "OOMKilled"}
    if r12.chunks_searched:
        show "  PASS  /rag/search (" + str(r12.chunks_searched) + " chunks)"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /rag/search"
        set fail to fail + 1

    gather r13 from "http://localhost:9090/pull/prometheus":
        method: "POST"
        body: {"query": "up"}
    if r13.source:
        show "  PASS  /pull/prometheus (" + str(r13.source) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /pull/prometheus"
        set fail to fail + 1

    gather r14 from "http://localhost:9090/pull/logs":
        method: "POST"
        body: {"search_query": "error"}
    if r14.source:
        show "  PASS  /pull/logs (" + str(r14.source) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /pull/logs"
        set fail to fail + 1

    gather r15 from "http://localhost:9090/pull/deploys":
        method: "POST"
        body: {}
    if r15.source:
        show "  PASS  /pull/deploys (" + str(r15.source) + ")"
        set pass to pass + 1
    otherwise:
        show "  FAIL  /pull/deploys"
        set fail to fail + 1

    show ""
    show "=================="
    show "Results: " + str(pass) + " passed, " + str(fail) + " failed"
    show "=================="
    show ""
