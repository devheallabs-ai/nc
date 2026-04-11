// ═══════════════════════════════════════════════════════════
//  NC ML — 7-Layer Abstraction Hierarchy
//
//  Builds multi-level compression of meaning using category
//  theory + MDL (Minimum Description Length) principles.
//
//  Hierarchy:
//    L1: Tokens        → "dog", "coffee"
//    L2: Patterns      → "dog runs", "coffee is hot"
//    L3: Concepts      → animal, drink (cluster groups)
//    L4: Relations     → (animal → action)
//    L5: Schemas       → (X → does → Y)
//    L6: Meta-schemas  → (agent → action → object)
//    L7: Principles    → (cause → effect)
//
//  Usage:
//    nc nc-ai/nc/ml/hierarchy.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-hierarchy"
version "1.0.0"

configure:
    cluster_threshold is 0.75
    min_pattern_freq is 3
    max_pattern_len is 5

// ── Hierarchy Storage ─────────────────────────────────────

set hierarchy to {
    "L1_tokens": {},
    "L2_patterns": {},
    "L3_concepts": {},
    "L4_relations": {},
    "L5_schemas": {},
    "L6_meta_schemas": {},
    "L7_principles": {}
}

set pattern_freq to {}
set concept_members to {}
set schema_instances to {}

// ── L1: Token Registration ────────────────────────────────

to register_token with token:
    purpose: "Register a token at L1"
    set t to lower(trim(token))
    set hierarchy.L1_tokens[t] to get(hierarchy.L1_tokens, t, 0) + 1
    respond with t

// ── L2: Pattern Extraction ────────────────────────────────

to extract_patterns with tokens, n:
    purpose: "Extract n-gram patterns from token sequence"
    set patterns to []
    set i to 0
    repeat while i is below len(tokens) - n + 1:
        set gram to []
        set j to 0
        repeat while j is below n:
            append tokens[i + j] to gram
            set j to j + 1
        set key to join(gram, " ")
        set pattern_freq[key] to get(pattern_freq, key, 0) + 1
        if pattern_freq[key] is above 1:
            set hierarchy.L2_patterns[key] to pattern_freq[key]
            append key to patterns
        set i to i + 1
    respond with patterns

// ── L2: Get Frequent Patterns ─────────────────────────────

to get_frequent_patterns with min_freq:
    purpose: "Return patterns above minimum frequency"
    set results to []
    repeat for each entry in pattern_freq:
        if entry.value is above min_freq:
            append {"pattern": entry.key, "freq": entry.value} to results
    respond with results

// ── L3: Concept Clustering ────────────────────────────────

to cluster_concepts with embedding_store, threshold:
    purpose: "Group similar words into concepts using cosine similarity"
    set words to []
    repeat for each entry in embedding_store:
        append entry.key to words
    set clusters to []
    set used to {}
    set i to 0
    repeat while i is below len(words):
        set w to words[i]
        if get(used, w) is equal nil:
            set group to [w]
            set j to i + 1
            repeat while j is below len(words):
                set w2 to words[j]
                if get(used, w2) is equal nil:
                    set vec_w to get(embedding_store, w)
                    set vec_w2 to get(embedding_store, w2)
                    set dot_val to tensor_dot(vec_w, vec_w2)
                    set norm_w to tensor_norm(vec_w)
                    set norm_w2 to tensor_norm(vec_w2)
                    set denom to norm_w * norm_w2 + 0.000001
                    set sim to dot_val / denom
                    if sim is above threshold:
                        append w2 to group
                        set used[w2] to true
                set j to j + 1
            // Name concept by first member
            set concept_name to "concept_" + w
            set hierarchy.L3_concepts[concept_name] to group
            set concept_members[concept_name] to group
            append {"name": concept_name, "members": group} to clusters
        set i to i + 1
    log "Found " + string(len(clusters)) + " concept clusters"
    respond with clusters

// ── L3: Find Concept For Word ─────────────────────────────

