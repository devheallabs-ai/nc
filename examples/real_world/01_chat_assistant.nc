// ═══════════════════════════════════════════════════════════
//  ChatGPT-style Assistant
//
//  Replaces: 50+ lines of Python with LLM SDK + memory management
//
//  curl -X POST http://localhost:8000/chat \
//    -d '{"message": "What is quantum computing?", "session": "user123"}'
// ═══════════════════════════════════════════════════════════

service "chat-assistant"
version "1.0.0"

configure:
    ai_model is "default"
    ai_system_prompt is "You are a helpful, knowledgeable assistant. Be concise but thorough. Use examples when helpful."

to chat with message, session:
    purpose: "Multi-turn conversation with memory"

    set memory to memory_new(30)
    memory_add(memory, "user", message)

    set history to memory_summary(memory)
    ask AI to "Conversation so far:\n{{history}}\nRespond to the latest message." save as reply

    memory_add(memory, "assistant", reply)

    respond with {"reply": reply, "session": session}

to chat_with_context with message, context:
    purpose: "Chat with additional context (documents, data)"

    ask AI to "Context: {{context}}\n\nUser question: {{message}}\n\nAnswer based on the context provided." save as reply

    respond with reply

to stream_chat with message:
    purpose: "Single message, no memory — fast response"

    ask AI to "{{message}}" save as reply
    respond with reply

api:
    POST /chat runs chat
    POST /chat/context runs chat_with_context
    POST /ask runs stream_chat
