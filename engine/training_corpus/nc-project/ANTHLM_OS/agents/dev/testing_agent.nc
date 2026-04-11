// ══════════════════════════════════════════════════════════════════
//  HiveANT — Testing Agent (Autonomous Software Development)
//
//  Generates and executes test suites, regression tests,
//  and performance benchmarks.
// ══════════════════════════════════════════════════════════════════

to generate_tests with code, language, test_framework:
    purpose: "Generate test suite for code"

    ask AI to "You are a Testing Agent. Generate comprehensive tests. CODE: {{code}}. LANGUAGE: {{language}}. FRAMEWORK: {{test_framework}}. Generate: 1) Unit tests for all functions. 2) Edge case tests. 3) Integration test outlines. 4) Error handling tests. Return ONLY valid JSON: {\"test_files\": [{\"path\": \"string\", \"content\": \"string\", \"test_count\": 0}], \"coverage_estimate\": 0.0, \"test_categories\": {\"unit\": 0, \"integration\": 0, \"edge_case\": 0, \"error_handling\": 0}}" save as tests

    respond with tests

to run_regression with service_name, change_description:
    purpose: "Design regression test plan for a change"

    ask AI to "You are a Testing Agent. Design a regression test plan. SERVICE: {{service_name}}. CHANGE: {{change_description}}. Return ONLY valid JSON: {\"regression_plan\": {\"critical_paths\": [\"string\"], \"test_cases\": [{\"name\": \"string\", \"type\": \"smoke|functional|performance|integration\", \"steps\": [\"string\"], \"expected\": \"string\", \"priority\": \"high|medium|low\"}], \"estimated_duration_minutes\": 0, \"risk_areas\": [\"string\"]}}" save as plan

    respond with plan
