service "users"
version "1.0.0"

to list_users:
    set items to load("users.json")
    respond with items

to get_user with id:
    set item to load("users.json", id)
    if item is empty:
        respond with error "User not found"
    respond with item

api:
    GET /users runs list_users
    GET /users/:id runs get_user

middleware:
    cors allow all
    rate limit 60 per minute
