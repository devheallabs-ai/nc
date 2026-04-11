// ═══════════════════════════════════════════════════════════
//  NC ML — User Personalization & Adaptive Learning
//
//  Learns user preferences, topics, and style over time.
//  Adapts responses based on interaction history.
//
//  Math:
//    weight = e^(-lambda * (t_now - t_i))  (recency)
//    Context = alpha * input + beta * user_profile
//    Score = base + lambda * preference
//
//  Features:
//    - User profile memory (topics, style, history)
//    - Recency-weighted learning
//    - Style detection (explanatory, short, normal)
//    - Personalized context vectors
//    - Personalized candidate ranking
//    - Adaptive response mode selection
//    - Profile persistence (save/load)
//
//  Usage:
//    nc nc-ai/nc/ml/personalization.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-personalization"
version "1.0.0"

configure:
    context_user_weight is 0.3
    context_input_weight is 0.7
    preference_boost is 0.1
    recency_decay is 0.001
    max_history is 500

// ── User Profile Structure ────────────────────────────────

set user_profile to {
    "topics": {},
    "style": {},
    "history": [],
    "preferences": {},
    "interaction_count": 0,
    "created_at": 0
}

// ── Initialize Profile ────────────────────────────────────

to init_profile:
    purpose: "Initialize a fresh user profile"
    set user_profile to {
        "topics": {},
        "style": {},
        "history": [],
        "preferences": {},
        "interaction_count": 0,
        "created_at": time()
    }
    respond with user_profile

// ── Update Profile From Input ─────────────────────────────

to update_profile with tokens:
    purpose: "Learn from user input tokens"
    // Update topic frequency
    repeat for each t in tokens:
        set user_profile.topics[t] to get(user_profile.topics, t, 0) + 1
    // Update history
    append {"tokens": tokens, "timestamp": time()} to user_profile.history
    // Trim history if too long
    if len(user_profile.history) is above max_history:
        set user_profile.history to slice(user_profile.history, len(user_profile.history) - max_history, max_history)
    // Increment interaction count
    set user_profile.interaction_count to user_profile.interaction_count + 1
    respond with true

// ── Detect User Style ─────────────────────────────────────

to detect_style with user_input:
    purpose: "Detect preferred response style from user input"
    set t to lower(user_input)
    set style to "normal"
    if contains(t, "why") or contains(t, "how") or contains(t, "explain"):
        set style to "explanatory"
    if contains(t, "detail") or contains(t, "in depth") or contains(t, "thoroughly"):
        set style to "detailed"
    if len(split(t, " ")) is below 4:
        set style to "short"
    if contains(t, "quick") or contains(t, "brief") or contains(t, "tldr"):
        set style to "short"
    respond with style

// ── Update Style Preference ───────────────────────────────

to update_style with user_input:
    purpose: "Track user style preferences over time"
    set style to run detect_style with user_input
    set user_profile.style[style] to get(user_profile.style, style, 0) + 1
    respond with style

// ── Get Preferred Style ───────────────────────────────────

to preferred_style:
    purpose: "Return user's most common style preference"
    set styles to user_profile.style
    if len(styles) is equal 0:
        respond with "normal"
    set best_style to "normal"
    set best_count to 0
    repeat for each entry in styles:
        if entry.value is above best_count:
            set best_count to entry.value
            set best_style to entry.key
    respond with best_style

// ── Recency Score ─────────────────────────────────────────

to recency_score with timestamp:
    purpose: "Calculate recency weight: e^(-decay * age)"
    set now to time()
    set age to now - timestamp
    set weight to exp(0.0 - recency_decay * age)
    respond with weight

// ── Get Top User Topics ───────────────────────────────────

to top_topics with k:
    purpose: "Get user's top-k most discussed topics"
    set topics to user_profile.topics
    set results to []
    set found to 0
    repeat while found is below k:
        set best_topic to nil
        set best_freq to -1
        repeat for each entry in topics:
            if entry.value is above best_freq:
                set best_freq to entry.value
                set best_topic to entry.key
        if best_topic is not equal nil:
            append {"topic": best_topic, "freq": best_freq} to results
            set topics[best_topic] to -1
            set found to found + 1
        otherwise:
            set found to k
    respond with results

// ── Personalized Context Vector ───────────────────────────

to personalized_context with tokens, embedding_store:
    purpose: "Mix input context with user profile for personalized vector"
    // Input vector
    set input_vec to tensor_create(1, 64)
    set input_count to 0
    repeat for each t in tokens:
        set v to get(embedding_store, t)
        if v is not equal nil:
            set input_vec to tensor_add(input_vec, v)
            set input_count to input_count + 1
    if input_count is above 0:
        set input_vec to tensor_scale(input_vec, 1.0 / input_count)
    // User profile vector
    set user_vec to tensor_create(1, 64)
    set user_count to 0
    repeat for each entry in user_profile.topics:
        set v to get(embedding_store, entry.key)
        if v is not equal nil:
            set weighted to tensor_scale(v, entry.value)
            set user_vec to tensor_add(user_vec, weighted)
            set user_count to user_count + 1
    if user_count is above 0:
        set user_vec to tensor_scale(user_vec, 1.0 / user_count)
    // Combine: alpha * input + beta * user
    set combined to tensor_add(
        tensor_scale(input_vec, context_input_weight),
        tensor_scale(user_vec, context_user_weight)
    )
    respond with combined

// ── Personalized Score ────────────────────────────────────

to personalized_score with base_score, word:
    purpose: "Boost score based on user topic preference"
    set preference to get(user_profile.topics, word, 0)
    set boost to preference_boost * preference
    set final to base_score + boost
    respond with final

// ── Choose Response Mode ──────────────────────────────────

to choose_mode:
    purpose: "Choose response mode based on user history"
    set style to run preferred_style
    if style is equal "explanatory":
        respond with "story"
    if style is equal "detailed":
        respond with "long"
    if style is equal "short":
        respond with "concise"
    respond with "normal"

// ── Set Preference ────────────────────────────────────────

to set_preference with key, value:
    purpose: "Explicitly set a user preference"
    set user_profile.preferences[key] to value
    respond with true

to get_preference with key, default_val:
    purpose: "Get a user preference with default"
    set val to get(user_profile.preferences, key)
    if val is equal nil:
        respond with default_val
    respond with val

// ── Save Profile ──────────────────────────────────────────

to save_profile with filepath:
    purpose: "Save user profile to disk"
    set json_str to json_encode(user_profile)
    store json_str to filepath
    log "Saved user profile to " + filepath
    respond with true

// ── Load Profile ──────────────────────────────────────────

to load_profile with filepath:
    purpose: "Load user profile from disk"
    try:
        gather data from filepath
        set user_profile to json_decode(data)
        log "Loaded user profile from " + filepath
        respond with true
    on error e:
        log "No existing profile at " + filepath + ", using fresh"
        run init_profile
        respond with false

// ── Profile Stats ─────────────────────────────────────────

to profile_stats:
    purpose: "Return user profile statistics"
    respond with {
        "interaction_count": user_profile.interaction_count,
        "unique_topics": len(user_profile.topics),
        "history_length": len(user_profile.history),
        "preferred_style": run preferred_style,
        "preferences": user_profile.preferences
    }
