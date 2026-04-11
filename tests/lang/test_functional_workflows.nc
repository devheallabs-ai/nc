// Functional tests: business workflow logic
// Focus: deterministic pipelines, routing, batching, and map/list mutations

service "test-functional-workflows"
version "1.0.0"

to calculate_subtotal with lines:
    set subtotal to 0
    repeat for each line in lines:
        set subtotal to subtotal + line.price * line.qty
    respond with subtotal

to test_invoice_total:
    set lines to [
        {"sku": "A-100", "price": 40, "qty": 2},
        {"sku": "B-220", "price": 15, "qty": 3},
        {"sku": "C-410", "price": 25, "qty": 1}
    ]
    run calculate_subtotal with lines
    set subtotal to result
    set tax to subtotal * 0.1
    set total to subtotal + tax
    respond with total

to classify_sla_action with priority, minutes_open:
    if priority is equal "critical":
        if minutes_open is above 10:
            respond with "page-now"
        otherwise:
            respond with "watch"
    otherwise:
        if minutes_open is above 120:
            respond with "escalate"
        otherwise:
            respond with "queue"

to test_sla_action_for_critical:
    run classify_sla_action with "critical", 22
    respond with result

to test_sla_action_for_normal:
    run classify_sla_action with "normal", 60
    respond with result

to test_batch_window_count:
    set jobs to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    set i to 0
    set batches to 0
    while i is below len(jobs):
        set batches to batches + 1
        set i to i + 3
    respond with batches

to test_skip_and_stop_window:
    set total to 0
    repeat for each n in [1, 2, 3, 4, 5, 6, 7, 8]:
        if n is below 3:
            skip
        if n is above 6:
            stop
        set total to total + n
    respond with total

to test_status_counter_map:
    set counters to {"ok": 0, "error": 0}
    set statuses to ["ok", "ok", "error", "ok", "error"]
    repeat for each s in statuses:
        if s is equal "ok":
            set counters.ok to counters.ok + 1
        otherwise:
            set counters.error to counters.error + 1
    respond with counters

to test_approval_routing_with_match:
    set decision to "manual_review"
    match decision:
        when "auto_approve":
            respond with "ship"
        when "manual_review":
            respond with "review"
        otherwise:
            respond with "hold"

to test_nested_template_audit:
    set ctx to {"tenant": "acme", "env": "prod", "service": "api-gateway"}
    set line to "[{{ctx.tenant}}|{{ctx.env}}] deploy {{ctx.service}}"
    respond with line

to test_try_fallback_path:
    try:
        set x to 10
        set y to 2
        set result to x / y
        respond with result
    on error:
        respond with "fallback"
