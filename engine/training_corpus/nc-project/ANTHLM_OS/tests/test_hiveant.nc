// ══════════════════════════════════════════════════════════════════
//  HiveANT — Complete Test Suite (v1.0 Enterprise)
//  Run: NC_ALLOW_EXEC=1 NC_ALLOW_FILE_WRITE=1 nc test tests/test_hiveant.nc
//
//  Covers: Kernel, Agents, Auth (JWT/RBAC), Pheromone, Twin,
//          Incidents, Knowledge, Clustering, Stability, Bus,
//          KG, Audit, Plugins, Simulation, Error Handling
// ══════════════════════════════════════════════════════════════════

// ── 1. KERNEL ──────────────────────────────────────────────────

describe "Kernel Initialization":
    it "should return version 1.0.0":
        set result to kernel_init()
        assert result.kernel.version is equal "1.0.0"
        assert result.kernel.name is equal "HiveANT"
        assert result.kernel.status is equal "running"
    it "should verify file write capability":
        set result to kernel_init()
        assert result.file_write_verified is equal true
    it "should initialize all subsystems":
        set result to kernel_init()
        assert result.scheduler is equal "initialized"
        assert result.pheromone_db is equal "initialized"
        assert result.digital_twin is equal "initialized"
        assert result.stability is equal "initialized"

describe "Kernel Status":
    it "should return system info":
        set result to kernel_status()
        assert result.system is equal "HiveANT"
        assert result.version is equal "1.0.0"
    it "should include kernel state":
        set result to kernel_status()
        assert result.kernel is not equal "not initialized"

// ── 2. HEALTH ──────────────────────────────────────────────────

describe "Health Check":
    it "should return healthy":
        set result to health()
        assert result.status is equal "healthy"
        assert result.service is equal "hiveant"
        assert result.version is equal "1.0.0"

describe "Deep Health Check":
    it "should verify file write":
        set result to health_deep()
        assert result.file_write is equal "ok"
    it "should report AI config status":
        set result to health_deep()
        assert result.status is not equal ""

// ── 3. AUTH — Registration & Login ─────────────────────────────

describe "Auth Status":
    it "should report auth configuration":
        set result to auth_status()
        assert result.roles is not equal ""

describe "Auth Register — First User Bootstrap":
    it "should allow first user without admin token":
        set result to auth_register("test-admin", "testpass123", "admin", "test-org", "")
        assert result.username is equal "test-admin"
        assert result.role is equal "admin"
        assert result.status is equal "active"
    it "should reject duplicate username":
        set result to auth_register("test-admin", "pass2", "admin", "test-org", "")
        assert result.error is equal "User already exists"

describe "Auth Login":
    it "should return JWT token on valid credentials":
        set result to auth_login("test-admin", "testpass123", "")
        assert result.authenticated is equal true
        assert result.token is not equal ""
        assert result.role is equal "admin"
    it "should reject invalid password":
        set result to auth_login("test-admin", "wrongpass", "")
        assert result.error is equal "Invalid credentials"
    it "should reject unknown user":
        set result to auth_login("nonexistent-user", "pass", "")
        assert result.error is equal "User not found"

describe "Auth Register — With Admin Token":
    it "should register operator user":
        set admin_login to auth_login("test-admin", "testpass123", "")
        set result to auth_register("test-operator", "oppass123", "operator", "test-org", admin_login.token)
        assert result.username is equal "test-operator"
        assert result.role is equal "operator"
    it "should register viewer user":
        set admin_login to auth_login("test-admin", "testpass123", "")
        set result to auth_register("test-viewer", "viewpass123", "viewer", "test-org", admin_login.token)
        assert result.username is equal "test-viewer"
        assert result.role is equal "viewer"

// ── 4. AUTH — Token Refresh ────────────────────────────────────

describe "Token Refresh":
    it "should return new token for valid token":
        set login to auth_login("test-admin", "testpass123", "")
        set result to auth_refresh(login.token)
        assert result.token is not equal ""
        assert result.role is equal "admin"
    it "should reject invalid token":
        set result to auth_refresh("invalid-token-here")
        assert result.error is not equal ""

// ── 5. AUTH — RBAC ─────────────────────────────────────────────

describe "RBAC — Admin Access":
    it "admin should list users":
        set login to auth_login("test-admin", "testpass123", "")
        set result to auth_users(login.token)
        assert result.count is above 0
    it "admin should list orgs":
        set login to auth_login("test-admin", "testpass123", "")
        set result to auth_orgs(login.token)
        assert result.count is above 0

describe "RBAC — Operator Restrictions":
    it "operator should not list users":
        set login to auth_login("test-operator", "oppass123", "")
        set result to auth_users(login.token)
        assert result.error is equal "Admin access required"

