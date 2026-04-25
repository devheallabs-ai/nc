service "TaskManager"

set tasks to []
set next_id to 1
set data_file to "data/tasks.json"

to load_tasks:
    try:
        set raw to read_file(data_file)
        set tasks to json_decode(raw)
        if len(tasks) > 0:
            set ids to []
            repeat for each task in tasks:
                append task["id"] to ids
            set next_id to max(ids) + 1
        log "Loaded " + len(tasks) + " tasks from disk"
    on error:
        set tasks to []
        set next_id to 1
        log "Starting with empty task list"

to save_tasks:
    set raw to json_encode(tasks)
    write_file(data_file, raw)
    log "Saved " + len(tasks) + " tasks to disk"

to create_task with body:
    set task to {}
    set task["id"] to next_id
    set next_id to next_id + 1
    set task["title"] to body["title"]
    set task["description"] to body["description"] or ""
    set task["priority"] to body["priority"] or "medium"
    set task["status"] to "todo"
    set task["due_date"] to body["due_date"] or "none"
    set task["created_at"] to time_now()
    set task["updated_at"] to time_now()
    set task["tags"] to body["tags"] or []
    append task to tasks
    save_tasks
    log "Created task #" + task["id"] + ": " + task["title"]
    respond with task

to list_tasks with query:
    set result to tasks
    if query["status"]:
        set filtered to []
        repeat for each task in result:
            if task["status"] == query["status"]:
                append task to filtered
        set result to filtered
    if query["priority"]:
        set filtered to []
        repeat for each task in result:
            if task["priority"] == query["priority"]:
                append task to filtered
        set result to filtered
    set summary to {}
    set summary["total"] to len(result)
    set summary["tasks"] to result
    respond with summary

to get_task with params:
    set target_id to params["id"]
    repeat for each task in tasks:
        if task["id"] == target_id:
            respond with task
    respond with {"error": "Task not found", "id": target_id}

to update_task with params:
    set target_id to params["id"]
    set updates to params["body"]
    repeat for each task in tasks:
        if task["id"] == target_id:
            if updates["title"]:
                set task["title"] to updates["title"]
            if updates["description"]:
                set task["description"] to updates["description"]
            if updates["priority"]:
                set task["priority"] to updates["priority"]
            if updates["status"]:
                set task["status"] to updates["status"]
            if updates["due_date"]:
                set task["due_date"] to updates["due_date"]
            if updates["tags"]:
                set task["tags"] to updates["tags"]
            set task["updated_at"] to time_now()
            save_tasks
            log "Updated task #" + task["id"]
            respond with task
    respond with {"error": "Task not found", "id": target_id}

to delete_task with params:
    set target_id to params["id"]
    set new_tasks to []
    set found to false
    repeat for each task in tasks:
        if task["id"] == target_id:
            set found to true
            log "Deleted task #" + task["id"] + ": " + task["title"]
        otherwise:
            append task to new_tasks
    if found:
        set tasks to new_tasks
        save_tasks
        respond with {"message": "Task deleted", "id": target_id}
    respond with {"error": "Task not found", "id": target_id}

to summarize_tasks:
    if len(tasks) == 0:
        respond with {"summary": "No tasks found."}
    set task_text to ""
    repeat for each task in tasks:
        set task_text to task_text + "- [" + task["status"] + "] " + task["title"] + " (priority: " + task["priority"] + ", due: " + task["due_date"] + ")\n"
    ask AI to "Summarize these tasks and highlight what needs immediate attention. Group by priority and status:\n" + task_text using task_text save as summary
    respond with {"summary": summary, "task_count": len(tasks)}

to prioritize_tasks:
    if len(tasks) == 0:
        respond with {"recommendation": "No tasks to prioritize."}
    set task_text to ""
    repeat for each task in tasks:
        set task_text to task_text + "- " + task["title"] + " | priority: " + task["priority"] + " | status: " + task["status"] + " | due: " + task["due_date"] + " | description: " + task["description"] + "\n"
    ask AI to "Analyze these tasks and suggest an optimal order to work on them. Consider deadlines, priorities, and dependencies. Provide a ranked list with reasoning:\n" + task_text using task_text save as recommendation
    respond with {"recommendation": recommendation, "task_count": len(tasks)}

to get_stats:
    set todo_count to 0
    set in_progress_count to 0
    set done_count to 0
    set high_count to 0
    set medium_count to 0
    set low_count to 0
    repeat for each task in tasks:
        if task["status"] == "todo":
            set todo_count to todo_count + 1
        if task["status"] == "in_progress":
            set in_progress_count to in_progress_count + 1
        if task["status"] == "done":
            set done_count to done_count + 1
        if task["priority"] == "high":
            set high_count to high_count + 1
        if task["priority"] == "medium":
            set medium_count to medium_count + 1
        if task["priority"] == "low":
            set low_count to low_count + 1
    set stats to {}
    set stats["total"] to len(tasks)
    set stats["by_status"] to {"todo": todo_count, "in_progress": in_progress_count, "done": done_count}
    set stats["by_priority"] to {"high": high_count, "medium": medium_count, "low": low_count}
    respond with stats

load_tasks

api:
    POST /tasks runs create_task
    GET /tasks runs list_tasks
    GET /tasks/:id runs get_task
    PUT /tasks/:id runs update_task
    DELETE /tasks/:id runs delete_task
    GET /tasks/summarize runs summarize_tasks
    GET /tasks/prioritize runs prioritize_tasks
    GET /stats runs get_stats
