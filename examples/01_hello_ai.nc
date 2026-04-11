service "hello-ai"
version "1.0.0"

to greet with name:
    ask AI to "Write a warm, one-sentence greeting for {{name}}" save as greeting
    respond with {"greeting": greeting}

api:
    POST /hello runs greet
