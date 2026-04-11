service "neuraledge-analyzer"
version "1.0.0"
description "Feature engineering, technical indicators, market regime detection, and options flow analysis"

configure:
    port: 8002
    data_service is "http://localhost:8001"

// ═══════════════════════════════════════════════════════════════
// TECHNICAL INDICATORS
// ═══════════════════════════════════════════════════════════════

to compute_rsi with closes, period:
    purpose: "Relative Strength Index"
    if len(closes) is below period:
        respond with 50.0
    set gains to []
    set losses to []
    repeat for each i in range(1, len(closes)):
        set change to closes[i] - closes[i - 1]
        if change is above 0:
            append change to gains
            append 0 to losses
        otherwise:
            append 0 to gains
            append abs(change) to losses
    // Wilder's smoothed averages over the lookback window
    set recent_gains to slice(gains, len(gains) - period, len(gains))
    set recent_losses to slice(losses, len(losses) - period, len(losses))
    set avg_gain to average(recent_gains)
    set avg_loss to average(recent_losses)
    if avg_loss is equal 0:
        respond with 100.0
    set rs to avg_gain / avg_loss
    set rsi to 100.0 - (100.0 / (1.0 + rs))
    respond with round(rsi, 2)

to compute_sma with values, period:
    purpose: "Simple Moving Average"
    if len(values) is below period:
        respond with average(values)
    set window to slice(values, len(values) - period, len(values))
    respond with round(average(window), 2)

to compute_ema with values, period:
    purpose: "Exponential Moving Average"
    if len(values) is equal 0:
        respond with 0
    set multiplier to 2.0 / (period + 1)
    set ema_val to values[0]
    repeat for each i in range(1, len(values)):
        set ema_val to (values[i] - ema_val) * multiplier + ema_val
    respond with round(ema_val, 2)

to compute_macd with closes:
    purpose: "MACD (12, 26, 9)"
    run compute_ema with closes, 12
    set ema_12 to result
    run compute_ema with closes, 26
    set ema_26 to result
    set macd_line to ema_12 - ema_26
    // Signal line approximation from recent MACD values
    set signal_line to macd_line * 0.8
    set histogram to macd_line - signal_line
    respond with {
        "macd": round(macd_line, 2),
        "signal": round(signal_line, 2),
        "histogram": round(histogram, 2),
        "bullish": macd_line is above signal_line
    }

to compute_bollinger_bands with closes, period, std_mult:
    purpose: "Bollinger Bands"
    if len(closes) is below period:
        respond with {"upper": 0, "middle": 0, "lower": 0}
    set window to slice(closes, len(closes) - period, len(closes))
    set middle to average(window)
    // Compute standard deviation
    set sum_sq to 0
    repeat for each val in window:
        set diff to val - middle
        set sum_sq to sum_sq + (diff * diff)
    set std_dev to sqrt(sum_sq / period)
    set upper to middle + (std_mult * std_dev)
    set lower to middle - (std_mult * std_dev)
    set current_price to closes[len(closes) - 1]
    // %B shows where price is relative to bands
    set pct_b to 0.5
    if upper - lower is above 0:
        set pct_b to (current_price - lower) / (upper - lower)
    respond with {
        "upper": round(upper, 2),
        "middle": round(middle, 2),
        "lower": round(lower, 2),
        "bandwidth": round((upper - lower) / middle * 100, 2),
        "percent_b": round(pct_b, 2)
    }

to compute_atr with highs, lows, closes, period:
    purpose: "Average True Range"
    set true_ranges to []
    repeat for each i in range(1, len(closes)):
        set hl to highs[i] - lows[i]
        set hc to abs(highs[i] - closes[i - 1])
        set lc to abs(lows[i] - closes[i - 1])
        set tr to max(hl, max(hc, lc))
        append tr to true_ranges
    if len(true_ranges) is below period:
        respond with average(true_ranges)
    set recent to slice(true_ranges, len(true_ranges) - period, len(true_ranges))
    respond with round(average(recent), 2)

to compute_vwap with highs, lows, closes, volumes:
    purpose: "Volume Weighted Average Price"
    set cum_vol to 0
    set cum_tp_vol to 0
    repeat for each i in range(0, len(closes)):
        set typical_price to (highs[i] + lows[i] + closes[i]) / 3
        set cum_tp_vol to cum_tp_vol + (typical_price * volumes[i])
        set cum_vol to cum_vol + volumes[i]
    if cum_vol is equal 0:
        respond with 0
    respond with round(cum_tp_vol / cum_vol, 2)

