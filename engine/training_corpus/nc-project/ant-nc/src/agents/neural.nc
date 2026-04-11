// ══════════════════════════════════════════════════════════════════
//  NEURAL NETWORK — Train or load a model for incident classification
//
//  NC supports: sklearn (.pkl), PyTorch (.pt), TensorFlow (.h5), ONNX (.onnx)
//  Requires: Python installed + NC_ALLOW_PICKLE=1 for .pkl models
//
//  Train a classifier externally, put it in models/, use it in SwarmOps:
//    models/incident-classifier.pkl   — classifies: config|deploy|infra|code-bug
//    models/severity-predictor.onnx   — predicts: critical|high|medium|low
// ══════════════════════════════════════════════════════════════════

to load_nn with model_path:
    purpose: "Load a neural network / ML model for incident classification"
    set nn to load_model(model_path)
    respond with nn

to predict_incident with model_path, features:
    purpose: "Use loaded ML model to classify an incident"
    set nn to load_model(model_path)
    set result to predict(nn, features)
    respond with {"model": model_path, "prediction": result, "features": features}

to nn_classify with error_rate, latency_ms, cpu_percent, memory_percent, recent_deploy:
    purpose: "Classify incident using ML model if available, otherwise use heuristics"
    set features to [error_rate, latency_ms, cpu_percent, memory_percent, recent_deploy]
    if file_exists("models/incident-classifier.pkl"):
        set nn to load_model("models/incident-classifier.pkl")
        set prediction to predict(nn, features)
        respond with {"method": "neural_network", "model": "incident-classifier.pkl", "prediction": prediction, "features": features}
    otherwise:
        // Heuristic fallback when no model is available
        set category to "unknown"
        if cpu_percent is above 90:
            set category to "resource-exhaustion"
        otherwise if latency_ms is above 5000:
            set category to "infrastructure"
        otherwise if error_rate is above 50:
            set category to "code-bug"
        otherwise if recent_deploy is above 0:
            set category to "deployment"
        respond with {"method": "heuristic", "prediction": category, "features": features, "note": "Put a trained model at models/incident-classifier.pkl for ML classification"}

// ══════════════════════════════════════════════════════════════════
//  CHAT — Conversational interface to talk with SwarmOps
//  Uses NC's built-in memory_new / memory_add for multi-turn context
// ══════════════════════════════════════════════════════════════════

to chat with message, session_id:
    purpose: "Chat with SwarmOps about your services, incidents, and infrastructure"

    // Load conversation memory (30 turns max)
    set mem to memory_new(30)
    memory_add(mem, "user", message)

    // Load current system context
    set incident_count to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set knowledge_summary to "none"
    if file_exists("knowledge/semantic.json"):
        set knowledge_summary to read_file("knowledge/semantic.json")

    set system_context to "You are SwarmOps, an AI SRE assistant. You help engineers debug production incidents. You have access to: " + trim(incident_count) + " past incidents and knowledge patterns: " + str(knowledge_summary) + ". You can investigate incidents, check service health, explain past incidents, and suggest improvements. Be concise and technical."

    set history to memory_summary(mem)
    ask AI to "{{system_context}}. Conversation: {{history}}. User says: {{message}}. Respond helpfully. If the user describes an incident, offer to investigate. If they ask about past incidents, summarize what you know." save as reply

    memory_add(mem, "assistant", str(reply))

    respond with {"reply": reply, "session": session_id, "context": {"incidents": trim(incident_count), "has_knowledge": knowledge_summary}}

to chat_about_incident with incident_id:
    purpose: "Chat about a specific past incident"
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        set data to read_file(path)
        set incident to json_decode(data)
        ask AI to "Explain this incident to an engineer in a conversational way. Include what happened, why, how it was fixed, and how to prevent it. Incident data: {{data}}" save as explanation
        respond with {"incident_id": incident_id, "explanation": explanation, "data": incident}
    otherwise:
        respond with {"error": "Incident not found"}

to chat_suggest with service_name:
    purpose: "Get proactive suggestions for a service based on past patterns"
    set past to shell("grep -l '\"" + service_name + "\"' incidents/*.json 2>/dev/null | wc -l || echo 0")
    set knowledge to "none"
    if file_exists("knowledge/semantic.json"):
        set knowledge to read_file("knowledge/semantic.json")

    ask AI to "Based on incident history for service {{service_name}} ({{past}} past incidents) and learned patterns: {{knowledge}}, suggest proactive improvements to prevent future incidents. Return JSON: {\"service\": \"string\", \"risk_level\": \"high|medium|low\", \"suggestions\": [{\"area\": \"string\", \"suggestion\": \"string\", \"priority\": \"string\"}], \"recurring_patterns\": [\"string\"]}" save as suggestions
    respond with suggestions

