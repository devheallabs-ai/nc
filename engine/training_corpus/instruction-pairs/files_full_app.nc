<|begin|>
// Description: a file upload and storage service with metadata management
// Type: full app
service "files"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to upload file with file and user_id:
    set file_id to generate_id()
    set path to save_file(file, file_id)
    set meta to {"id": file_id, "user_id": user_id, "name": file.name, "size": file.size, "type": file.type, "path": path, "created_at": now()}
    set files to load("files.json")
    add meta to files
    save files to "files.json"
    respond with meta

to list files with user_id:
    set files to load("files.json")
    set user_files to filter(files, "user_id", user_id)
    respond with user_files

to get file with id:
    set files to load("files.json")
    set file to find_by(files, "id", id)
    if file is empty:
        respond with error "File not found" status 404
    respond with file

to delete file with id:
    set files to load("files.json")
    set index to find_index(files, id)
    set removed to files[index]
    delete_file(removed.path)
    remove removed from files
    save files to "files.json"
    respond with {"deleted": true}

api:
    POST /files runs upload_file
    GET /files runs list_files
    GET /files/:id runs get_file
    DELETE /files/:id runs delete_file
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "files"}

// === NC_FILE_SEPARATOR ===

page "Files"
title "Files | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "upload":
    heading "File Storage"
    form action "/files" method POST enctype multipart:
        input "file" type file accept "*/*"
        button "Upload" type submit style primary

section "files":
    list from "/files" as files:
        card:
            icon files.type
            text files.name
            text files.size
            link "Download" to "/files/{{files.id}}"
            button "Delete" action "deleteFile" style danger

// === NC_AGENT_SEPARATOR ===

// Files AI Agent
service "files-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to describe file with file_id:
    set file to load("files.json", file_id)
    ask AI to "Describe what this file might contain based on: name={{file.name}}, type={{file.type}}, size={{file.size}}" save as description
    respond with {"description": description}

to handle with prompt:
    purpose: "Handle user request for files"
    ask AI to "You are a helpful files assistant. {{prompt}}" save as response
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
