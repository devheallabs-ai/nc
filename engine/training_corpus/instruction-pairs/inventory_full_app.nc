<|begin|>
// Description: an inventory management system with stock tracking and alerts
// Type: full app
service "inventory"
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
    respond with {"status": "ok", "service": "inventory"}

// === NC_FILE_SEPARATOR ===

page "Inventory"
title "Inventory | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

section "inventory":
    heading "Inventory Management"
    button "Add Item" action "openAddForm" style primary

    grid from "/inventory" as items columns 3:
        card:
            image items.image
            heading items.name
            text items.sku style mono
            text items.stock style number
            badge items.stock "In Stock" "Low Stock" threshold 10
            button "Update Stock" action "updateStock" style secondary

section "alerts":
    heading "Low Stock Alerts"
    list from "/inventory/alerts/low-stock" as low:
        card:
            heading low.name
            text low.stock style danger
            button "Reorder" action "reorder" style primary

// === NC_AGENT_SEPARATOR ===

// Inventory AI Agent
service "inventory-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to forecast demand with item_id:
    set history to load("sales.json", item_id)
    ask AI to "Forecast demand for next month based on sales history: {{history}}" save as forecast
    respond with {"forecast": forecast}

to generate reorder suggestion with low_items:
    ask AI to "Suggest reorder quantities for these low-stock items: {{low_items}}" save as suggestions
    respond with {"suggestions": suggestions}

to handle with prompt:
    purpose: "Handle user request for inventory"
    ask AI to "You are a helpful inventory assistant. {{prompt}}" save as response
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
