<|begin|>
// Description: an inventory management system with stock tracking and alerts (API only, no frontend)
// Type: service
service "inventory-api"
version "1.0.0"

to list items:
    set items to load("inventory.json")
    respond with items

to get item with id:
    set item to load("inventory.json", id)
    respond with item

to add item with data:
    set data.id to generate_id()
    set data.created_at to now()
    set items to load("inventory.json")
    add data to items
    save items to "inventory.json"
    respond with data

to update stock with id and quantity:
    set items to load("inventory.json")
    set index to find_index(items, id)
    set items[index].stock to quantity
    set items[index].updated_at to now()
    save items to "inventory.json"
    respond with items[index]

to low stock alert:
    set items to load("inventory.json")
    set low to filter_by_threshold(items, "stock", 10)
    respond with low

api:
    GET /inventory runs list_items
    GET /inventory/:id runs get_item
    POST /inventory runs add_item
    PUT /inventory/:id/stock runs update_stock
    GET /inventory/alerts/low-stock runs low_stock_alert
    GET /health runs health_check

to health check:
    respond with {"status": "ok"}
<|end|>
