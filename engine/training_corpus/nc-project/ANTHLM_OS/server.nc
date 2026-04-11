// ══════════════════════════════════════════════════════════════════
//  HiveANT v1.0.0 — Swarm Autonomous AI Operating System
//
//  Start:  NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc serve server.nc
// ══════════════════════════════════════════════════════════════════

service "hiveant"
version "1.0.0"

configure:
    port: 7700
    ai_model is "env:NC_AI_MODEL"
    ai_url is "env:NC_AI_URL"
    ai_key is "env:NC_AI_KEY"
    ai_system_prompt is "You are HiveANT, a Swarm Autonomous AI Operating System. You coordinate thousands of agents using Ant Colony and Bee Colony algorithms to investigate, diagnose, fix, and learn from software system failures. Always return ONLY valid JSON."
    rate_limit: 100
    cors: true
    log_requests: true
    // TLS: Set NC_TLS_CERT and NC_TLS_KEY env vars to enable HTTPS
    // tls_cert is "env:NC_TLS_CERT"
    // tls_key is "env:NC_TLS_KEY"

// ══════════════════════════════════════════════════════════════════
//  AUTHENTICATION & AUTHORIZATION (JWT + RBAC + Multi-Tenant)
//
//  Roles:
//    admin    — Full access: investigate, self-heal, spawn, kill, cleanup, users
//    operator — Investigate, feedback, spawn agents, view everything
//    viewer   — Read-only: dashboards, incidents, knowledge, status
//
//  Users stored in agents_state/users/ with hashed passwords (SHA-256).
//  JWT tokens are HS256-signed with configurable expiry.
//  Multi-tenant: users belong to an org_id, data isolated per org.
// ══════════════════════════════════════════════════════════════════

to auth_register with username, password, role, org_id, admin_token:
    purpose: "Register a new user (admin only, or first-user bootstrap)"
    set jwt_secret to env("NC_JWT_SECRET")
    if jwt_secret:
        if admin_token:
            set claims to jwt_verify(admin_token)
            if claims:
                if claims.role is not equal "admin":
                    respond with {"error": "Only admins can register users", "_status": 403}
            otherwise:
                respond with {"error": "Invalid admin token", "_status": 401}
        otherwise:
            set user_count to shell("ls agents_state/users/*.json 2>/dev/null | wc -l || echo 0")
            if user_count is above 0:
                respond with {"error": "Admin token required to register users", "_status": 401}

    set user_role to validate_role(role)

    if org_id:
        set user_org to org_id
    otherwise:
        set user_org to "default"

    set safe_user to replace(replace(replace(str(username), ";", ""), "|", ""), " ", "")
    set safe_org to replace(replace(replace(str(user_org), ";", ""), "|", ""), " ", "")

    shell("mkdir -p agents_state/users agents_state/orgs")
    set user_path to "agents_state/users/" + safe_user + ".json"
    if file_exists(user_path):
        respond with {"error": "User already exists", "_status": 409}

    set password_hash to hash_password(password)
    set user to {"username": safe_user, "password_hash": password_hash, "role": user_role, "org_id": safe_org, "created_at": time_iso(), "last_login": "never", "login_count": 0, "status": "active"}
    write_file(user_path, json_encode(user))

    set org_path to "agents_state/orgs/" + safe_org + ".json"
    if file_exists(org_path):
        try:
            set org to json_decode(read_file(org_path))
            set org.members to org.members + [safe_user]
            set org.member_count to org.member_count + 1
            write_file(org_path, json_encode(org))
        on_error:
            log "WARN: Could not update org file"
    otherwise:
        set org to {"org_id": safe_org, "members": [safe_user], "member_count": 1, "created_at": time_iso(), "status": "active"}
        write_file(org_path, json_encode(org))

    audit_log_action("user_registered", safe_user, safe_user, {"role": user_role, "org_id": safe_org})
    respond with {"username": safe_user, "role": user_role, "org_id": safe_org, "status": "active"}

to auth_login with username, password, api_key:
    purpose: "Authenticate and return JWT token"
    set jwt_secret to env("NC_JWT_SECRET")

    if jwt_secret:
        set token_expiry to env("HIVEANT_TOKEN_EXPIRY")
        if token_expiry:
            set expiry to token_expiry
        otherwise:
            set expiry to 86400

        if username:
            set safe_user to replace(replace(str(username), ";", ""), "|", "")
            set user_path to "agents_state/users/" + safe_user + ".json"
            if file_exists(user_path):
                try:
                    set user to json_decode(read_file(user_path))
                on_error:
                    respond with {"error": "Corrupt user file", "_status": 500}

                if verify_password(password, user.password_hash):
                    if user.status is not equal "active":
                        respond with {"error": "Account disabled", "_status": 403}

                    set token to jwt_generate(safe_user, user.role, 86400, {"org_id": user.org_id})

                    set user.last_login to time_iso()
                    set user.login_count to user.login_count + 1
                    write_file(user_path, json_encode(user))

                    audit_log_action("login_success", safe_user, safe_user, {"role": user.role, "org_id": user.org_id})

                    respond with {"authenticated": true, "token": token, "user": safe_user, "role": user.role, "org_id": user.org_id, "expires_in": expiry}
                otherwise:
                    audit_log_action("login_failed", safe_user, safe_user, {"reason": "invalid_password"})
                    respond with {"error": "Invalid credentials", "_status": 401}
            otherwise:
                respond with {"error": "User not found", "_status": 404}

        if api_key:
            set master_key to env("HIVEANT_API_KEY")
            if master_key:
                if api_key is equal master_key:
                    set token to jwt_generate("api-key-admin", "admin", 86400, {"org_id": "default"})
                    respond with {"authenticated": true, "token": token, "user": "api-key-admin", "role": "admin", "org_id": "default", "expires_in": expiry}

        respond with {"error": "Provide username+password or api_key", "_status": 400}
    otherwise:
        respond with {"authenticated": true, "token": "no-auth-configured", "user": "anonymous", "role": "admin", "org_id": "default", "message": "No JWT secret configured — open access"}

to auth_refresh with token:
    purpose: "Refresh an existing JWT token before it expires"
    set jwt_secret to env("NC_JWT_SECRET")
    if jwt_secret:
        if token:
            set claims to jwt_verify(token)
            if claims:
                set user_path to "agents_state/users/" + str(claims.user_id) + ".json"
                if file_exists(user_path):
                    try:
                        set user to json_decode(read_file(user_path))
                        if user.status is not equal "active":
                            respond with {"error": "Account disabled", "_status": 403}
                    on_error:
                        log "WARN: Could not read user file for token refresh"

                set token_expiry to env("HIVEANT_TOKEN_EXPIRY")
                if token_expiry:
                    set expiry to token_expiry
                otherwise:
                    set expiry to 86400

                set new_claims to {"user_id": claims.user_id, "role": claims.role, "org_id": claims.org_id, "iat": time_now()}
                set new_token to jwt_generate(claims.user_id, claims.role, 86400, {"org_id": claims.org_id})
                respond with {"token": new_token, "user": claims.user_id, "role": claims.role, "org_id": claims.org_id, "expires_in": expiry}
            otherwise:
                respond with {"error": "Token expired or invalid — please login again", "_status": 401}
        otherwise:
            respond with {"error": "Token required", "_status": 400}
    otherwise:
        respond with {"token": "no-auth-configured", "message": "Auth not enabled"}

to auth_status:
    purpose: "Check authentication configuration"
    set jwt_secret to env("NC_JWT_SECRET")
    set user_count to shell("ls agents_state/users/*.json 2>/dev/null | wc -l || echo 0")
    set org_count to shell("ls agents_state/orgs/*.json 2>/dev/null | wc -l || echo 0")
    if jwt_secret:
        respond with {"auth_required": true, "method": "jwt_hs256", "header": "Authorization: Bearer <token>", "users": user_count, "organizations": org_count, "roles": ["admin", "operator", "viewer"], "features": ["password_hashing", "token_refresh", "multi_tenant", "per_role_rate_limit"], "rate_limits": {"admin": 1000, "operator": 200, "viewer": 60, "window_seconds": 60}}
    otherwise:
        respond with {"auth_required": false, "message": "Set HIVEANT_JWT_SECRET to enable authentication"}

