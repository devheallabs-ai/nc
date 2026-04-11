// ═══════════════════════════════════════════════════════════
//  FinOps Guardrails
//
//  Real-world use case:
//  - Budget burn monitoring
//  - Rightsizing candidate detection
//  - Portfolio-level spend summaries
//
//  curl -X POST http://localhost:8000/portfolio/summary \
//    -d '{"workspaces":[{"name":"core-api","current_month_spend":9200,"monthly_budget":10000}]}'
// ═══════════════════════════════════════════════════════════

service "finops-guardrails"
version "1.0.0"

to evaluate_workspace with workspace:
    purpose: "Evaluate budget burn health for a single workspace"

    set burn_rate to workspace.current_month_spend / workspace.monthly_budget
    set projected_spend to burn_rate * workspace.monthly_budget

    if burn_rate is above 1:
        respond with {"workspace": workspace.name, "level": "critical", "burn_rate": burn_rate, "projected_spend": projected_spend}
    otherwise:
        if burn_rate is above 0.85:
            respond with {"workspace": workspace.name, "level": "warning", "burn_rate": burn_rate, "projected_spend": projected_spend}
        otherwise:
            respond with {"workspace": workspace.name, "level": "healthy", "burn_rate": burn_rate, "projected_spend": projected_spend}

to prioritize_rightsizing with resources:
    purpose: "Find resources with sustained idle capacity"

    set candidates to []

    repeat for each r in resources:
        if r.cpu_idle_percent is above 70 and r.memory_idle_percent is above 60:
            append {"name": r.name, "monthly_cost": r.monthly_cost, "suggestion": "downsize"} to candidates

    respond with candidates

to summarize_portfolio with workspaces:
    purpose: "Build critical/warning counts for many workspaces"

    set critical_count to 0
    set warning_count to 0

    repeat for each w in workspaces:
        run evaluate_workspace with w
        if result.level is equal "critical":
            set critical_count to critical_count + 1
        otherwise:
            if result.level is equal "warning":
                set warning_count to warning_count + 1

    respond with {
        "total_workspaces": len(workspaces),
        "critical": critical_count,
        "warning": warning_count
    }

api:
    POST /workspace/evaluate runs evaluate_workspace
    POST /resources/rightsizing runs prioritize_rightsizing
    POST /portfolio/summary runs summarize_portfolio