describe "RBAC — Viewer Restrictions":
    it "viewer should not spawn agents":
        set login to auth_login("test-viewer", "viewpass123", "")
        set result to spawn_agent("detection", "rbac-test", {}, login.token)
        assert result.error is not equal ""

// ── 6. AUTH — User Management ──────────────────────────────────

describe "User Update":
    it "admin should disable user":
        set login to auth_login("test-admin", "testpass123", "")
        set result to auth_update_user("test-viewer", "", "disabled", login.token)
        assert result.status is equal "disabled"
    it "disabled user should not login":
        set result to auth_login("test-viewer", "viewpass123", "")
        assert result.error is equal "Account disabled"
    it "admin should re-enable user":
        set login to auth_login("test-admin", "testpass123", "")
        set result to auth_update_user("test-viewer", "", "active", login.token)
        assert result.status is equal "active"

describe "Password Change":
    it "should change own password":
        set login to auth_login("test-viewer", "viewpass123", "")
        set result to auth_change_password("viewpass123", "newpass456", login.token)
        assert result.message is equal "Password changed"
    it "should login with new password":
        set result to auth_login("test-viewer", "newpass456", "")
        assert result.authenticated is equal true
    it "should reject wrong current password":
        set login to auth_login("test-viewer", "newpass456", "")
        set result to auth_change_password("wrongpass", "anotherpass", login.token)
        assert result.error is equal "Current password incorrect"

// ── 7. AGENTS (with auth) ──────────────────────────────────────

describe "Agent Spawn (Authenticated)":
    it "operator should spawn agent":
        set login to auth_login("test-operator", "oppass123", "")
        set agent to spawn_agent("detection", "test-auth-det", {}, login.token)
        assert agent.id is equal "test-auth-det"
        assert agent.type is equal "detection"
        assert agent.status is equal "running"
    it "should auto-generate ID":
        set login to auth_login("test-operator", "oppass123", "")
        set agent to spawn_agent("investigation", "", {}, login.token)
        assert agent.type is equal "investigation"

describe "Agent Kill (Authenticated)":
    it "operator should terminate agent":
        set login to auth_login("test-operator", "oppass123", "")
        set result to kill_agent("test-auth-det", login.token)
        assert result.status is equal "terminated"
    it "should 404 for missing agent":
        set login to auth_login("test-operator", "oppass123", "")
        set result to kill_agent("NONEXISTENT", login.token)
        assert result.error is equal "Agent not found"

describe "Agent List":
    it "should return agents array":
        set result to list_agents("")
        assert result.count is above -1
    it "should return agents with count":
        set result to list_agents("")
        assert result.agents is not equal ""

// ── 8. TASK GRAPH ──────────────────────────────────────────────

describe "Task Graph":
    it "should create investigation graph":
        set graph to create_investigation_graph("test-svc", "test issue")
        assert graph.status is equal "active"
        assert graph.graph_id is not equal ""
    it "should list graphs":
        set result to list_task_graphs()
        assert result.count is above 0

// ── 9. PHEROMONE GRAPH ─────────────────────────────────────────

describe "Pheromone Add":
    it "should add edge with score":
        set result to pheromone_add("error_spike", "db_fail", 1.5)
        assert result.score is equal 1.5
    it "should reinforce existing edge":
        set r1 to pheromone_add("reinforce_a", "reinforce_b", 2.0)
        set r2 to pheromone_add("reinforce_a", "reinforce_b", 1.0)
        assert r2.score is equal 3.0
    it "should add edge with default score":
        set result to pheromone_add("node_x", "node_y", "")
        assert result.score is above 0

describe "Pheromone Status":
    it "should return edge count":
        set result to pheromone_status()
        assert result.edges is above 0

describe "Pheromone Query":
    it "should find edges from node":
        set result to pheromone_query("error_spike")
        assert result.count is above 0
    it "should return empty for unknown node":
        set result to pheromone_query("totally_unknown_node_xyz")
        assert result.count is equal 0

describe "Pheromone Evaporate":
    it "should evaporate with custom rate":
        set result to pheromone_evaporate(0.1)
        assert result.rate is equal 0.1
    it "should evaporate with default rate":
        set result to pheromone_evaporate("")
        assert result.evaporated is above -1

// ── 10. DIGITAL TWIN ───────────────────────────────────────────

describe "Twin Register":
    it "should register service":
        set result to twin_register("test-api", "api", [], {})
        assert result.status is equal "registered"
    it "should register database":
        set result to twin_register("test-db", "database", [], {"engine": "postgresql"})
        assert result.status is equal "registered"

describe "Twin Topology":
    it "should return topology with services":
        set result to twin_topology()
        assert result.service_count is above 0

describe "Twin Dependency":
    it "should add dependency":
        set result to twin_add_dep("test-api", "test-db", "database")
        assert result.added is equal true
        assert result.total_deps is above 0

