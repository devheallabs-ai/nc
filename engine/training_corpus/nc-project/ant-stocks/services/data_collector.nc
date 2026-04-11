service "neuraledge-data-collector"
version "1.0.0"
description "Collects Indian stock market data from public sources (Yahoo Finance, NSE)"

configure:
    port: 8001
    data_dir is "data"

// ─── NIFTY 50 Symbols ───

to get_nifty50_symbols:
    set symbols to [
        "RELIANCE.NS", "TCS.NS", "HDFCBANK.NS", "INFY.NS", "ICICIBANK.NS",
        "HINDUNILVR.NS", "ITC.NS", "SBIN.NS", "BHARTIARTL.NS", "KOTAKBANK.NS",
        "LT.NS", "AXISBANK.NS", "ASIANPAINT.NS", "MARUTI.NS", "TITAN.NS",
        "SUNPHARMA.NS", "BAJFINANCE.NS", "WIPRO.NS", "ULTRACEMCO.NS", "NESTLEIND.NS",
        "HCLTECH.NS", "BAJAJFINSV.NS", "POWERGRID.NS", "NTPC.NS", "ONGC.NS",
        "TATAMOTORS.NS", "ADANIPORTS.NS", "COALINDIA.NS", "JSWSTEEL.NS",
        "TATASTEEL.NS", "TECHM.NS", "INDUSINDBK.NS", "HINDALCO.NS", "GRASIM.NS",
        "BRITANNIA.NS", "CIPLA.NS", "DRREDDY.NS", "DIVISLAB.NS", "EICHERMOT.NS",
        "APOLLOHOSP.NS", "SBILIFE.NS", "HDFCLIFE.NS", "TATACONSUM.NS", "BAJAJ-AUTO.NS",
        "BPCL.NS", "HEROMOTOCO.NS", "UPL.NS", "LTIM.NS", "ADANIENT.NS"
    ]
    respond with symbols

to get_index_symbols:
    respond with ["^NSEI", "^NSEBANK"]

to get_banknifty_symbols:
    set symbols to [
        "HDFCBANK.NS", "ICICIBANK.NS", "SBIN.NS", "KOTAKBANK.NS", "AXISBANK.NS",
        "INDUSINDBK.NS", "BANDHANBNK.NS", "FEDERALBNK.NS", "IDFCFIRSTB.NS",
        "PNB.NS", "BANKBARODA.NS", "AUBANK.NS"
    ]
    respond with symbols

to get_nifty_next50_symbols:
    set symbols to [
        "ABB.NS", "ADANIGREEN.NS", "ADANIPOWER.NS", "AMBUJACEM.NS", "ATGL.NS",
        "AUROPHARMA.NS", "BAJAJHLDNG.NS", "BANKBARODA.NS", "BEL.NS", "BERGEPAINT.NS",
        "BOSCHLTD.NS", "CANBK.NS", "CHOLAFIN.NS", "COLPAL.NS", "CONCOR.NS",
        "DLF.NS", "DABUR.NS", "DMART.NS", "GAIL.NS", "GODREJCP.NS",
        "HAVELLS.NS", "HINDPETRO.NS", "ICICIPRULI.NS", "ICICIGI.NS", "IDEA.NS",
        "INDHOTEL.NS", "IOC.NS", "IRCTC.NS", "JINDALSTEL.NS", "JIOFIN.NS",
        "LICI.NS", "LUPIN.NS", "MARICO.NS", "MAXHEALTH.NS", "MCDOWELL-N.NS",
        "MOTHERSON.NS", "NAUKRI.NS", "NHPC.NS", "NMDC.NS", "PAGEIND.NS",
        "PFC.NS", "PIDILITIND.NS", "PNB.NS", "RECLTD.NS", "SBICARD.NS",
        "SHREECEM.NS", "SIEMENS.NS", "TATAPOWER.NS", "TORNTPHARM.NS", "VBL.NS"
    ]
    respond with symbols

