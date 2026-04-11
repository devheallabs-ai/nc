// ══════════════════════════════════════════════════════════
//  NC v1.0.0 Enhancement Tests — 27 New Functions
// ══════════════════════════════════════════════════════════

set passed to 0
set failed to 0
set total to 111

// ── items(record) ───────────────────────────────────────
set rec to {"name": "Alice", "age": 30}
set pairs to items(rec)
set pair_count to len(pairs)
if pair_count is equal 2:
    set passed to passed + 1
    log "  PASS: items() returns correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: items() expected 2 pairs"

// items returns lists of [key, val]
set first_pair to pairs[0]
if len(first_pair) is equal 2:
    set passed to passed + 1
    log "  PASS: items() pair has 2 elements"
otherwise:
    set failed to failed + 1
    log "  FAIL: items() pair format wrong"

// ── merge(record1, record2) ─────────────────────────────
set a_rec to {"x": 1, "y": 2}
set b_rec to {"y": 3, "z": 4}
set merged to merge(a_rec, b_rec)
if merged["x"] is equal 1:
    set passed to passed + 1
    log "  PASS: merge() keeps first record values"
otherwise:
    set failed to failed + 1
    log "  FAIL: merge() first record"

if merged["y"] is equal 3:
    set passed to passed + 1
    log "  PASS: merge() second record overwrites"
otherwise:
    set failed to failed + 1
    log "  FAIL: merge() overwrite"

if merged["z"] is equal 4:
    set passed to passed + 1
    log "  PASS: merge() adds new keys"
otherwise:
    set failed to failed + 1
    log "  FAIL: merge() new keys"

// ── find(list, key, value) ──────────────────────────────
set users to [{"name": "Alice", "role": "admin"}, {"name": "Bob", "role": "user"}, {"name": "Charlie", "role": "admin"}]
set found to find(users, "name", "Bob")
if found["role"] is equal "user":
    set passed to passed + 1
    log "  PASS: find() locates correct record"
otherwise:
    set failed to failed + 1
    log "  FAIL: find() wrong record"

set not_found to find(users, "name", "David")
if not_found is equal none:
    set passed to passed + 1
    log "  PASS: find() returns none when not found"
otherwise:
    set failed to failed + 1
    log "  FAIL: find() should return none"

// find with integer values
set products to [{"id": 1, "name": "Widget"}, {"id": 2, "name": "Gadget"}]
set p to find(products, "id", 2)
if p["name"] is equal "Gadget":
    set passed to passed + 1
    log "  PASS: find() works with integer values"
otherwise:
    set failed to failed + 1
    log "  FAIL: find() integer values"

// ── group_by(list, field) ───────────────────────────────
set employees to [{"name": "A", "dept": "eng"}, {"name": "B", "dept": "sales"}, {"name": "C", "dept": "eng"}, {"name": "D", "dept": "sales"}]
set grouped to group_by(employees, "dept")
set eng_group to grouped["eng"]
if len(eng_group) is equal 2:
    set passed to passed + 1
    log "  PASS: group_by() correct group size"
otherwise:
    set failed to failed + 1
    log "  FAIL: group_by() group size"

set sales_group to grouped["sales"]
if len(sales_group) is equal 2:
    set passed to passed + 1
    log "  PASS: group_by() second group correct"
otherwise:
    set failed to failed + 1
    log "  FAIL: group_by() second group"

// ── take(list, n) ───────────────────────────────────────
set numbers to [10, 20, 30, 40, 50]
set first3 to take(numbers, 3)
if len(first3) is equal 3:
    set passed to passed + 1
    log "  PASS: take() returns correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: take() count"

if first3[0] is equal 10:
    set passed to passed + 1
    log "  PASS: take() first element correct"
otherwise:
    set failed to failed + 1
    log "  FAIL: take() first element"

if first3[2] is equal 30:
    set passed to passed + 1
    log "  PASS: take() last element correct"
otherwise:
    set failed to failed + 1
    log "  FAIL: take() last element"

// take more than available
set all5 to take(numbers, 10)
if len(all5) is equal 5:
    set passed to passed + 1
    log "  PASS: take() caps at list size"
otherwise:
    set failed to failed + 1
    log "  FAIL: take() cap"

// ── drop(list, n) ───────────────────────────────────────
set last2 to drop(numbers, 3)
if len(last2) is equal 2:
    set passed to passed + 1
    log "  PASS: drop() returns correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: drop() count"

