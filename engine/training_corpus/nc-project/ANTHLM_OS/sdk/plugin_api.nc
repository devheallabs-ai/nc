// ══════════════════════════════════════════════════════════════════
//  HiveANT — Plugin API (SDK)
//
//  Defines the interface for extending HiveANT with plugins.
//  Plugins can add new agent types, data adapters, or integrations.
// ══════════════════════════════════════════════════════════════════

to plugin_register with plugin_name, plugin_type, version, author:
    purpose: "Register a plugin with the HiveANT ecosystem"
    shell("mkdir -p agents_state/plugins")
    set plugin to {"name": plugin_name, "type": plugin_type, "version": version, "author": author, "registered_at": time_now(), "status": "active"}
    write_file("agents_state/plugins/" + plugin_name + ".json", json_encode(plugin))
    log "PLUGIN: Registered " + plugin_name + " v" + version
    respond with plugin

to plugin_list:
    purpose: "List all registered plugins"
    set result to shell("ls agents_state/plugins/*.json 2>/dev/null | while read f; do cat \"$f\" 2>/dev/null; echo ','; done || echo NONE")
    if result is equal "NONE":
        respond with {"plugins": [], "count": 0}
    respond with {"plugins": result, "count": 0}

to plugin_execute with plugin_name, action, params:
    purpose: "Execute a plugin action"
    set path to "agents_state/plugins/" + plugin_name + ".json"
    if file_exists(path):
        set plugin to json_decode(read_file(path))
        log "PLUGIN: Executing " + plugin_name + "." + action
        respond with {"plugin": plugin_name, "action": action, "status": "executed", "params": params}
    otherwise:
        respond with {"error": "Plugin not found", "_status": 404}
