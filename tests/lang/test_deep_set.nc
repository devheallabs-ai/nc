// Test: 2-level deep property set (set x.a.b to val)
// Verifies nested map property assignment
// Bug fix: Previously only single-level set worked

service "test-deep-set"
version "1.0.0"

to test deep set number:
    set state to {"events": {"cpu": 85, "memory": 72}}
    set state.events.cpu to 60
    set result to state["events"]
    respond with result["cpu"]

to test deep set string:
    set config to {"db": {"host": "old.server.com", "port": 5432}}
    set config.db.host to "new.server.com"
    set db to config["db"]
    respond with db["host"]

to test deep set preserves sibling:
    set state to {"events": {"cpu": 85, "memory": 72}}
    set state.events.cpu to 60
    set result to state["events"]
    respond with result["memory"]

to test single level still works:
    set data to {"name": "alice", "age": 25}
    set data.name to "bob"
    respond with data.name

to test deep set theme:
    set app to {"config": {"theme": "light"}}
    set app.config.theme to "dark"
    set cfg to app["config"]
    respond with cfg["theme"]

to test deep set to zero:
    set metrics to {"server": {"errors": 42}}
    set metrics.server.errors to 0
    set srv to metrics["server"]
    respond with srv["errors"]
