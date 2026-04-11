// ═══════════════════════════════════════════════════════════
//  Customer Support AI
//
//  Replaces: 200+ lines of Python with intent detection + routing + response gen
//
//  Classifies tickets, generates responses, escalates to humans.
//
//  curl -X POST http://localhost:8000/ticket \
//    -d '{"message": "My order hasnt arrived", "customer_id": "C123"}'
// ═══════════════════════════════════════════════════════════

service "customer-support"
version "1.0.0"

configure:
    ai_model is "default"
    ai_system_prompt is "You are a professional customer support agent. Be empathetic, helpful, and solution-oriented."

to handle_ticket with message, customer_id:
    purpose: "Classify and respond to a customer support ticket"

    ask AI to "Classify this customer message. Return JSON with: intent (billing/shipping/product/technical/complaint/other), urgency (high/medium/low), sentiment (angry/frustrated/neutral/happy), needs_human (true/false), suggested_response (string).\n\nCustomer {{customer_id}}: {{message}}" save as analysis

    set validation to validate(analysis, ["intent", "urgency", "suggested_response"])

    if validation.valid is equal no:
        ask AI to "Respond helpfully to this customer: {{message}}" save as fallback
        respond with {"response": fallback, "status": "auto"}

    if analysis.needs_human:
        log "ESCALATION: Ticket from {{customer_id}} needs human review — {{analysis.intent}}"
        respond with {"response": analysis.suggested_response, "status": "escalated", "intent": analysis.intent, "urgency": analysis.urgency}

    if analysis.urgency is equal "high":
        log "HIGH URGENCY: {{customer_id}} — {{analysis.intent}}"

    respond with {"response": analysis.suggested_response, "status": "auto_resolved", "intent": analysis.intent, "urgency": analysis.urgency, "sentiment": analysis.sentiment}

to generate_faq with topics:
    purpose: "Auto-generate FAQ from a list of topics"

    set faqs to []
    repeat for each topic in topics:
        ask AI to "Write a helpful FAQ entry for this topic. Return JSON with: question (string), answer (string). Topic: {{topic}}" save as faq
        append faq to faqs

    respond with faqs

to analyze_feedback with feedback_list:
    purpose: "Analyze customer feedback and identify trends"

    ask AI to "Analyze this customer feedback. Return JSON with: overall_sentiment (positive/neutral/negative), top_issues (list of strings), top_praises (list of strings), suggestions (list of strings), satisfaction_score (1-10).\n\nFeedback:\n{{feedback_list}}" save as analysis

    respond with analysis

api:
    POST /ticket runs handle_ticket
    POST /faq runs generate_faq
    POST /feedback runs analyze_feedback
