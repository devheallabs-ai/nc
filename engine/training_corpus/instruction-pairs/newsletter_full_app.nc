<|begin|>
// Description: a newsletter and email marketing service
// Type: full app
service "newsletter"
version "1.0.0"

to subscribe with email and name:
    set subscribers to load("subscribers.json")
    set existing to find_by(subscribers, "email", email)
    if existing is not empty:
        respond with error "Already subscribed" status 409
    set subscriber to {"id": generate_id(), "email": email, "name": name, "subscribed_at": now(), "active": true}
    add subscriber to subscribers
    save subscribers to "subscribers.json"
    respond with subscriber

to unsubscribe with email:
    set subscribers to load("subscribers.json")
    set index to find_index_by(subscribers, "email", email)
    set subscribers[index].active to false
    save subscribers to "subscribers.json"
    respond with {"unsubscribed": true}

to send campaign with subject and content:
    set subscribers to load("subscribers.json")
    set active to filter(subscribers, "active", true)
    repeat for each sub in active:
        send_email(sub.email, subject, content)
    respond with {"sent": length(active)}

to list subscribers:
    set subscribers to load("subscribers.json")
    respond with subscribers

api:
    POST /subscribe runs subscribe
    DELETE /unsubscribe runs unsubscribe
    POST /campaign runs send_campaign
    GET /subscribers runs list_subscribers
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "newsletter"}

// === NC_FILE_SEPARATOR ===

page "Newsletter"
title "Newsletter | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "newsletter":
    heading "Newsletter"
    text "Stay updated with our latest news"

    form action "/subscribe" method POST:
        input "name" placeholder "Your name"
        input "email" type email placeholder "your@email.com"
        button "Subscribe" type submit style primary

section "campaigns":
    heading "Send Campaign"
    form action "/campaign" method POST:
        input "subject" placeholder "Email subject"
        input "content" type textarea placeholder "Email content"
        button "Send to All" type submit style primary
        button "AI Generate" action "aiGenerate" style ai

// === NC_AGENT_SEPARATOR ===

// Newsletter AI Agent
service "newsletter-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to generate email with topic:
    ask AI to "Write an engaging newsletter email about: {{topic}}. Include subject line, intro, body, and CTA." save as email
    respond with {"email": email}

to personalize email with subscriber and template:
    ask AI to "Personalize this email template for {{subscriber.name}}: {{template}}" save as personalized
    respond with {"email": personalized}

to handle with prompt:
    purpose: "Handle user request for newsletter"
    ask AI to "You are a helpful newsletter assistant. {{prompt}}" save as response
    respond with {"reply": response}

to classify with input:
    ask AI to "Classify as: create, read, update, delete, help. Input: {{input}}" save as intent
    respond with {"intent": intent}

api:
    POST /agent          runs handle
    POST /agent/classify  runs classify
    GET  /agent/health    runs health_check

to health check:
    respond with {"status": "ok", "ai": "local"}
<|end|>
