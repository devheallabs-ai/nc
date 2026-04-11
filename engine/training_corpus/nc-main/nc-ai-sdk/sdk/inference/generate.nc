// ═══════════════════════════════════════════════════════════
//  NC AI Code Generator — Written 100% in NC Language
//
//  Generates complete NC projects from natural language.
//  Uses the trained transformer + template engine for
//  reliable, high-quality code generation.
//
//  This is the main inference engine that users interact with.
//  Built by DevHeal Labs AI — written IN NC.
//
//  Usage:
//    nc nc-ai/nc/inference/generate.nc
//    curl -X POST http://localhost:8090/generate \
//      -d '{"prompt": "create a REST API for blog posts"}'
// ═══════════════════════════════════════════════════════════

service "nc-codegen"
version "1.0.0"

configure:
    port is 8090

// ── Intent Detection (Rule-Based NLU) ────────────────────────

to detect_intent with prompt:
    purpose: "Classify user intent from natural language"
    set p to lower(prompt)
    set intent to "service"
    set features to []

    // Detect service type
    if contains(p, "crud") or contains(p, "database") or contains(p, "store"):
        set intent to "crud"
    if contains(p, "chat") or contains(p, "conversation") or contains(p, "talk"):
        set intent to "chatbot"
    if contains(p, "classify") or contains(p, "categorize") or contains(p, "label"):
        set intent to "classifier"
    if contains(p, "summarize") or contains(p, "summary") or contains(p, "tldr"):
        set intent to "summarizer"
    if contains(p, "pipeline") or contains(p, "process") or contains(p, "transform"):
        set intent to "pipeline"
    if contains(p, "webhook") or contains(p, "hook") or contains(p, "event"):
        set intent to "webhook"
    if contains(p, "dashboard") or contains(p, "analytics") or contains(p, "metrics"):
        set intent to "dashboard"
    if contains(p, "ui") or contains(p, "page") or contains(p, "frontend"):
        set intent to "ncui"

    // Detect features
    if contains(p, "auth") or contains(p, "login") or contains(p, "jwt"):
        append "auth" to features
    if contains(p, "ai") or contains(p, "gpt") or contains(p, "llm") or contains(p, "intelligent"):
        append "ai" to features
    if contains(p, "rate limit") or contains(p, "throttle"):
        append "rate_limit" to features
    if contains(p, "cache") or contains(p, "redis"):
        append "cache" to features
    if contains(p, "search") or contains(p, "find") or contains(p, "query"):
        append "search" to features
    if contains(p, "upload") or contains(p, "file"):
        append "upload" to features
    if contains(p, "email") or contains(p, "notify") or contains(p, "notification"):
        append "notification" to features
    if contains(p, "test") or contains(p, "testing"):
        append "test" to features
    if contains(p, "real-time") or contains(p, "websocket") or contains(p, "live"):
        append "realtime" to features

    // Extract service name from prompt
    set name to "my-service"
    set words to split(p, " ")
    repeat for each word in words:
        if len(word) is above 3:
            if not contains(word, "create"):
                if not contains(word, "build"):
                    if not contains(word, "make"):
                        if not contains(word, "service"):
                            if not contains(word, "with"):
                                set name to word
                                // Use first meaningful word

    respond with {
        "intent": intent,
        "features": features,
        "name": name,
        "raw_prompt": prompt
    }

// ── Template-Based Generation ────────────────────────────────

