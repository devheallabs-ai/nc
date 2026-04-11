<|begin|>
// Description: an e-commerce store with products, cart, and orders (API only, no frontend)
// Type: service
service "ecommerce-api"
version "1.0.0"

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
    respond with {"status": "ok"}
<|end|>