to find_concept with word:
    purpose: "Find which concept a word belongs to"
    set w to lower(trim(word))
    repeat for each entry in concept_members:
        repeat for each member in entry.value:
            if member is equal w:
                respond with entry.key
    respond with nil

// ── L4: Relation Extraction ──────────────────────────────

to extract_relations with tokens:
    purpose: "Extract subject-relation-object patterns from tokens"
    set relations to []
    set relation_words to ["is", "has", "gives", "causes", "contains", "makes", "leads", "produces", "eats", "drinks", "runs", "uses", "needs", "creates"]
    set i to 0
    repeat while i is below len(tokens) - 2:
        set mid to lower(tokens[i + 1])
        set is_rel to false
        repeat for each rw in relation_words:
            if mid is equal rw:
                set is_rel to true
        if is_rel is equal true:
            set rel to {"subject": tokens[i], "relation": mid, "object": tokens[i + 2]}
            append rel to relations
            set key to tokens[i] + "|" + mid + "|" + tokens[i + 2]
            set hierarchy.L4_relations[key] to rel
        set i to i + 1
    respond with relations

// ── L5: Schema Generalization ─────────────────────────────

to generalize_to_schema with relation:
    purpose: "Generalize a concrete relation to an abstract schema"
    set schema to {"X": relation.subject, "action": relation.relation, "Y": relation.object}
    set schema_key to "X " + relation.relation + " Y"
    set hierarchy.L5_schemas[schema_key] to get(hierarchy.L5_schemas, schema_key, 0) + 1
    // Track instances
    if get(schema_instances, schema_key) is equal nil:
        set schema_instances[schema_key] to []
    append relation to schema_instances[schema_key]
    respond with schema_key

// ── L5: Batch Schema Extraction ───────────────────────────

to extract_schemas with relations:
    purpose: "Generalize all relations to schemas"
    set schemas to []
    repeat for each rel in relations:
        set sk to run generalize_to_schema with rel
        append sk to schemas
    respond with schemas

// ── L6: Meta-Schema Detection ─────────────────────────────

to detect_meta_schemas:
    purpose: "Detect meta-schemas from schema patterns"
    set meta to {}
    // Group schemas by structure
    set action_verbs to ["is", "has", "gives", "causes", "contains", "makes", "leads"]
    set state_verbs to ["is", "has"]
    set causal_verbs to ["causes", "gives", "makes", "leads", "produces", "creates"]
    set consumptive_verbs to ["eats", "drinks", "uses", "needs"]
    repeat for each entry in hierarchy.L5_schemas:
        set parts to split(entry.key, " ")
        if len(parts) is above 1:
            set verb to parts[1]
            // Classify into meta categories
            set is_causal to false
            repeat for each cv in causal_verbs:
                if verb is equal cv:
                    set is_causal to true
            if is_causal is equal true:
                set meta["agent_causes_effect"] to get(meta, "agent_causes_effect", 0) + entry.value
                set hierarchy.L6_meta_schemas["agent_causes_effect"] to meta["agent_causes_effect"]
            set is_state to false
            repeat for each sv in state_verbs:
                if verb is equal sv:
                    set is_state to true
            if is_state is equal true:
                set meta["entity_has_property"] to get(meta, "entity_has_property", 0) + entry.value
                set hierarchy.L6_meta_schemas["entity_has_property"] to meta["entity_has_property"]
            set is_consume to false
            repeat for each uv in consumptive_verbs:
                if verb is equal uv:
                    set is_consume to true
            if is_consume is equal true:
                set meta["agent_consumes_resource"] to get(meta, "agent_consumes_resource", 0) + entry.value
                set hierarchy.L6_meta_schemas["agent_consumes_resource"] to meta["agent_consumes_resource"]
    log "Detected " + string(len(meta)) + " meta-schemas"
    respond with meta

// ── L7: Principle Extraction ──────────────────────────────

