<|begin|>
// Description: a booking and scheduling system for appointments
// Type: full app
service "booking"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to list slots:
    set slots to load("slots.json")
    set available to filter(slots, "available", true)
    respond with available

to book slot with slot_id and user_id and data:
    set slots to load("slots.json")
    set index to find_index(slots, slot_id)
    if slots[index].available is false:
        respond with error "Slot not available" status 409
    set booking to {"id": generate_id(), "slot_id": slot_id, "user_id": user_id, "status": "confirmed", "created_at": now()}
    merge data into booking
    set slots[index].available to false
    save slots to "slots.json"
    set bookings to load("bookings.json")
    add booking to bookings
    save bookings to "bookings.json"
    respond with booking

to cancel booking with id:
    set bookings to load("bookings.json")
    set index to find_index(bookings, id)
    set slot_id to bookings[index].slot_id
    set bookings[index].status to "cancelled"
    save bookings to "bookings.json"
    set slots to load("slots.json")
    set slot_index to find_index(slots, slot_id)
    set slots[slot_index].available to true
    save slots to "slots.json"
    respond with {"cancelled": true}

to list my bookings with user_id:
    set bookings to load("bookings.json")
    set my_bookings to filter(bookings, "user_id", user_id)
    respond with my_bookings

api:
    GET /slots runs list_slots
    POST /bookings runs book_slot
    DELETE /bookings/:id runs cancel_booking
    GET /bookings/my runs list_my_bookings
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "booking"}

// === NC_FILE_SEPARATOR ===

page "Booking"
title "Booking | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "booking":
    heading "Book an Appointment"
    text "Choose a time that works for you"

section "slots":
    grid from "/slots" as slots columns 4:
        card:
            heading slots.date
            text slots.time
            text slots.duration
            button "Book" action "bookSlot" style primary

section "my-bookings":
    heading "My Bookings"
    list from "/bookings/my" as bookings:
        card:
            heading bookings.date
            text bookings.time
            badge bookings.status
            button "Cancel" action "cancelBooking" style danger

// === NC_AGENT_SEPARATOR ===

// Booking AI Agent
service "booking-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to suggest best time with preferences:
    ask AI to "Suggest the best appointment time based on preferences: {{preferences}}" save as suggestion
    respond with {"suggestion": suggestion}

to send reminder with booking:
    ask AI to "Write a friendly appointment reminder for: {{booking.date}} at {{booking.time}}" save as reminder
    respond with {"reminder": reminder}

to handle with prompt:
    purpose: "Handle user request for booking"
    ask AI to "You are a helpful booking assistant. {{prompt}}" save as response
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
