service "neuraledge-signal-engine"
version "1.0.0"
description "Signal aggregation, risk management, and LLM reasoning for trading signals"

configure:
    port: 8004
    data_service is "http://localhost:8001"
    analyzer_service is "http://localhost:8002"
    swarm_service is "http://localhost:8003"
    ai_model is "gpt-4o-mini"
    max_capital_per_trade_pct: 2.0
    max_portfolio_exposure_pct: 20.0
    default_stop_loss_pct: 3.0
    min_confidence: 60.0

// ═══════════════════════════════════════════════════════════════
// LLM REASONING ENGINE
// ═══════════════════════════════════════════════════════════════

to llm_market_analysis with symbol, analysis_data, regime_data:
    purpose: "Use LLM to generate strategic market insight"
    set prompt to """You are an expert Indian stock market analyst. Analyze the following data for {{symbol}} and provide a structured trading recommendation.

TECHNICAL ANALYSIS:
- RSI: {{analysis_data.indicators.rsi}}
- MACD: {{analysis_data.indicators.macd.macd}} (Signal: {{analysis_data.indicators.macd.signal}}, Bullish: {{analysis_data.indicators.macd.bullish}})
- SMA 20: {{analysis_data.indicators.sma_20}}
- SMA 50: {{analysis_data.indicators.sma_50}}
- SMA 200: {{analysis_data.indicators.sma_200}}
- Bollinger %B: {{analysis_data.indicators.bollinger.percent_b}}
- ADX: {{analysis_data.indicators.adx.adx}} (Trend: {{analysis_data.indicators.adx.trend_strength}})
- Stochastic: {{analysis_data.indicators.stochastic.k}} ({{analysis_data.indicators.stochastic.signal}})
- Current Price: {{analysis_data.current_price}}

MARKET REGIME:
- Regime: {{regime_data.regime}}
- Volatility: {{regime_data.volatility_level}}
- Trend: {{regime_data.trend}}

MARKET SIGNALS:
- Volume Spike: {{analysis_data.signals.volume_spike.spike}}
- Gap: {{analysis_data.signals.gap.gap}}
- Breakout: {{analysis_data.signals.breakout.breakout}}
- Trend Direction: {{analysis_data.signals.trend.trend}}

Return your analysis as JSON with these fields:
- sentiment: "bullish" or "bearish" or "neutral"
- confidence: number 0-100
- signal: "BUY" or "SELL" or "HOLD"
- expected_move_pct: number (expected percentage move)
- time_horizon: string (e.g. "1-2 days" or "3-5 days")
- reasoning: string (one paragraph explanation)
- risk_factors: list of strings
- key_levels: object with "support" and "resistance" numbers"""

    ask AI to prompt using analysis_data, regime_data save as llm_insight
    respond with llm_insight

to llm_news_sentiment with symbol:
    purpose: "Analyze news sentiment for a symbol using LLM"
    set news_url to "{{config.data_service}}/news"
    set news_data to http_get(news_url)
    set prompt to """Analyze the current market trends for {{symbol}} on the Indian stock exchange (NSE/BSE).

Trending stocks and market context:
{{news_data}}

Provide a structured JSON response:
- overall_sentiment: "bullish" or "bearish" or "neutral"
- confidence: number 0-100
- key_drivers: list of strings explaining major factors
- risk_events: list of upcoming risks
- sector_outlook: string"""

    ask AI to prompt save as sentiment
    respond with sentiment

// ═══════════════════════════════════════════════════════════════
// SIGNAL AGGREGATION
// ═══════════════════════════════════════════════════════════════

