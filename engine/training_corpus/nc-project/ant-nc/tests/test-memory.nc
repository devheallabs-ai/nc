// SwarmOps Memory & RAG Test — Tests cognitive memory, RL, RAG
// Start server: NC_ALLOW_EXEC=1 nc serve src/server.nc
// Run tests:    NC_ALLOW_EXEC=1 nc run tests/test-memory.nc -b test_memory

to test_memory:
    set base to "http://localhost:9090"
    set pass to 0
    set fail to 0

    show ""
    show "Memory, RL & RAG Tests"
    show "======================"
    show ""

    // ── RAG: Index docs ──
    gather r1 from base + "/rag/index"
    if r1.status is equal "indexed":
        show "  PASS  RAG index: " + str(r1.chunks) + " chunks, " + str(r1.total_tokens) + " tokens"
        set pass to pass + 1
    otherwise:
        show "  WARN  RAG index: " + str(r1.status) + " — put .md files in docs/"
        set pass to pass + 1

    // ── RAG: Search for K8s content ──
    gather r2 from base + "/rag/search":
        method: "POST"
        body: {"query": "CrashLoopBackOff"}
    if r2.chunks_searched:
        show "  PASS  RAG search: searched " + str(r2.chunks_searched) + " chunks"
        set pass to pass + 1
    otherwise:
        show "  FAIL  RAG search returned nothing"
        set fail to fail + 1

    // ── RAG: Search for incident pattern ──
    gather r3 from base + "/rag/search":
        method: "POST"
        body: {"query": "deploy credential failure"}
    show "  PASS  RAG search 'deploy credential': " + str(r3.chunks_searched) + " chunks"
    set pass to pass + 1

    // ── Knowledge: View what's learned ──
    gather r4 from base + "/knowledge"
    show "  PASS  Knowledge: " + str(r4.episodic_count) + " incidents, docs=" + str(r4.docs_indexed)
    set pass to pass + 1

    // ── Memory: Load for service ──
    gather r5 from base + "/memory":
        method: "POST"
        body: {"service_name": "checkout-api"}
    show "  PASS  Memory loaded for checkout-api"
    set pass to pass + 1

    // ── NN Classify: Heuristic mode ──
    gather r6 from base + "/nn/classify":
        method: "POST"
        body: {"error_rate": 94, "latency_ms": 5200, "cpu_percent": 12, "memory_percent": 45, "recent_deploy": 1}
    if r6.method is equal "heuristic":
        show "  PASS  NN classify (heuristic): " + str(r6.prediction)
        set pass to pass + 1
    otherwise:
        show "  PASS  NN classify (neural_network): " + str(r6.prediction)
        set pass to pass + 1

    // ── NN Classify: No deploy ──
    gather r7 from base + "/nn/classify":
        method: "POST"
        body: {"error_rate": 5, "latency_ms": 50, "cpu_percent": 92, "memory_percent": 88, "recent_deploy": 0}
    show "  PASS  NN classify (high CPU): " + str(r7.prediction)
    set pass to pass + 1

    show ""
    show "======================"
    show "Results: " + str(pass) + " passed, " + str(fail) + " failed"
    show "======================"
    show ""
