service "neuraledge-tests"
version "1.0.0"
description "Test suite for NeuralEdge — validates indicators, signals, swarm, and risk engine"

configure:
    port: 8100
    log_requests: true

// =================================================================
// TEST HARNESS
// =================================================================

to assert_equal with actual, expected, test_name:
    if actual is equal expected:
        respond with {"test": test_name, "passed": yes, "actual": actual, "expected": expected}
    otherwise:
        respond with {"test": test_name, "passed": no, "actual": actual, "expected": expected}

to assert_above with actual, threshold, test_name:
    if actual is above threshold:
        respond with {"test": test_name, "passed": yes, "actual": actual, "threshold": threshold}
    otherwise:
        respond with {"test": test_name, "passed": no, "actual": actual, "threshold": threshold}

to assert_below with actual, threshold, test_name:
    if actual is below threshold:
        respond with {"test": test_name, "passed": yes, "actual": actual, "threshold": threshold}
    otherwise:
        respond with {"test": test_name, "passed": no, "actual": actual, "threshold": threshold}

to assert_between with actual, low, high, test_name:
    if actual is at least low and actual is at most high:
        respond with {"test": test_name, "passed": yes, "actual": actual, "low": low, "high": high}
    otherwise:
        respond with {"test": test_name, "passed": no, "actual": actual, "low": low, "high": high}

to assert_not_equal with actual, unexpected, test_name:
    if actual is not equal unexpected:
        respond with {"test": test_name, "passed": yes, "actual": actual}
    otherwise:
        respond with {"test": test_name, "passed": no, "actual": actual, "unexpected": unexpected}

to count_passed with results:
    set c to 0
    repeat for each t in results:
        if t.passed is equal yes:
            set c to c + 1
    respond with c

// =================================================================
// FUNCTIONS UNDER TEST — clean NC, no workarounds
// =================================================================

to compute_rsi with closes, period:
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
    if len(values) is below period:
        respond with round(average(values), 2)
    set window to slice(values, len(values) - period, len(values))
    respond with round(average(window), 2)

to compute_ema with values, period:
    if len(values) is equal 0:
        respond with 0
    set multiplier to 2.0 / (period + 1)
    set ema_val to values[0]
    repeat for each i in range(1, len(values)):
        set ema_val to (values[i] - ema_val) * multiplier + ema_val
    respond with round(ema_val, 2)

to compute_macd with closes:
    if len(closes) is below 26:
        respond with {"macd": 0, "signal": 0, "histogram": 0, "bullish": no}
    set fast_mult to 2.0 / (12 + 1)
    set slow_mult to 2.0 / (26 + 1)
    set sig_mult to 2.0 / (9 + 1)
    set fast_ema to closes[0]
    set slow_ema to closes[0]
    set macd_values to []
    repeat for each i in range(1, len(closes)):
        set fast_ema to (closes[i] - fast_ema) * fast_mult + fast_ema
        set slow_ema to (closes[i] - slow_ema) * slow_mult + slow_ema
        append fast_ema - slow_ema to macd_values
    set signal_ema to macd_values[0]
    repeat for each i in range(1, len(macd_values)):
        set signal_ema to (macd_values[i] - signal_ema) * sig_mult + signal_ema
    set macd_line to macd_values[len(macd_values) - 1]
    set signal_line to signal_ema
    set histogram to macd_line - signal_line
    respond with {
        "macd": round(macd_line, 2),
        "signal": round(signal_line, 2),
        "histogram": round(histogram, 2),
        "bullish": macd_line is above signal_line
    }

to compute_bollinger_bands with closes, period, std_mult:
    if len(closes) is below period:
        respond with {"upper": 0, "middle": 0, "lower": 0, "bandwidth": 0, "percent_b": 0.5}
    set window to slice(closes, len(closes) - period, len(closes))
    set middle to average(window)
    set sum_sq to 0
    repeat for each val in window:
        set diff to val - middle
        set sum_sq to sum_sq + (diff * diff)
    set std_dev to sqrt(sum_sq / period)
    set upper to middle + (std_mult * std_dev)
    set lower to middle - (std_mult * std_dev)
    set current_price to closes[len(closes) - 1]
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
    set true_ranges to []
    repeat for each i in range(1, len(closes)):
        set hl to highs[i] - lows[i]
        set hc to abs(highs[i] - closes[i - 1])
        set lc to abs(lows[i] - closes[i - 1])
        set tr to max(hl, max(hc, lc))
        append tr to true_ranges
    if len(true_ranges) is below period:
        respond with round(average(true_ranges), 2)
    set recent to slice(true_ranges, len(true_ranges) - period, len(true_ranges))
    respond with round(average(recent), 2)

