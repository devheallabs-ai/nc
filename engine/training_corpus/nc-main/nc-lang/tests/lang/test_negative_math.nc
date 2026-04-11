// Test: Negative numbers, subtraction producing negatives, abs(), float precision
// Covers: Bug fixes 1 and 4 from C tests — negative subtraction, abs() reliability,
//         RSI loss capture pattern, ATR pattern, average(), sum(), filter_by()
// Mirrors C test sections: Negative Subtraction, abs() on Negative Numbers,
//   average() on Inline Lists, filter_by, string equality in loops

service "test-negative-math"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// Negative Subtraction
// ═══════════════════════════════════════════════════════════

to test subtraction producing negative:
    set a to 198
    set b to 200
    set result to a - b
    if result is equal to -2:
        respond with "pass"

to test float subtraction producing negative:
    set a to 198.0
    set b to 200.0
    set result to a - b
    if result is below 0:
        if abs(result + 2.0) is below 0.01:
            respond with "pass"

to test negative result in further arithmetic:
    set a to 5
    set b to 10
    set diff to a - b
    set doubled to diff * 2
    if doubled is equal to -10:
        respond with "pass"

to test chain of negative operations:
    set x to 3
    set y to 10
    set z to x - y
    set w to z - 5
    // z = -7, w = -12
    if w is equal to -12:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// abs() Function
// ═══════════════════════════════════════════════════════════

to test abs negative integer:
    set result to abs(-5)
    if result is equal to 5:
        respond with "pass"

to test abs negative variable:
    set x to -42
    set result to abs(x)
    if result is equal to 42:
        respond with "pass"

to test abs zero:
    set result to abs(0)
    if result is equal to 0:
        respond with "pass"

to test abs positive unchanged:
    set result to abs(7)
    if result is equal to 7:
        respond with "pass"

to test abs negative float:
    set result to abs(-3.14)
    if result is above 3.13 and result is below 3.15:
        respond with "pass"

to test abs of subtraction result:
    // ATR pattern: abs(low - prev_close)
    set high to 105.0
    set low to 98.0
    set prev_close to 100.0
    set tr1 to high - low
    set tr2 to abs(high - prev_close)
    set tr3 to abs(low - prev_close)
    if tr3 is above 1.99 and tr3 is below 2.01:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// RSI Loss Capture Pattern
// ═══════════════════════════════════════════════════════════

to test rsi loss capture:
    set prices to [100.0, 102.0, 99.0, 97.0, 101.0]
    set losses to 0.0
    set prev to 100.0
    repeat for each price in prices:
        set change to price - prev
        if change is below 0:
            set losses to losses + abs(change)
        set prev to price
    // Losses: (99-102)=3 + (97-99)=2 = 5.0
    if losses is above 4.99 and losses is below 5.01:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// Float Precision
// ═══════════════════════════════════════════════════════════

to test float addition precision:
    set a to 0.1
    set b to 0.2
    set result to a + b
    // Should be approximately 0.3 (floating point)
    if result is above 0.29 and result is below 0.31:
        respond with "pass"

to test float multiplication:
    set result to 2.5 * 4.0
    if result is equal to 10.0:
        respond with "pass"

to test float division:
    set result to 7.0 / 2.0
    if result is above 3.49 and result is below 3.51:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// average() on Inline Lists
// ═══════════════════════════════════════════════════════════

to test average inline integer list:
    set result to average([10, 20, 30])
    if result is above 19.99 and result is below 20.01:
        respond with "pass"

to test average inline float list:
    set result to average([10.0, 20.0, 30.0])
    if result is above 19.99 and result is below 20.01:
        respond with "pass"

to test average single element:
    set result to average([5])
    if result is above 4.99 and result is below 5.01:
        respond with "pass"

to test average variable list:
    set nums to [100.0, 200.0, 300.0]
    set result to average(nums)
    if result is above 199.99 and result is below 200.01:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// sum() on Inline Lists
// ═══════════════════════════════════════════════════════════

to test sum inline float list:
    set result to sum([10.0, 20.0, 30.0])
    if result is above 59.99 and result is below 60.01:
        respond with "pass"

to test sum inline integer list:
    set result to sum([1, 2, 3, 4, 5])
    if result is equal to 15:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// max() and min() on Lists
// ═══════════════════════════════════════════════════════════

to test max of list:
    set nums to [3, 7, 1, 9, 4]
    set result to max(nums)
    if result is equal to 9:
        respond with "pass"