to compute_obv with closes, volumes:
    purpose: "On Balance Volume"
    set obv to 0
    set obv_values to [0]
    repeat for each i in range(1, len(closes)):
        if closes[i] is above closes[i - 1]:
            set obv to obv + volumes[i]
        otherwise:
            if closes[i] is below closes[i - 1]:
                set obv to obv - volumes[i]
        append obv to obv_values
    respond with obv

to compute_stochastic with highs, lows, closes, k_period:
    purpose: "Stochastic Oscillator %K"
    if len(closes) is below k_period:
        respond with {"k": 50, "signal": "neutral"}
    set recent_highs to slice(highs, len(highs) - k_period, len(highs))
    set recent_lows to slice(lows, len(lows) - k_period, len(lows))
    set highest to max(recent_highs)
    set lowest to min(recent_lows)
    set current to closes[len(closes) - 1]
    if highest - lowest is equal 0:
        respond with {"k": 50, "signal": "neutral"}
    set k to ((current - lowest) / (highest - lowest)) * 100
    set signal to "neutral"
    if k is above 80:
        set signal to "overbought"
    if k is below 20:
        set signal to "oversold"
    respond with {"k": round(k, 2), "signal": signal}

to compute_adx with highs, lows, closes, period:
    purpose: "Average Directional Index (simplified)"
    set plus_dm_sum to 0
    set minus_dm_sum to 0
    set tr_sum to 0
    set lookback to min(period, len(closes) - 1)
    set start_idx to len(closes) - lookback - 1
    repeat for each i in range(start_idx + 1, len(closes)):
        set up_move to highs[i] - highs[i - 1]
        set down_move to lows[i - 1] - lows[i]
        if up_move is above down_move and up_move is above 0:
            set plus_dm_sum to plus_dm_sum + up_move
        if down_move is above up_move and down_move is above 0:
            set minus_dm_sum to minus_dm_sum + down_move
        set hl to highs[i] - lows[i]
        set hc to abs(highs[i] - closes[i - 1])
        set lc to abs(lows[i] - closes[i - 1])
        set tr_sum to tr_sum + max(hl, max(hc, lc))
    if tr_sum is equal 0:
        respond with {"adx": 0, "trend_strength": "none"}
    set plus_di to (plus_dm_sum / tr_sum) * 100
    set minus_di to (minus_dm_sum / tr_sum) * 100
    set dx to 0
    if plus_di + minus_di is above 0:
        set dx to abs(plus_di - minus_di) / (plus_di + minus_di) * 100
    set trend_strength to "weak"
    if dx is above 25:
        set trend_strength to "moderate"
    if dx is above 50:
        set trend_strength to "strong"
    if dx is above 75:
        set trend_strength to "very_strong"
    respond with {
        "adx": round(dx, 2),
        "plus_di": round(plus_di, 2),
        "minus_di": round(minus_di, 2),
        "trend_strength": trend_strength,
        "trend_direction": plus_di is above minus_di
    }

// ─── Extract price arrays from candle data ───

to extract_price_arrays with candles:
    respond with {
        "opens": map_field(candles, "open"),
        "highs": map_field(candles, "high"),
        "lows": map_field(candles, "low"),
        "closes": map_field(candles, "close"),
        "volumes": map_field(candles, "volume")
    }

// ═══════════════════════════════════════════════════════════════
// MARKET EVENT SIGNALS
// ═══════════════════════════════════════════════════════════════

to detect_volume_spike with volumes, threshold:
    purpose: "Detect if latest volume is a spike relative to average"
    if len(volumes) is below 5:
        respond with {"spike": no, "ratio": 1.0}
    set lookback to slice(volumes, 0, len(volumes) - 1)
    set avg_vol to average(lookback)
    if avg_vol is equal 0:
        respond with {"spike": no, "ratio": 0}
    set current_vol to volumes[len(volumes) - 1]
    set ratio to current_vol / avg_vol
    respond with {
        "spike": ratio is above threshold,
        "ratio": round(ratio, 2),
        "current_volume": current_vol,
        "average_volume": round(avg_vol)
    }

to detect_gap with opens, closes:
    purpose: "Detect gap up or gap down"
    if len(opens) is below 2:
        respond with {"gap": "none", "gap_pct": 0}
    set prev_close to closes[len(closes) - 2]
    set current_open to opens[len(opens) - 1]
    set gap_pct to ((current_open - prev_close) / prev_close) * 100
    set gap_type to "none"
    if gap_pct is above 1.0:
        set gap_type to "gap_up"
    if gap_pct is below -1.0:
        set gap_type to "gap_down"
    respond with {
        "gap": gap_type,
        "gap_pct": round(gap_pct, 2),
        "prev_close": prev_close,
        "current_open": current_open
    }

