// ══════════════════════════════════════════════════════════════════
//  HiveANT — Audit Logging System
//
//  Comprehensive audit trail for all agent actions.
//  Immutable log of who did what, when, and why.
// ══════════════════════════════════════════════════════════════════

to audit_log with action, agent_id, details, outcome, risk_level:
    purpose: "Record an auditable action"
    shell("mkdir -p agents_state/audit")
    set entry to {"action": action, "agent_id": agent_id, "details": details, "outcome": outcome, "risk_level": risk_level, "timestamp": time_now()}
    set audit_id to "AUD-" + str(floor(random() * 90000 + 10000))
    write_file("agents_state/audit/" + audit_id + ".json", json_encode(entry))
    respond with {"audit_id": audit_id, "logged": true}

to audit_trail with count:
    purpose: "Retrieve recent audit entries"
    if count:
        set lim to str(count)
    otherwise:
        set lim to "50"
    set result to shell("ls -t agents_state/audit/AUD-*.json 2>/dev/null | head -" + lim + " | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"entries": [], "count": 0}
    set ids to split(trim(result), "\n")
    respond with {"entries": ids, "count": len(ids)}

to audit_search with action_type:
    purpose: "Search audit log by action type"
    set result to shell("grep -rl '\"action\":\"" + action_type + "\"' agents_state/audit/*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"query": action_type, "matches": [], "count": 0}
    set ids to split(trim(result), "\n")
    respond with {"query": action_type, "matches": ids, "count": len(ids)}
