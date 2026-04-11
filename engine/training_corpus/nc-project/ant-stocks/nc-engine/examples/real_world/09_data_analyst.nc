// ═══════════════════════════════════════════════════════════
//  AI Data Analyst
//
//  Replaces: 150+ lines of Python with data processing + LLM analysis
//
//  Upload data files, ask questions, get insights.
//
//  curl -X POST http://localhost:8000/analyze \
//    -d '{"file": "sales.csv", "question": "What was the best month?"}'
// ═══════════════════════════════════════════════════════════

service "data-analyst"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"

to analyze with file, question:
    purpose: "Answer questions about data files"

    set data to read_file(file)

    if type(data) is equal "none":
        respond with {"error": "Cannot read file", "file": file}

    ask AI to "You are a data analyst. Analyze this data and answer the question.\n\nData:\n{{data}}\n\nQuestion: {{question}}\n\nProvide a clear answer with specific numbers from the data." save as answer

    respond with {"answer": answer, "file": file, "question": question}

to insights with file:
    purpose: "Auto-generate insights from a data file"

    set data to read_file(file)

    ask AI to "Analyze this dataset and provide insights. Return JSON with: summary (what this data is about), row_estimate (approximate number of rows), columns (list of column names found), key_metrics (list of important numbers), trends (list of patterns), anomalies (list of unusual findings), recommendations (list of actions to take).\n\nData:\n{{data}}" save as analysis

    respond with analysis

to sql_query with file, natural_query:
    purpose: "Convert natural language to SQL, then answer"

    set data to read_file(file)

    ask AI to "Given this CSV data, the user wants to know: {{natural_query}}\n\nFirst, explain what SQL query would answer this (if it were in a database). Then actually answer the question by analyzing the data directly.\n\nReturn JSON with: sql_equivalent (string), answer (string), data_points (list of relevant values).\n\nData:\n{{data}}" save as result

    respond with result

to dashboard with file:
    purpose: "Generate a text-based dashboard from data"

    set data to read_file(file)

    ask AI to "Create a text-based dashboard for this data. Include:\n- Key Performance Indicators (top 5 metrics)\n- Trend summary\n- Alert section (anything concerning)\n- Recommendation section\n\nFormat it clearly with headers and bullet points.\n\nData:\n{{data}}" save as dashboard

    respond with dashboard

api:
    POST /analyze runs analyze
    POST /insights runs insights
    POST /query runs sql_query
    POST /dashboard runs dashboard
