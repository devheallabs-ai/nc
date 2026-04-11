// ═══════════════════════════════════════════════════════════
//  NC ML — Fluency Engine (Natural Language Output)
//
//  Transforms reasoning paths into natural, human-readable
//  sentences using templates, connectors, and variation.
//
//  Features:
//    - Role extraction (subject, cause, effect)
//    - Template scoring + selection
//    - Sentence connectors for flow
//    - Synonym variation (anti-repetition)
//    - Multi-sentence paragraph generation
//    - Phrase memory (n-gram learning)
//
//  Usage:
//    nc nc-ai/nc/ml/fluency.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-fluency"
version "1.0.0"

// ── Phrase Memory ─────────────────────────────────────────

set phrase_mem to {}

to store_phrase with tokens, n:
    purpose: "Store n-gram phrases for learning"
    set i to 0
    repeat while i is below len(tokens) - n + 1:
        set gram to []
        set j to 0
        repeat while j is below n:
            append tokens[i + j] to gram
            set j to j + 1
        set key to join(gram, " ")
        set phrase_mem[key] to get(phrase_mem, key, 0) + 1
        set i to i + 1
    respond with true

to get_phrases with min_freq:
    purpose: "Get phrases above minimum frequency"
    set results to []
    repeat for each entry in phrase_mem:
        if entry.value is above min_freq:
            append {"phrase": entry.key, "freq": entry.value} to results
    respond with results

// ── Templates ─────────────────────────────────────────────

set templates to [
    "{subject} contains {cause}, which leads to {effect}.",
    "{subject} works because {cause} affects {effect}.",
    "The reason is that {cause} influences {effect} through {subject}.",
    "{subject} is important because {cause} produces {effect}.",
    "In simple terms, {subject} involves {cause}, resulting in {effect}.",
    "Because of {cause}, {subject} leads to {effect}.",
    "{cause} plays a key role in how {subject} produces {effect}.",
    "When it comes to {subject}, {cause} directly impacts {effect}.",
    "{subject} relies on {cause} to achieve {effect}.",
    "Through {cause}, {subject} is able to produce {effect}."
]

set short_templates to [
    "{subject} leads to {effect}.",
    "{subject} involves {cause}.",
    "{cause} produces {effect}."
]

// ── Role Extraction ───────────────────────────────────────

to extract_roles with path:
    purpose: "Extract subject, cause, effect from a reasoning path"
    set subject to ""
    set cause to ""
    set effect to ""
    if len(path) is above 0:
        set subject to path[0]
    if len(path) is above 1:
        set cause to path[1]
    if len(path) is above 2:
        set effect to path[len(path) - 1]
    otherwise:
        set effect to cause
    respond with {"subject": subject, "cause": cause, "effect": effect}

// ── Template Scoring ──────────────────────────────────────

to template_score with template, roles:
    purpose: "Score how well a template fits the available roles"
    set score to 0
    if contains(template, "{subject}"):
        if len(roles.subject) is above 0:
            set score to score + 1
    if contains(template, "{cause}"):
        if len(roles.cause) is above 0:
            set score to score + 1
    if contains(template, "{effect}"):
        if len(roles.effect) is above 0:
            set score to score + 1
    respond with score

// ── Choose Best Template ──────────────────────────────────

to choose_template with roles, template_list:
    purpose: "Select best-scoring template for given roles"
    set best_template to template_list[0]
    set best_score to 0
    set idx to 0
    repeat for each t in template_list:
        set s to run template_score with t, roles
        // Add index-based variation to avoid always picking the same
        set varied_score to s + (idx * 0.01)
        if varied_score is above best_score:
            set best_score to varied_score
            set best_template to t
        set idx to idx + 1
    respond with best_template

// ── Fill Template ─────────────────────────────────────────

to fill_template with template, roles:
    purpose: "Fill a template with extracted roles"
    set result to template
    set result to replace(result, "{subject}", roles.subject)
    set result to replace(result, "{cause}", roles.cause)
    set result to replace(result, "{effect}", roles.effect)
    set result to replace(result, "{mechanism}", roles.cause)
    respond with result

