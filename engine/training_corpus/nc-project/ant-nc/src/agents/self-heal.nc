// ══════════════════════════════════════════════════════════════════
//  SELF-HEAL — Detect → Diagnose → Fix → Verify → Rollback
// ══════════════════════════════════════════════════════════════════

to self_heal with service_name, description, auto_execute, secret:
    purpose: "Full self-healing loop: detect, diagnose, fix, verify"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized. Pass secret parameter matching HEAL_SECRET env var.", "_status": 401}

    // Cooldown: prevent re-heal within 60 seconds
    if file_exists(".last_heal"):
        set last_heal_time to read_file(".last_heal")

    write_file(".last_heal", str(time_now()))
    log "SELF-HEAL STARTED: {{service_name}}"

    set heal_log to "=== Self-Heal Report ===\nService: " + service_name + "\nStarted: " + str(time_now()) + "\n\n"

    // Step 1: Gather current state
    set heal_log to heal_log + "STEP 1: Gathering system state...\n"
    set health_data to "unknown"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    set metrics_data to "not configured"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set error_q to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + service_name + "\"}[5m])"
        gather metrics_data from error_q

    set log_data to "not configured"
    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + service_name + "+AND+level:ERROR&size=20&sort=@timestamp:desc"
        gather log_data from log_url

    set deploy_data to "not configured"
    set gh_token to env("GITHUB_TOKEN")
    if gh_token:
        set gh_repo to env("GITHUB_REPO")
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=5"
        gather deploy_data from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token}

    set heal_log to heal_log + "  Health: " + str(health_data) + "\n  Metrics: collected\n  Logs: collected\n  Deploys: collected\n\n"

    // Step 2: AI Diagnosis
    set heal_log to heal_log + "STEP 2: AI diagnosing root cause...\n"
    set full_context to "SERVICE: {{service_name}}. ISSUE: {{description}}. HEALTH: " + str(health_data) + ". METRICS: " + str(metrics_data) + ". LOGS: " + str(log_data) + ". DEPLOYS: " + str(deploy_data) + "."

    ask AI to "You are SwarmOps Self-Healing Agent. Diagnose this incident AND provide exact fix commands that can be executed automatically. IMPORTANT: Commands must be safe, idempotent, and reversible. Context: {{full_context}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"fix_commands\": [{\"command\": \"string (exact shell command)\", \"description\": \"string\", \"risk\": \"low|medium|high\", \"reversible\": true}], \"verify_command\": \"string (command to check if fix worked)\", \"rollback_commands\": [{\"command\": \"string\", \"description\": \"string\"}], \"safe_to_auto_execute\": true}" save as diagnosis

    set heal_log to heal_log + "  Root cause: " + diagnosis.root_cause + "\n  Confidence: " + str(diagnosis.confidence_percent) + "%\n  Category: " + diagnosis.category + "\n\n"

    // Step 3: Execute fix (only if auto_execute is "yes" and confidence > 70)
    set executed to "no"
    set exec_results to []
    if auto_execute is equal "yes":
        if diagnosis.confidence_percent is above 70:
            if diagnosis.safe_to_auto_execute:
                set heal_log to heal_log + "STEP 3: AUTO-EXECUTING FIX (confidence > 70%, safe=true)...\n"
                repeat for each fix_cmd in diagnosis.fix_commands:
                    if fix_cmd.risk is not equal "high":
                        log "EXECUTING: " + fix_cmd.command
                        set cmd_result to shell(fix_cmd.command)
                        set exec_results to exec_results + [{"command": fix_cmd.command, "result": cmd_result}]
                        set heal_log to heal_log + "  Executed: " + fix_cmd.command + "\n  Result: " + str(cmd_result) + "\n"
                    otherwise:
                        set heal_log to heal_log + "  SKIPPED (high risk): " + fix_cmd.command + "\n"
                        set exec_results to exec_results + [{"command": fix_cmd.command, "result": "SKIPPED — high risk"}]
                set executed to "yes"
            otherwise:
                set heal_log to heal_log + "STEP 3: SKIPPED — AI marked as unsafe for auto-execution\n\n"
        otherwise:
            set heal_log to heal_log + "STEP 3: SKIPPED — confidence " + str(diagnosis.confidence_percent) + "% below 70% threshold\n\n"
    otherwise:
        set heal_log to heal_log + "STEP 3: SKIPPED — auto_execute not enabled (set to 'yes' to enable)\n\n"

    // Step 4: Verify fix worked
    set verification to "not run"
    if executed is equal "yes":
        set heal_log to heal_log + "\nSTEP 4: VERIFYING FIX...\n"
        if diagnosis.verify_command:
            set verification to shell(diagnosis.verify_command)
            set heal_log to heal_log + "  Command: " + diagnosis.verify_command + "\n  Result: " + str(verification) + "\n"

        // Step 5: Re-check health after fix
        set heal_log to heal_log + "\nSTEP 5: RE-CHECKING HEALTH...\n"
        if urls_str:
            set post_health to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")
            set heal_log to heal_log + "  Post-fix health: " + str(post_health) + "\n"

    // Save report
    set incident_id to "HEAL-" + str(floor(random() * 9000 + 1000))
    set report to {"incident_id": incident_id, "service": service_name, "description": description, "root_cause": diagnosis.root_cause, "confidence": diagnosis.confidence_percent, "category": diagnosis.category, "fix_commands": diagnosis.fix_commands, "rollback_commands": diagnosis.rollback_commands, "auto_executed": executed, "execution_results": exec_results, "verification": verification, "safe_to_auto_execute": diagnosis.safe_to_auto_execute, "heal_log": heal_log, "healed_at": time_now()}

    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(report))
    log heal_log

    // Notify Slack
    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set action_text to "Manual action required"
        if executed is equal "yes":
            set action_text to "AUTO-FIXED"
        set slack_msg to "{\"text\":\"*SwarmOps Self-Heal — " + incident_id + "*\\n*Service:* " + service_name + "\\n*Root Cause:* " + diagnosis.root_cause + "\\n*Status:* " + action_text + "\\n*Confidence:* " + str(diagnosis.confidence_percent) + "%\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg

    respond with report

// ══════════════════════════════════════════════════════════════════
//  ROLLBACK — Undo a self-heal action
// ══════════════════════════════════════════════════════════════════

to rollback with incident_id, secret:
    purpose: "Rollback a self-heal action using saved rollback commands"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        set rollback_results to []
        if incident.rollback_commands:
            repeat for each rb in incident.rollback_commands:
                log "ROLLBACK: " + rb.command
                set result to shell(rb.command)
                set rollback_results to rollback_results + [{"command": rb.command, "result": result}]
        respond with {"incident_id": incident_id, "action": "rollback", "results": rollback_results}
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}

// ══════════════════════════════════════════════════════════════════
//  WATCHDOG — Continuous monitoring loop
//  Call POST /watchdog to start a single check cycle
// ══════════════════════════════════════════════════════════════════

to watchdog:
    purpose: "Check all monitored URLs — if any unhealthy, auto-investigate"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set check_result to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); if [ \"$code\" != \"200\" ]; then echo \"UNHEALTHY:$url:$code\"; fi; done")
        if len(trim(check_result)) is above 0:
            log "WATCHDOG ALERT: " + check_result
            respond with {"status": "unhealthy_detected", "details": check_result, "action": "Call POST /self-heal with service_name and auto_execute=yes to auto-fix"}
        otherwise:
            respond with {"status": "all_healthy", "checked": urls_str}
    otherwise:
        respond with {"status": "not_configured", "error": "Set MONITOR_URLS in .env"}
