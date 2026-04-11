// Test: Session management
// Verifies create, set, get, exists, destroy

service "test-sessions"
version "1.0.0"

to test session create:
    set sid to session_create()
    if sid is not equal to nothing:
        respond with "created"

to test session set and get:
    set sid to session_create()
    session_set(sid, "username", "alice")
    set val to session_get(sid, "username")
    respond with val

to test session multiple keys:
    set sid to session_create()
    session_set(sid, "name", "bob")
    session_set(sid, "role", "admin")
    set name to session_get(sid, "name")
    set role to session_get(sid, "role")
    respond with name + ":" + role

to test session exists:
    set sid to session_create()
    set exists to session_exists(sid)
    respond with exists

to test session destroy:
    set sid to session_create()
    session_set(sid, "key", "value")
    session_destroy(sid)
    set exists to session_exists(sid)
    respond with exists

to test session missing key:
    set sid to session_create()
    set val to session_get(sid, "nonexistent")
    if val is equal to nothing:
        respond with "none"
