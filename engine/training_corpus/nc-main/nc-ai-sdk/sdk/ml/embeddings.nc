// ═══════════════════════════════════════════════════════════
//  NC ML — Embeddings & Semantic Bridge
//
//  Hebbian learning embeddings (CPU-native, no backprop).
//  Builds vector representations through co-occurrence +
//  reinforcement: "neurons that fire together wire together."
//
//  Features:
//    - Hebbian co-occurrence learning
//    - Cosine similarity search
//    - Semantic bridge (fuzzy node lookup)
//    - Cached embedding store
//    - Vector normalization + stability filtering
//
//  Usage:
//    nc nc-ai/nc/ml/embeddings.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-embeddings"
version "1.0.0"

configure:
    embed_dim is 64
    learning_rate is 0.05
    window_size is 3
    stability_threshold is 10.0

// ── Embedding Store ───────────────────────────────────────

set embedding_store to {}
set word_freq to {}
set co_occurrence to {}

// ── Initialize Embedding ──────────────────────────────────

to init_embedding with word:
    purpose: "Create a random embedding vector for a new word"
    if get(embedding_store, word) is equal nil:
        set vec to tensor_random(1, embed_dim)
        set vec to tensor_scale(vec, 0.1)
        set embedding_store[word] to vec
        set word_freq[word] to 0
    respond with embedding_store[word]

// ── Get Embedding Vector ──────────────────────────────────

to vec with word:
    purpose: "Retrieve or create embedding for a word"
    if get(embedding_store, word) is equal nil:
        run init_embedding with word
    set word_freq[word] to get(word_freq, word, 0) + 1
    respond with embedding_store[word]

// ── Cosine Similarity ─────────────────────────────────────

to cosine_sim with vec_a, vec_b:
    purpose: "Compute cosine similarity between two vectors"
    set dot_product to tensor_dot(vec_a, vec_b)
    set norm_a to tensor_norm(vec_a)
    set norm_b to tensor_norm(vec_b)
    set denominator to norm_a * norm_b
    if denominator is below 0.0001:
        respond with 0.0
    set similarity to dot_product / denominator
    respond with similarity

// ── Hebbian Update ────────────────────────────────────────

to hebbian_update with word_a, word_b:
    purpose: "Strengthen connection between co-occurring words"
    set va to run vec with word_a
    set vb to run vec with word_b
    // Hebbian rule: delta = lr * va * vb
    set delta to tensor_mul(va, vb)
    set delta to tensor_scale(delta, learning_rate)
    // Update both vectors toward each other
    set new_va to tensor_add(va, tensor_scale(delta, 0.5))
    set new_vb to tensor_add(vb, tensor_scale(delta, 0.5))
    set embedding_store[word_a] to new_va
    set embedding_store[word_b] to new_vb
    // Track co-occurrence
    set pair_key to word_a + "|" + word_b
    set co_occurrence[pair_key] to get(co_occurrence, pair_key, 0) + 1
    respond with true

// ── Learn From Token Sequence ─────────────────────────────

to learn_embeddings with tokens:
    purpose: "Learn embeddings from a token sequence using Hebbian co-occurrence"
    set i to 0
    set learned to 0
    repeat while i is below len(tokens):
        set j to 1
        repeat while j is below window_size + 1:
            if i + j is below len(tokens):
                run hebbian_update with tokens[i], tokens[i + j]
                set learned to learned + 1
            set j to j + 1
        set i to i + 1
    log "Learned " + string(learned) + " co-occurrence pairs"
    respond with learned

// ── Context Vector ────────────────────────────────────────

to context_vec with tokens:
    purpose: "Compute average context vector for a token list"
    if len(tokens) is equal 0:
        respond with tensor_create(1, embed_dim)
    set result to tensor_create(1, embed_dim)
    set count to 0
    repeat for each token in tokens:
        set v to run vec with token
        set result to tensor_add(result, v)
        set count to count + 1
    if count is above 0:
        set result to tensor_scale(result, 1.0 / count)
    respond with result

// ── Weighted Context Vector ───────────────────────────────