to get_top200_symbols:
    purpose: "All NIFTY 50 + Next 50 + additional large-caps for Top 200 coverage"
    run get_nifty50_symbols
    set n50 to result
    run get_nifty_next50_symbols
    set nn50 to result
    set extra to [
        "ZOMATO.NS", "PAYTM.NS", "POLYCAB.NS", "PERSISTENT.NS", "COFORGE.NS",
        "MPHASIS.NS", "TRENT.NS", "PIIND.NS", "ASTRAL.NS", "SUNDARMFIN.NS",
        "CUMMINSIND.NS", "VOLTAS.NS", "ESCORTS.NS", "PETRONET.NS", "MFSL.NS",
        "OBEROIRLTY.NS", "IPCALAB.NS", "APLAPOLLO.NS", "SAIL.NS", "NATIONALUM.NS",
        "ABCAPITAL.NS", "LICHSGFIN.NS", "MRF.NS", "ACC.NS", "GMRINFRA.NS",
        "BALKRISIND.NS", "BHARATFORG.NS", "CROMPTON.NS", "DEEPAKNTR.NS", "TVSMOTOR.NS",
        "BIOCON.NS", "UBL.NS", "GUJGASLTD.NS", "HONAUT.NS", "INDIGO.NS",
        "LTTS.NS", "MANAPPURAM.NS", "METROPOLIS.NS", "MUTHOOTFIN.NS", "OFSS.NS",
        "SYNGENE.NS", "TATACHEM.NS", "TATACOMM.NS", "TATAELXSI.NS", "TORNTPOWER.NS",
        "ALKEM.NS", "ATUL.NS", "CANFINHOME.NS", "COROMANDEL.NS", "EMAMILTD.NS",
        "GLENMARK.NS", "GRANULES.NS", "HDFCAMC.NS", "IIFL.NS", "IRFC.NS",
        "IGL.NS", "JUBLFOOD.NS", "L&TFH.NS", "LAURUSLABS.NS", "LTIM.NS",
        "NAVINFLUOR.NS", "RBLBANK.NS", "SUNTV.NS", "SUVENPHAR.NS", "TATAMTRDVR.NS",
        "ZEEL.NS", "ZYDUSLIFE.NS", "PRESTIGE.NS", "PHOENIXLTD.NS", "SONACOMS.NS",
        "KPITTECH.NS", "CGPOWER.NS", "POWERINDIA.NS", "SUPREMEIND.NS", "SUMICHEM.NS",
        "APOLLOTYRE.NS", "ASHOKLEY.NS", "AUBANK.NS", "BANDHANBNK.NS", "BATAINDIA.NS",
        "CUB.NS", "CRISIL.NS", "DALBHARAT.NS", "DIXON.NS", "FEDERALBNK.NS",
        "GICRE.NS", "GSPL.NS", "HINDCOPPER.NS", "KEI.NS", "KEC.NS",
        "LALPATHLAB.NS", "MCX.NS", "PVRINOX.NS", "RAMCOCEM.NS", "STARHEALTH.NS",
        "THERMAX.NS", "TIINDIA.NS", "VEDL.NS", "YESBANK.NS", "360ONE.NS"
    ]
    set all_symbols to []
    repeat for each s in n50:
        append s to all_symbols
    repeat for each s in nn50:
        append s to all_symbols
    repeat for each s in extra:
        append s to all_symbols
    respond with all_symbols

// ─── Sector Definitions ───

