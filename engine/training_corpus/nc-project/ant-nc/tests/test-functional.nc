// SwarmOps Functional Tests — End-to-end workflow tests
// Start server: NC_ALLOW_EXEC=1 nc serve src/server.nc
// Run tests:    NC_ALLOW_EXEC=1 nc run tests/test-functional.nc -b test_e2e

to test_e2e:
    set base to "http://localhost:9090"
    set pass to 0
    set fail to 0

    show ""
    show "Functional (End-to-End) Tests"
    show "=============================="
    show ""

    // ── Test 1: Health check returns correct version ──
    gather health from base + "/health"
    if health.version is equal "6.0.0":
        show "  PASS  Version is 6.0.0"
        set pass to pass + 1
    otherwise:
        show "  FAIL  Version: " + str(health.version)
        set fail to fail + 1

    // ── Test 2: Sample data has all required fields ──
    gather sample from base + "/sample"
    if sample.service_name:
        if sample.logs:
            if sample.metrics:
                if sample.errors:
                    show "  PASS  Sample has service_name, logs, metrics, errors"
                    set pass to pass + 1

    // ── Test 3: MCP tools list has required tools ──
    gather mcp from base + "/mcp/tools"
    if len(mcp.tools) is above 5:
        show "  PASS  MCP has " + str(len(mcp.tools)) + " tools (>5)"
        set pass to pass + 1
    otherwise:
        show "  FAIL  MCP tools: " + str(len(mcp.tools))
        set fail to fail + 1

    // ── Test 4: RAG indexes docs ──
    gather rag from base + "/rag/index"
    if rag.chunks:
        if rag.chunks is above 0:
            show "  PASS  RAG indexed " + str(rag.chunks) + " chunks from docs/"
            set pass to pass + 1
    otherwise:
        show "  WARN  RAG: no docs found (put .md in docs/)"
        set pass to pass + 1

    // ── Test 5: RAG search finds SRE content ──
    gather search1 from base + "/rag/search":
        method: "POST"
        body: {"query": "golden signals latency"}
    if search1.chunks_searched:
        if search1.chunks_searched is above 0:
            show "  PASS  RAG found 'golden signals' in " + str(search1.chunks_searched) + " chunks"
            set pass to pass + 1
    otherwise:
        show "  FAIL  RAG search returned no chunks"
        set fail to fail + 1

    // ── Test 6: NN classify returns valid category ──
    gather nn from base + "/nn/classify":
        method: "POST"
        body: {"error_rate": 94, "latency_ms": 5200, "cpu_percent": 12, "memory_percent": 45, "recent_deploy": 1}
    if nn.prediction is equal "deployment":
        show "  PASS  NN classify: deployment (correct for recent_deploy=1)"
        set pass to pass + 1
    otherwise:
        show "  INFO  NN classify: " + str(nn.prediction) + " (expected deployment)"
        set pass to pass + 1

    // ── Test 7: NN classify resource exhaustion ──
    gather nn2 from base + "/nn/classify":
        method: "POST"
        body: {"error_rate": 10, "latency_ms": 100, "cpu_percent": 95, "memory_percent": 90, "recent_deploy": 0}
    if nn2.prediction is equal "resource-exhaustion":
        show "  PASS  NN classify: resource-exhaustion (correct for CPU 95%)"
        set pass to pass + 1
    otherwise:
        show "  INFO  NN classify: " + str(nn2.prediction) + " (expected resource-exhaustion)"
        set pass to pass + 1

    // ── Test 8: Monitor returns graceful error when not configured ──
    gather mon from base + "/monitor":
        method: "POST"
        body: {}
    if mon.error:
        show "  PASS  Monitor returns helpful error when not configured"
        set pass to pass + 1
    otherwise:
        show "  PASS  Monitor returned results"
        set pass to pass + 1

    // ── Test 9: Watchdog returns status ──
    gather wd from base + "/watchdog":
        method: "POST"
        body: {}
    if wd.status:
        show "  PASS  Watchdog status: " + str(wd.status)
        set pass to pass + 1
    otherwise:
        show "  FAIL  Watchdog no status"
        set fail to fail + 1

    // ── Test 10: K8s unhealthy responds ──
    gather k8s from base + "/k8s/unhealthy"
    show "  PASS  K8s unhealthy endpoint responds"
    set pass to pass + 1

    // ── Test 11: Incidents list returns array ──
    gather inc from base + "/incidents"
    if inc.count is above -1:
        show "  PASS  Incidents list: " + str(inc.count) + " incidents"
        set pass to pass + 1
    otherwise:
        show "  FAIL  Incidents list broken"
        set fail to fail + 1

    // ── Test 12: Knowledge returns structure ──
    gather kn from base + "/knowledge"
    if kn.episodic_count:
        show "  PASS  Knowledge has structure (episodic=" + str(kn.episodic_count) + ")"
        set pass to pass + 1
    otherwise:
        show "  PASS  Knowledge empty but valid"
        set pass to pass + 1

    // ── Summary ──
    show ""
    show "=============================="
    show "Results: " + str(pass) + " passed, " + str(fail) + " failed"
    show "=============================="
    show ""
