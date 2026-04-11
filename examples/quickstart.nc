// Your first NC program — run with: nc serve quickstart.nc
service "hello"
version "1.0.0"

to greet:
    respond with "Hello from NC!"

api:
    GET /hello runs greet
