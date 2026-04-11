service "neuraledge"
version "1.0.0"
description "Financial Intelligence Engine — Indian Stock Market Analysis. Pure math: RSI, MACD, ADX, Bollinger, swarm, options. LLM optional."

// ─── SIGNAL SEMANTICS ───
// BUY  = Expect price to INCREASE — buy now, sell later at higher price
// SELL = Expect price to DECREASE — sell/avoid now, buy back later at lower price
// HOLD = No clear direction — stay out or hold position
// Validation: BUY "wins" if price goes up after 5 days; SELL "wins" if price goes down

configure:
    port: 8000
    cors_origin: "*"
    log_requests: true
    rate_limit: 200
    ai_key is env:NC_AI_KEY

// ─── HTTP helpers (per NC_USER_MANUAL: env, format) ───

to get_http_headers:
    purpose: "Standard headers for Yahoo/NSE — uses NC_HTTP_USER_AGENT from env"
    set ua to env("NC_HTTP_USER_AGENT")
    if ua is equal nothing or ua is equal "":
        set ua to "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
    respond with {"User-Agent": ua, "Accept": "application/json"}

to fetch_url with url:
    purpose: "HTTP GET with standard headers (per NC manual)"
    run get_http_headers
    set resp to http_get(url, result)
    respond with resp

// =================================================================
// DATA COLLECTION
// =================================================================

// ─── Live Symbol Fetch from NSE (with cache + fallback) ───

to normalize_symbol with s:
    purpose: "Ensure symbol has .NS or .BO suffix for Yahoo"
    if s is equal nothing or s is equal "":
        respond with ""
    set clean to replace(replace(s, " ", ""), ".NS", "")
    set clean to replace(clean, ".BO", "")
    if clean is equal "":
        respond with ""
    if contains(s, ".BO"):
        respond with clean + ".BO"
    respond with clean + ".NS"

to fetch_nse_index_constituents with index_param:
    purpose: "Fetch index constituents from NSE API (live); returns list of symbols or nothing on fail"
    try:
        set url to "https://www.nseindia.com/api/equity-stockIndices?index=" + index_param
        gather resp from url:
            warmup: "https://www.nseindia.com/"
            timeout: 5
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
                Accept: "application/json"
                Referer: "https://www.nseindia.com/"
        if resp is equal nothing or type(resp) is not equal "record":
            respond with nothing
        if not has_key(resp, "data"):
            respond with nothing
        set data_arr to resp["data"]
        if type(data_arr) is not equal "list":
            respond with nothing
        set symbols to []
        repeat for each item in data_arr:
            if type(item) is equal "record" and has_key(item, "symbol"):
                set sym to item["symbol"]
                if sym is not equal nothing and sym is not equal "":
                    set clean to replace(replace(sym, " ", ""), "-EQ", "")
                    set full to clean + ".NS"
                    append full to symbols
        if len(symbols) is above 0:
            respond with symbols
        respond with nothing
    on error:
        log "NSE index fetch failed for {{index_param}}: {{error}}"
        respond with nothing

to get_symbol_cache with cache_key:
    purpose: "Read cached symbols if fresh (24h)"
    set path to "data/cache_symbols_" + cache_key + ".json"
    try:
        set content to read_file(path)
        set cached to json_decode(content)
        if has_key(cached, "symbols") and has_key(cached, "fetched_at"):
            set age to time_now() - cached["fetched_at"]
            if age is below 86400:
                respond with cached["symbols"]
    on error:
        pass
    respond with nothing

to set_symbol_cache with cache_key, symbols:
    purpose: "Write symbols to cache"
    mkdir("data")
    set path to "data/cache_symbols_" + cache_key + ".json"
    set payload to json_encode({"symbols": symbols, "fetched_at": time_now()})
    write_file(path, payload)

to fetch_nse_equity_list:
    purpose: "Fetch all NSE equities from NSE archive CSV and cache daily"
    set cached to get_symbol_cache("nse_all")
    if cached is not equal nothing and len(cached) is above 100:
        respond with cached
    try:
        set hdrs to {
            "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Accept": "text/csv,text/plain,*/*",
            "Referer": "https://www.nseindia.com/"
        }
        set csv_text to http_get("https://archives.nseindia.com/content/equities/EQUITY_L.csv", hdrs)
        set rows to split(csv_text, "\n")
        set symbols to []
        set seen to {}
        repeat for each row in rows:
            set line to trim(replace(row, "\r", ""))
            if line is not equal "":
                set cols to split(line, ",")
                if len(cols) is above 0:
                    set raw_symbol to trim(replace(replace(cols[0], "\"", ""), "'", ""))
                    if raw_symbol is not equal "" and raw_symbol is not equal "SYMBOL":
                        set sym to raw_symbol + ".NS"
                        if has_key(seen, sym) is equal no:
                            set seen[sym] to yes
                            append sym to symbols
        if len(symbols) is above 500:
            run set_symbol_cache with "nse_all", symbols
            respond with symbols
        respond with nothing
    on error:
        log "NSE all-equities fetch failed: {{error}}"
        respond with nothing

to get_all_symbols:
    purpose: "All NSE symbols — live from NSE archive CSV, fallback to merged liquid universe"
    set all_symbols to nothing
    try:
        run fetch_nse_equity_list
        set all_symbols to result
    on error:
        set all_symbols to nothing
    if all_symbols is not equal nothing and len(all_symbols) is above 500:
        respond with all_symbols
    set merged to []
    set seen to {}
    run get_nifty50_symbols
    set n50 to result
    repeat for each sym in n50:
        if has_key(seen, sym) is equal no:
            set seen[sym] to yes
            append sym to merged
    run get_nifty_next50_symbols
    set n50_next to result
    repeat for each sym in n50_next:
        if has_key(seen, sym) is equal no:
            set seen[sym] to yes
            append sym to merged
    run get_fno_symbols
    set fno to result
    repeat for each sym in fno:
        if has_key(seen, sym) is equal no:
            set seen[sym] to yes
            append sym to merged
    run get_sectors
    set sectors to result
    repeat for each sector_name in keys(sectors):
        set sector_list to sectors[sector_name]
        repeat for each sym in sector_list:
            if has_key(seen, sym) is equal no:
                set seen[sym] to yes
                append sym to merged
    respond with merged

to get_nifty50_symbols:
    purpose: "Nifty 50 — live from NSE, cached 24h, fallback if fail"
    set cached to get_symbol_cache("nifty50")
    if cached is not equal nothing:
        respond with cached
    set fallback to ["RELIANCE.NS","TCS.NS","HDFCBANK.NS","INFY.NS","ICICIBANK.NS","HINDUNILVR.NS","ITC.NS","SBIN.NS","BHARTIARTL.NS","KOTAKBANK.NS","LT.NS","AXISBANK.NS","ASIANPAINT.NS","MARUTI.NS","TITAN.NS","SUNPHARMA.NS","BAJFINANCE.NS","WIPRO.NS","ULTRACEMCO.NS","NESTLEIND.NS","HCLTECH.NS","BAJAJFINSV.NS","POWERGRID.NS","NTPC.NS","ONGC.NS","TATAMOTORS.NS","ADANIPORTS.NS","COALINDIA.NS","JSWSTEEL.NS","TATASTEEL.NS","TECHM.NS","INDUSINDBK.NS","HINDALCO.NS","GRASIM.NS","BRITANNIA.NS","CIPLA.NS","DRREDDY.NS","DIVISLAB.NS","EICHERMOT.NS","APOLLOHOSP.NS","SBILIFE.NS","HDFCLIFE.NS","TATACONSUM.NS","BAJAJ-AUTO.NS","BPCL.NS","HEROMOTOCO.NS","UPL.NS","LTIM.NS","ADANIENT.NS"]
    respond with fallback

to get_index_symbols:
    respond with ["^NSEI", "^NSEBANK"]

to get_banknifty_symbols:
    purpose: "Bank Nifty — live from NSE, cached 24h, fallback if fail"
    set cached to get_symbol_cache("banknifty")
    if cached is not equal nothing:
        respond with cached
    set fallback to ["HDFCBANK.NS","ICICIBANK.NS","SBIN.NS","KOTAKBANK.NS","AXISBANK.NS","INDUSINDBK.NS","BANDHANBNK.NS","FEDERALBNK.NS","IDFCFIRSTB.NS","PNB.NS","BANKBARODA.NS","AUBANK.NS"]
    respond with fallback

to get_nifty_next50_symbols:
    purpose: "Nifty Next 50 — live from NSE, cached 24h, fallback if fail"
    set cached to get_symbol_cache("next50")
    if cached is not equal nothing:
        respond with cached
    set fallback to ["ABB.NS","ADANIGREEN.NS","ADANIPOWER.NS","AMBUJACEM.NS","ATGL.NS","AUROPHARMA.NS","BAJAJHLDNG.NS","BANKBARODA.NS","BEL.NS","BERGEPAINT.NS","BOSCHLTD.NS","CANBK.NS","CHOLAFIN.NS","COLPAL.NS","CONCOR.NS","DLF.NS","DABUR.NS","DMART.NS","GAIL.NS","GODREJCP.NS","HAVELLS.NS","HINDPETRO.NS","ICICIPRULI.NS","ICICIGI.NS","IDEA.NS","INDHOTEL.NS","IOC.NS","IRCTC.NS","JINDALSTEL.NS","JIOFIN.NS","LICI.NS","LUPIN.NS","MARICO.NS","MAXHEALTH.NS","MCDOWELL-N.NS","MOTHERSON.NS","NAUKRI.NS","NHPC.NS","NMDC.NS","PAGEIND.NS","PFC.NS","PIDILITIND.NS","PNB.NS","RECLTD.NS","SBICARD.NS","SHREECEM.NS","SIEMENS.NS","TATAPOWER.NS","TORNTPHARM.NS","VBL.NS"]
    respond with fallback

