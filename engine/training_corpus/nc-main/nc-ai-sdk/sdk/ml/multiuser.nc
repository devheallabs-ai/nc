// ═══════════════════════════════════════════════════════════
//  NC ML — Multi-User SaaS System
//
//  Isolates user data while sharing global knowledge.
//  Each user has their own profile, memory, and feedback.
//
//  Architecture:
//    User Request → User ID → User Profile
//      → Shared Knowledge Base → Reasoning → Response
//
//  Features:
//    - User isolation (separate profiles)
//    - Shared global knowledge graph
//    - Per-user feedback & preferences
//    - User session management
//    - SQLite-ready data model
//    - Bulk user operations
//    - Usage analytics
//
//  Usage:
//    nc nc-ai/nc/ml/multiuser.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-multiuser"
version "1.0.0"

configure:
    max_users is 10000
    session_timeout is 3600
    data_dir is "./nc-ai/data/users"

// ── User Database ─────────────────────────────────────────

set users to {}
set sessions to {}
set global_knowledge to {}
set usage_log to []

// ── Get or Create User ────────────────────────────────────

to get_user with user_id:
    purpose: "Get existing user or create new one"
    if get(users, user_id) is equal nil:
        set users[user_id] to {
            "id": user_id,
            "profile": {
                "topics": {},
                "style": {},
                "history": [],
                "preferences": {}
            },
            "memory": {},
            "feedback": {},
            "created_at": time(),
            "last_active": time(),
            "interaction_count": 0
        }
        log "New user created: " + user_id
    set users[user_id].last_active to time()
    respond with users[user_id]

// ── Update User Profile ───────────────────────────────────

to update_user_profile with user_id, tokens:
    purpose: "Update user's topic preferences from input"
    set user to run get_user with user_id
    repeat for each t in tokens:
        set user.profile.topics[t] to get(user.profile.topics, t, 0) + 1
    // Update history
    append {"tokens": tokens, "time": time()} to user.profile.history
    // Trim history
    if len(user.profile.history) is above 200:
        set user.profile.history to slice(user.profile.history, len(user.profile.history) - 200, 200)
    set user.interaction_count to user.interaction_count + 1
    set users[user_id] to user
    respond with true

// ── Update User Style ─────────────────────────────────────

to update_user_style with user_id, input_text:
    purpose: "Track user's style preference"
    set user to run get_user with user_id
    set t to lower(input_text)
    set style to "normal"
    if contains(t, "why") or contains(t, "how") or contains(t, "explain"):
        set style to "explanatory"
    if len(split(t, " ")) is below 4:
        set style to "short"
    set user.profile.style[style] to get(user.profile.style, style, 0) + 1
    set users[user_id] to user
    respond with style

// ── Get User Preferred Style ──────────────────────────────

to user_preferred_style with user_id:
    purpose: "Get user's most common style"
    set user to run get_user with user_id
    set styles to user.profile.style
    if len(styles) is equal 0:
        respond with "normal"
    set best to "normal"
    set best_count to 0
    repeat for each entry in styles:
        if entry.value is above best_count:
            set best_count to entry.value
            set best to entry.key
    respond with best

// ── Store User Feedback ───────────────────────────────────

to store_user_feedback with user_id, input_text, response, score:
    purpose: "Store feedback for a specific user"
    set user to run get_user with user_id
    set key to input_text + "|||" + response
    set user.feedback[key] to get(user.feedback, key, 0) + score
    set users[user_id] to user
    respond with true

// ── Get User Feedback ─────────────────────────────────────

to get_user_feedback with user_id, input_text, candidate:
    purpose: "Get feedback score for a user's input-candidate pair"
    set user to run get_user with user_id
    set key to input_text + "|||" + candidate
    respond with get(user.feedback, key, 0)

// ── Store User Memory ─────────────────────────────────────

to store_user_memory with user_id, key, value:
    purpose: "Store a key-value pair in user's personal memory"
    set user to run get_user with user_id
    set user.memory[key] to value
    set users[user_id] to user
    respond with true

// ── Get User Memory ───────────────────────────────────────

to get_user_memory with user_id, key:
    purpose: "Retrieve from user's personal memory"
    set user to run get_user with user_id
    respond with get(user.memory, key)

// ── Set User Preference ───────────────────────────────────

