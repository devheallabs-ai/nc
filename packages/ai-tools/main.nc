service "ai-tools"
version "1.0.0"
description "Reusable AI behaviors — classify, summarize, translate, extract"

to classify_text with text, categories:
    purpose: "Classify text into one of the given categories"
    ask AI to "Classify this text into exactly one of these categories: {{categories}}. Return JSON with: category, confidence (0-1). Text: {{text}}" save as result
    respond with result

to summarize_text with text, max_length:
    purpose: "Summarize text to a given length"
    ask AI to "Summarize this text in {{max_length}} words or fewer: {{text}}" save as summary
    respond with summary

to translate_text with text, from_lang, to_lang:
    purpose: "Translate text between languages"
    ask AI to "Translate from {{from_lang}} to {{to_lang}}. Return only the translation: {{text}}" save as translation
    respond with translation

to extract_json with text, fields:
    purpose: "Extract structured data from unstructured text"
    ask AI to "Extract these fields from the text: {{fields}}. Return valid JSON only. Text: {{text}}" save as extracted
    set validation to validate(extracted, fields)
    respond with extracted

to ask_with_retry with question, context:
    purpose: "Ask AI with automatic retry and fallback"
    set models to ["openai/gpt-4o-mini", "openai/gpt-4o", "anthropic/claude-3-haiku-20240307"]
    set answer to ai_with_fallback(question, context, models)
    respond with answer
