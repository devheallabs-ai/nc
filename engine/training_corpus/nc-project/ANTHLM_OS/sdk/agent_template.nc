// ══════════════════════════════════════════════════════════════════
//  HiveANT — Agent Development Template (SDK)
//
//  Use this template to create custom agents for the HiveANT
//  swarm ecosystem. Copy this file and modify the behaviors.
//
//  Custom agents can:
//    - Query the pheromone graph
//    - Access cognitive memory
//    - Publish/subscribe to the message bus
//    - Interact with the digital twin
//    - Use AI for reasoning
// ══════════════════════════════════════════════════════════════════

// STEP 1: Define your agent's main behavior
to custom_agent_run with input_data, context:
    purpose: "Main entry point for your custom agent"
    log "CUSTOM AGENT: Starting with input=" + str(input_data)

    // STEP 2: Load relevant knowledge
    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")

    // STEP 3: Use AI for reasoning
    ask AI to "You are a custom agent in the HiveANT swarm. Analyze the input and produce results. INPUT: {{input_data}}. CONTEXT: {{context}}. KNOWLEDGE: {{semantic}}. Return ONLY valid JSON with your analysis results." save as agent_result

    // STEP 4: Store results
    shell("mkdir -p agents_state/custom")
    set result_id to "CUSTOM-" + str(floor(random() * 90000 + 10000))
    write_file("agents_state/custom/" + result_id + ".json", json_encode(agent_result))

    // STEP 5: Return results
    set agent_result.result_id to result_id
    set agent_result.completed_at to time_now()
    respond with agent_result

// STEP 6: Define any helper behaviors your agent needs
to custom_agent_status:
    purpose: "Check custom agent status"
    set result_count to shell("ls agents_state/custom/*.json 2>/dev/null | wc -l || echo 0")
    respond with {"agent_type": "custom", "results_produced": trim(result_count)}
