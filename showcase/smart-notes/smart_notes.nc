service "SmartNotes"

set notes to []
set next_id to 1
set data_file to "data/notes.json"

to load_notes:
    try:
        set raw to read_file(data_file)
        set notes to json_decode(raw)
        if len(notes) > 0:
            set ids to []
            repeat for each note in notes:
                append note["id"] to ids
            set next_id to max(ids) + 1
        log "Loaded " + len(notes) + " notes from disk"
    on error:
        set notes to []
        set next_id to 1
        log "Starting with empty notes"

to save_notes:
    set raw to json_encode(notes)
    write_file(data_file, raw)

to create_note with body:
    set note to {}
    set note["id"] to next_id
    set next_id to next_id + 1
    set note["title"] to body["title"]
    set note["content"] to body["content"]
    set note["created_at"] to time_now()
    set note["updated_at"] to time_now()
    set note["tags"] to []
    set note["category"] to "uncategorized"

    set tag_prompt to "Analyze this note and return ONLY a JSON object with two fields: \"tags\" (array of 3-5 short keyword tags) and \"category\" (one of: work, personal, idea, reference, journal, meeting, todo). Note title: " + note["title"] + ". Note content: " + note["content"]
    ask AI to tag_prompt using note["content"] save as ai_result

    try:
        set parsed to json_decode(ai_result)
        set note["tags"] to parsed["tags"]
        set note["category"] to parsed["category"]
    on error:
        set note["tags"] to split(lower(note["title"]), " ")
        set note["category"] to "uncategorized"

    append note to notes
    save_notes
    log "Created note #" + note["id"] + ": " + note["title"] + " [" + note["category"] + "]"
    respond with note

to list_notes with query:
    set result to notes
    if query["category"]:
        set filtered to []
        repeat for each note in result:
            if note["category"] == query["category"]:
                append note to filtered
        set result to filtered
    if query["tag"]:
        set filtered to []
        repeat for each note in result:
            if contains(note["tags"], query["tag"]):
                append note to filtered
        set result to filtered
    set summaries to []
    repeat for each note in result:
        set summary to {}
        set summary["id"] to note["id"]
        set summary["title"] to note["title"]
        set summary["category"] to note["category"]
        set summary["tags"] to note["tags"]
        set summary["created_at"] to note["created_at"]
        append summary to summaries
    respond with {"count": len(summaries), "notes": summaries}

to get_note with params:
    set target_id to params["id"]
    repeat for each note in notes:
        if note["id"] == target_id:
            respond with note
    respond with {"error": "Note not found", "id": target_id}

to update_note with params:
    set target_id to params["id"]
    set updates to params["body"]
    repeat for each note in notes:
        if note["id"] == target_id:
            if updates["title"]:
                set note["title"] to updates["title"]
            if updates["content"]:
                set note["content"] to updates["content"]
            set note["updated_at"] to time_now()

            if updates["content"] or updates["title"]:
                set tag_prompt to "Analyze this note and return ONLY a JSON object with \"tags\" (3-5 keyword tags) and \"category\" (work/personal/idea/reference/journal/meeting/todo). Title: " + note["title"] + ". Content: " + note["content"]
                ask AI to tag_prompt using note["content"] save as ai_result
                try:
                    set parsed to json_decode(ai_result)
                    set note["tags"] to parsed["tags"]
                    set note["category"] to parsed["category"]
                on error:
                    log "Could not re-tag note #" + note["id"]

            save_notes
            log "Updated note #" + note["id"]
            respond with note
    respond with {"error": "Note not found", "id": target_id}

to delete_note with params:
    set target_id to params["id"]
    set new_notes to []
    set found to false
    repeat for each note in notes:
        if note["id"] == target_id:
            set found to true
        otherwise:
            append note to new_notes
    if found:
        set notes to new_notes
        save_notes
        respond with {"message": "Note deleted", "id": target_id}
    respond with {"error": "Note not found", "id": target_id}

to search_notes with body:
    set query to body["query"]
    log "Searching notes for: " + query

    set text_matches to []
    repeat for each note in notes:
        if contains(lower(note["title"]), lower(query)):
            append note to text_matches
        otherwise:
            if contains(lower(note["content"]), lower(query)):
                append note to text_matches

    if len(notes) > 0:
        set note_index to ""
        repeat for each note in notes:
            set note_index to note_index + "ID " + note["id"] + ": " + note["title"] + " | Tags: " + join(note["tags"], ", ") + " | Category: " + note["category"] + " | Content preview: " + note["content"] + "\n---\n"
        set search_prompt to "Given this search query: \"" + query + "\"\n\nFind the most relevant notes from this list. Return ONLY a JSON array of note IDs that are semantically relevant, ranked by relevance:\n\n" + note_index
        ask AI to search_prompt using note_index save as ai_result
        try:
            set relevant_ids to json_decode(ai_result)
            set ai_matches to []
            repeat for each rid in relevant_ids:
                repeat for each note in notes:
                    if note["id"] == rid:
                        append note to ai_matches
        on error:
            set ai_matches to []

    set result to {}
    set result["query"] to query
    set result["text_matches"] to len(text_matches)
    set result["semantic_matches"] to len(ai_matches)
    set result["results"] to ai_matches
    respond with result

to summarize_note with params:
    set target_id to params["id"]
    repeat for each note in notes:
        if note["id"] == target_id:
            set prompt to "Summarize this note in 2-3 concise sentences. Capture the key points:\n\nTitle: " + note["title"] + "\n\n" + note["content"]
            ask AI to prompt using note["content"] save as summary
            set result to {}
            set result["id"] to note["id"]
            set result["title"] to note["title"]
            set result["summary"] to summary
            respond with result
    respond with {"error": "Note not found", "id": target_id}

to summarize_all:
    if len(notes) == 0:
        respond with {"summary": "No notes to summarize."}
    set all_text to ""
    repeat for each note in notes:
        set all_text to all_text + "- " + note["title"] + " [" + note["category"] + "]: " + note["content"] + "\n"
    set prompt to "Provide a high-level summary of all these notes. Group by category and highlight key themes, action items, and important information:\n\n" + all_text
    ask AI to prompt using all_text save as summary
    respond with {"note_count": len(notes), "summary": summary}

to get_categories:
    set cats to {}
    repeat for each note in notes:
        set cat to note["category"]
        if cats[cat]:
            set cats[cat] to cats[cat] + 1
        otherwise:
            set cats[cat] to 1
    respond with {"categories": cats, "total_notes": len(notes)}

load_notes

api:
    POST /notes runs create_note
    GET /notes runs list_notes
    GET /notes/:id runs get_note
    PUT /notes/:id runs update_note
    DELETE /notes/:id runs delete_note
    POST /search runs search_notes
    GET /notes/:id/summary runs summarize_note
    GET /summarize runs summarize_all
    GET /categories runs get_categories
