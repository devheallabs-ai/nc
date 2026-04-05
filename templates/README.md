# NC Templates — Clone and Deploy

Production-ready AI API templates. Each is a single file you can deploy immediately.

## Quick Start

```bash
# 1. Pick a template
cp templates/ticket-classifier.nc my-service.nc

# 2. Set your AI key
export NC_AI_KEY="sk-your-key-here"

# 3. Deploy
nc serve my-service.nc
```

Or use `nc init`:

```bash
nc init my-service
# Creates my-service/ with service.nc, .env.example, and README.md
```

## Templates

| Template | What it does | Lines |
|---|---|---|
| `ticket-classifier.nc` | Classify support tickets via AI | 28 |
| `chat-api.nc` | Conversational chatbot API | 27 |
| `webhook-processor.nc` | Process GitHub/Stripe webhooks | 43 |
| `content-moderator.nc` | AI-powered content safety screening | 40 |
| `data-enrichment.nc` | Enrich records with AI-generated insights | 42 |

## What's Included

Every template comes production-ready with:
- Health check endpoint (`GET /health`)
- Rate limiting
- CORS support
- Request logging
- Proper error handling
- Environment-based configuration (no hardcoded keys)

## Customizing

1. Change the AI prompt to match your use case
2. Add more routes in the `api:` block
3. Add `auth: "bearer"` to `middleware:` for JWT authentication
4. Set `NC_CORS_ORIGIN` for production CORS restrictions
