// NC Standard Library — logging
// Structured logging for applications

service "nc.logging"
version "1.0.0"

to info with message:
    log message

to warn with message:
    log "WARN: " + message

to error with message:
    log "ERROR: " + message

to debug with message:
    log "DEBUG: " + message

to critical with message:
    log "CRITICAL: " + message
    notify ops_team message