if last2[0] is equal 40:
    set passed to passed + 1
    log "  PASS: drop() first remaining correct"
otherwise:
    set failed to failed + 1
    log "  FAIL: drop() first remaining"

// ── compact(list) ───────────────────────────────────────
set messy to [1, none, 2, none, 3, none]
set clean to compact(messy)
if len(clean) is equal 3:
    set passed to passed + 1
    log "  PASS: compact() removes nones"
otherwise:
    set failed to failed + 1
    log "  FAIL: compact() remove nones"

if clean[0] is equal 1:
    set passed to passed + 1
    log "  PASS: compact() preserves values"
otherwise:
    set failed to failed + 1
    log "  FAIL: compact() values"

if clean[2] is equal 3:
    set passed to passed + 1
    log "  PASS: compact() preserves order"
otherwise:
    set failed to failed + 1
    log "  FAIL: compact() order"

// ── reduce(list, op) ───────────────────────────────────
set nums to [1, 2, 3, 4, 5]
set total_sum to reduce(nums, "+")
if total_sum is equal 15:
    set passed to passed + 1
    log "  PASS: reduce(+) sums correctly"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(+)"

set product to reduce(nums, "*")
if product is equal 120:
    set passed to passed + 1
    log "  PASS: reduce(*) multiplies correctly"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(*)"

set min_val to reduce(nums, "min")
if min_val is equal 1:
    set passed to passed + 1
    log "  PASS: reduce(min) finds minimum"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(min)"

set max_val to reduce(nums, "max")
if max_val is equal 5:
    set passed to passed + 1
    log "  PASS: reduce(max) finds maximum"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(max)"

// reduce with add alias
set add_sum to reduce(nums, "add")
if add_sum is equal 15:
    set passed to passed + 1
    log "  PASS: reduce(add) alias works"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(add) alias"

// reduce strings
set words to ["hello", " ", "world"]
set joined_str to reduce(words, "join")
if joined_str is equal "hello world":
    set passed to passed + 1
    log "  PASS: reduce(join) concatenates strings"
otherwise:
    set failed to failed + 1
    log "  FAIL: reduce(join)"

// ── map(list, field) ────────────────────────────────────
set people to [{"name": "Alice", "age": 30}, {"name": "Bob", "age": 25}]
set names to map(people, "name")
if len(names) is equal 2:
    set passed to passed + 1
    log "  PASS: map() returns correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: map() count"

if names[0] is equal "Alice":
    set passed to passed + 1
    log "  PASS: map() extracts first field"
otherwise:
    set failed to failed + 1
    log "  FAIL: map() first field"

if names[1] is equal "Bob":
    set passed to passed + 1
    log "  PASS: map() extracts second field"
otherwise:
    set failed to failed + 1
    log "  FAIL: map() second field"

// ── title_case(str) ─────────────────────────────────────
set titled to title_case("hello world")
if titled is equal "Hello World":
    set passed to passed + 1
    log "  PASS: title_case() basic"
otherwise:
    set failed to failed + 1
    log "  FAIL: title_case() basic"

set titled2 to title_case("HELLO WORLD")
if titled2 is equal "Hello World":
    set passed to passed + 1
    log "  PASS: title_case() from uppercase"
otherwise:
    set failed to failed + 1
    log "  FAIL: title_case() uppercase"

set titled3 to title_case("the quick brown fox")
if titled3 is equal "The Quick Brown Fox":
    set passed to passed + 1
    log "  PASS: title_case() multi-word"
otherwise:
    set failed to failed + 1
    log "  FAIL: title_case() multi-word"

// ── capitalize(str) ─────────────────────────────────────
set capped to capitalize("hello world")
if capped is equal "Hello world":
    set passed to passed + 1
    log "  PASS: capitalize() basic"
otherwise:
    set failed to failed + 1
    log "  FAIL: capitalize() basic"

set capped2 to capitalize("HELLO")
if capped2 is equal "Hello":
    set passed to passed + 1
    log "  PASS: capitalize() from uppercase"
otherwise:
    set failed to failed + 1
    log "  FAIL: capitalize() uppercase"

// ── pad_left(str, width, fill?) ─────────────────────────
set padded to pad_left("42", 5, "0")
if padded is equal "00042":
    set passed to passed + 1
    log "  PASS: pad_left() with zero fill"
