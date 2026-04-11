service "neuraledge-digital-twin"
version "1.0.0"
description "Market Digital Twin: virtual market simulation where AI agents experiment before real signals"

configure:
    port: 8005
    data_service is "http://localhost:8001"
    analyzer_service is "http://localhost:8002"
    simulation_steps: 500
    agent_count: 100
    monte_carlo_runs: 30

// ═══════════════════════════════════════════════════════════════
// MARKET STATE BUILDER
// ═══════════════════════════════════════════════════════════════

to build_market_state with candles:
    purpose: "Build structured market state from raw candle data"
    set n to len(candles)
    if n is below 20:
        respond with {"error": "Insufficient data"}
    set closes to []
    set volumes to []
    set highs to []
    set lows to []
    repeat for each c in candles:
        append c.close to closes
        append c.volume to volumes
        append c.high to highs
        append c.low to lows
    // Price trend: compare recent vs older average
    set recent_20 to slice(closes, n - 20, n)
    set older_20 to slice(closes, max(0, n - 40), n - 20)
    set recent_avg to average(recent_20)
    set older_avg to average(older_20)
    set trend_pct to (recent_avg - older_avg) / older_avg * 100
    set trend to "sideways"
    if trend_pct is above 2:
        set trend to "uptrend"
    if trend_pct is below -2:
        set trend to "downtrend"
    // Volatility
    set returns to []
    repeat for each i in range(1, n):
        append (closes[i] - closes[i - 1]) / closes[i - 1] to returns
    set recent_returns to slice(returns, len(returns) - 20, len(returns))
    set mean_ret to average(recent_returns)
    set sum_sq to 0
    repeat for each r in recent_returns:
        set diff to r - mean_ret
        set sum_sq to sum_sq + (diff * diff)
    set daily_vol to sqrt(sum_sq / len(recent_returns))
    set annualized_vol to daily_vol * sqrt(252) * 100
    // Volume trend
    set recent_vol to average(slice(volumes, n - 10, n))
    set older_vol to average(slice(volumes, max(0, n - 30), n - 10))
    set vol_trend to "stable"
    if recent_vol is above older_vol * 1.3:
        set vol_trend to "increasing"
    if recent_vol is below older_vol * 0.7:
        set vol_trend to "decreasing"
    // Momentum (rate of change 10-day)
    set momentum to (closes[n - 1] - closes[max(0, n - 11)]) / closes[max(0, n - 11)] * 100
    // Mean reversion pressure
    set sma_50 to average(slice(closes, max(0, n - 50), n))
    set deviation_from_mean to (closes[n - 1] - sma_50) / sma_50 * 100
    respond with {
        "current_price": closes[n - 1],
        "trend": trend,
        "trend_strength": round(abs(trend_pct), 2),
        "volatility": round(annualized_vol, 2),
        "daily_volatility": round(daily_vol, 4),
        "momentum": round(momentum, 2),
        "volume_trend": vol_trend,
        "deviation_from_mean": round(deviation_from_mean, 2),
        "mean_reversion_pressure": abs(deviation_from_mean) is above 5,
        "recent_high": max(recent_20),
        "recent_low": min(recent_20),
        "avg_volume": round(recent_vol),
        "price_history": closes,
        "volume_history": volumes,
        "returns": returns
    }

// ═══════════════════════════════════════════════════════════════
// PRICE SIMULATION ENGINE
// ═══════════════════════════════════════════════════════════════

to simulate_price_path with market_state, steps:
    purpose: "Simulate a realistic price path using geometric Brownian motion with regime awareness"
    set price to market_state.current_price
    set daily_vol to market_state.daily_volatility
    set path to [price]
    // Adjust drift based on market trend
    set drift to 0
    if market_state.trend is equal "uptrend":
        set drift to 0.0003
    if market_state.trend is equal "downtrend":
        set drift to -0.0003
    // Mean reversion component
    set mean_price to market_state.current_price / (1 + market_state.deviation_from_mean / 100)
    set mean_reversion_strength to 0.02
    repeat for each step in range(0, steps):
        // Random walk with drift, volatility, and mean reversion
        set random_shock to (random() - 0.5) * 2 * daily_vol
        set reversion to (mean_price - price) / price * mean_reversion_strength
        // Occasional volatility spikes (5% chance)
        set vol_multiplier to 1.0
        if random() is below 0.05:
            set vol_multiplier to 2.0 + random() * 2.0
        set daily_return to drift + random_shock * vol_multiplier + reversion
        set price to price * (1 + daily_return)
        // Price floor: can't go below 1% of original
        if price is below market_state.current_price * 0.5:
            set price to market_state.current_price * 0.5
        append round(price, 2) to path
    respond with path

