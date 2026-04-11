<|begin|>
// Description: an AI chatbot with conversation memory and context
// Type: full app
service "chatbot"
version "1.0.0"

to chat with message and session_id:
    set mem to memory_new(20)
    set history to memory_load(session_id)
    if history is not empty:
        set mem to history
    memory_add(mem, "user", message)
    set context to memory_summary(mem)
    ask AI to "You are a helpful assistant. History: {{context}}\nUser: {{message}}" save as reply
    memory_add(mem, "assistant", reply)
    memory_save(session_id, mem)
    respond with {"reply": reply, "session": session_id}

to clear session with session_id:
    memory_clear(session_id)
    respond with {"cleared": true}

api:
    POST /chat runs chat
    DELETE /sessions/:id runs clear_session
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "chatbot"}

// === NC_FILE_SEPARATOR ===

page "Chatbot"
title "Chatbot | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "chat":
    heading "AI Assistant"
    text "Ask me anything. I remember our conversation."

section "messages":
    list as messages:
        card:
            text messages.role style badge
            text messages.content

section "input":
    form action "/chat" method POST:
        input "message" placeholder "Type your message..."
        button "Send" type submit style primary
    button "Clear Chat" action "clearSession" style secondary

// === NC_AGENT_SEPARATOR ===

// Chatbot AI Agent
service "chatbot-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to smart reply with message and persona:
    ask AI to "Reply as {{persona}}. Message: {{message}}" save as reply
    respond with {"reply": reply}

to handle with prompt:
    purpose: "Handle user request for chatbot"
    ask AI to "You are a helpful chatbot assistant. {{prompt}}" save as response
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
