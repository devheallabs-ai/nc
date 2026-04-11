service "ai-chatbot"
version "1.0.0"

configure:
    ai_model is "default"
    ai_system_prompt is "You are a helpful, friendly assistant. Be concise."

to chat with message, session_id:
    purpose: "Multi-turn chatbot with conversation memory"

    set mem to memory_new(20)
    memory_add(mem, "user", message)

    set history to memory_summary(mem)
    ask AI to "Conversation history: {{history}} Respond to the latest message." save as reply

    memory_add(mem, "assistant", reply)
    respond with {"reply": reply, "session": session_id}

to simple_chat with message:
    purpose: "Single-turn chat — no memory"
    ask AI to "{{message}}" save as reply
    respond with reply

api:
    POST /chat runs chat
    POST /ask runs simple_chat
