// ═══════════════════════════════════════════════════════════
//  NC Agentic AI Corpus — Plan-Act-Observe Training Data
//
//  This corpus teaches NOVA how to power NC agents:
//  - Structured JSON tool-calling responses
//  - Plan → Act → Observe conversation patterns
//  - Multi-tool chains and reasoning traces
//  - Proper finish actions with synthesized results
//
//  Copyright 2026 DevHeal Labs AI. All rights reserved.
// ═══════════════════════════════════════════════════════════

=== AGENT CONVERSATION: Research Agent ===

[SYSTEM] You are an AI agent with purpose: "Research topics and summarize findings"
Available tools:
- search(query): Search for information on a topic
- analyze(data): Analyze data and extract key insights
- summarize(content): Produce a concise summary of long content

[USER] Task: What are the benefits of renewable energy?

[AGENT PLAN] I need to: 1) Search for renewable energy benefits 2) Analyze the findings 3) Summarize into a clear answer.

[AGENT ACTION] {"action": "search", "input": {"query": "benefits of renewable energy"}}

[OBSERVATION] Renewable energy sources like solar, wind, and hydroelectric power offer many benefits: reduced greenhouse gas emissions, lower long-term costs, energy independence, job creation in the green sector, and sustainability for future generations. Solar panel costs have dropped 89% since 2010.

[AGENT ACTION] {"action": "analyze", "input": {"data": "Renewable energy benefits: reduced emissions, lower costs (solar down 89% since 2010), energy independence, job creation, sustainability"}}

[OBSERVATION] Key insights: 1) Environmental: significant reduction in carbon emissions 2) Economic: rapidly falling costs make renewables competitive with fossil fuels 3) Strategic: reduces dependence on imported fuels

[AGENT ACTION] {"action": "finish", "result": "Renewable energy offers three major benefits: (1) Environmental — significantly reduces greenhouse gas emissions, (2) Economic — costs have plummeted (solar down 89% since 2010) making it competitive with fossil fuels, and (3) Strategic — provides energy independence and creates new jobs in the green economy."}

=== AGENT CONVERSATION: Data Scientist Agent ===

[SYSTEM] You are an AI agent with purpose: "Analyze datasets, compute statistics, and explain findings"
Available tools:
- calculate(expression): Evaluate a mathematical expression
- analyze(data): Analyze data and extract insights
- summarize(content): Produce a concise summary

[USER] Task: A store sold 120, 145, 98, 167, 133 items over 5 days. What's the trend?

[AGENT PLAN] I need to: 1) Calculate the average 2) Analyze the trend pattern 3) Provide findings.

[AGENT ACTION] {"action": "calculate", "input": {"expression": "(120 + 145 + 98 + 167 + 133) / 5"}}

[OBSERVATION] {"result": 132.6}

[AGENT ACTION] {"action": "analyze", "input": {"data": "Daily sales: [120, 145, 98, 167, 133]. Average: 132.6. Pattern: up, down, up, down — volatile."}}

[OBSERVATION] Key insights: 1) Average daily sales is 132.6 items 2) High variance suggests inconsistent demand 3) Peak on day 4 (167) and trough on day 3 (98) — 70% swing

[AGENT ACTION] {"action": "finish", "result": "Sales analysis: Average is 132.6 items/day with high volatility (98-167 range, 70% swing). The pattern is irregular (up-down-up-down), suggesting external factors drive demand rather than a steady trend. Recommendation: investigate what drove the day 4 peak (167) and day 3 dip (98)."}

=== AGENT CONVERSATION: Code Review Agent ===

[SYSTEM] You are an AI agent with purpose: "Review code for bugs, security issues, and best practices"
Available tools:
- search(query): Search documentation and best practices
- analyze(data): Analyze code and identify issues

[USER] Task: Review this function for security issues: to login with username, password: set query to "SELECT * FROM users WHERE name='" + username + "'" gather results from database using query if results has items: respond with "Welcome " + username

[AGENT PLAN] I need to: 1) Analyze the code for vulnerabilities 2) Search for best practices 3) Provide fix.

[AGENT ACTION] {"action": "analyze", "input": {"data": "Function concatenates user input directly into SQL query string. No input sanitization or parameterized queries."}}

[OBSERVATION] Critical issue found: SQL injection vulnerability. The username parameter is concatenated directly into the SQL query without sanitization. An attacker could input: admin' OR '1'='1 to bypass authentication.

[AGENT ACTION] {"action": "search", "input": {"query": "SQL injection prevention parameterized queries"}}

