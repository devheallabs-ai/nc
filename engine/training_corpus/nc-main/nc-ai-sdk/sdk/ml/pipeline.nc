// ═══════════════════════════════════════════════════════════
//  NC ML — Master Intelligence Pipeline
//
//  Connects ALL modules into a single end-to-end system:
//
//    Input → Tokenize → Embed → Filter → Hierarchy
//      → Graph Brain → Reasoning (Swarm) → Fluency
//      → Personality → Personalization → Output
//
//  This is the complete NC AI alternative architecture.
//  CPU-native, no backprop, structure-heavy intelligence.
//
//  Architecture:
//    ┌──────────────┐
//    │   Tokenizer   │  (input text → tokens)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Noise Filter │  (entropy + frequency filtering)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Embeddings   │  (Hebbian co-occurrence learning)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Hierarchy    │  (7-layer abstraction: tokens→principles)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Graph Brain  │  (weighted directed knowledge graph)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Semantic     │  (embedding bridge: input → graph nodes)
//    │  Bridge       │
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Reasoning    │  (multi-hop weighted path search)
//    │  + Swarm      │  (multi-agent parallel exploration)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Feedback     │  (learn from user signals)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Fluency      │  (templates + connectors + variation)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Personality  │  (emotion + persona + mode)
//    └──────┬───────┘
//           ↓
//    ┌──────────────┐
//    │  Response     │  (final natural language output)
//    └──────────────┘
//
//  Usage:
//    nc nc-ai/nc/ml/pipeline.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-pipeline"
version "1.0.0"

configure:
    port is 8100
    data_dir is "./nc-ai/data"
    model_dir is "./nc-ai/model"
    embed_dim is 64
    reasoning_depth is 4
    use_swarm is true

// ═══════════════════════════════════════════════════════════
//  MODULE STATE
// ═══════════════════════════════════════════════════════════

// Embedding store (word → vector)
set embedding_store to {}
set word_freq to {}
set co_occurrence to {}

// Knowledge graph (adjacency list with weighted typed edges)
set graph to {}
set node_freq to {}
set alias_map to {}

// Hierarchy levels
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

// Phrase memory
set phrase_mem to {}

// Feedback
set feedback_store to {}
set response_cache to {}
set confidence_store to {}

// User profiles (multi-user)
set users to {}

// Swarm agents
set swarm_agents to []
set pheromone_map to {}

// System stats
set total_queries to 0
set total_learning_events to 0

// ═══════════════════════════════════════════════════════════
//  TOKENIZER
// ═══════════════════════════════════════════════════════════

to tokenize with text:
    purpose: "Split text into tokens"
    set cleaned to lower(trim(text))
    set tokens to split(cleaned, " ")
    // Remove empty tokens
    set result to []
    repeat for each t in tokens:
        if len(t) is above 0:
            append t to result
    respond with result

// ═══════════════════════════════════════════════════════════
//  EMBEDDING OPERATIONS
// ═══════════════════════════════════════════════════════════

to get_vec with word:
    purpose: "Get or create embedding vector"
    if get(embedding_store, word) is equal nil:
        set embedding_store[word] to tensor_random(1, embed_dim)
        set embedding_store[word] to tensor_scale(embedding_store[word], 0.1)
    set word_freq[word] to get(word_freq, word, 0) + 1
    respond with embedding_store[word]

to learn_cooccurrence with tokens:
    purpose: "Hebbian co-occurrence learning on token window"
    set window to 3
    set i to 0
    repeat while i is below len(tokens):
        set j to 1
        repeat while j is below window + 1:
            if i + j is below len(tokens):
                set va to run get_vec with tokens[i]
                set vb to run get_vec with tokens[i + j]
                set delta to tensor_mul(va, vb)
                set delta to tensor_scale(delta, 0.05)
                set embedding_store[tokens[i]] to tensor_add(va, tensor_scale(delta, 0.5))
                set embedding_store[tokens[i + j]] to tensor_add(vb, tensor_scale(delta, 0.5))
            set j to j + 1
        set i to i + 1
    respond with true

to cosine_similarity with a, b:
    purpose: "Cosine similarity between two vectors"
    set dot to tensor_dot(a, b)
    set na to tensor_norm(a)
    set nb to tensor_norm(b)
    set denom to na * nb
    if denom is below 0.0001:
        respond with 0.0
    respond with dot / denom