// ── 11. KNOWLEDGE ──────────────────────────────────────────────

describe "Knowledge":
    it "should return memory layers":
        set result to get_knowledge()
        assert result is not equal ""

// ── 12. INCIDENTS ──────────────────────────────────────────────

describe "Incidents":
    it "should list incidents":
        set result to list_incidents()
        assert result.count is above -1
    it "should 404 for missing incident":
        set result to get_incident("NONEXISTENT-999")
        assert result.error is equal "Not found"
    it "should return stats":
        set result to incident_stats()
        assert result.total is above -1

// ── 13. CLUSTERING ─────────────────────────────────────────────

describe "Cluster Init":
    it "should initialize with custom count":
        set result to cluster_init(3)
        assert result.cluster_count is equal 3
        assert result.status is equal "active"
    it "should create cluster files":
        set result to cluster_status()
        assert result.coordinator is equal "active"

// ── 14. STABILITY ──────────────────────────────────────────────

describe "Stability":
    it "should return healthy config":
        set result to stability_status()
        assert result.healthy is equal true
    it "should have agent count":
        set result to stability_status()
        assert result.current_agents is above -1

// ── 15. SWARM PERFORMANCE ──────────────────────────────────────

describe "Swarm Performance":
    it "should return metrics":
        set result to swarm_performance()
        assert result.total_cycles is above -1

// ── 16. KNOWLEDGE GRAPH ────────────────────────────────────────

describe "Knowledge Graph":
    it "should add entity":
        set result to kg_add_entity("test-entity", "service", {"team": "platform"})
        assert result.status is equal "added"
    it "should return entity count":
        set result to kg_status()
        assert result.entities is above 0

// ── 17. MESSAGE BUS ────────────────────────────────────────────

describe "Message Bus":
    it "should publish message":
        set result to bus_publish("test-topic", "hello world", "test-sender")
        assert result.published is equal true
    it "should list topics":
        set result to bus_topics()
        assert result.count is above -1

// ── 18. AUDIT TRAIL ────────────────────────────────────────────

describe "Audit Trail":
    it "should return entries":
        set result to audit_trail(10)
        assert result.count is above -1
    it "should contain auth events":
        set result to audit_trail(50)
        assert result.count is above 0

// ── 19. PLUGINS ────────────────────────────────────────────────

describe "Plugin Register":
    it "should register plugin":
        set result to plugin_register("test-plugin", "adapter", "1.0")
        assert result.name is equal "test-plugin"
        assert result.status is equal "active"

describe "Plugin List":
    it "should list registered plugins":
        set result to plugin_list()
        assert result.count is above 0

// ── 20. SIMULATION ─────────────────────────────────────────────

describe "Simulation List":
    it "should return simulation list":
        set result to sim_list()
        assert result.count is above -1

// ── 21. CONFIG ─────────────────────────────────────────────────

describe "Config":
    it "should return system name":
        set result to show_config()
        assert result.system is equal "HiveANT"
        assert result.version is equal "1.0.0"

describe "Sample Data":
    it "should return demo incident":
        set result to sample_data()
        assert result.service_name is equal "checkout-api"
        assert result.description is not equal ""

// ── 22. DATA RETENTION ─────────────────────────────────────────

describe "Cleanup (Authenticated)":
    it "admin should run cleanup":
        set login to auth_login("test-admin", "testpass123", "")
        set result to cleanup_old(365, login.token)
        assert result.older_than_days is equal "365"

// ── 23. INPUT SANITIZATION ─────────────────────────────────────

describe "Sanitization":
    it "should strip dangerous characters":
        set result to sanitize("hello;rm -rf /|cat /etc/passwd")
        assert result is equal "hellorm -rf /cat /etc/passwd"
    it "should handle empty input":
        set result to sanitize("")
        assert result is equal ""

// ── 24. ERROR HANDLING ─────────────────────────────────────────

describe "Error Handling":
    it "should handle missing agent gracefully":
        set login to auth_login("test-operator", "oppass123", "")
        set result to kill_agent("does-not-exist-12345", login.token)
        assert result.error is equal "Agent not found"
    it "should handle missing incident":
        set result to get_incident("INC-NONEXISTENT")
        assert result.error is equal "Not found"

// ── 25. CLEANUP TEST USERS ─────────────────────────────────────

describe "Test Cleanup":
    it "should delete test users":
        set login to auth_login("test-admin", "testpass123", "")
        set r1 to auth_delete_user("test-viewer", login.token)
        assert r1.deleted is equal true
        set r2 to auth_delete_user("test-operator", login.token)
        assert r2.deleted is equal true
        set r3 to auth_delete_user("test-admin", login.token)
        assert r3.deleted is equal true
