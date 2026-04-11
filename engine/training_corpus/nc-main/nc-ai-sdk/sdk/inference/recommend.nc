// ═══════════════════════════════════════════════════════════
//  NC AI Code Recommendations — Written 100% in NC
//
//  Analyze NC code and provide recommendations for
//  improvements, best practices, security, and performance.
//
//  Built by DevHeal Labs AI
//
//  Usage:
//    nc nc-ai/nc/inference/recommend.nc --file service.nc
// ═══════════════════════════════════════════════════════════

service "nc-recommend"
version "1.0.0"

// ── Best Practices Database ──────────────────────────────

to get_best_practices:
    purpose: "NC coding best practices from nc-lang and nc-ui"
    respond with {
        service: [
            {rule: "Always declare service name and version", check: "service_declaration"},
            {rule: "Add health_check endpoint for monitoring", check: "health_endpoint"},
            {rule: "Use middleware for cors and logging", check: "middleware"},
            {rule: "Validate input data before storing", check: "input_validation"},
            {rule: "Use meaningful function names (verb_noun pattern)", check: "naming"},
            {rule: "Add error handling with 'if X is empty' checks", check: "error_handling"},
            {rule: "Use configure: block for settings", check: "config_block"},
            {rule: "Keep functions focused — one task per function", check: "function_size"},
            {rule: "Use generate_id() for unique IDs", check: "id_generation"},
            {rule: "Log important operations", check: "logging"}
        ],
        ncui: [
            {rule: "Define a style: block with brand colors", check: "style_block"},
            {rule: "Use responsive grid layouts", check: "grid_usage"},
            {rule: "Add navigation for multi-section pages", check: "navigation"},
            {rule: "Include data: binding for dynamic content", check: "data_binding"},
            {rule: "Add actions: for interactivity", check: "actions"},
            {rule: "Use semantic section names", check: "section_names"},
            {rule: "Add loading states for data fetches", check: "loading_states"},
            {rule: "Include accessibility hints", check: "accessibility"},
            {rule: "Use animate for smooth transitions", check: "animations"},
            {rule: "Add footer with attribution", check: "footer"}
        ],
        security: [
            {rule: "Never store passwords in plain text", check: "password_storage"},
            {rule: "Use jwt for authentication tokens", check: "auth_tokens"},
            {rule: "Validate file uploads (size, type)", check: "upload_validation"},
            {rule: "Rate limit sensitive endpoints", check: "rate_limiting"},
            {rule: "Sanitize user input before database operations", check: "input_sanitization"}
        ],
        performance: [
            {rule: "Use pagination for list endpoints", check: "pagination"},
            {rule: "Cache frequently accessed data", check: "caching"},
            {rule: "Limit response payload size", check: "payload_size"},
            {rule: "Use batch operations for bulk updates", check: "batch_ops"}
        ]
    }

// ── Analyze NC Service Code ──────────────────────────────

to analyze_service with code:
    purpose: "Analyze NC service and return recommendations"
    set recommendations to []

    // Service structure
    if code does not contain "service \"":
        append {severity: "error", msg: "Add service declaration: service \"my-api\"", category: "structure"} to recommendations
    if code does not contain "version \"":
        append {severity: "warning", msg: "Add version: version \"1.0.0\"", category: "structure"} to recommendations

    // Health & monitoring
    if code does not contain "health":
        append {severity: "info", msg: "Add health_check endpoint for monitoring: to health_check: respond with {status: \"healthy\"}", category: "reliability"} to recommendations

    // Middleware
    if code does not contain "middleware":
        append {severity: "info", msg: "Add middleware: block with cors and log_requests", category: "security"} to recommendations

    // Error handling
    set functions to count_occurrences(code, "to ")
    set error_checks to count_occurrences(code, "is empty")
    if functions > 0 and error_checks < functions / 2:
        append {severity: "warning", msg: "Add error handling — check if values are empty before using them", category: "reliability"} to recommendations

    // Input validation
    if code contains "store " and code does not contain "validate" and code does not contain "is empty":
        append {severity: "warning", msg: "Validate data before storing — check required fields", category: "security"} to recommendations

    // Logging
    if code does not contain "log ":
        append {severity: "info", msg: "Add logging for important operations: log \"Created item: \" + id", category: "observability"} to recommendations

    // Configure block
    if code does not contain "configure:":
        append {severity: "info", msg: "Add configure: block for settings (port, ai_model, etc.)", category: "maintainability"} to recommendations

    // Authentication
    if code contains "user" and code does not contain "auth" and code does not contain "token" and code does not contain "jwt":
        append {severity: "warning", msg: "User-facing service without auth — consider adding authentication", category: "security"} to recommendations

    // Pagination
    if code contains "gather" and code does not contain "limit" and code does not contain "page":
        append {severity: "info", msg: "Add pagination to list endpoints to handle large datasets", category: "performance"} to recommendations

    // Timestamps
    if code contains "store" and code does not contain "created_at" and code does not contain "now()":
        append {severity: "info", msg: "Add timestamps: set data.created_at to now()", category: "data"} to recommendations

    // ID generation
    if code contains "store" and code does not contain "generate_id":
        append {severity: "warning", msg: "Use generate_id() for unique IDs: set data.id to generate_id()", category: "data"} to recommendations

    respond with recommendations

