// ═══════════════════════════════════════════════════════════
//  AI Code Reviewer
//
//  Replaces: 120+ lines of Python with AST parsing + LLM integration
//
//  Reviews code, finds bugs, suggests improvements.
//
//  curl -X POST http://localhost:8000/review \
//    -d '{"code": "def add(a, b): return a + b", "language": "python"}'
// ═══════════════════════════════════════════════════════════

service "code-reviewer"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o"

to review_code with code, language:
    purpose: "Review code for bugs, security issues, and improvements"

    ask AI to "Review this {{language}} code. Return JSON with: bugs (list of objects with line and description), security_issues (list of objects with severity and description), improvements (list of strings), quality_score (1-10), summary (string).\n\nCode:\n{{code}}" save as review

    respond with review

to review_file with file_path:
    purpose: "Review code from a file"

    set code to read_file(file_path)

    if type(code) is equal "none":
        respond with {"error": "File not found", "path": file_path}

    ask AI to "Review this code for bugs, security vulnerabilities, and improvements. Return JSON with: bugs (list), security_issues (list), improvements (list), quality_score (1-10).\n\nFile: {{file_path}}\nCode:\n{{code}}" save as review

    respond with review

to explain_code with code, language:
    purpose: "Explain what code does in plain English"

    ask AI to "Explain this {{language}} code in plain English. What does it do? How does it work? Any non-obvious behavior?\n\nCode:\n{{code}}" save as explanation

    respond with explanation

to generate_tests with code, language, framework:
    purpose: "Generate test cases for code"

    ask AI to "Generate comprehensive test cases for this {{language}} code using {{framework}}. Cover: happy path, edge cases, error cases. Return the test code.\n\nCode:\n{{code}}" save as tests

    respond with {"tests": tests, "language": language, "framework": framework}

to convert_code with code, from_lang, to_lang:
    purpose: "Convert code from one language to another"

    ask AI to "Convert this {{from_lang}} code to {{to_lang}}. Maintain the same functionality. Use idiomatic {{to_lang}} patterns.\n\nCode:\n{{code}}" save as converted

    respond with {"original_language": from_lang, "target_language": to_lang, "converted": converted}

api:
    POST /review runs review_code
    POST /review/file runs review_file
    POST /explain runs explain_code
    POST /tests runs generate_tests
    POST /convert runs convert_code
