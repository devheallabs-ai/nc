// ══════════════════════════════════════════════════════════════════
//  HiveANT — Security & Governance Policy Engine
//
//  Safeguards:
//    - Role-based permissions
//    - Action approval policies
//    - Rollback mechanisms
//    - Audit logs
//    - Resource limits
//  Agents must never execute unsafe actions.
// ══════════════════════════════════════════════════════════════════

to policy_check with action, agent_id, risk_level, secret:
    purpose: "Check if an action is permitted by security policy"

    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"permitted": false, "reason": "Authentication failed", "_status": 401}

    set require_approval to env("REQUIRE_APPROVAL_FOR")
    if require_approval:
        if risk_level is equal require_approval:
            respond with {
                "permitted": false,
                "reason": "Action requires manual approval",
                "risk_level": risk_level,
                "action": action,
                "agent_id": agent_id,
                "approval_required": true
            }

    set max_actions to env("MAX_ACTIONS_PER_MINUTE")
    if max_actions:
        set rate_file to "agents_state/rate-" + agent_id + ".json"
        if file_exists(rate_file):
            set rate_data to json_decode(read_file(rate_file))
            set elapsed to time_now() - rate_data.window_start
            if elapsed is below 60:
                if rate_data.count is above max_actions:
                    respond with {"permitted": false, "reason": "Rate limit exceeded", "limit": max_actions, "agent_id": agent_id}

    shell("mkdir -p agents_state/audit")
    set audit_entry to {
        "action": action,
        "agent_id": agent_id,
        "risk_level": risk_level,
        "permitted": true,
        "timestamp": time_now()
    }
    set audit_id to "AUD-" + str(floor(random() * 90000 + 10000))
    write_file("agents_state/audit/" + audit_id + ".json", json_encode(audit_entry))

    respond with {"permitted": true, "audit_id": audit_id, "action": action}

to audit_log with action, agent_id, details, outcome:
    purpose: "Record an action in the audit log"
    shell("mkdir -p agents_state/audit")
    set entry to {
        "action": action,
        "agent_id": agent_id,
        "details": details,
        "outcome": outcome,
        "timestamp": time_now()
    }
    set audit_id to "AUD-" + str(floor(random() * 90000 + 10000))
    write_file("agents_state/audit/" + audit_id + ".json", json_encode(entry))
    respond with {"audit_id": audit_id, "logged": true}

to get_audit_trail with count:
    purpose: "Retrieve recent audit log entries"
    if count:
        set limit to str(count)
    otherwise:
        set limit to "50"
    set result to shell("ls -t agents_state/audit/*.json 2>/dev/null | head -" + limit + " | while read f; do cat \"$f\" 2>/dev/null; echo ','; done || echo NONE")
    if result is equal "NONE":
        respond with {"entries": [], "count": 0}
    otherwise:
        respond with {"entries": result, "count": limit}

to rollback_action with incident_id, secret:
    purpose: "Rollback a previous action using saved rollback commands"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}

    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        set rollback_results to []
        set allowed_prefixes to ["kubectl", "docker", "systemctl", "helm", "curl"]
        if incident.rollback_commands:
            repeat for each rb in incident.rollback_commands:
                set cmd_lower to lower(str(rb.command))
                set is_allowed to false
                repeat for each prefix in allowed_prefixes:
                    if starts_with(cmd_lower, prefix):
                        set is_allowed to true
                        stop
                if is_allowed:
                    log "ROLLBACK: " + rb.command
                    set result to shell(rb.command)
                    set rollback_results to rollback_results + [{"command": rb.command, "result": result, "status": "executed"}]
                otherwise:
                    log "ROLLBACK BLOCKED: " + rb.command
                    set rollback_results to rollback_results + [{"command": rb.command, "result": "BLOCKED — command not in allowlist", "status": "blocked"}]

        shell("mkdir -p agents_state/audit")
        set audit_entry to {"action": "rollback", "incident_id": incident_id, "results": rollback_results, "timestamp": time_now()}
        write_file("agents_state/audit/ROLLBACK-" + incident_id + ".json", json_encode(audit_entry))

        respond with {"incident_id": incident_id, "action": "rollback", "results": rollback_results}
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}