to get_sectors:
    respond with {
        "banking": ["HDFCBANK.NS", "ICICIBANK.NS", "SBIN.NS", "KOTAKBANK.NS", "AXISBANK.NS", "INDUSINDBK.NS", "PNB.NS", "BANKBARODA.NS", "FEDERALBNK.NS", "AUBANK.NS"],
        "it": ["TCS.NS", "INFY.NS", "WIPRO.NS", "HCLTECH.NS", "TECHM.NS", "LTIM.NS", "COFORGE.NS", "MPHASIS.NS", "PERSISTENT.NS", "KPITTECH.NS"],
        "pharma": ["SUNPHARMA.NS", "CIPLA.NS", "DRREDDY.NS", "DIVISLAB.NS", "APOLLOHOSP.NS", "LUPIN.NS", "AUROPHARMA.NS", "BIOCON.NS", "TORNTPHARM.NS", "ZYDUSLIFE.NS"],
        "auto": ["TATAMOTORS.NS", "MARUTI.NS", "BAJAJ-AUTO.NS", "EICHERMOT.NS", "HEROMOTOCO.NS", "TVSMOTOR.NS", "ASHOKLEY.NS", "ESCORTS.NS", "APOLLOTYRE.NS", "MOTHERSON.NS"],
        "fmcg": ["HINDUNILVR.NS", "ITC.NS", "NESTLEIND.NS", "BRITANNIA.NS", "TATACONSUM.NS", "DABUR.NS", "MARICO.NS", "COLPAL.NS", "GODREJCP.NS", "EMAMILTD.NS"],
        "energy": ["RELIANCE.NS", "ONGC.NS", "BPCL.NS", "NTPC.NS", "POWERGRID.NS", "COALINDIA.NS", "GAIL.NS", "IOC.NS", "ADANIGREEN.NS", "TATAPOWER.NS"],
        "metals": ["TATASTEEL.NS", "JSWSTEEL.NS", "HINDALCO.NS", "VEDL.NS", "SAIL.NS", "NATIONALUM.NS", "JINDALSTEL.NS", "NMDC.NS", "HINDCOPPER.NS", "COALINDIA.NS"],
        "realty": ["DLF.NS", "OBEROIRLTY.NS", "PRESTIGE.NS", "PHOENIXLTD.NS", "GODREJPROP.NS"],
        "finance": ["BAJFINANCE.NS", "BAJAJFINSV.NS", "SBILIFE.NS", "HDFCLIFE.NS", "ICICIPRULI.NS", "ICICIGI.NS", "CHOLAFIN.NS", "MUTHOOTFIN.NS", "MFSL.NS", "HDFCAMC.NS"],
        "infra": ["LT.NS", "ADANIPORTS.NS", "ULTRACEMCO.NS", "GRASIM.NS", "AMBUJACEM.NS", "ACC.NS", "SHREECEM.NS", "BHARTIARTL.NS", "CONCOR.NS", "IRCTC.NS"]
    }

// ─── Yahoo Finance Data Collection ───

to fetch_stock_ohlcv with symbol, period, interval:
    purpose: "Fetch OHLCV data from Yahoo Finance for a single symbol"
    try:
        gather response from "https://query2.finance.yahoo.com/v8/finance/chart/{{symbol}}?range={{period}}&interval={{interval}}&includePrePost=false":
            timeout: 60
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"
        if response.chart is equal nothing:
            respond with {"error": "No data for {{symbol}}"}
        set result_data to response.chart.result[0]
        set timestamps to result_data.timestamp
        set quotes to result_data.indicators.quote[0]
        set candles to []
        repeat for each i in range(0, len(timestamps)):
            set candle to {
                "timestamp": timestamps[i],
                "open": quotes.open[i],
                "high": quotes.high[i],
                "low": quotes.low[i],
                "close": quotes.close[i],
                "volume": quotes.volume[i]
            }
            append candle to candles
        set meta to result_data.meta
        respond with {
            "symbol": symbol,
            "currency": meta.currency,
            "exchange": meta.exchangeName,
            "candles": candles,
            "count": len(candles)
        }
    on error:
        log "Error fetching {{symbol}}: {{error}}"
        respond with {"error": "Failed to fetch {{symbol}}", "details": str(error)}

to fetch_bulk_ohlcv with symbols, period, interval:
    purpose: "Fetch OHLCV for multiple symbols"
    set results to {}
    repeat for each symbol in symbols:
        run fetch_stock_ohlcv with symbol, period, interval
        set results[symbol] to result
        wait 500 milliseconds
    respond with results

