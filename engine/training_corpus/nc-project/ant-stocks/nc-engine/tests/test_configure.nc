service "config-test"
version "1.0.0"

configure:
    ai_url is env:MY_CUSTOM_AI_URL
    ai_key is env:MY_CUSTOM_API_KEY
    ai_model is "gpt-4o"
    max_retries: 3

to classify with ticket:
    ask AI to "classify this ticket" using ticket
    save as category
    respond with category

to fetch_data:
    gather result from "https://api.example.com/data":
        method: "POST"
    respond with result

to health_check:
    respond with {"status": "healthy"}

api:
    POST /classify runs classify
    POST /data runs fetch_data
    GET /health runs health_check
