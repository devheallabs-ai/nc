service "email-classifier"
version "1.0.0"

configure:
    ai_model is "default"

to classify with email_text, sender:
    purpose: "Classify incoming emails and route to the right team"

    ask AI to "Classify this email. Return JSON with: category (support/sales/billing/spam/other), priority (high/medium/low), sentiment (positive/neutral/negative), summary (one sentence). Email from {{sender}}: {{email_text}}" save as result

    set validation to validate(result, ["category", "priority", "sentiment"])

    if validation.valid is equal no:
        ask AI to "Return ONLY valid JSON with fields: category, priority, sentiment, summary. Classify: {{email_text}}" save as result

    if result.category is equal "spam":
        log "Spam from {{sender}} — rejected"
        respond with {"status": "rejected", "reason": "spam"}

    if result.priority is equal "high":
        log "HIGH PRIORITY from {{sender}}: {{result.summary}}"

    respond with result

api:
    POST /classify runs classify
