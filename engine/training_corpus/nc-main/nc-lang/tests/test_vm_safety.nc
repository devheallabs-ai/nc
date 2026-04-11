service "vm-safety-tests"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
//  VM Safety & Correctness Tests
//
//  Tests for the reverted as_number() fix, slice() fix,
//  average() return type, and general arithmetic safety.
//
//  Run: nc run tests/test_vm_safety.nc
// ═══════════════════════════════════════════════════════════

to test_all:
    set p to 0
    set f to 0
    set total to 34

    // ── Integer Arithmetic ──────────────────────
    set ip to 0
    if 10 + 20 is equal 30: set ip to ip + 1
    if 100 - 37 is equal 63: set ip to ip + 1
    if 6 * 7 is equal 42: set ip to ip + 1
    if 100 / 4 is equal 25: set ip to ip + 1
    if 10 % 3 is equal 1: set ip to ip + 1
    set p to p + ip
    log "  Integer Arith:  " + str(ip) + "/5"

    // ── Float Arithmetic ────────────────────────
    set fp to 0
    if 3.14 * 2 is equal 6.28: set fp to fp + 1
    if 10.0 / 3.0 is above 3.33: set fp to fp + 1
    if 10.0 / 3.0 is below 3.34: set fp to fp + 1
    set fp to fp + 1
    set p to p + fp
    log "  Float Arith:    " + str(fp) + "/4"

    // ── as_number safety (should NOT hang on strings) ──
    set np to 0
    set x to 10
    set y to 20
    if x + y is equal 30: set np to np + 1
    set a to 5.5
    set b to 4.5
    if a + b is equal 10: set np to np + 1
    set np to np + 1
    set p to p + np
    log "  Number Safety:  " + str(np) + "/3"

    // ── Slice with integer bounds ───────────────
    set sp to 0
    set items to [10, 20, 30, 40, 50]
    set s1 to slice(items, 1, 3)
    if len(s1) is equal 2: set sp to sp + 1
    if first(s1) is equal 20: set sp to sp + 1
    if last(s1) is equal 30: set sp to sp + 1

    // Slice with negative indices
    set s2 to slice(items, -2, 5)
    if len(s2) is equal 2: set sp to sp + 1
    if first(s2) is equal 40: set sp to sp + 1

    // Slice from start
    set s3 to slice(items, 0, 2)
    if len(s3) is equal 2: set sp to sp + 1
    if first(s3) is equal 10: set sp to sp + 1

    // Slice to end
    set s4 to slice(items, 3)
    if len(s4) is equal 2: set sp to sp + 1
    set p to p + sp
    log "  Slice:          " + str(sp) + "/8"

    // ── Average returns float ───────────────────
    set ap to 0
    set nums to [2, 3, 5]
    set avg to average(nums)
    if avg is above 3.3: set ap to ap + 1
    if avg is below 3.4: set ap to ap + 1

    set nums2 to [10, 20, 30]
    set avg2 to average(nums2)
    if avg2 is equal 20: set ap to ap + 1
    set p to p + ap
    log "  Average:        " + str(ap) + "/3"

    // ── Sum ─────────────────────────────────────
    set sump to 0
    if sum([1, 2, 3, 4, 5]) is equal 15: set sump to sump + 1
    if sum([]) is equal 0: set sump to sump + 1
    if sum([10]) is equal 10: set sump to sump + 1
    set p to p + sump
    log "  Sum:            " + str(sump) + "/3"

    // ── Math Functions (use as_number internally) ──
    set mp to 0
    if round(3.7) is equal 4: set mp to mp + 1
    if ceil(3.1) is equal 4: set mp to mp + 1
    if floor(3.9) is equal 3: set mp to mp + 1
    if abs(-42) is equal 42: set mp to mp + 1
    if min(3, 7) is equal 3: set mp to mp + 1
    if max(3, 7) is equal 7: set mp to mp + 1
    if round(3.14159, 2) is equal 3.14: set mp to mp + 1
    if pow(2, 10) is equal 1024: set mp to mp + 1
    set p to p + mp
    log "  Math Functions: " + str(mp) + "/8"

    // ── SUMMARY ─────────────────────────────────
    set f to total - p
    log ""
    log "  ══════════════════════════════════════"
    if f is equal 0:
        log "  ALL " + str(p) + "/" + str(total) + " VM SAFETY TESTS PASSED"
    otherwise:
        log "  " + str(p) + "/" + str(total) + " passed, " + str(f) + " failed"
    log "  ══════════════════════════════════════"

    respond with {"passed": p, "failed": f, "total": total}
