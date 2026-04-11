# Using Python ML Models with NC

Two ways to use a Python-trained model from NC:

## Method 1: Model as API (Recommended for Production)

```bash
# Train model once
python3 train_model.py

# Start model server (keep running)
python3 serve_model.py

# Run NC prediction
nc run predict.nc -b predict

# Or serve as HTTP API
nc serve predict.nc
# Then: curl -X POST http://localhost:8000/predict -d '{"age":35,"salary":60000}'
```

## Method 2: Direct Python Bridge (Quick & Simple)

```bash
# Train model once
python3 train_model.py

# Run directly — no server needed
nc run predict_direct.nc -b predict_customer
```

## When to Use Which

| Method | Best For |
|--------|----------|
| API server | Production, multiple callers, fast response |
| Direct bridge | Quick testing, one-off predictions |
