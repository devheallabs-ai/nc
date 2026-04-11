// Test: emit keyword — event emission
// Verifies emit fires without crashing

service "test-emit"
version "1.0.0"

to test emit event:
    emit "order.completed"
    respond with "emitted"

to test emit with data:
    set order to {"id": 123, "total": 49.99}
    emit "order.created"
    respond with "ok"

to test emit multiple:
    emit "step.start"
    emit "step.process"
    emit "step.done"
    respond with "all emitted"
