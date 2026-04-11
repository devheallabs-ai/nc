// ══════════════════════════════════════════════════════════════════
//  MCP — Model Context Protocol integration
//  SwarmOps exposes tools that MCP clients (Claude, Cursor, etc.) can call
//  Also can call external MCP tool servers for extended capabilities
// ══════════════════════════════════════════════════════════════════

to mcp_tools:
    purpose: "List available MCP tools — for Claude Desktop, Cursor, etc."
    respond with {"tools": [{"name": "investigate", "description": "Investigate a production incident. Pulls metrics, logs, deploys and finds root cause.", "parameters": {"service_name": "string", "description": "string"}}, {"name": "k8s_investigate", "description": "Investigate Kubernetes cluster issues. Pulls pod status, logs, events.", "parameters": {"namespace": "string", "description": "string"}}, {"name": "chat", "description": "Ask SwarmOps about incidents, services, or infrastructure.", "parameters": {"message": "string"}}, {"name": "k8s_unhealthy", "description": "Find all unhealthy pods across the cluster.", "parameters": {}}, {"name": "k8s_logs", "description": "Get logs from a Kubernetes pod.", "parameters": {"pod_name": "string", "namespace": "string", "lines": "number"}}, {"name": "k8s_fix", "description": "Fix a K8s issue: restart, rollback, scale a deployment.", "parameters": {"action": "restart|rollback|scale|delete-pod", "target": "string", "namespace": "string", "secret": "string"}}, {"name": "self_heal", "description": "Auto-diagnose and fix a service issue.", "parameters": {"service_name": "string", "description": "string", "auto_execute": "yes|no", "secret": "string"}}, {"name": "knowledge", "description": "View learned patterns from past incidents.", "parameters": {}}, {"name": "rag_search", "description": "Search runbooks and docs.", "parameters": {"query": "string"}}]}

to mcp_call with tool_name, arguments:
    purpose: "MCP tool execution endpoint — called by MCP clients"
    log "MCP tool call: {{tool_name}}"

    if tool_name is equal "investigate":
        gather result from "http://localhost:9090/investigate":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_investigate":
        gather result from "http://localhost:9090/k8s/investigate":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "chat":
        gather result from "http://localhost:9090/chat":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_unhealthy":
        gather result from "http://localhost:9090/k8s/unhealthy"
        respond with result

    if tool_name is equal "k8s_logs":
        gather result from "http://localhost:9090/k8s/logs":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "k8s_fix":
        gather result from "http://localhost:9090/k8s/fix":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "self_heal":
        gather result from "http://localhost:9090/self-heal":
            method: "POST"
            body: arguments
        respond with result

    if tool_name is equal "knowledge":
        gather result from "http://localhost:9090/knowledge"
        respond with result

    if tool_name is equal "rag_search":
        gather result from "http://localhost:9090/rag/search":
            method: "POST"
            body: arguments
        respond with result

    respond with {"error": "Unknown tool: " + tool_name}

to mcp_external with tool_name, arguments:
    purpose: "Call an external MCP tool server"
    set mcp_url to env("NC_MCP_URL")
    if mcp_url:
        set mcp_path to env("NC_MCP_PATH")
        if mcp_path:
            set full_url to mcp_url + mcp_path
        otherwise:
            set full_url to mcp_url + "/api/v1/tools/call"
        gather result from full_url:
            method: "POST"
            body: {"tool_name": tool_name, "arguments": arguments}
        respond with result
    otherwise:
        respond with {"error": "NC_MCP_URL not configured"}