otherwise:
    set failed to failed + 1
    log "  FAIL: pad_left() zero fill"

set padded2 to pad_left("hi", 6)
if padded2 is equal "    hi":
    set passed to passed + 1
    log "  PASS: pad_left() default space fill"
otherwise:
    set failed to failed + 1
    log "  FAIL: pad_left() space fill"

// pad_left when already wide enough
set padded3 to pad_left("hello", 3)
if padded3 is equal "hello":
    set passed to passed + 1
    log "  PASS: pad_left() no pad when wide enough"
otherwise:
    set failed to failed + 1
    log "  FAIL: pad_left() no pad"

// ── pad_right(str, width, fill?) ────────────────────────
set rpadded to pad_right("42", 5, "0")
if rpadded is equal "42000":
    set passed to passed + 1
    log "  PASS: pad_right() with zero fill"
otherwise:
    set failed to failed + 1
    log "  FAIL: pad_right() zero fill"

set rpadded2 to pad_right("hi", 6)
if rpadded2 is equal "hi    ":
    set passed to passed + 1
    log "  PASS: pad_right() default space fill"
otherwise:
    set failed to failed + 1
    log "  FAIL: pad_right() space fill"

// ── char_at(str, index) ─────────────────────────────────
set ch to char_at("Hello", 0)
if ch is equal "H":
    set passed to passed + 1
    log "  PASS: char_at() first char"
otherwise:
    set failed to failed + 1
    log "  FAIL: char_at() first char"

set ch2 to char_at("Hello", 4)
if ch2 is equal "o":
    set passed to passed + 1
    log "  PASS: char_at() last char"
otherwise:
    set failed to failed + 1
    log "  FAIL: char_at() last char"

set ch3 to char_at("Hello", 10)
if ch3 is equal none:
    set passed to passed + 1
    log "  PASS: char_at() out of bounds returns none"
otherwise:
    set failed to failed + 1
    log "  FAIL: char_at() out of bounds"

// ── repeat_string(str, n) ───────────────────────────────
set repeated to repeat_string("ab", 3)
if repeated is equal "ababab":
    set passed to passed + 1
    log "  PASS: repeat_string() basic"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_string() basic"

set repeated2 to repeat_string("*", 5)
if repeated2 is equal "*****":
    set passed to passed + 1
    log "  PASS: repeat_string() single char"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_string() single char"

set repeated3 to repeat_string("x", 0)
if repeated3 is equal "":
    set passed to passed + 1
    log "  PASS: repeat_string() zero times"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_string() zero times"

// ── isinstance(val, type_name) ──────────────────────────
if isinstance("hello", "text") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() text"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() text"

if isinstance("hello", "string") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() string alias"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() string alias"

if isinstance(42, "number") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() number"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() number"

if isinstance(42, "int") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() int"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() int"

if isinstance([1,2,3], "list") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() list"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() list"

if isinstance({"a": 1}, "record") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() record"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() record"

if isinstance({"a": 1}, "map") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() map alias"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() map alias"

if isinstance(true, "bool") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() bool"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() bool"

if isinstance(none, "none") is equal true:
    set passed to passed + 1
    log "  PASS: isinstance() none"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() none"

if isinstance("hello", "number") is equal false:
    set passed to passed + 1
    log "  PASS: isinstance() mismatch returns false"
otherwise:
    set failed to failed + 1
    log "  FAIL: isinstance() mismatch"

// ── is_empty(val) ───────────────────────────────────────
if is_empty("") is equal true:
    set passed to passed + 1
    log "  PASS: is_empty() empty string"
otherwise:
    set failed to failed + 1
    log "  FAIL: is_empty() empty string"

if is_empty("hello") is equal false:
    set passed to passed + 1
    log "  PASS: is_empty() non-empty string"
otherwise:
    set failed to failed + 1
    log "  FAIL: is_empty() non-empty string"

if is_empty([]) is equal true:
    set passed to passed + 1
    log "  PASS: is_empty() empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: is_empty() empty list"

if is_empty([1]) is equal false:
    set passed to passed + 1
    log "  PASS: is_empty() non-empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: is_empty() non-empty list"

if is_empty(none) is equal true:
    set passed to passed + 1
    log "  PASS: is_empty() none"
otherwise:
    set failed to failed + 1
    log "  FAIL: is_empty() none"

// ── clamp(val, lo, hi) ─────────────────────────────────
set clamped1 to clamp(15, 0, 10)
if clamped1 is equal 10:
    set passed to passed + 1
    log "  PASS: clamp() above max"
