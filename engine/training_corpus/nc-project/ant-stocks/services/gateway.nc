service "neuraledge-gateway"
version "1.0.0"
description "Main API gateway for NeuralEdge Financial Intelligence Engine"

configure:
    port: 8000
    data_service is "http://localhost:8001"
    analyzer_service is "http://localhost:8002"
    swarm_service is "http://localhost:8003"
    signal_service is "http://localhost:8004"
    twin_service is "http://localhost:8005"
    scheduler_service is "http://localhost:8006"
    ai_model is "gpt-4o-mini"

middleware:
    cors: true
    log_requests: true

// ═══════════════════════════════════════════════════════════════
// ORCHESTRATION BEHAVIORS
// ═══════════════════════════════════════════════════════════════

to get_full_intelligence with symbol:
    purpose: "Complete financial intelligence report for a symbol"
    log "Generating full intelligence for {{symbol}}..."
    set analysis to http_post("{{config.analyzer_service}}/analyze", {"symbol": symbol})
    set swarm_signal to http_post("{{config.swarm_service}}/signal", {"symbol": symbol})
    set sig_result to http_post("{{config.signal_service}}/signal", {"symbol": symbol})
    set twin_signal to http_post("{{config.twin_service}}/validated-signal", {"symbol": symbol})
    set final_confidence to (sig_result.confidence + twin_signal.confidence) / 2
    // LLM synthesis
    ask AI to """You are a senior Indian stock market analyst. Synthesize the following multi-source intelligence for {{symbol}} into a clear, actionable trading recommendation.

TECHNICAL ANALYSIS:
- Market Regime: {{analysis.regime.regime}} ({{analysis.regime.confidence}}% confidence)
- Trend: {{analysis.signals.trend.trend}}
- RSI: {{analysis.indicators.rsi}}
- MACD Bullish: {{analysis.indicators.macd.bullish}}
- Stochastic: {{analysis.indicators.stochastic.signal}}

SWARM INTELLIGENCE:
- Signal: {{swarm_signal.signal}} ({{swarm_signal.confidence}}% confidence)
- Best Strategy: {{swarm_signal.strategy_type}}
- Win Rate: {{swarm_signal.win_rate}}%

SIGNAL ENGINE:
- Final Signal: {{sig_result.signal}} ({{sig_result.confidence}}% confidence)
- Approved by Risk: {{sig_result.approved}}

DIGITAL TWIN SIMULATION:
- Signal: {{twin_signal.signal}} ({{twin_signal.confidence}}% confidence)
- Monte Carlo Probability Up: {{twin_signal.monte_carlo.probability_up}}%
- Expected Return: {{twin_signal.monte_carlo.expected_return}}%
- Best Simulated Strategy: {{twin_signal.simulation.best_strategy}}

Provide a JSON response with:
- recommendation: "STRONG_BUY", "BUY", "HOLD", "SELL", or "STRONG_SELL"
- confidence: number 0-100
- summary: one paragraph executive summary
- key_factors: list of top 3 driving factors
- risk_warning: one sentence risk warning
- target_price_range: object with "low" and "high"
- time_horizon: recommended holding period""" save as synthesis

    respond with {
        "symbol": symbol,
        "timestamp": time_now(),
        "recommendation": synthesis.recommendation,
        "confidence": round(final_confidence, 2),
        "executive_summary": synthesis.summary,
        "key_factors": synthesis.key_factors,
        "risk_warning": synthesis.risk_warning,
        "target_range": synthesis.target_price_range,
        "time_horizon": synthesis.time_horizon,
        "current_price": analysis.current_price,
        "components": {
            "technical_analysis": {
                "regime": analysis.regime,
                "indicators": analysis.indicators,
                "signals": analysis.signals
            },
            "swarm_intelligence": swarm_signal,
            "signal_engine": {
                "signal": sig_result.signal,
                "confidence": sig_result.confidence,
                "approved": sig_result.approved,
                "risk_management": sig_result.risk_management,
                "source_scores": sig_result.source_scores
            },
            "digital_twin": {
                "signal": twin_signal.signal,
                "confidence": twin_signal.confidence,
                "monte_carlo": twin_signal.monte_carlo,
                "simulation": twin_signal.simulation
            },
            "llm_synthesis": synthesis
        }
    }