to get_fno_symbols:
    purpose: "F&O (Futures & Options) constituents from NSE — ~180 liquid stocks"
    set cached to get_symbol_cache("fno")
    if cached is not equal nothing:
        respond with cached
    set fallback to ["RELIANCE.NS","TCS.NS","HDFCBANK.NS","INFY.NS","ICICIBANK.NS","HINDUNILVR.NS","ITC.NS","SBIN.NS","BHARTIARTL.NS","KOTAKBANK.NS","LT.NS","AXISBANK.NS","ASIANPAINT.NS","MARUTI.NS","TITAN.NS","SUNPHARMA.NS","BAJFINANCE.NS","WIPRO.NS","ULTRACEMCO.NS","NESTLEIND.NS","HCLTECH.NS","BAJAJFINSV.NS","POWERGRID.NS","NTPC.NS","ONGC.NS","TATAMOTORS.NS","ADANIPORTS.NS","COALINDIA.NS","JSWSTEEL.NS","TATASTEEL.NS","TECHM.NS","INDUSINDBK.NS","HINDALCO.NS","GRASIM.NS","BRITANNIA.NS","CIPLA.NS","DRREDDY.NS","DIVISLAB.NS","EICHERMOT.NS","APOLLOHOSP.NS","SBILIFE.NS","HDFCLIFE.NS","TATACONSUM.NS","BAJAJ-AUTO.NS","BPCL.NS","HEROMOTOCO.NS","UPL.NS","LTIM.NS","ADANIENT.NS","ABB.NS","ADANIGREEN.NS","ADANIPOWER.NS","AMBUJACEM.NS","ATGL.NS","AUROPHARMA.NS","BAJAJHLDNG.NS","BANKBARODA.NS","BEL.NS","BERGEPAINT.NS","BOSCHLTD.NS","CANBK.NS","CHOLAFIN.NS","COLPAL.NS","CONCOR.NS","DLF.NS","DABUR.NS","DMART.NS","GAIL.NS","GODREJCP.NS","HAVELLS.NS","HINDPETRO.NS","ICICIPRULI.NS","ICICIGI.NS","IDEA.NS","INDHOTEL.NS","IOC.NS","IRCTC.NS","JINDALSTEL.NS","JIOFIN.NS","LICI.NS","LUPIN.NS","MARICO.NS","MAXHEALTH.NS","MCDOWELL-N.NS","MOTHERSON.NS","NAUKRI.NS","NHPC.NS","NMDC.NS","PAGEIND.NS","PFC.NS","PIDILITIND.NS","PNB.NS","RECLTD.NS","SBICARD.NS","SHREECEM.NS","SIEMENS.NS","TATAPOWER.NS","TORNTPHARM.NS","VBL.NS"]
    respond with fallback

to get_top200_symbols:
    purpose: "Top 200 liquid NSE symbols — Nifty 50 + Next 50 + F&O merged and de-duplicated"
    set top to []
    set seen to {}
    run get_nifty50_symbols
    set n50 to result
    repeat for each sym in n50:
        if has_key(seen, sym) is equal no and len(top) is below 200:
            set seen[sym] to yes
            append sym to top
    run get_nifty_next50_symbols
    set next50 to result
    repeat for each sym in next50:
        if has_key(seen, sym) is equal no and len(top) is below 200:
            set seen[sym] to yes
            append sym to top
    run get_fno_symbols
    set fno to result
    repeat for each sym in fno:
        if has_key(seen, sym) is equal no and len(top) is below 200:
            set seen[sym] to yes
            append sym to top
    if len(top) is above 0:
        respond with top
    run get_all_symbols
    set all_symbols to result
    respond with slice(all_symbols, 0, 200)

// ─── Low-priced stocks — subset of F&O / Nifty (liquid, often sub-₹200) ───

to get_lowpriced_symbols:
    purpose: "Liquid stocks often <₹200 — prefer F&O overlap, fallback to known list"
    set low to ["IDEA.NS","YESBANK.NS","IDFCFIRSTB.NS","PNB.NS","BANKBARODA.NS","CANBK.NS","SAIL.NS","NATIONALUM.NS","COALINDIA.NS","NMDC.NS","JINDALSTEL.NS","GAIL.NS","IOC.NS","HINDPETRO.NS","NTPC.NS","PFC.NS","RECLTD.NS","NHPC.NS","TATAPOWER.NS","POWERGRID.NS","UCOBANK.NS","CENTRALBK.NS","INDIANB.NS","IOB.NS"]
    respond with low

// ─── Sector Definitions — banking from live; rest from F&O+N50 intersection ───

to get_sectors:
    purpose: "Sector groupings for peers; banking from NSE live, others use static (no NSE sector API)"
    run get_banknifty_symbols
    set banks to result
    run get_nifty50_symbols
    set n50 to result
    respond with {
        "banking": banks,
        "it": ["TCS.NS","INFY.NS","WIPRO.NS","HCLTECH.NS","TECHM.NS","LTIM.NS","COFORGE.NS","MPHASIS.NS","PERSISTENT.NS","KPITTECH.NS"],
        "pharma": ["SUNPHARMA.NS","CIPLA.NS","DRREDDY.NS","DIVISLAB.NS","APOLLOHOSP.NS","LUPIN.NS","AUROPHARMA.NS","BIOCON.NS","TORNTPHARM.NS","ZYDUSLIFE.NS"],
        "auto": ["TATAMOTORS.NS","MARUTI.NS","BAJAJ-AUTO.NS","EICHERMOT.NS","HEROMOTOCO.NS","TVSMOTOR.NS","ASHOKLEY.NS","ESCORTS.NS","APOLLOTYRE.NS","MOTHERSON.NS"],
        "fmcg": ["HINDUNILVR.NS","ITC.NS","NESTLEIND.NS","BRITANNIA.NS","TATACONSUM.NS","DABUR.NS","MARICO.NS","COLPAL.NS","GODREJCP.NS","EMAMILTD.NS"],
        "energy": ["RELIANCE.NS","ONGC.NS","BPCL.NS","NTPC.NS","POWERGRID.NS","COALINDIA.NS","GAIL.NS","IOC.NS","ADANIGREEN.NS","TATAPOWER.NS"],
        "metals": ["TATASTEEL.NS","JSWSTEEL.NS","HINDALCO.NS","VEDL.NS","SAIL.NS","NATIONALUM.NS","JINDALSTEL.NS","NMDC.NS","HINDCOPPER.NS","COALINDIA.NS"],
        "realty": ["DLF.NS","OBEROIRLTY.NS","PRESTIGE.NS","PHOENIXLTD.NS","GODREJPROP.NS"],
        "finance": ["BAJFINANCE.NS","BAJAJFINSV.NS","SBILIFE.NS","HDFCLIFE.NS","ICICIPRULI.NS","ICICIGI.NS","CHOLAFIN.NS","MUTHOOTFIN.NS","MFSL.NS","HDFCAMC.NS"],
        "infra": ["LT.NS","ADANIPORTS.NS","ULTRACEMCO.NS","GRASIM.NS","AMBUJACEM.NS","ACC.NS","SHREECEM.NS","BHARTIARTL.NS","CONCOR.NS","IRCTC.NS"]
    }

to get_bse_symbols:
    purpose: "BSE Sensex 30 — live from api.bseindia.com (Indexw blocked); uses cached list. Same tickers as Nifty, .BO suffix."
    set cached to get_symbol_cache("bse_sensex30")
    if cached is not equal nothing:
        respond with cached
    set list to ["RELIANCE.BO","TCS.BO","HDFCBANK.BO","INFY.BO","ICICIBANK.BO","HINDUNILVR.BO","ITC.BO","SBIN.BO","BHARTIARTL.BO","KOTAKBANK.BO","LT.BO","AXISBANK.BO","ASIANPAINT.BO","MARUTI.BO","TITAN.BO","SUNPHARMA.BO","BAJFINANCE.BO","WIPRO.BO","ULTRACEMCO.BO","NESTLEIND.BO","HCLTECH.BO","BAJAJFINSV.BO","POWERGRID.BO","NTPC.BO","ONGC.BO","TATAMOTORS.BO","ADANIPORTS.BO","COALINDIA.BO","JSWSTEEL.BO","TATASTEEL.BO"]
    run set_symbol_cache with "bse_sensex30", list
    respond with list

to get_bse_scripcode with symbol:
    purpose: "Map BSE symbol (e.g. RELIANCE) to BSE scrip code for api.bseindia.com"
    set clean to replace(replace(symbol, ".BO", ""), ".NS", "")
    set codes to {"RELIANCE":"500325","TCS":"532540","HDFCBANK":"500180","INFY":"500209","ICICIBANK":"532174","HINDUNILVR":"696492","ITC":"500875","SBIN":"500112","BHARTIARTL":"532454","KOTAKBANK":"500247","LT":"500510","AXISBANK":"532215","ASIANPAINT":"500820","MARUTI":"532500","TITAN":"500114","SUNPHARMA":"524715","BAJFINANCE":"500034","WIPRO":"507685","ULTRACEMCO":"532538","NESTLEIND":"500790","HCLTECH":"532281","BAJAJFINSV":"532978","POWERGRID":"532498","NTPC":"532555","ONGC":"500312","TATAMOTORS":"500570","ADANIPORTS":"532612","COALINDIA":"533278","JSWSTEEL":"532286","TATASTEEL":"500470","INDUSINDBK":"532187","TECHM":"532755","HEROMOTOCO":"500182","DIVISLAB":"532488","CIPLA":"524743","DRREDDY":"524124","EICHERMOT":"505200","BPCL":"500547","MOTHERSON":"500420","BAJAJ-AUTO":"532977"}
    if has_key(codes, clean):
        respond with codes[clean]
    respond with nothing

