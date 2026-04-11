<|begin|>
// Description: a customer support ticket system with AI classification and routing
// Type: full app
service "support"
version "1.0.0"

to create ticket with data:
    set data.id to generate_id()
    set data.status to "open"
    set data.priority to "medium"
    set data.created_at to now()
    set tickets to load("tickets.json")
    add data to tickets
    save tickets to "tickets.json"
    respond with data

to list tickets:
    set tickets to load("tickets.json")
    respond with tickets

to get ticket with id:
    set ticket to load("tickets.json", id)
    respond with ticket

to update ticket with id and data:
    set tickets to load("tickets.json")
    set index to find_index(tickets, id)
    merge data into tickets[index]
    set tickets[index].updated_at to now()
    save tickets to "tickets.json"
    respond with tickets[index]

to close ticket with id:
    set tickets to load("tickets.json")
    set index to find_index(tickets, id)
    set tickets[index].status to "closed"
    set tickets[index].closed_at to now()
    save tickets to "tickets.json"
    respond with tickets[index]

api:
    POST /tickets runs create_ticket
    GET /tickets runs list_tickets
    GET /tickets/:id runs get_ticket
    PUT /tickets/:id runs update_ticket
    POST /tickets/:id/close runs close_ticket
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "support"}

// === NC_FILE_SEPARATOR ===

page "Support"
title "Support | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "tickets":
    heading "Support Tickets"
    button "New Ticket" action "openForm" style primary

    list from "/tickets" as tickets:
        card:
            badge tickets.priority
            heading tickets.title
            text tickets.description
            badge tickets.status
            button "View" action "viewTicket" style secondary
            button "Close" action "closeTicket" style danger

section "new-ticket":
    form action "/tickets" method POST:
        input "title" placeholder "Issue title"
        input "description" type textarea placeholder "Describe your issue"
        dropdown "priority" options "low,medium,high,critical"
        button "Submit Ticket" type submit style primary

// === NC_AGENT_SEPARATOR ===

// Support AI Agent
service "support-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to classify ticket with ticket:
    ask AI to "Classify this support ticket into: bug, feature, question, billing, security. Title: {{ticket.title}} Description: {{ticket.description}}" save as category
    respond with {"category": category}

to suggest response with ticket:
    ask AI to "Suggest a helpful response for this support ticket: {{ticket.title}} — {{ticket.description}}" save as suggestion
    respond with {"suggestion": suggestion}

to set priority with ticket:
    ask AI to "Rate priority as low, medium, high, or critical. Ticket: {{ticket.title}} — {{ticket.description}}" save as priority
    respond with {"priority": priority}

to handle with prompt:
    purpose: "Handle user request for support"
    ask AI to "You are a helpful support assistant. {{prompt}}" save as response
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