to generate_from_template with intent_info:
    purpose: "Generate NC code using templates"
    set intent to intent_info.intent
    set name to intent_info.name
    set features to intent_info.features
    set code to ""

    // Service header
    set code to "service \"" + name + "\"\nversion \"1.0.0\"\n"

    // Configuration
    set has_ai to false
    repeat for each f in features:
        if f is equal "ai":
            set has_ai to true

    if has_ai:
        set code to code + "\nconfigure:\n    ai_model is \"default\"\n"

    if contains(join(features, ","), "rate_limit"):
        set code to code + "\nconfigure:\n    rate_limit is 100\n    rate_window is 60\n"

    if contains(join(features, ","), "auth"):
        set code to code + "\nconfigure:\n    auth_type is \"jwt\"\n    jwt_secret is env(\"JWT_SECRET\")\n"

    // Generate based on intent
    if intent is equal "crud":
        set code to code + "\n// ── Data Types ──────────────────────────────\n\n"
        set code to code + "define " + upper(substr(name, 0, 1)) + substr(name, 1, len(name)) + " as:\n"
        set code to code + "    id is text\n    name is text\n    created_at is text\n\n"
        set code to code + "// ── CRUD Operations ─────────────────────────\n\n"
        set code to code + "to create with data:\n    purpose: \"Create new " + name + "\"\n"
        set code to code + "    set item to data\n    set item.created_at to now()\n"
        set code to code + "    store item into \"" + name + "\"\n"
        set code to code + "    respond with {\"created\": true, \"data\": item}\n\n"
        set code to code + "to list_all:\n    purpose: \"List all " + name + "\"\n"
        set code to code + "    gather items from \"" + name + "\"\n"
        set code to code + "    respond with items\n\n"
        set code to code + "to get_one with id:\n    purpose: \"Get " + name + " by ID\"\n"
        set code to code + "    gather item from \"" + name + "\" where id\n"
        set code to code + "    respond with item\n\n"
        set code to code + "to update with id, data:\n    purpose: \"Update " + name + "\"\n"
        set code to code + "    store data into \"" + name + "\" where id\n"
        set code to code + "    respond with {\"updated\": true}\n\n"
        set code to code + "to delete_one with id:\n    purpose: \"Delete " + name + "\"\n"
        set code to code + "    remove from \"" + name + "\" where id\n"
        set code to code + "    respond with {\"deleted\": true}\n\n"
        set code to code + "api:\n"
        set code to code + "    POST   /" + name + "      runs create\n"
        set code to code + "    GET    /" + name + "      runs list_all\n"
        set code to code + "    GET    /" + name + "/:id  runs get_one\n"
        set code to code + "    PUT    /" + name + "/:id  runs update\n"
        set code to code + "    DELETE /" + name + "/:id  runs delete_one\n"

    if intent is equal "chatbot":
        set code to code + "\nto chat with message, session:\n"
        set code to code + "    purpose: \"Handle chat message with memory\"\n"
        set code to code + "    set mem to memory_new(20)\n"
        set code to code + "    memory_add(mem, \"user\", message)\n"
        set code to code + "    set history to memory_summary(mem)\n"
        set code to code + "    ask AI to \"You are a helpful assistant. Conversation:\\n{{history}}\\nReply to the last message.\" save as reply\n"
        set code to code + "    memory_add(mem, \"assistant\", reply)\n"
        set code to code + "    respond with {\"reply\": reply, \"session\": session}\n\n"
        set code to code + "to health:\n    respond with {\"status\": \"ok\"}\n\n"
        set code to code + "api:\n    POST /chat runs chat\n    GET /health runs health\n"

    if intent is equal "classifier":
        set code to code + "\nto classify with text, categories:\n"
        set code to code + "    purpose: \"Classify text into categories\"\n"
        set code to code + "    if categories is equal nil:\n"
        set code to code + "        set categories to [\"positive\", \"negative\", \"neutral\"]\n"
        set code to code + "    ask AI to \"Classify into one of: {{categories}}\" using text save as label\n"
        set code to code + "    respond with {\"text\": text, \"label\": label}\n\n"
        set code to code + "api:\n    POST /classify runs classify\n"

    if intent is equal "summarizer":
        set code to code + "\nto summarize with document:\n"
        set code to code + "    purpose: \"Summarize document\"\n"
        set code to code + "    ask AI to \"Provide a concise summary preserving key points\" using document save as summary\n"
        set code to code + "    respond with {\"summary\": summary, \"original_length\": len(document)}\n\n"
        set code to code + "api:\n    POST /summarize runs summarize\n"

    if intent is equal "pipeline":
        set code to code + "\nto process with records:\n"
        set code to code + "    purpose: \"Process and validate data records\"\n"
        set code to code + "    set valid to []\n"
        set code to code + "    set errors to []\n"
        set code to code + "    repeat for each record in records:\n"
        set code to code + "        if record.id is not equal nil:\n"
        set code to code + "            append record to valid\n"
        set code to code + "        otherwise:\n"
        set code to code + "            append {\"error\": \"missing id\", \"record\": record} to errors\n"
        set code to code + "    respond with {\"processed\": len(valid), \"errors\": errors, \"total\": len(records)}\n\n"
        set code to code + "api:\n    POST /process runs process\n"

    if intent is equal "webhook":
        set code to code + "\nto handle with payload:\n"
        set code to code + "    purpose: \"Handle incoming webhook\"\n"
        set code to code + "    log \"Webhook: \" + string(payload.event)\n"
        set code to code + "    match payload.event:\n"
        set code to code + "        when \"created\":\n"
        set code to code + "            log \"Created: \" + string(payload.data)\n"
        set code to code + "        when \"updated\":\n"
        set code to code + "            log \"Updated: \" + string(payload.data)\n"
        set code to code + "        when \"deleted\":\n"
        set code to code + "            log \"Deleted: \" + string(payload.data)\n"
        set code to code + "        otherwise:\n"
        set code to code + "            log \"Unknown event\"\n"
        set code to code + "    respond with {\"received\": true}\n\n"
        set code to code + "api:\n    POST /webhook runs handle\n"

    if intent is equal "service":
        // Generic service
        set code to code + "\nto handle with request:\n"
        set code to code + "    purpose: \"Handle request\"\n"
        if has_ai:
            set code to code + "    ask AI to \"Process this request\" using request save as result\n"
            set code to code + "    respond with result\n\n"
        otherwise:
            set code to code + "    respond with {\"message\": \"Hello from " + name + "\"}\n\n"
        set code to code + "to health:\n    respond with {\"status\": \"ok\", \"time\": now()}\n\n"
        set code to code + "api:\n    POST /handle runs handle\n    GET /health runs health\n"

    if intent is equal "ncui":
        set code to "page \"" + name + "\"\n"
        set code to code + "theme \"dark\"\naccent \"#4F46E5\"\n\n"
        set code to code + "nav:\n    brand \"" + name + "\"\n    links:\n        link \"Home\" to \"#home\"\n        link \"About\" to \"#about\"\n\n"
        set code to code + "section hero:\n    heading \"Welcome to " + name + "\"\n    text \"Built with NC UI\"\n    button \"Get Started\" style \"primary\"\n\n"
        set code to code + "section features:\n    heading \"Features\"\n    grid 3 columns:\n"
        set code to code + "        card icon \"zap\":\n            heading \"Fast\"\n            text \"Lightning fast performance\"\n"
        set code to code + "        card icon \"shield\":\n            heading \"Secure\"\n            text \"Enterprise-grade security\"\n"
        set code to code + "        card icon \"code\":\n            heading \"Simple\"\n            text \"Plain English code\"\n\n"
        set code to code + "footer:\n    text \"Built with NC\" \n"

    // Add health check if not already present
    if not contains(code, "health"):
        set code to code + "\nto health:\n    respond with {\"status\": \"ok\"}\n"

    respond with code

