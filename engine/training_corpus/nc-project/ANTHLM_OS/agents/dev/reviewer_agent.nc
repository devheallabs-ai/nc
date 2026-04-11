// ══════════════════════════════════════════════════════════════════
//  HiveANT — Reviewer Agent (Autonomous Software Development)
//
//  Performs automated code review, security analysis, and
//  quality assessment on generated or existing code.
// ══════════════════════════════════════════════════════════════════

to review_code with code, language, context:
    purpose: "Perform automated code review"

    ask AI to "You are a Senior Code Reviewer Agent. Review this code thoroughly. CODE: {{code}}. LANGUAGE: {{language}}. CONTEXT: {{context}}. Check for: 1) Security vulnerabilities (injection, auth bypass, data exposure). 2) Performance issues (N+1 queries, memory leaks, blocking calls). 3) Error handling completeness. 4) Code quality and maintainability. 5) Test coverage gaps. Return ONLY valid JSON: {\"review_score\": 0.0, \"approved\": true, \"issues\": [{\"severity\": \"critical|high|medium|low\", \"category\": \"security|performance|quality|testing|style\", \"line\": \"string\", \"description\": \"string\", \"suggestion\": \"string\"}], \"security_findings\": [{\"vulnerability\": \"string\", \"severity\": \"string\", \"cwe\": \"string\"}], \"summary\": \"string\"}" save as review

    respond with review

to review_config with config_change, service_name:
    purpose: "Review a configuration change for safety"

    ask AI to "You are a Configuration Review Agent. Review this config change for safety. CONFIG: {{config_change}}. SERVICE: {{service_name}}. Check: 1) Will this cause downtime? 2) Is it reversible? 3) Does it affect other services? 4) Are secrets properly handled? Return ONLY valid JSON: {\"approved\": true, \"risk_level\": \"low|medium|high\", \"issues\": [{\"issue\": \"string\", \"severity\": \"string\"}], \"recommendation\": \"approve|modify|reject\"}" save as review

    respond with review
