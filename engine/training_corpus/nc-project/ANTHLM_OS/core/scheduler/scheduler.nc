// ══════════════════════════════════════════════════════════════════
//  HiveANT — Swarm Scheduler
//
//  Distributes tasks to agents using priority queues.
//  Tracks task assignments, deadlines, retries, and load balancing.
// ══════════════════════════════════════════════════════════════════

to scheduler_init:
    purpose: "Initialize the scheduler subsystem"
    shell("mkdir -p agents_state/queue")
    set sched to {
        "initialized_at": time_now(),
        "tasks_queued": 0,
        "tasks_dispatched": 0,
        "tasks_completed": 0,
        "tasks_failed": 0,
        "max_retries": 3,
        "queue": []
    }
    write_file("agents_state/scheduler.json", json_encode(sched))
    log "SCHEDULER: Initialized"
    respond with sched

to enqueue_task with task_id, task_type, priority, payload, assigned_agent:
    purpose: "Add a task to the scheduler queue"
    if file_exists("agents_state/scheduler.json") is not equal true:
        respond with {"error": "Scheduler not initialized"}

    set sched to json_decode(read_file("agents_state/scheduler.json"))

    if task_id:
        set tid to task_id
    otherwise:
        set tid to task_type + "-" + str(floor(random() * 90000 + 10000))

    if priority:
        set p to priority
    otherwise:
        set p to 5

    set task to {
        "task_id": tid,
        "type": task_type,
        "priority": p,
        "status": "queued",
        "payload": payload,
        "assigned_agent": assigned_agent,
        "created_at": time_now(),
        "retries": 0,
        "result": ""
    }

    write_file("agents_state/queue/" + tid + ".json", json_encode(task))
    set sched.tasks_queued to sched.tasks_queued + 1
    write_file("agents_state/scheduler.json", json_encode(sched))

    log "SCHEDULER: Queued task " + tid + " (type=" + task_type + ", priority=" + str(p) + ")"
    respond with task

to dispatch_tasks:
    purpose: "Dispatch queued tasks to available agents"
    set queued to shell("ls agents_state/queue/*.json 2>/dev/null || echo NONE")
    if queued is equal "NONE":
        respond with {"dispatched": 0, "message": "No tasks in queue"}

    set dispatched to 0
    set files to split(trim(queued), "\n")
    repeat for each f in files:
        set task to json_decode(read_file(f))
        if task.status is equal "queued":
            if task.assigned_agent:
                set agent_path to "agents_state/agent-" + task.assigned_agent + ".json"
                if file_exists(agent_path):
                    set agent to json_decode(read_file(agent_path))
                    if agent.status is equal "running":
                        set task.status to "dispatched"
                        set task.dispatched_at to time_now()
                        write_file(f, json_encode(task))
                        set dispatched to dispatched + 1
            otherwise:
                set task.status to "dispatched"
                set task.dispatched_at to time_now()
                write_file(f, json_encode(task))
                set dispatched to dispatched + 1

    set sched to json_decode(read_file("agents_state/scheduler.json"))
    set sched.tasks_dispatched to sched.tasks_dispatched + dispatched
    write_file("agents_state/scheduler.json", json_encode(sched))

    log "SCHEDULER: Dispatched " + str(dispatched) + " tasks"
    respond with {"dispatched": dispatched}

to complete_task with task_id, result, success:
    purpose: "Mark a task as completed or failed"
    set path to "agents_state/queue/" + task_id + ".json"
    if file_exists(path):
        set task to json_decode(read_file(path))
        set sched to json_decode(read_file("agents_state/scheduler.json"))
        if success is equal "yes":
            set task.status to "completed"
            set task.result to result
            set task.completed_at to time_now()
            set sched.tasks_completed to sched.tasks_completed + 1
        otherwise:
            set task.retries to task.retries + 1
            if task.retries is above sched.max_retries:
                set task.status to "failed"
                set task.result to result
                set sched.tasks_failed to sched.tasks_failed + 1
            otherwise:
                set task.status to "queued"
                log "SCHEDULER: Retrying task " + task_id + " (attempt " + str(task.retries) + ")"
        write_file(path, json_encode(task))
        write_file("agents_state/scheduler.json", json_encode(sched))
        respond with task
    otherwise:
        respond with {"error": "Task not found", "_status": 404}

to scheduler_status:
    purpose: "Get scheduler statistics"
    if file_exists("agents_state/scheduler.json"):
        set sched to json_decode(read_file("agents_state/scheduler.json"))
        respond with sched
    otherwise:
        respond with {"error": "Scheduler not initialized"}
