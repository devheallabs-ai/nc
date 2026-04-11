// ═══════════════════════════════════════════════════════════
//  Multi-Model LLM Router
//
//  Replaces: 100+ lines of Python with multi-model routing logic
//
//  Routes requests to the best model based on task type.
//  Falls back to cheaper models if expensive ones fail.
//
//  curl -X POST http://localhost:8000/smart \
//    -d '{"prompt": "Write a poem", "task": "creative"}'
// ═══════════════════════════════════════════════════════════

service "llm-router"
version "1.0.0"

configure:
    ai_model is "default"

to smart_route with prompt, task:
    purpose: "Route to the best model based on task type"

    match task:
        when "creative":
            ask AI to "{{prompt}}" using model "openai/gpt-4o" save as result
        when "code":
            ask AI to "{{prompt}}" using model "openai/gpt-4o" save as result
        when "fast":
            ask AI to "{{prompt}}" using model "openai/gpt-4o-mini" save as result
        when "cheap":
            ask AI to "{{prompt}}" using model "openai/gpt-4o-mini" save as result
        otherwise:
            ask AI to "{{prompt}}" save as result

    respond with {"response": result, "task": task}

to with_fallback with prompt:
    purpose: "Try expensive model first, fall back to cheaper ones"

    set models to ["openai/gpt-4o", "openai/gpt-4o-mini", "anthropic/claude-3-haiku-20240307"]

    set result to ai_with_fallback(prompt, {}, models)

    respond with result

to compare_models with prompt:
    purpose: "Send same prompt to multiple models, compare outputs"

    set results to []

    ask AI to "{{prompt}}" using model "openai/gpt-4o-mini" save as gpt_mini
    append {"model": "gpt-4o-mini", "response": gpt_mini} to results

    ask AI to "{{prompt}}" using model "openai/gpt-4o" save as gpt4o
    append {"model": "gpt-4o", "response": gpt4o} to results

    respond with results

to chain_models with input_text:
    purpose: "Chain: one model processes, another reviews"

    ask AI to "Analyze this text and extract key points as JSON: {{input_text}}" using model "openai/gpt-4o-mini" save as analysis

    ask AI to "Review this analysis for accuracy and completeness. Add anything missing. Analysis: {{analysis}}" using model "openai/gpt-4o" save as reviewed

    respond with {"initial_analysis": analysis, "reviewed": reviewed}

api:
    POST /smart runs smart_route
    POST /fallback runs with_fallback
    POST /compare runs compare_models
    POST /chain runs chain_models