to scan_market:
    purpose: "Scan all NIFTY 50 stocks and return top opportunities"
    if circuit_open("signal_service"):
        respond with {"error": "Signal service temporarily unavailable", "_status": 503}
    gather symbols from "{{config.data_service}}/symbols/nifty50"
    set opportunities to []
    set analyzed to 0
    repeat for each symbol in symbols:
        try:
            set scan_result to http_post("{{config.signal_service}}/signal", {"symbol": symbol})
            set analyzed to analyzed + 1
            if scan_result.approved is equal yes and scan_result.confidence is above 65:
                append {
                    "symbol": symbol,
                    "signal": scan_result.signal,
                    "confidence": scan_result.confidence,
                    "current_price": scan_result.current_price,
                    "risk": scan_result.risk_management
                } to opportunities
        on error:
            log "Failed to analyze {{symbol}}"
    respond with {
        "timestamp": time_now(),
        "stocks_analyzed": analyzed,
        "opportunities_found": len(opportunities),
        "opportunities": opportunities
    }

to get_dashboard_data:
    purpose: "Get all data needed for the dashboard"
    gather snapshot from "{{config.data_service}}/snapshot"
    // Get NIFTY and BANKNIFTY analysis
    set indices to {}
    try:
        gather nifty from "{{config.analyzer_service}}/regime":
            method: "POST"
            body: {"symbol": "^NSEI"}
            content_type: "application/json"
        set indices.nifty to nifty
    on error:
        log "Could not fetch NIFTY regime"
    try:
        gather banknifty from "{{config.analyzer_service}}/regime":
            method: "POST"
            body: {"symbol": "^NSEBANK"}
            content_type: "application/json"
        set indices.banknifty to banknifty
    on error:
        log "Could not fetch BANKNIFTY regime"
    respond with {
        "timestamp": time_now(),
        "market_snapshot": snapshot,
        "index_regimes": indices,
        "system_status": {
            "data_collector": "active",
            "analyzer": "active",
            "swarm_engine": "active",
            "signal_engine": "active",
            "digital_twin": "active"
        }
    }

to get_system_health:
    purpose: "Check health of all microservices"
    set services to {}
    set all_healthy to yes
    try:
        gather h from "{{config.data_service}}/health"
        set services.data_collector to h
    on error:
        set services.data_collector to {"status": "unhealthy"}
        set all_healthy to no
    try:
        gather h from "{{config.analyzer_service}}/health"
        set services.analyzer to h
    on error:
        set services.analyzer to {"status": "unhealthy"}
        set all_healthy to no
    try:
        gather h from "{{config.swarm_service}}/health"
        set services.swarm_engine to h
    on error:
        set services.swarm_engine to {"status": "unhealthy"}
        set all_healthy to no
    try:
        gather h from "{{config.signal_service}}/health"
        set services.signal_engine to h
    on error:
        set services.signal_engine to {"status": "unhealthy"}
        set all_healthy to no
    try:
        gather h from "{{config.twin_service}}/health"
        set services.digital_twin to h
    on error:
        set services.digital_twin to {"status": "unhealthy"}
        set all_healthy to no
    try:
        gather h from "{{config.scheduler_service}}/health"
        set services.scheduler to h
    on error:
        set services.scheduler to {"status": "unhealthy"}
        set all_healthy to no
    respond with {
        "gateway": "healthy",
        "all_healthy": all_healthy,
        "services": services,
        "timestamp": time_now()
    }

// ═══════════════════════════════════════════════════════════════
// CONVERSATIONAL AI INTERFACE
// ═══════════════════════════════════════════════════════════════