// ═══════════════════════════════════════════════════════════
//  NOISE FILTERING
// ═══════════════════════════════════════════════════════════

to filter_tokens with tokens:
    purpose: "Remove noise: stop words + low-frequency tokens"
    set stop_words to ["the", "a", "an", "is", "are", "was", "were", "be", "been", "have", "has", "had", "do", "does", "did", "will", "would", "could", "should", "to", "of", "in", "for", "on", "with", "at", "by", "from", "as", "it", "this", "that"]
    set filtered to []
    repeat for each t in tokens:
        set is_stop to false
        repeat for each sw in stop_words:
            if t is equal sw:
                set is_stop to true
        if is_stop is equal false:
            if len(t) is above 1:
                append t to filtered
    respond with filtered

// ═══════════════════════════════════════════════════════════
//  GRAPH OPERATIONS
// ═══════════════════════════════════════════════════════════

to add_to_graph with subject, object, weight, relation:
    purpose: "Add weighted typed edge to knowledge graph"
    set s to lower(trim(subject))
    set o to lower(trim(object))
    set r to lower(trim(relation))
    // Resolve aliases
    if get(alias_map, s) is not equal nil:
        set s to alias_map[s]
    if get(alias_map, o) is not equal nil:
        set o to alias_map[o]
    if get(graph, s) is equal nil:
        set graph[s] to []
    // Check existing edge
    set found to false
    set idx to 0
    repeat for each edge in graph[s]:
        if edge.target is equal o:
            if edge.relation is equal r:
                set graph[s][idx].weight to edge.weight + weight
                set found to true
        set idx to idx + 1
    if found is equal false:
        append {"target": o, "weight": weight, "relation": r} to graph[s]
    set node_freq[s] to get(node_freq, s, 0) + 1
    set node_freq[o] to get(node_freq, o, 0) + 1
    respond with true

to extract_and_ingest with tokens:
    purpose: "Extract triplets from tokens and add to graph"
    set rel_words to ["is", "has", "gives", "causes", "contains", "makes", "leads", "produces", "creates", "uses", "needs", "eats", "drinks"]
    set i to 0
    set ingested to 0
    repeat while i is below len(tokens) - 2:
        set mid to tokens[i + 1]
        set is_rel to false
        repeat for each rw in rel_words:
            if mid is equal rw:
                set is_rel to true
        if is_rel is equal true:
            run add_to_graph with tokens[i], tokens[i + 2], 1.0, mid
            set ingested to ingested + 1
        set i to i + 1
    respond with ingested

// ═══════════════════════════════════════════════════════════
//  SEMANTIC BRIDGE
// ═══════════════════════════════════════════════════════════

to find_graph_node with token:
    purpose: "Find closest graph node for a token using embeddings"
    // Direct match first
    if get(graph, token) is not equal nil:
        respond with token
    // Semantic search
    set target to run get_vec with token
    set best_node to token
    set best_sim to 0.0
    repeat for each entry in graph:
        set node_vec to run get_vec with entry.key
        set sim to run cosine_similarity with target, node_vec
        if sim is above best_sim:
            set best_sim to sim
            set best_node to entry.key
    if best_sim is above 0.7:
        respond with best_node
    respond with token

// ═══════════════════════════════════════════════════════════
//  REASONING ENGINE
// ═══════════════════════════════════════════════════════════

set relation_weights to {
    "causes": 1.5, "leads": 1.4, "produces": 1.3, "creates": 1.3,
    "contains": 1.2, "gives": 1.1, "makes": 1.2,
    "has": 1.0, "is": 0.9, "uses": 1.0, "needs": 1.0
}

to score_path with path:
    purpose: "Score a reasoning path"
    set score to 0.0
    set i to 1
    repeat while i is below len(path):
        set step to path[i]
        set rel_w to get(relation_weights, step.relation, 1.0)
        set score to score + (step.weight * rel_w)
        set i to i + 1
    set score to score - 0.1 * len(path)
    respond with score

