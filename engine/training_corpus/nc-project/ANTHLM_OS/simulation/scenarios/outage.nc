// ══════════════════════════════════════════════════════════════════
//  HiveANT — Simulation Scenario: Service Outage
// ══════════════════════════════════════════════════════════════════

to scenario_outage with service_name, severity:
    purpose: "Generate a service outage simulation scenario"

    ask AI to "Generate a realistic service outage scenario. SERVICE: {{service_name}}. SEVERITY: {{severity}}. Include: synthetic logs, metrics, events, deploy history, the actual root cause, and the correct fix. Return ONLY valid JSON: {\"scenario_id\": \"string\", \"type\": \"outage\", \"service\": \"string\", \"severity\": \"string\", \"synthetic_data\": {\"logs\": [\"string\"], \"metrics\": [{\"name\": \"string\", \"value\": \"string\"}], \"events\": [{\"time\": \"string\", \"event\": \"string\"}]}, \"actual_root_cause\": \"string\", \"actual_fix\": \"string\", \"expected_detection_time_seconds\": 0}" save as scenario

    respond with scenario
