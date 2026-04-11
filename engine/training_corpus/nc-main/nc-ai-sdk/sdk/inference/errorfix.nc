// ═══════════════════════════════════════════════════════════
//  NC AI Error Detection & Auto-Fix — Written 100% in NC
//
//  Validates NC and NCUI code, detects common errors,
//  and automatically repairs them. Learns from nc-lang
//  and nc-ui internals to understand the full stack.
//
//  Built by DevHeal Labs AI
//
//  Usage:
//    nc nc-ai/nc/inference/errorfix.nc --check service.nc
//    nc nc-ai/nc/inference/errorfix.nc --fix service.nc
// ═══════════════════════════════════════════════════════════

service "nc-errorfix"
version "1.0.0"

// ── NC Language Rules (learned from nc-lang engine) ──────

to get_nc_rules:
    purpose: "Return NC language syntax rules for validation"
    set rules to {
        service: {
            required: ["service declaration", "version", "api block"],
            keywords: ["service \"", "version \"", "api:"],
            functions: ["to ", "respond with", "set ", "gather from", "store into"]
        },
        ncui: {
            required: ["page declaration", "at least one section", "footer"],
            keywords: ["page \"", "section", "footer:"],
            elements: ["heading", "text", "button", "card", "grid", "list", "form", "input"]
        },
        common: {
            must_have: ["proper indentation", "colon after function names", "string quotes"],
            forbidden: ["undefined variables", "missing closing quotes", "empty api block"]
        }
    }
    respond with rules

// ── Validate NC Service File ─────────────────────────────

to validate_service with code:
    purpose: "Check NC service file for errors"
    set errors to []
    set warnings to []

    // Check service declaration
    if code does not contain "service \"":
        append "Missing service declaration (e.g., service \"my-api\")" to errors

    // Check version
    if code does not contain "version \"":
        append "Missing version declaration (e.g., version \"1.0.0\")" to warnings

    // Check for at least one function
    if code does not contain "to ":
        append "No functions defined (use: to function_name with params:)" to errors

    // Check API routes
    if code does not contain "api:":
        append "Missing api: route block" to errors
    else:
        // Check route format
        if code does not contain "GET " and code does not contain "POST " and code does not contain "runs ":
            append "API block has no routes (e.g., GET /items runs list_items)" to errors

    // Check for respond with
    if code does not contain "respond with":
        append "No 'respond with' in any function — functions should return data" to warnings

    // Check for common mistakes
    if code contains "function ":
        append "Use 'to function_name:' instead of 'function' (NC syntax)" to errors
    if code contains "return ":
        append "Use 'respond with' instead of 'return' (NC syntax)" to errors
    if code contains "var " or code contains "let " or code contains "const ":
        append "Use 'set variable to value' instead of var/let/const (NC syntax)" to errors
    if code contains "console.log":
        append "Use 'log' instead of 'console.log' (NC syntax)" to errors
    if code contains "require(" or code contains "import ":
        append "NC has built-in modules — no imports needed" to warnings

    // Check health endpoint
    if code does not contain "health":
        append "Consider adding a health_check endpoint" to warnings

    respond with {errors: errors, warnings: warnings, valid: length(errors) is 0}

// ── Validate NCUI Page File ─────────────────────────────

to validate_page with code:
    purpose: "Check NCUI page file for errors"
    set errors to []
    set warnings to []

    // Check page declaration
    if code does not contain "page \"":
        append "Missing page declaration (e.g., page \"My App\")" to errors

    // Check style block
    if code does not contain "style:":
        append "Missing style: block" to warnings

    // Check for sections
    if code does not contain "section":
        append "No sections defined — add at least one section" to errors

    // Check footer
    if code does not contain "footer":
        append "Missing footer" to warnings

    // Check for interactive elements
    if code does not contain "button" and code does not contain "form":
        append "No interactive elements (buttons/forms) found" to warnings

    // Check for data binding
    if code does not contain "data:" and code does not contain "actions:":
        append "No data binding or actions — page will be static" to warnings

    // Common NCUI mistakes
    if code contains "<div" or code contains "<span":
        append "Use NC UI components (section, card, grid) instead of HTML tags" to errors
    if code contains "className" or code contains "class=":
        append "Use style: block and NC styling instead of CSS classes" to errors

    respond with {errors: errors, warnings: warnings, valid: length(errors) is 0}

// ── Auto-Fix NC Service ──────────────────────────────────

