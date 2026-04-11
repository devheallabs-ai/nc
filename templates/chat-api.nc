// Chat API — Conversational AI chatbot endpoint
//
// A simple AI chatbot as an HTTP API with system prompt.
// Deploy: NC_AI_KEY=sk-xxx nc serve templates/chat-api.nc

service "chat-api"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"
    ai_key is "env:NC_AI_KEY"
    ai_system_prompt is "You are a helpful, concise assistant. Keep responses under 3 sentences unless asked for more detail."
    port: 8000

to chat with message:
    purpose: "Respond to a user message"
    ask AI to "{{message}}" save as reply
    respond with {"reply": reply}

to health:
    respond with {"status": "healthy", "service": "chat-api"}

middleware:
    rate_limit: 60
    cors: true
    log_requests: true

api:
    POST /chat runs chat
    GET /health runs health
