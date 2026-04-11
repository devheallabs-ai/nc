// NC Standard Library — migrate/hybrid
// Hybrid execution: NC orchestrates, other runtimes do the heavy lifting
// Use when ML models, GPU compute, or complex libraries shouldn't be rewritten

service "nc.migrate.hybrid"
version "1.0.0"
description "Hybrid execution wrappers — run Python/Java/Go from NC seamlessly"

// ═══════════════════════════════════════════════════════════
//  Runtime executors — call any language from NC
// ═══════════════════════════════════════════════════════════

to run_python with script and args:
    purpose: "Execute a Python script and return its output"
    try:
        set cmd to "python3 {{script}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Python execution failed: {{e}}"
        respond with nothing

to run_python_with_input with script and input_data:
    purpose: "Execute Python with JSON input via stdin"
    try:
        set json_input to json_encode(input_data)
        write_file("/tmp/_nc_hybrid_input.json", json_input)
        set result to exec("python3 {{script}} /tmp/_nc_hybrid_input.json")
        set parsed to json_decode(result)
        respond with parsed
    on error e:
        show "Python execution failed: {{e}}"
        respond with nothing

to run_java with class_or_jar and args:
    purpose: "Execute a Java class or JAR and return its output"
    try:
        if class_or_jar ends_with ".jar":
            set cmd to "java -jar {{class_or_jar}} {{args}}"
        otherwise:
            set cmd to "java {{class_or_jar}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Java execution failed: {{e}}"
        respond with nothing

to run_node with script and args:
    purpose: "Execute a Node.js script and return its output"
    try:
        set cmd to "node {{script}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Node.js execution failed: {{e}}"
        respond with nothing

to run_go with file_or_binary and args:
    purpose: "Execute a Go file or binary and return its output"
    try:
        if file_or_binary ends_with ".go":
            set cmd to "go run {{file_or_binary}} {{args}}"
        otherwise:
            set cmd to "{{file_or_binary}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Go execution failed: {{e}}"
        respond with nothing

to run_ruby with script and args:
    purpose: "Execute a Ruby script and return its output"
    try:
        set cmd to "ruby {{script}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Ruby execution failed: {{e}}"
        respond with nothing

to run_rust with binary and args:
    purpose: "Execute a Rust binary and return its output"
    try:
        set cmd to "{{binary}} {{args}}"
        set result to exec(cmd)
        respond with result
    on error e:
        show "Rust execution failed: {{e}}"
        respond with nothing

to run_any with command:
    purpose: "Execute any shell command and return parsed output"
    // Security: reject commands containing shell metacharacters
    set dangerous_chars to [";", "|", "&", "`", "$", "(", ")", "{", "}", "<", ">"]
    repeat for each ch in dangerous_chars:
        if command contains ch:
            show "Security: rejected command containing '{{ch}}'"
            respond with {ok: no, error: "Command contains forbidden shell metacharacter"}
    try:
        set result to exec(command)
        try:
            set parsed to json_decode(result)
            respond with parsed
        on error:
            respond with result
    on error e:
        show "Command failed: {{e}}"
        respond with nothing

// ═══════════════════════════════════════════════════════════
//  ML Model Wrappers — seamless model interaction
// ═══════════════════════════════════════════════════════════

to python_predict with model_script and input_data:
    purpose: "Send data to a Python ML model and get predictions"
    set json_input to json_encode(input_data)
    write_file("/tmp/_nc_ml_input.json", json_input)
    try:
        set raw to exec("python3 {{model_script}} predict /tmp/_nc_ml_input.json")
        set prediction to json_decode(raw)
        respond with prediction
    on error e:
        show "ML prediction failed: {{e}}"
        respond with {ok: no, error: e}

to python_train with model_script and training_config:
    purpose: "Trigger model training via Python and monitor progress"
    set config_json to json_encode(training_config)
    write_file("/tmp/_nc_train_config.json", config_json)
    try:
        set raw to exec("python3 {{model_script}} train /tmp/_nc_train_config.json")
        set result to json_decode(raw)
        respond with result
    on error e:
        show "ML training failed: {{e}}"
        respond with {ok: no, error: e}

to python_evaluate with model_script and test_data:
    purpose: "Evaluate an ML model on test data"
    set json_input to json_encode(test_data)
    write_file("/tmp/_nc_eval_input.json", json_input)
    try:
        set raw to exec("python3 {{model_script}} evaluate /tmp/_nc_eval_input.json")
        set metrics to json_decode(raw)
        respond with metrics
    on error e:
        show "ML evaluation failed: {{e}}"
        respond with {ok: no, error: e}

// ═══════════════════════════════════════════════════════════
//  Service Wrappers — keep services in their original runtime
// ═══════════════════════════════════════════════════════════

to start_service with command and port:
    purpose: "Start an external service and wait for it to be ready"
    // Security: validate port is numeric and in valid range
    if port is below 1:
        respond with {ok: no, error: "Invalid port number"}
    if port is above 65535:
        respond with {ok: no, error: "Invalid port number"}
    show "Starting service on port {{port}}..."
    set pid to exec("{{command}} &")
    // Wait for service to be ready
    set ready to no
    set attempts to 0
    while ready is no:
        set attempts to attempts + 1
        if attempts is above 30:
            show "Service failed to start after 30 attempts"
            respond with {ok: no, error: "timeout"}
        try:
            gather from "http://localhost:{{port}}/health":
                save as: health
            set ready to yes
        on error:
            exec("sleep 1")
    show "Service ready on port {{port}}"
    respond with {ok: yes, port: port, pid: pid}

to call_service with base_url and endpoint and data:
    purpose: "Call an external service API and return the response"
    set url to "{{base_url}}{{endpoint}}"
    set body to json_encode(data)
    try:
        gather from url:
            method: "POST"
            body: body
            save as: response
        respond with response
    on error e:
        show "Service call failed: {{e}}"
        respond with {ok: no, error: e}

// ═══════════════════════════════════════════════════════════
//  Pipeline: chain NC + external runtimes
// ═══════════════════════════════════════════════════════════

to hybrid_pipeline with steps:
    purpose: "Execute a sequence of NC and external steps"
    set results to []
    set previous_output to nothing

    repeat for each step in steps:
        if step.type is "nc":
            set output to step.behavior(previous_output)
        otherwise if step.type is "python":
            set output to run_python_with_input(step.script, previous_output)
        otherwise if step.type is "shell":
            set output to run_any(step.command)
        otherwise:
            show "Unknown step type: {{step.type}}"
            set output to nothing

        set previous_output to output
        append {step: step.name, output: output} to results

    respond with results

// ═══════════════════════════════════════════════════════════
//  Environment check — verify runtimes are available
// ═══════════════════════════════════════════════════════════

to check_runtime with runtime:
    purpose: "Check if a runtime is installed and accessible"
    try:
        match runtime:
            when "python":
                set version to exec("python3 --version")
            when "java":
                set version to exec("java -version 2>&1")
            when "node":
                set version to exec("node --version")
            when "go":
                set version to exec("go version")
            when "ruby":
                set version to exec("ruby --version")
            when "rust":
                set version to exec("rustc --version")
            otherwise:
                set version to exec("{{runtime}} --version 2>&1")
        respond with {available: yes, version: trim(version)}
    on error:
        respond with {available: no, version: nothing}

to check_all_runtimes:
    purpose: "Check which runtimes are available on this system"
    set runtimes to ["python", "java", "node", "go", "ruby", "rust"]
    set results to {}
    repeat for each rt in runtimes:
        set status to check_runtime(rt)
        set results.{{rt}} to status
    respond with results