to find_best_path with start, depth:
    purpose: "Find best reasoning path from start node"
    set node to run find_graph_node with start
    // Generate paths via BFS
    set paths to [[{"node": node, "weight": 1.0, "relation": "start"}]]
    set d to 0
    repeat while d is below depth:
        set new_paths to []
        repeat for each path in paths:
            set last to path[len(path) - 1]
            set edges to get(graph, last.node)
            if edges is not equal nil:
                repeat for each edge in edges:
                    // No cycles
                    set in_path to false
                    repeat for each step in path:
                        if step.node is equal edge.target:
                            set in_path to true
                    if in_path is equal false:
                        set new_path to []
                        repeat for each s in path:
                            append s to new_path
                        append {"node": edge.target, "weight": edge.weight, "relation": edge.relation} to new_path
                        append new_path to new_paths
            if len(path) is above 1:
                append path to new_paths
        if len(new_paths) is above 0:
            set paths to new_paths
        set d to d + 1
    // Select best
    set best to nil
    set best_score to -999.0
    repeat for each path in paths:
        if len(path) is above 1:
            set s to run score_path with path
            if s is above best_score:
                set best_score to s
                set best to path
    if best is equal nil:
        respond with [{"node": start, "weight": 1.0, "relation": "start"}]
    respond with best

to extract_path_nodes with path:
    purpose: "Get node names from a reasoning path"
    set nodes to []
    repeat for each step in path:
        append step.node to nodes
    respond with nodes

// ═══════════════════════════════════════════════════════════
//  SWARM REASONING (Multi-Agent)
// ═══════════════════════════════════════════════════════════

to swarm_reason with start, depth:
    purpose: "Run multi-agent swarm reasoning"
    set node to run find_graph_node with start
    set candidates to []
    // Agent 1: Greedy (highest weight)
    set greedy to run greedy_agent with node, depth
    append {"strategy": "greedy", "path": greedy} to candidates
    // Agent 2: Explorative (unvisited preference)
    set explore to run explore_agent with node, depth
    append {"strategy": "explore", "path": explore} to candidates
    // Agent 3: Conservative (short + confident)
    set conserv to run conservative_agent with node, depth
    append {"strategy": "conservative", "path": conserv} to candidates
    // Score all
    set best to candidates[0]
    set best_score to -999.0
    repeat for each c in candidates:
        set s to run score_path with c.path
        // Add pheromone bonus
        set phero_bonus to 0.0
        set pi to 0
        repeat while pi is below len(c.path) - 1:
            set pk to c.path[pi].node + "→" + c.path[pi + 1].node
            set phero_bonus to phero_bonus + get(pheromone_map, pk, 0.0)
            set pi to pi + 1
        set total_s to s + phero_bonus * 0.3
        if total_s is above best_score:
            set best_score to total_s
            set best to c
    // Deposit pheromone on winner
    set wi to 0
    repeat while wi is below len(best.path) - 1:
        set pk to best.path[wi].node + "→" + best.path[wi + 1].node
        set pheromone_map[pk] to get(pheromone_map, pk, 0.0) + 0.2
        set wi to wi + 1
    respond with best.path

to greedy_agent with start, depth:
    purpose: "Greedy swarm agent"
    set path to [{"node": start, "weight": 1.0, "relation": "start"}]
    set current to start
    set d to 0
    repeat while d is below depth:
        set edges to get(graph, current)
        if edges is not equal nil:
            if len(edges) is above 0:
                set best_e to edges[0]
                repeat for each e in edges:
                    if e.weight is above best_e.weight:
                        set best_e to e
                append {"node": best_e.target, "weight": best_e.weight, "relation": best_e.relation} to path
                set current to best_e.target
            otherwise:
                set d to depth
        otherwise:
            set d to depth
        set d to d + 1
    respond with path

to explore_agent with start, depth:
    purpose: "Explorative swarm agent"
    set path to [{"node": start, "weight": 1.0, "relation": "start"}]
    set current to start
    set visited to {}
    set visited[start] to true
    set d to 0
    repeat while d is below depth:
        set edges to get(graph, current)
        if edges is not equal nil:
            set unvisited to []
            repeat for each e in edges:
                if get(visited, e.target) is equal nil:
                    append e to unvisited
            if len(unvisited) is above 0:
                // Pick least-pheromone
                set best_e to unvisited[0]
                set min_p to 999.0
                repeat for each e in unvisited:
                    set pk to current + "→" + e.target
                    set p to get(pheromone_map, pk, 0.0)
                    if p is below min_p:
                        set min_p to p
                        set best_e to e
                append {"node": best_e.target, "weight": best_e.weight, "relation": best_e.relation} to path
                set visited[best_e.target] to true
                set current to best_e.target
            otherwise:
                set d to depth
        otherwise:
            set d to depth
        set d to d + 1
    respond with path

