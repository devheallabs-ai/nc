// My first Notation-as-Code program
// Run: nc run examples/01_hello_world.nc

service "hello-world"
version "1.0.0"

to greet with name:
    purpose: "Say hello to someone"
    respond with "Hello, " + name + "! Welcome to Notation-as-Code."

to health check:
    respond with "healthy"

api:
    GET /hello runs greet
    GET /health runs health_check
