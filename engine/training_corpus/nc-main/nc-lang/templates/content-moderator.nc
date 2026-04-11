// Content Moderator — AI-powered content safety screening
//
// Checks user-submitted content for policy violations.
// Deploy: NC_AI_KEY=sk-xxx nc serve templates/content-moderator.nc

service "content-moderator"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"
    ai_key is "env:NC_AI_KEY"
    port: 8000

to moderate with content:
    purpose: "Check content for policy violations"
    ask AI to "Review this content for safety. Respond with JSON: {\"safe\": true/false, \"reason\": \"explanation\", \"category\": \"none/spam/hate/violence/nsfw\"}. Content: {{content}}" save as result
    respond with result

to health:
    respond with {"status": "healthy", "service": "content-moderator"}

middleware:
    auth: "bearer"
    rate_limit: 50
    cors: true
    log_requests: true

api:
    POST /moderate runs moderate
    GET /health runs health
