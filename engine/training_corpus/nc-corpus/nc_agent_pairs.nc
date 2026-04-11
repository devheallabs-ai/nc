// NC Agentic Tool-Calling Training Pairs
// Each pair: system+task prompt → JSON action response

// === Pair 1 ===
// PROMPT: You are an AI agent. Research topics and summarize findings.

Available tools:
- search(query): Sear...
// RESPONSE: {"action": "search", "input": {"query": "quantum computing basics"}}

// ask AI to "You are an AI agent. Research topics and summarize findings.\n\nAvailable tools:\n- search(query): Search for information on a topic\n- summarize(content): Produce a concise summary\n\nTask: What is q" save as result
// expected: {"action": "search", "input": {"query": "quantum computing basics"}}

// === Pair 2 ===
// PROMPT: You are an AI agent. Research topics and summarize findings.

Available tools:
- search(query): Sear...
// RESPONSE: {"action": "summarize", "input": {"content": "Quantum computing uses qubits in superposition states enabling parallel computation for factoring, optimization and simulation problems."}}

// ask AI to "You are an AI agent. Research topics and summarize findings.\n\nAvailable tools:\n- search(query): Search for information\n- summarize(content): Summarize text\n\nTask: What is quantum computing?\n\nT" save as result
// expected: {"action": "summarize", "input": {"content": "Quantum computing uses qubits in superposition states enabling parallel computation for factoring, optimization and simulation problems."}}

// === Pair 3 ===
// PROMPT: You are an AI agent. Research topics and summarize findings.

Available tools:
- search(query): Sear...
// RESPONSE: {"action": "finish", "result": "Quantum computing harnesses quantum mechanics to process information using qubits that can exist in multiple states simultaneously. This enables faster solutions for specific problems like cryptography, optimization, and molecular simulation compared to classical computers."}

// ask AI to "You are an AI agent. Research topics and summarize findings.\n\nAvailable tools:\n- search(query): Search for information\n- summarize(content): Summarize text\n\nTask: What is quantum computing?\n\nT" save as result
// expected: {"action": "finish", "result": "Quantum computing harnesses quantum mechanics to process information using qubits that can exist in multiple states simultaneously. This enables faster solutions for specific problems like cryptography, optimization, and molecular simulation compared to classical computers."}

// === Pair 4 ===
// PROMPT: You are an AI agent. Analyze data and provide insights.

Available tools:
- calculate(expression): E...
// RESPONSE: {"action": "calculate", "input": {"expression": "(85 + 92 + 78 + 96 + 88) / 5"}}

// ask AI to "You are an AI agent. Analyze data and provide insights.\n\nAvailable tools:\n- calculate(expression): Evaluate math\n- analyze(data): Extract insights from data\n\nTask: What is the average of 85, 92," save as result
// expected: {"action": "calculate", "input": {"expression": "(85 + 92 + 78 + 96 + 88) / 5"}}

// === Pair 5 ===
// PROMPT: You are an AI agent. Analyze data and provide insights.

Available tools:
- calculate(expression): E...
// RESPONSE: {"action": "finish", "result": "The average of 85, 92, 78, 96, 88 is 87.8"}

// ask AI to "You are an AI agent. Analyze data and provide insights.\n\nAvailable tools:\n- calculate(expression): Evaluate math\n- analyze(data): Extract insights\n\nTask: What is the average of 85, 92, 78, 96, 8" save as result
// expected: {"action": "finish", "result": "The average of 85, 92, 78, 96, 88 is 87.8"}

// === Pair 6 ===
// PROMPT: You are an AI agent. Debug and fix code issues.

Available tools:
- search(query): Search documentat...
// RESPONSE: {"action": "search", "input": {"query": "division by zero error handling"}}

// ask AI to "You are an AI agent. Debug and fix code issues.\n\nAvailable tools:\n- search(query): Search documentation\n- analyze(data): Analyze code or data\n- calculate(expression): Evaluate expressions\n\nTask" save as result
// expected: {"action": "search", "input": {"query": "division by zero error handling"}}

// === Pair 7 ===
// PROMPT: You are an AI agent. Debug and fix code issues.

Available tools:
- search(query): Search documentat...
// RESPONSE: {"action": "finish", "result": "Division by zero crashes because it's mathematically undefined. To fix: check if the divisor is zero before dividing, wrap in try/on_error block, or provide a default value. Example in NC:\n\nto safe_divide with a, b:\n    if b is equal 0:\n        respond with 0\n    respond with a / b"}