[OBSERVATION] Prevention methods: 1) Use parameterized queries/prepared statements 2) Input validation and sanitization 3) Principle of least privilege for database accounts 4) Use ORM frameworks that handle escaping.

[AGENT ACTION] {"action": "finish", "result": "CRITICAL: SQL injection vulnerability. The username is concatenated directly into the query. Fix:\n\nto login with username, password:\n    set query to \"SELECT * FROM users WHERE name=?\"\n    gather results from database using query with [username]\n    if results has items:\n        respond with \"Welcome \" + username\n\nUse parameterized queries (?) instead of string concatenation to prevent injection attacks."}

=== AGENT CONVERSATION: Debugging Agent ===

[SYSTEM] You are an AI agent with purpose: "Debug and fix code issues"
Available tools:
- search(query): Search documentation
- analyze(data): Analyze code or error messages
- calculate(expression): Evaluate expressions

[USER] Task: My NC program crashes with "index out of range" when processing a list of 10 items in a repeat loop.

[AGENT PLAN] I need to: 1) Analyze the common causes of index out of range 2) Search for NC-specific loop patterns 3) Provide a fix.

[AGENT ACTION] {"action": "analyze", "input": {"data": "Index out of range error in repeat loop with 10 items. Common causes: off-by-one error (iterating 0-10 instead of 0-9), modifying list during iteration, or wrong loop bounds."}}

[OBSERVATION] Most likely cause: off-by-one error. If using 'repeat 10 times' with 1-based index accessing 0-based list, or using 'repeat from 0 to 10' which includes 10 (11 iterations for 10 items).

[AGENT ACTION] {"action": "search", "input": {"query": "NC language repeat loop list iteration"}}

[OBSERVATION] NC repeat syntax: 'repeat with item in list:' iterates safely. 'repeat from 0 to 9:' for index-based (inclusive both ends). 'repeat count of list times:' for count-based. Using 'repeat from 0 to count of list:' causes off-by-one because it includes the endpoint.

[AGENT ACTION] {"action": "finish", "result": "The crash is an off-by-one error. In NC, 'repeat from 0 to 10' iterates 11 times (0,1,...,10) but your list only has indices 0-9.\n\nFix — use either:\n1) repeat with item in mylist:        // safest, no index needed\n       show item\n2) repeat from 0 to 9:                // explicit bounds\n       show mylist[i]\n3) repeat count of mylist times:       // count-based\n       show mylist[i]"}

=== AGENT CONVERSATION: Multi-Step Research ===

[SYSTEM] You are an AI agent with purpose: "Research any topic thoroughly"
Available tools:
- search(query): Search for information
- analyze(data): Extract key insights
- summarize(content): Produce a summary

[USER] Task: Compare Python and NC for building web APIs

[AGENT PLAN] I need to: 1) Search for Python web API approach 2) Search for NC web API approach 3) Analyze differences 4) Summarize comparison.

[AGENT ACTION] {"action": "search", "input": {"query": "Python Flask FastAPI web API development"}}

[OBSERVATION] Python web APIs typically use Flask or FastAPI. Flask requires decorators (@app.route), explicit request parsing, JSON serialization, and WSGI/ASGI server setup. FastAPI adds type hints and auto-docs but still requires import statements, class definitions, and deployment configuration. Average: 30-50 lines for a CRUD API.

[AGENT ACTION] {"action": "search", "input": {"query": "NC language API endpoint definition"}}

[OBSERVATION] NC defines APIs with plain English: 'api: GET /users runs list users' — one line per endpoint. Behaviors (functions) use 'to list users:' with 'respond with' for returns. No imports, no decorators, no serialization code. Average: 10-15 lines for a CRUD API.

[AGENT ACTION] {"action": "analyze", "input": {"data": "Python: 30-50 lines, requires imports/decorators/serialization. NC: 10-15 lines, plain English, no boilerplate. Both support REST, JSON, middleware."}}

[OBSERVATION] Key differences: 1) NC is 3x more concise 2) NC uses plain English vs Python's code syntax 3) Python has larger ecosystem/libraries 4) NC has built-in API routing, Python needs frameworks 5) NC targets readability-first development

[AGENT ACTION] {"action": "finish", "result": "Python vs NC for Web APIs:\n\n| Aspect | Python (Flask/FastAPI) | NC |\n|--------|----------------------|----|\n| Lines for CRUD | 30-50 | 10-15 |\n| Syntax | Decorators + code | Plain English |\n| Routing | Framework required | Built-in |\n| Ecosystem | Massive | Growing |\n| Learning curve | Moderate | Low |\n\nNC is 3x more concise with built-in API support. Python has a larger ecosystem. Choose NC for rapid prototyping and readable APIs; Python for complex integrations with existing libraries."}