to compute_stochastic with highs, lows, closes, k_period:
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

to classify_sector_signal with avg_change:
    if avg_change is above 3:
        respond with "strong_bullish"
    if avg_change is above 1:
        respond with "bullish"
    if avg_change is below -3:
        respond with "strong_bearish"
    if avg_change is below -1:
        respond with "bearish"
    respond with "neutral"

to aggregate_signals with technical_signal, swarm_signal, options_signal, llm_signal:
    set w_technical to 0.30
    set w_swarm to 0.25
    set w_options to 0.20
    set w_llm to 0.25
    set tech_score to 0
    if technical_signal.regime is equal "bullish_momentum":
        set tech_score to 0.7
    if technical_signal.regime is equal "bearish_momentum":
        set tech_score to -0.7
    if technical_signal.trend is equal "bullish":
        set tech_score to tech_score + 0.3
    if technical_signal.trend is equal "bearish":
        set tech_score to tech_score - 0.3
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
    set composite to (tech_score * w_technical) + (swarm_score * w_swarm) + (options_score * w_options) + (llm_score * w_llm)
    set final_signal to "HOLD"
    if composite is above 0.2:
        set final_signal to "BUY"
    if composite is below -0.2:
        set final_signal to "SELL"
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
    set confidence to confidence + (agree_count * 5)
    if confidence is above 95:
        set confidence to 95
    respond with {"signal": final_signal, "confidence": round(confidence, 2), "composite_score": round(composite, 3), "source_agreement": agree_count}

to risk_check with signal, symbol, current_price, atr, portfolio:
    set approved to yes
    set rejections to []
    if signal.confidence is below 60:
        set approved to no
        append "Low confidence" to rejections
    set capital to portfolio.total_capital
    set stop_loss_distance to atr * 2
    set stop_loss_price to current_price - stop_loss_distance
    if signal.signal is equal "SELL":
        set stop_loss_price to current_price + stop_loss_distance
    set target_price to current_price + (atr * 3)
    if signal.signal is equal "SELL":
        set target_price to current_price - (atr * 3)
    set vol_adjusted_confidence to signal.confidence
    if atr / current_price is above 0.04:
        set vol_adjusted_confidence to signal.confidence * 0.8
        if vol_adjusted_confidence is below 60:
            set approved to no
    if signal.signal is equal "HOLD":
        set approved to no
    respond with {
        "approved": approved,
        "stop_loss": round(stop_loss_price, 2),
        "target_price": round(target_price, 2),
        "adjusted_confidence": round(vol_adjusted_confidence, 2)
    }

to mutate_strategy with strategy:
    set new_params to {}
    repeat for each key in keys(strategy.params):
        set new_params[key] to strategy.params[key]
    set mutated to {
        "type": strategy.type, "params": new_params,
        "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
    }
    respond with mutated

