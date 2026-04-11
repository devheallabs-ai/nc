<|begin|>
// Description: a todo app with task management and AI assistance
// Type: full app
service "todo"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to list tasks:
    set tasks to load("tasks.json")
    respond with tasks

to create task with data:
    set data.id to generate_id()
    set data.status to "pending"
    set data.created_at to now()
    set tasks to load("tasks.json")
    add data to tasks
    save tasks to "tasks.json"
    respond with data

to complete task with id:
    set tasks to load("tasks.json")
    set index to find_index(tasks, id)
    if index is -1:
        respond with error "Task not found"
    set tasks[index].status to "done"
    set tasks[index].completed_at to now()
    save tasks to "tasks.json"
    respond with tasks[index]

to delete task with id:
    set tasks to load("tasks.json")
    set index to find_index(tasks, id)
    set removed to tasks[index]
    remove removed from tasks
    save tasks to "tasks.json"
    respond with removed

api:
    GET /tasks runs list_tasks
    POST /tasks runs create_task
    PUT /tasks/:id/complete runs complete_task
    DELETE /tasks/:id runs delete_task
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "todo"}

// === NC_FILE_SEPARATOR ===

page "Todo"
title "Todo | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

nav:
    brand "Todo App"
    link "Tasks" to "/tasks"

section "hero":
    heading "My Tasks"
    text "Stay organized with AI-powered task management"
    button "Add Task" action "createTask" style primary

section "task-list":
    list from "/tasks" as tasks:
        card:
            heading tasks.title
            text tasks.status
            button "Complete" action "completeTask" style success
            button "Delete" action "deleteTask" style danger

section "add-task":
    form action "/tasks" method POST:
        input "title" placeholder "What needs to be done?"
        input "description" placeholder "Details..."
        button "Add Task" type submit style primary

// === NC_AGENT_SEPARATOR ===

// Todo AI Agent
service "todo-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to suggest tasks with context:
    ask AI to "Suggest 5 tasks based on: {{context}}" save as suggestions
    respond with {"suggestions": suggestions}

to prioritize with tasks:
    ask AI to "Prioritize these tasks by urgency: {{tasks}}" save as ordered
    respond with {"ordered": ordered}

to handle with prompt:
    purpose: "Handle user request for todo"
    ask AI to "You are a helpful todo assistant. {{prompt}}" save as response
    respond with {"reply": response}

to classify with input:
    ask AI to "Classify as: create, read, update, delete, help. Input: {{input}}" save as intent
    respond with {"intent": intent}

api:
    POST /agent          runs handle
    POST /agent/classify  runs classify
    GET  /agent/health    runs health_check

to health check:
    respond with {"status": "ok", "ai": "local"}
<|end|>
