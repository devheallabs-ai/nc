// ══════════════════════════════════════════════════════════════════
//  HiveANT — Code Generation Model Layer
//
//  Generates configuration patches, infrastructure code,
//  and remediation scripts.
// ══════════════════════════════════════════════════════════════════

to gen_config_patch with service_name, issue, current_config:
    purpose: "Generate a configuration patch"
    ask AI to "Generate a configuration patch for {{service_name}} to fix: {{issue}}. CURRENT CONFIG: {{current_config}}. Return ONLY valid JSON: {\"patch\": {\"file\": \"string\", \"changes\": [{\"key\": \"string\", \"old_value\": \"string\", \"new_value\": \"string\", \"reason\": \"string\"}]}, \"apply_command\": \"string\", \"rollback_command\": \"string\", \"validation\": \"string\"}" save as patch
    respond with patch

to gen_remediation_script with root_cause, context:
    purpose: "Generate a remediation script"
    ask AI to "Generate a safe remediation script for: {{root_cause}}. CONTEXT: {{context}}. Return ONLY valid JSON: {\"script\": {\"language\": \"bash\", \"commands\": [{\"cmd\": \"string\", \"description\": \"string\", \"idempotent\": true, \"reversible\": true}]}, \"pre_checks\": [\"string\"], \"post_checks\": [\"string\"], \"rollback\": [{\"cmd\": \"string\"}]}" save as script
    respond with script
