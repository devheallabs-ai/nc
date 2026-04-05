# Smart Notes

An AI-powered notes application built with NC. Notes are automatically tagged, categorized, and searchable using both text and semantic matching.

## Features

- Create, read, update, and delete notes
- AI auto-tagging: notes are automatically assigned 3-5 keyword tags on creation
- AI auto-categorization: notes are classified into categories (work, personal, idea, reference, journal, meeting, todo)
- Semantic search: find notes by meaning, not just keywords
- Note summarization: get concise summaries of individual notes or all notes
- Category overview and statistics
- File-based JSON persistence

## How to Run

```bash
nc serve smart_notes.nc
```

Notes are stored in `data/notes.json` and persist across restarts.

## API Endpoints

### Notes CRUD

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/notes` | Create a note (auto-tagged by AI) |
| `GET` | `/notes` | List all notes (supports `?category=` and `?tag=` filters) |
| `GET` | `/notes/:id` | Get a specific note |
| `PUT` | `/notes/:id` | Update a note (re-tagged by AI) |
| `DELETE` | `/notes/:id` | Delete a note |

### AI Features

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/search` | Semantic search across all notes |
| `GET` | `/notes/:id/summary` | AI summary of a specific note |
| `GET` | `/summarize` | AI summary of all notes grouped by category |

### Organization

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/categories` | List all categories with note counts |

## Example Usage

**Create a note:**
```json
POST /notes
{
  "title": "Sprint Planning Notes",
  "content": "Discussed Q2 roadmap. Priority items: auth system overhaul, dashboard redesign, API rate limiting. Team capacity is 40 story points. John takes auth, Sarah takes dashboard."
}
```

Response (AI auto-tags and categorizes):
```json
{
  "id": 1,
  "title": "Sprint Planning Notes",
  "content": "...",
  "tags": ["sprint", "planning", "roadmap", "q2", "team"],
  "category": "meeting",
  "created_at": "2026-03-22T10:00:00Z"
}
```

**Semantic search:**
```json
POST /search
{
  "query": "what are the team priorities this quarter?"
}
```

This finds relevant notes even if they do not contain the exact words in the query.

**Filter by category:**
```
GET /notes?category=meeting
```

**Filter by tag:**
```
GET /notes?tag=roadmap
```

**Summarize everything:**
```
GET /summarize
```

## curl Examples

**Create a note:**
```bash
curl -X POST http://localhost:8080/notes \
  -H "Content-Type: application/json" \
  -d '{"title": "Sprint Planning Notes", "content": "Discussed Q2 roadmap. Priority items: auth system overhaul, dashboard redesign."}'
```

**List all notes:**
```bash
curl http://localhost:8080/notes
```

**Filter by category:**
```bash
curl "http://localhost:8080/notes?category=meeting"
```

**Filter by tag:**
```bash
curl "http://localhost:8080/notes?tag=roadmap"
```

**Get a specific note:**
```bash
curl http://localhost:8080/notes/1
```

**Update a note:**
```bash
curl -X PUT http://localhost:8080/notes/1 \
  -H "Content-Type: application/json" \
  -d '{"title": "Updated Notes", "content": "Updated content here."}'
```

**Delete a note:**
```bash
curl -X DELETE http://localhost:8080/notes/1
```

**Semantic search:**
```bash
curl -X POST http://localhost:8080/search \
  -H "Content-Type: application/json" \
  -d '{"query": "what are the team priorities this quarter?"}'
```

**Summarize a note:**
```bash
curl http://localhost:8080/notes/1/summary
```

**Summarize all notes:**
```bash
curl http://localhost:8080/summarize
```

**List categories:**
```bash
curl http://localhost:8080/categories
```

## Categories

Notes are automatically classified into one of:
- `work` - Work-related tasks and projects
- `personal` - Personal notes and reminders
- `idea` - Creative ideas and brainstorming
- `reference` - Reference material and documentation
- `journal` - Daily reflections and logs
- `meeting` - Meeting notes and action items
- `todo` - Task lists and to-dos
