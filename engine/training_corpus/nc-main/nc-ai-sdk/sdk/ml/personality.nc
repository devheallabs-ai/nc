// ═══════════════════════════════════════════════════════════
//  NC ML — Personality, Emotion & Story Mode
//
//  Adds conversational tone control, emotion detection,
//  storytelling mode, and long-form explanation generation.
//
//  Features:
//    - Persona profiles (default, friendly, expert, concise)
//    - Keyword-based emotion detection
//    - Emotional response adaptation
//    - Story mode (setup → mechanism → outcome)
//    - Long-form mode (explain → elaborate → summarize)
//    - Response mode selection (normal, story, long, concise)
//
//  Usage:
//    nc nc-ai/nc/ml/personality.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-personality"
version "1.0.0"

// ── Persona Profiles ──────────────────────────────────────

set personas to {
    "default": {
        "prefix": "",
        "polite": true,
        "verbosity": 1.0,
        "greeting": "Sure"
    },
    "friendly": {
        "prefix": "Hey,",
        "polite": true,
        "verbosity": 1.2,
        "greeting": "Great question!"
    },
    "expert": {
        "prefix": "Here is a precise explanation:",
        "polite": false,
        "verbosity": 1.1,
        "greeting": ""
    },
    "concise": {
        "prefix": "",
        "polite": false,
        "verbosity": 0.7,
        "greeting": ""
    },
    "teacher": {
        "prefix": "Let me explain step by step.",
        "polite": true,
        "verbosity": 1.5,
        "greeting": "Good question!"
    }
}

set active_persona to "default"

// ── Set Persona ───────────────────────────────────────────

to set_persona with name:
    purpose: "Switch to a named persona"
    if get(personas, name) is not equal nil:
        set active_persona to name
        log "Persona set to: " + name
        respond with true
    log "Unknown persona: " + name
    respond with false

// ── Apply Persona ─────────────────────────────────────────

to apply_persona with text:
    purpose: "Apply active persona style to response text"
    set p to personas[active_persona]
    set result to text
    // Add greeting if polite
    if p.polite is equal true:
        if len(p.greeting) is above 0:
            set result to p.greeting + " " + result
        otherwise:
            set result to "Sure — " + result
    // Add prefix
    if len(p.prefix) is above 0:
        set result to p.prefix + " " + result
    respond with result

// ── Emotion Detection ─────────────────────────────────────

set emotion_lexicon to {
    "confused": ["why", "how", "don't understand", "unclear", "confused", "what do you mean", "explain", "huh"],
    "frustrated": ["not working", "error", "failed", "broken", "wrong", "doesn't work", "annoying", "frustrated", "stuck"],
    "curious": ["what", "tell me", "how does", "interested", "curious", "wondering", "can you explain"],
    "happy": ["great", "thanks", "awesome", "perfect", "good", "excellent", "amazing", "love it"],
    "urgent": ["asap", "immediately", "urgent", "critical", "now", "hurry", "quickly", "emergency"]
}

to detect_emotion with text:
    purpose: "Detect user emotion from input text"
    set t to lower(text)
    set best_emotion to "neutral"
    set best_score to 0
    repeat for each entry in emotion_lexicon:
        set emotion to entry.key
        set keywords to entry.value
        set score to 0
        repeat for each kw in keywords:
            if contains(t, kw):
                set score to score + 1
        if score is above best_score:
            set best_score to score
            set best_emotion to emotion
    respond with best_emotion

// ── Emotional Response Prefix ─────────────────────────────

to emotional_prefix with emotion:
    purpose: "Generate an empathetic prefix based on detected emotion"
    if emotion is equal "confused":
        respond with "Let me break this down simply."
    if emotion is equal "frustrated":
        respond with "I understand this is frustrating. Let me help."
    if emotion is equal "curious":
        respond with "Great question!"
    if emotion is equal "happy":
        respond with "Glad to help!"
    if emotion is equal "urgent":
        respond with "Right away."
    respond with ""

// ── Story Mode ────────────────────────────────────────────