to weighted_context_vec with tokens:
    purpose: "Compute frequency-weighted context vector"
    if len(tokens) is equal 0:
        respond with tensor_create(1, embed_dim)
    set result to tensor_create(1, embed_dim)
    set total_weight to 0.0
    repeat for each token in tokens:
        set v to run vec with token
        set freq to get(word_freq, token, 1)
        set weight to 1.0 / (1.0 + log(freq))
        set result to tensor_add(result, tensor_scale(v, weight))
        set total_weight to total_weight + weight
    if total_weight is above 0.0:
        set result to tensor_scale(result, 1.0 / total_weight)
    respond with result

// ── Find Similar Words ────────────────────────────────────

to find_similar with word, top_k:
    purpose: "Find top-k most similar words by cosine similarity"
    set target_vec to run vec with word
    set scores to []
    repeat for each entry in embedding_store:
        if entry.key is not equal word:
            set sim to run cosine_sim with target_vec, entry.value
            append {"word": entry.key, "score": sim} to scores
    // Sort by score descending (selection sort for top_k)
    set results to []
    set found to 0
    repeat while found is below top_k:
        set best_score to -1.0
        set best_idx to -1
        set idx to 0
        repeat for each s in scores:
            if s.score is above best_score:
                set best_score to s.score
                set best_idx to idx
            set idx to idx + 1
        if best_idx is above -1:
            append scores[best_idx] to results
            set scores[best_idx].score to -2.0
            set found to found + 1
        otherwise:
            set found to top_k
    respond with results

// ── Semantic Bridge ───────────────────────────────────────

to find_closest_node with token, graph_nodes, threshold:
    purpose: "Find closest graph node for a token using embeddings"
    set target_vec to run vec with token
    set best_node to token
    set best_sim to 0.0
    repeat for each node in graph_nodes:
        set node_vec to run vec with node
        set sim to run cosine_sim with target_vec, node_vec
        if sim is above best_sim:
            set best_sim to sim
            set best_node to node
    if best_sim is below threshold:
        respond with token
    respond with best_node

// ── Vector Stability Check ────────────────────────────────

to is_stable with word:
    purpose: "Check if embedding is stable (not noisy)"
    set v to run vec with word
    set norm to tensor_norm(v)
    if norm is above stability_threshold:
        respond with false
    respond with true

// ── Normalize All Embeddings ──────────────────────────────

to normalize_all:
    purpose: "Normalize all embeddings to unit length"
    set normalized to 0
    repeat for each entry in embedding_store:
        set norm to tensor_norm(entry.value)
        if norm is above 0.0001:
            set embedding_store[entry.key] to tensor_scale(entry.value, 1.0 / norm)
            set normalized to normalized + 1
    log "Normalized " + string(normalized) + " embeddings"
    respond with normalized

// ── Nonlinear Composition ─────────────────────────────────

to nonlinear_compose with vec_a, vec_b:
    purpose: "Compose two vectors nonlinearly: a + b + a*b"
    set sum_vec to tensor_add(vec_a, vec_b)
    set product_vec to tensor_mul(vec_a, vec_b)
    set result to tensor_add(sum_vec, product_vec)
    respond with result

// ── Export / Import Embeddings ────────────────────────────

to export_embeddings with filepath:
    purpose: "Save embeddings to file"
    set data to {"embeddings": embedding_store, "freq": word_freq, "co_occur": co_occurrence}
    set json_str to json_encode(data)
    store json_str to filepath
    log "Exported " + string(len(embedding_store)) + " embeddings to " + filepath
    respond with true

to import_embeddings with filepath:
    purpose: "Load embeddings from file"
    try:
        gather data from filepath
        set parsed to json_decode(data)
        set embedding_store to parsed.embeddings
        set word_freq to parsed.freq
        set co_occurrence to parsed.co_occur
        log "Imported embeddings from " + filepath
        respond with true
    on error e:
        log "Failed to import embeddings: " + string(e)
        respond with false

// ── Stats ─────────────────────────────────────────────────

to embedding_stats:
    purpose: "Return embedding store statistics"
    set total_words to len(embedding_store)
    set total_pairs to len(co_occurrence)
    respond with {"total_words": total_words, "total_pairs": total_pairs, "embed_dim": embed_dim}