to simulate_multiple_paths with market_state, steps, num_paths:
    purpose: "Monte Carlo simulation: generate multiple price scenarios"
    set paths to []
    set final_prices to []
    repeat for each p in range(0, num_paths):
        run simulate_price_path with market_state, steps
        set simulated_path to result
        append simulated_path to paths
        append simulated_path[len(simulated_path) - 1] to final_prices
    set avg_final to average(final_prices)
    set min_final to min(final_prices)
    set max_final to max(final_prices)
    // Probability of price increase
    set up_count to 0
    repeat for each fp in final_prices:
        if fp is above market_state.current_price:
            set up_count to up_count + 1
    set prob_up to (up_count / num_paths) * 100
    // Expected move
    set expected_return to (avg_final - market_state.current_price) / market_state.current_price * 100
    // Value at Risk (5th percentile)
    set sorted_finals to sort(final_prices)
    set var_index to round(num_paths * 0.05)
    if var_index is below 0:
        set var_index to 0
    set var_5pct to sorted_finals[var_index]
    set var_pct to (market_state.current_price - var_5pct) / market_state.current_price * 100
    respond with {
        "num_paths": num_paths,
        "steps": steps,
        "current_price": market_state.current_price,
        "avg_final_price": round(avg_final, 2),
        "min_final_price": round(min_final, 2),
        "max_final_price": round(max_final, 2),
        "probability_up": round(prob_up, 2),
        "expected_return_pct": round(expected_return, 2),
        "value_at_risk_5pct": round(var_pct, 2),
        "paths": paths
    }

// ═══════════════════════════════════════════════════════════════
// DIGITAL TWIN AGENTS
// ═══════════════════════════════════════════════════════════════

to create_twin_agent with agent_id:
    purpose: "Create a trading agent for the digital twin"
    set strategies to ["rsi_reversal", "ema_crossover", "breakout", "mean_reversion", "momentum"]
    set chosen to strategies[round(random() * (len(strategies) - 1))]
    set agent to {
        "id": agent_id,
        "strategy": chosen,
        "capital": 100000,
        "position": 0,
        "entry_price": 0,
        "trades": [],
        "total_pnl": 0,
        "wins": 0,
        "losses": 0
    }
    // Strategy-specific parameters with randomization
    match chosen:
        when "rsi_reversal":
            set agent.params to {"oversold": 25 + round(random() * 15), "overbought": 65 + round(random() * 20)}
        when "ema_crossover":
            set agent.params to {"fast": 5 + round(random() * 10), "slow": 20 + round(random() * 20)}
        when "breakout":
            set agent.params to {"lookback": 10 + round(random() * 20)}
        when "mean_reversion":
            set agent.params to {"sma_period": 20 + round(random() * 20), "deviation": 1.5 + random()}
        otherwise:
            set agent.params to {"momentum_period": 10 + round(random() * 10), "threshold": 2 + random() * 3}
    respond with agent

to agent_decision with agent, price, prev_price, market_state:
    purpose: "Agent makes trading decision based on current state"
    set decision to "hold"
    match agent.strategy:
        when "rsi_reversal":
            set change_pct to (price - prev_price) / prev_price * 100
            // Simplified RSI proxy using price momentum
            if change_pct is below -1 * agent.params.oversold / 30:
                set decision to "buy"
            if change_pct is above agent.params.overbought / 30:
                set decision to "sell"
        when "ema_crossover":
            set fast_weight to 2.0 / (agent.params.fast + 1)
            if market_state.momentum is above 1:
                set decision to "buy"
            if market_state.momentum is below -1:
                set decision to "sell"
        when "breakout":
            if price is above market_state.recent_high:
                set decision to "buy"
            if price is below market_state.recent_low:
                set decision to "sell"
        when "mean_reversion":
            set dev to (price - market_state.current_price) / market_state.current_price * 100
            if dev is below (0 - agent.params.deviation):
                set decision to "buy"
            if dev is above agent.params.deviation:
                set decision to "sell"
        otherwise:
            if market_state.trend is equal "uptrend" and market_state.momentum is above 2:
                set decision to "buy"
            if market_state.trend is equal "downtrend" and market_state.momentum is below -2:
                set decision to "sell"
    respond with decision

