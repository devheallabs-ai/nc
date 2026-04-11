// Orders Service -- runnable demo for backend generation validation

service "orders-api"
version "1.0.0"

to list_orders:
    gather items from "orders"
    respond with items

to get_order with id:
    gather item from "orders" where id is id
    if item is empty:
        respond with error "Order not found"
    respond with item

to create_order with data:
    set data.id to generate_id()
    set data.created_at to now()
    set data.updated_at to now()
    if data.status is empty:
        set data.status to "pending"
    store data into "orders"
    respond with data

to update_order with id and data:
    set data.id to id
    set data.updated_at to now()
    store data into "orders"
    respond with data

to delete_order with id:
    gather existing from "orders" where id is id
    if existing is empty:
        respond with error "Order not found"
    gather orders_list from "orders"
    remove existing from orders_list
    store orders_list into "orders"
    respond with {"deleted": id}

to health_check:
    respond with {"status": "healthy", "service": "orders-api"}

middleware:
    use cors
    use log_requests

api:
    GET  /orders      runs list_orders
    GET  /orders/:id  runs get_order
    POST /orders      runs create_order
    PUT  /orders/:id  runs update_order
    DELETE /orders/:id runs delete_order
    GET  /health      runs health_check