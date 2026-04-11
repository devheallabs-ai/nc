// ═══════════════════════════════════════════════════════════
//  NC AI — Language Rules Database
//  Knowledge extracted from nc-lang engine and nc-ui compiler
//
//  These rules let NC AI understand NC syntax deeply
//  and fix errors on the fly. Learned from:
//    - nc-lang/engine/src/ (C source → NC syntax rules)
//    - nc-ui/compiler.js (NCUI compilation rules)
//
//  Built by DevHeal Labs AI
// ═══════════════════════════════════════════════════════════

// ── NC Language Syntax Rules (from nc-lang engine) ───────

to get_nc_syntax_rules:
    purpose: "NC language syntax rules learned from the engine"
    respond with {
        // Service declarations
        service: {
            syntax: "service \"name\"",
            required: true,
            example: "service \"my-api\"",
            fix: "Add service declaration at top of file"
        },
        version: {
            syntax: "version \"x.y.z\"",
            required: false,
            example: "version \"1.0.0\"",
            fix: "Add version after service declaration"
        },

        // Functions
        function_def: {
            syntax: "to function_name with param1 and param2:",
            patterns: [
                "to name:",
                "to name with param:",
                "to name with param1 and param2:"
            ],
            wrong: ["function name()", "def name:", "fn name()"],
            fix: "Use 'to function_name:' or 'to function_name with params:'"
        },

        // Variables
        assignment: {
            syntax: "set variable to value",
            patterns: ["set x to 1", "set name to \"hello\"", "set data to {}"],
            wrong: ["var x = 1", "let x = 1", "const x = 1", "x = 1"],
            fix: "Use 'set variable to value'"
        },

        // Return values
        respond: {
            syntax: "respond with value",
            patterns: ["respond with result", "respond with {key: value}", "respond with error \"msg\""],
            wrong: ["return value", "return result"],
            fix: "Use 'respond with value'"
        },

        // Data operations
        store: {
            syntax: "store data into \"collection\"",
            example: "store user into \"users\"",
            related: ["gather from", "find where", "remove from"]
        },
        gather: {
            syntax: "gather items from \"collection\"",
            patterns: ["gather items from \"users\"", "gather item from \"users\" where id is id"],
            related: ["store into", "find where"]
        },

        // Control flow
        conditionals: {
            syntax: "if condition:",
            patterns: [
                "if x is empty:",
                "if x is not empty:",
                "if x is equal y:",
                "if x contains y:"
            ],
            wrong: ["if (x == y)", "if x === y"],
            fix: "Use 'if x is equal y:' or 'if x is empty:'"
        },
        loops: {
            syntax: "repeat N times: / repeat for each item in list:",
            patterns: [
                "repeat 10 times:",
                "repeat for each item in items:",
                "repeat while x is true:"
            ],
            wrong: ["for (i = 0)", "while (true)", "forEach"],
            fix: "Use 'repeat N times:' or 'repeat for each item in list:'"
        },

        // API routes
        api_block: {
            syntax: "api:\n    METHOD /path runs function_name",
            methods: ["GET", "POST", "PUT", "DELETE", "PATCH"],
            example: "api:\n    GET /items runs list_items\n    POST /items runs create_item",
            fix: "Add api: block with route definitions"
        },

        // Middleware
        middleware: {
            syntax: "middleware:\n    use middleware_name",
            available: ["cors", "log_requests", "auth", "rate_limit", "compress"],
            example: "middleware:\n    use cors\n    use log_requests"
        },

        // Configure
        configure: {
            syntax: "configure:\n    key is value",
            example: "configure:\n    port is 8000\n    ai_model is \"default\"",
            options: ["port", "ai_model", "database", "cache", "log_level"]
        },

        // AI integration
        ai: {
            syntax: "ask AI to \"prompt\" save as variable",
            example: "ask AI to \"Analyze this data: {{data}}\" save as analysis",
            fix: "Use 'ask AI to \"prompt\" save as result'"
        },

        // Logging
        logging: {
            syntax: "log \"message\"",
            patterns: ["log \"Created item\"", "log \"Error: \" + msg"],
            wrong: ["console.log()", "print()", "println()"],
            fix: "Use 'log \"message\"'"
        }
    }

// ── NCUI Syntax Rules (from nc-ui compiler) ──────────────