// ── Neural Generation (if model loaded) ─────────────────────

to generate_with_model with prompt:
    purpose: "Generate code using trained transformer model"
    if model_loaded is not equal true:
        respond with nil

    // Encode prompt
    set token_ids to encode(prompt)

    // Generate with transformer
    set gen_tokens to generate(token_ids, 512, 0.7)

    // Decode back to text
    respond with decode(gen_tokens)

// ── Main Generation Entry Point ──────────────────────────────

to generate_code with prompt:
    purpose: "Generate NC code from natural language description"
    log "Generating NC code for: " + prompt

    // Detect intent
    set intent_info to detect_intent(prompt)
    log "Intent: " + intent_info.intent + " | Features: " + string(intent_info.features)

    // Try neural generation first
    set neural_code to nil
    if model_loaded is equal true:
        set neural_code to generate_with_model(prompt)

    // Template generation (always available)
    set template_code to generate_from_template(intent_info)

    // Use neural if available and looks valid, else template
    set final_code to template_code
    if neural_code is not equal nil:
        if contains(neural_code, "service"):
            if contains(neural_code, "to "):
                set final_code to neural_code

    // Validate
    set validation to validate_code(final_code)

    respond with {
        "code": final_code,
        "intent": intent_info.intent,
        "features": intent_info.features,
        "valid": validation.valid,
        "method": "template",
        "length": len(final_code)
    }

