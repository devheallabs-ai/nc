// ══════════════════════════════════════════════════════════════════
//  HiveANT — Deployment Agent (Autonomous Software Development)
//
//  Manages software releases: creates deployment plans,
//  executes canary/blue-green deployments, monitors rollouts.
// ══════════════════════════════════════════════════════════════════

to plan_deployment with service_name, version, changes, strategy:
    purpose: "Create a deployment plan"

    ask AI to "You are a Deployment Agent. Create a deployment plan. SERVICE: {{service_name}}. VERSION: {{version}}. CHANGES: {{changes}}. STRATEGY: {{strategy}} (canary|blue-green|rolling|recreate). Return ONLY valid JSON: {\"deployment_plan\": {\"service\": \"string\", \"version\": \"string\", \"strategy\": \"string\", \"pre_deploy\": [{\"step\": 0, \"action\": \"string\", \"command\": \"string\"}], \"deploy_steps\": [{\"step\": 0, \"action\": \"string\", \"command\": \"string\", \"verification\": \"string\"}], \"post_deploy\": [{\"step\": 0, \"action\": \"string\", \"command\": \"string\"}], \"rollback_plan\": [{\"step\": 0, \"command\": \"string\"}], \"canary_config\": {\"initial_percent\": 0, \"increment\": 0, \"interval_minutes\": 0}, \"monitoring\": [{\"metric\": \"string\", \"threshold\": \"string\", \"action_if_breached\": \"string\"}], \"estimated_duration_minutes\": 0, \"risk_level\": \"low|medium|high\"}}" save as plan

    respond with plan

to monitor_rollout with deployment_id, service_name:
    purpose: "Monitor a deployment rollout for issues"

    set prom_url to env("PROMETHEUS_URL")
    set health to "unknown"
    set urls_str to env("MONITOR_URLS")
    if urls_str:
        set health to shell("for url in $(echo '" + urls_str + "' | tr ',' ' '); do echo \"$url: $(curl -s -o /dev/null -w '%{http_code}' --connect-timeout 3 $url 2>/dev/null)\"; done")

    ask AI to "You are a Deployment Monitor Agent. Check the health of this rollout. SERVICE: {{service_name}}. DEPLOYMENT: {{deployment_id}}. HEALTH: {{health}}. Return ONLY valid JSON: {\"rollout_status\": \"healthy|degraded|failing\", \"metrics\": [{\"metric\": \"string\", \"status\": \"ok|warning|critical\"}], \"recommendation\": \"continue|pause|rollback\", \"details\": \"string\"}" save as monitor

    respond with monitor