otherwise:
    set failed to failed + 1
    log "  FAIL: clamp() above max"

set clamped2 to clamp(-5, 0, 10)
if clamped2 is equal 0:
    set passed to passed + 1
    log "  PASS: clamp() below min"
otherwise:
    set failed to failed + 1
    log "  FAIL: clamp() below min"

set clamped3 to clamp(5, 0, 10)
if clamped3 is equal 5:
    set passed to passed + 1
    log "  PASS: clamp() within range"
otherwise:
    set failed to failed + 1
    log "  FAIL: clamp() within range"

set clamped4 to clamp(0, 0, 10)
if clamped4 is equal 0:
    set passed to passed + 1
    log "  PASS: clamp() at minimum"
otherwise:
    set failed to failed + 1
    log "  FAIL: clamp() at minimum"

set clamped5 to clamp(10, 0, 10)
if clamped5 is equal 10:
    set passed to passed + 1
    log "  PASS: clamp() at maximum"
otherwise:
    set failed to failed + 1
    log "  FAIL: clamp() at maximum"

// ── sign(val) ───────────────────────────────────────────
set s1 to sign(42)
if s1 is equal 1:
    set passed to passed + 1
    log "  PASS: sign() positive"
otherwise:
    set failed to failed + 1
    log "  FAIL: sign() positive"

set s2 to sign(-7)
if s2 is equal -1:
    set passed to passed + 1
    log "  PASS: sign() negative"
otherwise:
    set failed to failed + 1
    log "  FAIL: sign() negative"

set s3 to sign(0)
if s3 is equal 0:
    set passed to passed + 1
    log "  PASS: sign() zero"
otherwise:
    set failed to failed + 1
    log "  FAIL: sign() zero"

// ── gcd(a, b) ──────────────────────────────────────────
set g1 to gcd(12, 8)
if g1 is equal 4:
    set passed to passed + 1
    log "  PASS: gcd(12, 8) = 4"
otherwise:
    set failed to failed + 1
    log "  FAIL: gcd(12, 8)"

set g2 to gcd(17, 13)
if g2 is equal 1:
    set passed to passed + 1
    log "  PASS: gcd(17, 13) = 1 (coprime)"
otherwise:
    set failed to failed + 1
    log "  FAIL: gcd(17, 13)"

set g3 to gcd(100, 75)
if g3 is equal 25:
    set passed to passed + 1
    log "  PASS: gcd(100, 75) = 25"
otherwise:
    set failed to failed + 1
    log "  FAIL: gcd(100, 75)"

set g4 to gcd(0, 5)
if g4 is equal 5:
    set passed to passed + 1
    log "  PASS: gcd(0, 5) = 5"
otherwise:
    set failed to failed + 1
    log "  FAIL: gcd(0, 5)"

// ── sorted(list) ───────────────────────────────────────
set unsorted to [3, 1, 4, 1, 5, 9, 2, 6]
set s_list to sorted(unsorted)
if s_list[0] is equal 1:
    set passed to passed + 1
    log "  PASS: sorted() first element"
otherwise:
    set failed to failed + 1
    log "  FAIL: sorted() first element"

if s_list[7] is equal 9:
    set passed to passed + 1
    log "  PASS: sorted() last element"
otherwise:
    set failed to failed + 1
    log "  FAIL: sorted() last element"

// original unchanged
if unsorted[0] is equal 3:
    set passed to passed + 1
    log "  PASS: sorted() non-mutating"
otherwise:
    set failed to failed + 1
    log "  FAIL: sorted() mutated original"

// sort strings
set words_list to ["banana", "apple", "cherry"]
set sorted_words to sorted(words_list)
if sorted_words[0] is equal "apple":
    set passed to passed + 1
    log "  PASS: sorted() strings"
otherwise:
    set failed to failed + 1
    log "  FAIL: sorted() strings"

// ── reversed(list) ─────────────────────────────────────
set orig to [1, 2, 3, 4, 5]
set rev to reversed(orig)
if rev[0] is equal 5:
    set passed to passed + 1
    log "  PASS: reversed() first element"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed() first"

if rev[4] is equal 1:
    set passed to passed + 1
    log "  PASS: reversed() last element"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed() last"

// original unchanged
if orig[0] is equal 1:
    set passed to passed + 1
    log "  PASS: reversed() non-mutating"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed() mutated original"