// ── Generate Sentence ─────────────────────────────────────

to generate_sentence with path:
    purpose: "Generate a single fluent sentence from a reasoning path"
    set roles to run extract_roles with path
    if len(path) is above 2:
        set template to run choose_template with roles, templates
    otherwise:
        set template to run choose_template with roles, short_templates
    set sentence to run fill_template with template, roles
    respond with sentence

// ── Connectors ────────────────────────────────────────────

set connectors to [
    "Also,",
    "In addition,",
    "Furthermore,",
    "So,",
    "Because of this,",
    "As a result,",
    "Moreover,",
    "This means that",
    "In other words,",
    "Essentially,"
]

set connector_idx to 0

to get_connector:
    purpose: "Get next connector (round-robin for variety)"
    set c to connectors[connector_idx]
    set connector_idx to (connector_idx + 1) % len(connectors)
    respond with c

// ── Synonym Variation ─────────────────────────────────────

set synonyms to {
    "gives": ["provides", "produces", "delivers", "offers"],
    "causes": ["leads to", "results in", "creates", "triggers"],
    "contains": ["includes", "holds", "carries", "has"],
    "makes": ["produces", "creates", "generates", "forms"],
    "uses": ["employs", "utilizes", "relies on", "leverages"],
    "leads": ["guides", "directs", "results in", "drives"],
    "important": ["significant", "crucial", "essential", "key"],
    "works": ["functions", "operates", "performs", "runs"],
    "produces": ["generates", "creates", "yields", "delivers"]
}

set synonym_counter to 0

to vary with text:
    purpose: "Replace words with synonyms for variation"
    set result to text
    repeat for each entry in synonyms:
        if contains(result, entry.key):
            set options to entry.value
            set pick to synonym_counter % len(options)
            set result to replace(result, entry.key, options[pick])
            set synonym_counter to synonym_counter + 1
    respond with result

// ── Multi-Sentence Paragraph ──────────────────────────────

to generate_paragraph with path:
    purpose: "Generate a multi-sentence paragraph from a reasoning path"
    set roles to run extract_roles with path
    // Sentence 1: Main statement
    set s1 to run generate_sentence with path
    // Sentence 2: Elaboration
    set s2 to ""
    if len(path) is above 2:
        set s2 to roles.cause + " is known to influence " + roles.effect + "."
    // Sentence 3: Summary
    set s3 to ""
    if len(path) is above 3:
        set connector to run get_connector
        set s3 to connector + " " + roles.subject + " is connected to " + roles.effect + " through " + roles.cause + "."
    // Combine
    set paragraph to s1
    if len(s2) is above 0:
        set connector to run get_connector
        set paragraph to paragraph + " " + connector + " " + s2
    if len(s3) is above 0:
        set paragraph to paragraph + " " + s3
    respond with paragraph

// ── Fluent Response ───────────────────────────────────────

to fluent_response with path:
    purpose: "Generate a varied, fluent response from a reasoning path"
    set paragraph to run generate_paragraph with path
    set varied to run vary with paragraph
    respond with varied

// ── Capitalize First Letter ───────────────────────────────

to capitalize_first with text:
    purpose: "Capitalize the first letter of text"
    if len(text) is equal 0:
        respond with text
    set first to upper(substr(text, 0, 1))
    set rest to substr(text, 1, len(text) - 1)
    respond with first + rest

// ── Format Response ───────────────────────────────────────

to format_response with text:
    purpose: "Clean up and format a response"
    set result to trim(text)
    // Remove double spaces
    set result to replace(result, "  ", " ")
    // Capitalize first letter
    set result to run capitalize_first with result
    respond with result

// ── Fluency Stats ─────────────────────────────────────────

to fluency_stats:
    purpose: "Return fluency engine statistics"
    respond with {
        "templates": len(templates),
        "short_templates": len(short_templates),
        "connectors": len(connectors),
        "synonym_groups": len(synonyms),
        "phrases_learned": len(phrase_mem)
    }
