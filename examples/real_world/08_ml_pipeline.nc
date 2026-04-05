// ═══════════════════════════════════════════════════════════
//  ML Model Pipeline
//
//  Replaces: 100+ lines of Python with ML framework + API server
//
//  Load trained models, run predictions, compare models.
//
//  Setup:
//    python3 -c "from sklearn.tree import DecisionTreeClassifier; import pickle;
//    m=DecisionTreeClassifier(); m.fit([[1,1],[2,2],[3,3]],[0,1,1]);
//    pickle.dump(m,open('model.pkl','wb'))"
//
//  curl -X POST http://localhost:8000/predict \
//    -d '{"features": [2.5, 2.5]}'
// ═══════════════════════════════════════════════════════════

service "ml-pipeline"
version "1.0.0"

configure:
    ai_model is "default"

to predict with features:
    purpose: "Run prediction on a trained sklearn model"

    set ml_model to load_model("model.pkl")

    if ml_model.status is not equal "loaded":
        respond with {"error": "Model not loaded", "details": ml_model}

    set prediction to predict(ml_model, features)

    respond with prediction

to predict_and_explain with features, context:
    purpose: "Predict + use AI to explain the prediction"

    set ml_model to load_model("model.pkl")
    set prediction to predict(ml_model, features)

    ask AI to "A machine learning model made this prediction: {{prediction}}. The input features were: {{features}}. Additional context: {{context}}. Explain in plain English why the model might have made this prediction and what it means." save as explanation

    respond with {"prediction": prediction, "explanation": explanation, "features": features}

to batch_predict with feature_sets:
    purpose: "Run predictions on multiple inputs"

    set ml_model to load_model("model.pkl")
    set results to []

    repeat for each features in feature_sets:
        set pred to predict(ml_model, features)
        append {"features": features, "prediction": pred} to results

    respond with results

to analyze_predictions with predictions:
    purpose: "AI analysis of prediction results"

    ask AI to "Analyze these ML predictions. Return JSON with: total (number), distribution (object with class counts), patterns (list of observed patterns), recommendations (list of strings).\n\nPredictions: {{predictions}}" save as analysis

    respond with analysis

api:
    POST /predict runs predict
    POST /predict/explain runs predict_and_explain
    POST /predict/batch runs batch_predict
    POST /analyze runs analyze_predictions
