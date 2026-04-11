// ══════════════════════════════════════════════════════════════════
//  HiveANT — Developer Agent (Autonomous Software Development)
//
//  Generates code, configuration patches, and infrastructure
//  changes based on specifications from the Architect Agent.
// ══════════════════════════════════════════════════════════════════

to generate_code with specification, language, context:
    purpose: "Generate code from a specification"

    ask AI to "You are a Developer Agent. Generate production-quality code. SPECIFICATION: {{specification}}. LANGUAGE: {{language}}. CONTEXT: {{context}}. Requirements: 1) Follow language best practices. 2) Include error handling. 3) Write testable code. 4) Add minimal necessary comments. Return ONLY valid JSON: {\"files\": [{\"path\": \"string\", \"content\": \"string\", \"language\": \"string\"}], \"dependencies\": [\"string\"], \"tests_needed\": [\"string\"], \"notes\": \"string\"}" save as code_result

    respond with code_result

to generate_patch with issue_description, affected_files, root_cause:
    purpose: "Generate a code patch to fix an issue"

    ask AI to "You are a Developer Agent. Generate a minimal patch to fix this issue. ISSUE: {{issue_description}}. AFFECTED FILES: {{affected_files}}. ROOT CAUSE: {{root_cause}}. Return ONLY valid JSON: {\"patch\": {\"description\": \"string\", \"changes\": [{\"file\": \"string\", \"change_type\": \"modify|add|delete\", \"before\": \"string\", \"after\": \"string\", \"line_range\": \"string\"}], \"risk_level\": \"low|medium|high\", \"testing_required\": [\"string\"]}}" save as patch_result

    respond with patch_result

to generate_config with service_name, change_type, parameters:
    purpose: "Generate configuration changes"

    ask AI to "You are a Developer Agent. Generate a configuration change. SERVICE: {{service_name}}. CHANGE TYPE: {{change_type}}. PARAMETERS: {{parameters}}. Return ONLY valid JSON: {\"config_change\": {\"service\": \"string\", \"type\": \"string\", \"before\": {}, \"after\": {}, \"commands\": [{\"cmd\": \"string\", \"description\": \"string\"}], \"rollback\": [{\"cmd\": \"string\"}], \"validation\": [\"string\"]}}" save as config_result

    respond with config_result
