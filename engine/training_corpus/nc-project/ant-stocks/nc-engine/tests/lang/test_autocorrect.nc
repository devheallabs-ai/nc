// Test: Auto-correct — NC's built-in intelligence
// Verifies that NC auto-corrects close typos and returns correct values

service "test-autocorrect"
version "1.0.0"

// --- Variable auto-correction ---

to test variable typo swap:
    set name to "Alice"
    respond with nme

to test variable typo extra char:
    set count to 42
    respond with countt

to test variable typo missing char:
    set items to [1, 2, 3]
    respond with len(item)

to test variable case:
    set Total to 100
    respond with total

// --- Function auto-correction ---

to test function typo swap:
    set items to [1, 2, 3]
    respond with lne(items)

to test function typo extra char:
    set data to [3, 1, 2]
    respond with sortt(data)

to test function upper typo:
    respond with uper("hello")

to test function lower typo:
    respond with lwoer("HELLO")

to test function print typo:
    set x to "world"
    pritn(x)
    respond with x

to test function trim typo:
    respond with tirm("  hi  ")

to test function replace typo:
    respond with repalce("hello world", "world", "NC")

// --- Correct code still works (no false corrections) ---

to test correct variable:
    set value to 999
    respond with value

to test correct function:
    respond with len([1, 2, 3, 4, 5])

to test correct upper:
    respond with upper("test")

to test correct math:
    respond with abs(-7) + sqrt(16)