to fetch_nifty50_data with period, interval:
    purpose: "Fetch data for all NIFTY 50 stocks"
    run get_nifty50_symbols
    set symbols to result
    run fetch_bulk_ohlcv with symbols, period, interval
    respond with result

to fetch_index_data with period:
    purpose: "Fetch NIFTY and BANKNIFTY index data"
    run get_index_symbols
    set idx_symbols to result
    set results to {}
    repeat for each symbol in idx_symbols:
        run fetch_stock_ohlcv with symbol, period, "1d"
        set results[symbol] to result
    respond with results

// ─── Stock Fundamentals ───

to fetch_fundamentals with symbol:
    purpose: "Fetch company fundamentals from Yahoo Finance"
    try:
        gather response from "https://query2.finance.yahoo.com/v10/finance/quoteSummary/{{symbol}}?modules=summaryDetail,defaultKeyStatistics,financialData,assetProfile":
            timeout: 60
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"
        set summary to response.quoteSummary.result[0]
        set detail to summary.summaryDetail
        set stats to summary.defaultKeyStatistics
        set financial to summary.financialData
        respond with {
            "symbol": symbol,
            "pe_ratio": detail.trailingPE.raw,
            "forward_pe": detail.forwardPE.raw,
            "pb_ratio": stats.priceToBook.raw,
            "market_cap": detail.marketCap.raw,
            "dividend_yield": detail.dividendYield.raw,
            "beta": stats.beta.raw,
            "fifty_two_week_high": detail.fiftyTwoWeekHigh.raw,
            "fifty_two_week_low": detail.fiftyTwoWeekLow.raw,
            "revenue": financial.totalRevenue.raw,
            "profit_margin": financial.profitMargins.raw,
            "roe": financial.returnOnEquity.raw,
            "debt_to_equity": financial.debtToEquity.raw,
            "sector": summary.assetProfile.sector,
            "industry": summary.assetProfile.industry
        }
    on error:
        log "Error fetching fundamentals for {{symbol}}: {{error}}"
        respond with {"error": "Failed to fetch fundamentals", "symbol": symbol}

// ─── NSE Data Collection ───

to fetch_nse_option_chain with symbol:
    purpose: "Fetch option chain from NSE public endpoint"
    try:
        gather response from "https://www.nseindia.com/api/option-chain-equities?symbol={{symbol}}":
            warmup: "https://www.nseindia.com/"
            timeout: 90
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                Accept: "application/json"
                Accept-Language: "en-US,en;q=0.9"
                Referer: "https://www.nseindia.com/option-chain"
        set records to response.records
        set chain to []
        repeat for each item in records.data:
            set entry to {
                "strike_price": item.strikePrice,
                "expiry": item.expiryDate
            }
            if item.CE is not equal nothing:
                set entry.call_oi to item.CE.openInterest
                set entry.call_volume to item.CE.totalTradedVolume
                set entry.call_iv to item.CE.impliedVolatility
                set entry.call_ltp to item.CE.lastPrice
                set entry.call_change_oi to item.CE.changeinOpenInterest
            if item.PE is not equal nothing:
                set entry.put_oi to item.PE.openInterest
                set entry.put_volume to item.PE.totalTradedVolume
                set entry.put_iv to item.PE.impliedVolatility
                set entry.put_ltp to item.PE.lastPrice
                set entry.put_change_oi to item.PE.changeinOpenInterest
            append entry to chain
        respond with {
            "symbol": symbol,
            "spot_price": records.underlyingValue,
            "timestamp": records.timestamp,
            "chain": chain,
            "strikePrices": records.strikePrices
        }
    on error:
        log "NSE option chain error for {{symbol}}: {{error}}"
        respond with {"error": "Failed to fetch option chain", "symbol": symbol}

to fetch_nse_index_option_chain with index_name:
    purpose: "Fetch NIFTY/BANKNIFTY index option chain"
    try:
        gather response from "https://www.nseindia.com/api/option-chain-indices?symbol={{index_name}}":
            warmup: "https://www.nseindia.com/"
            timeout: 90
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                Accept: "application/json"
                Referer: "https://www.nseindia.com/option-chain"
        respond with response
    on error:
        respond with {"error": "Failed to fetch index option chain", "index": index_name}