=== AGENT CONVERSATION: Coordinator Agent ===

[SYSTEM] You are an AI agent with purpose: "Break complex tasks into subtasks and delegate to specialist agents"
Available tools:
- search(query): Search for information
- analyze(data): Analyze data
- summarize(content): Summarize content
- calculate(expression): Evaluate math

[USER] Task: Create a project plan for building a to-do app in NC

[AGENT PLAN] I need to: 1) Search for NC app architecture patterns 2) Analyze requirements for a to-do app 3) Calculate effort estimates 4) Summarize into a plan.

[AGENT ACTION] {"action": "search", "input": {"query": "NC language web app architecture patterns CRUD"}}

[OBSERVATION] NC app architecture: service declaration, behaviors for logic, api block for routes, templates for UI. Standard CRUD pattern: define data model, create behaviors for create/read/update/delete, map to API endpoints. NC supports JSON storage, database adapters, and template rendering.

[AGENT ACTION] {"action": "analyze", "input": {"data": "To-do app needs: data model (id, title, done), CRUD behaviors, API endpoints, simple UI. NC provides: service block, behaviors, api routing, templates."}}

[OBSERVATION] Architecture breakdown: 1) Service setup (2 lines) 2) Data behaviors: add_todo, list_todos, complete_todo, delete_todo (20 lines) 3) API routes: POST/GET/PUT/DELETE /todos (4 lines) 4) Optional UI template (10 lines). Total: ~36 lines.

[AGENT ACTION] {"action": "calculate", "input": {"expression": "2 + 20 + 4 + 10"}}

[OBSERVATION] {"result": 36}

[AGENT ACTION] {"action": "finish", "result": "NC To-Do App Plan (~36 lines total):\n\n1. Service Setup (2 lines)\n   - service declaration and version\n\n2. Data Layer (20 lines)\n   - to add todo with title:\n   - to list todos:\n   - to complete todo with id:\n   - to delete todo with id:\n\n3. API Routes (4 lines)\n   - POST /todos, GET /todos, PUT /todos/:id, DELETE /todos/:id\n\n4. UI Template (10 lines)\n   - Simple HTML with form and list\n\nEstimated effort: 15 minutes for an experienced NC developer."}

=== AGENT CONVERSATION: Security Analysis ===

[SYSTEM] You are an AI agent with purpose: "Analyze systems for security vulnerabilities"
Available tools:
- search(query): Search security databases and documentation
- analyze(data): Analyze code or configurations for vulnerabilities

[USER] Task: Check if this NC API is secure: api: GET /users runs list_users POST /users runs create_user DELETE /users/:id runs delete_user

[AGENT PLAN] I need to: 1) Analyze the API endpoints for common security issues 2) Search for API security best practices 3) Provide recommendations.

[AGENT ACTION] {"action": "analyze", "input": {"data": "Three endpoints: GET /users (list all), POST /users (create), DELETE /users/:id (delete). No authentication mentioned. No rate limiting. No input validation visible."}}

[OBSERVATION] Security concerns: 1) No authentication — anyone can access all endpoints 2) No authorization — any user can delete any other user 3) No rate limiting on POST — enables spam 4) DELETE by ID without ownership check 5) GET returns all users — potential data exposure

[AGENT ACTION] {"action": "search", "input": {"query": "REST API security best practices authentication authorization"}}

[OBSERVATION] API security essentials: 1) Authentication on every endpoint (JWT, API key, OAuth) 2) Authorization checks (role-based access) 3) Rate limiting 4) Input validation 5) HTTPS only 6) Audit logging 7) Pagination for list endpoints 8) Don't expose sensitive user fields.

[AGENT ACTION] {"action": "finish", "result": "Security issues found:\n\n1. NO AUTHENTICATION — Add auth middleware:\n   api:\n       middleware: [verify_token]\n       GET /users runs list_users\n\n2. NO AUTHORIZATION — Check user roles:\n   to delete_user with id:\n       check current_user has role \"admin\"\n\n3. NO RATE LIMITING — Add limits:\n   POST /users has rate_limit 10 per minute\n\n4. DATA EXPOSURE — Don't return passwords/emails in list:\n   to list_users:\n       gather users from database\n       respond with users picking [\"id\", \"name\"]\n\n5. Missing: HTTPS enforcement, input validation, audit logging."}

=== TOOL-CALLING JSON FORMAT EXAMPLES ===

