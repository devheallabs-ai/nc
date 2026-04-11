service "nc-debug"
version "1.0.0"

configure:
    port: 8200

to test_inline_list:
    set vals to [10, 20, 30, 40, 50]
    set avg to average(vals)
    set first to vals[0]
    set last to vals[4]
    set s to 0
    repeat for each v in vals:
        set s to s + v
    respond with {
        "vals": vals,
        "len": len(vals),
        "first": first,
        "last": last,
        "avg_builtin": avg,
        "manual_sum": s,
        "manual_avg": s / 5,
        "type_first": type(first),
        "type_avg": type(avg)
    }

to test_subtraction:
    set a to 198
    set b to 200
    set diff to a - b
    set abs_diff to abs(diff)
    set diff_float to 198.0 - 200.0
    respond with {
        "a": a,
        "b": b,
        "a_minus_b": diff,
        "abs_a_minus_b": abs_diff,
        "float_diff": diff_float,
        "is_negative": diff is below 0,
        "type_diff": type(diff)
    }

to test_list_from_function:
    run get_test_list
    set vals to result
    set avg to average(vals)
    set first to vals[0]
    respond with {
        "vals": vals,
        "len": len(vals),
        "first": first,
        "avg": avg,
        "type_first": type(first)
    }

to get_test_list:
    respond with [10, 20, 30, 40, 50]

to test_slice_math:
    set vals to [100, 110, 120, 130, 140]
    set window to slice(vals, 2, 5)
    set avg to average(window)
    set first_w to window[0]
    set diff to window[1] - window[0]
    respond with {
        "original": vals,
        "sliced": window,
        "sliced_len": len(window),
        "sliced_first": first_w,
        "sliced_avg": avg,
        "diff_1_0": diff,
        "type_sliced_first": type(first_w)
    }

to test_append_math:
    set arr to []
    append 100 to arr
    append 200 to arr
    append 300 to arr
    set avg to average(arr)
    set diff to arr[1] - arr[0]
    respond with {
        "arr": arr,
        "len": len(arr),
        "avg": avg,
        "diff_1_0": diff,
        "type_0": type(arr[0])
    }

to test_abs_negative:
    set x to 5 - 10
    set y to abs(x)
    set z to abs(-3)
    set w to abs(0 - 7)
    respond with {
        "5_minus_10": x,
        "abs_of_it": y,
        "abs_neg3": z,
        "abs_0_minus_7": w,
        "type_x": type(x)
    }

to test_stochastic_debug:
    set highs to [102, 103, 105, 104, 107, 109, 108, 111, 113, 112, 115, 117, 116, 119, 121]
    set lows to [98, 99, 101, 100, 103, 105, 104, 107, 109, 108, 111, 113, 112, 115, 117]
    set closes to [100, 101, 103, 102, 105, 107, 106, 109, 111, 110, 113, 115, 114, 117, 119]
    set k_period to 14
    set rh to slice(highs, len(highs) - k_period, len(highs))
    set rl to slice(lows, len(lows) - k_period, len(lows))
    set highest to max(rh)
    set lowest to min(rl)
    set current to closes[len(closes) - 1]
    set range to highest - lowest
    set k to 50
    if range is above 0:
        set k to ((current - lowest) / range) * 100
    respond with {
        "highs_len": len(highs),
        "sliced_highs": rh,
        "sliced_lows": rl,
        "highest": highest,
        "lowest": lowest,
        "current": current,
        "range": range,
        "k": k,
        "type_highest": type(highest),
        "type_current": type(current)
    }

api:
    GET /health runs health
    GET /test/inline-list runs test_inline_list
    GET /test/subtraction runs test_subtraction
    GET /test/function-list runs test_list_from_function
    GET /test/slice-math runs test_slice_math
    GET /test/append-math runs test_append_math
    GET /test/abs-negative runs test_abs_negative
    GET /test/stochastic runs test_stochastic_debug

to health:
    respond with {"status": "ok"}