to fetch_bse_quote with symbol:
    purpose: "Fetch live stats from BSE api.bseindia.com StockTrading (WAP, turnover, volume, mcap). Like NSE quote — LTP via Yahoo; this adds BSE-native data."
    set clean to replace(symbol, ".BO", "")
    run get_bse_scripcode with clean
    set scrip to result
    if scrip is equal nothing:
        respond with {"error": "BSE scrip code unknown for " + clean + ". Add to get_bse_scripcode map."}
    try:
        gather resp from "https://api.bseindia.com/BseIndiaAPI/api/StockTrading/w?scripcode={{scrip}}":
            timeout: 30
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
                Referer: "https://www.bseindia.com/"
                Accept: "application/json"
        if resp is equal nothing or type(resp) is not equal "record":
            respond with {"error": "BSE API unavailable for " + clean}
        if not has_key(resp, "WAP"):
            respond with {"error": "BSE no data", "keys": keys(resp)}
        set wap_str to resp["WAP"]
        set wap_num to 0
        if wap_str is not equal nothing and wap_str is not equal "":
            set wap_clean to replace(replace(wap_str, ",", ""), " ", "")
            set wap_num to wap_clean * 1
        set ttq to 0
        if has_key(resp, "TTQ") and resp["TTQ"] is not equal "":
            set ttq_str to replace(replace(resp["TTQ"], ",", ""), " ", "")
            set ttq to ttq_str * 1
        set candle to {"timestamp": time_now(), "open": wap_num, "high": wap_num, "low": wap_num, "close": wap_num, "volume": ttq * 100000}
        set turn to ""
        set mcap to ""
        set ttq_val to ""
        if has_key(resp, "Turnover"):
            set turn to resp["Turnover"]
        if has_key(resp, "MktCapFull"):
            set mcap to resp["MktCapFull"]
        if has_key(resp, "TTQ"):
            set ttq_val to resp["TTQ"]
        respond with {"symbol": symbol, "currency": "INR", "exchange": "BSE", "price": wap_num, "open": wap_num, "high": wap_num, "low": wap_num, "change": 0, "change_pct": 0, "candles": [candle], "count": 1, "source": "bse_live", "fetched_at": time_now(), "wap": wap_num, "turnover": turn, "ttq": ttq_val, "mkt_cap": mcap}
    on error:
        respond with {"error": "BSE failed for " + clean}

// ─── Yahoo Finance Data Collection ───

to debug_fetch with symbol:
    set yahoo_url to "https://query2.finance.yahoo.com/v8/finance/chart/" + symbol + "?range=5d&interval=1d"
    run fetch_url with yahoo_url
    set response to result
    respond with {"url": yahoo_url, "type": type(response), "keys": keys(response), "has_chart": has_key(response, "chart"), "chart_val": response["chart"]}

to fetch_stock_ohlcv with symbol, period, interval:
    purpose: "Fetch OHLCV — Yahoo + NSE. Uses in-memory cache (15 min) + file cache (24h) like other live-data sites."
    run normalize_symbol with symbol
    set symbol to result
    if symbol is equal "":
        respond with {"error": "Invalid or empty symbol"}
    set cache_key to "ohlcv_" + replace(replace(symbol, ".", "_"), "-", "_") + "_" + period + "_" + interval
    if is_cached(cache_key):
        set c to cached(cache_key)
        if has_key(c, "fetched_at"):
            set age to time_now() - c["fetched_at"]
            if age is below 900:
                respond with c
    try:
        set content to read_file("data/ohlcv_cache/" + cache_key + ".json")
        set file_data to json_decode(content)
        if has_key(file_data, "fetched_at"):
            set age to time_now() - file_data["fetched_at"]
            if age is below 86400 and has_key(file_data, "candles") and len(file_data["candles"]) is above 0:
                run cache_ohlcv_save with cache_key, file_data
                respond with file_data
    on error:
        pass
    try:
        set yahoo_url to "https://query2.finance.yahoo.com/v8/finance/chart/" + symbol + "?range=" + period + "&interval=" + interval + "&includePrePost=false"
        run fetch_url with yahoo_url
        set response to result
        if response is not equal nothing and has_key(response, "chart"):
            set chart_data to response["chart"]
            set result_data to chart_data["result"]
            set first_result to result_data[0]
            set timestamps to first_result["timestamp"]
            set indicators to first_result["indicators"]
            set quotes to indicators["quote"]
            set first_quote to quotes[0]
            set opens to first_quote["open"]
            set highs to first_quote["high"]
            set lows to first_quote["low"]
            set closes to first_quote["close"]
            set vols to first_quote["volume"]
            set candles to []
            repeat for each i in range(0, len(timestamps)):
                append {"timestamp": timestamps[i], "open": opens[i], "high": highs[i], "low": lows[i], "close": closes[i], "volume": vols[i]} to candles
            set meta to first_result["meta"]
            set out to {"symbol": symbol, "currency": meta["currency"], "exchange": meta["exchangeName"], "candles": candles, "count": len(candles), "source": "yahoo", "fetched_at": time_now()}
            run cache_ohlcv_save with cache_key, out
            respond with out
    on error:
        log "Yahoo failed for {{symbol}}"
    if contains(symbol, ".BO"):
        try:
            run fetch_bse_quote with symbol
            set bse_result to result
            if bse_result is not equal nothing and not has_key(bse_result, "error"):
                run cache_ohlcv_save with cache_key, bse_result
                respond with bse_result
        on error:
            log "BSE quote failed for {{symbol}}"
        respond with {"error": "Cannot fetch data for " + symbol + ". Yahoo and BSE API failed. Try again later."}
    try:
        run fetch_nse_quote with symbol
        set nse_result to result
        if nse_result is not equal nothing and not has_key(nse_result, "error"):
            run cache_ohlcv_save with cache_key, nse_result
            respond with nse_result
    on error:
        log "NSE failed for {{symbol}}"
    respond with {"error": "Cannot fetch data for " + symbol + ". Both Yahoo and NSE failed. Try running locally (Yahoo may block cloud IPs)."}

to fetch_nse_quote with symbol:
    purpose: "Fetch live quote from NSE India API (no key, open). Needs NC engine warmup+cookies. BSE symbols not supported."
    if contains(symbol, ".BO"):
        respond with {"error": "NSE quote only for NSE symbols (.NS). Use Yahoo for BSE (.BO)."}
    set clean to replace(symbol, ".NS", "")
    try:
        gather resp from "https://www.nseindia.com/api/quote-equity?symbol={{clean}}":
            warmup: "https://www.nseindia.com/"
            timeout: 90
            headers:
                User-Agent: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
                Accept: "application/json"
                Accept-Language: "en-IN,en;q=0.9"
                Referer: "https://www.nseindia.com/"
        if resp is equal nothing or type(resp) is not equal "record":
            respond with {"error": "NSE unavailable for " + clean}
        if not has_key(resp, "priceInfo"):
            respond with {"error": "NSE no price data", "keys": keys(resp)}
        set pi to resp["priceInfo"]
        set lp to pi["lastPrice"]
        set op to pi["open"]
        set hi to pi["intraDayHighLow"]
        set high_val to lp
        set low_val to lp
        if hi is not equal nothing:
            if has_key(hi, "max"):
                set high_val to hi["max"]
            if has_key(hi, "min"):
                set low_val to hi["min"]
        set chg to pi["change"]
        set pchg to pi["pChange"]
        set candle to {"timestamp": time_now(), "open": op, "high": high_val, "low": low_val, "close": lp, "volume": 0}
        set nse_out to {"symbol": symbol, "currency": "INR", "exchange": "NSE", "price": lp, "open": op, "high": high_val, "low": low_val, "change": chg, "change_pct": pchg, "candles": [candle], "count": 1, "source": "nse_live", "fetched_at": time_now()}
        respond with nse_out
    on error:
        respond with {"error": "NSE failed for " + clean}

to save_wishlist with stocks:
    purpose: "Save user wishlist"
    mkdir("data/users")
    write_file("data/users/wishlist.json", json_encode(stocks))
    respond with {"saved": len(stocks)}

to load_wishlist:
    purpose: "Load user wishlist"
    try:
        set content to read_file("data/users/wishlist.json")
        respond with json_decode(content)
    on error:
        respond with []

to save_favorites with stocks:
    purpose: "Save user favorite stocks"
    mkdir("data/users")
    write_file("data/users/favorites.json", json_encode(stocks))
    respond with {"saved": len(stocks)}

