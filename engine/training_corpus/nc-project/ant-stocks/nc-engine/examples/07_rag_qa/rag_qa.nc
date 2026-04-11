service "document-qa"
version "1.0.0"

configure:
    ai_url is "https://your-llm-endpoint.example.com/v1/chat/completions"
    ai_key is "env:LITELLM_API_KEY"
    ai_model is "openai/gpt-4o-mini"
    ai_format is "openai"

to answer_question:
    purpose: "Answer questions about documents using RAG"

    set raw_text to read_file("llama_index_data/paul_graham_essay.txt")

    set chunks to chunk(raw_text, 500)

    ask AI to "Based on the following document, answer this question: What did the author do growing up? Document: {{raw_text}}" save as answer

    show answer

    respond with answer
