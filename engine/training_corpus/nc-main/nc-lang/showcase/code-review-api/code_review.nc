service "CodeReview"

set supported_languages to ["python", "javascript", "java", "go", "c", "nc", "ruby", "rust", "typescript"]
set review_count to 0

to review_code with body:
    set code to body["code"]
    set language to body["language"] or "unknown"
    set focus to body["focus"] or "all"
    if len(trim(code)) == 0:
        respond with {"error": "No code provided"}
    set review_count to review_count + 1
    log "Review #" + review_count + " - Language: " + language + " - Focus: " + focus
    set prompt to "You are an expert code reviewer. Review the following " + language + " code.\n\n"
    if focus == "bugs":
        set prompt to prompt + "Focus specifically on bugs, logic errors, and potential runtime issues.\n\n"
    if focus == "security":
        set prompt to prompt + "Focus specifically on security vulnerabilities, injection risks, and unsafe patterns.\n\n"
    if focus == "style":
        set prompt to prompt + "Focus specifically on code style, naming conventions, and readability.\n\n"
    if focus == "performance":
        set prompt to prompt + "Focus specifically on performance issues, inefficiencies, and optimization opportunities.\n\n"
    if focus == "all":
        set prompt to prompt + "Analyze for: bugs, security vulnerabilities, style issues, and performance.\n\n"
    set prompt to prompt + "Return your review as structured JSON with this format:\n"
    set prompt to prompt + "{ \"score\": <1-10>, \"issues\": [{\"severity\": \"critical|warning|info\", \"line\": <approx line>, \"category\": \"bug|security|style|performance\", \"message\": \"description\", \"suggestion\": \"fix\"}], \"summary\": \"brief overall assessment\" }\n\n"
    set prompt to prompt + "Code:\n```" + language + "\n" + code + "\n```"
    ask AI to prompt using code save as review
    set result to {}
    set result["review_id"] to review_count
    set result["language"] to language
    set result["focus"] to focus
    set result["analysis"] to review
    set result["timestamp"] to time_now()
    respond with result

to review_file with body:
    set file_path to body["file"]
    set language to body["language"] or "unknown"
    try:
        set code to read_file(file_path)
        set result to review_code({"code": code, "language": language, "focus": body["focus"] or "all"})
        set result["file"] to file_path
        respond with result
    on error:
        respond with {"error": "Could not read file: " + file_path}

to explain_code with body:
    set code to body["code"]
    set language to body["language"] or "unknown"
    if len(trim(code)) == 0:
        respond with {"error": "No code provided"}
    set prompt to "Explain this " + language + " code in plain English. Break it down step by step. Mention what it does, how it works, and any notable patterns or techniques used:\n\n```" + language + "\n" + code + "\n```"
    ask AI to prompt using code save as explanation
    set result to {}
    set result["language"] to language
    set result["explanation"] to explanation
    respond with result

to suggest_improvements with body:
    set code to body["code"]
    set language to body["language"] or "unknown"
    set goal to body["goal"] or "general improvement"
    if len(trim(code)) == 0:
        respond with {"error": "No code provided"}
    set prompt to "You are a senior developer. Refactor and improve this " + language + " code.\n"
    set prompt to prompt + "Goal: " + goal + "\n\n"
    set prompt to prompt + "Provide the improved code with explanations of what changed and why.\n\n"
    set prompt to prompt + "```" + language + "\n" + code + "\n```"
    ask AI to prompt using code save as improved
    set result to {}
    set result["language"] to language
    set result["goal"] to goal
    set result["improvements"] to improved
    respond with result

to compare_approaches with body:
    set code_a to body["code_a"]
    set code_b to body["code_b"]
    set language to body["language"] or "unknown"
    set prompt to "Compare these two " + language + " code approaches. Evaluate readability, performance, maintainability, and correctness. Recommend which is better and why.\n\n"
    set prompt to prompt + "Approach A:\n```" + language + "\n" + code_a + "\n```\n\n"
    set prompt to prompt + "Approach B:\n```" + language + "\n" + code_b + "\n```"
    ask AI to prompt using code_a save as comparison
    set result to {}
    set result["language"] to language
    set result["comparison"] to comparison
    respond with result

to get_languages:
    respond with {"supported_languages": supported_languages}

to get_stats:
    set stats to {}
    set stats["total_reviews"] to review_count
    set stats["supported_languages"] to supported_languages
    respond with stats

api:
    POST /review runs review_code
    POST /review/file runs review_file
    POST /explain runs explain_code
    POST /improve runs suggest_improvements
    POST /compare runs compare_approaches
    GET /languages runs get_languages
    GET /stats runs get_stats
