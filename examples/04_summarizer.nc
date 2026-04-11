service "summarizer"
version "1.0.0"

to summarize with document:
    ask AI to "Summarize this document in 3 bullet points. Be concise." using document save as summary
    respond with {"summary": summary}

to summarize_with_length with request:
    set text to request.text
    set max_words to request.max_words
    ask AI to "Summarize this in under {{max_words}} words." using text save as summary
    respond with {"summary": summary, "max_words": max_words}

api:
    POST /summarize runs summarize
    POST /summarize/custom runs summarize_with_length
