<|begin|>
// Description: a voting and poll system with real-time results
// Type: full app
service "polls"
version "1.0.0"

to create poll with data:
    set data.id to generate_id()
    set data.created_at to now()
    set data.votes to {}
    set polls to load("polls.json")
    add data to polls
    save polls to "polls.json"
    respond with data

to list polls:
    set polls to load("polls.json")
    respond with polls

to get poll with id:
    set poll to load("polls.json", id)
    respond with poll

to vote with poll_id and option and user_id:
    set polls to load("polls.json")
    set index to find_index(polls, poll_id)
    if polls[index].votes[user_id] is not empty:
        respond with error "Already voted" status 409
    set polls[index].votes[user_id] to option
    save polls to "polls.json"
    respond with polls[index]

to get results with poll_id:
    set poll to load("polls.json", poll_id)
    set results to count_votes(poll.votes)
    respond with results

api:
    POST /polls runs create_poll
    GET /polls runs list_polls
    GET /polls/:id runs get_poll
    POST /polls/:id/vote runs vote
    GET /polls/:id/results runs get_results
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "polls"}

// === NC_FILE_SEPARATOR ===

page "Polls"
title "Polls | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "polls":
    heading "Polls & Voting"
    button "Create Poll" action "openCreateForm" style primary

    list from "/polls" as polls:
        card:
            heading polls.question
            list polls.options as option:
                button option action "vote" style option
            link "View Results" to "/polls/{{polls.id}}/results"

section "create-poll":
    form action "/polls" method POST:
        input "question" placeholder "Ask your question..."
        input "options" placeholder "Option 1, Option 2, Option 3"
        button "Create Poll" type submit style primary

// === NC_AGENT_SEPARATOR ===

// Polls AI Agent
service "polls-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to generate poll question with topic:
    ask AI to "Generate 5 interesting poll questions about: {{topic}}" save as questions
    respond with {"questions": questions}

to analyze results with results:
    ask AI to "Analyze these poll results and give insights: {{results}}" save as insights
    respond with {"insights": insights}

to handle with prompt:
    purpose: "Handle user request for polls"
    ask AI to "You are a helpful polls assistant. {{prompt}}" save as response
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
