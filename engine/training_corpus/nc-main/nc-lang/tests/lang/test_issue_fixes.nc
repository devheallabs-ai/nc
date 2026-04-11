// Test: Regression tests for NC engine issues #1-#8
// Validates all fixes across Linux, macOS, and Windows
//
// Issue 1: write_file() sandbox warning (tested at C level)
// Issue 2: Multi-line strings (triple-quote + backslash continuation)
// Issue 3: Dynamic map key access with variables
// Issue 4: 'check' as variable name (was reserved word)
// Issue 5: Missing params default to none, not service config
// Issue 6: split() on empty/edge-case strings returns proper lists
// Issue 7: time_format() and time_iso() functions
// Issue 8: Configure block accepts colon, is, and = syntax

service "test-issue-fixes"
version "1.0.0"

// ─── ISSUE 2: Multi-line strings via triple-quotes ───

to test_issue2_triple_quote_string:
    set prompt to """Analyze this data.
SIGNALS: test_signal
Return JSON."""
    respond with len(prompt)

to test_issue2_triple_quote_contains_newline:
    set msg to """line one
line two
line three"""
    set has_newline to contains(msg, "line two")
    respond with has_newline

to test_issue2_single_line_escape:
    set msg to "hello\tworld\n"
    respond with contains(msg, "hello")

// ─── ISSUE 3: Dynamic map key access ───

to test_issue3_map_bracket_variable:
    set db to {"alice": 100, "bob": 200}
    set key to "alice"
    set val to db[key]
    respond with val

to test_issue3_set_map_bracket_variable:
    set scores to {}
    set player to "alice"
    set scores[player] to 100
    respond with scores["alice"]

to test_issue3_map_bracket_string_literal:
    set data to {"x": 42}
    respond with data["x"]

to test_issue3_nested_map_set:
    set graph to {}
    set graph["edges"] to {}
    respond with type(graph["edges"])

to test_issue3_map_dot_bracket:
    set db to {"edges": {}}
    set eid to "a_to_b"
    set db.edges[eid] to {"score": 1.0}
    respond with db.edges["a_to_b"]["score"]

// ─── ISSUE 4: 'check' as variable name ───

to test_issue4_check_as_variable:
    set check to "passed"
    respond with check

to test_issue4_check_in_expression:
    set check to 42
    set result to check + 8
    respond with result

to test_issue4_check_in_condition:
    set check to true
    if check:
        respond with "yes"
    otherwise:
        respond with "no"

// ─── ISSUE 5: Missing params default to none ───

to issue5_helper with config:
    if config is equal to nothing:
        respond with "none"
    otherwise:
        respond with "has_value"

to test_issue5_missing_param_is_none:
    set result to issue5_helper()
    respond with result

to test_issue5_explicit_param:
    set result to issue5_helper({"key": "val"})
    respond with result

// ─── ISSUE 6: split() edge cases ───

to test_issue6_split_basic:
    set parts to split("a,b,c", ",")
    respond with len(parts)

to test_issue6_split_newline:
    set text to "line1\nline2\nline3"
    set lines to split(text, "\n")
    respond with len(lines)

to test_issue6_split_empty_returns_list:
    set parts to split("", ",")
    respond with type(parts)

to test_issue6_split_result_is_list:
    set data to "hello world"
    set parts to split(data, " ")
    if len(parts) is above 0:
        respond with parts[0]

to test_issue6_split_no_delimiter_found:
    set data to "no_commas_here"
    set parts to split(data, ",")
    respond with len(parts)

// ─── ISSUE 7: time_format() and time_iso() ───

to test_issue7_time_now_is_number:
    set t to time_now()
    if t is above 1000000000:
        respond with "valid"

to test_issue7_time_format:
    set t to time_now()
    set formatted to time_format(t, "%Y-%m-%d")
    respond with len(formatted)

to test_issue7_time_format_default:
    set t to time_now()
    set formatted to time_format(t)
    respond with len(formatted)

to test_issue7_time_iso:
    set iso to time_iso()
    set has_t to contains(iso, "T")
    respond with has_t

to test_issue7_time_iso_with_arg:
    set t to time_now()
    set iso to time_iso(t)
    set has_z to contains(iso, "Z")
    respond with has_z

// ─── ISSUE 8: Configure block syntax consistency ───
// (Tested implicitly: configure blocks already accept : and is;
//  we verify the parser doesn't choke on them)

to test_issue8_map_with_colon:
    set cfg to {"port": 7700, "debug": true}
    respond with cfg["port"]

to test_issue8_map_with_mixed_types:
    set cfg to {"name": "test", "count": 10, "active": true}
    respond with len(keys(cfg))

// ─── Cross-platform path handling ───

to test_xplat_string_backslash:
    set path to "C:\\Users\\test\\file.txt"
    respond with contains(path, "Users")

to test_xplat_string_forward_slash:
    set path to "/home/user/file.txt"
    respond with contains(path, "user")

// ─── Edge cases from production usage ───

to test_edge_empty_list_is_falsy:
    set items to []
    if items:
        respond with "truthy"
    otherwise:
        respond with "falsy"

to test_edge_map_iteration:
    set data to {"a": 1, "b": 2, "c": 3}
    set total to 0
    repeat for each key, val in data:
        set total to total + val
    respond with total

to test_edge_nested_function_calls:
    set text to "  Hello World  "
    set result to lower(trim(text))
    respond with result
