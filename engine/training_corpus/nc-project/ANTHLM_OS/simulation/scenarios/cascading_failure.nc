// ══════════════════════════════════════════════════════════════════
//  HiveANT — Simulation Scenario: Cascading Failure
// ══════════════════════════════════════════════════════════════════

to scenario_cascade with entry_service, dependency_chain:
    purpose: "Generate a cascading failure simulation"

    ask AI to "Generate a cascading failure scenario. ENTRY SERVICE: {{entry_service}}. DEPENDENCY CHAIN: {{dependency_chain}}. Show how failure propagates through the chain with synthetic telemetry at each hop. Return ONLY valid JSON: {\"scenario_id\": \"string\", \"type\": \"cascading_failure\", \"cascade_path\": [{\"service\": \"string\", \"failure_type\": \"string\", \"delay_seconds\": 0, \"symptoms\": [\"string\"]}], \"synthetic_data\": {\"logs\": [\"string\"], \"metrics\": [{\"name\": \"string\", \"value\": \"string\"}]}, \"actual_root_cause\": \"string\", \"blast_radius\": [\"string\"]}" save as scenario

    respond with scenario
