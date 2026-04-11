// ══════════════════════════════════════════════════════════════════
//  HiveANT — Local Deploy with Ollama (NC version)
//  Usage: NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc run scripts/deploy-local.nc
// ══════════════════════════════════════════════════════════════════

to deploy_local:
    log "HiveANT — Local Deploy with Ollama"
    log "═══════════════════════════════════"

    // ── 1. Verify NC Engine ─────────────────────────────────────
    set nc_bin to "/Users/NuckalaSai.Narender4/Documents/notation as code/nc/build/nc"
    set nc_path to trim(shell("command -v nc 2>/dev/null || echo NOT_FOUND"))
    if nc_path is not equal "NOT_FOUND":
        set nc_bin to nc_path
        log "[ok] NC Engine: " + nc_bin
    otherwise:
        if file_exists(nc_bin):
            log "[ok] NC Engine: " + nc_bin
        otherwise:
            log "[FAIL] NC not built"
            respond with {"error": "NC Engine not found"}

    // ── 2. Start Ollama if not running ──────────────────────────
    set ollama_running to shell("pgrep -x ollama > /dev/null 2>&1 && echo YES || echo NO")
    if trim(ollama_running) is not equal "YES":
        log "Starting Ollama..."
        shell("OLLAMA_FLASH_ATTENTION=1 /opt/homebrew/opt/ollama/bin/ollama serve > /tmp/ollama.log 2>&1 &")
        shell("sleep 8")

    set ollama_check to shell("curl -s http://localhost:11434/api/tags > /dev/null 2>&1 && echo OK || echo FAILED")
    if trim(ollama_check) is not equal "OK":
        log "[FAIL] Ollama not running"
        respond with {"error": "Ollama not running"}
    log "[ok] Ollama running"

    // ── 3. Detect or pull model ─────────────────────────────────
    set model to trim(shell("curl -s http://localhost:11434/api/tags | grep -o '\"name\":\"[^\"]*\"' | head -1 | cut -d'\"' -f4 2>/dev/null || echo ''"))
    if len(model) is below 1:
        log "Pulling phi3..."
        shell("ollama pull phi3")
        set model to "phi3"
    log "[ok] Model: " + model

    // ── 4. Write config ─────────────────────────────────────────
    shell("mkdir -p config")
    set env_content to "NC_ALLOW_EXEC=1\nNC_ALLOW_FILE_WRITE=1\nNC_AI_URL=http://localhost:11434/v1/chat/completions\nNC_AI_MODEL=" + model + "\nNC_AI_KEY=local\nHEAL_SECRET=hiveant-dev\n"
    write_file("config/.env", env_content)
    log "[ok] Config written"

    // ── 5. Create directories ───────────────────────────────────
    shell("mkdir -p incidents knowledge docs memory/pheromone/edges memory/twins/services memory/tasks memory/causal memory/knowledge_graph/entities memory/knowledge_graph/relations agents_state/queue agents_state/clusters agents_state/audit agents_state/bus agents_state/plugins sandbox_envs simulation/results")

    // ── 6. Kill existing HiveANT ────────────────────────────────
    shell("lsof -ti:7700 2>/dev/null | xargs kill -9 2>/dev/null || true")
    shell("sleep 1")

    // ── 7. Start HiveANT ────────────────────────────────────────
    log "Starting HiveANT..."
    shell("source config/.env && NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 " + nc_bin + " serve server.nc > /tmp/hiveant.log 2>&1 &")
    shell("sleep 3")

    set health to shell("curl -s http://localhost:7700/health 2>/dev/null || echo FAILED")
    if trim(health) is equal "FAILED":
        shell("sleep 5")
        set health to shell("curl -s http://localhost:7700/health 2>/dev/null || echo FAILED")

    if trim(health) is equal "FAILED":
        log "[FAIL] HiveANT failed to start. Check /tmp/hiveant.log"
        respond with {"error": "Failed to start"}

    set pid to trim(shell("lsof -ti:7700 2>/dev/null | head -1 || echo unknown"))
    log "[ok] HiveANT at http://localhost:7700 PID=" + pid

    // ── 8. Initialize kernel ────────────────────────────────────
    log "Initializing kernel..."
    set init_result to shell("curl -s -X POST http://localhost:7700/kernel/init 2>/dev/null || echo FAILED")
    log init_result

    // ── 9. Deep health check ────────────────────────────────────
    log ""
    log "Health check..."
    set deep_health to shell("curl -s http://localhost:7700/health/deep 2>/dev/null || echo FAILED")
    log deep_health

    // ── 10. Run sample investigation ────────────────────────────
    log ""
    log "Running investigation (30-120s)..."
    set inv_result to shell("curl -s -X POST http://localhost:7700/investigate -H 'Content-Type: application/json' -d '{\"service_name\":\"checkout-api\",\"description\":\"HTTP 500 errors after deploy v2.4.1 updated db credentials. 94 percent error rate.\"}' 2>/dev/null || echo FAILED")
    log inv_result

    log ""
    log "LIVE! Dashboard: http://localhost:7700  Stop: kill " + pid

    respond with {"status": "deployed", "model": model, "pid": pid, "port": 7700, "dashboard": "http://localhost:7700"}

deploy_local()