to crossover_strategies with parent1, parent2:
    if parent1.type is equal parent2.type:
        set child_params to {}
        repeat for each key in keys(parent1.params):
            if random() is below 0.5 and has_key(parent2.params, key):
                set child_params[key] to parent2.params[key]
            otherwise:
                set child_params[key] to parent1.params[key]
        respond with {"type": parent1.type, "params": child_params, "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}
    otherwise:
        set source to parent1
        if parent2.fitness is above parent1.fitness:
            set source to parent2
        set child_params to {}
        repeat for each key in keys(source.params):
            set child_params[key] to source.params[key]
        respond with {"type": source.type, "params": child_params, "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}

// =================================================================
// TEST DATA
// =================================================================

to get_uptrend_closes:
    respond with [100, 101, 103, 102, 105, 107, 106, 109, 111, 110, 113, 115, 114, 117, 119, 118, 121, 123, 122, 125, 127, 126, 129, 131, 130, 133, 135, 134, 137, 139]

to get_downtrend_closes:
    respond with [200, 198, 196, 197, 194, 192, 193, 190, 188, 189, 186, 184, 185, 182, 180, 181, 178, 176, 177, 174, 172, 173, 170, 168, 169, 166, 164, 165, 162, 160]

to get_sideways_closes:
    respond with [100, 101, 99, 100, 102, 98, 101, 100, 99, 101, 100, 102, 98, 100, 101, 99, 100, 102, 98, 101, 100, 99, 101, 100, 102, 98, 100, 101, 99, 100]

to get_test_highs:
    respond with [102, 103, 105, 104, 107, 109, 108, 111, 113, 112, 115, 117, 116, 119, 121, 120, 123, 125, 124, 127, 129, 128, 131, 133, 132, 135, 137, 136, 139, 141]

to get_test_lows:
    respond with [98, 99, 101, 100, 103, 105, 104, 107, 109, 108, 111, 113, 112, 115, 117, 116, 119, 121, 120, 123, 125, 124, 127, 129, 128, 131, 133, 132, 135, 137]

// =================================================================
// TEST SUITES
// =================================================================

to test_rsi:
    set results to []
    run get_uptrend_closes
    run compute_rsi with result, 14
    run assert_above with result, 50, "RSI uptrend > 50"
    append result to results
    run get_downtrend_closes
    run compute_rsi with result, 14
    run assert_below with result, 50, "RSI downtrend < 50"
    append result to results
    run get_sideways_closes
    run compute_rsi with result, 14
    run assert_between with result, 30, 70, "RSI sideways 30-70"
    append result to results
    set short to [100, 102, 101, 103, 105]
    run compute_rsi with short, 14
    run assert_equal with result, 50.0, "RSI short = 50"
    append result to results
    run count_passed with results
    respond with {"suite": "RSI", "tests": results, "passed": result, "total": len(results)}

to test_macd:
    set results to []
    set short to [100, 102, 101]
    run compute_macd with short
    run assert_equal with result.macd, 0, "MACD short = 0"
    append result to results
    run get_uptrend_closes
    run compute_macd with result
    set m to result
    run assert_above with m.macd, 0, "MACD uptrend +"
    append result to results
    set old_bug to round(m.macd * 0.8, 2)
    run assert_not_equal with m.signal, old_bug, "Signal != MACD*0.8"
    append result to results
    set expected_h to round(m.macd - m.signal, 2)
    run assert_equal with m.histogram, expected_h, "Hist = MACD-Signal"
    append result to results
    run get_downtrend_closes
    run compute_macd with result
    run assert_below with result.macd, 0, "MACD downtrend -"
    append result to results
    run count_passed with results
    respond with {"suite": "MACD", "tests": results, "passed": result, "total": len(results)}

to test_sma_ema:
    set results to []
    set vals to [10, 20, 30, 40, 50]
    run compute_sma with vals, 5
    run assert_equal with result, 30.0, "SMA 5 = 30"
    append result to results
    run compute_sma with vals, 3
    run assert_equal with result, 40.0, "SMA 3 = 40"
    append result to results
    run get_uptrend_closes
    set closes to result
    run compute_sma with closes, 20
    set sma_v to result
    run compute_ema with closes, 20
    set ema_v to result
    run assert_above with ema_v, sma_v, "EMA > SMA uptrend"
    append result to results
    set empty to []
    run compute_ema with empty, 10
    run assert_equal with result, 0, "EMA empty = 0"
    append result to results
    run count_passed with results
    respond with {"suite": "SMA/EMA", "tests": results, "passed": result, "total": len(results)}

to test_bollinger:
    set results to []
    run get_uptrend_closes
    run compute_bollinger_bands with result, 20, 2.0
    set bb to result
    run assert_above with bb.upper, bb.middle, "Upper > middle"
    append result to results
    run assert_above with bb.middle, bb.lower, "Middle > lower"
    append result to results
    run assert_above with bb.bandwidth, 0, "Bandwidth > 0"
    append result to results
    set short to [100, 102, 101]
    run compute_bollinger_bands with short, 20, 2.0
    run assert_equal with result.upper, 0, "Short data = 0"
    append result to results
    run count_passed with results
    respond with {"suite": "Bollinger", "tests": results, "passed": result, "total": len(results)}

to test_atr:
    set results to []
    run get_test_highs
    set highs to result
    run get_test_lows
    set lows to result
    run get_uptrend_closes
    set closes to result
    run compute_atr with highs, lows, closes, 14
    run assert_above with result, 0, "ATR > 0"
    append result to results
    run assert_below with result, 50, "ATR < 50"
    append result to results
    run count_passed with results
    respond with {"suite": "ATR", "tests": results, "passed": result, "total": len(results)}

to test_stochastic:
    set results to []
    run get_test_highs
    set highs to result
    run get_test_lows
    set lows to result
    run get_uptrend_closes
    set closes to result
    run compute_stochastic with highs, lows, closes, 14
    run assert_between with result.k, 0, 100, "K in 0-100"
    append result to results
    run assert_above with result.k, 50, "K high in uptrend"
    append result to results
    set short_h to [102, 104, 103, 105, 107]
    set short_l to [98, 100, 99, 101, 103]
    set short_c to [100, 102, 101, 103, 105]
    run compute_stochastic with short_h, short_l, short_c, 14
    run assert_equal with result.k, 50, "Short K = 50"
    append result to results
    run count_passed with results
    respond with {"suite": "Stochastic", "tests": results, "passed": result, "total": len(results)}

to test_adx:
    set results to []
    run get_test_highs
    set highs to result
    run get_test_lows
    set lows to result
    run get_uptrend_closes
    set closes to result
    run compute_adx with highs, lows, closes, 14
    set adx to result
    run assert_between with adx.adx, 0, 100, "ADX 0-100"
    append result to results
    run assert_equal with adx.trend_direction, yes, "Bullish uptrend"
    append result to results
    run assert_above with adx.plus_di, 0, "plus_di > 0"
    append result to results
    run count_passed with results
    respond with {"suite": "ADX", "tests": results, "passed": result, "total": len(results)}

to test_sector_signals:
    set results to []
    run classify_sector_signal with 5.0
    run assert_equal with result, "strong_bullish", "5% = strong_bullish"
    append result to results
    run classify_sector_signal with 2.0
    run assert_equal with result, "bullish", "2% = bullish"
    append result to results
    run classify_sector_signal with 0.5
    run assert_equal with result, "neutral", "0.5% = neutral"
    append result to results
    run classify_sector_signal with -2.0
    run assert_equal with result, "bearish", "-2% = bearish"
    append result to results
    run classify_sector_signal with -5.0
    run assert_equal with result, "strong_bearish", "-5% = strong_bearish"
    append result to results
    run classify_sector_signal with 3.5
    run assert_equal with result, "strong_bullish", "3.5% = strong_bullish"
    append result to results
    run classify_sector_signal with -3.5
    run assert_equal with result, "strong_bearish", "-3.5% = strong_bearish"
    append result to results
    run count_passed with results
    respond with {"suite": "Sector Signals", "tests": results, "passed": result, "total": len(results)}

to test_aggregation:
    set results to []
    set tech to {"regime": "bullish_momentum", "trend": "bullish", "rsi": 25}
    set swarm to {"signal": "buy", "confidence": 80}
    set opts to {"signal": "bullish", "confidence": 70}
    set llm to {"sentiment": "bullish", "confidence": 75}
    run aggregate_signals with tech, swarm, opts, llm
    run assert_equal with result.signal, "BUY", "All bullish = BUY"
    append result to results
    set tech to {"regime": "bearish_momentum", "trend": "bearish", "rsi": 80}
    set swarm to {"signal": "sell", "confidence": 80}
    set opts to {"signal": "bearish", "confidence": 70}
    set llm to {"sentiment": "bearish", "confidence": 75}
    run aggregate_signals with tech, swarm, opts, llm
    run assert_equal with result.signal, "SELL", "All bearish = SELL"
    append result to results
    set tech to {"regime": "sideways", "trend": "sideways", "rsi": 50}
    set swarm to {"signal": "hold", "confidence": 40}
    run aggregate_signals with tech, swarm, nothing, nothing
    run assert_equal with result.signal, "HOLD", "Neutral = HOLD"
    append result to results
    run count_passed with results
    respond with {"suite": "Aggregation", "tests": results, "passed": result, "total": len(results)}

to test_risk:
    set results to []
    set sig to {"signal": "BUY", "confidence": 45}
    set pf to {"total_capital": 1000000, "current_exposure": 0}
    run risk_check with sig, "T.NS", 1000, 20, pf
    run assert_equal with result.approved, no, "Low conf rejected"
    append result to results
    set sig to {"signal": "HOLD", "confidence": 70}
    run risk_check with sig, "T.NS", 1000, 20, pf
    run assert_equal with result.approved, no, "HOLD rejected"
    append result to results
    set sig to {"signal": "BUY", "confidence": 80}
    run risk_check with sig, "T.NS", 1000, 20, pf
    run assert_equal with result.approved, yes, "BUY approved"
    append result to results
    set sig to {"signal": "BUY", "confidence": 75}
    run risk_check with sig, "T.NS", 1000, 20, pf
    run assert_below with result.stop_loss, 1000, "BUY SL < entry"
    append result to results
    set sig to {"signal": "SELL", "confidence": 75}
    run risk_check with sig, "T.NS", 1000, 20, pf
    run assert_above with result.stop_loss, 1000, "SELL SL > entry"
    append result to results
    set sig to {"signal": "BUY", "confidence": 70}
    run risk_check with sig, "T.NS", 1000, 50, pf
    run assert_below with result.adjusted_confidence, 70, "Vol reduces conf"
    append result to results
    run count_passed with results
    respond with {"suite": "Risk Engine", "tests": results, "passed": result, "total": len(results)}

to test_mutation:
    set results to []
    set orig to {"type": "rsi_reversal", "params": {"oversold": 30, "overbought": 70, "period": 14}, "fitness": 50, "sharpe": 1.2, "win_rate": 60, "max_dd": 10, "trades": 20}
    set saved_os to orig.params.oversold
    set saved_fit to orig.fitness
    run mutate_strategy with orig
    set m to result
    run assert_equal with m.fitness, 0, "Mutated fitness=0"
    append result to results
    run assert_equal with m.type, "rsi_reversal", "Type preserved"
    append result to results
    run assert_equal with orig.params.oversold, saved_os, "Orig param safe"
    append result to results
    run assert_equal with orig.fitness, saved_fit, "Orig fitness safe"
    append result to results
    run count_passed with results
    respond with {"suite": "Mutation", "tests": results, "passed": result, "total": len(results)}

to test_crossover:
    set results to []
    set p1 to {"type": "ema_crossover", "params": {"fast_period": 10, "slow_period": 30}, "fitness": 40, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}
    set p2 to {"type": "ema_crossover", "params": {"fast_period": 8, "slow_period": 25}, "fitness": 60, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}
    set p1f to p1.fitness
    run crossover_strategies with p1, p2
    set ch to result
    run assert_equal with ch.fitness, 0, "Child fitness=0"
    append result to results
    run assert_equal with ch.type, "ema_crossover", "Type kept"
    append result to results
    run assert_equal with p1.fitness, p1f, "Parent safe"
    append result to results
    set a to {"type": "rsi_reversal", "params": {"oversold": 30}, "fitness": 20, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}
    set b to {"type": "breakout", "params": {"lookback": 20}, "fitness": 80, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0}
    run crossover_strategies with a, b
    run assert_equal with result.type, "breakout", "Fitter wins"
    append result to results
    run count_passed with results
    respond with {"suite": "Crossover", "tests": results, "passed": result, "total": len(results)}

// =================================================================
// RUN ALL
// =================================================================

to run_all_tests:
    log "=== NEURALEDGE TESTS ==="
    set suites to []
    set tp to 0
    set tt to 0
    run test_rsi
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "RSI:        {{result.passed}}/{{result.total}}"
    run test_macd
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "MACD:       {{result.passed}}/{{result.total}}"
    run test_sma_ema
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "SMA/EMA:    {{result.passed}}/{{result.total}}"
    run test_bollinger
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Bollinger:  {{result.passed}}/{{result.total}}"
    run test_atr
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "ATR:        {{result.passed}}/{{result.total}}"
    run test_stochastic
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Stochastic: {{result.passed}}/{{result.total}}"
    run test_adx
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "ADX:        {{result.passed}}/{{result.total}}"
    run test_sector_signals
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Sectors:    {{result.passed}}/{{result.total}}"
    run test_aggregation
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Signals:    {{result.passed}}/{{result.total}}"
    run test_risk
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Risk:       {{result.passed}}/{{result.total}}"
    run test_mutation
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Mutation:   {{result.passed}}/{{result.total}}"
    run test_crossover
    append result to suites
    set tp to tp + result.passed
    set tt to tt + result.total
    log "Crossover:  {{result.passed}}/{{result.total}}"
    set ok to tp is equal tt
    log "=== TOTAL: {{tp}}/{{tt}} ==="
    respond with {"status": ok, "total_passed": tp, "total_tests": tt, "pass_rate_pct": round((tp / tt) * 100, 1), "suites": suites}

api:
    GET /health runs test_health
    GET /tests/all runs run_all_tests
    GET /tests/rsi runs test_rsi
    GET /tests/macd runs test_macd
    GET /tests/sma-ema runs test_sma_ema
    GET /tests/bollinger runs test_bollinger
    GET /tests/atr runs test_atr
    GET /tests/stochastic runs test_stochastic
    GET /tests/adx runs test_adx
    GET /tests/sector-signals runs test_sector_signals
    GET /tests/aggregation runs test_aggregation
    GET /tests/risk runs test_risk
    GET /tests/mutation runs test_mutation
    GET /tests/crossover runs test_crossover

to test_health:
    respond with {"service": "neuraledge-tests", "status": "healthy"}