// ─── News Collection ───

to fetch_market_news:
    purpose: "Fetch latest financial news headlines"
    try:
        gather response from "https://query2.finance.yahoo.com/v1/finance/trending/IN":
            timeout: 30
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"
        set trending to response.finance.result[0].quotes
        set news_items to []
        repeat for each item in trending:
            append {"symbol": item.symbol, "name": item.shortName, "exchange": item.exchange} to news_items
        respond with {"trending": news_items, "count": len(news_items)}
    on error:
        respond with {"error": "Failed to fetch news", "details": str(error)}

// ─── Data Persistence ───

to save_market_data with symbol, data:
    purpose: "Save market data to local file"
    set filename to "data/{{symbol}}_ohlcv.json"
    set payload to json_encode(data)
    write_file(filename, payload)
    log "Saved data for {{symbol}}"
    respond with {"saved": symbol, "file": filename}

to load_market_data with symbol:
    purpose: "Load saved market data"
    set filename to "data/{{symbol}}_ohlcv.json"
    try:
        set content to read_file(filename)
        set data to json_decode(content)
        respond with data
    on error:
        respond with {"error": "No saved data for {{symbol}}"}

to collect_all_data:
    purpose: "Full data collection run for all NIFTY 50 stocks"
    log "Starting full data collection..."
    run get_nifty50_symbols
    set symbols to result
    set collected to 0
    set errors to 0
    repeat for each symbol in symbols:
        run fetch_stock_ohlcv with symbol, "6mo", "1d"
        if result.error is equal nothing:
            run save_market_data with symbol, result
            set collected to collected + 1
        otherwise:
            set errors to errors + 1
        wait 1 seconds
    log "Collection complete: {{collected}} succeeded, {{errors}} failed"
    respond with {"collected": collected, "errors": errors, "total": len(symbols)}

// ─── Sector Analysis ───

to analyze_sector with sector_name:
    purpose: "Analyze performance of a market sector"
    run get_sectors
    set sectors to result
    if has_key(sectors, sector_name) is equal no:
        respond with {"error": "Unknown sector: {{sector_name}}", "available": ["banking", "it", "pharma", "auto", "fmcg", "energy", "metals", "realty", "finance", "infra"]}
    set sector_symbols to sectors[sector_name]
    set performances to []
    set total_change to 0
    set count to 0
    repeat for each symbol in sector_symbols:
        try:
            run fetch_stock_ohlcv with symbol, "1mo", "1d"
            set data to result
            if data.error is equal nothing and len(data.candles) is above 1:
                set first_close to data.candles[0].close
                set last_close to data.candles[len(data.candles) - 1].close
                set change_pct to ((last_close - first_close) / first_close) * 100
                set total_change to total_change + change_pct
                set count to count + 1
                append {"symbol": symbol, "price": last_close, "change_1m_pct": round(change_pct, 2)} to performances
        on error:
            log "Failed to fetch {{symbol}} for sector analysis"
        wait 1 seconds
    set avg_change to 0
    if count is above 0:
        set avg_change to total_change / count
    set sector_signal to "neutral"
    if avg_change is above 3:
        set sector_signal to "strong_bullish"
    if avg_change is above 1:
        set sector_signal to "bullish"
    if avg_change is below -3:
        set sector_signal to "strong_bearish"
    if avg_change is below -1:
        set sector_signal to "bearish"
    respond with {
        "sector": sector_name,
        "avg_change_1m_pct": round(avg_change, 2),
        "signal": sector_signal,
        "stocks_analyzed": count,
        "stocks": performances,
        "timestamp": time_now()
    }

to analyze_all_sectors:
    purpose: "Quick overview of all sector performances"
    set sector_names to ["banking", "it", "pharma", "auto", "fmcg", "energy", "metals", "finance", "infra"]
    set results to []
    repeat for each name in sector_names:
        run analyze_sector with name
        append {"sector": name, "avg_change": result.avg_change_1m_pct, "signal": result.signal} to results
    respond with {"sectors": results, "timestamp": time_now()}