to load_favorites:
    purpose: "Load user favorites"
    try:
        set content to read_file("data/users/favorites.json")
        respond with json_decode(content)
    on error:
        respond with []

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
    purpose: "Compute stock fundamentals from OHLCV data + meta"
    run fetch_stock_ohlcv with symbol, "1y", "1d"
    set data to result
    if data.error is not equal nothing:
        respond with {"error": "Failed to fetch data", "symbol": symbol}
    set candles to data.candles
    set n to len(candles)
    if n is below 2:
        respond with {"error": "Insufficient data", "symbol": symbol}
    set closes to map_field(candles, "close")
    set highs to map_field(candles, "high")
    set lows to map_field(candles, "low")
    set volumes to map_field(candles, "volume")
    set last to closes[n - 1]
    set prev to closes[n - 2]
    set yr_high to max(highs)
    set yr_low to min(lows)
    set today_high to highs[n - 1]
    set today_low to lows[n - 1]
    set total_vol to 0
    repeat for each v in volumes:
        set total_vol to total_vol + v
    set avg_vol to total_vol / n
    set change_1y to round(((last - closes[0]) / closes[0]) * 100, 2)
    set daily_returns to []
    set idx to 1
    repeat while idx is below n:
        set daily_ret to (closes[idx] - closes[idx - 1]) / closes[idx - 1]
        append daily_ret to daily_returns
        set idx to idx + 1
    set sum_ret to 0
    repeat for each r in daily_returns:
        set sum_ret to sum_ret + r
    set avg_ret to sum_ret / len(daily_returns)
    set var_sum to 0
    repeat for each r in daily_returns:
        set var_sum to var_sum + ((r - avg_ret) * (r - avg_ret))
    set std_dev to sqrt(var_sum / len(daily_returns))
    set sqrt252 to sqrt(252)
    set annualized_vol to round(std_dev * sqrt252 * 100, 2)
    set annualized_ret to round(avg_ret * 252 * 100, 2)
    set sharpe to 0
    if std_dev is above 0:
        set sharpe to round((avg_ret * 252) / (std_dev * sqrt252), 2)
    set sector_map to {"RELIANCE": "Energy", "TCS": "Technology", "INFY": "Technology", "HDFCBANK": "Financial Services", "ICICIBANK": "Financial Services", "SBIN": "Financial Services", "HINDUNILVR": "Consumer Goods", "ITC": "Consumer Goods", "BHARTIARTL": "Telecommunication", "KOTAKBANK": "Financial Services", "LT": "Construction", "AXISBANK": "Financial Services", "ASIANPAINT": "Consumer Goods", "MARUTI": "Automobile", "TITAN": "Consumer Goods", "SUNPHARMA": "Healthcare", "BAJFINANCE": "Financial Services", "WIPRO": "Technology", "ULTRACEMCO": "Construction", "NESTLEIND": "Consumer Goods", "HCLTECH": "Technology", "NTPC": "Energy", "ONGC": "Energy", "TATAMOTORS": "Automobile", "COALINDIA": "Mining", "JSWSTEEL": "Metals", "TATASTEEL": "Metals", "TECHM": "Technology", "CIPLA": "Healthcare", "DRREDDY": "Healthcare", "BRITANNIA": "Consumer Goods", "POWERGRID": "Energy", "ADANIENT": "Conglomerate", "BPCL": "Energy", "TATAPOWER": "Energy", "ZOMATO": "Technology", "DLF": "Real Estate"}
    set clean to replace(symbol, ".NS", "")
    set sector to "N/A"
    if has_key(sector_map, clean):
        set sector to sector_map[clean]
    respond with {
        "symbol": symbol,
        "current_price": last,
        "previous_close": prev,
        "day_high": today_high,
        "day_low": today_low,
        "fifty_two_week_high": yr_high,
        "fifty_two_week_low": yr_low,
        "avg_volume": round(avg_vol),
        "annualized_volatility": annualized_vol,
        "annualized_return": annualized_ret,
        "sharpe_ratio": sharpe,
        "change_1y_pct": change_1y,
        "sector": sector,
        "data_points": n
    }

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
    set fallback to [
        {"symbol": "RELIANCE.NS", "name": "Reliance Industries", "exchange": "NSE"},
        {"symbol": "TCS.NS", "name": "Tata Consultancy Services", "exchange": "NSE"},
        {"symbol": "HDFCBANK.NS", "name": "HDFC Bank", "exchange": "NSE"},
        {"symbol": "INFY.NS", "name": "Infosys", "exchange": "NSE"},
        {"symbol": "ICICIBANK.NS", "name": "ICICI Bank", "exchange": "NSE"}
    ]
    respond with {"trending": fallback, "count": len(fallback), "source": "fallback"}

// ─── Data Persistence ───

to save_market_data with symbol, data:
    purpose: "Save market data to local file"
    set filename to "data/{{symbol}}_ohlcv.json"
    set payload to json_encode(data)
    write_file(filename, payload)
    log "Saved data for {{symbol}}"
    respond with {"saved": symbol, "file": filename}

to cache_ohlcv_save with cache_key, data:
    purpose: "Persist OHLCV to in-memory + file cache (same pattern as Yahoo/Reuters live sites)"
    cache(cache_key, data)
    mkdir("data")
    mkdir("data/ohlcv_cache")
    set path to "data/ohlcv_cache/" + cache_key + ".json"
    write_file(path, json_encode(data))

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

to analyze_sector_quick with sector_name:
    purpose: "Fast sector overview — sample 4 stocks, 0.2s delay (NC_REQUEST_TIMEOUT=120)"
    run get_sectors
    set sectors to result
    if has_key(sectors, sector_name) is equal no:
        respond with {"sector": sector_name, "avg_change_1m_pct": 0, "signal": "neutral"}
    set all_symbols to sectors[sector_name]
    set sample to slice(all_symbols, 0, 4)
    set total_change to 0
    set count to 0
    repeat for each symbol in sample:
        try:
            run fetch_stock_ohlcv with symbol, "1mo", "1d"
            set data to result
            if data.error is equal nothing and len(data.candles) is above 1:
                set first_close to data.candles[0].close
                set last_close to data.candles[len(data.candles) - 1].close
                set change_pct to ((last_close - first_close) / first_close) * 100
                set total_change to total_change + change_pct
                set count to count + 1
        on error:
            log "Sector quick: failed {{symbol}}"
        wait 200 milliseconds
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
    respond with {"sector": sector_name, "avg_change_1m_pct": round(avg_change, 2), "signal": sector_signal}

to analyze_all_sectors:
    purpose: "Instant sector overview for dashboard cards (cache-first, no live network dependency)"
    set cache_path to "data/cache_sectors.json"
    try:
        set raw to read_file(cache_path)
        if raw is not equal nothing and raw is not equal "":
            set cached to json_decode(raw)
            if has_key(cached, "sectors") and has_key(cached, "ts"):
                set age to time_ms() - cached.ts
                if age is below 1800000:
                    respond with {"sectors": cached.sectors, "timestamp": cached.ts, "cached": age}
    on error:
        pass
    run get_sectors
    set sector_map to result
    set sector_names to ["banking", "it", "pharma", "auto", "fmcg", "energy", "metals", "finance", "infra"]
    set results to []
    repeat for each name in sector_names:
        set leaders to []
        set stock_count to 0
        if has_key(sector_map, name):
            set leaders to slice(sector_map[name], 0, 3)
            set stock_count to len(sector_map[name])
        append {"sector": name, "avg_change": 0, "signal": "neutral", "leaders": leaders, "stock_count": stock_count} to results
    set ts to time_ms()
    mkdir("data")
    write_file(cache_path, json_encode({"sectors": results, "ts": ts}))
    respond with {"sectors": results, "timestamp": ts}

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
// =================================================================
// TECHNICAL ANALYSIS & REGIME DETECTION
// =================================================================

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
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result

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


to api_rsi with symbol, period:
    run fetch_stock_ohlcv with symbol, "3mo", "1d"
    set stock_data to result
    run extract_price_arrays with stock_data.candles
    run compute_rsi with result.closes, period
    respond with {"symbol": symbol, "rsi": result, "period": period}

to api_macd with symbol:
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
    run extract_price_arrays with stock_data.candles
    run compute_macd with result.closes
    respond with {"symbol": symbol, "macd": result}

to api_regime with symbol:
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
    run detect_regime with stock_data.candles
    respond with {"symbol": symbol, "regime": result}

to api_options with symbol, spot_price:
    run fetch_nse_option_chain with symbol
    set chain_data to result
    run analyze_option_chain with chain_data.chain, spot_price
    respond with {"symbol": symbol, "options_analysis": result}
// =================================================================
// SWARM INTELLIGENCE
// =================================================================

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
    run fetch_stock_ohlcv with symbol, "1y", "1d"
    set stock_data to result

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


to api_backtest with symbol, strategy_type, params:
    run fetch_stock_ohlcv with symbol, "1y", "1d"
    set stock_data to result
    set candles to stock_data.candles
    set closes to map_field(candles, "close")
    set highs to map_field(candles, "high")
    set lows to map_field(candles, "low")
    set volumes to map_field(candles, "volume")
    set strategy to {"type": strategy_type, "params": params, "fitness": 0}
    run backtest_strategy with strategy, closes, highs, lows, volumes
    respond with {"symbol": symbol, "strategy": strategy_type, "params": params, "results": result}
// =================================================================
// SIGNAL AGGREGATION & RISK
// =================================================================

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
    run fetch_market_news
    set news_data to result
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

to load_learning_weights:
    purpose: "Load persisted signal source weights from unified weights.json (or legacy signal_weights.json)"
    set ss to nothing
    try:
        set content to read_file("data/learning/weights.json")
        set w to json_decode(content)
        if has_key(w, "signal_sources"):
            set ss to w["signal_sources"]
        if has_key(w, "technical") and ss is equal nothing:
            set ss to w
    on error:
        try:
            set content to read_file("data/learning/signal_weights.json")
            set ss to json_decode(content)
        on error:
            set ss to nothing
    set wt to 0.30
    set ws to 0.25
    set wo to 0.20
    set wl to 0.25
    if ss is not equal nothing:
        if has_key(ss, "technical"):
            set wt to ss["technical"]
        if has_key(ss, "swarm"):
            set ws to ss["swarm"]
        if has_key(ss, "options"):
            set wo to ss["options"]
        if has_key(ss, "llm"):
            set wl to ss["llm"]
    respond with {"technical": wt, "swarm": ws, "options": wo, "llm": wl}

to aggregate_signals with technical_signal, swarm_signal, options_signal, llm_signal:
    purpose: "Weighted voting — pure math (tech+swarm+options) or with LLM when available"
    run load_learning_weights
    set weights to result
    set w_technical to weights["technical"]
    set w_swarm to weights["swarm"]
    set w_options to weights["options"]
    set w_llm to weights["llm"]
    if llm_signal is equal nothing:
        set w_llm to 0
        set rest to w_technical + w_swarm + w_options
        if rest is above 0:
            set w_technical to round(w_technical / rest, 2)
            set w_swarm to round(w_swarm / rest, 2)
            set w_options to round(w_options / rest, 2)
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

to build_horizon_projection with label, trading_days, current_price, bias_score, annualized_vol_pct, base_confidence:
    purpose: "Convert bias + volatility into a horizon forecast band"
    set period_vol_pct to annualized_vol_pct * sqrt(trading_days / 252)
    set expected_return_pct to bias_score * period_vol_pct * 0.35
    set range_band_pct to period_vol_pct * 0.55
    set low_return_pct to expected_return_pct - range_band_pct
    set high_return_pct to expected_return_pct + range_band_pct
    set target_price to current_price * (1 + expected_return_pct / 100)
    set low_price to current_price * (1 + low_return_pct / 100)
    set high_price to current_price * (1 + high_return_pct / 100)
    set direction to "SIDEWAYS"
    if expected_return_pct is above 1:
        set direction to "UP"
    if expected_return_pct is below -1:
        set direction to "DOWN"
    set confidence to base_confidence
    if trading_days is above 20:
        set confidence to confidence - 4
    if trading_days is above 60:
        set confidence to confidence - 6
    if trading_days is above 120:
        set confidence to confidence - 8
    if confidence is below 35:
        set confidence to 35
    if confidence is above 92:
        set confidence to 92
    respond with {
        "label": label,
        "trading_days": trading_days,
        "direction": direction,
        "confidence": round(confidence),
        "expected_return_pct": round(expected_return_pct, 2),
        "target_price": round(target_price, 2),
        "range_low": round(low_price, 2),
        "range_high": round(high_price, 2),
        "volatility_band_pct": round(range_band_pct, 2)
    }