// ── pluck(list, field) ─────────────────────────────────
set items_list to [{"name": "A", "price": 10}, {"name": "B", "price": 20}, {"name": "C", "price": 30}]
set prices to pluck(items_list, "price")
if len(prices) is equal 3:
    set passed to passed + 1
    log "  PASS: pluck() count"
otherwise:
    set failed to failed + 1
    log "  FAIL: pluck() count"

if prices[0] is equal 10:
    set passed to passed + 1
    log "  PASS: pluck() first value"
otherwise:
    set failed to failed + 1
    log "  FAIL: pluck() first value"

if prices[2] is equal 30:
    set passed to passed + 1
    log "  PASS: pluck() last value"
otherwise:
    set failed to failed + 1
    log "  FAIL: pluck() last value"

// ── chunk_list(list, size) ──────────────────────────────
set big_list to [1, 2, 3, 4, 5, 6, 7]
set chunks to chunk_list(big_list, 3)
if len(chunks) is equal 3:
    set passed to passed + 1
    log "  PASS: chunk_list() correct chunk count"
otherwise:
    set failed to failed + 1
    log "  FAIL: chunk_list() chunk count"

set first_chunk to chunks[0]
if len(first_chunk) is equal 3:
    set passed to passed + 1
    log "  PASS: chunk_list() first chunk size"
otherwise:
    set failed to failed + 1
    log "  FAIL: chunk_list() first chunk size"

set last_chunk to chunks[2]
if len(last_chunk) is equal 1:
    set passed to passed + 1
    log "  PASS: chunk_list() last chunk partial"
otherwise:
    set failed to failed + 1
    log "  FAIL: chunk_list() last chunk"

if first_chunk[0] is equal 1:
    set passed to passed + 1
    log "  PASS: chunk_list() preserves values"
otherwise:
    set failed to failed + 1
    log "  FAIL: chunk_list() values"

// ── repeat_value(val, n) ────────────────────────────────
set zeros to repeat_value(0, 5)
if len(zeros) is equal 5:
    set passed to passed + 1
    log "  PASS: repeat_value() correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_value() count"

if zeros[0] is equal 0:
    set passed to passed + 1
    log "  PASS: repeat_value() correct value"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_value() value"

if zeros[4] is equal 0:
    set passed to passed + 1
    log "  PASS: repeat_value() last value"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_value() last"

// repeat with string value
set stars to repeat_value("*", 3)
if len(stars) is equal 3:
    set passed to passed + 1
    log "  PASS: repeat_value() with strings"
otherwise:
    set failed to failed + 1
    log "  FAIL: repeat_value() strings"

// ── dot_product(list1, list2) ───────────────────────────
set v1 to [1, 2, 3]
set v2 to [4, 5, 6]
set dp to dot_product(v1, v2)
// 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
if dp is equal 32.0:
    set passed to passed + 1
    log "  PASS: dot_product() correct"
otherwise:
    set failed to failed + 1
    log "  FAIL: dot_product()"

set v3 to [1, 0, 0]
set v4 to [0, 1, 0]
set dp2 to dot_product(v3, v4)
if dp2 is equal 0.0:
    set passed to passed + 1
    log "  PASS: dot_product() orthogonal vectors"
otherwise:
    set failed to failed + 1
    log "  FAIL: dot_product() orthogonal"

// ── linspace(start, end, n) ─────────────────────────────
set ls to linspace(0, 10, 5)
if len(ls) is equal 5:
    set passed to passed + 1
    log "  PASS: linspace() correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: linspace() count"

if ls[0] is equal 0.0:
    set passed to passed + 1
    log "  PASS: linspace() first value"
otherwise:
    set failed to failed + 1
    log "  FAIL: linspace() first value"

if ls[4] is equal 10.0:
    set passed to passed + 1
    log "  PASS: linspace() last value"
otherwise:
    set failed to failed + 1
    log "  FAIL: linspace() last value"

// Check middle value: 0 + 2.5*2 = 5.0
if ls[2] is equal 5.0:
    set passed to passed + 1
    log "  PASS: linspace() middle value"
otherwise:
    set failed to failed + 1
    log "  FAIL: linspace() middle value"

// ── lerp(a, b, t) ──────────────────────────────────────
set l1 to lerp(0, 10, 0.5)
if l1 is equal 5.0:
    set passed to passed + 1
    log "  PASS: lerp() midpoint"