to ask_market with question:
    purpose: "Natural language market query interface"
    ask AI to """You are an AI financial assistant for the Indian stock market (NSE/BSE).
The user is asking: "{{question}}"

If they ask about a specific stock, extract the NSE symbol (add .NS suffix if missing).
If they ask for market overview, suggest checking NIFTY 50 or sector-wise analysis.
If they ask about strategies, explain the swarm intelligence approach.

Respond with JSON:
- intent: "stock_analysis", "market_overview", "strategy_query", "general_question"
- symbol: string or null (extracted NSE symbol like "TCS.NS")
- response: your helpful answer
- suggested_actions: list of suggested next steps""" save as parsed

    // If a specific stock was identified, fetch analysis
    if parsed.intent is equal "stock_analysis" and parsed.symbol is not equal nothing:
        try:
            gather analysis from "{{config.analyzer_service}}/analyze":
                method: "POST"
                body: {"symbol": parsed.symbol}
                content_type: "application/json"
            set parsed.analysis to {
                "current_price": analysis.current_price,
                "regime": analysis.regime,
                "rsi": analysis.indicators.rsi,
                "trend": analysis.signals.trend.trend
            }
        on error:
            log "Could not fetch analysis for {{parsed.symbol}}"

    respond with parsed

// ═══════════════════════════════════════════════════════════════
// API ROUTES
// ═══════════════════════════════════════════════════════════════

api:
    GET /health runs get_system_health
    GET /dashboard runs get_dashboard_data
    GET /scan runs scan_market
    POST /intelligence runs get_full_intelligence
    POST /ask runs ask_market
    // Proxy routes to individual services
    POST /data/stock runs proxy_stock_data
    POST /data/options runs proxy_options
    GET /data/news runs proxy_news
    POST /analyze runs proxy_analyze
    POST /swarm/evolve runs proxy_evolve
    POST /signal runs proxy_signal
    POST /twin/simulate runs proxy_simulate
    POST /twin/validate runs proxy_validate
    GET /symbols/top200 runs proxy_top200
    POST /sector/analyze runs proxy_sector
    GET /sector/all runs proxy_all_sectors
    GET /fii-dii runs proxy_fii_dii
    POST /scheduler/run runs proxy_scheduler_run
    POST /scheduler/workflow runs proxy_scheduler_workflow

// ─── Proxy Behaviors ───

to proxy_stock_data with symbol, period, interval:
    gather result from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": period, "interval": interval}
        content_type: "application/json"
    respond with result

to proxy_options with symbol:
    gather result from "{{config.data_service}}/options/chain":
        method: "POST"
        body: {"symbol": symbol}
        content_type: "application/json"
    respond with result

to proxy_news:
    gather result from "{{config.data_service}}/news"
    respond with result

to proxy_analyze with symbol:
    gather result from "{{config.analyzer_service}}/analyze":
        method: "POST"
        body: {"symbol": symbol}
        content_type: "application/json"
    respond with result

to proxy_evolve with symbol, generations, population_size:
    gather result from "{{config.swarm_service}}/evolve":
        method: "POST"
        body: {"symbol": symbol, "generations": generations, "population_size": population_size}
        content_type: "application/json"
    respond with result

to proxy_signal with symbol:
    gather result from "{{config.signal_service}}/signal":
        method: "POST"
        body: {"symbol": symbol}
        content_type: "application/json"
    respond with result

to proxy_simulate with symbol, num_agents, sim_steps:
    gather result from "{{config.twin_service}}/simulate":
        method: "POST"
        body: {"symbol": symbol, "num_agents": num_agents, "sim_steps": sim_steps}
        content_type: "application/json"
    respond with result

to proxy_validate with symbol:
    gather result from "{{config.twin_service}}/validated-signal":
        method: "POST"
        body: {"symbol": symbol}
        content_type: "application/json"
    respond with result

to proxy_top200:
    gather result from "{{config.data_service}}/symbols/top200"
    respond with result

to proxy_sector with sector_name:
    gather result from "{{config.data_service}}/sector/analyze":
        method: "POST"
        body: {"sector_name": sector_name}
        content_type: "application/json"
    respond with result

to proxy_all_sectors:
    gather result from "{{config.data_service}}/sector/all"
    respond with result

to proxy_fii_dii:
    gather result from "{{config.data_service}}/fii-dii"
    respond with result

to proxy_scheduler_run with task:
    gather result from "{{config.scheduler_service}}/run/{{task}}":
        method: "POST"
        content_type: "application/json"
    respond with result

to proxy_scheduler_workflow with workflow:
    gather result from "{{config.scheduler_service}}/workflow/{{workflow}}":
        method: "POST"
        content_type: "application/json"
    respond with result