to extract_principles:
    purpose: "Extract high-level principles from meta-schemas"
    set principles to []
    // Cause-effect principle
    if get(hierarchy.L6_meta_schemas, "agent_causes_effect") is not equal nil:
        set p to {"name": "causality", "description": "actions produce effects", "strength": hierarchy.L6_meta_schemas["agent_causes_effect"]}
        append p to principles
        set hierarchy.L7_principles["causality"] to p
    // Property principle
    if get(hierarchy.L6_meta_schemas, "entity_has_property") is not equal nil:
        set p to {"name": "attribution", "description": "entities have properties", "strength": hierarchy.L6_meta_schemas["entity_has_property"]}
        append p to principles
        set hierarchy.L7_principles["attribution"] to p
    // Consumption principle
    if get(hierarchy.L6_meta_schemas, "agent_consumes_resource") is not equal nil:
        set p to {"name": "consumption", "description": "agents consume resources", "strength": hierarchy.L6_meta_schemas["agent_consumes_resource"]}
        append p to principles
        set hierarchy.L7_principles["consumption"] to p
    log "Extracted " + string(len(principles)) + " principles"
    respond with principles

// ── Full Hierarchy Build ──────────────────────────────────

to build_hierarchy with tokens, embedding_store:
    purpose: "Build complete 7-layer hierarchy from token sequence"
    log "Building abstraction hierarchy..."
    // L1: Register tokens
    repeat for each t in tokens:
        run register_token with t
    log "  L1: " + string(len(hierarchy.L1_tokens)) + " tokens"
    // L2: Extract patterns (bigrams + trigrams)
    run extract_patterns with tokens, 2
    run extract_patterns with tokens, 3
    log "  L2: " + string(len(hierarchy.L2_patterns)) + " patterns"
    // L3: Cluster concepts
    set clusters to run cluster_concepts with embedding_store, cluster_threshold
    log "  L3: " + string(len(clusters)) + " concepts"
    // L4: Extract relations
    set relations to run extract_relations with tokens
    log "  L4: " + string(len(relations)) + " relations"
    // L5: Generalize to schemas
    set schemas to run extract_schemas with relations
    log "  L5: " + string(len(hierarchy.L5_schemas)) + " schemas"
    // L6: Detect meta-schemas
    set metas to run detect_meta_schemas
    log "  L6: " + string(len(metas)) + " meta-schemas"
    // L7: Extract principles
    set principles to run extract_principles
    log "  L7: " + string(len(principles)) + " principles"
    respond with hierarchy

// ── Query Hierarchy ───────────────────────────────────────

to query_level with level:
    purpose: "Query a specific hierarchy level"
    if level is equal 1:
        respond with hierarchy.L1_tokens
    if level is equal 2:
        respond with hierarchy.L2_patterns
    if level is equal 3:
        respond with hierarchy.L3_concepts
    if level is equal 4:
        respond with hierarchy.L4_relations
    if level is equal 5:
        respond with hierarchy.L5_schemas
    if level is equal 6:
        respond with hierarchy.L6_meta_schemas
    if level is equal 7:
        respond with hierarchy.L7_principles
    respond with nil

// ── Pattern Merge (MDL-based) ─────────────────────────────

to merge_patterns:
    purpose: "Merge frequent patterns into higher-order patterns (MDL compression)"
    set merged to []
    set frequent to run get_frequent_patterns with min_pattern_freq
    repeat for each p in frequent:
        set gain to p.freq * len(split(p.pattern, " "))
        if gain is above 10:
            append {"pattern": p.pattern, "gain": gain} to merged
    log "Merged " + string(len(merged)) + " high-gain patterns"
    respond with merged

// ── Hierarchy Stats ───────────────────────────────────────

to hierarchy_stats:
    purpose: "Return statistics for each hierarchy level"
    respond with {
        "L1_tokens": len(hierarchy.L1_tokens),
        "L2_patterns": len(hierarchy.L2_patterns),
        "L3_concepts": len(hierarchy.L3_concepts),
        "L4_relations": len(hierarchy.L4_relations),
        "L5_schemas": len(hierarchy.L5_schemas),
        "L6_meta_schemas": len(hierarchy.L6_meta_schemas),
        "L7_principles": len(hierarchy.L7_principles)
    }
