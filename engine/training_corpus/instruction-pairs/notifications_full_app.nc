<|begin|>
// Description: a notification service with email, push, and in-app notifications
// Type: full app
service "notifications"
version "1.0.0"

to send notification with user_id and type and message:
    set notif to {"id": generate_id(), "user_id": user_id, "type": type, "message": message, "read": false, "created_at": now()}
    set notifications to load("notifications.json")
    add notif to notifications
    save notifications to "notifications.json"
    if type is "email":
        send_email(user_id, message)
    respond with notif

to list notifications with user_id:
    set notifications to load("notifications.json")
    set my_notifs to filter(notifications, "user_id", user_id)
    respond with my_notifs

to mark read with id:
    set notifications to load("notifications.json")
    set index to find_index(notifications, id)
    set notifications[index].read to true
    set notifications[index].read_at to now()
    save notifications to "notifications.json"
    respond with notifications[index]

to mark all read with user_id:
    set notifications to load("notifications.json")
    repeat for each notif in notifications:
        if notif.user_id is user_id:
            set notif.read to true
    save notifications to "notifications.json"
    respond with {"marked": true}

api:
    POST /notifications runs send_notification
    GET /notifications runs list_notifications
    PUT /notifications/:id/read runs mark_read
    PUT /notifications/read-all runs mark_all_read
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "notifications"}

// === NC_FILE_SEPARATOR ===

page "Notifications"
title "Notifications | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "notifications":
    heading "Notifications"
    button "Mark All Read" action "markAllRead" style secondary

    list from "/notifications" as notifs:
        card style notifs.read ? "read" : "unread":
            icon notifs.type
            text notifs.message
            text notifs.created_at style meta
            button "Mark Read" action "markRead" style ghost

// === NC_AGENT_SEPARATOR ===

// Notifications AI Agent
service "notifications-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to compose notification with context:
    ask AI to "Write a clear, friendly notification message for: {{context}}" save as message
    respond with {"message": message}

to handle with prompt:
    purpose: "Handle user request for notifications"
    ask AI to "You are a helpful notifications assistant. {{prompt}}" save as response
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
