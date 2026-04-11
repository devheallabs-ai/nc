<|begin|>
// Description: a todo app with task management and AI assistance (API only, no frontend)
// Type: service
service "todo-api"
version "1.0.0"

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
    respond with {"status": "ok"}
<|end|>
