service "multi-provider-demo"
version "1.0.0"

# This exact same code works with ANY AI provider.
# Switch providers by changing NC_AI_PROVIDER environment variable:
#
#   NC_AI_PROVIDER=openai nc serve 05_multi_provider.nc
#   NC_AI_PROVIDER=anthropic nc serve 05_multi_provider.nc
#   NC_AI_PROVIDER=ollama nc serve 05_multi_provider.nc
#
# Zero code changes. Same bytecode. Different provider.

to analyze with text:
    ask AI to "Analyze the sentiment of this text. Return: positive, negative, or neutral." using text save as sentiment
    ask AI to "Extract the key topics from this text as a comma-separated list." using text save as topics
    respond with {"sentiment": sentiment, "topics": topics}

api:
    POST /analyze runs analyze
