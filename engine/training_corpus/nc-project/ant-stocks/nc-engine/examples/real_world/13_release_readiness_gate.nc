// ═══════════════════════════════════════════════════════════
//  Release Readiness Gate
//
//  Real-world use case:
//  - Convert quality signals into one release score
//  - Route to ship / manual review / hold
//  - Generate release note summaries from telemetry
//
//  curl -X POST http://localhost:8000/release/decision \
//    -d '{"report":{"failed_tests":3,"error_rate":1.5,"rollback_count":0,"critical_incidents":0}}'
// ═══════════════════════════════════════════════════════════

service "release-readiness-gate"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o-mini"

to compute_release_score with report:
    purpose: "Score release quality using deterministic guardrails"

    set score to 100

    if report.failed_tests is above 0:
        set score to score - report.failed_tests * 5

    if report.error_rate is above 2:
        set score to score - 20

    if report.rollback_count is above 0:
        set score to score - 15

    if report.critical_incidents is above 0:
        set score to score - 30

    if score is below 0:
        set score to 0

    respond with score

to decide_release with report:
    purpose: "Turn release score into a clear decision"

    run compute_release_score with report
    set score to result

    if score is above 85:
        respond with {"decision": "ship", "score": score}
    otherwise:
        if score is above 70:
            respond with {"decision": "manual_review", "score": score}
        otherwise:
            respond with {"decision": "hold", "score": score}

to draft_release_notes with report:
    purpose: "Summarize release state for engineers and stakeholders"

    ask AI to "Write concise release notes from this report. Include: quality summary, known risks, mitigation steps, and rollout recommendation.\n\nReport: {{report}}" save as notes

    respond with notes

api:
    POST /release/score runs compute_release_score
    POST /release/decision runs decide_release
    POST /release/notes runs draft_release_notes
