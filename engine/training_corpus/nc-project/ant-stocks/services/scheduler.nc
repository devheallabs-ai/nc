service "neuraledge-scheduler"
version "1.0.0"
description "Periodic task scheduler — data collection, model retraining, strategy evaluation"

configure:
    port: 8006
    data_service is "http://localhost:8001"
    analyzer_service is "http://localhost:8002"
    swarm_service is "http://localhost:8003"
    signal_service is "http://localhost:8004"
    twin_service is "http://localhost:8005"
    gateway_service is "http://localhost:8000"

// ═══════════════════════════════════════════════════════════════
// SCHEDULED TASKS
// ═══════════════════════════════════════════════════════════════

to task_collect_data:
    purpose: "Collect fresh OHLCV data for all NIFTY 50 stocks"
    log "SCHEDULER: Starting data collection..."
    set start_time to time_now()
    try:
        gather result from "{{config.data_service}}/collect/all":
            method: "POST"
            content_type: "application/json"
        log "SCHEDULER: Data collection complete — {{result.collected}} stocks collected, {{result.errors}} errors"
        respond with {"task": "collect_data", "status": "completed", "result": result, "started": start_time, "finished": time_now()}
    on error:
        log "SCHEDULER: Data collection FAILED — {{error}}"
        respond with {"task": "collect_data", "status": "failed", "error": str(error)}

to task_collect_index_options:
    purpose: "Collect NIFTY and BANKNIFTY option chains"
    log "SCHEDULER: Collecting index option chains..."
    set results to {}
    try:
        gather nifty_chain from "{{config.data_service}}/options/index":
            method: "POST"
            body: {"index_name": "NIFTY"}
            content_type: "application/json"
        set results.nifty to "collected"
    on error:
        set results.nifty to "failed"

    wait 2 seconds

    try:
        gather banknifty_chain from "{{config.data_service}}/options/index":
            method: "POST"
            body: {"index_name": "BANKNIFTY"}
            content_type: "application/json"
        set results.banknifty to "collected"
    on error:
        set results.banknifty to "failed"

    log "SCHEDULER: Option chains collected"
    respond with {"task": "collect_options", "status": "completed", "results": results}

to task_sector_scan:
    purpose: "Analyze all sectors for rotation signals"
    log "SCHEDULER: Running sector scan..."
    try:
        gather result from "{{config.data_service}}/sector/all"
        log "SCHEDULER: Sector scan complete"
        respond with {"task": "sector_scan", "status": "completed", "sectors": result}
    on error:
        respond with {"task": "sector_scan", "status": "failed", "error": str(error)}

to task_generate_signals:
    purpose: "Generate signals for top stocks after market close"
    log "SCHEDULER: Generating signals for NIFTY 50..."
    set scan_symbols to [
        "RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "ICICIBANK.NS",
        "SBIN.NS", "BHARTIARTL.NS", "ITC.NS", "KOTAKBANK.NS", "LT.NS",
        "AXISBANK.NS", "TATAMOTORS.NS", "BAJFINANCE.NS", "SUNPHARMA.NS", "TITAN.NS",
        "WIPRO.NS", "HCLTECH.NS", "MARUTI.NS", "NTPC.NS", "POWERGRID.NS"
    ]
    set signals to []
    set generated to 0
    repeat for each symbol in scan_symbols:
        try:
            gather signal from "{{config.signal_service}}/signal":
                method: "POST"
                body: {"symbol": symbol}
                content_type: "application/json"
            append {"symbol": symbol, "signal": signal.signal, "confidence": signal.confidence, "approved": signal.approved} to signals
            set generated to generated + 1
        on error:
            log "SCHEDULER: Signal generation failed for {{symbol}}"
        wait 2 seconds
    log "SCHEDULER: Generated {{generated}} signals"
    respond with {"task": "generate_signals", "status": "completed", "signals_generated": generated, "signals": signals}