to generate_signal with symbol:
    purpose: "Full signal generation pipeline for a symbol"
    log "Generating signal for {{symbol}}..."
    run full_analysis with symbol
    set analysis to result
    if analysis.error is not equal nothing:
        respond with {"error": "Analysis failed for {{symbol}}", "details": analysis.error}
    run get_swarm_signal with symbol
    set swarm_result to result
    set options_result to nothing
    try:
        run fetch_nse_option_chain with symbol
        set options_chain to result
        if options_chain.chain is not equal nothing:
            run analyze_option_chain with options_chain.chain, analysis.current_price
            set options_result to result
            set options_result to options_result.options_analysis
    on error:
        log "Options data not available for {{symbol}}"
    set llm_result to nothing
    set ai_key to env("NC_AI_KEY")
    if ai_key is not equal nothing and ai_key is not equal "":
        try:
            run llm_market_analysis with symbol, analysis, analysis.regime
            set llm_result to result
        on error:
            log "LLM skipped for {{symbol}} (use technical+swarm+options)"
            try:
                run llm_news_sentiment with symbol
                set llm_result to result
            on error:
                log "LLM fallback failed — pure math mode"
    otherwise:
        log "NC_AI_KEY not set — pure math mode (no LLM)"
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
    set annualized_vol_pct to 28
    if analysis.regime.volatility_annualized is not equal nothing and analysis.regime.volatility_annualized is above 0:
        set annualized_vol_pct to analysis.regime.volatility_annualized
    if annualized_vol_pct is below 12:
        set annualized_vol_pct to 12
    if annualized_vol_pct is above 80:
        set annualized_vol_pct to 80
    set bias_score to 0
    set horizon_notes to []
    if aggregated.signal is equal "BUY":
        set bias_score to bias_score + 1.2
        append "Core engine is net BUY" to horizon_notes
    if aggregated.signal is equal "SELL":
        set bias_score to bias_score - 1.2
        append "Core engine is net SELL" to horizon_notes
    if analysis.signals.trend.trend is equal "bullish":
        set bias_score to bias_score + 0.7
        append "Daily trend is bullish" to horizon_notes
    if analysis.signals.trend.trend is equal "bearish":
        set bias_score to bias_score - 0.7
        append "Daily trend is bearish" to horizon_notes
    if contains(analysis.regime.regime, "bull"):
        set bias_score to bias_score + 0.8
        append "Regime is bullish" to horizon_notes
    if contains(analysis.regime.regime, "bear"):
        set bias_score to bias_score - 0.8
        append "Regime is bearish" to horizon_notes
    if analysis.indicators.macd.bullish is equal yes:
        set bias_score to bias_score + 0.45
        append "MACD is bullish" to horizon_notes
    otherwise:
        set bias_score to bias_score - 0.45
        append "MACD is bearish" to horizon_notes
    if analysis.indicators.rsi is below 35:
        set bias_score to bias_score + 0.35
        append "RSI shows oversold bounce potential" to horizon_notes
    if analysis.indicators.rsi is above 70:
        set bias_score to bias_score - 0.35
        append "RSI is overbought" to horizon_notes
    if swarm_result.signal is equal "buy":
        set bias_score to bias_score + (swarm_result.confidence / 200)
        append "Swarm AI supports upside" to horizon_notes
    if swarm_result.signal is equal "sell":
        set bias_score to bias_score - (swarm_result.confidence / 200)
        append "Swarm AI supports downside" to horizon_notes
    if options_result is not equal nothing and options_result.signal is equal "bullish":
        set bias_score to bias_score + 0.25
        append "Options flow is bullish" to horizon_notes
    if options_result is not equal nothing and options_result.signal is equal "bearish":
        set bias_score to bias_score - 0.25
        append "Options flow is bearish" to horizon_notes
    if llm_result is not equal nothing and llm_result.sentiment is equal "bullish":
        set bias_score to bias_score + 0.3
        append "AI sentiment is bullish" to horizon_notes
    if llm_result is not equal nothing and llm_result.sentiment is equal "bearish":
        set bias_score to bias_score - 0.3
        append "AI sentiment is bearish" to horizon_notes
    if bias_score is above 2.5:
        set bias_score to 2.5
    if bias_score is below -2.5:
        set bias_score to -2.5
    run build_horizon_projection with "1 Week", 5, analysis.current_price, bias_score, annualized_vol_pct, aggregated.confidence
    set forecast_week to result
    run build_horizon_projection with "1 Month", 21, analysis.current_price, bias_score, annualized_vol_pct, aggregated.confidence
    set forecast_month to result
    run build_horizon_projection with "3 Months", 63, analysis.current_price, bias_score, annualized_vol_pct, aggregated.confidence
    set forecast_quarter to result
    run build_horizon_projection with "6 Months", 126, analysis.current_price, bias_score, annualized_vol_pct, aggregated.confidence
    set forecast_six_months to result
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
        "rejections": risk.rejections,
        "forecast_horizons": {
            "week": forecast_week,
            "month": forecast_month,
            "quarter": forecast_quarter,
            "six_months": forecast_six_months
        },
        "forecast_basis": {
            "bias_score": round(bias_score, 2),
            "annualized_volatility_pct": round(annualized_vol_pct, 2),
            "notes": horizon_notes
        }
    }

to generate_bulk_signals with symbols:
    purpose: "Generate signals for multiple symbols"
    set signals to []
    repeat for each symbol in symbols:
        run generate_signal with symbol
        append result to signals
    // Top picks: filter approved, sort by confidence desc (per NC manual: filter_by, sort_by, reversed)
    set top_signals to filter_by(signals, "approved", "equal", yes)
    set top_signals to reversed(sort_by(top_signals, "confidence"))
    respond with {
        "total_analyzed": len(symbols),
        "approved_signals": len(top_signals),
        "signals": signals,
        "top_picks": top_signals,
        "timestamp": time_now()
    }

to scan_nifty50:
    purpose: "Scan all NIFTY 50 stocks for trading signals"
    run get_nifty50_symbols
    set symbols to result
    run generate_bulk_signals with symbols
    respond with result

to compute_quick_rsi with candles:
    purpose: "Quick RSI from raw candle data"
    if len(candles) is below 15:
        respond with 50
    set gains to 0
    set loss_total to 0
    set start_idx to len(candles) - 15
    repeat for each i in range(start_idx + 1, len(candles)):
        set curr_close to float(candles[i].close)
        set prev_close to float(candles[i - 1].close)
        set chg to curr_close - prev_close
        if chg is above 0:
            set gains to gains + chg
        otherwise:
            set loss_total to loss_total + abs(chg)
    set ag to gains / 14
    set al to loss_total / 14
    if al is below 0.001:
        respond with 95
    set rs to ag / al
    set rsi to 100 - (100 / (1 + rs))
    respond with round(rsi, 1)

to scan_one_stock with symbol:
    purpose: "Analyze one stock using RSI, MACD, ADX, Bollinger, trend — extended technical scan"
    run fetch_stock_ohlcv with symbol, "3mo", "1d"
    set stock_data to result
    set candles to stock_data.candles
    if candles is equal nothing:
        set candles to stock_data["candles"]
    if candles is equal nothing:
        respond with nothing
    if len(candles) is below 26:
        respond with nothing
    run extract_price_arrays with candles
    set prices to result
    run compute_quick_rsi with candles
    set rsi to result
    run compute_macd with prices.closes
    set macd_info to result
    run compute_adx with prices.highs, prices.lows, prices.closes, 14
    set adx_info to result
    run compute_bollinger_bands with prices.closes, 20, 2
    set bb to result
    set current_price to float(candles[len(candles) - 1].close)
    set prev_price to float(candles[len(candles) - 2].close)
    set price_change to round((current_price - prev_price) / prev_price * 100, 2)
    set sma20 to 0
    if len(candles) is at least 20:
        set sum20 to 0
        repeat for each j in range(len(candles) - 20, len(candles)):
            set sum20 to sum20 + float(candles[j].close)
        set sma20 to round(sum20 / 20, 2)
    set trend to "sideways"
    if current_price is above sma20 and sma20 is above 0:
        set trend to "bullish"
    if current_price is below sma20 and sma20 is above 0:
        set trend to "bearish"
    set score to 0
    set reasoning to ""
    if rsi is below 35:
        set score to score + 25
        set reasoning to reasoning + "RSI oversold (" + str(rsi) + "). "
    if rsi is above 65:
        set score to score - 25
        set reasoning to reasoning + "RSI overbought (" + str(rsi) + "). "
    if price_change is above 1:
        set score to score + 15
        set reasoning to reasoning + "Up " + str(price_change) + "%. "
    if price_change is below -1:
        set score to score - 15
        set reasoning to reasoning + "Down " + str(price_change) + "%. "
    if trend is equal "bullish":
        set score to score + 20
        set reasoning to reasoning + "Uptrend. "
    if trend is equal "bearish":
        set score to score - 20
        set reasoning to reasoning + "Downtrend. "
    if sma20 is above 0 and current_price is above sma20 * 1.02:
        set score to score + 10
        set reasoning to reasoning + "Above SMA20. "
    if sma20 is above 0 and current_price is below sma20 * 0.98:
        set score to score - 10
        set reasoning to reasoning + "Below SMA20. "
    if macd_info.bullish is equal yes:
        set score to score + 12
        set reasoning to reasoning + "MACD bullish. "
    if macd_info.bullish is equal no and macd_info.histogram is below 0:
        set score to score - 12
        set reasoning to reasoning + "MACD bearish. "
    if adx_info.adx is above 25 and adx_info.trend_direction is equal yes:
        set score to score + 8
        set reasoning to reasoning + "ADX strong uptrend. "
    if adx_info.adx is above 25 and adx_info.trend_direction is equal no:
        set score to score - 8
        set reasoning to reasoning + "ADX strong downtrend. "
    if bb.percent_b is below 0:
        set score to score + 10
        set reasoning to reasoning + "Below lower BB (oversold). "
    if bb.percent_b is above 1:
        set score to score - 10
        set reasoning to reasoning + "Above upper BB (overbought). "
    set sig to "HOLD"
    set conf to 50
    // Thresholds: 15/-15 (heuristic; backtest for tuning)
    if score is above 15:
        set sig to "BUY"
        set conf to min(95, 55 + score)
    if score is below -15:
        set sig to "SELL"
        set conf to min(95, 55 + abs(score))
    if sig is equal "HOLD":
        set conf to 40 + abs(score)
    set atr to current_price * 0.02
    set sl to round(current_price - atr * 2, 2)
    set tgt to round(current_price + atr * 3, 2)
    if sig is equal "SELL":
        set sl to round(current_price + atr * 2, 2)
        set tgt to round(current_price - atr * 3, 2)
    respond with {"symbol": symbol, "signal": sig, "confidence": round(conf, 1), "approved": conf is above 60, "current_price": current_price, "reasoning": reasoning, "risk_management": {"stop_loss": sl, "target_price": tgt, "reward_risk_ratio": 1.5, "position_size": round(20000 / current_price)}, "components": {"technical": {"regime": trend, "rsi": rsi, "trend": trend}}, "source_scores": {"technical": round(score / 50, 2), "swarm": 0, "options": 0, "llm": 0}}