// ask AI to "You are an AI agent. Debug and fix code issues.\n\nAvailable tools:\n- search(query): Search documentation\n- analyze(data): Analyze code\n\nTask: Why does dividing by zero crash my program?\n\nTool '" save as result
// expected: {"action": "finish", "result": "Division by zero crashes because it's mathematically undefined. To fix: check if the divisor is zero before dividing, wrap in try/on_error block, or provide a default value. Example in NC:\n\nto safe_divide with a, b:\n    if b is equal 0:\n        respond with 0\n    respond with a / b"}

// === Pair 8 ===
// PROMPT: You are an AI agent. Help build NC applications.

Available tools:
- search(query): Search NC docume...
// RESPONSE: {"action": "search", "input": {"query": "NC REST API weather service example"}}

// ask AI to "You are an AI agent. Help build NC applications.\n\nAvailable tools:\n- search(query): Search NC documentation\n- analyze(data): Analyze requirements\n- summarize(content): Summarize findings\n\nTask:" save as result
// expected: {"action": "search", "input": {"query": "NC REST API weather service example"}}

// === Pair 9 ===
// PROMPT: You are an AI agent. Help build NC applications.

Available tools:
- search(query): Search docs
- an...
// RESPONSE: {"action": "analyze", "input": {"data": "Requirements: weather API needs GET /weather endpoint, accepts city parameter, fetches from weather API, returns temperature and conditions"}}

// ask AI to "You are an AI agent. Help build NC applications.\n\nAvailable tools:\n- search(query): Search docs\n- analyze(data): Analyze requirements\n\nTask: Build a weather API service in NC\n\nTool 'search' re" save as result
// expected: {"action": "analyze", "input": {"data": "Requirements: weather API needs GET /weather endpoint, accepts city parameter, fetches from weather API, returns temperature and conditions"}}

// === Pair 10 ===
// PROMPT: You are an AI agent. Help build NC applications.

Available tools:
- search(query): Search docs
- an...
// RESPONSE: {"action": "finish", "result": "service \"weather-api\"\nversion \"1.0.0\"\n\nto get weather with city:\n    purpose: \"Fetch current weather for a city\"\n    gather data from \"https://api.weather.example/current?city={{city}}\"\n    respond with {\"city\": city, \"temperature\": data.temp, \"conditions\": data.description}\n\napi:\n    GET /weather runs get weather"}

// ask AI to "You are an AI agent. Help build NC applications.\n\nAvailable tools:\n- search(query): Search docs\n- analyze(data): Analyze requirements\n\nTask: Build a weather API service in NC\n\nTool 'search' re" save as result
// expected: {"action": "finish", "result": "service \"weather-api\"\nversion \"1.0.0\"\n\nto get weather with city:\n    purpose: \"Fetch current weather for a city\"\n    gather data from \"https://api.weather.example/current?city={{city}}\"\n    respond with {\"city\": city, \"temperature\": data.temp, \"conditions\": data.description}\n\napi:\n    GET /weather runs get weather"}

// === Pair 11 ===
// PROMPT: You are an AI agent. Compare technologies objectively.

Available tools:
- search(query): Search for...
// RESPONSE: {"action": "search", "input": {"query": "Python Flask FastAPI vs NC language API development comparison"}}

// ask AI to "You are an AI agent. Compare technologies objectively.\n\nAvailable tools:\n- search(query): Search for information\n- analyze(data): Analyze and compare\n- summarize(content): Summarize findings\n\nT" save as result
// expected: {"action": "search", "input": {"query": "Python Flask FastAPI vs NC language API development comparison"}}

// === Pair 12 ===
// PROMPT: You are an AI agent. Compare technologies.

Available tools:
- search(query): Search
- analyze(data)...
// RESPONSE: {"action": "analyze", "input": {"data": "Python: mature ecosystem, many libraries, verbose syntax, runtime needed. NC: plain English syntax, compiles native, built-in AI, fewer libraries, newer language."}}

// ask AI to "You are an AI agent. Compare technologies.\n\nAvailable tools:\n- search(query): Search\n- analyze(data): Analyze\n- summarize(content): Summarize\n\nTask: Compare Python vs NC for APIs\n\nTool 'searc" save as result
// expected: {"action": "analyze", "input": {"data": "Python: mature ecosystem, many libraries, verbose syntax, runtime needed. NC: plain English syntax, compiles native, built-in AI, fewer libraries, newer language."}}

// === Pair 13 ===
// PROMPT: You are an AI agent. Compare technologies.

