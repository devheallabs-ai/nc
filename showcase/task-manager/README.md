# Task Manager API

A task and project management API built with NC, featuring AI-powered summarization and prioritization.

## Features

- Full CRUD operations for tasks
- Priority levels: `high`, `medium`, `low`
- Status tracking: `todo`, `in_progress`, `done`
- Due date support
- Tag-based organization
- AI-powered task summarization
- AI-powered priority recommendations
- File-based JSON persistence
- Dashboard statistics

## How to Run

```bash
nc serve task_manager.nc
```

The service starts and loads any existing tasks from `data/tasks.json`.

## API Endpoints

### Tasks

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/tasks` | Create a new task |
| `GET` | `/tasks` | List all tasks (supports `?status=` and `?priority=` filters) |
| `GET` | `/tasks/:id` | Get a specific task |
| `PUT` | `/tasks/:id` | Update a task |
| `DELETE` | `/tasks/:id` | Delete a task |

### AI Features

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/tasks/summarize` | AI summary of all tasks with attention highlights |
| `GET` | `/tasks/prioritize` | AI-recommended task ordering with reasoning |

### Stats

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/stats` | Task counts grouped by status and priority |

## Example Usage

**Create a task:**
```json
POST /tasks
{
  "title": "Build login page",
  "description": "Create OAuth2 login flow with Google and GitHub",
  "priority": "high",
  "due_date": "2026-04-01",
  "tags": ["frontend", "auth"]
}
```

**Update task status:**
```json
PUT /tasks/1
{
  "status": "in_progress"
}
```

**Filter tasks:**
```
GET /tasks?status=todo&priority=high
```

## curl Examples

**Create a task:**
```bash
curl -X POST http://localhost:8080/tasks \
  -H "Content-Type: application/json" \
  -d '{"title": "Build login page", "description": "Create OAuth2 login flow", "priority": "high", "due_date": "2026-04-01", "tags": ["frontend", "auth"]}'
```

**List all tasks:**
```bash
curl http://localhost:8080/tasks
```

**Filter tasks:**
```bash
curl "http://localhost:8080/tasks?status=todo&priority=high"
```

**Get a specific task:**
```bash
curl http://localhost:8080/tasks/1
```

**Update task status:**
```bash
curl -X PUT http://localhost:8080/tasks/1 \
  -H "Content-Type: application/json" \
  -d '{"status": "in_progress"}'
```

**Delete a task:**
```bash
curl -X DELETE http://localhost:8080/tasks/1
```

**AI summary:**
```bash
curl http://localhost:8080/tasks/summarize
```

**AI priority recommendations:**
```bash
curl http://localhost:8080/tasks/prioritize
```

**Dashboard stats:**
```bash
curl http://localhost:8080/stats
```

## Data Storage

Tasks are persisted as JSON in the `data/` directory. The file is automatically created and updated on every write operation.
