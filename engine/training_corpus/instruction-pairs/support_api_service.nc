<|begin|>
// Description: a customer support ticket system with AI classification and routing (API only, no frontend)
// Type: service
service "support-api"
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
    respond with {"status": "ok"}
<|end|>