Available tools:
- search(query): Search
- analyze(data)...
// RESPONSE: {"action": "finish", "result": "Python vs NC for APIs:\n\nPython: Mature ecosystem with Flask/FastAPI, thousands of libraries, large community. Requires decorators, type hints, async boilerplate. Needs runtime.\n\nNC: 10x less code with plain English syntax, compiles to native binary, built-in AI with ask AI, zero boilerplate. Newer ecosystem.\n\nRecommendation: NC for new AI-powered APIs. Python for projects needing specific libraries."}

// ask AI to "You are an AI agent. Compare technologies.\n\nAvailable tools:\n- search(query): Search\n- analyze(data): Analyze\n- summarize(content): Summarize\n\nTask: Compare Python vs NC for APIs\n\nTool 'searc" save as result
// expected: {"action": "finish", "result": "Python vs NC for APIs:\n\nPython: Mature ecosystem with Flask/FastAPI, thousands of libraries, large community. Requires decorators, type hints, async boilerplate. Needs runtime.\n\nNC: 10x less code with plain English syntax, compiles to native binary, built-in AI with ask AI, zero boilerplate. Newer ecosystem.\n\nRecommendation: NC for new AI-powered APIs. Python for projects needing specific libraries."}

// === Pair 14 ===
// PROMPT: You are an AI agent. You help with data analysis.

Available tools:
- calculate(expression): Math ev...
// RESPONSE: {"action": "calculate", "input": {"expression": "10000 * (1 + 0.05)^3"}}

// ask AI to "You are an AI agent. You help with data analysis.\n\nAvailable tools:\n- calculate(expression): Math evaluation\n- analyze(data): Data analysis\n\nTask: Calculate compound interest on $10000 at 5% for" save as result
// expected: {"action": "calculate", "input": {"expression": "10000 * (1 + 0.05)^3"}}

// === Pair 15 ===
// PROMPT: You are an AI agent. Data analysis.

Available tools:
- calculate(expression): Math
- analyze(data):...
// RESPONSE: {"action": "calculate", "input": {"expression": "11576.25 - 10000"}}

// ask AI to "You are an AI agent. Data analysis.\n\nAvailable tools:\n- calculate(expression): Math\n- analyze(data): Analysis\n\nTask: Calculate compound interest on $10000 at 5% for 3 years\n\nTool 'calculate' r" save as result
// expected: {"action": "calculate", "input": {"expression": "11576.25 - 10000"}}

// === Pair 16 ===
// PROMPT: You are an AI agent. Data analysis.

Available tools:
- calculate(expression): Math
- analyze(data):...
// RESPONSE: {"action": "finish", "result": "Compound interest calculation:\n- Principal: $10,000\n- Rate: 5% annually\n- Duration: 3 years\n- Final amount: $11,576.25\n- Interest earned: $1,576.25\n\nFormula: A = P(1 + r)^t = 10000 * (1.05)^3 = $11,576.25"}

// ask AI to "You are an AI agent. Data analysis.\n\nAvailable tools:\n- calculate(expression): Math\n- analyze(data): Analysis\n\nTask: Calculate compound interest on $10000 at 5% for 3 years\n\nTool 'calculate' r" save as result
// expected: {"action": "finish", "result": "Compound interest calculation:\n- Principal: $10,000\n- Rate: 5% annually\n- Duration: 3 years\n- Final amount: $11,576.25\n- Interest earned: $1,576.25\n\nFormula: A = P(1 + r)^t = 10000 * (1.05)^3 = $11,576.25"}

// === Pair 17 ===
// PROMPT: You are an AI agent. Help users learn NC.

Available tools:
- search(query): Search NC docs
- summar...
// RESPONSE: {"action": "search", "input": {"query": "NC language list creation and repeat for each loop"}}

// ask AI to "You are an AI agent. Help users learn NC.\n\nAvailable tools:\n- search(query): Search NC docs\n- summarize(content): Summarize\n\nTask: How do I create a list and loop through it in NC?" save as result
// expected: {"action": "search", "input": {"query": "NC language list creation and repeat for each loop"}}

// === Pair 18 ===
// PROMPT: You are an AI agent. Help users learn NC.

Available tools:
- search(query): Search docs
- summarize...
// RESPONSE: {"action": "finish", "result": "Creating and looping through lists in NC:\n\n// Create a list\nset fruits to [\"apple\", \"banana\", \"cherry\"]\n\n// Loop through it\nrepeat for each fruit in fruits:\n    show fruit\n\n// Build a list dynamically\nset numbers to []\nrepeat 5 times with i:\n    append i to numbers\n\n// Result: [1, 2, 3, 4, 5]"}

