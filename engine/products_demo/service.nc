// Products Service -- catalog CRUD backend demo

service "products-api"
version "1.0.0"

to list_products:
    gather all from "products"
    respond with all

to get_product with id:
    gather item from "products" where id is id
    if item is empty:
        respond with error "Product not found"
    respond with item

to create_product with data:
    set data.id to generate_id()
    set data.created_at to now()
    set data.updated_at to now()
    if data.in_stock is empty:
        set data.in_stock to true
    store data into "products"
    respond with data

to update_product with id and data:
    set data.id to id
    set data.updated_at to now()
    store data into "products"
    respond with data

to delete_product with id:
    gather existing from "products" where id is id
    if existing is empty:
        respond with error "Product not found"
    gather products_list from "products"
    remove existing from products_list
    store products_list into "products"
    respond with {"deleted": id}

to health_check:
    respond with {"status": "ok", "service": "products-api"}

middleware:
    use cors
    use log_requests

api:
    GET    /products      runs list_products
    GET    /products/:id  runs get_product
    POST   /products      runs create_product
    PUT    /products/:id  runs update_product
    DELETE /products/:id  runs delete_product
    GET    /health        runs health_check