to get_ncui_syntax_rules:
    purpose: "NC UI syntax rules learned from the compiler"
    respond with {
        // Page declaration
        page: {
            syntax: "page \"Title\"",
            required: true,
            example: "page \"My App\"",
            fix: "Add page declaration at top of .ncui file"
        },

        // Style block
        style: {
            syntax: "style:\n    property is \"value\"",
            properties: [
                "background", "text color", "accent", "font",
                "border radius", "shadow", "gradient"
            ],
            example: "style:\n    background is \"#0a0a0f\"\n    text color is \"#e0e0e8\"\n    accent is \"#2563eb\"\n    font is \"Inter, system-ui, sans-serif\""
        },

        // Layout components
        section: {
            syntax: "section \"name\":",
            attributes: ["centered", "dark", "wide", "narrow"],
            example: "section hero centered:\n    heading \"Welcome\" size 1"
        },
        grid: {
            syntax: "grid N columns:",
            options: ["2 columns", "3 columns", "4 columns"],
            example: "grid 3 columns:\n    card:\n        heading \"Feature\""
        },
        card: {
            syntax: "card [icon \"name\"]:",
            attributes: ["icon", "style", "link"],
            example: "card icon \"rocket\":\n    heading \"Fast\"\n    text \"Lightning fast\""
        },

        // Content elements
        heading: {syntax: "heading \"text\" [size N]", sizes: [1, 2, 3, 4, 5, 6]},
        text: {syntax: "text \"content\" [color name]", colors: ["muted", "accent", "primary"]},
        button: {syntax: "button \"label\" [action name] [style type]", styles: ["primary", "secondary", "danger", "ghost"]},
        input: {syntax: "input \"label\" bind variable [required]"},
        image: {syntax: "image \"url\" [alt \"description\"]"},
        link: {syntax: "link \"text\" href \"url\""},

        // Navigation
        nav: {
            syntax: "nav:\n    brand \"Name\"\n    links:\n        link \"text\" to \"#section\"",
            example: "nav:\n    brand \"My App\"\n    links:\n        link \"Home\" to \"#home\"\n        link \"About\" to \"#about\""
        },

        // Data binding
        data: {
            syntax: "data:\n    variable from \"/api/endpoint\"",
            example: "data:\n    items from \"/items\"\n    user from \"/auth/me\""
        },

        // Actions
        actions: {
            syntax: "actions:\n    on event_name:\n        action",
            example: "actions:\n    on create_item:\n        post \"/items\" with {name: new_name}\n        reload items"
        },

        // Footer
        footer: {
            syntax: "footer:\n    text \"content\"",
            example: "footer:\n    text \"Built with NC\" color muted\n    link \"Docs\" href \"/docs\""
        },

        // Animations
        animate: {
            syntax: "animate \"type\"",
            types: ["fade-up", "fade-in", "slide-left", "slide-right", "stagger", "scale"],
            example: "animate \"fade-up\""
        },

        // Templates
        template_vars: {
            syntax: "{{variable}}",
            example: "heading \"{{item.name}}\"\ntext \"Created: {{item.created_at}}\""
        }
    }

// ── Error Patterns & Fixes ───────────────────────────────

to get_common_errors:
    purpose: "Common NC errors and their fixes, learned from user patterns"
    respond with [
        {
            pattern: "function ",
            fix: "to ",
            msg: "Use 'to function_name:' instead of 'function'"
        },
        {
            pattern: "return ",
            fix: "respond with ",
            msg: "Use 'respond with' instead of 'return'"
        },
        {
            pattern: "var ",
            fix: "set ",
            msg: "Use 'set variable to value' instead of 'var'"
        },
        {
            pattern: "let ",
            fix: "set ",
            msg: "Use 'set variable to value' instead of 'let'"
        },
        {
            pattern: "const ",
            fix: "set ",
            msg: "Use 'set variable to value' instead of 'const'"
        },
        {
            pattern: "console.log(",
            fix: "log ",
            msg: "Use 'log' instead of 'console.log'"
        },
        {
            pattern: "require(",
            fix: "",
            msg: "NC has built-in modules — no require/import needed"
        },
        {
            pattern: "import ",
            fix: "",
            msg: "NC has built-in modules — no require/import needed"
        },
        {
            pattern: "<div",
            fix: "section",
            msg: "Use NC UI components (section, card, grid) instead of HTML"
        },
        {
            pattern: "className",
            fix: "style:",
            msg: "Use style: block instead of CSS classes"
        }
    ]

// ── Smart Fix Suggestions ────────────────────────────────

to suggest_fix with error_message:
    purpose: "Given an error message, suggest the NC way to fix it"

    set msg to lower(error_message)
    set suggestions to []

    if msg contains "undeclared" or msg contains "undefined":
        append "Declare with 'set variable to value' before using it" to suggestions

    if msg contains "syntax" or msg contains "unexpected":
        append "Check NC syntax: functions use 'to name:', not 'function name()'" to suggestions
        append "Variables use 'set x to value', not 'var/let/const x = value'" to suggestions

    if msg contains "not found" or msg contains "missing":
        append "Ensure the file exists and path is correct" to suggestions
        append "Check that service name matches the file" to suggestions

    if msg contains "type" or msg contains "cannot":
        append "NC is dynamically typed — check if the value is what you expect" to suggestions
        append "Use 'if x is empty:' to guard against null values" to suggestions

    if msg contains "connection" or msg contains "timeout":
        append "Check that the server is running and port is correct" to suggestions
        append "Add error handling: if response is empty: respond with error \"...\"" to suggestions

    if length(suggestions) is 0:
        append "Check the NC User Manual: nc-lang/docs/NC_USER_MANUAL.md" to suggestions
        append "Run 'nc ai check <file>' for auto-diagnosis" to suggestions

    respond with suggestions
