// ═══════════════════════════════════════════════════════════
//  Agentic AI — First-Class Agent Support in NC
//
//  NC is the first language with native agent + tool syntax.
//  The agent keyword defines an autonomous AI that uses tools
//  in a Plan → Act → Observe loop.
//
//  This replaces 500+ lines of Python (LangChain/CrewAI)
//  with 60 lines of plain English.
//
//  Usage:
//    run agent researcher with "What is quantum computing?"
//        save as: answer
//    show answer
// ═══════════════════════════════════════════════════════════

service "agentic-ai-demo"
version "1.0.0"

// ── Define Tools ────────────────────────────────────────────

@tool
to search with query:
    purpose: "Search the web for information on a topic"
    gather results from "https://api.search.example/q={{query}}"
    respond with results

@tool
to analyze with data:
    purpose: "Analyze data and extract key insights"
    ask AI to "Analyze this data and extract the 3 most important insights:\n{{data}}" save as insights
    respond with insights

@tool
to summarize with content:
    purpose: "Produce a concise summary of long content"
    ask AI to "Summarize the following in 2-3 sentences:\n{{content}}" save as summary
    respond with summary

@tool
to calculate with expression:
    purpose: "Evaluate a mathematical expression"
    ask AI to "Calculate: {{expression}}. Return JSON: {\"result\": <number>}" save as result
    respond with result

// ── Define Agents ───────────────────────────────────────────

agent researcher:
    purpose: "Research any topic thoroughly by searching, analyzing, and summarizing"
    model: "gpt-4o"
    tools: [search, analyze, summarize]
    max_steps: 8

agent data_scientist:
    purpose: "Analyze datasets, compute statistics, and explain findings"
    model: "gpt-4o"
    tools: [calculate, analyze, summarize]
    max_steps: 6

agent coordinator:
    purpose: "Break complex tasks into subtasks and delegate to specialist agents"
    tools: [search, analyze, summarize, calculate]
    max_steps: 10

// ── Run Agents ──────────────────────────────────────────────

to research topic with question:
    purpose: "Run the researcher agent on a question"
    run agent researcher with question
        save as: answer
    respond with answer

to analyze data with dataset:
    purpose: "Run the data scientist agent on a dataset"
    run agent data_scientist with dataset
        save as: analysis
    respond with analysis

// ── API Endpoints ───────────────────────────────────────────

api:
    POST /research runs research topic
    POST /analyze  runs analyze data