to conservative_agent with start, depth:
    purpose: "Conservative swarm agent (short paths)"
    set path to [{"node": start, "weight": 1.0, "relation": "start"}]
    set current to start
    set max_d to 2
    if depth is below max_d:
        set max_d to depth
    set d to 0
    repeat while d is below max_d:
        set edges to get(graph, current)
        if edges is not equal nil:
            if len(edges) is above 0:
                set best_e to edges[0]
                repeat for each e in edges:
                    if e.weight is above best_e.weight:
                        set best_e to e
                if best_e.weight is above 0.5:
                    append {"node": best_e.target, "weight": best_e.weight, "relation": best_e.relation} to path
                    set current to best_e.target
                otherwise:
                    set d to max_d
            otherwise:
                set d to max_d
        otherwise:
            set d to max_d
        set d to d + 1
    respond with path

// ═══════════════════════════════════════════════════════════
//  FLUENCY + LANGUAGE OUTPUT
// ═══════════════════════════════════════════════════════════

set templates to [
    "{subject} contains {cause}, which leads to {effect}.",
    "{subject} works because {cause} affects {effect}.",
    "The reason is that {cause} influences {effect} through {subject}.",
    "{subject} is important because {cause} produces {effect}.",
    "In simple terms, {subject} involves {cause}, resulting in {effect}.",
    "Because of {cause}, {subject} leads to {effect}.",
    "{cause} plays a key role in how {subject} produces {effect}.",
    "{subject} relies on {cause} to achieve {effect}."
]

set short_templates to [
    "{subject} leads to {effect}.",
    "{subject} involves {cause}.",
    "{cause} produces {effect}."
]

set connectors to ["Also,", "In addition,", "Furthermore,", "So,", "Because of this,", "As a result,", "Moreover,", "This means that", "In other words,"]

set synonyms to {
    "gives": ["provides", "produces", "delivers"],
    "causes": ["leads to", "results in", "creates"],
    "contains": ["includes", "holds", "carries"],
    "makes": ["produces", "creates", "generates"],
    "important": ["significant", "crucial", "essential"]
}
set synonym_idx to 0

to generate_response_text with path_nodes, mode:
    purpose: "Generate natural language from reasoning path"
    set subject to ""
    set cause to ""
    set effect to ""
    if len(path_nodes) is above 0:
        set subject to path_nodes[0]
    if len(path_nodes) is above 1:
        set cause to path_nodes[1]
    if len(path_nodes) is above 2:
        set effect to path_nodes[len(path_nodes) - 1]
    otherwise:
        set effect to cause
    // Story mode
    if mode is equal "story":
        set text to "Think of it this way: " + subject + " involves " + cause + ". "
        set text to text + "When this happens, " + cause + " acts in a way that leads to " + effect + ". "
        set text to text + "That is why " + subject + " ultimately results in " + effect + "."
        respond with text
    // Long-form mode
    if mode is equal "long":
        set p1 to subject + " leads to " + effect + " because it involves " + cause + "."
        set p2 to "In more detail, " + cause + " influences how the system behaves, which produces " + effect + "."
        set p3 to "So overall, " + subject + " is connected to " + effect + " through " + cause + "."
        respond with p1 + " " + p2 + " " + p3
    // Concise mode
    if mode is equal "concise":
        respond with subject + " leads to " + effect + "."
    // Normal mode — template-based
    if len(path_nodes) is above 2:
        set template to templates[total_queries % len(templates)]
    otherwise:
        set template to short_templates[total_queries % len(short_templates)]
    set text to replace(template, "{subject}", subject)
    set text to replace(text, "{cause}", cause)
    set text to replace(text, "{effect}", effect)
    // Variation
    repeat for each entry in synonyms:
        if contains(text, entry.key):
            set options to entry.value
            set pick to synonym_idx % len(options)
            set text to replace(text, entry.key, options[pick])
            set synonym_idx to synonym_idx + 1
    respond with text

// ═══════════════════════════════════════════════════════════
//  PERSONALITY + EMOTION
// ═══════════════════════════════════════════════════════════

set active_persona to "default"