// ── Analyze NCUI Page ────────────────────────────────────

to analyze_page with code:
    purpose: "Analyze NCUI page and return recommendations"
    set recommendations to []

    if code does not contain "style:":
        append {severity: "warning", msg: "Add style: block with colors, font, and accent", category: "design"} to recommendations

    if code does not contain "nav" and code does not contain "header":
        append {severity: "info", msg: "Add navigation header for better UX", category: "navigation"} to recommendations

    if code does not contain "grid" and code does not contain "row":
        append {severity: "info", msg: "Use grid/row layouts for responsive design", category: "layout"} to recommendations

    if code does not contain "data:":
        append {severity: "info", msg: "Add data: block to bind API data to your page", category: "interactivity"} to recommendations

    if code does not contain "actions:":
        append {severity: "info", msg: "Add actions: block for user interactions", category: "interactivity"} to recommendations

    if code does not contain "animate":
        append {severity: "info", msg: "Add animate \"fade-up\" for smooth transitions", category: "polish"} to recommendations

    if code does not contain "loading" and code contains "data:":
        append {severity: "info", msg: "Add loading states for data fetches", category: "ux"} to recommendations

    if code does not contain "footer":
        append {severity: "info", msg: "Add footer section", category: "structure"} to recommendations

    respond with recommendations

// ── Full Code Review ─────────────────────────────────────

to review with filepath:
    purpose: "Complete code review with recommendations"

    set content to read_file(filepath)
    if content is empty:
        respond with error "File not found: " + filepath

    set filename to basename(filepath)
    set is_ncui to filename ends with ".ncui"

    // Get recommendations
    if is_ncui:
        set recs to analyze_page(content)
    else:
        set recs to analyze_service(content)

    // Count by severity
    set error_count to 0
    set warning_count to 0
    set info_count to 0
    repeat for each r in recs:
        if r.severity is "error":
            set error_count to error_count + 1
        if r.severity is "warning":
            set warning_count to warning_count + 1
        if r.severity is "info":
            set info_count to info_count + 1

    // Format output
    log "NC AI — Code Review: " + filename
    log "─────────────────────────────"
    set lines to count_occurrences(content, "\n") + 1
    set functions to count_occurrences(content, "to ")
    log "Lines: " + lines + " | Functions: " + functions
    log ""

    if length(recs) is 0:
        log "  All checks passed — code looks great!"
    else:
        if error_count > 0:
            log "  ERRORS (" + error_count + "):"
            repeat for each r in recs:
                if r.severity is "error":
                    log "    [x] " + r.msg
            log ""

        if warning_count > 0:
            log "  WARNINGS (" + warning_count + "):"
            repeat for each r in recs:
                if r.severity is "warning":
                    log "    [!] " + r.msg
            log ""

        if info_count > 0:
            log "  SUGGESTIONS (" + info_count + "):"
            repeat for each r in recs:
                if r.severity is "info":
                    log "    [i] " + r.msg
            log ""

    set score to 100 - (error_count * 20) - (warning_count * 10) - (info_count * 3)
    if score < 0:
        set score to 0
    log "  Code Quality Score: " + score + "/100"

    respond with {
        file: filepath,
        score: score,
        errors: error_count,
        warnings: warning_count,
        suggestions: info_count,
        recommendations: recs
    }

// ── API Routes ───────────────────────────────────────────

api:
    POST /review      runs review
    POST /analyze     runs analyze_service