to execute_agent_trade with agent, price, decision:
    purpose: "Execute trade for agent in simulation"
    if decision is equal "buy" and agent.position is equal 0:
        set agent.position to agent.capital / price
        set agent.entry_price to price
    if decision is equal "sell" and agent.position is above 0:
        set exit_value to agent.position * price
        set pnl to exit_value - (agent.position * agent.entry_price)
        set agent.total_pnl to agent.total_pnl + pnl
        set agent.capital to agent.capital + pnl
        if pnl is above 0:
            set agent.wins to agent.wins + 1
        otherwise:
            set agent.losses to agent.losses + 1
        append {"entry": agent.entry_price, "exit": price, "pnl": round(pnl)} to agent.trades
        set agent.position to 0
        set agent.entry_price to 0
    respond with agent

// ═══════════════════════════════════════════════════════════════
// FULL DIGITAL TWIN SIMULATION
// ═══════════════════════════════════════════════════════════════

to run_simulation with symbol, num_agents, sim_steps:
    purpose: "Run complete digital twin simulation"
    log "Starting Digital Twin simulation for {{symbol}} with {{num_agents}} agents over {{sim_steps}} steps"
    // Fetch real market data
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    if stock_data.error is not equal nothing:
        respond with {"error": "Cannot fetch data for {{symbol}}"}
    // Build market state
    run build_market_state with stock_data.candles
    set market_state to result
    // Generate simulated price path
    run simulate_price_path with market_state, sim_steps
    set sim_prices to result
    // Create agents
    set agents to []
    repeat for each i in range(0, num_agents):
        run create_twin_agent with i
        append result to agents
    // Run simulation
    repeat for each step in range(1, len(sim_prices)):
        set current_price to sim_prices[step]
        set prev_price to sim_prices[step - 1]
        set new_agents to []
        repeat for each agent in agents:
            run agent_decision with agent, current_price, prev_price, market_state
            set decision to result
            run execute_agent_trade with agent, current_price, decision
            append result to new_agents
        set agents to new_agents
    // Close all open positions at final price
    set final_price to sim_prices[len(sim_prices) - 1]
    set final_agents to []
    repeat for each agent in agents:
        if agent.position is above 0:
            run execute_agent_trade with agent, final_price, "sell"
            set agent to result
        append agent to final_agents
    set agents to final_agents
    // Rank agents by performance
    set agent_results to []
    repeat for each agent in agents:
        set total_trades to agent.wins + agent.losses
        set win_rate to 0
        if total_trades is above 0:
            set win_rate to (agent.wins / total_trades) * 100
        set return_pct to (agent.total_pnl / 100000) * 100
        append {
            "id": agent.id,
            "strategy": agent.strategy,
            "params": agent.params,
            "total_pnl": round(agent.total_pnl),
            "return_pct": round(return_pct, 2),
            "wins": agent.wins,
            "losses": agent.losses,
            "win_rate": round(win_rate, 2),
            "total_trades": total_trades,
            "final_capital": round(agent.capital)
        } to agent_results
    set best_agent to max_by(agent_results, "return_pct")
    set best_return to best_agent.return_pct
    set profitable to filter_by(agent_results, "return_pct", "above", 0)
    set top_agents to filter_by(profitable, "win_rate", "above", 50)
    // Strategy performance aggregation
    set strategy_perf to {}
    repeat for each ar in agent_results:
        set stype to ar.strategy
        if has_key(strategy_perf, stype) is equal no:
            set strategy_perf[stype] to {"count": 0, "total_return": 0, "profitable": 0}
        set sp to strategy_perf[stype]
        set sp.count to sp.count + 1
        set sp.total_return to sp.total_return + ar.return_pct
        if ar.return_pct is above 0:
            set sp.profitable to sp.profitable + 1
        set strategy_perf[stype] to sp
    // Compute averages
    set strategy_summary to {}
    repeat for each stype in keys(strategy_perf):
        set perf to strategy_perf[stype]
        set avg_return to perf.total_return / perf.count
        set profit_rate to (perf.profitable / perf.count) * 100
        set strategy_summary[stype] to {
            "agent_count": perf.count,
            "avg_return_pct": round(avg_return, 2),
            "profitable_pct": round(profit_rate, 2)
        }
    log "Simulation complete. Best agent: {{best_agent.strategy}} with {{best_return}}% return"
    respond with {
        "symbol": symbol,
        "simulation_steps": sim_steps,
        "num_agents": num_agents,
        "market_state": {
            "trend": market_state.trend,
            "volatility": market_state.volatility,
            "momentum": market_state.momentum
        },
        "price_simulation": {
            "start_price": sim_prices[0],
            "end_price": sim_prices[len(sim_prices) - 1],
            "sim_return_pct": round((sim_prices[len(sim_prices) - 1] - sim_prices[0]) / sim_prices[0] * 100, 2)
        },
        "best_agent": best_agent,
        "top_performers": top_agents,
        "strategy_performance": strategy_summary,
        "total_profitable_agents": len(top_agents),
        "agent_results": agent_results,
        "timestamp": time_now()
    }

