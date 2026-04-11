<|begin|>
// Description: a note-taking app with AI summarization and search
// Type: full app
service "notes"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to list notes with user_id:
    set notes to load("notes.json")
    set my_notes to filter(notes, "user_id", user_id)
    respond with my_notes

to create note with user_id and data:
    set data.id to generate_id()
    set data.user_id to user_id
    set data.created_at to now()
    set notes to load("notes.json")
    add data to notes
    save notes to "notes.json"
    respond with data

to update note with id and data:
    set notes to load("notes.json")
    set index to find_index(notes, id)
    merge data into notes[index]
    set notes[index].updated_at to now()
    save notes to "notes.json"
    respond with notes[index]

to delete note with id:
    set notes to load("notes.json")
    set index to find_index(notes, id)
    set removed to notes[index]
    remove removed from notes
    save notes to "notes.json"
    respond with {"deleted": true}

to search notes with query and user_id:
    set notes to load("notes.json")
    set my_notes to filter(notes, "user_id", user_id)
    set results to search(my_notes, query)
    respond with results

api:
    GET /notes runs list_notes
    POST /notes runs create_note
    PUT /notes/:id runs update_note
    DELETE /notes/:id runs delete_note
    GET /notes/search runs search_notes
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "notes"}

// === NC_FILE_SEPARATOR ===

page "Notes"
title "Notes | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "notes":
    heading "My Notes"
    button "New Note" action "createNote" style primary
    input "search" placeholder "Search notes..." action "searchNotes"

    grid from "/notes" as notes columns 3:
        card:
            heading notes.title
            text notes.content style truncate
            text notes.updated_at style meta
            button "Edit" action "editNote" style secondary
            button "Delete" action "deleteNote" style danger
            button "AI Summary" action "summarizeNote" style ai

section "editor":
    form action "/notes" method POST:
        input "title" placeholder "Note title"
        input "content" type textarea placeholder "Start writing..."
        button "Save Note" type submit style primary

// === NC_AGENT_SEPARATOR ===

// Notes AI Agent
service "notes-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to summarize note with content:
    ask AI to "Summarize this note in 3 bullet points: {{content}}" save as summary
    respond with {"summary": summary}

to enhance note with content:
    ask AI to "Improve and expand on this note: {{content}}" save as enhanced
    respond with {"enhanced": enhanced}

to extract action items with content:
    ask AI to "Extract action items from this note: {{content}}" save as actions
    respond with {"actions": actions}

to handle with prompt:
    purpose: "Handle user request for notes"
    ask AI to "You are a helpful notes assistant. {{prompt}}" save as response
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
