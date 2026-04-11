<|begin|>
// Description: an e-commerce store with products, cart, and orders
// Type: full app
service "ecommerce"
version "1.0.0"

middleware auth_check:
    set token to request.headers.authorization
    if token is empty:
        respond with error "Unauthorized" status 401
    set user to verify_token(token)
    if user is empty:
        respond with error "Invalid token" status 401

to list products:
    set products to load("products.json")
    respond with products

to get product with id:
    set product to load("products.json", id)
    respond with product

to add to cart with user_id and product_id and quantity:
    set cart to load("cart.json", user_id)
    if cart is empty:
        set cart to {"user_id": user_id, "items": []}
    set item to {"product_id": product_id, "quantity": quantity}
    add item to cart.items
    save cart to "cart.json"
    respond with cart

to checkout with user_id:
    set cart to load("cart.json", user_id)
    set order to {"id": generate_id(), "user_id": user_id, "items": cart.items, "status": "pending", "created_at": now()}
    set orders to load("orders.json")
    add order to orders
    save orders to "orders.json"
    respond with order

to list orders with user_id:
    set orders to load("orders.json")
    set user_orders to filter(orders, "user_id", user_id)
    respond with user_orders

api:
    GET /products runs list_products
    GET /products/:id runs get_product
    POST /cart runs add_to_cart
    POST /checkout runs checkout
    GET /orders runs list_orders
    GET /health runs health_check

to health check:
    respond with {"status": "ok", "service": "ecommerce"}

// === NC_FILE_SEPARATOR ===

page "Ecommerce"
title "Ecommerce | NC App"

theme:
    primary is "#2563eb"
    background is "#f8fafc"
    font is "Inter, sans-serif"

nav:
    brand "Shop"
    link "Products" to "/products"
    link "Cart" to "/cart" icon "cart"
    link "Orders" to "/orders"

section "products":
    heading "Our Products"
    grid from "/products" as products columns 4:
        card:
            image products.image
            heading products.name
            text products.price style price
            button "Add to Cart" action "addToCart" style primary

section "cart":
    heading "Your Cart"
    list from "/cart" as items:
        card:
            text items.name
            text items.quantity
            text items.price
    button "Checkout" action "checkout" style primary

// === NC_AGENT_SEPARATOR ===

// Ecommerce AI Agent
service "ecommerce-agent"
version "1.0.0"

configure:
    max_tokens is 512
    temperature is 0.7

to recommend with user_id:
    set history to load("orders.json", user_id)
    ask AI to "Recommend products for a customer who bought: {{history}}" save as recommendations
    respond with {"recommendations": recommendations}

to answer product question with question and product_id:
    set product to load("products.json", product_id)
    ask AI to "Answer this product question: {{question}}. Product info: {{product}}" save as answer
    respond with {"answer": answer}

to handle with prompt:
    purpose: "Handle user request for ecommerce"
    ask AI to "You are a helpful ecommerce assistant. {{prompt}}" save as response
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