otherwise:
    set failed to failed + 1
    log "  FAIL: lerp() midpoint"

set l2 to lerp(0, 10, 0)
if l2 is equal 0.0:
    set passed to passed + 1
    log "  PASS: lerp() at start"
otherwise:
    set failed to failed + 1
    log "  FAIL: lerp() start"

set l3 to lerp(0, 10, 1)
if l3 is equal 10.0:
    set passed to passed + 1
    log "  PASS: lerp() at end"
otherwise:
    set failed to failed + 1
    log "  FAIL: lerp() end"

// ── to_json / from_json ─────────────────────────────────
set data to {"name": "test", "value": 42}
set json_str to to_json(data)
if isinstance(json_str, "text") is equal true:
    set passed to passed + 1
    log "  PASS: to_json() returns string"
otherwise:
    set failed to failed + 1
    log "  FAIL: to_json() type"

// ── Combined test: take + sorted ────────────────────────
set big_nums to [50, 10, 30, 20, 40]
set top3 to take(sorted(big_nums), 3)
if top3[0] is equal 10:
    set passed to passed + 1
    log "  PASS: take(sorted()) combo first"
otherwise:
    set failed to failed + 1
    log "  FAIL: take(sorted()) combo"

if top3[2] is equal 30:
    set passed to passed + 1
    log "  PASS: take(sorted()) combo third"
otherwise:
    set failed to failed + 1
    log "  FAIL: take(sorted()) combo third"

// ── Combined test: compact + reversed ───────────────────
set mixed to [none, 3, none, 1, 2, none]
set clean_rev to reversed(compact(mixed))
if clean_rev[0] is equal 2:
    set passed to passed + 1
    log "  PASS: reversed(compact()) combo"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed(compact()) combo"

if len(clean_rev) is equal 3:
    set passed to passed + 1
    log "  PASS: reversed(compact()) correct count"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed(compact()) count"

// ── Combined test: map + reduce ─────────────────────────
set orders to [{"item": "A", "total": 100}, {"item": "B", "total": 200}, {"item": "C", "total": 150}]
set totals to map(orders, "total")
set grand_total to reduce(totals, "+")
if grand_total is equal 450:
    set passed to passed + 1
    log "  PASS: map() + reduce() pipeline"
otherwise:
    set failed to failed + 1
    log "  FAIL: map() + reduce() pipeline"

// ── Combined test: find + merge ─────────────────────────
set db to [{"id": 1, "name": "Widget"}, {"id": 2, "name": "Gadget"}]
set item to find(db, "id", 1)
set updated to merge(item, {"price": 9.99})
if updated["name"] is equal "Widget":
    set passed to passed + 1
    log "  PASS: find() + merge() keeps original"
otherwise:
    set failed to failed + 1
    log "  FAIL: find() + merge()"

if updated["price"] is equal 9.99:
    set passed to passed + 1
    log "  PASS: find() + merge() adds new field"
otherwise:
    set failed to failed + 1
    log "  FAIL: find() + merge() new field"

// ── Edge case: empty inputs ─────────────────────────────
set empty_items to items({})
if len(empty_items) is equal 0:
    set passed to passed + 1
    log "  PASS: items() on empty record"
otherwise:
    set failed to failed + 1
    log "  FAIL: items() empty"

set empty_compact to compact([])
if len(empty_compact) is equal 0:
    set passed to passed + 1
    log "  PASS: compact() on empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: compact() empty"

set empty_take to take([], 5)
if len(empty_take) is equal 0:
    set passed to passed + 1
    log "  PASS: take() on empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: take() empty"

set empty_sorted to sorted([])
if len(empty_sorted) is equal 0:
    set passed to passed + 1
    log "  PASS: sorted() on empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: sorted() empty"

set empty_reversed to reversed([])
if len(empty_reversed) is equal 0:
    set passed to passed + 1
    log "  PASS: reversed() on empty list"
otherwise:
    set failed to failed + 1
    log "  FAIL: reversed() empty"

// ── Results ─────────────────────────────────────────────
log ""
log "══════════════════════════════════════════"
if failed is equal 0:
    log "  ALL " + str(passed) + "/" + str(total) + " V1 ENHANCEMENT TESTS PASSED"
otherwise:
    log "  FAILED: " + str(failed) + " / " + str(total) + " tests failed"
log "══════════════════════════════════════════"