to fix_service with code and filename:
    purpose: "Automatically fix common NC service errors"
    set fixed to code
    set fixes_applied to []

    // Fix missing service declaration
    if fixed does not contain "service \"":
        set app_name to replace(filename, ".nc", "")
        set app_name to replace(app_name, "_service", "")
        set fixed to "service \"" + app_name + "-api\"\nversion \"1.0.0\"\n\n" + fixed
        append "Added service declaration" to fixes_applied

    // Fix missing api block
    if fixed does not contain "api:":
        // Detect function names and generate routes
        set routes to "\napi:\n"
        if fixed contains "to list_":
            set routes to routes + "    GET  /items          runs list_items\n"
        if fixed contains "to get_":
            set routes to routes + "    GET  /items/:id      runs get_item\n"
        if fixed contains "to create_":
            set routes to routes + "    POST /items          runs create_item\n"
        if fixed contains "to update_":
            set routes to routes + "    PUT  /items/:id      runs update_item\n"
        if fixed contains "to delete_":
            set routes to routes + "    DELETE /items/:id    runs delete_item\n"
        if fixed contains "to health":
            set routes to routes + "    GET  /health         runs health_check\n"
        set fixed to fixed + routes
        append "Added api routes block" to fixes_applied

    // Fix missing health check
    if fixed does not contain "health":
        set health_fn to "\nto health_check:\n    respond with {\"status\": \"healthy\"}\n"
        // Insert before api: block
        set fixed to replace(fixed, "api:", health_fn + "\napi:\n    GET /health runs health_check")
        append "Added health_check function" to fixes_applied

    // Fix JavaScript-style code
    set fixed to replace(fixed, "function ", "to ")
    set fixed to replace(fixed, "return ", "respond with ")
    set fixed to replace(fixed, "console.log(", "log ")
    set fixed to replace(fixed, "var ", "set ")
    set fixed to replace(fixed, "let ", "set ")
    set fixed to replace(fixed, "const ", "set ")

    respond with {code: fixed, fixes: fixes_applied}

// ── Auto-Fix NCUI Page ──────────────────────────────────

to fix_page with code and filename:
    purpose: "Automatically fix common NCUI page errors"
    set fixed to code
    set fixes_applied to []

    // Fix missing page declaration
    if fixed does not contain "page \"":
        set app_name to replace(filename, ".ncui", "")
        set app_name to capitalize(replace(app_name, "_", " "))
        set fixed to "page \"" + app_name + "\"\n\n" + fixed
        append "Added page declaration" to fixes_applied

    // Fix missing style block
    if fixed does not contain "style:":
        set style_block to "style:\n    background is \"#ffffff\"\n    text color is \"#1a1a2e\"\n    accent is \"#2563eb\"\n    font is \"Inter, system-ui, sans-serif\"\n\n"
        // Insert after page declaration
        set page_end to index_of(fixed, "\n") + 1
        set fixed to substring(fixed, 0, page_end) + "\n" + style_block + substring(fixed, page_end)
        append "Added style block" to fixes_applied

    // Fix missing section
    if fixed does not contain "section":
        set section to "\nsection \"Main\":\n    heading \"Welcome\" size 1\n    text \"Your app is ready\"\n    button \"Get Started\" style primary\n\n"
        set fixed to fixed + section
        append "Added default section" to fixes_applied

    // Fix missing footer
    if fixed does not contain "footer":
        set fixed to fixed + "\nfooter:\n    text \"Built with NC\" color muted\n"
        append "Added footer" to fixes_applied

    respond with {code: fixed, fixes: fixes_applied}

// ── Check and Fix Any NC File ────────────────────────────

to check_and_fix with filepath:
    purpose: "Validate and auto-fix any NC or NCUI file"

    set content to read_file(filepath)
    if content is empty:
        respond with error "File not found: " + filepath

    set filename to basename(filepath)
    set is_ncui to filename ends with ".ncui"

    // Validate
    if is_ncui:
        set result to validate_page(content)
    else:
        set result to validate_service(content)

    log "Checking: " + filepath
    log "  Errors: " + length(result.errors)
    log "  Warnings: " + length(result.warnings)

    // Auto-fix if errors found
    if result.valid is false:
        log "  Auto-fixing..."
        if is_ncui:
            set fix_result to fix_page(content, filename)
        else:
            set fix_result to fix_service(content, filename)

        // Write fixed file
        write_file(filepath, fix_result.code)
        log "  Applied " + length(fix_result.fixes) + " fixes:"
        repeat for each fix in fix_result.fixes:
            log "    - " + fix

        // Log error-fix pair for self-learning
        set error_log to open("nc-ai/training_data/error_fixes.txt", "append")
        write error_log "ERROR: " + join(result.errors, "; ") + "\n"
        write error_log "FIX: " + join(fix_result.fixes, "; ") + "\n"
        write error_log "FILE: " + filepath + "\n\n"
        close(error_log)

        respond with {file: filepath, fixed: true, fixes: fix_result.fixes}
    else:
        if length(result.warnings) > 0:
            log "  Warnings:"
            repeat for each w in result.warnings:
                log "    - " + w
        else:
            log "  All checks passed"
        respond with {file: filepath, fixed: false, warnings: result.warnings}

// ── Batch Check Entire Project ───────────────────────────

to check_project with project_dir:
    purpose: "Validate and fix all NC files in a project"
    set files to scan_nc_files(project_dir)
    set results to []
    set total_fixes to 0

    repeat for each filepath in files:
        set result to check_and_fix(filepath)
        append result to results
        if result.fixed:
            set total_fixes to total_fixes + length(result.fixes)

    log "Project check complete: " + length(files) + " files, " + total_fixes + " fixes applied"
    respond with {files: length(files), fixes: total_fixes, results: results}

// ── API Routes ───────────────────────────────────────────

api:
    POST /check      runs check_and_fix
    POST /project    runs check_project