// ask AI to "You are an AI agent. Help users learn NC.\n\nAvailable tools:\n- search(query): Search docs\n- summarize(content): Summarize\n\nTask: How do I create a list and loop through it in NC?\n\nTool 'search'" save as result
// expected: {"action": "finish", "result": "Creating and looping through lists in NC:\n\n// Create a list\nset fruits to [\"apple\", \"banana\", \"cherry\"]\n\n// Loop through it\nrepeat for each fruit in fruits:\n    show fruit\n\n// Build a list dynamically\nset numbers to []\nrepeat 5 times with i:\n    append i to numbers\n\n// Result: [1, 2, 3, 4, 5]"}

// === Pair 19 ===
// PROMPT: You are an AI agent. Diagnose and fix system issues.

Available tools:
- search(query): Search logs ...
// RESPONSE: {"action": "search", "input": {"query": "API response time degradation common causes"}}

// ask AI to "You are an AI agent. Diagnose and fix system issues.\n\nAvailable tools:\n- search(query): Search logs and documentation\n- analyze(data): Analyze error patterns\n- calculate(expression): Compute metr" save as result
// expected: {"action": "search", "input": {"query": "API response time degradation common causes"}}

// === Pair 20 ===
// PROMPT: You are an AI agent. Diagnose system issues.

