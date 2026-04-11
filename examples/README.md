# NC Examples

Each example is a complete, working AI-powered API. Run any of them with:

```bash
NC_AI_KEY=your-api-key nc serve examples/01_hello_ai.nc
```

Then test with curl:

```bash
curl -X POST http://localhost:8000/hello -H "Content-Type: application/json" -d '{"name": "World"}'
```

## Examples

| # | File | What it does | Lines |
|---|------|-------------|-------|
| 01 | `01_hello_ai.nc` | AI-powered greeting API | 8 |
| 02 | `02_ticket_classifier.nc` | Classify support tickets into categories | 9 |
| 03 | `03_content_moderator.nc` | Content toxicity analysis | 9 |
| 04 | `04_summarizer.nc` | Document summarization with configurable length | 15 |
| 05 | `05_multi_provider.nc` | Same code, any AI provider — switch via config | 16 |

## The Point

Each example replaces 30-50 lines of Python + Flask + AI SDK with under 20 lines of NC.

No framework. No SDK. No package manager. No Docker. One file. One binary.
