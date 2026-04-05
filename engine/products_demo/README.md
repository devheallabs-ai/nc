# Products Demo — NC Lang

A product-catalog backend + UI demo showing NC's split generation in action.

## Quick Start

### 1 — Start the backend
```bash
cd nc-lang/engine/products_demo
../build/nc serve service.nc        # starts on http://localhost:8001
```

### 2 — Build the UI
```bash
../build/nc ui build app.ncui       # produces app.html
```

### 3 — Open `app.html` in a browser

The form on the page POSTs directly to the live backend.

## API (port 8001)

| Method | Path              | Handler          |
|--------|-------------------|------------------|
| GET    | /products         | list_products    |
| GET    | /products/:id     | get_product      |
| POST   | /products         | create_product   |
| PUT    | /products/:id     | update_product   |
| DELETE | /products/:id     | delete_product   |
| GET    | /health           | health_check     |
