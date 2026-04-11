// External API integration examples for NC
// Demonstrates calling AI endpoints and REST APIs

service "api-integration-demo"
version "1.0.0"

configure:
    ai_url is "env:NC_AI_URL"
    ai_model is "env:NC_AI_MODEL"
    ai_key is env("NC_AI_KEY")
    port: 8080

// --- AI Endpoints ---

to summarize with text:
    ask AI to "Summarize the following text concisely: {{text}}" save as summary
    respond with summary

to classify with input:
    ask AI to "Classify this input into one of: question, statement, command. Return JSON with category and confidence: {{input}}" save as result
    respond with result

to translate with text and target_lang:
    ask AI to "Translate the following text to {{target_lang}}: {{text}}" save as translation
    respond with translation

// --- External REST API ---

to geocode with address:
    set api_key to env("GEOCODE_API_KEY")
    gather location from "https://api.example.com/geocode?address={{address}}&key={{api_key}}"
    respond with location

// --- Search API ---

to search with query:
    set api_key to env("SEARCH_API_KEY")
    gather results from "https://api.example.com/search?q={{query}}&key={{api_key}}"
    respond with results

api:
    POST /summarize    runs summarize
    POST /classify     runs classify
    POST /translate    runs translate
    GET  /geocode      runs geocode
    GET  /search       runs search
