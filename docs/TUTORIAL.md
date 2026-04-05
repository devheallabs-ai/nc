
  


# Your First NC Service in 5 Minutes

NC is a plain English programming language for building AI-powered services. No Python. No imports. No boilerplate.

## Step 1: Install NC (30 seconds)

```bash
git clone https://github.com/devheallabs-ai/nc-lang.git
cd nc/nc
make
sudo cp build/nc /usr/local/bin/
```

Verify: `nc version`

## Step 2: Create Your First Service (60 seconds)

Create a file called `my_service.nc`:

```nc
service "my-first-service"
version "1.0.0"

configure:
    ai_model is "nova"

to greet with name:
    respond with "Hello, " + name + "! Welcome to NC."

to ask_question with question:
    ask AI to "{{question}}" save as answer
    respond with answer

api:
    POST /greet runs greet
    POST /ask runs ask_question
```

## Step 3: Run It (10 seconds)

**Test a behavior directly:**
```bash
nc run my_service.nc -b greet
```

**Start as an API server:**
```bash
NC_AI_KEY="<YOUR_API_KEY>" nc serve my_service.nc
```

**Call it:**
```bash
curl -X POST http://localhost:8000/greet \
  -d '{"name": "World"}'
# â†’ {"status":"ok","data":"Hello, World! Welcome to NC."}

curl -X POST http://localhost:8000/ask \
  -d '{"question": "What is the capital of France?"}'
# â†’ "The capital of France is Paris."
```

## Step 4: Add AI Intelligence (2 minutes)

Create `classifier.nc`:

```nc
service "email-classifier"
version "1.0.0"

configure:
    ai_model is "nova"

to classify with email_text:
    ask AI to "Classify this email into one of: support, sales, spam, other. Return JSON with category and confidence. Email: {{email_text}}" save as result

    if result.category is equal "spam":
        log "Spam detected, ignoring"
        respond with {"status": "rejected", "reason": "spam"}

    respond with result

api:
    POST /classify runs classify
```

```bash
NC_AI_KEY="<YOUR_API_KEY>" nc serve classifier.nc
curl -X POST http://localhost:8000/classify \
  -d '{"email_text": "Buy cheap watches now!!!"}'
# â†’ {"category": "spam", "confidence": 0.95}
```

## Step 5: Document Q&A with RAG (2 minutes)

Create `qa.nc`:

```nc
service "document-qa"
version "1.0.0"

configure:
    ai_model is "nova"

to answer with question, file_path:
    set document to read_file(file_path)
    ask AI to "Based on this document, answer: {{question}}. Document: {{document}}" save as answer
    respond with answer

api:
    POST /ask runs answer
```

```bash
echo "NC is a plain English programming language. It compiles to bytecode." > info.txt
NC_AI_KEY="<YOUR_API_KEY>" nc serve qa.nc
curl -X POST http://localhost:8000/ask \
  -d '{"question": "What is NC?", "file_path": "info.txt"}'
# â†’ "NC is a plain English programming language that compiles to bytecode."
```

## What's Next?

- **REPL**: `nc repl` â€” interactive NC shell
- **Debug**: `nc debug service.nc` â€” step through code
- **Packages**: `nc pkg install github.com/user/package` â€” install community packages
- **Examples**: See the `examples/` directory for more

## Python vs NC Cheat Sheet

| Task | Python | NC |
|------|--------|-----|
| Call AI | `openai.ChatCompletion.create(...)` (15 lines) | `ask AI to "..." save as result` |
| HTTP server | `from flask import Flask` + routes (25 lines) | `api: POST /path runs behavior` |
| Read file | `open('f').read()` (3 lines) | `read_file("f")` |
| JSON parse | `import json; json.loads(s)` | `json_decode(s)` |
| Environment var | `import os; os.getenv('X')` | `env:X` in configure |

