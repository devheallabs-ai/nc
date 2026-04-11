service "moderator"
version "1.0.0"

to moderate with content:
    ask AI to "Analyze this content for toxicity. Return JSON with 'safe' (true/false) and 'reason' (brief explanation)." using content save as result
    respond with result

api:
    POST /moderate runs moderate