to set_user_preference with user_id, key, value:
    purpose: "Set a specific preference for a user"
    set user to run get_user with user_id
    set user.profile.preferences[key] to value
    set users[user_id] to user
    respond with true

// ── Get User Top Topics ───────────────────────────────────

to user_top_topics with user_id, k:
    purpose: "Get user's top-k most discussed topics"
    set user to run get_user with user_id
    set topics to user.profile.topics
    set results to []
    set found to 0
    repeat while found is below k:
        set best_t to nil
        set best_f to -1
        repeat for each entry in topics:
            if entry.value is above best_f:
                set best_f to entry.value
                set best_t to entry.key
        if best_t is not equal nil:
            append {"topic": best_t, "freq": best_f} to results
            set topics[best_t] to -1
            set found to found + 1
        otherwise:
            set found to k
    respond with results

// ── Global Knowledge ──────────────────────────────────────

to add_global_knowledge with key, value:
    purpose: "Add knowledge to shared global store"
    set global_knowledge[key] to value
    respond with true

to get_global_knowledge with key:
    purpose: "Get from shared global knowledge"
    respond with get(global_knowledge, key)

// ── Session Management ────────────────────────────────────

to start_session with user_id:
    purpose: "Start a new user session"
    set session to {
        "user_id": user_id,
        "started_at": time(),
        "queries": 0
    }
    set sessions[user_id] to session
    respond with session

to end_session with user_id:
    purpose: "End a user session"
    set session to get(sessions, user_id)
    if session is not equal nil:
        // Log usage
        append {"user_id": user_id, "queries": session.queries, "duration": time() - session.started_at} to usage_log
        set sessions[user_id] to nil
    respond with true

to track_query with user_id:
    purpose: "Increment query count for active session"
    set session to get(sessions, user_id)
    if session is not equal nil:
        set session.queries to session.queries + 1
        set sessions[user_id] to session
    respond with true

// ── Handle Request (Main Entry) ───────────────────────────

to handle_request with user_id, input_text, tokens:
    purpose: "Main request handler for multi-user system"
    // Get or create user
    set user to run get_user with user_id
    // Update profile
    run update_user_profile with user_id, tokens
    // Update style
    set style to run update_user_style with user_id, input_text
    // Track query
    run track_query with user_id
    // Return user context for downstream processing
    respond with {
        "user": user,
        "style": style,
        "interaction_count": user.interaction_count
    }

// ── List Users ────────────────────────────────────────────

to list_users:
    purpose: "List all registered users"
    set user_list to []
    repeat for each entry in users:
        append {"id": entry.key, "interactions": entry.value.interaction_count, "last_active": entry.value.last_active} to user_list
    respond with user_list

// ── Delete User ───────────────────────────────────────────

to delete_user with user_id:
    purpose: "Remove a user and all their data"
    set users[user_id] to nil
    set sessions[user_id] to nil
    log "Deleted user: " + user_id
    respond with true

// ── Save All Users ────────────────────────────────────────

to save_all with filepath:
    purpose: "Save all user data to file"
    set data to {
        "users": users,
        "global_knowledge": global_knowledge,
        "usage_log": usage_log
    }
    set json_str to json_encode(data)
    store json_str to filepath
    log "Saved " + string(len(users)) + " users to " + filepath
    respond with true

// ── Load All Users ────────────────────────────────────────

to load_all with filepath:
    purpose: "Load all user data from file"
    try:
        gather data from filepath
        set parsed to json_decode(data)
        set users to parsed.users
        set global_knowledge to parsed.global_knowledge
        set usage_log to parsed.usage_log
        log "Loaded users from " + filepath
        respond with true
    on error e:
        log "No user data at " + filepath
        respond with false

// ── Usage Analytics ───────────────────────────────────────

to analytics:
    purpose: "Return usage analytics"
    set total_queries to 0
    set total_interactions to 0
    repeat for each entry in users:
        set total_interactions to total_interactions + entry.value.interaction_count
    repeat for each log_entry in usage_log:
        set total_queries to total_queries + log_entry.queries
    respond with {
        "total_users": len(users),
        "active_sessions": len(sessions),
        "total_interactions": total_interactions,
        "logged_sessions": len(usage_log),
        "total_queries": total_queries,
        "global_knowledge_items": len(global_knowledge)
    }
