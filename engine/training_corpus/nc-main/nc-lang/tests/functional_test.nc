service "nc-functional-tests"
version "1.0.0"

to test_all:
    set p to 0
    set f to 0
    set total to 53

// ── Types ──
if type("hello") is equal "text": set p to p + 1
if type(42) is equal "number": set p to p + 1
if type(3.14) is equal "number": set p to p + 1
if type(yes) is equal "yesno": set p to p + 1
if type([1, 2]) is equal "list": set p to p + 1
if type(nothing) is equal "nothing": set p to p + 1
if str(42) is equal "42": set p to p + 1
if is_text("hi"): set p to p + 1
if is_number(42): set p to p + 1
if is_list([1]): set p to p + 1
if int("10") is equal 10: set p to p + 1
log "  Types:       " + str(p) + "/11"

// ── Strings ──
set sp to 0
if upper("hello") is equal "HELLO": set sp to sp + 1
if lower("WORLD") is equal "world": set sp to sp + 1
if trim("  nc  ") is equal "nc": set sp to sp + 1
if len("hello") is equal 5: set sp to sp + 1
if contains("hello world", "world"): set sp to sp + 1
if starts_with("hello", "hel"): set sp to sp + 1
if ends_with("hello", "llo"): set sp to sp + 1
if replace("hello", "l", "r") is equal "herro": set sp to sp + 1
if join(["a", "b", "c"], "-") is equal "a-b-c": set sp to sp + 1
set parts to split("x,y,z", ",")
if len(parts) is equal 3: set sp to sp + 1
set p to p + sp
log "  Strings:     " + str(sp) + "/10"

// ── Lists ──
set lp to 0
set nums to [5, 3, 1, 4, 2]
if len(nums) is equal 5: set lp to lp + 1
if first(nums) is equal 5: set lp to lp + 1
if last(nums) is equal 2: set lp to lp + 1
if sum(nums) is equal 15: set lp to lp + 1
if average(nums) is equal 3: set lp to lp + 1
set uniq to unique([1, 1, 2, 2, 3])
if len(uniq) is equal 3: set lp to lp + 1
set flat to flatten([[1, 2], [3, 4]])
if len(flat) is equal 4: set lp to lp + 1
set rev to reverse([1, 2, 3])
if first(rev) is equal 3: set lp to lp + 1
set p to p + lp
log "  Lists:       " + str(lp) + "/8"

// ── Math ──
set mp to 0
if abs(-42) is equal 42: set mp to mp + 1
if sqrt(16) is equal 4: set mp to mp + 1
if pow(2, 10) is equal 1024: set mp to mp + 1
if min(3, 7) is equal 3: set mp to mp + 1
if max(3, 7) is equal 7: set mp to mp + 1
if round(3.7) is equal 4: set mp to mp + 1
if ceil(3.1) is equal 4: set mp to mp + 1
if floor(3.9) is equal 3: set mp to mp + 1
set p to p + mp
log "  Math:        " + str(mp) + "/8"

// ── Control ──
set cp to 0
if 85 is above 80: set cp to cp + 1
if 85 is below 90: set cp to cp + 1
if 85 is at least 85: set cp to cp + 1
if 85 is at most 85: set cp to cp + 1
set t to 0
repeat for each n in [10, 20, 30]:
    set t to t + n
    if t is equal 60: set cp to cp + 1
    set i to 0
    while i is below 5:
        set i to i + 1
        if i is equal 5: set cp to cp + 1
        set c to 0
        repeat 4 times:
            set c to c + 1
            if c is equal 4: set cp to cp + 1
            set status to "active"
            match status:
                when "active":
                    set m to "online"
                otherwise:
                    set m to "offline"
                    if m is equal "online": set cp to cp + 1
                    set p to p + cp
                    log "  Control:     " + str(cp) + "/8"

// ── Expressions ──
set ep to 0
if "Hello, " + "World!" is equal "Hello, World!": set ep to ep + 1
if 10 + 20 is equal 30: set ep to ep + 1
if 100 - 37 is equal 63: set ep to ep + 1
if 6 * 7 is equal 42: set ep to ep + 1
if 100 / 4 is equal 25: set ep to ep + 1
set p to p + ep
log "  Expressions: " + str(ep) + "/5"

// ── JSON ──
set jp to 0
set data to {"name": "NC", "version": 1}
set encoded to json_encode(data)
if contains(encoded, "NC"): set jp to jp + 1
set raw to "{\"lang\":\"NC\"}"
set parsed to json_decode(raw)
set re to json_encode(parsed)
if contains(re, "NC"): set jp to jp + 1
set p to p + jp
log "  JSON:        " + str(jp) + "/2"

// ── Cache ──
set kp to 0
cache("vm_test", "vm_val")
if cached("vm_test") is equal "vm_val": set kp to kp + 1
set p to p + kp
log "  Cache:       " + str(kp) + "/1"

set f to total - p
log ""
log "  ══════════════════════════════════════"
if f is equal 0:
    log "  ALL " + str(p) + "/" + str(total) + " TESTS PASSED"
otherwise:
    log "  " + str(p) + "/" + str(total) + " passed, " + str(f) + " failed"
    log "  ══════════════════════════════════════"

    respond with {"passed": p, "failed": f, "total": total}
