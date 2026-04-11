<|begin|>
// Description: a booking and scheduling system for appointments (API only, no frontend)
// Type: service
service "booking-api"
version "1.0.0"

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
    respond with {"status": "ok"}
<|end|>