to story_from_path with path:
    purpose: "Turn a reasoning path into a narrative story"
    if len(path) is below 3:
        respond with join(path, " → ")
    set subject to path[0]
    set cause to path[1]
    set effect to path[len(path) - 1]
    set story to "Think of it this way: " + subject + " involves " + cause + ". "
    set story to story + "When this happens, " + cause + " acts in a way that leads to " + effect + ". "
    set story to story + "That is why " + subject + " ultimately results in " + effect + "."
    // Add mid-path details if available
    if len(path) is above 3:
        set mid_parts to []
        set i to 2
        repeat while i is below len(path) - 1:
            append path[i] to mid_parts
            set i to i + 1
        set mid_text to join(mid_parts, ", then ")
        set story to story + " Along the way, " + mid_text + " also plays a role."
    respond with story

// ── Long-Form Mode ────────────────────────────────────────

to long_form with path:
    purpose: "Generate a detailed multi-paragraph explanation"
    if len(path) is below 3:
        respond with join(path, " relates to ")
    set subject to path[0]
    set cause to path[1]
    set effect to path[len(path) - 1]
    // Part 1: Explain
    set part1 to subject + " leads to " + effect + " because it involves " + cause + "."
    // Part 2: Elaborate
    set part2 to "In more detail, " + cause + " influences how the system behaves, which produces " + effect + ". "
    set part2 to part2 + "This connection between " + cause + " and " + effect + " is a fundamental relationship."
    // Part 3: Summarize
    set part3 to "So overall, " + subject + " is connected to " + effect + " through " + cause + "."
    // Add intermediate steps
    if len(path) is above 3:
        set chain_parts to []
        set i to 1
        repeat while i is below len(path):
            append path[i] to chain_parts
            set i to i + 1
        set chain_text to join(chain_parts, " → ")
        set part2 to part2 + " The full chain is: " + subject + " → " + chain_text + "."
    set result to part1 + "\n\n" + part2 + "\n\n" + part3
    respond with result

// ── Concise Mode ──────────────────────────────────────────

to concise_response with path:
    purpose: "Generate a brief, direct response"
    if len(path) is below 2:
        respond with path[0]
    if len(path) is equal 2:
        respond with path[0] + " → " + path[1]
    set subject to path[0]
    set effect to path[len(path) - 1]
    respond with subject + " leads to " + effect + "."

// ── Mode Selection ────────────────────────────────────────

to generate_by_mode with path, mode:
    purpose: "Generate response in the specified mode"
    if mode is equal "story":
        set result to run story_from_path with path
        respond with result
    if mode is equal "long":
        set result to run long_form with path
        respond with result
    if mode is equal "concise":
        set result to run concise_response with path
        respond with result
    // Default: normal (single sentence via fluency)
    respond with nil

// ── Full Personality Response ─────────────────────────────

to personality_response with user_input, path, mode:
    purpose: "Generate response with emotion + persona + mode"
    // Detect emotion
    set emotion to run detect_emotion with user_input
    set emo_prefix to run emotional_prefix with emotion
    // Generate content by mode
    set content to run generate_by_mode with path, mode
    // If nil, caller should use fluency engine
    if content is equal nil:
        respond with nil
    // Combine emotion + content
    set full to ""
    if len(emo_prefix) is above 0:
        set full to emo_prefix + " "
    set full to full + content
    // Apply persona
    set final to run apply_persona with full
    respond with final

// ── Auto-Detect Best Mode ─────────────────────────────────

to auto_mode with user_input:
    purpose: "Automatically choose response mode based on input"
    set t to lower(user_input)
    // Detect mode signals
    if contains(t, "explain") or contains(t, "detail") or contains(t, "in depth"):
        respond with "long"
    if contains(t, "story") or contains(t, "example") or contains(t, "imagine"):
        respond with "story"
    if contains(t, "brief") or contains(t, "short") or contains(t, "quick"):
        respond with "concise"
    if contains(t, "why") or contains(t, "how does"):
        respond with "story"
    respond with "normal"

// ── Personality Stats ─────────────────────────────────────

to personality_stats:
    purpose: "Return personality engine statistics"
    respond with {
        "active_persona": active_persona,
        "persona_count": len(personas),
        "emotion_categories": len(emotion_lexicon)
    }