to test min of list:
    set nums to [3, 7, 1, 9, 4]
    set result to min(nums)
    if result is equal to 1:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// round() with Precision
// ═══════════════════════════════════════════════════════════

to test round with decimals:
    set result to round(3.14159, 2)
    if result is above 3.139 and result is below 3.141:
        respond with "pass"

to test round basic:
    set result to round(2.5)
    if result is equal to 3:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// otherwise if Chains (Bug 2 from C tests)
// ═══════════════════════════════════════════════════════════

to test otherwise if score A:
    set score to 95
    if score is above 90:
        respond with "A"
    otherwise if score is above 80:
        respond with "B"
    otherwise:
        respond with "C"

to test otherwise if score B:
    set score to 85
    if score is above 90:
        respond with "A"
    otherwise if score is above 80:
        respond with "B"
    otherwise:
        respond with "C"

to test otherwise if score C:
    set score to 50
    if score is above 90:
        respond with "A"
    otherwise if score is above 80:
        respond with "B"
    otherwise:
        respond with "C"

to test sector signal classifier:
    set strength to 0.95
    set signal to "neutral"
    if strength is above 0.8:
        set signal to "strong_bullish"
    otherwise if strength is above 0.5:
        set signal to "bullish"
    otherwise if strength is above 0.2:
        set signal to "neutral"
    otherwise:
        set signal to "bearish"
    if signal is equal to "strong_bullish":
        respond with "pass"

to test sector signal mid range:
    set strength to 0.6
    set signal to "neutral"
    if strength is above 0.8:
        set signal to "strong_bullish"
    otherwise if strength is above 0.5:
        set signal to "bullish"
    otherwise if strength is above 0.2:
        set signal to "neutral"
    otherwise:
        set signal to "bearish"
    if signal is equal to "bullish":
        respond with "pass"

to test sector signal low:
    set strength to 0.1
    set signal to "neutral"
    if strength is above 0.8:
        set signal to "strong_bullish"
    otherwise if strength is above 0.5:
        set signal to "bullish"
    otherwise if strength is above 0.2:
        set signal to "neutral"
    otherwise:
        set signal to "bearish"
    if signal is equal to "bearish":
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// respond with Early Exit (Bug 3 from C tests)
// ═══════════════════════════════════════════════════════════

to test respond early exit:
    set x to -5
    if x is below 0:
        respond with "negative"
    respond with "positive"

to test respond in otherwise if chain:
    set code to 404
    if code is equal to 200:
        respond with "ok"
    otherwise if code is equal to 404:
        respond with "not_found"
    otherwise if code is equal to 500:
        respond with "server_error"
    respond with "unknown"

// ═══════════════════════════════════════════════════════════
// filter_by with String Equality (Bug 6 from C tests)
// ═══════════════════════════════════════════════════════════

to test filter by string equal:
    set items to [
        {"name": "a", "status": "PASS"},
        {"name": "b", "status": "FAIL"},
        {"name": "c", "status": "PASS"}
    ]
    set passed to filter_by(items, "status", "equal", "PASS")
    if len(passed) is equal to 2:
        respond with "pass"

to test filter by no matches:
    set items to [
        {"name": "a", "status": "PASS"},
        {"name": "b", "status": "PASS"}
    ]
    set failed to filter_by(items, "status", "equal", "FAIL")
    if len(failed) is equal to 0:
        respond with "pass"

// ═══════════════════════════════════════════════════════════
// String Equality in Loops (Bug 7 from C tests)
// ═══════════════════════════════════════════════════════════

to test string equal in loop:
    set items to [
        {"name": "a", "status": "PASS"},
        {"name": "b", "status": "FAIL"},
        {"name": "c", "status": "PASS"}
    ]
    set count to 0
    repeat for each item in items:
        if item.status is equal to "PASS":
            set count to count + 1
    if count is equal to 2:
        respond with "pass"

to test string equal in flat list loop:
    set items to ["red", "blue", "red", "green"]
    set count to 0
    repeat for each item in items:
        if item is equal to "red":
            set count to count + 1
    if count is equal to 2:
        respond with "pass"

to test string not equal in loop:
    set items to ["red", "blue", "red", "green"]
    set count to 0
    repeat for each item in items:
        if item is not equal to "red":
            set count to count + 1
    if count is equal to 2:
        respond with "pass"

to test none not equal to string:
    set x to nothing
    if x is equal to "hello":
        respond with "matched"
    respond with "not_matched"
