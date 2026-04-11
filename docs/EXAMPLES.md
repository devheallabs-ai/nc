
  


# NC Examples — Explained

## Example 1: Hello World (`examples/01_hello_world.nc`)

The simplest possible NC program:

```
service "hello-world"
version "1.0.0"

to greet with name:
    purpose: "Say hello to someone"
    respond with "Hello, " + name + "! Welcome to NC."

to health check:
    respond with "healthy"

api:
    GET /hello runs greet
    GET /health runs health_check
```

### How to run:
```bash
nc run examples/01_hello_world.nc
nc run examples/01_hello_world.nc -b greet
```

### What it shows:
- `service` and `version` declarations
- Behavior with parameters (`with name`)
- String concatenation (`+`)
- API route mapping

---

## Example 2: AI Ticket Classifier (`examples/02_ai_classifier.nc`)

Uses AI to classify support tickets:

```
to classify a ticket:
    ask AI to "classify this ticket" using ticket:
        confidence: 0.7
        save as: classification
    respond with classification
```

### What it shows:
- `ask AI to` — first-class AI operations
- `using` — pass data to the AI
- `save as` — store AI result in a variable
- `confidence` — minimum confidence threshold

---

## Example 3: Control Flow (`examples/03_control_flow.nc`)

Demonstrates all control structures:

```
// Conditionals
if score is above 90:
    respond with "excellent"
otherwise:
    respond with "ok"

// Pattern matching
match status:
    when "healthy":
        respond with "good"
    otherwise:
        respond with "unknown"

// Loops
repeat for each item in items:
    set total to total + item

// Error handling
try:
    gather data from risky_source
on error:
    log "failed"
```

---

## Example 4: Healing Service (`examples/existing_services/healing.nc`)

A real infrastructure healing service — diagnoses issues and auto-remediates:

```
to diagnose an issue:
    gather metrics from prometheus
    gather logs from loki
    gather events from kubernetes
    ask AI to "find root cause" using metrics, logs, events
    respond with diagnosis

to auto heal with issue and diagnosis:
    needs approval when blast_radius is equal to "namespace"
    apply action using kubernetes
    wait 30 seconds
    check if pod is healthy using prometheus
```

### What it shows:
- Multiple `gather` from different sources
- AI-powered root cause analysis
- Approval gates (`needs approval when`)
- `wait` for verification
- `check` for health validation

---

## Example 5: Kubernetes Service (`examples/existing_services/kubernetes.nc`)

Kubernetes management via plain English:

```
to diagnose pod with namespace and pod_name:
    gather status from kubernetes
    gather logs from kubernetes
    gather metrics from prometheus
    ask AI to "diagnose this pod issue" using status, logs, metrics
    respond with diagnosis

to scale deployment with namespace and deployment and replicas:
    needs approval when replicas is above 10
    apply scaling using kubernetes
    wait 15 seconds
    check if deployment is ready
```

---

## Example 6: AI Agent (`examples/existing_services/ai_agent.nc`)

Multi-agent AI system:

```
to chat with message:
    ask AI to "classify this query intent" using message
    gather knowledge from rag
    ask AI to "answer using context and knowledge" using message, intent, knowledge
    store message into "conversation_memory"
    respond with response
```

---

## New Real-World Examples

### FinOps Guardrails (`examples/real_world/11_finops_guardrails.nc`)

Use NC for budget burn checks, rightsizing candidates, and portfolio summaries.

### Customer Success Ops (`examples/real_world/12_customer_success_ops.nc`)

Score account health, generate AI success playbooks, and create renewal watchlists.

### Release Readiness Gate (`examples/real_world/13_release_readiness_gate.nc`)

Compute release score, decide ship/review/hold, and produce AI-assisted release notes.

---

## Using NC with an AI Proxy (AI proxy, local AI server, or any AI-provider-compatible gateway)

NC works with any service that exposes an AI-provider-compatible `/chat/completions` endpoint.
This includes [AI proxy Proxy](https://docs.litellm.ai/docs/proxy/user_keys), local AI server, local AI runtime, Azure API Management, and custom gateways.

### Setup

Set your proxy endpoint and credentials as environment variables. NC reads these at runtime — no API keys or URLs go into source code.

```bash
# Point NC to your proxy endpoint
export NC_AI_URL="<your-proxy-url>/chat/completions"
export NC_AI_KEY="<your-proxy-api-key>"
export NC_AI_MODEL="nova"
```

### Example: AI Classification via Proxy

```
service "ticket-classifier"
version "1.0.0"

configure:
    ai_key is env:PROXY_API_KEY
    ai_model is "nova"

to classify with ticket:
    ask AI to "classify this support ticket
        as: technical, billing, or general" using ticket
        save as category
    respond with category

api:
    POST /classify runs classify
```

Your NC code does not change based on the provider. Whether you point `NC_AI_URL` to NC AI locally, to an AI proxy, or to a self-hosted local AI server instance, the `ask AI` instruction works the same way.

### Example: Multi-model routing via proxy

If your proxy supports model routing (as AI proxy does), you can specify different models per call:

```
to process with request:
    ask AI to "classify this ticket quickly"
        using request
        using model "nova"
        save as category

    ask AI to "write a detailed response"
        using request
        using model "nova"
        save as response

    respond with {
        "category": category,
        "response": response
    }
```

The proxy receives the model name and routes the request to the correct backend.

### Example: Configure block for proxy

```
service "proxy-app"
version "1.0.0"

configure:
    ai_url is env:PROXY_URL
    ai_key is env:PROXY_API_KEY
    ai_model is "nova"

to summarize with document:
    ask AI to "summarize this document in 3 bullet points" using document
        save as summary
    respond with summary

api:
    POST /summarize runs summarize
```

### Supported proxy gateways

| Gateway | How to connect |
|---------|----------------|
| AI proxy | Set `NC_AI_URL` to your AI proxy proxy `/chat/completions` endpoint |
| local AI server | Set `NC_AI_URL` to your local AI server server endpoint |
| local AI runtime | Set `NC_AI_URL` to your local AI runtime endpoint |
| Azure API Management | Set `NC_AI_URL` to your Azure gateway endpoint |
| AWS Bedrock (via proxy) | Use an AI proxy or compatible gateway |
| Any AI-provider-compatible API | Set `NC_AI_URL` to the `/chat/completions` endpoint |

### Security notes

- Always use `env:VAR_NAME` in configure blocks — never hardcode keys or URLs
- NC automatically redacts API keys from log output
- NC supports HTTP allowlists to restrict which domains AI calls can reach
- Pass credentials via environment variables, CI secrets, or container configuration

---

## Running Any Example

```bash
# Validate syntax
nc validate examples/01_hello_world.nc

# Run and see service info
nc run examples/01_hello_world.nc

# Run a specific behavior
nc run examples/01_hello_world.nc -b greet

# See the compiled bytecodes
nc bytecode examples/01_hello_world.nc

# Generate LLVM IR
nc compile examples/01_hello_world.nc

# Run semantic analysis
nc analyze examples/existing_services/healing.nc
```