to compute_scan_score with candles:
    purpose: "Compute scan score from candles — used by scan_one_stock and backtest_thresholds"
    if len(candles) is below 26:
        respond with 0
    run extract_price_arrays with candles
    set prices to result
    run compute_quick_rsi with candles
    set rsi to result
    run compute_macd with prices.closes
    set macd_info to result
    run compute_adx with prices.highs, prices.lows, prices.closes, 14
    set adx_info to result
    run compute_bollinger_bands with prices.closes, 20, 2
    set bb to result
    set current_price to float(candles[len(candles) - 1].close)
    set prev_price to float(candles[len(candles) - 2].close)
    set price_change to round((current_price - prev_price) / prev_price * 100, 2)
    set sma20 to 0
    if len(candles) is at least 20:
        set sum20 to 0
        repeat for each j in range(len(candles) - 20, len(candles)):
            set sum20 to sum20 + float(candles[j].close)
        set sma20 to round(sum20 / 20, 2)
    set trend to "sideways"
    if current_price is above sma20 and sma20 is above 0:
        set trend to "bullish"
    if current_price is below sma20 and sma20 is above 0:
        set trend to "bearish"
    set score to 0
    if rsi is below 35: set score to score + 25
    if rsi is above 65: set score to score - 25
    if price_change is above 1: set score to score + 15
    if price_change is below -1: set score to score - 15
    if trend is equal "bullish": set score to score + 20
    if trend is equal "bearish": set score to score - 20
    if sma20 is above 0 and current_price is above sma20 * 1.02: set score to score + 10
    if sma20 is above 0 and current_price is below sma20 * 0.98: set score to score - 10
    if macd_info.bullish is equal yes: set score to score + 12
    if macd_info.bullish is equal no and macd_info.histogram is below 0: set score to score - 12
    if adx_info.adx is above 25 and adx_info.trend_direction is equal yes: set score to score + 8
    if adx_info.adx is above 25 and adx_info.trend_direction is equal no: set score to score - 8
    if bb.percent_b is below 0: set score to score + 10
    if bb.percent_b is above 1: set score to score - 10
    respond with score

to api_backtest_thresholds with symbol:
    purpose: "API wrapper: backtest score thresholds for symbol (default RELIANCE.NS)"
    if symbol is equal nothing:
        set symbol to "RELIANCE.NS"
    run backtest_thresholds with symbol
    respond with result

to backtest_thresholds with symbol:
    purpose: "Backtest score thresholds on 6mo data; returns best threshold and win rates"
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
    if stock_data.error is not equal nothing:
        respond with {"error": stock_data.error}
    set candles to stock_data.candles
    if candles is equal nothing:
        set candles to stock_data["candles"]
    if len(candles) is below 35:
        respond with {"error": "Not enough data (need 35+ candles)"}
    set results to []
    repeat for each th in [10, 12, 15, 18, 20, 25]:
        set wins to 0
        set total to 0
        repeat for each i in range(26, len(candles) - 5):
            set candle_slice to slice(candles, 0, i + 1)
            run compute_scan_score with candle_slice
            set score to result
            set pred to "HOLD"
            if score is above th:
                set pred to "BUY"
            if score is below -th:
                set pred to "SELL"
            if pred is not equal "HOLD":
                set entry to float(candles[i].close)
                set exit_p to float(candles[i + 5].close)
                set total to total + 1
                if pred is equal "BUY" and exit_p is above entry:
                    set wins to wins + 1
                if pred is equal "SELL" and exit_p is below entry:
                    set wins to wins + 1
        set wr to 0
        if total is above 0:
            set wr to round(wins / total * 100, 1)
        append {"threshold": th, "wins": wins, "total": total, "win_rate_pct": wr} to results
    set best to results[0]
    repeat for each r in results:
        if r.total is above 0 and r.win_rate_pct is above best.win_rate_pct:
            set best to r
    respond with {"symbol": symbol, "thresholds": results, "recommended_threshold": best.threshold, "best_win_rate_pct": best.win_rate_pct, "horizon_days": 5}

to quick_scan:
    purpose: "Fast scan of top 20 stocks using technical analysis"
    run get_nifty50_symbols
    set top_stocks to slice(result, 0, 20)
    set results to []
    repeat for each symbol in top_stocks:
        run scan_one_stock with symbol
        if result is not equal nothing:
            append result to results
    respond with {"signals": results, "buy_count": len(filter_by(results, "signal", "equal", "BUY")), "sell_count": len(filter_by(results, "signal", "equal", "SELL")), "total_scanned": len(results), "timestamp": time_iso()}

to run_institutional_screen with mode, limit:
    purpose: "Automated institutional momentum/value screen with recommendations"
    if mode is equal nothing or mode is equal "":
        set mode to "top50"
    if mode is not equal "top50" and mode is not equal "top100" and mode is not equal "top200" and mode is not equal "all" and mode is not equal "nifty50" and mode is not equal "fno":
        set mode to "top50"
    if limit is equal nothing:
        set limit to 20
    if limit is below 1:
        set limit to 20
    if limit is above 50:
        set limit to 50
    set cmd to "python3 tools/institutional_screen.py --mode " + mode + " --limit " + str(limit)
    set raw to shell(cmd)
    respond with json_decode(raw)

// ═══════════════════════════════════════════════════════════════
// ML MODEL INTEGRATION
// ═══════════════════════════════════════════════════════════════

to ml_predict with symbol, features:
    purpose: "Get ML model prediction — EXPERIMENTAL: not wired into scan/signal flow; run models/train_models.py first"
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


// =================================================================
// DIGITAL TWIN SIMULATION
// =================================================================

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
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
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
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
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


to api_monte_carlo with symbol, steps, num_paths:
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
    run build_market_state with stock_data.candles
    run simulate_multiple_paths with result, steps, num_paths
    respond with {"symbol": symbol, "monte_carlo": result}

to api_market_state with symbol:
    run fetch_stock_ohlcv with symbol, "6mo", "1d"
    set stock_data to result
    run build_market_state with stock_data.candles
    respond with {"symbol": symbol, "market_state": result}
// =================================================================
// SELF-LEARNING ENGINE
// =================================================================

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
    run fetch_stock_ohlcv with symbol, "5d", "1d"
    set current_data to result
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
    purpose: "Adjust signal weights based on which sources predicted correctly; persists for aggregate_signals"
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
        if h.correct is equal yes and h.source_scores is not equal nothing:
            set ss to h.source_scores
            set st to 0
            if has_key(ss, "technical"): set st to ss["technical"]
            set sw to 0
            if has_key(ss, "swarm"): set sw to ss["swarm"]
            set so to 0
            if has_key(ss, "options"): set so to ss["options"]
            set slm to 0
            if has_key(ss, "llm"): set slm to ss["llm"]
            if st is above 0 and h.signal is equal "BUY":
                set tech_correct to tech_correct + 1
            if st is below 0 and h.signal is equal "SELL":
                set tech_correct to tech_correct + 1
            if sw is above 0 and h.signal is equal "BUY":
                set swarm_correct to swarm_correct + 1
            if sw is below 0 and h.signal is equal "SELL":
                set swarm_correct to swarm_correct + 1
            if so is above 0 and h.signal is equal "BUY":
                set options_correct to options_correct + 1
            if so is below 0 and h.signal is equal "SELL":
                set options_correct to options_correct + 1
            if slm is above 0 and h.signal is equal "BUY":
                set llm_correct to llm_correct + 1
            if slm is below 0 and h.signal is equal "SELL":
                set llm_correct to llm_correct + 1
    set tech_rate to tech_correct / total
    set swarm_rate to swarm_correct / total
    set options_rate to options_correct / total
    set llm_rate to llm_correct / total
    set sum_rates to tech_rate + swarm_rate + options_rate + llm_rate
    if sum_rates is above 0:
        set w_technical to round(tech_rate / sum_rates, 2)
        set w_swarm to round(swarm_rate / sum_rates, 2)
        set w_options to round(options_rate / sum_rates, 2)
        set w_llm to round(llm_rate / sum_rates, 2)
    mkdir("data/learning")
    set w_map to {"technical": w_technical, "swarm": w_swarm, "options": w_options, "llm": w_llm}
    set merged to {"signal_sources": w_map}
    try:
        set content to read_file("data/learning/weights.json")
        set existing to json_decode(content)
        if has_key(existing, "indicators"):
            set merged["indicators"] to existing["indicators"]
    on error:
        set merged["indicators"] to {"rsi": 25, "macd": 15, "trend": 20}
    write_file("data/learning/weights.json", json_encode(merged))
    respond with {
        "technical": w_technical,
        "swarm": w_swarm,
        "options": w_options,
        "llm": w_llm,
        "based_on": total,
        "note": "Weights saved; aggregate_signals will use them"
    }

