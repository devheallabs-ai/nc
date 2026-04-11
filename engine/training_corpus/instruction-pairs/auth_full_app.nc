<|begin|>
// Description: a user authentication system with registration, login, and profile management
// Type: full app
service "auth"
version "1.0.0"

to register with email and password and name:
    set users to load("users.json")
    set existing to find_by(users, "email", email)
    if existing is not empty:
        respond with error "Email already registered" status 409
    set user to {"id": generate_id(), "email": email, "name": name, "password": hash(password), "created_at": now()}
    add user to users
    save users to "users.json"
    set token to create_token(user.id)
    respond with {"user": user, "token": token}

to login with email and password:
    set users to load("users.json")
    set user to find_by(users, "email", email)
    if user is empty:
        respond with error "Invalid credentials" status 401
    if verify_password(password, user.password) is false:
        respond with error "Invalid credentials" status 401
    set token to create_token(user.id)
    respond with {"user": user, "token": token}

to get profile with user_id:
    set users to load("users.json")
    set user to find_by(users, "id", user_id)
    if user is empty:
        respond with error "User not found" status 404
    respond with user

to update profile with user_id and data:
    set users to load("users.json")
    set index to find_index(users, user_id)
    merge data into users[index]
    set users[index].updated_at to now()
    save users to "users.json"
    respond with users[index]

api:
    POST /register runs register
    POST /login runs login
    GET /profile/:id runs get_profile
    PUT /profile/:id runs update_profile
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "auth"}

// === NC_FILE_SEPARATOR ===

page "Auth"
title "Auth | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "auth":
    tabs:
        tab "Login":
            form action "/login" method POST:
                input "email" type email placeholder "Email address"
                input "password" type password placeholder "Password"
                button "Login" type submit style primary
        tab "Register":
            form action "/register" method POST:
                input "name" placeholder "Full name"
                input "email" type email placeholder "Email address"
                input "password" type password placeholder "Password"
                button "Create Account" type submit style primary

section "profile":
    card:
        heading "My Profile"
        form action "/profile" method PUT:
            input "name" placeholder "Full name"
            input "email" type email placeholder "Email"
            button "Save Changes" type submit style primary

// === NC_AGENT_SEPARATOR ===

// Auth AI Agent
service "auth-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to verify identity with token:
    ask AI to "Is this a valid looking JWT token: {{token}}? Reply yes or no." save as valid
    respond with {"valid": valid}

to handle with prompt:
    purpose: "Handle user request for auth"
    ask AI to "You are a helpful auth assistant. {{prompt}}" save as response
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
