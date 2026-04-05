service "langchain-test"
version "1.0.0"

configure:
    ai_url is "https://your-llm-endpoint.example.com/v1/chat/completions"
    ai_key is "env:LITELLM_API_KEY"
    ai_model is "default"
    ai_format is "openai"
    

to test_litellm:
    ask AI to "What is LiteLLM and why is it useful? Answer in 1 sentence." save as response

    show response

    respond with response
