// Direct Python model import — no server, no inline scripts
//
// NC loads the model once, keeps it in memory, runs predictions fast.
//
// Setup:
//   1. python3 train_model.py      (train once, creates model.pkl)
//   2. nc run predict_direct.nc -b predict_customer
//
// Supports: .pkl (sklearn), .pt/.pth (PyTorch), .h5 (TensorFlow), .onnx

service "direct-predictor"
version "1.0.0"

to predict_customer:
    purpose: "Load a Python model and run prediction directly"

    set model to load_model("model.pkl")
    log "Model loaded: {{model.type}} from {{model.path}}"

    set result to predict(model, [35, 60000])
    show result

    set result2 to predict(model, [20, 20000])
    show result2

    set result3 to predict(model, [45, 90000])
    show result3

    respond with result

to batch_predict:
    purpose: "Run multiple predictions on a loaded model"

    set model to load_model("model.pkl")

    set customers to [
        [25, 30000],
        [35, 60000],
        [45, 90000],
        [20, 20000],
        [55, 100000]
    ]

    set results to []

    repeat for each features in customers:
        set prediction to predict(model, features)
        append prediction to results

    show results
    respond with results
