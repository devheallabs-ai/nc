// Test: API route declarations
// Verifies all HTTP methods and route parsing

service "test-api"
version "1.0.0"

to get items:
    respond with "items list"

to create item:
    respond with "item created"

to update item:
    respond with "item updated"

to delete item:
    respond with "item deleted"

to health check:
    respond with "healthy"

api:
    GET /items runs get_items
    POST /items runs create_item
    PUT /items runs update_item
    DELETE /items runs delete_item
    GET /health runs health_check
