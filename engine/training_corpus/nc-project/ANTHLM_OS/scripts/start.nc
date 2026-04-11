// ══════════════════════════════════════════════════════════════════
//  HiveANT — Start Script (NC version)
//  Usage: NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc run scripts/start.nc
// ══════════════════════════════════════════════════════════════════

to start:
    log ""
    log "  HiveANT — Swarm Autonomous AI OS"
    log "  ════════════════════════════════════"
    log ""

    // ── Load environment ────────────────────────────────────────
    if file_exists("config/.env"):
        log "  Loading configuration from config/.env..."
        shell("export $(grep -v '^#' config/.env | xargs) 2>/dev/null || true")

    // ── Create required directories ─────────────────────────────
    shell("mkdir -p incidents knowledge docs memory/pheromone/edges memory/twins/services memory/tasks memory/causal memory/knowledge_graph/entities memory/knowledge_graph/relations agents_state/queue agents_state/clusters agents_state/audit agents_state/bus agents_state/plugins sandbox_envs simulation/results")

    // ── Check NC is available ───────────────────────────────────
    set nc_check to shell("command -v nc 2>/dev/null || echo NOT_FOUND")
    if trim(nc_check) is equal "NOT_FOUND":
        log "  [FAIL] NC Engine not found."
        log "    Install from: /Users/NuckalaSai.Narender4/Documents/notation as code"
        log "    cd '/Users/NuckalaSai.Narender4/Documents/notation as code/nc'"
        log "    make"
        log "    sudo cp build/nc /usr/local/bin/"
        respond with {"error": "NC Engine not found"}

    // ── Resolve port ────────────────────────────────────────────
    set port_val to env("HIVEANT_PORT")
    if port_val:
        set port to port_val
    otherwise:
        set port to "7700"

    log "  Starting HiveANT on port " + str(port) + "..."
    log "  Dashboard: http://localhost:" + str(port)
    log ""

    // ── Kill any existing process on the port ───────────────────
    shell("lsof -ti:" + str(port) + " 2>/dev/null | xargs kill -9 2>/dev/null || true")
    shell("sleep 1")

    // ── Start the server ────────────────────────────────────────
    set nc_bin to trim(shell("command -v nc 2>/dev/null"))
    shell("NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 " + nc_bin + " serve server.nc > /tmp/hiveant.log 2>&1 &")
    shell("sleep 3")

    // ── Verify it started ───────────────────────────────────────
    set health to shell("curl -s http://localhost:" + str(port) + "/health 2>/dev/null || echo FAILED")
    if trim(health) is not equal "FAILED":
        set pid to trim(shell("lsof -ti:" + str(port) + " 2>/dev/null | head -1 || echo unknown"))
        log "  [ok] HiveANT running (PID=" + pid + ")"
        log "  Dashboard: http://localhost:" + str(port)
        log "  Stop: kill " + pid
        respond with {"status": "running", "port": port, "pid": pid, "dashboard": "http://localhost:" + str(port)}
    otherwise:
        shell("sleep 5")
        set health2 to shell("curl -s http://localhost:" + str(port) + "/health 2>/dev/null || echo FAILED")
        if trim(health2) is not equal "FAILED":
            set pid to trim(shell("lsof -ti:" + str(port) + " 2>/dev/null | head -1 || echo unknown"))
            log "  [ok] HiveANT running (PID=" + pid + ")"
            respond with {"status": "running", "port": port, "pid": pid}
        otherwise:
            log "  [FAIL] HiveANT failed to start. Check /tmp/hiveant.log"
            set logs to shell("tail -20 /tmp/hiveant.log 2>/dev/null || echo 'no logs'")
            log logs
            respond with {"status": "failed", "logs": logs}

start()