Available tools:
- search(query): Search
- analyze(dat...
// RESPONSE: {"action": "analyze", "input": {"data": "50ms to 2000ms is a 40x slowdown. Most likely causes: 1) Database query without index (would show in slow query log), 2) External API timeout (2s is common timeout), 3) Connection pool exhausted (requests queuing)"}}

// ask AI to "You are an AI agent. Diagnose system issues.\n\nAvailable tools:\n- search(query): Search\n- analyze(data): Analyze\n- calculate(expression): Compute\n\nTask: API response time went from 50ms to 2000m" save as result
// expected: {"action": "analyze", "input": {"data": "50ms to 2000ms is a 40x slowdown. Most likely causes: 1) Database query without index (would show in slow query log), 2) External API timeout (2s is common timeout), 3) Connection pool exhausted (requests queuing)"}}

// === Pair 21 ===
// PROMPT: You are an AI agent. Diagnose system issues.

Available tools:
- search(query): Search
- analyze(dat...
// RESPONSE: {"action": "finish", "result": "Diagnosis: 40x slowdown (50ms to 2000ms) likely caused by:\n\n1. **Database** (most likely): Missing index on a new query. Check slow query log.\n2. **External API**: A downstream service timing out at 2s default.\n3. **Connection pool**: Exhausted pool causing request queuing.\n\nImmediate steps:\n- Check database slow query log\n- Monitor external API response times\n- Check connection pool usage\n- Review recent deployments for new queries\n\nNC fix example:\ntry:\n    gather data from api_url\non_error:\n    log \"API timeout - using cache\"\n    respond with cached_data"}

// ask AI to "You are an AI agent. Diagnose system issues.\n\nAvailable tools:\n- search(query): Search\n- analyze(data): Analyze\n- calculate(expression): Compute\n\nTask: API response time went from 50ms to 2000m" save as result
// expected: {"action": "finish", "result": "Diagnosis: 40x slowdown (50ms to 2000ms) likely caused by:\n\n1. **Database** (most likely): Missing index on a new query. Check slow query log.\n2. **External API**: A downstream service timing out at 2s default.\n3. **Connection pool**: Exhausted pool causing request queuing.\n\nImmediate steps:\n- Check database slow query log\n- Monitor external API response times\n- Check connection pool usage\n- Review recent deployments for new queries\n\nNC fix example:\ntry:\n    gather data from api_url\non_error:\n    log \"API timeout - using cache\"\n    respond with cached_data"}

// === Pair 22 ===
// PROMPT: You are an AI agent with no tools available.

Task: What is 2 + 2?...
// RESPONSE: {"action": "finish", "result": "4"}

// ask AI to "You are an AI agent with no tools available.\n\nTask: What is 2 + 2?" save as result
// expected: {"action": "finish", "result": "4"}

// === Pair 23 ===
// PROMPT: You are an AI agent. General assistant.

Available tools:
- search(query): Search

Task: Hello, how ...
// RESPONSE: {"action": "finish", "result": "Hello! I'm NOVA, an AI assistant built by DevHeal Labs. I'm ready to help you with research, analysis, coding, or any questions. What would you like to work on?"}

// ask AI to "You are an AI agent. General assistant.\n\nAvailable tools:\n- search(query): Search\n\nTask: Hello, how are you?" save as result
// expected: {"action": "finish", "result": "Hello! I'm NOVA, an AI assistant built by DevHeal Labs. I'm ready to help you with research, analysis, coding, or any questions. What would you like to work on?"}

// === Pair 24 ===
// PROMPT: You are an AI agent. Code reviewer.

Available tools:
- analyze(data): Analyze code quality
- search...
// RESPONSE: {"action": "analyze", "input": {"data": "NC behavior: add user with name, email. Creates map, stores to users, returns user. Missing: input validation, duplicate check, error handling, purpose statement."}}

// ask AI to "You are an AI agent. Code reviewer.\n\nAvailable tools:\n- analyze(data): Analyze code quality\n- search(query): Search best practices\n\nTask: Review this NC code:\nto add user with name, email:\n   " save as result
// expected: {"action": "analyze", "input": {"data": "NC behavior: add user with name, email. Creates map, stores to users, returns user. Missing: input validation, duplicate check, error handling, purpose statement."}}

// === Pair 25 ===
// PROMPT: You are an AI agent. Code reviewer.

Available tools:
- analyze(data): Analyze code
- search(query):...
// RESPONSE: {"action": "finish", "result": "Code Review:\n\nIssues found:\n1. No input validation (empty name/email)\n2. No duplicate email check\n3. No error handling\n4. Missing purpose statement\n\nImproved version:\nto add user with name, email:\n    purpose: \"Create a new user account\"\n    if name is equal \"\" or email is equal \"\":\n        respond with {\"error\": \"name and email required\"}\n    check \"email not already registered\" using email\n    set user to {\"name\": name, \"email\": email, \"created\": now()}\n    try:\n        store user into \"users\"\n    on_error:\n        respond with {\"error\": \"failed to save user\"}\n    respond with user"}

// ask AI to "You are an AI agent. Code reviewer.\n\nAvailable tools:\n- analyze(data): Analyze code\n- search(query): Search best practices\n\nTask: Review this NC code: to add user...\n\nTool 'analyze' returned: " save as result
// expected: {"action": "finish", "result": "Code Review:\n\nIssues found:\n1. No input validation (empty name/email)\n2. No duplicate email check\n3. No error handling\n4. Missing purpose statement\n\nImproved version:\nto add user with name, email:\n    purpose: \"Create a new user account\"\n    if name is equal \"\" or email is equal \"\":\n        respond with {\"error\": \"name and email required\"}\n    check \"email not already registered\" using email\n    set user to {\"name\": name, \"email\": email, \"created\": now()}\n    try:\n        store user into \"users\"\n    on_error:\n        respond with {\"error\": \"failed to save user\"}\n    respond with user"}

// === Pair 26 ===
// PROMPT: You are an AI agent. Multi-step researcher.

Available tools:
- search(query): Web search
- analyze(...
// RESPONSE: {"action": "calculate", "input": {"expression": "366 * 24 * 60 * 60"}}

// ask AI to "You are an AI agent. Multi-step researcher.\n\nAvailable tools:\n- search(query): Web search\n- analyze(data): Analysis\n- summarize(content): Summarization\n- calculate(expression): Math\n\nTask: How" save as result
// expected: {"action": "calculate", "input": {"expression": "366 * 24 * 60 * 60"}}

// === Pair 27 ===
// PROMPT: You are an AI agent.

Available tools:
- calculate(expression): Math

Task: How many seconds in a le...
// RESPONSE: {"action": "finish", "result": "A leap year has 31,622,400 seconds.\n\nCalculation: 366 days x 24 hours x 60 minutes x 60 seconds = 31,622,400"}

// ask AI to "You are an AI agent.\n\nAvailable tools:\n- calculate(expression): Math\n\nTask: How many seconds in a leap year?\n\nTool 'calculate' returned: {\"result\": 31622400}\n\nContinue with the next step." save as result
// expected: {"action": "finish", "result": "A leap year has 31,622,400 seconds.\n\nCalculation: 366 days x 24 hours x 60 minutes x 60 seconds = 31,622,400"}

// === Pair 28 ===
// PROMPT: You are an AI agent. Security analyst.

Available tools:
- search(query): Search security advisories...
// RESPONSE: {"action": "analyze", "input": {"data": "Security issues: 1) Plaintext password comparison (should hash), 2) SQL/NoSQL injection via {{username}} template, 3) No rate limiting, 4) No password complexity check, 5) Token generation may be weak"}}

// ask AI to "You are an AI agent. Security analyst.\n\nAvailable tools:\n- search(query): Search security advisories\n- analyze(data): Analyze vulnerabilities\n\nTask: Is this NC code secure?\nto login with userna" save as result
// expected: {"action": "analyze", "input": {"data": "Security issues: 1) Plaintext password comparison (should hash), 2) SQL/NoSQL injection via {{username}} template, 3) No rate limiting, 4) No password complexity check, 5) Token generation may be weak"}}

// === Pair 29 ===
// PROMPT: You are an AI agent. Security analyst.

Available tools:
- search(query): Search
- analyze(data): An...
// RESPONSE: {"action": "finish", "result": "SECURITY AUDIT: 5 Critical Issues Found\n\n1. **Plaintext passwords** - CRITICAL: Never store/compare plain passwords\n2. **Injection** - HIGH: {{username}} in gather URL allows injection\n3. **No rate limiting** - MEDIUM: Enables brute force attacks\n4. **No input validation** - MEDIUM: Missing length/format checks\n5. **Token security** - LOW: Verify generate_token uses cryptographic randomness\n\nFixed version:\nto login with username, password:\n    purpose: \"Authenticate user securely\"\n    if len(username) is below 3 or len(password) is below 8:\n        respond with {\"error\": \"invalid credentials\"}\n    set safe_user to sanitize(username)\n    gather user from \"db://users\" where \"name\" is safe_user\n    set hashed to hash(password, user.salt)\n    if hashed is equal user.password_hash:\n        respond with {\"token\": jwt_sign({\"sub\": username})}\n    respond with {\"error\": \"invalid credentials\"}"}

// ask AI to "You are an AI agent. Security analyst.\n\nAvailable tools:\n- search(query): Search\n- analyze(data): Analyze\n\nTask: Is this NC code secure? to login with username, password...\n\nTool 'analyze' ret" save as result
// expected: {"action": "finish", "result": "SECURITY AUDIT: 5 Critical Issues Found\n\n1. **Plaintext passwords** - CRITICAL: Never store/compare plain passwords\n2. **Injection** - HIGH: {{username}} in gather URL allows injection\n3. **No rate limiting** - MEDIUM: Enables brute force attacks\n4. **No input validation** - MEDIUM: Missing length/format checks\n5. **Token security** - LOW: Verify generate_token uses cryptographic randomness\n\nFixed version:\nto login with username, password:\n    purpose: \"Authenticate user securely\"\n    if len(username) is below 3 or len(password) is below 8:\n        respond with {\"error\": \"invalid credentials\"}\n    set safe_user to sanitize(username)\n    gather user from \"db://users\" where \"name\" is safe_user\n    set hashed to hash(password, user.salt)\n    if hashed is equal user.password_hash:\n        respond with {\"token\": jwt_sign({\"sub\": username})}\n    respond with {\"error\": \"invalid credentials\"}"}

// === Pair 30 ===
// PROMPT: You are an AI agent. Data transformer.

Available tools:
- analyze(data): Analyze data structure
- c...
// RESPONSE: {"action": "analyze", "input": {"data": "CSV with 3 columns (name, age, city) and 2 rows. Convert to JSON array of objects."}}

// ask AI to "You are an AI agent. Data transformer.\n\nAvailable tools:\n- analyze(data): Analyze data structure\n- calculate(expression): Math operations\n\nTask: Convert this CSV to JSON: name,age,city\nAlice,30" save as result
// expected: {"action": "analyze", "input": {"data": "CSV with 3 columns (name, age, city) and 2 rows. Convert to JSON array of objects."}}

// === Pair 31 ===
// PROMPT: You are an AI agent. Data transformer.

Available tools:
- analyze(data): Analyze
- calculate(expres...
// RESPONSE: {"action": "finish", "result": "[{\"name\": \"Alice\", \"age\": 30, \"city\": \"NYC\"}, {\"name\": \"Bob\", \"age\": 25, \"city\": \"LA\"}]"}

// ask AI to "You are an AI agent. Data transformer.\n\nAvailable tools:\n- analyze(data): Analyze\n- calculate(expression): Math\n\nTask: Convert CSV to JSON: name,age,city\nAlice,30,NYC\nBob,25,LA\n\nTool 'analyz" save as result
// expected: {"action": "finish", "result": "[{\"name\": \"Alice\", \"age\": 30, \"city\": \"NYC\"}, {\"name\": \"Bob\", \"age\": 25, \"city\": \"LA\"}]"}

// === Pair 32 ===
// PROMPT: You are an AI agent. NC language expert.

Available tools:
- search(query): Search NC documentation
...
// RESPONSE: {"action": "search", "input": {"query": "NC agent health monitoring check gather schedule"}}

// ask AI to "You are an AI agent. NC language expert.\n\nAvailable tools:\n- search(query): Search NC documentation\n- analyze(data): Analyze code\n\nTask: Write an NC agent that monitors server health" save as result
// expected: {"action": "search", "input": {"query": "NC agent health monitoring check gather schedule"}}

// === Pair 33 ===
// PROMPT: You are an AI agent. NC expert.

Available tools:
- search(query): Search docs
- analyze(data): Anal...
// RESPONSE: {"action": "finish", "result": "service \"health-monitor\"\nversion \"1.0.0\"\n\n@tool\nto check health with url:\n    purpose: \"Check if a service is responding\"\n    try:\n        gather response from url\n        respond with {\"status\": \"healthy\", \"url\": url}\n    on_error:\n        respond with {\"status\": \"down\", \"url\": url}\n\n@tool\nto send alert with message:\n    purpose: \"Send alert notification\"\n    notify \"ops-channel\" message\n    respond with {\"sent\": true}\n\nagent monitor:\n    purpose: \"Monitor server health and alert on failures\"\n    tools: [check health, send alert]\n    max_steps: 5\n\nevery 1 minute:\n    run agent monitor with \"Check all services\""}

// ask AI to "You are an AI agent. NC expert.\n\nAvailable tools:\n- search(query): Search docs\n- analyze(data): Analyze\n\nTask: Write an NC agent that monitors server health\n\nTool 'search' returned: NC support" save as result
// expected: {"action": "finish", "result": "service \"health-monitor\"\nversion \"1.0.0\"\n\n@tool\nto check health with url:\n    purpose: \"Check if a service is responding\"\n    try:\n        gather response from url\n        respond with {\"status\": \"healthy\", \"url\": url}\n    on_error:\n        respond with {\"status\": \"down\", \"url\": url}\n\n@tool\nto send alert with message:\n    purpose: \"Send alert notification\"\n    notify \"ops-channel\" message\n    respond with {\"sent\": true}\n\nagent monitor:\n    purpose: \"Monitor server health and alert on failures\"\n    tools: [check health, send alert]\n    max_steps: 5\n\nevery 1 minute:\n    run agent monitor with \"Check all services\""}

// === Pair 34 ===
// PROMPT: You are an AI agent. You must respond with JSON only.

Available tools:
- search(query): Search

Tas...
// RESPONSE: {"action": "search", "input": {"query": "NC language features list"}}

// ask AI to "You are an AI agent. You must respond with JSON only.\n\nAvailable tools:\n- search(query): Search\n\nTask: search for NC language features" save as result
// expected: {"action": "search", "input": {"query": "NC language features list"}}

// === Pair 35 ===
// PROMPT: You are an AI agent.

Available tools:
- search(query): Search

Task: search for NC language feature...
// RESPONSE: {"action": "finish", "result": "NC Language Key Features:\n1. Plain English syntax (no semicolons, no brackets)\n2. Built-in AI: ask AI to \"prompt\" save as result\n3. First-class agents: agent keyword with tools\n4. Native compilation to binary\n5. REST API routing built-in\n6. Pattern matching with match/when\n7. Generics with type parameters\n8. Async/await for concurrency\n9. Operator overloading\n10. Auto-correction of typos"}

// ask AI to "You are an AI agent.\n\nAvailable tools:\n- search(query): Search\n\nTask: search for NC language features\n\nTool 'search' returned: NC features: plain English syntax, built-in AI with ask AI, native" save as result
// expected: {"action": "finish", "result": "NC Language Key Features:\n1. Plain English syntax (no semicolons, no brackets)\n2. Built-in AI: ask AI to \"prompt\" save as result\n3. First-class agents: agent keyword with tools\n4. Native compilation to binary\n5. REST API routing built-in\n6. Pattern matching with match/when\n7. Generics with type parameters\n8. Async/await for concurrency\n9. Operator overloading\n10. Auto-correction of typos"}

// === Pair 36 ===
// PROMPT: You are an AI agent. Task planner.

Available tools:
- search(query): Research
- analyze(data): Anal...
// RESPONSE: {"action": "search", "input": {"query": "microservices architecture e-commerce platform services"}}

// ask AI to "You are an AI agent. Task planner.\n\nAvailable tools:\n- search(query): Research\n- analyze(data): Analysis\n- calculate(expression): Math\n- summarize(content): Summarize\n\nTask: Plan a microservic" save as result
// expected: {"action": "search", "input": {"query": "microservices architecture e-commerce platform services"}}

// === Pair 37 ===
// PROMPT: You are an AI agent. Task planner.

Available tools:
- search(query): Research
- analyze(data): Anal...
// RESPONSE: {"action": "analyze", "input": {"data": "9 services identified. Core path: user -> catalog -> cart -> order -> payment -> notification. Supporting: inventory, search, recommendation. Each service needs its own DB, API, and health check."}}

// ask AI to "You are an AI agent. Task planner.\n\nAvailable tools:\n- search(query): Research\n- analyze(data): Analysis\n- calculate(expression): Math\n- summarize(content): Summarize\n\nTask: Plan microservices" save as result
// expected: {"action": "analyze", "input": {"data": "9 services identified. Core path: user -> catalog -> cart -> order -> payment -> notification. Supporting: inventory, search, recommendation. Each service needs its own DB, API, and health check."}}

// === Pair 38 ===
// PROMPT: You are an AI agent. Task planner.

Available tools:
- search, analyze, calculate, summarize

Task: ...
// RESPONSE: {"action": "finish", "result": "E-Commerce Microservices Architecture:\n\n**Core Services (Phase 1):**\n1. User Service - auth, profiles, addresses\n2. Product Catalog - items, categories, pricing\n3. Cart Service - shopping cart, sessions\n4. Order Service - order lifecycle, history\n5. Payment Service - transactions, refunds\n\n**Supporting Services (Phase 2):**\n6. Inventory Service - stock tracking\n7. Notification Service - email, SMS, push\n8. Search Service - product search, filters\n9. Recommendation Service - personalized suggestions\n\n**Infrastructure:**\n- API Gateway for routing\n- Event bus for inter-service communication\n- Per-service databases\n- Health monitoring agent\n\nEach service in NC: ~30-50 lines of plain English code."}

// ask AI to "You are an AI agent. Task planner.\n\nAvailable tools:\n- search, analyze, calculate, summarize\n\nTask: Plan microservices for e-commerce\n\nTool 'search' returned: 9 typical services...\nTool 'analy" save as result
// expected: {"action": "finish", "result": "E-Commerce Microservices Architecture:\n\n**Core Services (Phase 1):**\n1. User Service - auth, profiles, addresses\n2. Product Catalog - items, categories, pricing\n3. Cart Service - shopping cart, sessions\n4. Order Service - order lifecycle, history\n5. Payment Service - transactions, refunds\n\n**Supporting Services (Phase 2):**\n6. Inventory Service - stock tracking\n7. Notification Service - email, SMS, push\n8. Search Service - product search, filters\n9. Recommendation Service - personalized suggestions\n\n**Infrastructure:**\n- API Gateway for routing\n- Event bus for inter-service communication\n- Per-service databases\n- Health monitoring agent\n\nEach service in NC: ~30-50 lines of plain English code."}

// === Pair 39 ===
// PROMPT: You are an AI agent. You help with NC code.

Available tools:
- search(query): Search NC stdlib

Tas...
// RESPONSE: {"action": "search", "input": {"query": "NC string functions stdlib upper lower split join replace contains trim"}}

// ask AI to "You are an AI agent. You help with NC code.\n\nAvailable tools:\n- search(query): Search NC stdlib\n\nTask: What string functions does NC have?" save as result
// expected: {"action": "search", "input": {"query": "NC string functions stdlib upper lower split join replace contains trim"}}

// === Pair 40 ===
// PROMPT: You are an AI agent. NC helper.

Available tools:
- search(query): Search

Task: What string functio...
// RESPONSE: {"action": "finish", "result": "NC String Functions:\n\n**Case:** upper(), lower()\n**Whitespace:** trim()\n**Split/Join:** split(delimiter), join(list, delimiter)\n**Search:** contains(), starts_with(), ends_with(), index_of()\n**Transform:** replace(old, new), substring(start, end), reverse()\n**Info:** len(), char_at(index)\n**Padding:** pad_left(width), pad_right(width)\n**Conversion:** str() - convert any value to string\n\nExample:\nset name to \"  Hello World  \"\nset clean to trim(name)\nset words to split(clean, \" \")\nshow upper(words[0])  // HELLO"}

// ask AI to "You are an AI agent. NC helper.\n\nAvailable tools:\n- search(query): Search\n\nTask: What string functions does NC have?\n\nTool 'search' returned: NC string functions: upper(), lower(), trim(), spli" save as result
// expected: {"action": "finish", "result": "NC String Functions:\n\n**Case:** upper(), lower()\n**Whitespace:** trim()\n**Split/Join:** split(delimiter), join(list, delimiter)\n**Search:** contains(), starts_with(), ends_with(), index_of()\n**Transform:** replace(old, new), substring(start, end), reverse()\n**Info:** len(), char_at(index)\n**Padding:** pad_left(width), pad_right(width)\n**Conversion:** str() - convert any value to string\n\nExample:\nset name to \"  Hello World  \"\nset clean to trim(name)\nset words to split(clean, \" \")\nshow upper(words[0])  // HELLO"}