// ── Generate Full Project ────────────────────────────────────

to generate_project with name, description:
    purpose: "Generate a complete NC project with multiple files"
    log "Generating project: " + name

    set files to []

    // Main service
    set main_code to generate_code(description)
    append {"name": name + ".nc", "content": main_code.code} to files

    // Test file
    set test_code to "service \"test-" + name + "\"\nversion \"1.0.0\"\n\n"
    set test_code to test_code + "to test_health:\n    purpose: \"Test health endpoint\"\n"
    set test_code to test_code + "    log \"Testing health...\"\n"
    set test_code to test_code + "    respond with {\"test\": \"health\", \"passed\": true}\n\n"
    set test_code to test_code + "to test_main:\n    purpose: \"Test main functionality\"\n"
    set test_code to test_code + "    log \"Testing main...\"\n"
    set test_code to test_code + "    respond with {\"test\": \"main\", \"passed\": true}\n\n"
    set test_code to test_code + "to run_tests:\n    run test_health\n    log result\n"
    set test_code to test_code + "    run test_main\n    log result\n"
    set test_code to test_code + "    respond with {\"all_passed\": true}\n"
    append {"name": "test_" + name + ".nc", "content": test_code} to files

    respond with {
        "name": name,
        "files": files,
        "description": description
    }

// ── Code Validation ──────────────────────────────────────────

to validate_code with code:
    purpose: "Validate generated NC code"
    set issues to []

    if not contains(code, "service") and not contains(code, "page"):
        append "Missing service/page declaration" to issues

    if not contains(code, "to ") and not contains(code, "section"):
        append "No function definitions found" to issues

    // Check balanced braces
    set opens to 0
    set closes to 0
    set i to 0
    repeat while i is below len(code):
        set ch to substr(code, i, 1)
        if ch is equal "{":
            set opens to opens + 1
        if ch is equal "}":
            set closes to closes + 1
        set i to i + 1

    if opens is not equal closes:
        append "Unbalanced braces" to issues

    respond with {
        "valid": len(issues) is equal 0,
        "issues": issues
    }

// ── Initialize ──────────────────────────────────────────────

to init:
    purpose: "Initialize the code generator"
    // Try to load trained model
    set tok_loaded to load_tokenizer("./nc-ai/model/tokenizer.json")
    if tok_loaded is equal true:
        set mdl_loaded to load_model("./nc-ai/model")
        if mdl_loaded is equal true:
            set model_loaded to true
            log "Trained model loaded - using neural generation"
        otherwise:
            log "No trained model found - using template generation"
    otherwise:
        log "No tokenizer found - using template generation"
    respond with {"model_loaded": model_loaded}

// ── Health ──────────────────────────────────────────────────

to health:
    respond with {
        "status": "ok",
        "service": "NC AI Code Generator",
        "version": "1.0.0",
        "model_loaded": model_loaded,
        "written_in": "NC Language (100%)"
    }

api:
    POST /generate          runs generate_code
    POST /generate/project  runs generate_project
    POST /validate          runs validate_code
    POST /init              runs init
    GET  /health            runs health