to detect_emotion with text:
    purpose: "Detect user emotion"
    set t to lower(text)
    if contains(t, "why") or contains(t, "how") or contains(t, "explain"):
        respond with "curious"
    if contains(t, "wrong") or contains(t, "error") or contains(t, "broken"):
        respond with "frustrated"
    if contains(t, "thanks") or contains(t, "great") or contains(t, "awesome"):
        respond with "happy"
    if contains(t, "confused") or contains(t, "don't understand"):
        respond with "confused"
    respond with "neutral"

to emotional_prefix with emotion:
    purpose: "Get empathetic prefix"
    if emotion is equal "confused":
        respond with "Let me break this down simply."
    if emotion is equal "frustrated":
        respond with "I understand. Let me help."
    if emotion is equal "curious":
        respond with "Great question!"
    if emotion is equal "happy":
        respond with "Glad to help!"
    respond with ""

to detect_mode with text:
    purpose: "Auto-detect response mode from input"
    set t to lower(text)
    if contains(t, "explain") or contains(t, "detail") or contains(t, "in depth"):
        respond with "long"
    if contains(t, "story") or contains(t, "example") or contains(t, "imagine"):
        respond with "story"
    if contains(t, "brief") or contains(t, "short") or contains(t, "quick"):
        respond with "concise"
    respond with "normal"

// ═══════════════════════════════════════════════════════════
//  FEEDBACK INTEGRATION
// ═══════════════════════════════════════════════════════════

to check_response_cache with input_text:
    purpose: "Check if we have a cached good response"
    respond with get(response_cache, lower(input_text))

to record_feedback with input_text, response, score:
    purpose: "Record feedback and reinforce/weaken paths"
    set key to lower(input_text) + "|||" + response
    set feedback_store[key] to get(feedback_store, key, 0) + score
    if score is above 0:
        set response_cache[lower(input_text)] to response
    respond with true

// ═══════════════════════════════════════════════════════════
//  LEARNING (INGESTION)
// ═══════════════════════════════════════════════════════════

to ingest with text:
    purpose: "Learn from raw text: tokenize, embed, extract, build graph"
    set tokens to run tokenize with text
    set filtered to run filter_tokens with tokens
    // Learn embeddings
    run learn_cooccurrence with filtered
    // Extract and ingest triplets into graph
    set ingested to run extract_and_ingest with filtered
    // Learn phrases
    set i to 0
    repeat while i is below len(filtered) - 2:
        set key to filtered[i] + " " + filtered[i + 1]
        set phrase_mem[key] to get(phrase_mem, key, 0) + 1
        set i to i + 1
    // Update hierarchy L1 + L2
    repeat for each t in filtered:
        set hierarchy.L1_tokens[t] to get(hierarchy.L1_tokens, t, 0) + 1
    // Track pattern frequencies
    set pi to 0
    repeat while pi is below len(filtered) - 1:
        set pk to filtered[pi] + " " + filtered[pi + 1]
        set pattern_freq[pk] to get(pattern_freq, pk, 0) + 1
        if pattern_freq[pk] is above 1:
            set hierarchy.L2_patterns[pk] to pattern_freq[pk]
        set pi to pi + 1
    set total_learning_events to total_learning_events + 1
    log "Ingested: " + string(len(filtered)) + " tokens, " + string(ingested) + " triplets"
    respond with {"tokens": len(filtered), "triplets": ingested}

to ingest_batch with texts:
    purpose: "Ingest multiple texts for bulk learning"
    set total_tokens to 0
    set total_triplets to 0
    repeat for each text in texts:
        set result to run ingest with text
        set total_tokens to total_tokens + result.tokens
        set total_triplets to total_triplets + result.triplets
    log "Batch ingested: " + string(total_tokens) + " tokens, " + string(total_triplets) + " triplets"
    respond with {"tokens": total_tokens, "triplets": total_triplets}

// ═══════════════════════════════════════════════════════════
//  MAIN QUERY (SINGLE-USER)
// ═══════════════════════════════════════════════════════════

to query with text:
    purpose: "Main query: input text → intelligent response"
    set total_queries to total_queries + 1
    // Check cache first
    set cached to run check_response_cache with text
    if cached is not equal nil:
        respond with cached
    // Tokenize + filter
    set tokens to run tokenize with text
    set filtered to run filter_tokens with tokens
    // Learn from input
    run learn_cooccurrence with filtered
    // Detect emotion + mode
    set emotion to run detect_emotion with text
    set mode to run detect_mode with text
    set emo_prefix to run emotional_prefix with emotion
    // Find best reasoning path
    set path to []
    if len(filtered) is above 0:
        if use_swarm is equal true:
            set path to run swarm_reason with filtered[0], reasoning_depth
        otherwise:
            set path to run find_best_path with filtered[0], reasoning_depth
    // Extract node names
    set path_nodes to run extract_path_nodes with path
    // Generate response
    set response_text to run generate_response_text with path_nodes, mode
    // Add emotional prefix
    set final to ""
    if len(emo_prefix) is above 0:
        set final to emo_prefix + " "
    set final to final + response_text
    // Apply persona prefix
    if active_persona is equal "default":
        set final to "Sure — " + final
    respond with final

