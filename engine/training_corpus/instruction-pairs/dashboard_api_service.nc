<|begin|>
// Description: an analytics dashboard with metrics, charts, and AI insights (API only, no frontend)
// Type: service
service "dashboard-api"
version "1.0.0"

to get metrics:
    set data to load("metrics.json")
    respond with data

to get summary:
    set metrics to load("metrics.json")
    set total to sum(metrics, "value")
    set average to avg(metrics, "value")
    respond with {"total": total, "average": average, "count": length(metrics)}

to add metric with data:
    set data.id to generate_id()
    set data.timestamp to now()
    set metrics to load("metrics.json")
    add data to metrics
    save metrics to "metrics.json"
    respond with data

to get trends with period:
    set metrics to load("metrics.json")
    set filtered to filter_by_period(metrics, period)
    respond with filtered

api:
    GET /metrics runs get_metrics
    GET /metrics/summary runs get_summary
    POST /metrics runs add_metric
    GET /metrics/trends runs get_trends
    GET /health runs health_check

to health check:
    respond with {"status": "ok"}
<|end|>
