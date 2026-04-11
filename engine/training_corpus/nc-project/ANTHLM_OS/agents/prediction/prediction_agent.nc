// ══════════════════════════════════════════════════════════════════
//  HiveANT — Prediction Agent
//
//  Predicts future failures using learned patterns.
//  Analyzes trends, seasonal patterns, and pheromone graph data
//  to forecast upcoming incidents.
// ══════════════════════════════════════════════════════════════════

to predict_failures with service_name, time_horizon:
    purpose: "Predict future failures for a service"
    log "PREDICTION AGENT: Forecasting failures for " + str(service_name)

    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")

    set procedural to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural to read_file("knowledge/procedural.json")

    set rl_policy to "none"
    if file_exists("knowledge/rl-policy.json"):
        set rl_policy to read_file("knowledge/rl-policy.json")

    set incident_history to shell("ls incidents/*.json 2>/dev/null | tail -30 | while read f; do cat \"$f\" 2>/dev/null | head -c 200; echo '---'; done || echo NONE")

    set pheromone_data to "none"
    if file_exists("memory/pheromone/graph_db.json"):
        set pheromone_data to read_file("memory/pheromone/graph_db.json")

    set current_metrics to "not available"
    set prom_url to env("PROMETHEUS_URL")
    if prom_url:
        set trend_q to prom_url + "/api/v1/query?query=rate(http_requests_total{service=\"" + service_name + "\"}[1h])"
        gather current_metrics from trend_q

    if time_horizon:
        set horizon to time_horizon
    otherwise:
        set horizon to "24h"

    ask AI to "You are the Prediction Agent in HiveANT. Forecast potential failures.

SERVICE: {{service_name}}
TIME HORIZON: {{horizon}}

LEARNED PATTERNS:
{{semantic}}

KNOWN RUNBOOKS:
{{procedural}}

RL POLICY:
{{rl_policy}}

INCIDENT HISTORY:
{{incident_history}}

PHEROMONE GRAPH (causal relationships):
{{pheromone_data}}

CURRENT METRICS:
{{current_metrics}}

Analyze:
1. Recurring patterns — do incidents cluster at certain times/conditions?
2. Trend analysis — are error rates, latency, or resource usage trending up?
3. Dependency risks — are upstream/downstream services showing stress?
4. Deploy risk — are recent changes similar to past failure triggers?
5. Pheromone hotspots — which causal paths have strong pheromone (likely failure modes)?

Return ONLY valid JSON:
{
  \"service\": \"string\",
  \"time_horizon\": \"string\",
  \"predictions\": [
    {
      \"failure_type\": \"string\",
      \"probability_percent\": 0,
      \"severity\": \"critical|high|medium|low\",
      \"estimated_time\": \"string\",
      \"trigger_conditions\": [\"string\"],
      \"evidence\": \"string\",
      \"prevention_steps\": [\"string\"]
    }
  ],
  \"risk_score\": 0.0,
  \"risk_level\": \"critical|high|medium|low\",
  \"proactive_actions\": [
    {\"action\": \"string\", \"priority\": \"high|medium|low\", \"command\": \"string\"}
  ],
  \"monitoring_recommendations\": [\"string\"]
}" save as predictions

    set predictions.predicted_at to time_now()
    shell("mkdir -p knowledge")
    write_file("knowledge/predictions-" + service_name + ".json", json_encode(predictions))

    log "PREDICTION AGENT: Risk level = " + predictions.risk_level + " for " + service_name
    respond with predictions

to predict_from_deploy with service_name, change_description, files_changed:
    purpose: "Predict if a deployment will cause failures"

    set semantic to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic to read_file("knowledge/semantic.json")

    ask AI to "You are the Prediction Agent. A deployment is about to happen. Predict whether it will cause failures.

SERVICE: {{service_name}}
CHANGE DESCRIPTION: {{change_description}}
FILES CHANGED: {{files_changed}}
LEARNED PATTERNS: {{semantic}}

Return ONLY valid JSON:
{
  \"deploy_risk_score\": 0.0,
  \"risk_level\": \"critical|high|medium|low\",
  \"predicted_issues\": [{\"issue\": \"string\", \"probability\": 0.0, \"mitigation\": \"string\"}],
  \"similar_past_incidents\": [\"string\"],
  \"recommendation\": \"proceed|caution|delay|block\",
  \"pre_deploy_checks\": [\"string\"]
}" save as deploy_pred

    set deploy_pred.service to service_name
    set deploy_pred.predicted_at to time_now()
    respond with deploy_pred