to auth_users with token:
    purpose: "List all users (admin only)"
    require_role(token, "admin")
    set result to shell("ls agents_state/users/*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"users": [], "count": 0}
    set users to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set u to json_decode(read_file(f))
                set users to users + [{"username": u.username, "role": u.role, "org_id": u.org_id, "status": u.status, "last_login": u.last_login, "login_count": u.login_count, "created_at": u.created_at}]
            on_error:
                skip
    respond with {"users": users, "count": len(users)}

to auth_update_user with target_username, new_role, new_status, token:
    purpose: "Update user role or status (admin only)"
    require_role(token, "admin")
    set safe_user to replace(replace(str(target_username), ";", ""), "|", "")
    set user_path to "agents_state/users/" + safe_user + ".json"
    if file_exists(user_path):
        try:
            set user to json_decode(read_file(user_path))
        on_error:
            respond with {"error": "Corrupt user file", "_status": 500}

        if new_role:
            set user.role to new_role
        if new_status:
            set user.status to new_status
        set user.updated_at to time_iso()
        write_file(user_path, json_encode(user))
        audit_log_action("user_updated", "admin", safe_user, {"new_role": new_role, "new_status": new_status})
        respond with {"username": safe_user, "role": user.role, "status": user.status, "updated": true}
    otherwise:
        respond with {"error": "User not found", "_status": 404}

to auth_delete_user with target_username, token:
    purpose: "Delete a user (admin only)"
    require_role(token, "admin")
    set safe_user to replace(replace(str(target_username), ";", ""), "|", "")
    set safe_user to safe_id(target_username)
    set user_path to "agents_state/users/" + safe_user + ".json"
    if file_exists(user_path):
        delete_file(user_path)
        audit_log_action("user_deleted", "admin", safe_user, {})
        log "AUTH: Deleted user " + safe_user
        respond with {"username": safe_user, "deleted": true}
    otherwise:
        respond with {"error": "User not found", "_status": 404}

to auth_change_password with current_password, new_password, token:
    purpose: "Change own password"
    set jwt_secret to env("NC_JWT_SECRET")
    if jwt_secret:
        if token:
            set claims to jwt_verify(token)
            if claims:
                set user_path to "agents_state/users/" + str(claims.user_id) + ".json"
                if file_exists(user_path):
                    try:
                        set user to json_decode(read_file(user_path))
                    on_error:
                        respond with {"error": "Corrupt user file", "_status": 500}
                    if verify_password(current_password, user.password_hash):
                        set user.password_hash to hash_password(new_password)
                        set user.updated_at to time_iso()
                        write_file(user_path, json_encode(user))
                        respond with {"message": "Password changed", "user": claims.user_id}
                    otherwise:
                        respond with {"error": "Current password incorrect", "_status": 401}
                otherwise:
                    respond with {"error": "User not found", "_status": 404}
            otherwise:
                respond with {"error": "Invalid token", "_status": 401}

to auth_orgs with token:
    purpose: "List organizations (admin only)"
    require_role(token, "admin")
    set result to shell("ls agents_state/orgs/*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"organizations": [], "count": 0}
    set orgs to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set o to json_decode(read_file(f))
                set orgs to orgs + [{"org_id": o.org_id, "member_count": o.member_count, "status": o.status, "created_at": o.created_at}]
            on_error:
                skip
    respond with {"organizations": orgs, "count": len(orgs)}

to require_role with token, required_role:
    purpose: "Verify JWT and check role permission"
    set jwt_secret to env("NC_JWT_SECRET")
    if jwt_secret:
        if token:
            set claims to jwt_verify(token)
            if claims:
                if required_role is equal "admin":
                    if claims.role is not equal "admin":
                        respond with {"error": "Admin access required", "your_role": claims.role, "_status": 403}
                if required_role is equal "operator":
                    if claims.role is equal "viewer":
                        respond with {"error": "Operator or admin access required", "your_role": claims.role, "_status": 403}
                respond with claims
            otherwise:
                respond with {"error": "Invalid or expired token", "_status": 401}
        otherwise:
            respond with {"error": "Authorization required — provide token in request body", "_status": 401}
    otherwise:
        log "WARN: NC_JWT_SECRET not set — auth disabled, all access granted"

// ══════════════════════════════════════════════════════════════════
//  AUDIT LOGGING — Structured audit trail for all actions
// ══════════════════════════════════════════════════════════════════

to audit_log_action with action, actor, target, details:
    purpose: "Write a structured audit log entry with who/what/when/where"
    shell("mkdir -p agents_state/audit")
    set ip to request_ip()
    set method to request_method()
    set path to request_path()
    set audit_id to "AUD-" + str(floor(random() * 90000 + 10000))
    set entry to {
        "audit_id": audit_id,
        "action": action,
        "actor": actor,
        "target": target,
        "details": details,
        "ip": ip,
        "method": method,
        "path": path,
        "timestamp": time_iso()
    }
    write_file("agents_state/audit/" + audit_id + ".json", json_encode(entry))
    log "AUDIT: " + str(action) + " by " + str(actor) + " on " + str(target) + " from " + str(ip)

// ══════════════════════════════════════════════════════════════════
//  UTILITY — Input sanitization (prevents shell injection + path traversal)
// ══════════════════════════════════════════════════════════════════

to sanitize with input_str:
    purpose: "Remove dangerous characters from user input before shell use"
    if input_str:
        set safe to replace(replace(replace(replace(replace(replace(str(input_str), ";", ""), "|", ""), "&", ""), "`", ""), "$", ""), "'", "")
        respond with safe
    otherwise:
        respond with ""

to safe_id with input_str:
    purpose: "Sanitize an ID for use in file paths — prevents path traversal"
    if input_str:
        set clean to replace(replace(replace(replace(replace(replace(replace(replace(str(input_str), "..", ""), "/", ""), "\\", ""), ";", ""), "|", ""), "&", ""), "`", ""), "'", "")
        set clean to replace(replace(replace(clean, "$", ""), " ", "-"), "\"", "")
        if len(clean) is above 128:
            respond with ""
        respond with clean
    otherwise:
        respond with ""

to validate_role with role_str:
    purpose: "Validate role is one of admin/operator/viewer"
    if role_str is equal "admin":
        respond with "admin"
    if role_str is equal "operator":
        respond with "operator"
    respond with "viewer"

to validate_integer with val, min_val, max_val, default_val:
    purpose: "Validate and constrain an integer value"
    if val:
        if val is above max_val:
            respond with max_val
        if val is below min_val:
            respond with min_val
        respond with val
    otherwise:
        respond with default_val

to validate_command with cmd:
    purpose: "Validate a shell command against allowlist before execution"
    set allowed_prefixes to ["kubectl", "docker", "systemctl", "curl", "helm", "echo", "cat", "grep", "ls", "ps", "top", "df", "du", "ping", "dig", "nslookup", "wget"]
    set cmd_str to str(cmd)
    set cmd_lower to lower(cmd_str)
    set is_safe to false
    repeat for each prefix in allowed_prefixes:
        if starts_with(cmd_lower, prefix):
            set is_safe to true
            stop
    if is_safe is not equal true:
        respond with {"allowed": false, "command": cmd_str, "reason": "Command not in allowlist. Allowed: kubectl, docker, systemctl, curl, helm"}
    if contains(cmd_str, "&&"):
        respond with {"allowed": false, "command": cmd_str, "reason": "Chained commands (&&) not allowed"}
    if contains(cmd_str, "||"):
        respond with {"allowed": false, "command": cmd_str, "reason": "Chained commands (||) not allowed"}
    if contains(cmd_str, "`"):
        respond with {"allowed": false, "command": cmd_str, "reason": "Backtick execution not allowed"}
    if contains(cmd_str, "$("):
        respond with {"allowed": false, "command": cmd_str, "reason": "Subshell execution not allowed"}
    respond with {"allowed": true, "command": cmd_str}

to validate_url with url_str:
    purpose: "Validate a URL is safe (no internal/metadata endpoints)"
    set blocked to ["169.254.169.254", "metadata.google", "metadata.azure", "100.100.100.200", "127.0.0.1", "localhost", "0.0.0.0"]
    set url_lower to lower(str(url_str))
    repeat for each blocked_host in blocked:
        if contains(url_lower, blocked_host):
            respond with false
    if starts_with(url_lower, "http://") or starts_with(url_lower, "https://"):
        respond with true
    respond with false

// ══════════════════════════════════════════════════════════════════
//  CORE KERNEL
// ══════════════════════════════════════════════════════════════════

to kernel_init with token:
    purpose: "Initialize the HiveANT kernel and all subsystems"
    require_role(token, "admin")
    shell("mkdir -p incidents knowledge docs memory/pheromone/edges memory/twins/services memory/tasks memory/causal memory/knowledge_graph/entities memory/knowledge_graph/relations agents_state/queue agents_state/clusters agents_state/audit agents_state/bus agents_state/plugins sandbox_envs simulation/results")

    set max_ag to env("HIVEANT_MAX_AGENTS")
    if max_ag:
        set max_agents_val to max_ag
    otherwise:
        set max_agents_val to 10000

    set kernel_state to {"version": "1.0.0", "name": "HiveANT", "started_at": time_iso(), "agent_count": 0, "max_agents": max_agents_val, "status": "running"}
    write_file("agents_state/kernel.json", json_encode(kernel_state))

    set sched to {"initialized_at": time_now(), "tasks_queued": 0, "tasks_dispatched": 0, "tasks_completed": 0, "tasks_failed": 0, "max_retries": 3}
    write_file("agents_state/scheduler.json", json_encode(sched))

    set db to {"version": "1.0.0", "nodes": {}, "edges": {}, "node_count": 0, "edge_count": 0, "total_reinforcements": 0, "total_evaporations": 0, "created_at": time_now(), "updated_at": time_now()}
    write_file("memory/pheromone/graph_db.json", json_encode(db))

    set twin to {"version": "1.0.0", "services": {}, "dependencies": [], "data_flows": [], "deployments": [], "service_count": 0, "last_updated": time_now(), "created_at": time_now()}
    write_file("memory/twins/system_model.json", json_encode(twin))

    set stability to {"max_agents_total": 1000, "max_agents_per_task": 20, "max_spawn_depth": 4, "task_timeout_seconds": 300, "duplicate_detection": true, "runaway_threshold_per_minute": 50}
    write_file("agents_state/stability_config.json", json_encode(stability))

    set perf to {"total_cycles": 0, "success_count": 0, "avg_time_seconds": 0, "avg_confidence": 0, "created_at": time_now()}
    write_file("knowledge/swarm_performance.json", json_encode(perf))

    set write_test to "hiveant_write_check_" + str(time_now())
    write_file("agents_state/.write_test", write_test)
    set read_back to read_file("agents_state/.write_test")
    if read_back is equal write_test:
        set write_ok to true
    otherwise:
        set write_ok to false
        log "CRITICAL: File write verification FAILED. Set NC_ALLOW_FILE_WRITE=1"

    log "HIVEANT KERNEL: All subsystems initialized (write_ok=" + str(write_ok) + ")"
    respond with {"kernel": kernel_state, "scheduler": "initialized", "pheromone_db": "initialized", "digital_twin": "initialized", "stability": "initialized", "file_write_verified": write_ok, "message": "HiveANT is ready."}

to kernel_status:
    purpose: "Get full system status"
    set kernel to "not initialized"
    if file_exists("agents_state/kernel.json"):
        try:
            set kernel to json_decode(read_file("agents_state/kernel.json"))
            set kernel.agent_count to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
            set kernel.uptime_seconds to time_now() - kernel.started_at
        on_error:
            set kernel to {"error": "corrupt kernel state"}

    set sched to "not initialized"
    if file_exists("agents_state/scheduler.json"):
        try:
            set sched to json_decode(read_file("agents_state/scheduler.json"))
        on_error:
            set sched to {"error": "corrupt scheduler state"}

    set pheromone to "not initialized"
    if file_exists("memory/pheromone/graph_db.json"):
        try:
            set pg to json_decode(read_file("memory/pheromone/graph_db.json"))
            set pheromone to {"edge_count": shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0"), "reinforcements": pg.total_reinforcements, "evaporations": pg.total_evaporations}
        on_error:
            set pheromone to {"error": "corrupt pheromone db"}

    set twin to "not initialized"
    if file_exists("memory/twins/system_model.json"):
        try:
            set tw to json_decode(read_file("memory/twins/system_model.json"))
            set twin to {"services": tw.service_count, "dependencies": len(tw.dependencies)}
        on_error:
            set twin to {"error": "corrupt twin model"}

    set incident_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")

    set stab to "not initialized"
    if file_exists("agents_state/stability_config.json"):
        try:
            set stab to json_decode(read_file("agents_state/stability_config.json"))
        on_error:
            set stab to {"error": "corrupt stability config"}

    respond with {"system": "HiveANT", "version": "1.0.0", "kernel": kernel, "scheduler": sched, "pheromone_graph": pheromone, "digital_twin": twin, "stability": stab, "incidents": incident_count}

// ══════════════════════════════════════════════════════════════════
//  AGENT MANAGEMENT (with stability checks)
// ══════════════════════════════════════════════════════════════════

to spawn_agent with agent_type, agent_id, config, token:
    purpose: "Spawn a new agent (with stability limit checks)"
    require_role(token, "operator")
    set valid_types to "detection investigation root_cause fix_generation validation learning prediction scout worker evaluator architect developer reviewer testing deployment"
    if agent_type:
        set safe_type to safe_id(agent_type)
    otherwise:
        respond with {"error": "agent_type is required"}

    if file_exists("agents_state/stability_config.json"):
        try:
            set stab to json_decode(read_file("agents_state/stability_config.json"))
            set current_count to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
            if current_count is above stab.max_agents_total:
                respond with {"error": "Agent limit reached", "limit": stab.max_agents_total, "current": current_count}
        on_error:
            log "WARN: Could not read stability config"

    if agent_id:
        set aid to replace(replace(str(agent_id), ";", ""), "|", "")
    otherwise:
        set aid to safe_type + "-" + str(floor(random() * 9000 + 1000))

    set agent to {"id": aid, "type": safe_type, "status": "running", "spawned_at": time_iso(), "last_heartbeat": time_iso(), "tasks_completed": 0, "cluster_id": "default"}
    write_file("agents_state/agent-" + aid + ".json", json_encode(agent))

    audit_log_action("agent_spawned", "operator", aid, {"agent_type": safe_type})
    log "KERNEL: Spawned agent " + aid + " (type=" + safe_type + ")"
    respond with agent

to kill_agent with agent_id, token:
    purpose: "Terminate an agent"
    require_role(token, "operator")
    set safe_id to safe_id(agent_id)
    set path to "agents_state/agent-" + safe_id + ".json"
    if file_exists(path):
        try:
            set agent to json_decode(read_file(path))
            set agent.status to "terminated"
            set agent.terminated_at to time_iso()
            write_file(path, json_encode(agent))

            audit_log_action("agent_killed", "operator", safe_id, {})

            respond with {"agent_id": safe_id, "status": "terminated"}
        on_error:
            respond with {"error": "Corrupt agent file", "_status": 500}
    otherwise:
        respond with {"error": "Agent not found", "_status": 404}

to list_agents with status_filter:
    purpose: "List all agents"
    set result to shell("ls agents_state/agent-*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"agents": [], "count": 0}
    set agents to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set data to json_decode(read_file(f))
                if status_filter:
                    if data.status is equal status_filter:
                        set agents to agents + [data]
                otherwise:
                    set agents to agents + [data]
            on_error:
                log "WARN: Corrupt agent file " + f
                skip
    respond with {"agents": agents, "count": len(agents)}

// ══════════════════════════════════════════════════════════════════
//  TASK GRAPH
// ══════════════════════════════════════════════════════════════════

to create_investigation_graph with service_name, description, token:
    purpose: "Auto-generate an investigation task graph"
    require_role(token, "operator")
    shell("mkdir -p memory/tasks")
    set graph_id to "TG-" + str(floor(random() * 90000 + 10000))
    set subtasks to [{"description": "Analyze system logs", "agent_type": "investigation", "priority": 1}, {"description": "Collect metrics", "agent_type": "investigation", "priority": 1}, {"description": "Inspect dependencies", "agent_type": "investigation", "priority": 2}, {"description": "Review deployments", "agent_type": "investigation", "priority": 2}, {"description": "ACO root cause analysis", "agent_type": "root_cause", "priority": 3}, {"description": "ABC remediation search", "agent_type": "fix_generation", "priority": 4}, {"description": "Sandbox validation", "agent_type": "validation", "priority": 5}, {"description": "Learn and update pheromones", "agent_type": "learning", "priority": 6}]
    set nodes to []
    set children to []
    set idx to 0
    repeat for each st in subtasks:
        set child_id to graph_id + "-T" + str(idx)
        set nodes to nodes + [{"id": child_id, "description": st.description, "agent_type": st.agent_type, "priority": st.priority, "status": "pending"}]
        set children to children + [child_id]
        set idx to idx + 1
    set graph to {"graph_id": graph_id, "goal": "Investigate " + str(service_name) + ": " + str(description), "service": service_name, "status": "active", "nodes": nodes, "children": children, "created_at": time_now()}
    write_file("memory/tasks/" + graph_id + ".json", json_encode(graph))
    respond with graph

to list_task_graphs:
    purpose: "List all task graphs"
    set result to shell("ls memory/tasks/TG-*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"graphs": [], "count": 0}
    set graphs to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set data to json_decode(read_file(f))
                set graphs to graphs + [{"graph_id": data.graph_id, "goal": data.goal, "status": data.status}]
            on_error:
                skip
    respond with {"graphs": graphs, "count": len(graphs)}

// ══════════════════════════════════════════════════════════════════
//  ANT COLONY — Root Cause Discovery (with validate)
// ══════════════════════════════════════════════════════════════════

to ant_explore with signals, system_context, token:
    purpose: "Run ant colony exploration to find root causes"
    require_role(token, "operator")
    shell("mkdir -p memory/pheromone/edges")
    set existing_paths to "none"
    if file_exists("memory/pheromone/causal_paths.json"):
        set existing_paths to read_file("memory/pheromone/causal_paths.json")
    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")

    set colony_size to env("ANT_COLONY_SIZE")
    if colony_size:
        set ants to colony_size
    otherwise:
        set ants to "50"

    ask AI to """You are an Ant Colony Optimization agent in HiveANT.
Explore causal paths from symptoms to root causes.

SIGNALS: {{signals}}.
SYSTEM CONTEXT: {{system_context}}.
EXISTING PHEROMONE PATHS: {{existing_paths}}.
LEARNED PATTERNS: {{semantic_mem}}.

Simulate {{ants}} ants exploring the causal graph.
Each ant builds: symptom -> intermediate -> root_cause.
Score paths by evidence strength. Reinforce the best paths.

Return ONLY valid JSON:
{
  "paths_explored": [{"ant_id": 0, "path": ["node1", "node2"], "description": "string", "pheromone_score": 0.0, "confidence": 0.0}],
  "best_path": {"path": ["node1", "root_cause"], "description": "string", "pheromone_score": 0.0, "confidence": 0.0},
  "root_cause_candidates": [{"cause": "string", "probability": 0.0, "total_pheromone": 0.0}],
  "pheromone_updates": [{"from": "string", "to": "string", "score": 0.0}]
}""" save as aco_result

    set aco_valid to validate(aco_result, ["best_path"])
    if aco_valid.valid is not equal yes:
        set aco_result to {"paths_explored": [], "best_path": {"path": [], "description": "ACO analysis failed — AI returned unexpected format", "pheromone_score": 0.0, "confidence": 0.0}, "root_cause_candidates": [], "pheromone_updates": []}

    write_file("memory/pheromone/causal_paths.json", json_encode(aco_result))

    if aco_result.pheromone_updates:
        repeat for each u in aco_result.pheromone_updates:
            set edge_file to "memory/pheromone/edges/" + str(u.from) + "__" + str(u.to) + ".json"
            set edge_data to {"from": u.from, "to": u.to, "pheromone_score": u.score, "last_updated": time_now()}
            write_file(edge_file, json_encode(edge_data))
        set db_path to "memory/pheromone/graph_db.json"
        if file_exists(db_path):
            set db to json_decode(read_file(db_path))
            set db.updated_at to time_now()
            set db.total_reinforcements to db.total_reinforcements + 1
            set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
            set db.edge_count to edge_count
            write_file(db_path, json_encode(db))

    log "ACO: Best path confidence = " + str(aco_result.best_path.confidence)
    respond with aco_result

// ══════════════════════════════════════════════════════════════════
//  BEE COLONY — Remediation Optimization (with validate)
// ══════════════════════════════════════════════════════════════════

to bee_optimize with root_cause, system_context, constraints, token:
    purpose: "Run bee colony to find optimal remediation"
    require_role(token, "operator")
    set past_fixes to "none"
    if file_exists("knowledge/procedural.json"):
        set past_fixes to read_file("knowledge/procedural.json")

    ask AI to """You are the Artificial Bee Colony in HiveANT.
Run Scout -> Worker -> Evaluator phases.

ROOT CAUSE: {{root_cause}}.
SYSTEM: {{system_context}}.
CONSTRAINTS: {{constraints}}.
PAST FIXES: {{past_fixes}}.

Scout Phase: Find 5+ diverse fix strategies (restart, scale, config, rollback, patch, infra).
Worker Phase: Evaluate each for effectiveness, safety, speed, reversibility.
Evaluator Phase: Select optimal fix with execution plan.

Return ONLY valid JSON:
{
  "recommended_fix": {
    "solution_id": "string", "category": "string", "description": "string",
    "commands": [{"step": 0, "cmd": "string", "description": "string", "risk": "low|medium|high"}],
    "pre_checks": ["string"], "post_checks": ["string"],
    "rollback_commands": [{"cmd": "string", "description": "string"}],
    "estimated_recovery_minutes": 0, "confidence_percent": 0, "risk_level": "string",
    "fitness": {"effectiveness": 0.0, "safety": 0.0, "speed": 0.0, "reversibility": 0.0, "total": 0.0}
  },
  "alternative_fixes": [{"id": "string", "description": "string", "when_to_use": "string", "fitness": 0.0}],
  "scout_count": 0, "worker_evaluations": 0
}""" save as abc_result

    set abc_valid to validate(abc_result, ["recommended_fix"])
    if abc_valid.valid is not equal yes:
        set abc_result to {"recommended_fix": {"solution_id": "FIX-MANUAL", "category": "manual", "description": "ABC optimization failed — manual investigation required", "commands": [], "confidence_percent": 0, "risk_level": "low"}, "alternative_fixes": [], "scout_count": 0, "worker_evaluations": 0}

    shell("mkdir -p memory/pheromone")
    write_file("memory/pheromone/abc_full_result.json", json_encode(abc_result))
    respond with abc_result

// ══════════════════════════════════════════════════════════════════
//  FULL SWARM INVESTIGATION (with validate on all AI + perf tracking)
// ══════════════════════════════════════════════════════════════════

to swarm_investigate with service_name, description, token:
    purpose: "Full autonomous investigation pipeline"
    require_role(token, "operator")

    set ai_url to env("NC_AI_URL")
    if ai_url:
        set ai_check to shell("curl -sf --connect-timeout 3 " + str(ai_url) + " -o /dev/null 2>/dev/null && echo OK || echo FAIL")
        if ai_check is equal "FAIL":
            respond with {"error": "AI model is not reachable at " + str(ai_url) + ". Start Ollama or check NC_AI_URL.", "_status": 503}
    otherwise:
        respond with {"error": "NC_AI_URL not configured. Set it in config/.env", "_status": 503}

    set inv_start to time_now()
    audit_log_action("investigation_started", "swarm", str(service_name), {"description": str(description)})
    log "HIVEANT SWARM: Full investigation for " + str(service_name) + " — " + str(description)
    shell("mkdir -p incidents knowledge docs memory/pheromone/edges memory/twins memory/tasks")

    set safe_svc to safe_id(service_name)

    set prom_url to env("PROMETHEUS_URL")
    set es_url to env("ELASTICSEARCH_URL")
    set gh_token to env("GITHUB_TOKEN")
    set metrics_data to "No Prometheus configured"
    set log_data to "No Elasticsearch configured"
    set deploy_data to "No GitHub configured"
    set health_data to "No health endpoints configured"

    if prom_url:
        set eq to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + safe_svc + "\"}[5m])"
        gather prom_errors from eq
        set lq to prom_url + "/api/v1/query?query=histogram_quantile(0.99,rate(http_request_duration_seconds_bucket{service=\"" + safe_svc + "\"}[5m]))"
        gather prom_latency from lq
        set metrics_data to json_encode({"error_rate": prom_errors, "latency_p99": prom_latency})

    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + safe_svc + "+AND+level:ERROR&size=30&sort=@timestamp:desc"
        gather es_logs from log_url
        set log_data to json_encode(es_logs)

    if gh_token:
        set gh_repo to env("GITHUB_REPO")
        set d_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=10"
        gather gh_commits from d_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        set deploy_data to json_encode(gh_commits)

    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")
    set procedural_mem to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural_mem to read_file("knowledge/procedural.json")
    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    set past_incidents to shell("ls incidents/*.json 2>/dev/null | tail -10 | while read f; do cat \"$f\" 2>/dev/null | head -c 300; echo '---'; done || echo NONE")
    set rag_context to "none"
    set all_docs to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
    if len(all_docs) is above 10:
        set doc_chunks to chunk(all_docs, 600, 80)
        set rag_context to str(top_k(doc_chunks, 3))

    set full_context to "SERVICE: {{service_name}}. ISSUE: {{description}}. METRICS: {{metrics_data}}. LOGS: {{log_data}}. DEPLOYS: {{deploy_data}}. HEALTH: {{health_data}}."
    set memory_context to "SEMANTIC: {{semantic_mem}}. PROCEDURAL: {{procedural_mem}}. RL: {{rl_policy}}. PAST INCIDENTS: {{past_incidents}}. DOCS: {{rag_context}}."

    ask AI to """You are the Investigation Agent in HiveANT.
Analyze production telemetry.

{{full_context}}.

Return ONLY valid JSON:
{
  "signals": [{"name": "string", "severity": "critical|high|medium|low", "evidence": "string"}],
  "error_pattern": "string",
  "metric_anomalies": [{"metric": "string", "value": "string", "normal": "string"}],
  "summary": "string"
}""" save as investigation

    set inv_valid to validate(investigation, ["summary"])
    if inv_valid.valid is not equal yes:
        set investigation to {"signals": [], "error_pattern": "AI analysis returned unexpected format", "metric_anomalies": [], "summary": "Investigation failed — using available raw data only"}

    ask AI to """You are the Root Cause Agent using Ant Colony Optimization.

SIGNALS: {{investigation}}.
SYSTEM: {{full_context}}.
MEMORY: {{memory_context}}.

Simulate ants exploring causal paths.
If semantic memory matches, boost confidence 15 percent.

Return ONLY valid JSON:
{
  "root_cause": "string", "confidence_percent": 0,
  "category": "configuration|deployment|code-bug|infrastructure|dependency|resource-exhaustion|network|security",
  "causal_chain": [{"node": "string", "type": "symptom|intermediate|root_cause", "pheromone_score": 0.0}],
  "evidence_chain": ["string"],
  "timeline": [{"time": "string", "event": "string"}],
  "affected_services": ["string"],
  "pheromone_updates": [{"from": "string", "to": "string", "score": 0.0}],
  "learning": "string"
}""" save as root_cause

    set rc_valid to validate(root_cause, ["root_cause", "confidence_percent"])
    if rc_valid.valid is not equal yes:
        set root_cause to {"root_cause": "Unable to determine", "confidence_percent": 0, "category": "unknown", "causal_chain": [], "evidence_chain": [], "affected_services": [service_name], "pheromone_updates": [], "timeline": []}

    ask AI to """You are the Fix Generation Agent using Artificial Bee Colony.

ROOT CAUSE: {{root_cause}}.
SYSTEM: {{full_context}}.
PAST FIXES: {{procedural_mem}}.

Run Scout->Worker->Evaluator.

Return ONLY valid JSON:
{
  "recommended_fix": {
    "solution_id": "string", "category": "string", "description": "string",
    "commands": [{"step": 0, "cmd": "string", "description": "string", "risk": "low|medium|high"}],
    "pre_checks": ["string"], "post_checks": ["string"],
    "rollback_commands": [{"cmd": "string", "description": "string"}],
    "estimated_recovery_minutes": 0, "confidence_percent": 0, "risk_level": "string"
  },
  "alternative_fixes": [{"id": "string", "description": "string"}],
  "verification": ["string"], "prevention": ["string"]
}""" save as remediation

    set rem_valid to validate(remediation, ["recommended_fix"])
    if rem_valid.valid is not equal yes:
        set remediation to {"recommended_fix": {"solution_id": "FIX-MANUAL", "description": "Manual investigation required", "commands": [{"step": 1, "cmd": "kubectl get pods -A", "description": "Check pods", "risk": "low"}], "confidence_percent": 0, "risk_level": "low"}, "alternative_fixes": [], "verification": [], "prevention": []}

    if root_cause.pheromone_updates:
        repeat for each u in root_cause.pheromone_updates:
            set edge_file to "memory/pheromone/edges/" + str(u.from) + "__" + str(u.to) + ".json"
            set edge_data to {"from": u.from, "to": u.to, "pheromone_score": u.score, "last_updated": time_now()}
            write_file(edge_file, json_encode(edge_data))
        set db_path to "memory/pheromone/graph_db.json"
        if file_exists(db_path):
            set db to json_decode(read_file(db_path))
            set db.total_reinforcements to db.total_reinforcements + 1
            set db.updated_at to time_now()
            set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
            set db.edge_count to edge_count
            write_file(db_path, json_encode(db))

    set incident_id to "INC-" + str(floor(random() * 9000 + 1000))
    set inv_duration to time_now() - inv_start
    set report to {"incident_id": incident_id, "service": service_name, "description": description, "root_cause": root_cause.root_cause, "confidence": root_cause.confidence_percent, "category": root_cause.category, "causal_chain": root_cause.causal_chain, "evidence_chain": root_cause.evidence_chain, "timeline": root_cause.timeline, "affected_services": root_cause.affected_services, "investigation": investigation, "remediation": remediation, "duration_seconds": inv_duration, "data_sources": {"prometheus": prom_url, "elasticsearch": es_url, "github": "configured"}, "investigated_at": time_iso()}

    write_file("incidents/" + incident_id + ".json", json_encode(report))
    audit_log_action("investigation_completed", "swarm", incident_id, {"service": service_name, "root_cause": root_cause.root_cause, "confidence": root_cause.confidence_percent, "duration_seconds": inv_duration})
    log "HIVEANT: " + incident_id + " complete in " + str(inv_duration) + "s — " + root_cause.root_cause

    set perf_path to "knowledge/swarm_performance.json"
    if file_exists(perf_path):
        try:
            set perf to json_decode(read_file(perf_path))
            set perf.total_cycles to perf.total_cycles + 1
            set perf.avg_confidence to root_cause.confidence_percent
            set perf.avg_time_seconds to inv_duration
            write_file(perf_path, json_encode(perf))
        on_error:
            log "WARN: Could not update swarm performance"

    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set slack_msg to "{\"text\":\"*HiveANT — " + incident_id + "*\\n*Service:* " + str(service_name) + "\\n*Root Cause:* " + root_cause.root_cause + "\\n*Confidence:* " + str(root_cause.confidence_percent) + "%\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg

    respond with report

// ══════════════════════════════════════════════════════════════════
//  SELF-HEAL (with validate + sanitized commands)
// ══════════════════════════════════════════════════════════════════

to self_heal with service_name, description, auto_execute, token:
    purpose: "Full self-healing: investigate + fix + verify"
    require_role(token, "admin")

    set safe_svc to safe_id(service_name)
    log "HIVEANT SELF-HEAL: " + safe_svc

    set context to "SERVICE: " + safe_svc + ". ISSUE: " + str(description)

    set metrics_data to "not configured"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set eq to prom_url + "/api/v1/query?query=rate(http_requests_total{status=~\"5..\",service=\"" + safe_svc + "\"}[5m])"
        gather metrics_data from eq

    set log_data to "not configured"
    set es_url to env("ELASTICSEARCH_URL")
    if es_url:
        set log_url to es_url + "/logs-*/_search?q=service:" + safe_svc + "+AND+level:ERROR&size=20&sort=@timestamp:desc"
        gather log_data from log_url

    set health_data to "unknown"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health_data to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    set full_ctx to context + " METRICS: " + str(metrics_data) + " LOGS: " + str(log_data) + " HEALTH: " + str(health_data)

    ask AI to "You are HiveANT Self-Healing Agent. Diagnose AND provide exact fix commands. Commands must be safe, idempotent, and reversible. Context: {{full_ctx}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"fix_commands\": [{\"command\": \"string\", \"description\": \"string\", \"risk\": \"low|medium|high\", \"reversible\": true}], \"verify_command\": \"string\", \"rollback_commands\": [{\"command\": \"string\", \"description\": \"string\"}], \"safe_to_auto_execute\": true}" save as diagnosis

    set diag_valid to validate(diagnosis, ["root_cause", "confidence_percent"])
    if diag_valid.valid is not equal yes:
        set diagnosis to {"root_cause": "Unable to determine", "confidence_percent": 0, "category": "unknown", "fix_commands": [], "verify_command": "", "rollback_commands": [], "safe_to_auto_execute": false}

    set conf_threshold to env("SELF_HEAL_CONFIDENCE_THRESHOLD")
    if conf_threshold:
        set threshold to conf_threshold
    otherwise:
        set threshold to 70

    set executed to "no"
    set exec_results to []

    if auto_execute is equal "yes":
        if diagnosis.confidence_percent is above threshold:
            if diagnosis.safe_to_auto_execute:
                repeat for each fix_cmd in diagnosis.fix_commands:
                    if fix_cmd.risk is not equal "high":
                        set cmd_check to validate_command(fix_cmd.command)
                        if cmd_check.allowed:
                            log "EXECUTING: " + fix_cmd.command
                            set cmd_result to shell(fix_cmd.command)
                            set exec_results to exec_results + [{"command": fix_cmd.command, "result": cmd_result, "status": "executed"}]
                            audit_log_action("command_executed", "self_heal", fix_cmd.command, {"risk": fix_cmd.risk})
                        otherwise:
                            set exec_results to exec_results + [{"command": fix_cmd.command, "result": "BLOCKED — " + cmd_check.reason, "status": "blocked"}]
                            audit_log_action("command_blocked", "self_heal", fix_cmd.command, {"reason": cmd_check.reason})
                    otherwise:
                        set exec_results to exec_results + [{"command": fix_cmd.command, "result": "SKIPPED — high risk", "status": "skipped"}]
                set executed to "yes"

    set verification to "not run"
    if executed is equal "yes":
        if diagnosis.verify_command:
            set verify_check to validate_command(diagnosis.verify_command)
            if verify_check.allowed:
                set verification to shell(diagnosis.verify_command)
            otherwise:
                set verification to "BLOCKED — " + verify_check.reason

    set incident_id to "HEAL-" + str(floor(random() * 9000 + 1000))
    set report to {"incident_id": incident_id, "service": safe_svc, "description": description, "root_cause": diagnosis.root_cause, "confidence": diagnosis.confidence_percent, "category": diagnosis.category, "fix_commands": diagnosis.fix_commands, "rollback_commands": diagnosis.rollback_commands, "auto_executed": executed, "execution_results": exec_results, "verification": verification, "healed_at": time_now()}
    write_file("incidents/" + incident_id + ".json", json_encode(report))

    audit_log_action("self_heal", "swarm", incident_id, {"service": safe_svc, "auto_executed": executed, "root_cause": diagnosis.root_cause, "confidence": diagnosis.confidence_percent})

    respond with report

// ══════════════════════════════════════════════════════════════════
//  LEARNING & FEEDBACK (with validate)
// ══════════════════════════════════════════════════════════════════

to add_feedback with incident_id, correct, notes, token:
    purpose: "Process feedback and update all memory layers"
    require_role(token, "operator")
    set safe_id to safe_id(incident_id)
    set path to "incidents/" + safe_id + ".json"
    if file_exists(path):
        try:
            set incident to json_decode(read_file(path))
        on_error:
            respond with {"error": "Corrupt incident file", "_status": 500}
        set incident.feedback_correct to correct
        set incident.feedback_notes to notes
        set incident.feedback_at to time_now()
        write_file(path, json_encode(incident))

        shell("mkdir -p knowledge")

        set rl_alpha to env("RL_ALPHA")
        if rl_alpha:
            set alpha to rl_alpha
        otherwise:
            set alpha to 0.1

        set reward to 0.0
        if correct is equal "yes":
            set reward to 1.0
        otherwise:
            set reward to -0.5

        set state_key to str(incident.category)
        set rl_path to "knowledge/rl-policy.json"
        if file_exists(rl_path):
            set rl_data to json_decode(read_file(rl_path))
        otherwise:
            set rl_ep to env("RL_EPSILON")
            if rl_ep:
                set init_epsilon to rl_ep
            otherwise:
                set init_epsilon to 0.2
            set rl_data to {"states": {}, "total_episodes": 0, "alpha": alpha, "epsilon": init_epsilon}

        set rl_data.total_episodes to rl_data.total_episodes + 1
        if rl_data.epsilon is above 0.05:
            set rl_data.epsilon to rl_data.epsilon * 0.95
        write_file(rl_path, json_encode(rl_data))

        if correct is equal "yes":
            set sem_path to "knowledge/semantic.json"
            if file_exists(sem_path):
                set sem to json_decode(read_file(sem_path))
            otherwise:
                set sem to {"patterns": [], "total": 0}
            set sem.total to sem.total + 1
            ask AI to "Extract a reusable pattern. Service={{incident.service}}, Category={{incident.category}}, Root_cause={{incident.root_cause}}. Return ONLY valid JSON: {\"pattern\": \"string\", \"trigger_signals\": [\"string\"], \"confidence\": 0.0}" save as learned
            set lv to validate(learned, ["pattern"])
            if lv.valid is equal yes:
                set sem.patterns to sem.patterns + [{"pattern": learned.pattern, "triggers": learned.trigger_signals, "confidence": learned.confidence, "from": safe_id, "learned_at": time_now()}]
                write_file(sem_path, json_encode(sem))

            set proc_path to "knowledge/procedural.json"
            if file_exists(proc_path):
                set proc to json_decode(read_file(proc_path))
            otherwise:
                set proc to {"runbooks": [], "total": 0}
            if incident.remediation:
                set proc.total to proc.total + 1
                set proc.runbooks to proc.runbooks + [{"trigger": state_key, "root_cause": str(incident.root_cause), "fix": incident.remediation, "from": safe_id, "learned_at": time_now()}]
                write_file(proc_path, json_encode(proc))

            set perf_path to "knowledge/swarm_performance.json"
            if file_exists(perf_path):
                set perf to json_decode(read_file(perf_path))
                set perf.success_count to perf.success_count + 1
                write_file(perf_path, json_encode(perf))

        audit_log_action("feedback_submitted", "user", safe_id, {"correct": correct, "reward": reward, "notes": notes})
        respond with {"incident_id": safe_id, "feedback": correct, "reward": reward, "rl_episodes": rl_data.total_episodes}
    otherwise:
        respond with {"error": "Incident not found", "_status": 404}

to get_knowledge:
    purpose: "View all learned knowledge"
    shell("mkdir -p knowledge")
    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")
    set procedural to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural to read_file("knowledge/procedural.json")
    set rl to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl to read_file("knowledge/rl-policy.json")
    set inc_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    respond with {"episodic_count": inc_count, "semantic": semantic, "procedural": procedural, "rl_policy": rl}

// ══════════════════════════════════════════════════════════════════
//  PREDICTION (with validate)
// ══════════════════════════════════════════════════════════════════

to predict with service_name, time_horizon:
    purpose: "Predict future failures"
    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")
    set pheromone to "none"
    if file_exists("memory/pheromone/graph_db.json"):
        set pheromone to read_file("memory/pheromone/graph_db.json")
    set history to shell("ls incidents/*.json 2>/dev/null | tail -20 | while read f; do cat \"$f\" 2>/dev/null | head -c 200; echo '---'; done || echo NONE")

    if time_horizon:
        set horizon to time_horizon
    otherwise:
        set horizon to "24h"

    ask AI to "You are the Prediction Agent in HiveANT. Forecast failures for {{service_name}} over {{horizon}}. PATTERNS: {{semantic}}. PHEROMONE GRAPH: {{pheromone}}. HISTORY: {{history}}. Return ONLY valid JSON: {\"predictions\": [{\"failure_type\": \"string\", \"probability_percent\": 0, \"severity\": \"critical|high|medium|low\", \"trigger_conditions\": [\"string\"], \"prevention_steps\": [\"string\"]}], \"risk_score\": 0.0, \"risk_level\": \"critical|high|medium|low\", \"proactive_actions\": [{\"action\": \"string\", \"priority\": \"string\"}]}" save as preds

    set pred_valid to validate(preds, ["predictions"])
    if pred_valid.valid is not equal yes:
        set preds to {"predictions": [], "risk_score": 0.0, "risk_level": "unknown", "proactive_actions": []}

    set preds.service to service_name
    set preds.predicted_at to time_now()
    audit_log_action("prediction_run", "user", str(service_name), {"horizon": horizon, "risk_score": preds.risk_score})
    respond with preds

// ══════════════════════════════════════════════════════════════════
//  DIGITAL TWIN
// ══════════════════════════════════════════════════════════════════

to twin_register with service_name, service_type, endpoints, metadata, token:
    purpose: "Register service in digital twin"
    require_role(token, "operator")
    shell("mkdir -p memory/twins/services")
    set safe_svc to safe_id(service_name)
    set svc to {"name": safe_svc, "type": service_type, "endpoints": endpoints, "metadata": metadata, "health": "unknown", "registered_at": time_now()}
    write_file("memory/twins/services/" + safe_svc + ".json", json_encode(svc))
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        set twin.service_count to twin.service_count + 1
        set twin.last_updated to time_now()
        write_file(twin_path, json_encode(twin))
    respond with {"service": safe_svc, "status": "registered"}

to twin_add_dep with from_service, to_service, dep_type, token:
    purpose: "Add dependency to digital twin"
    require_role(token, "operator")
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        set twin.dependencies to twin.dependencies + [{"from": from_service, "to": to_service, "type": dep_type, "added_at": time_now()}]
        set twin.last_updated to time_now()
        write_file(twin_path, json_encode(twin))
        respond with {"added": true, "total_deps": len(twin.dependencies)}
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_topology:
    purpose: "Get system topology"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        set svc_list to shell("ls memory/twins/services/*.json 2>/dev/null | while read f; do cat \"$f\" 2>/dev/null; echo ','; done || echo ''")
        set twin.service_details to svc_list
        respond with twin
    otherwise:
        respond with {"error": "Twin not initialized"}

to twin_impact with service_name:
    purpose: "Blast radius analysis"
    set twin_path to "memory/twins/system_model.json"
    if file_exists(twin_path):
        set twin to json_decode(read_file(twin_path))
        ask AI to "Analyze blast radius if '{{service_name}}' fails. TOPOLOGY: {{twin}}. Return ONLY valid JSON: {\"failed_service\": \"string\", \"directly_affected\": [\"string\"], \"indirectly_affected\": [\"string\"], \"blast_radius_percent\": 0, \"cascade_risk\": \"high|medium|low\", \"mitigation\": [\"string\"]}" save as impact
        set impact_valid to validate(impact, ["failed_service"])
        if impact_valid.valid is not equal yes:
            set impact to {"failed_service": service_name, "directly_affected": [], "indirectly_affected": [], "blast_radius_percent": 0, "cascade_risk": "unknown", "mitigation": ["Manual analysis required"]}
        respond with impact
    otherwise:
        respond with {"error": "Twin not initialized"}

// ══════════════════════════════════════════════════════════════════
//  CLUSTERING
// ══════════════════════════════════════════════════════════════════

to cluster_init with cluster_count, token:
    purpose: "Initialize swarm clusters"
    require_role(token, "admin")
    shell("mkdir -p agents_state/clusters")
    if cluster_count:
        set num to cluster_count
    otherwise:
        set cl_env to env("HIVEANT_CLUSTERS")
        if cl_env:
            set num to cl_env
        otherwise:
            set num to 10
    set cap_env to env("HIVEANT_AGENTS_PER_CLUSTER")
    if cap_env:
        set cap to cap_env
    otherwise:
        set cap to 100
    set coordinator to {"cluster_count": num, "clusters": [], "status": "active", "created_at": time_now()}
    set idx to 0
    repeat for each i in range(num):
        set cid to "CLUSTER-" + str(idx)
        write_file("agents_state/clusters/" + cid + ".json", json_encode({"id": cid, "active_agents": 0, "capacity": cap, "status": "ready"}))
        set coordinator.clusters to coordinator.clusters + [cid]
        set idx to idx + 1
    write_file("agents_state/coordinator.json", json_encode(coordinator))
    respond with coordinator

to cluster_status:
    purpose: "Get cluster status"
    if file_exists("agents_state/coordinator.json"):
        set coord to json_decode(read_file("agents_state/coordinator.json"))
        set clusters to []
        repeat for each cid in coord.clusters:
            set cp to "agents_state/clusters/" + cid + ".json"
            if file_exists(cp):
                set clusters to clusters + [json_decode(read_file(cp))]
        respond with {"coordinator": "active", "cluster_count": coord.cluster_count, "clusters": clusters}
    otherwise:
        respond with {"error": "Clustering not initialized"}

// ══════════════════════════════════════════════════════════════════
//  PHEROMONE GRAPH (with evaporation fix)
// ══════════════════════════════════════════════════════════════════

to pheromone_status:
    purpose: "Get pheromone graph stats"
    set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
    if file_exists("memory/pheromone/graph_db.json"):
        set db to json_decode(read_file("memory/pheromone/graph_db.json"))
        respond with {"edges": edge_count, "reinforcements": db.total_reinforcements, "evaporations": db.total_evaporations, "last_updated": db.updated_at}
    otherwise:
        respond with {"status": "not_initialized", "edges": edge_count}

to pheromone_add with from_node, to_node, score, token:
    purpose: "Add or reinforce a pheromone edge"
    require_role(token, "operator")
    set safe_from to safe_id(from_node)
    set safe_to to safe_id(to_node)
    shell("mkdir -p memory/pheromone/edges")
    set edge_file to "memory/pheromone/edges/" + safe_from + "__" + safe_to + ".json"
    if score:
        set s to score
    otherwise:
        set s to 1.0
    set new_score to s
    if file_exists(edge_file):
        set existing to json_decode(read_file(edge_file))
        set new_score to existing.pheromone_score + s
    set edge_data to {"from": safe_from, "to": safe_to, "pheromone_score": new_score, "visit_count": 1, "last_updated": time_now()}
    write_file(edge_file, json_encode(edge_data))
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        set db to json_decode(read_file(db_path))
        set db.total_reinforcements to db.total_reinforcements + 1
        set db.updated_at to time_now()
        set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
        set db.edge_count to edge_count
        write_file(db_path, json_encode(db))
    respond with {"edge": safe_from + "->" + safe_to, "score": new_score}

to pheromone_evaporate with decay_rate:
    purpose: "Apply pheromone evaporation to all edges"
    if decay_rate:
        set rate to decay_rate
    otherwise:
        set rate_env to env("PHEROMONE_DECAY_RATE")
        if rate_env:
            set rate to rate_env
        otherwise:
            set rate to 0.05
    set min_env to env("PHEROMONE_MIN_THRESHOLD")
    if min_env:
        set min_threshold to min_env
    otherwise:
        set min_threshold to 0.01

    set edge_files to shell("ls memory/pheromone/edges/*.json 2>/dev/null || echo NONE")
    if edge_files is equal "NONE":
        respond with {"evaporated": 0, "removed": 0, "rate": rate}

    set evaporated to 0
    set removed to 0
    set files to split(edge_files, "\n")
    repeat for each f in files:
        if file_exists(f):
            set edge to json_decode(read_file(f))
            set new_score to edge.pheromone_score * (1.0 - rate)
            if new_score is below min_threshold:
                delete_file(f)
                set removed to removed + 1
            otherwise:
                set edge.pheromone_score to new_score
                set edge.last_updated to time_now()
                write_file(f, json_encode(edge))
                set evaporated to evaporated + 1

    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        set db to json_decode(read_file(db_path))
        set db.total_evaporations to db.total_evaporations + 1
        set db.updated_at to time_now()
        set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
        set db.edge_count to edge_count
        write_file(db_path, json_encode(db))

    log "PHEROMONE: Evaporation complete — decayed=" + str(evaporated) + " removed=" + str(removed)
    respond with {"evaporated": evaporated, "removed": removed, "rate": rate}

to pheromone_query with from_node:
    purpose: "Query edges from a specific node"
    set safe_from to safe_id(from_node)
    set result to shell("ls memory/pheromone/edges/" + safe_from + "__*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"from": safe_from, "edges": [], "count": 0}
    set edges to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set edges to edges + [json_decode(read_file(f))]
            on_error:
                skip
    respond with {"from": safe_from, "edges": edges, "count": len(edges)}

// ══════════════════════════════════════════════════════════════════
//  CHAT (with validate)
// ══════════════════════════════════════════════════════════════════

to chat with message, session_id:
    purpose: "Chat with HiveANT"
    set mem to memory_new(30)
    memory_add(mem, "user", message)
    set inc_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set knowledge to "none"
    if file_exists("knowledge/semantic.json"):
        set knowledge to read_file("knowledge/semantic.json")
    set context to "You are HiveANT, a Swarm Autonomous AI OS. You have " + str(inc_count) + " past incidents. Knowledge: " + str(knowledge)
    set history to memory_summary(mem)
    ask AI to "{{context}}. Conversation: {{history}}. User: {{message}}. Respond helpfully about incidents, system health, swarm operations, or infrastructure." save as reply
    memory_add(mem, "assistant", str(reply))
    respond with {"reply": reply, "session": session_id}

// ══════════════════════════════════════════════════════════════════
//  INCIDENTS & DATA RETENTION
// ══════════════════════════════════════════════════════════════════

to list_incidents:
    set result to shell("ls -t incidents/*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"incidents": [], "count": 0}
    set ids to split(result, "\n")
    respond with {"incidents": ids, "count": len(ids)}

to get_incident with incident_id:
    set safe_id to safe_id(incident_id)
    set path to "incidents/" + safe_id + ".json"
    if file_exists(path):
        respond with json_decode(read_file(path))
    otherwise:
        respond with {"error": "Not found", "_status": 404}

to incident_stats:
    purpose: "Get incident statistics"
    set total to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set categories to shell("cat incidents/*.json 2>/dev/null | grep -o '\"category\":\"[^\"]*\"' | sort | uniq -c | sort -rn | head -10 2>/dev/null || echo 'none'")
    respond with {"total": total, "categories": categories}

to cleanup_old with days, token:
    purpose: "Delete incidents and terminated agents older than N days"
    require_role(token, "admin")
    set d to str(validate_integer(days, 1, 365, 30))
    set deleted_incidents to shell("find incidents/ -name '*.json' -mtime +" + d + " -delete -print 2>/dev/null | wc -l || echo 0")
    set deleted_agents to shell("find agents_state/ -name 'agent-*.json' -mtime +" + d + " -delete -print 2>/dev/null | wc -l || echo 0")
    set deleted_audit to shell("find agents_state/audit/ -name 'AUD-*.json' -mtime +" + d + " -delete -print 2>/dev/null | wc -l || echo 0")
    log "CLEANUP: Removed " + str(deleted_incidents) + " incidents, " + str(deleted_agents) + " agents, " + str(deleted_audit) + " audit entries older than " + d + " days"
    respond with {"deleted_incidents": deleted_incidents, "deleted_agents": deleted_agents, "deleted_audit": deleted_audit, "older_than_days": d}

// ══════════════════════════════════════════════════════════════════
//  SAMPLE DATA & CONFIG
// ══════════════════════════════════════════════════════════════════

to sample_data:
    respond with {"service_name": "checkout-api", "description": "Checkout failing with HTTP 500 errors since 15:42 UTC", "logs": "2024-03-15 15:42:03 ERROR [checkout-api] FATAL: password authentication failed for user checkout_svc. 2024-03-15 15:42:04 ERROR [checkout-api] ConnectionPool exhausted: 0/20 connections. 2024-03-15 15:42:05 WARN [checkout-api] Circuit breaker OPEN for database. 2024-03-15 15:41:55 INFO [deploy-bot] Deployed checkout-api v2.4.1 (updated db credentials)", "metrics": "checkout-api latency_p99: 5200ms (normal: 45ms), error_rate: 94% (normal: 0.1%), postgresql connections: 148/200 (normal: 45/200)", "errors": "FATAL: password authentication failed (x342 in 1 min), ConnectionPool exhausted (x156), Upstream timeout (x89)", "deploy_history": "15:42 UTC - checkout-api v2.4.1 - Updated db credentials config"}

to show_config with token:
    purpose: "Show connected data sources (admin only)"
    require_role(token, "admin")
    set prom to env("PROMETHEUS_URL")
    if prom:
        set prom to "configured"
    set es to env("ELASTICSEARCH_URL")
    if es:
        set es to "configured"
    set gh to env("GITHUB_TOKEN")
    if gh:
        set gh to "configured"
    set slack to env("SLACK_WEBHOOK")
    if slack:
        set slack to "configured"
    set urls to env("MONITOR_URLS")
    if urls:
        set urls to "configured"
    set ai_key to env("NC_AI_KEY")
    set ai_url to env("NC_AI_URL")
    set ai_model to env("NC_AI_MODEL")
    if ai_key:
        set ai_key to "configured"
    respond with {"system": "HiveANT", "version": "1.0.0", "ai": {"url": ai_url, "model": ai_model, "key": ai_key}, "prometheus": prom, "elasticsearch": es, "github": gh, "slack": slack, "monitor_urls": urls}

// ══════════════════════════════════════════════════════════════════
//  UI & HEALTH
// ══════════════════════════════════════════════════════════════════

to home:
    set html to read_file("public/index.html")
    respond with html

to health:
    respond with {"status": "healthy", "service": "hiveant", "version": "1.0.0"}

to health_deep:
    purpose: "Deep health check — verifies AI, file write, external services"
    set checks to {"service": "hiveant", "version": "1.0.0"}
    set checks.kernel to file_exists("agents_state/kernel.json")
    set write_test to "health_check_" + str(time_now())
    write_file("agents_state/.health_test", write_test)
    set read_back to read_file("agents_state/.health_test")
    if read_back is equal write_test:
        set checks.file_write to "ok"
    otherwise:
        set checks.file_write to "FAILED"
    set checks.ai_configured to false
    set ai_url to env("NC_AI_URL")
    if ai_url:
        set checks.ai_configured to true
        set checks.ai_url to ai_url
        set checks.ai_model to env("NC_AI_MODEL")
    set checks.prometheus to false
    set prom to env("PROMETHEUS_URL")
    if prom:
        set checks.prometheus to prom
    set checks.elasticsearch to false
    set es to env("ELASTICSEARCH_URL")
    if es:
        set checks.elasticsearch to es
    set agent_count to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
    set checks.active_agents to agent_count
    set incident_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set checks.total_incidents to incident_count
    set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
    set checks.pheromone_edges to edge_count
    if checks.kernel:
        if checks.file_write is equal "ok":
            if checks.ai_configured:
                set checks.status to "healthy"
            otherwise:
                set checks.status to "degraded — no AI model configured"
        otherwise:
            set checks.status to "unhealthy — file write broken"
    otherwise:
        set checks.status to "uninitialized — call POST /kernel/init"
    respond with checks

// ══════════════════════════════════════════════════════════════════
//  WATCHDOG
// ══════════════════════════════════════════════════════════════════

to watchdog:
    purpose: "Check all monitored URLs"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set sanitized_urls to replace(replace(replace(replace(replace(str(urls_str), ";", ""), "|", ""), "&", ""), "`", ""), "$", "")
        set scan_result to shell("for url in $(echo '" + sanitized_urls + "' | tr ',' ' '); do code=$(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 5 $url 2>/dev/null); if [ \"$code\" != \"200\" ]; then echo \"UNHEALTHY:$url:$code\"; fi; done")
        if len(scan_result) is above 0:
            respond with {"status": "unhealthy_detected", "details": scan_result, "action": "POST /self-heal"}
        otherwise:
            respond with {"status": "all_healthy", "checked": urls_str}
    otherwise:
        respond with {"error": "MONITOR_URLS not configured"}

// ══════════════════════════════════════════════════════════════════
//  CAUSAL REASONING (with validate)
// ══════════════════════════════════════════════════════════════════

to causal_analyze with events, system_context:
    purpose: "Build causal graph from events"
    shell("mkdir -p memory/causal")
    set pheromone_data to "none"
    if file_exists("memory/pheromone/graph_db.json"):
        set pheromone_data to read_file("memory/pheromone/graph_db.json")
    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")
    ask AI to "You are a Causal Reasoning Engine using Bayesian inference. Construct a causal graph from events. EVENTS: {{events}}. CONTEXT: {{system_context}}. PHEROMONE: {{pheromone_data}}. PATTERNS: {{semantic_mem}}. Apply P(cause|evidence) scoring. Return ONLY valid JSON: {\"causal_graph\": [{\"cause\": \"string\", \"effect\": \"string\", \"probability\": 0.0, \"evidence\": \"string\"}], \"ranked_causes\": [{\"cause\": \"string\", \"posterior_probability\": 0.0}], \"reasoning_trace\": \"string\"}" save as causal_result
    set cv to validate(causal_result, ["ranked_causes"])
    if cv.valid is not equal yes:
        set causal_result to {"causal_graph": [], "ranked_causes": [], "reasoning_trace": "Causal analysis failed — AI returned unexpected format"}
    write_file("memory/causal/latest_analysis.json", json_encode(causal_result))
    respond with causal_result

// ══════════════════════════════════════════════════════════════════
//  OBSERVABILITY (with validate)
// ══════════════════════════════════════════════════════════════════

to observe with service_name, time_window:
    purpose: "Detect anomalies across all telemetry"
    set safe_svc to safe_id(service_name)
    set prom_url to env("PROMETHEUS_URL")
    set es_url to env("ELASTICSEARCH_URL")
    set metrics_data to "not configured"
    set log_data to "not configured"
    if prom_url:
        set eq to prom_url + "/api/v1/query?query=rate(http_requests_total{service=\"" + safe_svc + "\"}[5m])"
        gather metrics_data from eq
    if es_url:
        set lurl to es_url + "/logs-*/_search?q=service:" + safe_svc + "+AND+level:ERROR&size=50&sort=@timestamp:desc"
        gather log_data from lurl
    ask AI to "You are the Observability Intelligence Agent. Analyze telemetry for anomalies. SERVICE: {{service_name}}. TIME: {{time_window}}. METRICS: {{metrics_data}}. LOGS: {{log_data}}. Return ONLY valid JSON: {\"anomalies\": [{\"type\": \"string\", \"severity\": \"critical|high|medium|low\", \"description\": \"string\"}], \"trends\": [{\"metric\": \"string\", \"direction\": \"increasing|decreasing|stable\"}], \"health_score\": 0.0}" save as obs_result
    set ov to validate(obs_result, ["anomalies"])
    if ov.valid is not equal yes:
        set obs_result to {"anomalies": [], "trends": [], "health_score": 0.0}
    respond with obs_result

// ══════════════════════════════════════════════════════════════════
//  SIMULATION (with validate)
// ══════════════════════════════════════════════════════════════════

to sim_run with scenario_type, complexity, token:
    purpose: "Run a swarm simulation"
    require_role(token, "operator")
    shell("mkdir -p simulation/results")
    ask AI to "You are the HiveANT Simulation Engine. Generate and simulate a synthetic incident. SCENARIO: {{scenario_type}}. COMPLEXITY: {{complexity}} (1-5). Return ONLY valid JSON: {\"scenario\": {\"type\": \"string\", \"description\": \"string\", \"actual_root_cause\": \"string\", \"actual_fix\": \"string\"}, \"simulation\": {\"agents_deployed\": 0, \"root_cause_found\": true, \"correct_root_cause\": true, \"time_to_resolution_seconds\": 0}, \"metrics\": {\"accuracy\": 0.0, \"efficiency\": 0.0}}" save as sim_result
    set sv to validate(sim_result, ["scenario"])
    if sv.valid is not equal yes:
        set sim_result to {"scenario": {"type": scenario_type, "description": "Simulation failed"}, "simulation": {"agents_deployed": 0, "root_cause_found": false}, "metrics": {"accuracy": 0.0, "efficiency": 0.0}}
    set sim_id to "SIM-" + str(floor(random() * 90000 + 10000))
    set sim_result.simulation_id to sim_id
    write_file("simulation/results/" + sim_id + ".json", json_encode(sim_result))
    respond with sim_result

to sim_list:
    purpose: "List past simulations"
    set result to shell("ls simulation/results/SIM-*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"simulations": [], "count": 0}
    set ids to split(result, "\n")
    respond with {"simulations": ids, "count": len(ids)}

// ══════════════════════════════════════════════════════════════════
//  SELF-IMPROVEMENT / STABILITY / MESSAGE BUS / KG / VECTOR SEARCH
// ══════════════════════════════════════════════════════════════════

to swarm_performance:
    purpose: "Get swarm performance metrics"
    if file_exists("knowledge/swarm_performance.json"):
        respond with json_decode(read_file("knowledge/swarm_performance.json"))
    otherwise:
        respond with {"total_cycles": 0, "success_count": 0, "message": "Run POST /kernel/init first"}

to stability_status:
    purpose: "Get swarm stability status"
    if file_exists("agents_state/stability_config.json"):
        set cfg to json_decode(read_file("agents_state/stability_config.json"))
        set total to shell("ls agents_state/agent-*.json 2>/dev/null | wc -l || echo 0")
        respond with {"config": cfg, "current_agents": total, "healthy": true}
    otherwise:
        respond with {"status": "not_initialized", "message": "Run POST /kernel/init first"}

to bus_publish with topic, message, sender_id:
    purpose: "Publish to message bus"
    shell("mkdir -p agents_state/bus")
    set msg to {"topic": topic, "message": message, "sender": sender_id, "timestamp": time_now()}
    write_file("agents_state/bus/topic-" + str(topic) + "-" + str(floor(random() * 90000)) + ".json", json_encode(msg))
    respond with {"published": true, "topic": topic}

to bus_topics:
    purpose: "List bus topics"
    set result to shell("ls agents_state/bus/topic-*.json 2>/dev/null | while read f; do basename \"$f\" .json; done | sort -u || echo NONE")
    if result is equal "NONE":
        respond with {"topics": [], "count": 0}
    set topics to split(result, "\n")
    respond with {"topics": topics, "count": len(topics)}

to kg_add_entity with entity_id, entity_type, properties:
    purpose: "Add entity to knowledge graph"
    shell("mkdir -p memory/knowledge_graph/entities")
    set safe_id to replace(replace(str(entity_id), ";", ""), "|", "")
    set entity to {"id": safe_id, "type": entity_type, "properties": properties, "created_at": time_now()}
    write_file("memory/knowledge_graph/entities/" + safe_id + ".json", json_encode(entity))
    respond with {"entity_id": safe_id, "status": "added"}

to kg_status:
    purpose: "Knowledge graph stats"
    set entities to shell("ls memory/knowledge_graph/entities/*.json 2>/dev/null | wc -l || echo 0")
    set relations to shell("ls memory/knowledge_graph/relations/*.json 2>/dev/null | wc -l || echo 0")
    respond with {"entities": entities, "relations": relations}

to vector_search with query, source_type:
    purpose: "Semantic search"
    if source_type is equal "incidents":
        set content to shell("ls incidents/*.json 2>/dev/null | tail -30 | while read f; do cat \"$f\" 2>/dev/null | head -c 500; echo '---'; done || echo ''")
    otherwise:
        set content to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")
    if len(content) is above 10:
        set doc_chunks to chunk(content, 600, 80)
        set results to top_k(doc_chunks, 5)
        respond with {"query": query, "results": results, "chunks_searched": len(doc_chunks)}
    otherwise:
        respond with {"query": query, "results": [], "message": "No content to search"}

// ══════════════════════════════════════════════════════════════════
//  SANDBOX / AUDIT / DEV AGENTS / PLUGINS (with validate)
// ══════════════════════════════════════════════════════════════════

to sandbox_test with commands, service_name:
    purpose: "Dry-run test commands"
    ask AI to "You are a Sandbox Agent. Simulate execution WITHOUT running. COMMANDS: {{commands}}. SERVICE: {{service_name}}. Return ONLY valid JSON: {\"sandbox_results\": [{\"command\": \"string\", \"predicted_outcome\": \"success|failure\", \"side_effects\": [\"string\"], \"safe\": true}], \"overall_safe\": true, \"recommendation\": \"proceed|caution|abort\"}" save as sandbox_result
    set sbv to validate(sandbox_result, ["overall_safe"])
    if sbv.valid is not equal yes:
        set sandbox_result to {"sandbox_results": [], "overall_safe": false, "recommendation": "abort"}
    respond with sandbox_result

to audit_trail with count:
    purpose: "Get recent audit entries with full data"
    if count:
        set lim to str(count)
    otherwise:
        set lim to "50"
    set entries to shell("python3 -c \"import json,subprocess;files=subprocess.run(['sh','-c','ls -t agents_state/audit/AUD-*.json 2>/dev/null | head -" + lim + "'],capture_output=True,text=True).stdout.strip().split('\\n');entries=[json.load(open(f)) for f in files if f.strip()];print(json.dumps(entries))\" 2>/dev/null || echo '[]'")
    respond with {"entries": entries, "count": len(entries)}

to dev_review with code, language:
    purpose: "Automated code review"
    ask AI to "You are a Senior Code Reviewer. Review for security, performance, quality. CODE: {{code}}. LANGUAGE: {{language}}. Return ONLY valid JSON: {\"review_score\": 0.0, \"approved\": true, \"issues\": [{\"severity\": \"critical|high|medium|low\", \"category\": \"security|performance|quality\", \"description\": \"string\", \"suggestion\": \"string\"}], \"summary\": \"string\"}" save as review
    set rv to validate(review, ["review_score"])
    if rv.valid is not equal yes:
        set review to {"review_score": 0.0, "approved": false, "issues": [], "summary": "Review failed — AI returned unexpected format"}
    respond with review

to dev_generate with specification, language:
    purpose: "Generate code"
    ask AI to "Generate production-quality code. SPEC: {{specification}}. LANGUAGE: {{language}}. Return ONLY valid JSON: {\"files\": [{\"path\": \"string\", \"content\": \"string\"}], \"dependencies\": [\"string\"], \"notes\": \"string\"}" save as code_result
    set gv to validate(code_result, ["files"])
    if gv.valid is not equal yes:
        set code_result to {"files": [], "dependencies": [], "notes": "Code generation failed"}
    respond with code_result

to dev_deploy_plan with service_name, version, strategy:
    purpose: "Create deployment plan"
    ask AI to "Create a deployment plan. SERVICE: {{service_name}}. VERSION: {{version}}. STRATEGY: {{strategy}} (canary|blue-green|rolling). Return ONLY valid JSON: {\"deployment_plan\": {\"strategy\": \"string\", \"pre_deploy\": [{\"action\": \"string\", \"command\": \"string\"}], \"deploy_steps\": [{\"action\": \"string\", \"command\": \"string\"}], \"rollback_plan\": [{\"command\": \"string\"}], \"estimated_minutes\": 0, \"risk_level\": \"low|medium|high\"}}" save as plan
    set pv to validate(plan, ["deployment_plan"])
    if pv.valid is not equal yes:
        set plan to {"deployment_plan": {"strategy": "manual", "pre_deploy": [], "deploy_steps": [], "rollback_plan": [], "estimated_minutes": 0, "risk_level": "unknown"}}
    respond with plan

to plugin_register with plugin_name, plugin_type, version, token:
    purpose: "Register a plugin"
    require_role(token, "admin")
    shell("mkdir -p agents_state/plugins")
    set safe_name to replace(replace(str(plugin_name), ";", ""), "|", "")
    set plugin to {"name": safe_name, "type": plugin_type, "version": version, "registered_at": time_now(), "status": "active"}
    write_file("agents_state/plugins/" + safe_name + ".json", json_encode(plugin))
    respond with plugin

to plugin_list:
    purpose: "List plugins"
    set result to shell("ls agents_state/plugins/*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"plugins": [], "count": 0}
    set ids to split(result, "\n")
    respond with {"plugins": ids, "count": len(ids)}

api:
    GET / runs home
    GET /health runs health
    GET /health/deep runs health_deep
    GET /config runs show_config
    GET /sample runs sample_data
    GET /auth/status runs auth_status
    POST /auth/login runs auth_login
    POST /auth/register runs auth_register
    POST /auth/refresh runs auth_refresh
    POST /auth/change-password runs auth_change_password
    GET /auth/users runs auth_users
    POST /auth/users/update runs auth_update_user
    POST /auth/users/delete runs auth_delete_user
    GET /auth/orgs runs auth_orgs
    POST /kernel/init runs kernel_init
    GET /kernel/status runs kernel_status
    POST /agents/spawn runs spawn_agent
    POST /agents/kill runs kill_agent
    GET /agents runs list_agents
    POST /tasks/create runs create_investigation_graph
    GET /tasks runs list_task_graphs
    POST /swarm/ant/explore runs ant_explore
    POST /swarm/bee/optimize runs bee_optimize
    POST /investigate runs swarm_investigate
    POST /self-heal runs self_heal
    POST /feedback runs add_feedback
    GET /knowledge runs get_knowledge
    POST /predict runs predict
    POST /twin/register runs twin_register
    POST /twin/dependency runs twin_add_dep
    GET /twin/topology runs twin_topology
    POST /twin/impact runs twin_impact
    POST /cluster/init runs cluster_init
    GET /cluster/status runs cluster_status
    GET /pheromone/status runs pheromone_status
    POST /pheromone/add runs pheromone_add
    POST /pheromone/evaporate runs pheromone_evaporate
    POST /pheromone/query runs pheromone_query
    GET /incidents runs list_incidents
    POST /incident runs get_incident
    GET /incidents/stats runs incident_stats
    POST /cleanup runs cleanup_old
    POST /chat runs chat
    POST /watchdog runs watchdog
    POST /causal/analyze runs causal_analyze
    POST /observe runs observe
    POST /simulation/run runs sim_run
    GET /simulation/list runs sim_list
    GET /swarm/performance runs swarm_performance
    GET /stability/status runs stability_status
    POST /bus/publish runs bus_publish
    GET /bus/topics runs bus_topics
    POST /kg/entity runs kg_add_entity
    GET /kg/status runs kg_status
    POST /search runs vector_search
    POST /sandbox/test runs sandbox_test
    GET /audit runs audit_trail
    POST /dev/review runs dev_review
    POST /dev/generate runs dev_generate
    POST /dev/deploy-plan runs dev_deploy_plan
    POST /plugins/register runs plugin_register
    GET /plugins runs plugin_list

every 1 hour:
    log "HIVEANT: Running scheduled pheromone evaporation"
    pheromone_evaporate(0.05)

every 24 hours:
    log "HIVEANT: Running scheduled cleanup (90 days)"
    shell("find incidents/ -name '*.json' -mtime +90 -delete 2>/dev/null; find agents_state/ -name 'agent-*.json' -mtime +90 -delete 2>/dev/null; find agents_state/audit/ -name 'AUD-*.json' -mtime +90 -delete 2>/dev/null")
    log "HIVEANT: Scheduled cleanup complete"
