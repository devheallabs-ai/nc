// Step 3: Use the Python model from NC
//
// This is the NC service that calls your Python ML model.
// The model runs as an API, NC talks to it in plain English.
//
// Setup:
//   1. python3 train_model.py          (train once)
//   2. python3 serve_model.py          (start model server)
//   3. nc run predict.nc -b predict    (run prediction)

service "purchase-predictor"
version "1.0.0"

configure:
    model_url is "http://localhost:5000"

to predict with age, salary:
    purpose: "Predict if a customer will buy based on age and salary"

    gather prediction from "{{config.model_url}}/predict":
        features: [age, salary]

    if prediction.label is equal "will_buy":
        log "Customer (age={{age}}, salary={{salary}}) WILL buy ({{prediction.confidence}} confidence)"
    otherwise:
        log "Customer (age={{age}}, salary={{salary}}) will NOT buy ({{prediction.confidence}} confidence)"

    respond with prediction

to batch_predict:
    purpose: "Run predictions for multiple customers"

    set customers to [
        {"age": 25, "salary": 30000},
        {"age": 35, "salary": 60000},
        {"age": 45, "salary": 90000},
        {"age": 20, "salary": 20000}
    ]

    set results to []

    repeat for each customer in customers:
        gather prediction from "{{config.model_url}}/predict":
            features: [customer.age, customer.salary]
        append prediction to results

    respond with results

api:
    POST /predict runs predict
