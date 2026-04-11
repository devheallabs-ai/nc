// Test: notify keyword — notification dispatch
// Verifies notify fires without crashing

service "test-notify"
version "1.0.0"

to test notify channel:
    notify "ops-team" "server is healthy"
    respond with "notified"

to test notify with variable:
    set message to "deployment complete"
    notify "deploy-channel" message
    respond with "ok"
