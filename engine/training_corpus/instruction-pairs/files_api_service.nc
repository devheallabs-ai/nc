<|begin|>
// Description: a file upload and storage service with metadata management (API only, no frontend)
// Type: service
service "files-api"
version "1.0.0"

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
    respond with {"status": "ok"}
<|end|>
