# Real-World NC Examples

These are production-ready patterns for building AI services with NC. Each example replaces 100-200 lines of Python with 20-50 lines of plain English.

## Quick Start

```bash
# Set YOUR AI provider credentials
export NC_AI_URL="<YOUR_PROVIDER_ENDPOINT>"
export NC_AI_KEY="<YOUR_API_KEY>"
export NC_AI_MODEL="<YOUR_MODEL_NAME>"

# Run any example as a server
nc serve examples/real_world/01_chat_assistant.nc

# Then call it
curl -X POST http://localhost:8000/chat -d '{"message": "Hello!", "session": "s1"}'
```

## Examples

| # | Example | What It Replaces | NC Lines | Python Lines |
|---|---------|-----------------|----------|-------------|
| 01 | [Chat Assistant](01_chat_assistant.nc) | Cloud AI assistants | 35 | 80+ |
| 02 | [RAG Knowledge Base](02_rag_knowledge_base.nc) | LlamaIndex, LangChain RAG | 60 | 150+ |
| 03 | [Multi-Model LLM Router](03_llm_multi_model.nc) | AI router, AI proxy | 55 | 120+ |
| 04 | [AI Agent with Tools](04_ai_agent.nc) | LangChain Agent, AutoGPT | 65 | 200+ |
| 05 | [Customer Support AI](05_customer_support.nc) | Zendesk AI, Intercom Fin | 55 | 180+ |
| 06 | [Content Generator](06_content_generator.nc) | Jasper, Copy.ai | 70 | 150+ |
| 07 | [Code Reviewer](07_code_reviewer.nc) | CodeRabbit, Copilot Review | 60 | 130+ |
| 08 | [ML Model Pipeline](08_ml_pipeline.nc) | MLflow, SageMaker | 50 | 120+ |
| 09 | [Data Analyst](09_data_analyst.nc) | Pandas AI, Julius AI | 55 | 160+ |
| 10 | [Workflow Automation](10_workflow_automation.nc) | Zapier, Make, n8n | 75 | 250+ |

## Common Patterns

### Pattern 1: Simple AI Call
```
ask AI to "do something with {{input}}" save as result
respond with result
```

### Pattern 2: RAG (Read → Chunk → Ask)
```
set doc to read_file("data.txt")
set chunks to chunk(doc, 1000)
ask AI to "Answer {{question}} from: {{chunks}}" save as answer
```

### Pattern 3: Classify → Route → Act
```
ask AI to "Classify: {{input}}" save as classification
match classification.category:
    when "urgent": notify "team" "Urgent: {{input}}"
    when "spam": log "Rejected"
respond with classification
```

### Pattern 4: Multi-Model Fallback
```
set models to ["nova", "nova-mini"]
set result to ai_with_fallback(prompt, context, models)
```

### Pattern 5: Validate AI Response
```
ask AI to "Return JSON with name, age" save as result
set check to validate(result, ["name", "age"])
if check.valid is equal no:
    ask AI to "Try again, return JSON with name and age" save as result
```

### Pattern 6: AI Agent (Plan → Execute → Synthesize)
```
ask AI to "Create a plan for: {{task}}" save as plan
repeat for each step in plan.steps:
    ask AI to "Execute: {{step}}" save as step_result
ask AI to "Synthesize results: {{results}}" save as final
```