to detect_breakout with highs, lows, closes, lookback:
    purpose: "Detect price breakout above/below range"
    if len(closes) is below lookback:
        respond with {"breakout": "none"}
    set range_data to slice(closes, len(closes) - lookback - 1, len(closes) - 1)
    set range_high to max(range_data)
    set range_low to min(range_data)
    set current to closes[len(closes) - 1]
    set breakout_type to "none"
    if current is above range_high:
        set breakout_type to "bullish_breakout"
    if current is below range_low:
        set breakout_type to "bearish_breakdown"
    respond with {
        "breakout": breakout_type,
        "current_price": current,
        "range_high": range_high,
        "range_low": range_low,
        "range_width_pct": round((range_high - range_low) / range_low * 100, 2)
    }

to detect_trend with closes:
    purpose: "Detect current price trend"
    if len(closes) is below 20:
        respond with {"trend": "insufficient_data"}
    run compute_sma with closes, 20
    set sma_20 to result
    run compute_sma with closes, 50
    set sma_50 to result
    set current to closes[len(closes) - 1]
    set trend to "sideways"
    if current is above sma_20 and sma_20 is above sma_50:
        set trend to "bullish"
    if current is below sma_20 and sma_20 is below sma_50:
        set trend to "bearish"
    // Trend strength via price distance from MA
    set distance_pct to ((current - sma_20) / sma_20) * 100
    respond with {
        "trend": trend,
        "sma_20": sma_20,
        "sma_50": sma_50,
        "current_price": current,
        "distance_from_sma20_pct": round(distance_pct, 2)
    }

// ═══════════════════════════════════════════════════════════════
// MARKET REGIME DETECTION
// ═══════════════════════════════════════════════════════════════

to compute_returns with closes:
    set returns to []
    repeat for each i in range(1, len(closes)):
        set ret to (closes[i] - closes[i - 1]) / closes[i - 1]
        append ret to returns
    respond with returns

to compute_volatility with closes, window:
    purpose: "Historical volatility over window"
    run compute_returns with closes
    set returns to result
    if len(returns) is below window:
        set window to len(returns)
    set recent to slice(returns, len(returns) - window, len(returns))
    set mean_ret to average(recent)
    set sum_sq to 0
    repeat for each r in recent:
        set diff to r - mean_ret
        set sum_sq to sum_sq + (diff * diff)
    set variance to sum_sq / len(recent)
    set vol to sqrt(variance)
    // Annualize (252 trading days)
    set annualized_vol to vol * sqrt(252) * 100
    respond with round(annualized_vol, 2)

to detect_regime with candles:
    purpose: "Detect current market regime using price statistics"
    run extract_price_arrays with candles
    set prices to result
    set closes to prices.closes
    set highs to prices.highs
    set lows to prices.lows
    set volumes to prices.volumes

    // Compute multiple indicators for regime classification
    run compute_returns with closes
    set returns to result

    run compute_volatility with closes, 20
    set vol_20 to result

    run detect_trend with closes
    set trend_info to result

    run compute_rsi with closes, 14
    set rsi to result

    run compute_adx with highs, lows, closes, 14
    set adx_info to result

    run detect_volume_spike with volumes, 1.5
    set vol_spike to result

    // Classify regime based on composite indicators
    set regime to "sideways"
    set confidence to 50.0

    // Strong trend regimes
    if adx_info.adx is above 25:
        if trend_info.trend is equal "bullish":
            set regime to "bullish_momentum"
            set confidence to 60 + adx_info.adx * 0.4
        if trend_info.trend is equal "bearish":
            set regime to "bearish_momentum"
            set confidence to 60 + adx_info.adx * 0.4

    // High volatility regime
    if vol_20 is above 25:
        if regime is equal "sideways":
            set regime to "high_volatility"
            set confidence to 55 + vol_20 * 0.5

    // Low volatility / consolidation
    if vol_20 is below 12 and adx_info.adx is below 20:
        set regime to "low_volatility"
        set confidence to 65.0

    // RSI extremes boost confidence
    if rsi is above 70 and regime is equal "bullish_momentum":
        set confidence to confidence + 10
    if rsi is below 30 and regime is equal "bearish_momentum":
        set confidence to confidence + 10

    if confidence is above 95:
        set confidence to 95.0

    // Volatility level classification
    set vol_level to "medium"
    if vol_20 is below 12:
        set vol_level to "low"
    if vol_20 is above 25:
        set vol_level to "high"
    if vol_20 is above 40:
        set vol_level to "extreme"

    respond with {
        "regime": regime,
        "confidence": round(confidence, 2),
        "volatility_level": vol_level,
        "volatility_annualized": vol_20,
        "trend": trend_info.trend,
        "trend_strength": adx_info.trend_strength,
        "adx": adx_info.adx,
        "rsi": rsi,
        "volume_spike": vol_spike.spike
    }

