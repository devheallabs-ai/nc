// ═══════════════════════════════════════════════════════════
//  NC ML — Feedback Learning (Without RL)
//
//  Learns from user feedback using direct weight adjustment,
//  graph reinforcement, and confidence scoring. No backprop,
//  no reward functions — just statistics + memory.
//
//  Math:
//    new_weight = old_weight + lr * feedback
//    confidence = positive / total
//    score_final = base + alpha * user_pref + beta * feedback
//
//  Features:
//    - Direct feedback storage (+1 / -1)
//    - Edge reinforcement (strengthen/weaken paths)
//    - Response caching (successful responses)
//    - Confidence tracking per response
//    - Auto-feedback detection (no buttons needed)
//    - Feedback-adjusted scoring
//    - Feedback decay over time
//
//  Usage:
//    nc nc-ai/nc/ml/feedback.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-feedback"
version "1.0.0"

configure:
    feedback_lr is 0.1
    feedback_weight is 0.2
    confidence_threshold is 0.6
    feedback_decay is 0.995

// ── Feedback Store ────────────────────────────────────────

set feedback_store to {}
set response_cache to {}
set confidence_store to {}
set feedback_count to {"positive": 0, "negative": 0, "total": 0}

// ── Store Feedback ────────────────────────────────────────

to store_feedback with input_text, response, score:
    purpose: "Store feedback for an input-response pair"
    set key to input_text + "|||" + response
    set current to get(feedback_store, key, 0)
    set feedback_store[key] to current + score
    // Update counts
    set feedback_count.total to feedback_count.total + 1
    if score is above 0:
        set feedback_count.positive to feedback_count.positive + 1
    otherwise:
        set feedback_count.negative to feedback_count.negative + 1
    // Cache positive responses
    if score is above 0:
        set response_cache[input_text] to response
    respond with true

// ── Get Feedback Score ────────────────────────────────────

to get_feedback with input_text, candidate:
    purpose: "Get accumulated feedback score for input-candidate pair"
    set key to input_text + "|||" + candidate
    set score to get(feedback_store, key, 0)
    respond with score

// ── Check Cache ───────────────────────────────────────────

to check_cache with input_text:
    purpose: "Check if a cached good response exists"
    set cached to get(response_cache, input_text)
    respond with cached

// ── Feedback-Adjusted Score ───────────────────────────────

to adjusted_score with base_score, input_text, candidate:
    purpose: "Combine base score with feedback for final ranking"
    set fb to run get_feedback with input_text, candidate
    set final to base_score + feedback_weight * fb
    respond with final

// ── Reinforce Path ────────────────────────────────────────

to reinforce_path with graph, path, positive:
    purpose: "Strengthen or weaken all edges in a reasoning path"
    set delta to feedback_lr
    if positive is equal false:
        set delta to 0.0 - feedback_lr
    set i to 0
    repeat while i is below len(path) - 1:
        set a to path[i]
        set b to path[i + 1]
        // Update edge weight in graph
        set edges to get(graph, a)
        if edges is not equal nil:
            set j to 0
            repeat for each edge in edges:
                if edge.target is equal b:
                    set graph[a][j].weight to edge.weight + delta
                set j to j + 1
        set i to i + 1
    respond with true

// ── Learn From Feedback ───────────────────────────────────

to learn with input_text, path, response, positive:
    purpose: "Full learning step: store feedback + reinforce path"
    // Store feedback
    set score to 1
    if positive is equal false:
        set score to -1
    run store_feedback with input_text, response, score
    // Update confidence
    set conf_key to join(path, "→")
    if get(confidence_store, conf_key) is equal nil:
        set confidence_store[conf_key] to {"positive": 0, "negative": 0}
    if positive is equal true:
        set confidence_store[conf_key].positive to confidence_store[conf_key].positive + 1
    otherwise:
        set confidence_store[conf_key].negative to confidence_store[conf_key].negative + 1
    log "Feedback recorded: " + input_text + " → " + string(score)
    respond with true

// ── Get Confidence ────────────────────────────────────────

to get_confidence with path:
    purpose: "Get confidence score for a reasoning path"
    set key to join(path, "→")
    set data to get(confidence_store, key)
    if data is equal nil:
        respond with 0.5
    set total to data.positive + data.negative
    if total is equal 0:
        respond with 0.5
    set confidence to data.positive / total
    respond with confidence

// ── Is Confident ──────────────────────────────────────────

to is_confident with path:
    purpose: "Check if confidence exceeds threshold"
    set conf to run get_confidence with path
    if conf is above confidence_threshold:
        respond with true
    respond with false

// ── Auto-Detect Feedback ──────────────────────────────────

to auto_feedback with user_input:
    purpose: "Detect implicit feedback from user response"
    set t to lower(user_input)
    // Negative signals
    set negative_signals to ["wrong", "not correct", "no that's not", "incorrect", "bad", "terrible", "useless", "not helpful", "doesn't make sense"]
    repeat for each signal in negative_signals:
        if contains(t, signal):
            respond with -1
    // Positive signals
    set positive_signals to ["thanks", "great", "perfect", "correct", "yes", "exactly", "good", "helpful", "makes sense", "got it"]
    repeat for each signal in positive_signals:
        if contains(t, signal):
            respond with 1
    // Neutral (continuation = slight positive)
    if len(t) is above 5:
        respond with 0
    respond with 0

// ── Detect Repeated Question ──────────────────────────────

to is_repeated with input_text, history:
    purpose: "Check if user is repeating a question (negative signal)"
    set t to lower(input_text)
    set repeat_count to 0
    repeat for each h in history:
        if lower(h) is equal t:
            set repeat_count to repeat_count + 1
    if repeat_count is above 0:
        respond with true
    respond with false

// ── Decay All Feedback ────────────────────────────────────

to decay_feedback:
    purpose: "Apply time decay to feedback scores"
    set decayed to 0
    repeat for each entry in feedback_store:
        set feedback_store[entry.key] to entry.value * feedback_decay
        set decayed to decayed + 1
    respond with decayed

// ── Top Cached Responses ──────────────────────────────────

to top_cached with k:
    purpose: "Get top-k cached successful responses"
    set results to []
    set count to 0
    repeat for each entry in response_cache:
        if count is below k:
            append {"input": entry.key, "response": entry.value} to results
            set count to count + 1
    respond with results

// ── Export / Import Feedback ──────────────────────────────

to export_feedback with filepath:
    purpose: "Save feedback data to file"
    set data to {
        "feedback": feedback_store,
        "cache": response_cache,
        "confidence": confidence_store,
        "counts": feedback_count
    }
    set json_str to json_encode(data)
    store json_str to filepath
    log "Exported feedback to " + filepath
    respond with true

to import_feedback with filepath:
    purpose: "Load feedback data from file"
    try:
        gather data from filepath
        set parsed to json_decode(data)
        set feedback_store to parsed.feedback
        set response_cache to parsed.cache
        set confidence_store to parsed.confidence
        set feedback_count to parsed.counts
        log "Imported feedback from " + filepath
        respond with true
    on error e:
        log "No feedback data at " + filepath
        respond with false

// ── Feedback Stats ────────────────────────────────────────

to feedback_stats:
    purpose: "Return feedback system statistics"
    respond with {
        "total_feedback": feedback_count.total,
        "positive": feedback_count.positive,
        "negative": feedback_count.negative,
        "cached_responses": len(response_cache),
        "tracked_paths": len(confidence_store)
    }