to aggregate_signals with technical_signal, swarm_signal, options_signal, llm_signal:
    purpose: "Weighted voting across all signal sources"
    // Weight configuration
    set w_technical to 0.30
    set w_swarm to 0.25
    set w_options to 0.20
    set w_llm to 0.25
    // Convert signals to numeric scores (-1 to +1)
    set tech_score to 0
    if technical_signal.regime is equal "bullish_momentum":
        set tech_score to 0.7
    if technical_signal.regime is equal "bearish_momentum":
        set tech_score to -0.7
    if technical_signal.trend is equal "bullish":
        set tech_score to tech_score + 0.3
    if technical_signal.trend is equal "bearish":
        set tech_score to tech_score - 0.3
    // RSI contribution
    if technical_signal.rsi is below 30:
        set tech_score to tech_score + 0.3
    if technical_signal.rsi is above 70:
        set tech_score to tech_score - 0.3
    set swarm_score to 0
    if swarm_signal.signal is equal "buy":
        set swarm_score to swarm_signal.confidence / 100
    if swarm_signal.signal is equal "sell":
        set swarm_score to 0 - (swarm_signal.confidence / 100)
    set options_score to 0
    if options_signal is not equal nothing:
        if options_signal.signal is equal "bullish":
            set options_score to options_signal.confidence / 100
        if options_signal.signal is equal "bearish":
            set options_score to 0 - (options_signal.confidence / 100)
    set llm_score to 0
    if llm_signal is not equal nothing:
        if llm_signal.sentiment is equal "bullish":
            set llm_score to llm_signal.confidence / 100
        if llm_signal.sentiment is equal "bearish":
            set llm_score to 0 - (llm_signal.confidence / 100)
    // Weighted composite
    set composite to (tech_score * w_technical) + (swarm_score * w_swarm) + (options_score * w_options) + (llm_score * w_llm)
    // Convert to signal
    set final_signal to "HOLD"
    if composite is above 0.2:
        set final_signal to "BUY"
    if composite is below -0.2:
        set final_signal to "SELL"
    // Confidence based on agreement among sources
    set confidence to abs(composite) * 100
    set agree_count to 0
    if tech_score is above 0 and composite is above 0:
        set agree_count to agree_count + 1
    if tech_score is below 0 and composite is below 0:
        set agree_count to agree_count + 1
    if swarm_score is above 0 and composite is above 0:
        set agree_count to agree_count + 1
    if swarm_score is below 0 and composite is below 0:
        set agree_count to agree_count + 1
    if options_score is above 0 and composite is above 0:
        set agree_count to agree_count + 1
    if options_score is below 0 and composite is below 0:
        set agree_count to agree_count + 1
    if llm_score is above 0 and composite is above 0:
        set agree_count to agree_count + 1
    if llm_score is below 0 and composite is below 0:
        set agree_count to agree_count + 1
    // Boost confidence when sources agree
    set confidence to confidence + (agree_count * 5)
    if confidence is above 95:
        set confidence to 95
    respond with {
        "signal": final_signal,
        "confidence": round(confidence, 2),
        "composite_score": round(composite, 3),
        "source_scores": {
            "technical": round(tech_score, 2),
            "swarm": round(swarm_score, 2),
            "options": round(options_score, 2),
            "llm": round(llm_score, 2)
        },
        "source_agreement": agree_count,
        "weights": {
            "technical": w_technical,
            "swarm": w_swarm,
            "options": w_options,
            "llm": w_llm
        }
    }

// ═══════════════════════════════════════════════════════════════
// RISK ENGINE
// ═══════════════════════════════════════════════════════════════

to risk_check with signal, symbol, current_price, atr, portfolio:
    purpose: "Validate signal against risk management rules"
    set approved to yes
    set rejections to []
    // Rule 1: Minimum confidence threshold
    if signal.confidence is below 60:
        set approved to no
        append "Confidence {{signal.confidence}}% below minimum 60%" to rejections
    // Rule 2: Position sizing (max 2% of capital per trade)
    set capital to portfolio.total_capital
    set max_position_value to capital * 0.02
    set position_size to round(max_position_value / current_price)
    // Rule 3: Stop loss calculation using ATR
    set stop_loss_distance to atr * 2
    set stop_loss_price to current_price - stop_loss_distance
    if signal.signal is equal "SELL":
        set stop_loss_price to current_price + stop_loss_distance
    set risk_per_share to abs(current_price - stop_loss_price)
    set total_risk to risk_per_share * position_size
    set risk_pct to (total_risk / capital) * 100
    if risk_pct is above 2:
        set position_size to round((capital * 0.02) / risk_per_share)
    // Rule 4: Portfolio exposure check
    set current_exposure to portfolio.current_exposure
    set new_exposure to current_exposure + (position_size * current_price)
    set exposure_pct to (new_exposure / capital) * 100
    if exposure_pct is above 20:
        set approved to no
        append "Portfolio exposure {{exposure_pct}}% would exceed 20% limit" to rejections
    // Rule 5: Volatility filter
    set vol_adjusted_confidence to signal.confidence
    if atr / current_price is above 0.04:
        set vol_adjusted_confidence to signal.confidence * 0.8
        if vol_adjusted_confidence is below 60:
            set approved to no
            append "High volatility reduces confidence below threshold" to rejections
    // Rule 6: No HOLD signals should generate trades
    if signal.signal is equal "HOLD":
        set approved to no
        append "HOLD signal - no trade recommended" to rejections
    set target_price to current_price + (atr * 3)
    if signal.signal is equal "SELL":
        set target_price to current_price - (atr * 3)
    set reward_risk_ratio to 0
    if risk_per_share is above 0:
        set reward_risk_ratio to abs(target_price - current_price) / risk_per_share
    respond with {
        "approved": approved,
        "rejections": rejections,
        "position_size": position_size,
        "position_value": round(position_size * current_price),
        "stop_loss": round(stop_loss_price, 2),
        "target_price": round(target_price, 2),
        "risk_per_share": round(risk_per_share, 2),
        "total_risk": round(total_risk),
        "risk_pct": round(risk_pct, 2),
        "reward_risk_ratio": round(reward_risk_ratio, 2),
        "adjusted_confidence": round(vol_adjusted_confidence, 2)
    }

// ═══════════════════════════════════════════════════════════════
// COMPLETE SIGNAL GENERATION PIPELINE
// ═══════════════════════════════════════════════════════════════