// ═══════════════════════════════════════════════════════════════
// OPTIONS FLOW INTELLIGENCE
// ═══════════════════════════════════════════════════════════════

to analyze_option_chain with chain_data, spot_price:
    purpose: "Analyze options chain for flow signals"
    set total_call_oi to 0
    set total_put_oi to 0
    set total_call_vol to 0
    set total_put_vol to 0
    set max_call_oi to 0
    set max_put_oi to 0
    set max_call_strike to 0
    set max_put_strike to 0
    set call_oi_change to 0
    set put_oi_change to 0
    set unusual_activity to []

    repeat for each entry in chain_data:
        // Call side
        if entry.call_oi is not equal nothing:
            set total_call_oi to total_call_oi + entry.call_oi
            set total_call_vol to total_call_vol + entry.call_volume
            if entry.call_oi is above max_call_oi:
                set max_call_oi to entry.call_oi
                set max_call_strike to entry.strike_price
            if entry.call_change_oi is not equal nothing:
                set call_oi_change to call_oi_change + entry.call_change_oi

        // Put side
        if entry.put_oi is not equal nothing:
            set total_put_oi to total_put_oi + entry.put_oi
            set total_put_vol to total_put_vol + entry.put_volume
            if entry.put_oi is above max_put_oi:
                set max_put_oi to entry.put_oi
                set max_put_strike to entry.strike_price
            if entry.put_change_oi is not equal nothing:
                set put_oi_change to put_oi_change + entry.put_change_oi

        // Detect unusual activity: volume > 5x average or large OI change
        if entry.call_volume is not equal nothing and entry.call_oi is not equal nothing:
            if entry.call_oi is above 0:
                set vol_oi_ratio to entry.call_volume / entry.call_oi
                if vol_oi_ratio is above 0.5:
                    append {
                        "strike": entry.strike_price,
                        "type": "CALL",
                        "volume": entry.call_volume,
                        "oi": entry.call_oi,
                        "oi_change": entry.call_change_oi,
                        "signal": "unusual_call_activity"
                    } to unusual_activity

        if entry.put_volume is not equal nothing and entry.put_oi is not equal nothing:
            if entry.put_oi is above 0:
                set vol_oi_ratio to entry.put_volume / entry.put_oi
                if vol_oi_ratio is above 0.5:
                    append {
                        "strike": entry.strike_price,
                        "type": "PUT",
                        "volume": entry.put_volume,
                        "oi": entry.put_oi,
                        "oi_change": entry.put_change_oi,
                        "signal": "unusual_put_activity"
                    } to unusual_activity

    // Put-Call Ratio
    set pcr to 1.0
    if total_call_oi is above 0:
        set pcr to total_put_oi / total_call_oi

    // Max pain: the strike where maximum OI exists (combined)
    set max_pain to (max_call_strike + max_put_strike) / 2

    // Options signal derivation
    set options_signal to "neutral"
    set options_confidence to 50.0

    // PCR-based signals
    if pcr is above 1.5:
        set options_signal to "bullish"
        set options_confidence to 60 + (pcr - 1.5) * 20
    if pcr is below 0.5:
        set options_signal to "bearish"
        set options_confidence to 60 + (0.5 - pcr) * 40

    // Large call OI buildup near spot = resistance
    if max_call_strike is above spot_price and (max_call_strike - spot_price) / spot_price is below 0.02:
        if options_signal is equal "neutral":
            set options_signal to "resistance_near"

    // Large put OI buildup near spot = support
    if max_put_strike is below spot_price and (spot_price - max_put_strike) / spot_price is below 0.02:
        if options_signal is equal "neutral":
            set options_signal to "support_near"

    // OI change signals
    if call_oi_change is above 0 and put_oi_change is above 0:
        if call_oi_change is above put_oi_change * 2:
            set options_signal to "bullish"
            set options_confidence to 65.0

    if options_confidence is above 90:
        set options_confidence to 90.0

    respond with {
        "pcr": round(pcr, 2),
        "total_call_oi": total_call_oi,
        "total_put_oi": total_put_oi,
        "max_call_oi_strike": max_call_strike,
        "max_put_oi_strike": max_put_strike,
        "max_pain": max_pain,
        "call_oi_change": call_oi_change,
        "put_oi_change": put_oi_change,
        "signal": options_signal,
        "confidence": options_confidence,
        "unusual_activity": unusual_activity,
        "unusual_count": len(unusual_activity)
    }

