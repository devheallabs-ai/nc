<|begin|>
// Description: a job queue system with workers, priorities, and status tracking
// Type: full app
service "job-queue"
version "1.0.0"

to enqueue job with data:
    set job to {"id": generate_id(), "status": "pending", "priority": data.priority or "normal", "payload": data.payload, "created_at": now(), "attempts": 0}
    set jobs to load("jobs.json")
    add job to jobs
    save jobs to "jobs.json"
    respond with job

to dequeue job:
    set jobs to load("jobs.json")
    set pending to filter(jobs, "status", "pending")
    if pending is empty:
        respond with {"job": null}
    set job to pending[0]
    set index to find_index(jobs, job.id)
    set jobs[index].status to "processing"
    set jobs[index].started_at to now()
    save jobs to "jobs.json"
    respond with job

to complete job with id:
    set jobs to load("jobs.json")
    set index to find_index(jobs, id)
    set jobs[index].status to "done"
    set jobs[index].completed_at to now()
    save jobs to "jobs.json"
    respond with jobs[index]

to fail job with id and error:
    set jobs to load("jobs.json")
    set index to find_index(jobs, id)
    set jobs[index].status to "failed"
    set jobs[index].error to error
    set jobs[index].attempts to jobs[index].attempts + 1
    save jobs to "jobs.json"
    respond with jobs[index]

to list jobs with status:
    set jobs to load("jobs.json")
    if status is not empty:
        set jobs to filter(jobs, "status", status)
    respond with jobs

api:
    POST /jobs runs enqueue_job
    GET /jobs/dequeue runs dequeue_job
    PUT /jobs/:id/complete runs complete_job
    PUT /jobs/:id/fail runs fail_job
    GET /jobs runs list_jobs
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "job-queue"}

// === NC_FILE_SEPARATOR ===

page "Job Queue"
title "Job Queue | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "queue":
    heading "Job Queue"

    grid columns 4:
        card:
            heading "Pending"
            text from "/jobs?status=pending" as jobs: jobs.length
        card:
            heading "Processing"
            text from "/jobs?status=processing" as jobs: jobs.length
        card:
            heading "Done"
            text from "/jobs?status=done" as jobs: jobs.length
        card:
            heading "Failed"
            text from "/jobs?status=failed" as jobs: jobs.length

    list from "/jobs" as jobs:
        card:
            text jobs.id style mono
            badge jobs.status
            badge jobs.priority
            text jobs.created_at style meta

// === NC_AGENT_SEPARATOR ===

// Job Queue AI Agent
service "job-queue-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to analyze job failures with failed_jobs:
    ask AI to "Analyze these failed jobs and suggest fixes: {{failed_jobs}}" save as analysis
    respond with {"analysis": analysis}

to handle with prompt:
    purpose: "Handle user request for job-queue"
    ask AI to "You are a helpful job-queue assistant. {{prompt}}" save as response
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