// Correct: single tool call
{"action": "search", "input": {"query": "machine learning basics"}}

// Correct: finish with result
{"action": "finish", "result": "Machine learning is a subset of AI that enables systems to learn from data without explicit programming."}

// Correct: tool with complex input
{"action": "analyze", "input": {"data": "Sales data: Q1=$50K, Q2=$62K, Q3=$48K, Q4=$71K. Total=$231K."}}

// Correct: calculate
{"action": "calculate", "input": {"expression": "50000 + 62000 + 48000 + 71000"}}

// Correct: summarize
{"action": "summarize", "input": {"content": "Long article about climate change impacts on agriculture, water resources, and biodiversity spanning 2000 words..."}}

=== NC AGENT SYNTAX REFERENCE ===

// Defining tools with @tool decorator
@tool
to search with query:
    purpose: "Search for information on a topic"
    gather results from "https://api.example.com/search?q={{query}}"
    respond with results

@tool
to analyze with data:
    purpose: "Analyze data and extract key insights"
    ask AI to "Analyze: {{data}}" save as insights
    respond with insights

// Defining agents
agent researcher:
    purpose: "Research any topic thoroughly"
    model: "gpt-4o"
    tools: [search, analyze, summarize]
    max_steps: 8

agent data_scientist:
    purpose: "Analyze data and compute statistics"
    tools: [calculate, analyze]
    max_steps: 6

// Running agents
run agent researcher with "What is quantum computing?"
    save as: answer
show answer

// Using agents in behaviors
to research topic with question:
    run agent researcher with question
        save as: result
    respond with result

// API endpoints that use agents
api:
    POST /research runs research topic
    POST /analyze runs analyze data

=== NC AGENT vs PYTHON LANGCHAIN ===

// NC — 15 lines
@tool
to search with query:
    purpose: "Search the web"
    gather results from "https://api.search.example/q={{query}}"
    respond with results

agent researcher:
    purpose: "Research topics thoroughly"
    tools: [search]
    max_steps: 5

to research with question:
    run agent researcher with question save as: answer
    respond with answer

// Python LangChain — 60+ lines
// from langchain.agents import initialize_agent, Tool
// from langchain.llms import OpenAI
// import requests
//
// def search(query):
//     resp = requests.get(f"https://api.search.example/q={query}")
//     return resp.json()
//
// tools = [Tool(name="search", func=search, description="Search the web")]
// llm = OpenAI(model="gpt-4o", temperature=0)
// agent = initialize_agent(tools, llm, agent="zero-shot-react-description",
//                          max_iterations=5, verbose=True)
// result = agent.run("Research quantum computing")

=== MULTI-AGENT PATTERNS ===

// Pattern 1: Sequential agents
run agent researcher with "Find data on climate change" save as: research
run agent data_scientist with research save as: analysis
run agent summarizer with analysis save as: report
show report

// Pattern 2: Coordinator delegates
agent coordinator:
    purpose: "Break tasks into subtasks and delegate"
    tools: [search, analyze, summarize, calculate]
    max_steps: 10

// Pattern 3: Specialized pipelines
to full_analysis with topic:
    run agent researcher with topic save as: raw_data
    run agent data_scientist with raw_data save as: stats
    run agent summarizer with stats save as: report
    respond with report

=== ERROR HANDLING IN AGENTS ===

// Agent with error recovery
to safe_research with question:
    try:
        run agent researcher with question save as: answer
        respond with answer
    on error:
        respond with {"error": "Agent failed", "fallback": "Please try a simpler question"}

// Agent timeout handling
agent careful_researcher:
    purpose: "Research with caution"
    tools: [search, summarize]
    max_steps: 3

=== AGENT OBSERVATION FORMAT ===

// After tool call, observation is appended to conversation:
// Step 1: Agent sends {"action": "search", "input": {"query": "NC language"}}
// Step 2: Tool executes search("NC language") → returns result
// Step 3: Observation appended: "Tool 'search' returned: NC is a plain-English programming language..."
// Step 4: Agent sees full history and decides next action
// Step 5: Agent sends {"action": "finish", "result": "NC is..."} when done

=== TRAINING NOTES ===

// This corpus trains NOVA to:
// 1. Always respond with valid JSON: {"action": "...", "input": {...}} or {"action": "finish", "result": "..."}
// 2. Plan before acting (state reasoning before tool calls)
// 3. Use tools appropriately based on their descriptions
// 4. Synthesize observations into coherent final answers
// 5. Know when to finish (don't loop unnecessarily)
// 6. Handle multi-step reasoning chains
// 7. Provide NC-specific code examples when relevant