to generate_signal with symbol:
    purpose: "Full signal generation pipeline for a symbol"
    log "Generating signal for {{symbol}}..."
    set analysis to http_post("{{config.analyzer_service}}/analyze", {"symbol": symbol})
    if analysis.error is not equal nothing:
        respond with {"error": "Analysis failed for {{symbol}}", "details": analysis.error}
    set swarm_result to http_post("{{config.swarm_service}}/signal", {"symbol": symbol})
    set options_result to nothing
    try:
        gather options_chain from "{{config.data_service}}/options/chain":
            method: "POST"
            body: {"symbol": symbol}
            content_type: "application/json"
        if options_chain.chain is not equal nothing:
            gather options_result from "{{config.analyzer_service}}/options/analyze":
                method: "POST"
                body: {"symbol": symbol, "spot_price": analysis.current_price}
                content_type: "application/json"
            set options_result to options_result.options_analysis
    on error:
        log "Options data not available for {{symbol}}"
    set llm_result to nothing
    try:
        run llm_market_analysis with symbol, analysis, analysis.regime
        set llm_result to result
    on error:
        log "LLM analysis skipped for {{symbol}}"
        run llm_news_sentiment with symbol
        set llm_result to result
    run aggregate_signals with analysis.regime, swarm_result, options_result, llm_result
    set aggregated to result
    set portfolio to {"total_capital": 1000000, "current_exposure": 0}
    run risk_check with aggregated, symbol, analysis.current_price, analysis.indicators.atr, portfolio
    set risk to result
    set final_approved to risk.approved
    if aggregated.confidence is below 60:
        set final_approved to no
    set expected_move to 0
    if llm_result is not equal nothing and llm_result.expected_move_pct is not equal nothing:
        set expected_move to llm_result.expected_move_pct
    set time_horizon to "1-3 days"
    if llm_result is not equal nothing and llm_result.time_horizon is not equal nothing:
        set time_horizon to llm_result.time_horizon
    set reasoning to "Signal based on technical analysis, swarm intelligence, and statistical models."
    if llm_result is not equal nothing and llm_result.reasoning is not equal nothing:
        set reasoning to llm_result.reasoning
    respond with {
        "symbol": symbol,
        "timestamp": time_now(),
        "signal": aggregated.signal,
        "confidence": aggregated.confidence,
        "approved": final_approved,
        "current_price": analysis.current_price,
        "expected_move_pct": expected_move,
        "time_horizon": time_horizon,
        "reasoning": reasoning,
        "risk_management": {
            "position_size": risk.position_size,
            "stop_loss": risk.stop_loss,
            "target_price": risk.target_price,
            "reward_risk_ratio": risk.reward_risk_ratio,
            "risk_pct": risk.risk_pct
        },
        "components": {
            "technical": {
                "regime": analysis.regime.regime,
                "rsi": analysis.indicators.rsi,
                "macd": analysis.indicators.macd,
                "trend": analysis.signals.trend.trend
            },
            "swarm": {
                "signal": swarm_result.signal,
                "confidence": swarm_result.confidence,
                "strategy_type": swarm_result.strategy_type
            },
            "options": options_result,
            "llm": llm_result
        },
        "source_scores": aggregated.source_scores,
        "rejections": risk.rejections
    }

to generate_bulk_signals with symbols:
    purpose: "Generate signals for multiple symbols"
    set signals to []
    repeat for each symbol in symbols:
        run generate_signal with symbol
        append result to signals
    // Sort by confidence descending
    set top_signals to []
    repeat for each sig in signals:
        if sig.approved is equal yes:
            append sig to top_signals
    respond with {
        "total_analyzed": len(symbols),
        "approved_signals": len(top_signals),
        "signals": signals,
        "top_picks": top_signals,
        "timestamp": time_now()
    }

to scan_nifty50:
    purpose: "Scan all NIFTY 50 stocks for trading signals"
    gather symbols from "{{config.data_service}}/symbols/nifty50"
    run generate_bulk_signals with symbols
    respond with result

// ═══════════════════════════════════════════════════════════════
// ML MODEL INTEGRATION
// ═══════════════════════════════════════════════════════════════

to ml_predict with symbol, features:
    purpose: "Get ML model prediction"
    try:
        set model to load_model("models/saved/{{symbol}}_ensemble.pkl")
        set prediction to predict(model, features)
        respond with {
            "symbol": symbol,
            "probability_up": prediction.probabilities[1],
            "probability_down": prediction.probabilities[0],
            "predicted_direction": prediction.prediction,
            "confidence": prediction.confidence
        }
    on error:
        log "ML model not available for {{symbol}}, using fallback"
        respond with {"symbol": symbol, "error": "model_not_available"}

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /signal runs generate_signal
    POST /signals/bulk runs generate_bulk_signals
    GET /scan/nifty50 runs scan_nifty50
    POST /ml/predict runs ml_predict
    POST /llm/analyze runs llm_market_analysis
    POST /llm/sentiment runs llm_news_sentiment

to health_check:
    respond with {"service": "signal-engine", "status": "healthy", "version": "1.0.0"}
