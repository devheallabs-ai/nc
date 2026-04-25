# Code Review API

An automated AI-powered code review service built with NC. Submit code and receive structured feedback on bugs, security, style, and performance.

## Features

- AI-driven code review with severity ratings
- Focused review modes: bugs, security, style, performance, or all
- Code explanation in plain English
- AI-suggested improvements and refactoring
- Side-by-side comparison of two code approaches
- Support for multiple programming languages
- Review from file path

## How to Run

```bash
nc serve code_review.nc
```

The service starts on port 8080 by default.

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/review` | Submit code for AI review |
| `POST` | `/review/file` | Review code from a file path |
| `POST` | `/explain` | Get a plain-English explanation of code |
| `POST` | `/improve` | Get AI-suggested improvements |
| `POST` | `/compare` | Compare two code approaches |
| `GET` | `/languages` | List supported languages |
| `GET` | `/stats` | Get review statistics |

## Example Usage

**Review code:**
```json
POST /review
{
  "code": "def login(user, pw):\n    query = 'SELECT * FROM users WHERE name=' + user\n    return db.execute(query)",
  "language": "python",
  "focus": "security"
}
```

Response:
```json
{
  "review_id": 1,
  "language": "python",
  "focus": "security",
  "analysis": {
    "score": 2,
    "issues": [
      {
        "severity": "critical",
        "line": 2,
        "category": "security",
        "message": "SQL injection vulnerability via string concatenation",
        "suggestion": "Use parameterized queries: db.execute('SELECT * FROM users WHERE name=?', (user,))"
      }
    ],
    "summary": "Critical security flaw. This code is vulnerable to SQL injection."
  }
}
```

**Get improvements:**
```json
POST /improve
{
  "code": "for i in range(len(items)):\n    print(items[i])",
  "language": "python",
  "goal": "make more pythonic"
}
```

**Compare two approaches:**
```json
POST /compare
{
  "code_a": "result = [x for x in items if x > 0]",
  "code_b": "result = list(filter(lambda x: x > 0, items))",
  "language": "python"
}
```

## curl Examples

**Review code for security issues:**
```bash
curl -X POST http://localhost:8080/review \
  -H "Content-Type: application/json" \
  -d '{"code": "def login(user, pw):\n    query = \"SELECT * FROM users WHERE name=\" + user\n    return db.execute(query)", "language": "python", "focus": "security"}'
```

**Explain code:**
```bash
curl -X POST http://localhost:8080/explain \
  -H "Content-Type: application/json" \
  -d '{"code": "result = [x for x in items if x > 0]", "language": "python"}'
```

**Get improvements:**
```bash
curl -X POST http://localhost:8080/improve \
  -H "Content-Type: application/json" \
  -d '{"code": "for i in range(len(items)):\n    print(items[i])", "language": "python", "goal": "make more pythonic"}'
```

**Compare two approaches:**
```bash
curl -X POST http://localhost:8080/compare \
  -H "Content-Type: application/json" \
  -d '{"code_a": "result = [x for x in items if x > 0]", "code_b": "result = list(filter(lambda x: x > 0, items))", "language": "python"}'
```

**List supported languages:**
```bash
curl http://localhost:8080/languages
```

**Get review stats:**
```bash
curl http://localhost:8080/stats
```

## Focus Modes

- `all` - Comprehensive review (default)
- `bugs` - Logic errors and runtime issues
- `security` - Vulnerabilities and unsafe patterns
- `style` - Naming, readability, conventions
- `performance` - Inefficiencies and optimization
