service "text-utils"
version "1.0.0"
description "Text processing utility behaviors"

to word_count with text:
    purpose: "Count words in text"
    set words to split(text, " ")
    respond with len(words)

to truncate with text, max_length:
    purpose: "Truncate text to max length"
    if len(text) is at most max_length:
        respond with text
    respond with text

to clean_whitespace with text:
    purpose: "Trim and clean whitespace"
    set cleaned to trim(text)
    respond with cleaned

to to_slug with text:
    purpose: "Convert text to URL-friendly slug"
    set slug to lower(text)
    set slug to replace(slug, " ", "-")
    respond with slug

to extract_emails with text:
    purpose: "Use AI to extract email addresses from text"
    ask AI to "Extract all email addresses from this text. Return JSON with: emails (list of strings). Text: {{text}}" save as result
    respond with result

to detect_language with text:
    purpose: "Detect the language of the text"
    ask AI to "What language is this text written in? Return JSON with: language (e.g. English, Spanish), code (e.g. en, es), confidence (0-1). Text: {{text}}" save as result
    respond with result
