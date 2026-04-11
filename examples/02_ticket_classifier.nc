service "ticket-classifier"
version "1.0.0"

to classify with ticket:
    ask AI to "Classify this support ticket into one of: billing, technical, account, general. Return only the category name." using ticket save as category
    respond with {"category": category, "ticket": ticket}

api:
    POST /classify runs classify
