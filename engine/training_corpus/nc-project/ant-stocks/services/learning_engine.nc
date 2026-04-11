service "neuraledge-learning-engine"
version "1.0.0"
description "Self-learning engine: tracks predictions, measures accuracy, auto-adjusts weights"

configure:
    port: 8007
    data_service is "http://localhost:8001"
    signal_service is "http://localhost:8004"

// ═══════════════════════════════════════════════════════════════
// PREDICTION TRACKING — Store every signal with outcome
// ═══════════════════════════════════════════════════════════════

to record_prediction with symbol, signal, confidence, entry_price, stop_loss, target_price:
    purpose: "Store a prediction for later accuracy tracking"
    set prediction to {
        "symbol": symbol,
        "signal": signal,
        "confidence": confidence,
        "entry_price": entry_price,
        "stop_loss": stop_loss,
        "target_price": target_price,
        "timestamp": time_iso(),
        "unix_time": time_now(),
        "outcome": "pending",
        "exit_price": 0,
        "pnl_pct": 0,
        "correct": no,
        "days_held": 0
    }
    set filename to "data/predictions/{{symbol}}_{{time_now()}}.json"
    mkdir("data/predictions")
    write_file(filename, json_encode(prediction))
    store prediction into "prediction_{{symbol}}_{{time_now()}}"
    respond with {"stored": yes, "prediction": prediction}

to check_prediction_outcome with symbol, entry_price, signal, entry_time:
    purpose: "Check if a past prediction was correct by comparing current price"
    set current_data to http_post("{{config.data_service}}/stock/ohlcv", {"symbol": symbol, "period": "5d", "interval": "1d"})
    if current_data.error is not equal nothing:
        respond with {"error": "Cannot fetch current price"}
    set candles to current_data.candles
    if len(candles) is below 1:
        respond with {"error": "No candle data"}
    set current_price to candles[len(candles) - 1].close
    set pnl_pct to ((current_price - entry_price) / entry_price) * 100
    set correct to no
    if signal is equal "BUY" and current_price is above entry_price:
        set correct to yes
    if signal is equal "SELL" and current_price is below entry_price:
        set correct to yes
    set days_elapsed to (time_now() - entry_time) / 86400
    respond with {
        "symbol": symbol,
        "signal": signal,
        "entry_price": entry_price,
        "current_price": current_price,
        "pnl_pct": round(pnl_pct, 2),
        "correct": correct,
        "days_elapsed": round(days_elapsed, 1)
    }

// ═══════════════════════════════════════════════════════════════
// ACCURACY TRACKER — Learn from past predictions
// ═══════════════════════════════════════════════════════════════

to compute_accuracy with predictions:
    purpose: "Compute accuracy stats from a list of resolved predictions"
    set total to len(predictions)
    if total is equal 0:
        respond with {"accuracy": 0, "total": 0}
    set correct_count to len(filter_by(predictions, "correct", "equal", yes))
    set accuracy to (correct_count / total) * 100
    set buy_preds to filter_by(predictions, "signal", "equal", "BUY")
    set sell_preds to filter_by(predictions, "signal", "equal", "SELL")
    set buy_correct to len(filter_by(buy_preds, "correct", "equal", yes))
    set sell_correct to len(filter_by(sell_preds, "correct", "equal", yes))
    set buy_accuracy to 0
    if len(buy_preds) is above 0:
        set buy_accuracy to (buy_correct / len(buy_preds)) * 100
    set sell_accuracy to 0
    if len(sell_preds) is above 0:
        set sell_accuracy to (sell_correct / len(sell_preds)) * 100
    set avg_pnl to 0
    if total is above 0:
        set avg_pnl to sum_by(predictions, "pnl_pct") / total
    respond with {
        "total_predictions": total,
        "correct": correct_count,
        "accuracy_pct": round(accuracy, 2),
        "buy_accuracy_pct": round(buy_accuracy, 2),
        "sell_accuracy_pct": round(sell_accuracy, 2),
        "avg_pnl_pct": round(avg_pnl, 2),
        "best_trade": max_by(predictions, "pnl_pct"),
        "worst_trade": min_by(predictions, "pnl_pct")
    }

// ═══════════════════════════════════════════════════════════════
// WEIGHT OPTIMIZATION — Auto-adjust signal source weights
// ═══════════════════════════════════════════════════════════════

