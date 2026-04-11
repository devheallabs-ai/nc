// Functional tests: data pipeline and state transitions
// Focus: normalization, aggregation, scoring, and retry routing

service "test-functional-data-pipelines"
version "1.0.0"

to normalize_event with evt:
    set normalized to {
        "tenant": evt.tenant,
        "source": evt.source,
        "amount": evt.amount,
        "valid": true
    }

    if evt.amount is below 0:
        set normalized.valid to false

    respond with normalized

to test_normalize_event_negative:
    set evt to {"tenant": "acme", "source": "billing", "amount": -20}
    run normalize_event with evt
    respond with result.valid

to aggregate_revenue with orders:
    set totals to {"web": 0, "sales": 0, "partner": 0}

    repeat for each order in orders:
        if order.channel is equal "web":
            set totals.web to totals.web + order.amount
        otherwise:
            if order.channel is equal "sales":
                set totals.sales to totals.sales + order.amount
            otherwise:
                set totals.partner to totals.partner + order.amount

    respond with totals

to test_aggregate_revenue:
    set orders to [
        {"channel": "web", "amount": 120},
        {"channel": "web", "amount": 80},
        {"channel": "sales", "amount": 300},
        {"channel": "partner", "amount": 50}
    ]
    run aggregate_revenue with orders
    respond with result

to compute_risk_score with txn:
    set score to 0

    if txn.amount is above 10000:
        set score to score + 50

    if txn.country is not equal to txn.home_country:
        set score to score + 30

    if txn.device_age_days is below 3:
        set score to score + 20

    respond with score

to test_compute_risk_score:
    set txn to {"amount": 18000, "country": "US", "home_country": "IN", "device_age_days": 1}
    run compute_risk_score with txn
    respond with result

to next_retry_state with current, attempts:
    match current:
        when "new":
            if attempts is above 0:
                respond with "retrying"
            otherwise:
                respond with "new"
        when "retrying":
            if attempts is above 2:
                respond with "failed"
            otherwise:
                respond with "retrying"
        when "done":
            respond with "done"
        otherwise:
            respond with "unknown"

to test_next_retry_state:
    run next_retry_state with "retrying", 3
    respond with result

to test_batch_cursor_progress:
    set cursor to 0
    set page_size to 4
    set total_records to 17
    set pages to 0

    while cursor is below total_records:
        set pages to pages + 1
        set cursor to cursor + page_size

    respond with pages
