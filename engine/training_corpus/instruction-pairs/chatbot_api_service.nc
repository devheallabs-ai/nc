<|begin|>
// Description: an AI chatbot with conversation memory and context (API only, no frontend)
// Type: service
service "chatbot-api"
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
    respond with {"status": "ok"}
<|end|>