to optimize_weights with history:
    purpose: "Adjust signal weights based on which sources predicted correctly"
    set w_technical to 0.30
    set w_swarm to 0.25
    set w_options to 0.20
    set w_llm to 0.25
    if len(history) is below 10:
        respond with {"technical": w_technical, "swarm": w_swarm, "options": w_options, "llm": w_llm, "note": "Not enough history to optimize"}
    set tech_correct to 0
    set swarm_correct to 0
    set options_correct to 0
    set llm_correct to 0
    set total to len(history)
    repeat for each h in history:
        if h.correct is equal yes:
            if h.source_scores is not equal nothing:
                if h.source_scores.technical is above 0 and h.signal is equal "BUY":
                    set tech_correct to tech_correct + 1
                if h.source_scores.technical is below 0 and h.signal is equal "SELL":
                    set tech_correct to tech_correct + 1
                if h.source_scores.swarm is above 0 and h.signal is equal "BUY":
                    set swarm_correct to swarm_correct + 1
                if h.source_scores.llm is above 0 and h.signal is equal "BUY":
                    set llm_correct to llm_correct + 1
    set tech_rate to tech_correct / total
    set swarm_rate to swarm_correct / total
    set llm_rate to llm_correct / total
    set sum_rates to tech_rate + swarm_rate + 0.1 + llm_rate
    if sum_rates is above 0:
        set w_technical to round(tech_rate / sum_rates, 2)
        set w_swarm to round(swarm_rate / sum_rates, 2)
        set w_options to round(0.1 / sum_rates, 2)
        set w_llm to round(llm_rate / sum_rates, 2)
    respond with {
        "technical": w_technical,
        "swarm": w_swarm,
        "options": w_options,
        "llm": w_llm,
        "based_on": total,
        "note": "Weights auto-adjusted from prediction history"
    }

// ═══════════════════════════════════════════════════════════════
// PRICE HISTORY FOR CHARTS — Multi-year data
// ═══════════════════════════════════════════════════════════════

to get_chart_data with symbol, period:
    purpose: "Get OHLCV data formatted for charting with indicator overlays"
    set chart_data to http_post("{{config.data_service}}/stock/ohlcv", {"symbol": symbol, "period": period, "interval": "1d"})
    if chart_data.error is not equal nothing:
        respond with {"error": chart_data.error}
    set candles to chart_data.candles
    set closes to map_field(candles, "close")
    set highs to map_field(candles, "high")
    set lows to map_field(candles, "low")
    set volumes to map_field(candles, "volume")
    set timestamps to map_field(candles, "timestamp")
    // Compute overlay indicators for chart
    set sma20 to []
    set sma50 to []
    set bb_upper to []
    set bb_lower to []
    set rsi_values to []
    set vol_avg to []
    repeat for each i in range(0, len(closes)):
        // SMA 20
        if i is at least 19:
            set w to slice(closes, i - 19, i + 1)
            append round(average(w), 2) to sma20
        otherwise:
            append nothing to sma20
        // SMA 50
        if i is at least 49:
            set w to slice(closes, i - 49, i + 1)
            append round(average(w), 2) to sma50
        otherwise:
            append nothing to sma50
        // Bollinger upper/lower
        if i is at least 19:
            set w to slice(closes, i - 19, i + 1)
            set mid to average(w)
            set sq to 0
            repeat for each v in w:
                set d to v - mid
                set sq to sq + (d * d)
            set sd to sqrt(sq / 20)
            append round(mid + 2 * sd, 2) to bb_upper
            append round(mid - 2 * sd, 2) to bb_lower
        otherwise:
            append nothing to bb_upper
            append nothing to bb_lower
        // Volume average
        if i is at least 19:
            set vw to slice(volumes, i - 19, i + 1)
            append round(average(vw)) to vol_avg
        otherwise:
            append nothing to vol_avg
    respond with {
        "symbol": symbol,
        "period": period,
        "count": len(candles),
        "timestamps": timestamps,
        "open": map_field(candles, "open"),
        "high": highs,
        "low": lows,
        "close": closes,
        "volume": volumes,
        "overlays": {
            "sma20": sma20,
            "sma50": sma50,
            "bb_upper": bb_upper,
            "bb_lower": bb_lower,
            "volume_avg": vol_avg
        }
    }

// ═══════════════════════════════════════════════════════════════
// PERFORMANCE DASHBOARD DATA
// ═══════════════════════════════════════════════════════════════

to get_system_performance:
    purpose: "Get overall system prediction performance"
    // Read stored predictions from disk
    set pred_files to []
    try:
        set dir_listing to shell("ls data/predictions/ 2>/dev/null | head -100")
        if len(dir_listing) is above 0:
            set pred_files to split(dir_listing, "\n")
    on error:
        log "No prediction history found"
    set predictions to []
    repeat for each fname in pred_files:
        if len(fname) is above 0:
            try:
                set content to read_file("data/predictions/" + fname)
                set pred to json_decode(content)
                append pred to predictions
            on error:
                log "Could not read {{fname}}"
    set resolved to filter_by(predictions, "outcome", "not_equal", "pending")
    if len(resolved) is above 0:
        run compute_accuracy with resolved
        set accuracy to result
    otherwise:
        set accuracy to {"total_predictions": 0, "accuracy_pct": 0, "note": "No resolved predictions yet"}
    respond with {
        "total_stored": len(predictions),
        "resolved": len(resolved),
        "pending": len(predictions) - len(resolved),
        "accuracy": accuracy,
        "timestamp": time_iso()
    }

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /predict/record runs record_prediction
    POST /predict/check runs check_prediction_outcome
    POST /predict/accuracy runs compute_accuracy
    POST /weights/optimize runs optimize_weights
    POST /chart runs get_chart_data
    GET /performance runs get_system_performance

to health_check:
    respond with {"service": "learning-engine", "status": "healthy", "version": "1.0.0", "timestamp": time_iso()}
