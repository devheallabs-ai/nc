service "poem-generator"
version "1.0.0"

configure:
    ai_url is "https://your-llm-endpoint.example.com/v1/chat/completions"
    ai_key is "env:LITELLM_API_KEY"
    ai_model is "default"
    ai_format is "openai"

to write_poem:
    ask AI to "this is a test request, write a short poem" save as response

    show response

    respond with response
