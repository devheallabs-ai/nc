// ══════════════════════════════════════════════════════════════════
//  HiveANT — One-Command Setup (NC version)
//  Usage: NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc run scripts/setup.nc
// ══════════════════════════════════════════════════════════════════

to setup:
    log ""
    log "  HiveANT — Setup"
    log "  ═══════════════════"
    log ""

    set all_ok to true

    // ── 1. Check NC Engine ──────────────────────────────────────
    set nc_check to shell("command -v nc 2>/dev/null || echo NOT_FOUND")
    if trim(nc_check) is not equal "NOT_FOUND":
        log "  [ok] NC Engine found: " + trim(nc_check)
    otherwise:
        set nc_bin to "/Users/NuckalaSai.Narender4/Documents/notation as code/nc/build/nc"
        if file_exists(nc_bin):
            log "  [!!] NC Engine built but not in PATH"
            log "    Run: sudo ln -sf '" + nc_bin + "' /usr/local/bin/nc"
        otherwise:
            log "  [FAIL] NC Engine not found"
            log "    Build it:"
            log "    cd '/Users/NuckalaSai.Narender4/Documents/notation as code/nc' && make"
            set all_ok to false

    // ── 2. Check AI Model ───────────────────────────────────────
    log ""
    set ollama_check to shell("command -v ollama 2>/dev/null || echo NOT_FOUND")
    if trim(ollama_check) is not equal "NOT_FOUND":
        log "  [ok] Ollama installed"
        set ollama_ping to shell("curl -s http://localhost:11434/api/tags 2>/dev/null || echo OFFLINE")
        if trim(ollama_ping) is not equal "OFFLINE":
            log "  [ok] Ollama running"
            set models to shell("curl -s http://localhost:11434/api/tags | grep -o '\"name\":\"[^\"]*\"' | head -5 2>/dev/null || echo NONE")
            if trim(models) is not equal "NONE":
                log "  [ok] Models available: " + trim(models)
            otherwise:
                log "  [!!] No models pulled. Run: ollama pull llama3"
        otherwise:
            log "  [!!] Ollama not running. Run: ollama serve"
    otherwise:
        log "  [!!] Ollama not installed (needed for local AI)"
        log "    Install: brew install ollama"
        log "    Then:    ollama serve & ollama pull llama3"
        log ""
        log "    Or use OpenAI/Anthropic API instead (set in .env)"

    // ── 3. Check optional dependencies ──────────────────────────
    log ""
    log "  Optional dependencies:"
    set docker_check to shell("command -v docker 2>/dev/null || echo NOT_FOUND")
    if trim(docker_check) is not equal "NOT_FOUND":
        log "  [ok] Docker"
    otherwise:
        log "  [-] Docker (not installed)"

    set kubectl_check to shell("command -v kubectl 2>/dev/null || echo NOT_FOUND")
    if trim(kubectl_check) is not equal "NOT_FOUND":
        log "  [ok] kubectl"
    otherwise:
        log "  [-] kubectl (not installed)"

    set prom_check to shell("command -v prometheus 2>/dev/null || echo NOT_FOUND")
    if trim(prom_check) is not equal "NOT_FOUND":
        log "  [ok] Prometheus"
    otherwise:
        log "  [-] Prometheus (not installed)"

    // ── 4. Setup config ─────────────────────────────────────────
    log ""
    if file_exists("config/.env"):
        log "  [ok] config/.env exists"
    otherwise:
        if file_exists("config/.env.example"):
            set template to read_file("config/.env.example")
            write_file("config/.env", template)
            log "  [ok] Created config/.env from template"
            log "  [!!] Edit config/.env to set your AI model URL and key"
        otherwise:
            log "  [FAIL] config/.env.example not found"
            set all_ok to false

    // ── 5. Create directories ───────────────────────────────────
    shell("mkdir -p incidents knowledge docs memory/pheromone/edges memory/twins/services memory/tasks memory/causal memory/knowledge_graph/entities memory/knowledge_graph/relations agents_state/queue agents_state/clusters agents_state/audit agents_state/bus agents_state/plugins sandbox_envs simulation/results")
    log "  [ok] Data directories created"

    // ── 6. Summary ──────────────────────────────────────────────
    log ""
    log "  ═══════════════════════════════════════════"
    if all_ok:
        log "  Setup complete!"
    otherwise:
        log "  Setup finished with warnings (see above)"
    log ""
    log "  To start HiveANT:"
    log "    source config/.env"
    log "    NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc serve server.nc"
    log ""
    log "  Or use:"
    log "    ./scripts/start.sh"
    log "    NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc run scripts/start.nc"
    log ""
    log "  Dashboard: http://localhost:7700"
    log "  ═══════════════════════════════════════════"
    log ""

    respond with {"status": "complete", "all_ok": all_ok}

setup()