to task_evolve_strategies:
    purpose: "Run evolutionary optimization for top stocks"
    log "SCHEDULER: Running strategy evolution..."
    set top_stocks to ["RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "ICICIBANK.NS"]
    set results to []
    repeat for each symbol in top_stocks:
        try:
            gather evo from "{{config.swarm_service}}/evolve":
                method: "POST"
                body: {"symbol": symbol, "generations": 30, "population_size": 100}
                content_type: "application/json"
            append {"symbol": symbol, "best_fitness": evo.best_strategy.fitness, "best_type": evo.best_strategy.type} to results
        on error:
            log "SCHEDULER: Evolution failed for {{symbol}}"
        wait 3 seconds
    log "SCHEDULER: Strategy evolution complete"
    respond with {"task": "evolve_strategies", "status": "completed", "results": results}

to task_twin_validation:
    purpose: "Run digital twin validation for stocks with active signals"
    log "SCHEDULER: Running digital twin validation..."
    set stocks to ["RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "SBIN.NS"]
    set validated to []
    repeat for each symbol in stocks:
        try:
            gather twin_result from "{{config.twin_service}}/validated-signal":
                method: "POST"
                body: {"symbol": symbol}
                content_type: "application/json"
            append {"symbol": symbol, "signal": twin_result.signal, "confidence": twin_result.confidence, "probability_up": twin_result.monte_carlo.probability_up} to validated
        on error:
            log "SCHEDULER: Twin validation failed for {{symbol}}"
        wait 3 seconds
    log "SCHEDULER: Digital twin validation complete"
    respond with {"task": "twin_validation", "status": "completed", "validated": validated}

// ═══════════════════════════════════════════════════════════════
// SCHEDULE RUNNER — Combines tasks into timed workflows
// ═══════════════════════════════════════════════════════════════

to run_market_open_tasks:
    purpose: "Tasks to run when market opens (9:15 AM IST)"
    log "=== MARKET OPEN WORKFLOW ==="
    run task_collect_data
    set data_result to result
    run task_collect_index_options
    set options_result to result
    respond with {
        "workflow": "market_open",
        "timestamp": time_now(),
        "tasks": [data_result, options_result]
    }

to run_intraday_tasks:
    purpose: "Tasks to run every 30 minutes during market hours"
    log "=== INTRADAY WORKFLOW ==="
    run task_collect_data
    set data_result to result
    run task_generate_signals
    set signals_result to result
    respond with {
        "workflow": "intraday",
        "timestamp": time_now(),
        "tasks": [data_result, signals_result]
    }

to run_market_close_tasks:
    purpose: "Tasks to run after market close (3:30 PM IST)"
    log "=== MARKET CLOSE WORKFLOW ==="
    run task_collect_data
    set data_result to result
    run task_sector_scan
    set sector_result to result
    run task_evolve_strategies
    set evo_result to result
    run task_twin_validation
    set twin_result to result
    run task_generate_signals
    set signals_result to result
    respond with {
        "workflow": "market_close",
        "timestamp": time_now(),
        "tasks": [data_result, sector_result, evo_result, twin_result, signals_result]
    }

to run_daily_maintenance:
    purpose: "Daily system maintenance — retraining, cleanup"
    log "=== DAILY MAINTENANCE ==="
    // Check system health
    try:
        gather health from "{{config.gateway_service}}/health"
        log "System health: all_healthy={{health.all_healthy}}"
    on error:
        log "WARN: Could not check system health"
    respond with {
        "workflow": "daily_maintenance",
        "timestamp": time_now(),
        "note": "Run 'python models/train_models.py --all' for model retraining"
    }

// ═══════════════════════════════════════════════════════════════
// BUILT-IN SCHEDULED TASKS (NC native `every` syntax)
// ═══════════════════════════════════════════════════════════════

every 30 minutes:
    log "SCHEDULER: Running intraday data collection..."
    run task_collect_data

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /run/collect runs task_collect_data
    POST /run/options runs task_collect_index_options
    POST /run/sectors runs task_sector_scan
    POST /run/signals runs task_generate_signals
    POST /run/evolve runs task_evolve_strategies
    POST /run/twin runs task_twin_validation
    POST /workflow/market-open runs run_market_open_tasks
    POST /workflow/intraday runs run_intraday_tasks
    POST /workflow/market-close runs run_market_close_tasks
    POST /workflow/maintenance runs run_daily_maintenance
    POST /start runs start_continuous_schedule

to health_check:
    respond with {"service": "scheduler", "status": "healthy", "version": "1.0.0", "timestamp": time_now()}