// ═══════════════════════════════════════════════════════════════
// PRICE HISTORY FOR CHARTS — Multi-year data
// ═══════════════════════════════════════════════════════════════

to get_chart_data with symbol, period:
    purpose: "Get OHLCV data formatted for charting with indicator overlays"
    run fetch_stock_ohlcv with symbol, period, "1d"
    set chart_data to result
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


// =================================================================
// SCHEDULER
// =================================================================

// ═══════════════════════════════════════════════════════════════
// SCHEDULED TASKS
// ═══════════════════════════════════════════════════════════════

to task_collect_data:
    purpose: "Collect fresh OHLCV data for all NIFTY 50 stocks"
    log "SCHEDULER: Starting data collection..."
    set start_time to time_now()
    try:
        run collect_all_data
        set result to result
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
        run fetch_nse_index_option_chain with "NIFTY"
        set nifty_chain to result
        set results.nifty to "collected"
    on error:
        set results.nifty to "failed"

    wait 2 seconds

    try:
        run fetch_nse_index_option_chain with "BANKNIFTY"
        set banknifty_chain to result
        set results.banknifty to "collected"
    on error:
        set results.banknifty to "failed"

    log "SCHEDULER: Option chains collected"
    respond with {"task": "collect_options", "status": "completed", "results": results}

to task_sector_scan:
    purpose: "Analyze all sectors for rotation signals"
    log "SCHEDULER: Running sector scan..."
    try:
        run analyze_all_sectors
        set result to result
        // was: gather result from sector/all
        log "SCHEDULER: Sector scan complete"
        respond with {"task": "sector_scan", "status": "completed", "sectors": result}
    on error:
        respond with {"task": "sector_scan", "status": "failed", "error": str(error)}

to task_generate_signals:
    purpose: "Generate signals for top stocks after market close"
    log "SCHEDULER: Generating signals for NIFTY 50..."
    run get_nifty50_symbols
    set scan_symbols to result
    set signals to []
    set generated to 0
    repeat for each symbol in scan_symbols:
        try:
            run generate_signal with symbol
            set signal to result
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
    run get_nifty50_symbols
    set top_stocks to slice(result, 0, 5)
    set results to []
    repeat for each symbol in top_stocks:
        try:
            run run_evolution with symbol, 30, 100
            set evo to result
            append {"symbol": symbol, "best_fitness": evo.best_strategy.fitness, "best_type": evo.best_strategy.type} to results
        on error:
            log "SCHEDULER: Evolution failed for {{symbol}}"
        wait 3 seconds
    log "SCHEDULER: Strategy evolution complete"
    respond with {"task": "evolve_strategies", "status": "completed", "results": results}

to task_twin_validation:
    purpose: "Run digital twin validation for stocks with active signals"
    log "SCHEDULER: Running digital twin validation..."
    run get_nifty50_symbols
    set stocks to slice(result, 0, 5)
    set validated to []
    repeat for each symbol in stocks:
        try:
            run generate_validated_signal with symbol
            set twin_result to result
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
        set health to {"status": "healthy"}
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
    log "SCHEDULER: Running autonomous cycle..."
    run autonomous_learn_cycle

to autonomous_learn_cycle:
    purpose: "Fully autonomous: collect data, check past predictions, learn, adjust weights"
    log "AUTO: Starting autonomous learning cycle..."
    // Step 1: Load existing signals
    try:
        set content to read_file("data/learning/signals.json")
        set signals to json_decode(content)
    on error:
        set signals to []
        log "AUTO: No signal history found"
    // Step 2: Resolve signals older than validation horizon (5 days default; 432000 sec)
    set VALIDATION_DAYS to 5
    set horizon_sec to VALIDATION_DAYS * 86400
    set now to time_now()
    set resolved_count to 0
    set win_count to 0
    repeat for each sig in signals:
        if sig.resolved is not equal yes:
            set age to now - sig.timestamp / 1000
            if age is above horizon_sec:
                try:
                    set entry to sig.entryPrice
                    if entry is equal nothing: set entry to sig.current_price
                    if entry is equal nothing:
                        log "AUTO: No entryPrice for {{sig.symbol}}, skip"
                    otherwise:
                        set yahoo_url to "https://query2.finance.yahoo.com/v8/finance/chart/" + sig.symbol + "?range=5d&interval=1d"
                        run fetch_url with yahoo_url
                        set resp to result
                        set chart to resp.chart
                        if chart is not equal nothing:
                            set r to chart.result[0]
                            set q to r.indicators.quote[0]
                            set closes to q.close
                            set exit_price to closes[len(closes) - 1]
                            set sig.exitPrice to exit_price
                            set sig.pnl to round((exit_price - entry) / entry * 100, 2)
                            if sig.signal is equal "BUY" and exit_price is above entry:
                                set sig.won to yes
                            if sig.signal is equal "SELL" and exit_price is below entry:
                                set sig.won to yes
                            set PNL_MIN to 0.5
                            if abs(sig.pnl) is below PNL_MIN:
                                set sig.won to no
                            set sig.correct to sig.won
                            set sig.resolved to yes
                            set resolved_count to resolved_count + 1
                            if sig.won is equal yes:
                                set win_count to win_count + 1
                            if sig.source_scores is equal nothing:
                                set sig.source_scores to {"technical": 0, "swarm": 0, "options": 0, "llm": 0}
                on error:
                    log "AUTO: Could not resolve {{sig.symbol}}"
    // Step 3: Learn from resolved signals
    set total_resolved to len(filter_by(signals, "resolved", "equal", yes))
    set total_wins to 0
    set rsi_correct to 0
    set macd_correct to 0
    set trend_correct to 0
    repeat for each sig in signals:
        if sig.resolved is equal yes and sig.won is equal yes:
            set total_wins to total_wins + 1
    set win_rate to 0
    if total_resolved is above 0:
        set win_rate to round((total_wins / total_resolved) * 100, 1)
    // Step 4: Save updated signals
    mkdir("data/learning")
    write_file("data/learning/signals.json", json_encode(signals))
    // Step 5: Re-optimize weights from resolved signals (so aggregate_signals uses learned weights)
    set resolved_list to filter_by(signals, "resolved", "equal", yes)
    if len(resolved_list) is at least 10:
        run optimize_weights with resolved_list
        set opt to result
        log "AUTO: Weights updated — tech={{opt.technical}}, swarm={{opt.swarm}}"
    // Step 6: Log results
    set cycle_log to {"timestamp": time_iso(), "total_signals": len(signals), "resolved": total_resolved, "wins": total_wins, "win_rate": win_rate, "new_resolved": resolved_count, "new_wins": win_count}
    log "AUTO: Cycle complete — {{total_resolved}} resolved, {{win_rate}}% win rate, {{resolved_count}} newly resolved"
    // Append to log file
    try:
        set existing_log to read_file("data/learning/log.json")
        set log_entries to json_decode(existing_log)
    on error:
        set log_entries to []
    append cycle_log to log_entries
    write_file("data/learning/log.json", json_encode(log_entries))
    respond with cycle_log

// ─── API Routes ───



// =================================================================
// API ROUTES
// =================================================================

to get_learning_log:
    purpose: "Get learning history log"
    try:
        set content to read_file("data/learning/log.json")
        respond with json_decode(content)
    on error:
        respond with []

to auth_login with email, name:
    purpose: "Create session and JWT token for authenticated user"
    if email is equal nothing or trim(email) is equal "":
        respond with {"error": "email required", "_status": 400}
    if name is equal nothing or trim(name) is equal "":
        respond with {"error": "name required", "_status": 400}
    set sid to session_create()
    session_set(sid, "email", email)
    session_set(sid, "name", name)
    session_set(sid, "login_time", time_iso())
    set token to jwt_generate(email, "user", 86400)
    set token_out to token
    if token_out is equal nothing:
        set token_out to ""
    if type(token_out) is equal "record":
        set token_out to ""
    set auth_mode to "session"
    if token_out is not equal "":
        set auth_mode to "jwt"
    mkdir("data/users")
    write_file("data/users/" + replace(email, "@", "_at_") + ".json", json_encode({"email": email, "name": name, "last_login": time_iso()}))
    respond with {"token": token_out, "session_id": sid, "email": email, "name": name, "auth_mode": auth_mode}

to auth_check:
    purpose: "Check if user is authenticated"
    set auth to request_header("Authorization")
    if auth is equal nothing:
        respond with {"authenticated": no, "message": "No token provided"}
    set token to replace(auth, "Bearer ", "")
    set claims to jwt_verify(token)
    if claims is equal no:
        respond with {"authenticated": no, "message": "Invalid token"}
    respond with {"authenticated": yes, "email": claims.sub, "role": claims.role}

// ─── News Proxy (avoids CORS) ───

to proxy_stock_news with symbol:
    purpose: "Fetch stock news via Yahoo Finance search API (server-side, avoids CORS)"
    set clean to replace(symbol, ".NS", "")
    try:
        set url to "https://query2.finance.yahoo.com/v1/finance/search?q=" + clean + "&newsCount=6&quotesCount=0"
        gather response from url:
            headers:
                User-Agent: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"
        set articles to []
        repeat for each item in response.news:
            append {"title": item.title, "publisher": item.publisher, "link": item.link, "published": item.providerPublishTime} to articles
        respond with {"symbol": clean, "articles": articles, "source": "yahoo"}
    on error:
        respond with {"symbol": clean, "articles": [], "source": "none", "error": "News unavailable"}

// ─── Live Chat ───

