<|begin|>
// Description: a user authentication system with registration, login, and profile management (API only, no frontend)
// Type: service
service "auth-api"
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
    respond with {"status": "ok"}
<|end|>