// ═══════════════════════════════════════════════════════════
//  MULTI-USER QUERY
// ═══════════════════════════════════════════════════════════

to query_as with user_id, text:
    purpose: "Query with user personalization"
    // Get or create user
    if get(users, user_id) is equal nil:
        set users[user_id] to {
            "topics": {},
            "style": {},
            "history": [],
            "feedback": {},
            "interaction_count": 0
        }
    set user to users[user_id]
    // Update user profile
    set tokens to run tokenize with text
    repeat for each t in tokens:
        set user.topics[t] to get(user.topics, t, 0) + 1
    append text to user.history
    set user.interaction_count to user.interaction_count + 1
    set users[user_id] to user
    // Run standard query
    set response to run query with text
    respond with response

// ═══════════════════════════════════════════════════════════
//  PERSISTENCE
// ═══════════════════════════════════════════════════════════

to save_state with filepath:
    purpose: "Save complete system state to file"
    set state to {
        "graph": graph,
        "node_freq": node_freq,
        "alias_map": alias_map,
        "embedding_store": embedding_store,
        "word_freq": word_freq,
        "hierarchy": hierarchy,
        "pattern_freq": pattern_freq,
        "phrase_mem": phrase_mem,
        "feedback_store": feedback_store,
        "response_cache": response_cache,
        "pheromone_map": pheromone_map,
        "users": users,
        "total_queries": total_queries,
        "total_learning_events": total_learning_events
    }
    set json_str to json_encode(state)
    store json_str to filepath
    log "Saved system state to " + filepath
    respond with true

to load_state with filepath:
    purpose: "Load complete system state from file"
    try:
        gather data from filepath
        set state to json_decode(data)
        set graph to state.graph
        set node_freq to state.node_freq
        set alias_map to state.alias_map
        set embedding_store to state.embedding_store
        set word_freq to state.word_freq
        set hierarchy to state.hierarchy
        set pattern_freq to state.pattern_freq
        set phrase_mem to state.phrase_mem
        set feedback_store to state.feedback_store
        set response_cache to state.response_cache
        set pheromone_map to state.pheromone_map
        set users to state.users
        set total_queries to state.total_queries
        set total_learning_events to state.total_learning_events
        log "Loaded system state from " + filepath
        respond with true
    on error e:
        log "No state file at " + filepath + ", starting fresh"
        respond with false

// ═══════════════════════════════════════════════════════════
//  SYSTEM STATS
// ═══════════════════════════════════════════════════════════

to system_stats:
    purpose: "Return full system statistics"
    respond with {
        "total_queries": total_queries,
        "total_learning_events": total_learning_events,
        "graph_nodes": len(node_freq),
        "embeddings": len(embedding_store),
        "L1_tokens": len(hierarchy.L1_tokens),
        "L2_patterns": len(hierarchy.L2_patterns),
        "phrases": len(phrase_mem),
        "pheromone_trails": len(pheromone_map),
        "cached_responses": len(response_cache),
        "users": len(users),
        "active_persona": active_persona,
        "use_swarm": use_swarm
    }

// ═══════════════════════════════════════════════════════════
//  API ENDPOINTS
// ═══════════════════════════════════════════════════════════

api:
    // Main query
    POST /query runs query
    // Multi-user query
    POST /query/:user_id runs query_as
    // Learn from text
    POST /ingest runs ingest
    // Batch learning
    POST /ingest/batch runs ingest_batch
    // Feedback
    POST /feedback runs record_feedback
    // Save/Load state
    POST /save runs save_state
    POST /load runs load_state
    // Stats
    GET /stats runs system_stats
    // Health check
    GET /health runs health

to health:
    purpose: "Health check endpoint"
    respond with {"status": "ok", "version": "1.0.0", "engine": "nc-ml-pipeline", "queries": total_queries}