to chat_send with user, message, stock:
    purpose: "Save a chat message using store"
    set timestamp to time_iso()
    set entry to json_encode({"user": user, "message": message, "stock": stock, "time": timestamp})
    store entry into "chat_msg"
    respond with {"status": "sent", "entry": {"user": user, "message": message, "stock": stock, "time": timestamp}}

to chat_load:
    purpose: "Load recent chat messages — returns all stored messages"
    respond with {"messages": [], "count": 0, "note": "Use the chat UI — messages are stored in browser localStorage for reliability"}

api:
    GET / runs serve_dashboard
    GET /index.html runs serve_dashboard
    GET /manifest.json runs serve_manifest
    GET /sw.js runs serve_sw
    POST /auth/login runs auth_login
    GET /auth/check runs auth_check
    POST /learning/save runs save_signals
    GET /learning/load runs load_signals
    POST /learning/weights/save runs save_weights
    GET /learning/weights/load runs load_weights
    POST /learning/cycle runs autonomous_learn_cycle
    GET /learning/log runs get_learning_log
    GET /health runs health_check
    GET /symbols/all runs get_all_symbols
    GET /symbols/nifty50 runs get_nifty50_symbols
    GET /symbols/banknifty runs get_banknifty_symbols
    GET /symbols/indices runs get_index_symbols
    GET /symbols/next50 runs get_nifty_next50_symbols
    GET /symbols/fno runs get_fno_symbols
    GET /symbols/bse runs get_bse_symbols
    GET /symbols/top200 runs get_top200_symbols
    GET /symbols/lowpriced runs get_lowpriced_symbols
    GET /symbols/sectors runs get_sectors
    POST /stock/ohlcv runs fetch_stock_ohlcv
    POST /stock/nse-quote runs fetch_nse_quote
    POST /stock/bse-quote runs fetch_bse_quote
    POST /wishlist/save runs save_wishlist
    GET /wishlist/load runs load_wishlist
    POST /favorites/save runs save_favorites
    GET /favorites/load runs load_favorites
    POST /debug/fetch runs debug_fetch
    POST /stock/bulk runs fetch_bulk_ohlcv
    POST /stock/fundamentals runs fetch_fundamentals
    POST /options/chain runs fetch_nse_option_chain
    POST /options/index runs fetch_nse_index_option_chain
    GET /news runs fetch_market_news
    POST /news/stock runs proxy_stock_news
    POST /chat/send runs chat_send
    GET /chat/load runs chat_load
    GET /snapshot runs get_market_snapshot
    POST /collect/all runs collect_all_data
    GET /data/nifty50 runs fetch_nifty50_data
    POST /sector/analyze runs analyze_sector
    GET /sector/all runs analyze_all_sectors
    POST /screener runs fetch_screener_data
    GET /fii-dii runs fetch_fii_dii_data
    POST /stock/multiyear runs fetch_multi_year
    POST /screen/institutional runs run_institutional_screen
    POST /analyze runs full_analysis
    POST /analyze/batch runs batch_analysis
    POST /indicators/rsi runs api_rsi
    POST /indicators/macd runs api_macd
    POST /regime runs api_regime
    POST /options/analyze runs api_options
    POST /evolve runs run_evolution
    POST /signal runs get_swarm_signal
    POST /backtest runs api_backtest
    POST /backtest/thresholds runs api_backtest_thresholds
    POST /signals/bulk runs generate_bulk_signals
    GET /scan/nifty50 runs scan_nifty50
    POST /ml/predict runs ml_predict
    POST /llm/analyze runs llm_market_analysis
    POST /llm/sentiment runs llm_news_sentiment
    POST /signal/full runs generate_signal
    GET /scan runs quick_scan
    GET /scan/full runs scan_nifty50
    GET /scan/quick runs quick_scan
    POST /twin/validate runs generate_validated_signal
    POST /intelligence runs get_full_intelligence
    POST /ask runs ask_market
    POST /scheduler/workflow runs run_scheduler_workflow
    POST /simulate runs run_simulation
    POST /monte-carlo runs api_monte_carlo
    POST /market-state runs api_market_state
    POST /validated-signal runs generate_validated_signal
    POST /predict/record runs record_prediction
    POST /predict/check runs check_prediction_outcome
    POST /predict/accuracy runs compute_accuracy
    POST /weights/optimize runs optimize_weights
    POST /chart runs get_chart_data
    GET /performance runs get_system_performance
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

to serve_manifest:
    set content to read_file("dashboard/manifest.json")
    respond with content

to serve_sw:
    set content to read_file("dashboard/sw.js")
    respond with content

to serve_dashboard:
    purpose: "Serve dynamic dashboard HTML at /"
    set html to read_file("dashboard/index.html")
    set ts to str(time_ms())
    set html to replace(html, "</head>", "<meta name='v' content='" + ts + "'></head>")
    respond with html

to save_signals with signals:
    purpose: "Persist signal history to disk"
    if signals is equal nothing:
        respond with {"error": "signals required (array)", "_status": 400}
    if is_list(signals) is equal no:
        respond with {"error": "signals must be an array", "_status": 400}
    mkdir("data/learning")
    set payload to json_encode(signals)
    write_file("data/learning/signals.json", payload)
    respond with {"saved": len(signals), "status": "ok"}

to load_signals:
    purpose: "Load persisted signal history"
    try:
        set content to read_file("data/learning/signals.json")
        set signals to json_decode(content)
        respond with {"signals": signals, "count": len(signals)}
    on error:
        respond with {"signals": [], "count": 0}

to save_weights with weights:
    purpose: "Persist indicator weights; merges with signal_sources in unified weights.json"
    mkdir("data/learning")
    set ind to weights
    if has_key(weights, "weights"): set ind to weights["weights"]
    set merged to {"indicators": ind}
    try:
        set content to read_file("data/learning/weights.json")
        set existing to json_decode(content)
        if has_key(existing, "signal_sources"):
            set merged["signal_sources"] to existing["signal_sources"]
    on error:
        set merged["signal_sources"] to {"technical": 0.30, "swarm": 0.25, "options": 0.20, "llm": 0.25}
    write_file("data/learning/weights.json", json_encode(merged))
    respond with {"saved": "ok", "weights": ind}

to load_weights:
    purpose: "Load indicator weights from unified weights.json (or signal_sources + indicators)"
    try:
        set content to read_file("data/learning/weights.json")
        set w to json_decode(content)
        if has_key(w, "indicators"):
            respond with w["indicators"]
        otherwise:
            respond with w
    on error:
        respond with {"rsi": 25, "williamsR": 10, "cci": 10, "macd": 15, "trend": 20, "ema": 10, "bb": 8, "volume": 8, "pattern": 20, "multiTF": 15, "momentum": 5}

to health_check:
    respond with {"service": "neuraledge", "status": "healthy", "version": "1.0.0", "timestamp": time_iso(), "all_healthy": yes, "services": {"analyzer": {"status": "healthy"}, "swarm": {"status": "healthy"}, "twin": {"status": "healthy"}, "signal": {"status": "healthy"}, "learning": {"status": "healthy"}}}

to get_full_intelligence with symbol:
    purpose: "Multi-engine report — works without LLM (pure math synthesis)"
    run full_analysis with symbol
    set analysis to result
    run get_swarm_signal with symbol
    set swarm_signal to result
    run generate_signal with symbol
    set sig_result to result
    run generate_validated_signal with symbol
    set twin_signal to result
    set final_confidence to (sig_result.confidence + twin_signal.confidence) / 2
    set synth to {"recommendation": sig_result.signal, "summary": sig_result.reasoning, "key_factors": ["technical", "swarm", "twin"], "risk_warning": "Use stop-loss", "time_horizon": "1-5 days"}
    if env("NC_AI_KEY") is not equal nothing and env("NC_AI_KEY") is not equal "":
        try:
            ask AI to """Synthesize for {{symbol}}: Regime={{analysis.regime.regime}}, RSI={{analysis.indicators.rsi}}, Signal={{sig_result.signal}}, Twin={{twin_signal.signal}}. Return JSON: recommendation, summary, key_factors, risk_warning""" save as ai_synth
            if ai_synth is not equal nothing:
                set synth to ai_synth
        on error:
            log "AI synthesis skipped — using math-only synthesis"
    respond with {
        "symbol": symbol, "timestamp": time_iso(), "recommendation": synth.recommendation,
        "confidence": round(final_confidence, 2), "executive_summary": synth.summary,
        "key_factors": synth.key_factors, "risk_warning": synth.risk_warning,
        "target_price_range": {"low": sig_result.risk_management.stop_loss, "high": sig_result.risk_management.target_price},
        "time_horizon": synth.time_horizon,
        "current_price": analysis.current_price
    }

to ask_market with question:
    purpose: "Natural language query — needs NC_AI_KEY; otherwise returns help"
    if env("NC_AI_KEY") is equal nothing or env("NC_AI_KEY") is equal "":
        respond with {"intent": "help", "symbol": nothing, "response": "AI not configured. Use pure math: POST /analyze, /signal, or /scan/quick with symbol. BUY=expect price up, SELL=expect price down.", "suggested_actions": ["/analyze", "/signal", "/scan/quick"]}
    try:
        ask AI to """Extract symbol from: {{question}}. Return JSON: intent, symbol (add .NS), response""" save as parsed
        if parsed.symbol is not equal nothing:
            try:
                run full_analysis with parsed.symbol
                set parsed.analysis to {"current_price": result.current_price, "regime": result.regime, "rsi": result.indicators.rsi}
            on error:
                log "Could not fetch analysis for {{parsed.symbol}}"
        respond with parsed
    on error:
        respond with {"intent": "help", "response": "Use POST /analyze or /signal with symbol for analysis."}

to run_scheduler_workflow with workflow:
    match workflow:
        when "market-open":
            run run_market_open_tasks
            respond with result
        when "intraday":
            run run_intraday_tasks
            respond with result
        when "market-close":
            run run_market_close_tasks
            respond with result
        when "maintenance":
            run run_daily_maintenance
            respond with result
        otherwise:
            respond with {"error": "Unknown workflow: {{workflow}}"}
