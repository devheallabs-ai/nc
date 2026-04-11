// ═══════════════════════════════════════════════════════════
//  AI Agent with Tools
//
//  Replaces: 150+ lines of Python with agent framework + tool registry
//
//  The agent decides what to do, uses tools, and responds.
//
//  curl -X POST http://localhost:8000/agent \
//    -d '{"task": "Find the weather in NYC and summarize todays news"}'
// ═══════════════════════════════════════════════════════════

service "ai-agent"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"

to agent with task:
    purpose: "AI agent that plans and executes multi-step tasks"

    ask AI to "You are an AI agent. Given this task, create a step-by-step plan. Return JSON with: steps (list of objects with action and description).\n\nTask: {{task}}" save as plan

    set results to []
    set step_num to 0

    repeat for each step in plan.steps:
        set step_num to step_num + 1
        log "Step {{step_num}}: {{step.description}}"

        match step.action:
            when "search":
                ask AI to "{{step.description}}" save as step_result
            when "analyze":
                ask AI to "Analyze the following and provide insights: {{step.description}}" save as step_result
            when "summarize":
                ask AI to "Summarize: {{step.description}}" save as step_result
            when "fetch":
                set step_result to {"note": "Would fetch from URL in production"}
            otherwise:
                ask AI to "{{step.description}}" save as step_result

        append {"step": step_num, "action": step.action, "result": step_result} to results

    ask AI to "Based on these completed steps, provide a final answer to the original task.\n\nTask: {{task}}\n\nStep results: {{results}}" save as final_answer

    respond with {"answer": final_answer, "steps_completed": step_num, "details": results}

to research with topic, depth:
    purpose: "Deep research agent — iteratively learns about a topic"

    set knowledge to ""
    set iteration to 0

    while iteration is below depth:
        set iteration to iteration + 1

        if iteration is equal 1:
            ask AI to "Research this topic and provide key facts: {{topic}}" save as findings
        otherwise:
            ask AI to "Based on what we know so far: {{knowledge}}\n\nDig deeper into {{topic}}. Find additional details, nuances, or related information we haven't covered." save as findings

        set knowledge to knowledge + "\n" + findings
        log "Research iteration {{iteration}} complete"

    ask AI to "Create a comprehensive research report on {{topic}} based on these findings:\n{{knowledge}}" save as report

    respond with {"report": report, "iterations": depth, "topic": topic}

to decide with situation, options:
    purpose: "Decision-making agent — analyzes options and recommends"

    ask AI to "Analyze this situation and evaluate each option. Return JSON with: analysis (string), scores (list of objects with option and score 1-10), recommendation (string), reasoning (string).\n\nSituation: {{situation}}\nOptions: {{options}}" save as decision

    respond with decision

api:
    POST /agent runs agent
    POST /research runs research
    POST /decide runs decide
