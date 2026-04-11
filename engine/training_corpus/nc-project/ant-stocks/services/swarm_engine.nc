service "neuraledge-swarm-engine"
version "1.0.0"
description "Swarm intelligence: evolutionary strategy generation and testing"

configure:
    port: 8003
    data_service is "http://localhost:8001"
    analyzer_service is "http://localhost:8002"
    population_size: 200
    mutation_rate: 0.15
    crossover_rate: 0.7
    generations: 50
    elite_pct: 0.1

// ═══════════════════════════════════════════════════════════════
// STRATEGY DEFINITIONS
// ═══════════════════════════════════════════════════════════════

to create_random_strategy:
    purpose: "Generate a random trading strategy gene"
    set strategy_types to ["rsi_reversal", "macd_crossover", "bollinger_bounce", "ema_crossover", "breakout", "mean_reversion", "momentum", "volume_breakout"]
    set chosen to strategy_types[round(random() * (len(strategy_types) - 1))]

    match chosen:
        when "rsi_reversal":
            respond with {
                "type": "rsi_reversal",
                "params": {
                    "oversold": 20 + round(random() * 20),
                    "overbought": 65 + round(random() * 25),
                    "period": 10 + round(random() * 10)
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "macd_crossover":
            respond with {
                "type": "macd_crossover",
                "params": {
                    "fast": 8 + round(random() * 8),
                    "slow": 20 + round(random() * 12),
                    "signal_period": 7 + round(random() * 4)
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "bollinger_bounce":
            respond with {
                "type": "bollinger_bounce",
                "params": {
                    "period": 15 + round(random() * 15),
                    "std_mult": 1.5 + random() * 1.5,
                    "exit_at_middle": random() is above 0.5
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "ema_crossover":
            respond with {
                "type": "ema_crossover",
                "params": {
                    "fast_period": 5 + round(random() * 15),
                    "slow_period": 20 + round(random() * 30)
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "breakout":
            respond with {
                "type": "breakout",
                "params": {
                    "lookback": 10 + round(random() * 30),
                    "volume_confirm": random() is above 0.4,
                    "atr_multiplier": 1.0 + random() * 2.0
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "mean_reversion":
            respond with {
                "type": "mean_reversion",
                "params": {
                    "sma_period": 15 + round(random() * 35),
                    "entry_deviation": 1.5 + random() * 2.0,
                    "exit_deviation": 0.2 + random() * 0.8
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        when "momentum":
            respond with {
                "type": "momentum",
                "params": {
                    "rsi_threshold": 50 + round(random() * 20),
                    "adx_threshold": 20 + round(random() * 15),
                    "ema_period": 10 + round(random() * 20)
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }
        otherwise:
            respond with {
                "type": "volume_breakout",
                "params": {
                    "volume_mult": 1.5 + random() * 2.0,
                    "price_change_pct": 0.5 + random() * 2.0,
                    "hold_periods": 2 + round(random() * 8)
                },
                "fitness": 0, "sharpe": 0, "win_rate": 0, "max_dd": 0, "trades": 0
            }

// ═══════════════════════════════════════════════════════════════
// STRATEGY SIGNAL GENERATION
// ═══════════════════════════════════════════════════════════════

to generate_strategy_signals with strategy, closes, highs, lows, volumes:
    purpose: "Generate buy/sell signals from strategy on price data"
    set signals to []
    set n to len(closes)

    match strategy.type:
        when "rsi_reversal":
            set period to strategy.params.period
            set oversold to strategy.params.oversold
            set overbought to strategy.params.overbought
            repeat for each i in range(period + 1, n):
                set window to slice(closes, i - period, i + 1)
                // Simplified RSI inline for speed
                set gains to 0
                set loss_sum to 0
                repeat for each j in range(1, len(window)):
                    set chg to window[j] - window[j - 1]
                    if chg is above 0:
                        set gains to gains + chg
                    otherwise:
                        set loss_sum to loss_sum + abs(chg)
                set avg_g to gains / period
                set avg_l to loss_sum / period
                set rsi to 50
                if avg_l is above 0:
                    set rs to avg_g / avg_l
                    set rsi to 100 - (100 / (1 + rs))
                if rsi is below oversold:
                    append {"idx": i, "signal": 1} to signals
                if rsi is above overbought:
                    append {"idx": i, "signal": -1} to signals

        when "ema_crossover":
            set fast_p to strategy.params.fast_period
            set slow_p to strategy.params.slow_period
            set fast_mult to 2.0 / (fast_p + 1)
            set slow_mult to 2.0 / (slow_p + 1)
            set fast_ema to closes[0]
            set slow_ema to closes[0]
            set prev_above to no
            repeat for each i in range(1, n):
                set fast_ema to (closes[i] - fast_ema) * fast_mult + fast_ema
                set slow_ema to (closes[i] - slow_ema) * slow_mult + slow_ema
                set currently_above to fast_ema is above slow_ema
                if currently_above is equal yes and prev_above is equal no:
                    append {"idx": i, "signal": 1} to signals
                if currently_above is equal no and prev_above is equal yes:
                    append {"idx": i, "signal": -1} to signals
                set prev_above to currently_above

        when "breakout":
            set lb to strategy.params.lookback
            repeat for each i in range(lb, n):
                set range_high to max(slice(highs, i - lb, i))
                set range_low to min(slice(lows, i - lb, i))
                if closes[i] is above range_high:
                    append {"idx": i, "signal": 1} to signals
                if closes[i] is below range_low:
                    append {"idx": i, "signal": -1} to signals

        when "mean_reversion":
            set sma_p to strategy.params.sma_period
            set entry_dev to strategy.params.entry_deviation
            set exit_dev to strategy.params.exit_deviation
            repeat for each i in range(sma_p, n):
                set window to slice(closes, i - sma_p, i)
                set sma to average(window)
                set deviation_pct to (closes[i] - sma) / sma * 100
                if deviation_pct is below (0 - entry_dev):
                    append {"idx": i, "signal": 1} to signals
                if deviation_pct is above entry_dev:
                    append {"idx": i, "signal": -1} to signals

        otherwise:
            // Momentum and volume strategies: default trend-following
            repeat for each i in range(20, n):
                set recent to slice(closes, i - 10, i + 1)
                set older to slice(closes, i - 20, i - 10)
                if average(recent) is above average(older) * 1.01:
                    append {"idx": i, "signal": 1} to signals
                if average(recent) is below average(older) * 0.99:
                    append {"idx": i, "signal": -1} to signals

    respond with signals

// ═══════════════════════════════════════════════════════════════
// BACKTESTING
// ═══════════════════════════════════════════════════════════════

to backtest_strategy with strategy, closes, highs, lows, volumes:
    purpose: "Simulate trades and compute performance metrics"
    run generate_strategy_signals with strategy, closes, highs, lows, volumes
    set signals to result

    set capital to 100000
    set position to 0
    set entry_price to 0
    set trades to []
    set equity_curve to [capital]
    set peak_equity to capital

    repeat for each sig in signals:
        set price to closes[sig.idx]
        if sig.signal is equal 1 and position is equal 0:
            set position to capital / price
            set entry_price to price
        if sig.signal is equal -1 and position is above 0:
            set exit_value to position * price
            set pnl to exit_value - (position * entry_price)
            set pnl_pct to (price - entry_price) / entry_price * 100
            append {"entry": entry_price, "exit": price, "pnl": pnl, "pnl_pct": round(pnl_pct, 2)} to trades
            set capital to capital + pnl
            set position to 0
            set entry_price to 0
            append capital to equity_curve
            if capital is above peak_equity:
                set peak_equity to capital

    // Close any open position at last price
    if position is above 0:
        set last_price to closes[len(closes) - 1]
        set exit_value to position * last_price
        set pnl to exit_value - (position * entry_price)
        set capital to capital + pnl
        append capital to equity_curve

    set total_trades to len(trades)
    set total_pnl to sum_by(trades, "pnl")
    set winning_trades to filter_by(trades, "pnl", "above", 0)
    set losing_trades to filter_by(trades, "pnl", "at_most", 0)
    set gross_profit to sum_by(winning_trades, "pnl")
    set gross_loss to 0
    repeat for each lt in losing_trades:
        set gross_loss to gross_loss + abs(lt.pnl)
    set win_rate to 0
    if total_trades is above 0:
        set win_rate to (len(winning_trades) / total_trades) * 100
    set profit_factor to 0
    if gross_loss is above 0:
        set profit_factor to gross_profit / gross_loss

    // Max drawdown
    set max_dd to 0
    set running_peak to equity_curve[0]
    repeat for each eq in equity_curve:
        if eq is above running_peak:
            set running_peak to eq
        set dd to (running_peak - eq) / running_peak * 100
        if dd is above max_dd:
            set max_dd to dd

    // Simplified Sharpe: annualized return / volatility proxy
    set total_return to (capital - 100000) / 100000
    set sharpe to 0
    if max_dd is above 0:
        set sharpe to total_return / (max_dd / 100)

    // Fitness = composite score
    set fitness to (win_rate * 0.3) + (sharpe * 20) + (profit_factor * 10) - (max_dd * 0.5)
    if total_trades is below 5:
        set fitness to fitness * 0.3

    respond with {
        "total_trades": total_trades,
        "win_rate": round(win_rate, 2),
        "total_return_pct": round(total_return * 100, 2),
        "sharpe": round(sharpe, 2),
        "max_drawdown": round(max_dd, 2),
        "profit_factor": round(profit_factor, 2),
        "final_capital": round(capital),
        "fitness": round(fitness, 2)
    }

// ═══════════════════════════════════════════════════════════════
// MUTATION & CROSSOVER
// ═══════════════════════════════════════════════════════════════

to mutate_strategy with strategy:
    purpose: "Mutate strategy parameters"
    set mutated to strategy
    set params to strategy.params

    match strategy.type:
        when "rsi_reversal":
            if random() is below 0.5:
                set params.oversold to max(10, params.oversold + round((random() - 0.5) * 10))
            otherwise:
                set params.overbought to min(95, params.overbought + round((random() - 0.5) * 10))
            set params.period to max(5, params.period + round((random() - 0.5) * 4))

        when "ema_crossover":
            set params.fast_period to max(3, params.fast_period + round((random() - 0.5) * 6))
            set params.slow_period to max(params.fast_period + 5, params.slow_period + round((random() - 0.5) * 10))

        when "breakout":
            set params.lookback to max(5, params.lookback + round((random() - 0.5) * 10))

        when "mean_reversion":
            set params.sma_period to max(10, params.sma_period + round((random() - 0.5) * 10))
            set params.entry_deviation to max(0.5, params.entry_deviation + (random() - 0.5) * 0.5)

        otherwise:
            // Generic mutation: randomly adjust numeric params
            log "Generic mutation for {{strategy.type}}"

    set mutated.params to params
    set mutated.fitness to 0
    respond with mutated

to crossover_strategies with parent1, parent2:
    purpose: "Combine two parent strategies"
    // If same type, blend parameters; otherwise pick the fitter parent type
    if parent1.type is equal parent2.type:
        set child to parent1
        set child.fitness to 0
        // Blend: pick each param from either parent randomly
        repeat for each key in keys(parent1.params):
            if random() is below 0.5:
                if has_key(parent2.params, key):
                    set child.params[key] to parent2.params[key]
        respond with child
    otherwise:
        // Pick the fitter parent's type
        if parent1.fitness is above parent2.fitness:
            set child to parent1
        otherwise:
            set child to parent2
        set child.fitness to 0
        respond with child

// ═══════════════════════════════════════════════════════════════
// SWARM EVOLUTION ENGINE
// ═══════════════════════════════════════════════════════════════

to initialize_population with size:
    purpose: "Create initial random population"
    set population to []
    repeat for each i in range(0, size):
        run create_random_strategy
        append result to population
    log "Initialized population of {{size}} strategies"
    respond with population

to evaluate_population with population, closes, highs, lows, volumes:
    purpose: "Evaluate all strategies in population"
    set evaluated to []
    repeat for each strategy in population:
        run backtest_strategy with strategy, closes, highs, lows, volumes
        set metrics to result
        set strategy.fitness to metrics.fitness
        set strategy.sharpe to metrics.sharpe
        set strategy.win_rate to metrics.win_rate
        set strategy.max_dd to metrics.max_drawdown
        set strategy.trades to metrics.total_trades
        append strategy to evaluated
    respond with evaluated

to select_parents with population, tournament_size:
    purpose: "Tournament selection"
    set best to population[round(random() * (len(population) - 1))]
    repeat for each i in range(1, tournament_size):
        set candidate to population[round(random() * (len(population) - 1))]
        if candidate.fitness is above best.fitness:
            set best to candidate
    respond with best

to evolve_one_generation with population, mutation_rate, crossover_rate:
    purpose: "Create next generation via selection, crossover, and mutation"
    set new_pop to []
    set pop_size to len(population)

    // Elitism: keep top 10%
    set elite_count to max(2, round(pop_size * 0.1))
    set sorted_pop to sort_by(population, "fitness")
    set elite to slice(sorted_pop, len(sorted_pop) - elite_count, len(sorted_pop))
    repeat for each e in elite:
        append e to new_pop

    // Fill rest with offspring
    while len(new_pop) is below pop_size:
        run select_parents with population, 5
        set parent1 to result
        run select_parents with population, 5
        set parent2 to result

        if random() is below crossover_rate:
            run crossover_strategies with parent1, parent2
            set child to result
        otherwise:
            set child to parent1
            set child.fitness to 0

        if random() is below mutation_rate:
            run mutate_strategy with child
            set child to result

        append child to new_pop

    respond with new_pop

to run_evolution with symbol, generations, population_size:
    purpose: "Run full evolutionary cycle for a stock"
    log "Starting evolution for {{symbol}}: {{generations}} generations, {{population_size}} agents"

    // Fetch historical data
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "1y", "interval": "1d"}
        content_type: "application/json"

    if stock_data.error is not equal nothing:
        respond with {"error": "Cannot fetch data for {{symbol}}"}

    set candles to stock_data.candles
    set closes to map_field(candles, "close")
    set highs to map_field(candles, "high")
    set lows to map_field(candles, "low")
    set volumes to map_field(candles, "volume")
    // Initialize
    run initialize_population with population_size
    set population to result

    set best_fitness to -999
    set best_strategy to nothing
    set generation_log to []

    // Evolution loop
    repeat for each gen in range(0, generations):
        run evaluate_population with population, closes, highs, lows, volumes
        set population to result

        set best_strategy to max_by(population, "fitness")
        set best_fitness to best_strategy.fitness
        set avg_fit to average(map_field(population, "fitness"))

        append {
            "generation": gen,
            "best_fitness": round(best_fitness, 2),
            "avg_fitness": round(avg_fit, 2),
            "best_type": best_strategy.type
        } to generation_log

        if gen is below generations - 1:
            run evolve_one_generation with population, 0.15, 0.7
            set population to result

    log "Evolution complete for {{symbol}}. Best fitness: {{best_fitness}}"

    set top_strategies to filter_by(population, "fitness", "above", 0)

    respond with {
        "symbol": symbol,
        "generations": generations,
        "population_size": population_size,
        "best_strategy": {
            "type": best_strategy.type,
            "params": best_strategy.params,
            "fitness": best_strategy.fitness,
            "sharpe": best_strategy.sharpe,
            "win_rate": best_strategy.win_rate,
            "max_drawdown": best_strategy.max_dd
        },
        "top_strategies": top_strategies,
        "evolution_log": generation_log
    }

// ─── Swarm Signal: aggregated signal from top strategies ───

to get_swarm_signal with symbol:
    purpose: "Get aggregated trading signal from evolved strategies"
    run run_evolution with symbol, 20, 50
    set evo_result to result

    if evo_result.error is not equal nothing:
        respond with {"error": evo_result.error}

    set best to evo_result.best_strategy
    set signal to "hold"
    set confidence to 40.0

    // Derive signal from best strategy characteristics
    if best.win_rate is above 55 and best.sharpe is above 0.5:
        set signal to "buy"
        set confidence to 50 + best.win_rate * 0.3 + best.sharpe * 10
    if best.win_rate is above 55 and best.sharpe is below -0.5:
        set signal to "sell"
        set confidence to 50 + best.win_rate * 0.3

    if confidence is above 90:
        set confidence to 90.0

    respond with {
        "symbol": symbol,
        "signal": signal,
        "confidence": round(confidence, 2),
        "strategy_type": best.type,
        "strategy_params": best.params,
        "win_rate": best.win_rate,
        "sharpe": best.sharpe,
        "fitness": best.fitness
    }

// ─── API Routes ───

api:
    GET /health runs health_check
    POST /evolve runs run_evolution
    POST /signal runs get_swarm_signal
    POST /backtest runs api_backtest

to health_check:
    respond with {"service": "swarm-engine", "status": "healthy", "version": "1.0.0"}

to api_backtest with symbol, strategy_type, params:
    gather stock_data from "{{config.data_service}}/stock/ohlcv":
        method: "POST"
        body: {"symbol": symbol, "period": "1y", "interval": "1d"}
        content_type: "application/json"
    set candles to stock_data.candles
    set closes to map_field(candles, "close")
    set highs to map_field(candles, "high")
    set lows to map_field(candles, "low")
    set volumes to map_field(candles, "volume")
    set strategy to {"type": strategy_type, "params": params, "fitness": 0}
    run backtest_strategy with strategy, closes, highs, lows, volumes
    respond with {"symbol": symbol, "strategy": strategy_type, "params": params, "results": result}