// ─── Screener.in Data (Fundamentals) ───

to fetch_screener_data with symbol:
    purpose: "Fetch fundamental data from Screener.in public pages"
    set clean_symbol to replace(symbol, ".NS", "")
    try:
        gather response from "https://www.screener.in/api/company/{{clean_symbol}}/chart/?q=Price-DMA50-DMA200-Volume&days=365":
            timeout: 45
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"
                Accept: "application/json"
        respond with {"symbol": symbol, "screener_data": response}
    on error:
        log "Screener.in fetch failed for {{symbol}}: {{error}}"
        respond with {"symbol": symbol, "error": "screener_unavailable"}

// ─── FII/DII Activity ───

to fetch_fii_dii_data:
    purpose: "Fetch FII/DII trading activity from NSE"
    try:
        gather response from "https://www.nseindia.com/api/fiidiiTradeReact":
            warmup: "https://www.nseindia.com/"
            timeout: 90
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                Accept: "application/json"
                Referer: "https://www.nseindia.com/reports/fii-dii"
        respond with response
    on error:
        respond with {"error": "FII/DII data unavailable"}

// ─── Market Snapshot ───

to get_market_snapshot:
    purpose: "Get current market state snapshot"
    run fetch_index_data with "5d"
    set indices to result
    run fetch_market_news
    set news to result
    respond with {
        "timestamp": time_now(),
        "indices": indices,
        "trending": news,
        "status": "live"
    }

// ─── API Routes ───

api:
    GET /health runs health_check
    GET /symbols/nifty50 runs get_nifty50_symbols
    GET /symbols/banknifty runs get_banknifty_symbols
    GET /symbols/indices runs get_index_symbols
    GET /symbols/next50 runs get_nifty_next50_symbols
    GET /symbols/top200 runs get_top200_symbols
    GET /symbols/sectors runs get_sectors
    POST /stock/ohlcv runs fetch_stock_ohlcv
    POST /stock/bulk runs fetch_bulk_ohlcv
    POST /stock/fundamentals runs fetch_fundamentals
    POST /options/chain runs fetch_nse_option_chain
    POST /options/index runs fetch_nse_index_option_chain
    GET /news runs fetch_market_news
    GET /snapshot runs get_market_snapshot
    POST /collect/all runs collect_all_data
    GET /data/nifty50 runs fetch_nifty50_data
    POST /sector/analyze runs analyze_sector
    GET /sector/all runs analyze_all_sectors
    POST /screener runs fetch_screener_data
    GET /fii-dii runs fetch_fii_dii_data
    POST /stock/multiyear runs fetch_multi_year

to fetch_multi_year with symbol, years:
    purpose: "Fetch multi-year daily data for deep analysis and ML training"
    set period_map to {"1": "1y", "2": "2y", "3": "5y", "5": "5y", "10": "10y"}
    set p to "2y"
    if has_key(period_map, str(years)):
        set p to period_map[str(years)]
    try:
        gather response from "https://query2.finance.yahoo.com/v8/finance/chart/{{symbol}}?range={{p}}&interval=1d&includePrePost=false":
            timeout: 60
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36"
        if response.chart is equal nothing:
            respond with {"error": "No data"}
        set rd to response.chart.result[0]
        set ts to rd.timestamp
        set q to rd.indicators.quote[0]
        set candles to []
        repeat for each i in range(0, len(ts)):
            append {"timestamp": ts[i], "open": q.open[i], "high": q.high[i], "low": q.low[i], "close": q.close[i], "volume": q.volume[i]} to candles
        respond with {"symbol": symbol, "period": p, "candles": candles, "count": len(candles)}
    on error:
        respond with {"error": "Failed to fetch multi-year data", "symbol": symbol}

to health_check:
    respond with {"service": "data-collector", "status": "healthy", "version": "1.0.0", "timestamp": time_now()}