// ═══════════════════════════════════════════════════════════════
// VALIDATED SIGNAL GENERATION
// ═══════════════════════════════════════════════════════════════

to generate_validated_signal with symbol:
    purpose: "Generate a trading signal validated through digital twin simulation"
    // Run Monte Carlo simulation
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    run build_market_state with stock_data.candles
    set market_state to result
    // Monte Carlo for probability estimation
    run simulate_multiple_paths with market_state, 30, 20
    set monte_carlo to result
    // Run agent simulation
    run run_simulation with symbol, 50, 100
    set sim_result to result
    // Derive validated signal
    set signal to "HOLD"
    set confidence to 40.0
    // Monte Carlo probability drives direction
    if monte_carlo.probability_up is above 60:
        set signal to "BUY"
        set confidence to monte_carlo.probability_up * 0.4
    if monte_carlo.probability_up is below 40:
        set signal to "SELL"
        set confidence to (100 - monte_carlo.probability_up) * 0.4
    // Agent consensus boosts confidence
    set profitable_pct to 0
    if sim_result.num_agents is above 0:
        set profitable_pct to (sim_result.total_profitable_agents / sim_result.num_agents) * 100
    if profitable_pct is above 60:
        set confidence to confidence + 20
    if profitable_pct is below 30:
        set confidence to confidence - 10
    // Best agent's strategy becomes recommendation
    set recommended_strategy to "none"
    if sim_result.best_agent is not equal nothing:
        set recommended_strategy to sim_result.best_agent.strategy
    if confidence is above 90:
        set confidence to 90.0
    if confidence is below 10:
        set confidence to 10.0
    respond with {
        "symbol": symbol,
        "signal": signal,
        "confidence": round(confidence, 2),
        "validated_through": "digital_twin_simulation",
        "monte_carlo": {
            "probability_up": monte_carlo.probability_up,
            "expected_return": monte_carlo.expected_return_pct,
            "value_at_risk": monte_carlo.value_at_risk_5pct,
            "scenarios_run": monte_carlo.num_paths
        },
        "simulation": {
            "agents_tested": sim_result.num_agents,
            "profitable_agents_pct": round(profitable_pct, 2),
            "best_strategy": recommended_strategy,
            "best_return_pct": sim_result.best_agent.return_pct,
            "strategy_rankings": sim_result.strategy_performance
        },
        "market_context": {
            "trend": market_state.trend,
            "volatility": market_state.volatility,
            "momentum": market_state.momentum
        },
        "timestamp": time_now()
    }

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /simulate runs run_simulation
    POST /monte-carlo runs api_monte_carlo
    POST /market-state runs api_market_state
    POST /validated-signal runs generate_validated_signal

to health_check:
    respond with {"service": "digital-twin", "status": "healthy", "version": "1.0.0"}

to api_monte_carlo with symbol, steps, num_paths:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    run build_market_state with stock_data.candles
    run simulate_multiple_paths with result, steps, num_paths
    respond with {"symbol": symbol, "monte_carlo": result}

to api_market_state with symbol:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "6mo", "interval": "1d"}
        content_type: "application/json"
    run build_market_state with stock_data.candles
    respond with {"symbol": symbol, "market_state": result}
