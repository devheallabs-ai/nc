// Data Enrichment — Enrich records with AI-generated insights
//
// Takes raw data records and adds AI-generated fields.
// Deploy: NC_AI_KEY=sk-xxx nc serve templates/data-enrichment.nc

service "data-enrichment"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"
    ai_key is "env:NC_AI_KEY"
    port: 8000

to enrich with record:
    purpose: "Add AI-generated insights to a data record"
    ask AI to "Analyze this record and return JSON with: summary (one line), sentiment (positive/neutral/negative), tags (up to 3). Record: {{record}}" save as insights
    respond with {"original": record, "enriched": insights}

to summarize with text:
    purpose: "Summarize text concisely"
    ask AI to "Summarize this in 2 sentences or fewer: {{text}}" save as summary
    respond with {"summary": summary}

to extract_entities with text:
    purpose: "Extract named entities from text"
    ask AI to "Extract named entities from this text. Return JSON: {\"people\": [], \"companies\": [], \"locations\": [], \"dates\": []}. Text: {{text}}" save as entities
    respond with entities

to health:
    respond with {"status": "healthy", "service": "data-enrichment"}

middleware:
    auth: "bearer"
    rate_limit: 100
    cors: true
    log_requests: true

api:
    POST /enrich runs enrich
    POST /summarize runs summarize
    POST /entities runs extract_entities
    GET /health runs health