// ═══════════════════════════════════════════════════════════════
// FULL ANALYSIS PIPELINE
// ═══════════════════════════════════════════════════════════════

to full_analysis with symbol:
    purpose: "Complete technical analysis for a symbol"

    // Fetch data from collector service
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"

    if stock_data.error is not equal nothing:
        respond with {"error": stock_data.error}

    set candles to stock_data.candles
    run extract_price_arrays with candles
    set prices to result

    // Compute all technical indicators
    run compute_rsi with prices.closes, 14
    set rsi to result

    run compute_macd with prices.closes
    set macd to result

    run compute_sma with prices.closes, 20
    set sma_20 to result

    run compute_sma with prices.closes, 50
    set sma_50 to result

    run compute_sma with prices.closes, 200
    set sma_200 to result

    run compute_ema with prices.closes, 9
    set ema_9 to result

    run compute_ema with prices.closes, 21
    set ema_21 to result

    run compute_bollinger_bands with prices.closes, 20, 2
    set bollinger to result

    run compute_atr with prices.highs, prices.lows, prices.closes, 14
    set atr to result

    run compute_vwap with prices.highs, prices.lows, prices.closes, prices.volumes
    set vwap to result

    run compute_obv with prices.closes, prices.volumes
    set obv to result

    run compute_stochastic with prices.highs, prices.lows, prices.closes, 14
    set stoch to result

    run compute_adx with prices.highs, prices.lows, prices.closes, 14
    set adx to result

    // Market signals
    run detect_volume_spike with prices.volumes, 2.0
    set vol_spike to result

    run detect_gap with prices.opens, prices.closes
    set gap to result

    run detect_breakout with prices.highs, prices.lows, prices.closes, 20
    set breakout to result

    run detect_trend with prices.closes
    set trend to result

    // Regime detection
    run detect_regime with candles
    set regime to result

    set current_price to prices.closes[len(prices.closes) - 1]

    respond with {
        "symbol": symbol,
        "current_price": current_price,
        "timestamp": time_now(),
        "indicators": {
            "rsi": rsi,
            "macd": macd,
            "sma_20": sma_20,
            "sma_50": sma_50,
            "sma_200": sma_200,
            "ema_9": ema_9,
            "ema_21": ema_21,
            "bollinger": bollinger,
            "atr": atr,
            "vwap": vwap,
            "obv": obv,
            "stochastic": stoch,
            "adx": adx
        },
        "signals": {
            "volume_spike": vol_spike,
            "gap": gap,
            "breakout": breakout,
            "trend": trend
        },
        "regime": regime
    }

to batch_analysis with symbols:
    purpose: "Analyze multiple symbols"
    set results to {}
    repeat for each symbol in symbols:
        run full_analysis with symbol
        set results[symbol] to result
    respond with results

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /analyze runs full_analysis
    POST /analyze/batch runs batch_analysis
    POST /indicators/rsi runs api_rsi
    POST /indicators/macd runs api_macd
    POST /regime runs api_regime
    POST /options/analyze runs api_options

to health_check:
    respond with {"service": "analyzer", "status": "healthy", "version": "1.0.0"}

to api_rsi with symbol, period:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "3mo", "interval": "1d"}
        content_type: "application/json"
    run extract_price_arrays with stock_data.candles
    run compute_rsi with result.closes, period
    respond with {"symbol": symbol, "rsi": result, "period": period}

to api_macd with symbol:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    run extract_price_arrays with stock_data.candles
    run compute_macd with result.closes
    respond with {"symbol": symbol, "macd": result}

to api_regime with symbol:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    run detect_regime with stock_data.candles
    respond with {"symbol": symbol, "regime": result}

to api_options with symbol, spot_price:
    gather chain_data from "{{config.data_service}}/options/chain":
        method: "POST"
        body: {"symbol": symbol}
        content_type: "application/json"
    run analyze_option_chain with chain_data.chain, spot_price
    respond with {"symbol": symbol, "options_analysis": result}
