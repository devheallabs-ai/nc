service "api-gateway"
version "1.0.0"

configure:
    ai_model is "default"
    rate_limit is 100

middleware:
    rate_limit
    auth

to health:
    respond with {"status": "healthy", "version": "1.0.0"}

to translate with text, target_language:
    purpose: "Translate text to any language"
    ask AI to "Translate the following text to {{target_language}}. Return only the translation, no explanation. Text: {{text}}" save as translation
    respond with {"original": text, "translated": translation, "language": target_language}

to summarize with text, max_sentences:
    purpose: "Summarize text to N sentences"
    ask AI to "Summarize the following text in {{max_sentences}} sentences or fewer: {{text}}" save as summary
    respond with {"summary": summary, "original_length": len(text)}

to extract_entities with text:
    purpose: "Extract named entities from text"
    ask AI to "Extract all named entities from this text. Return JSON with: people (list), organizations (list), locations (list), dates (list). Text: {{text}}" save as entities
    respond with entities

to sentiment with text:
    purpose: "Analyze sentiment"
    ask AI to "Analyze the sentiment of this text. Return JSON with: sentiment (positive/negative/neutral), score (0-1), explanation (one sentence). Text: {{text}}" save as result
    respond with result

api:
    GET /health runs health
    POST /translate runs translate
    POST /summarize runs summarize
    POST /entities runs extract_entities
    POST /sentiment runs sentiment
