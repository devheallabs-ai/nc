// ═══════════════════════════════════════════════════════════
//  NC ML — Noise Filtering & Memory Management
//
//  Information-theoretic filtering using entropy, frequency
//  analysis, and stability checks. Prevents graph/memory
//  explosion at scale.
//
//  Math:
//    H(x) = -sum(p(x) * log(p(x)))   (Shannon entropy)
//    Score = frequency * predictability
//    Decay: weight *= 0.99^t           (exponential forgetting)
//
//  Features:
//    - Frequency-based token filtering
//    - Entropy scoring (noise detection)
//    - Pattern importance scoring (freq * length)
//    - Vector stability filtering
//    - Exponential decay (forgetting mechanism)
//    - Memory budget enforcement
//    - Deduplication
//
//  Usage:
//    nc nc-ai/nc/ml/noise_filter.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-noise-filter"
version "1.0.0"

configure:
    min_token_freq is 3
    max_entropy is 3.5
    decay_factor is 0.99
    memory_budget is 50000
    stability_max_norm is 10.0

// ── Token Frequency Store ─────────────────────────────────

set token_counts to {}
set total_tokens to 0

// ── Count Tokens ──────────────────────────────────────────

to count_tokens with tokens:
    purpose: "Update frequency counts for a token list"
    repeat for each t in tokens:
        set token_counts[t] to get(token_counts, t, 0) + 1
        set total_tokens to total_tokens + 1
    respond with total_tokens

// ── Frequency Filter ──────────────────────────────────────

to filter_by_frequency with tokens, min_freq:
    purpose: "Keep only tokens that appear at least min_freq times"
    set filtered to []
    repeat for each t in tokens:
        set freq to get(token_counts, t, 0)
        if freq is above min_freq - 1:
            append t to filtered
    respond with filtered

// ── Token Probability ─────────────────────────────────────

to token_probability with token:
    purpose: "Calculate probability of a token"
    set freq to get(token_counts, token, 0)
    if total_tokens is equal 0:
        respond with 0.0
    set prob to freq / total_tokens
    respond with prob

// ── Shannon Entropy ───────────────────────────────────────

to entropy with tokens:
    purpose: "Calculate Shannon entropy for a token set"
    // Count frequencies in this set
    set local_counts to {}
    set local_total to 0
    repeat for each t in tokens:
        set local_counts[t] to get(local_counts, t, 0) + 1
        set local_total to local_total + 1
    if local_total is equal 0:
        respond with 0.0
    set h to 0.0
    repeat for each entry in local_counts:
        set p to entry.value / local_total
        if p is above 0.0:
            set h to h - (p * log(p))
    respond with h

// ── Entropy-Based Noise Detection ─────────────────────────

to is_noisy with tokens, threshold:
    purpose: "Detect if a token set is noisy (high entropy)"
    set h to run entropy with tokens
    if h is above threshold:
        respond with true
    respond with false

// ── Pattern Importance Score ──────────────────────────────

to pattern_score with pattern, freq:
    purpose: "Score a pattern: frequency * length (MDL-inspired)"
    set parts to split(pattern, " ")
    set length_score to len(parts)
    set importance to freq * length_score
    respond with importance

// ── Filter Patterns By Importance ─────────────────────────

to filter_patterns with pattern_freq, min_score:
    purpose: "Keep only patterns above importance threshold"
    set kept to []
    repeat for each entry in pattern_freq:
        set score to run pattern_score with entry.key, entry.value
        if score is above min_score:
            append {"pattern": entry.key, "freq": entry.value, "score": score} to kept
    respond with kept

// ── Vector Stability Check ────────────────────────────────

to check_stability with embedding_store:
    purpose: "Filter out unstable (noisy) embeddings"
    set unstable to []
    set stable to 0
    repeat for each entry in embedding_store:
        set norm to tensor_norm(entry.value)
        if norm is above stability_max_norm:
            append entry.key to unstable
        otherwise:
            set stable to stable + 1
    log "Stable: " + string(stable) + ", Unstable: " + string(len(unstable))
    respond with {"stable_count": stable, "unstable": unstable}

// ── Remove Unstable Embeddings ────────────────────────────

to remove_unstable with embedding_store:
    purpose: "Remove embeddings with excessive norm"
    set result to run check_stability with embedding_store
    set removed to 0
    repeat for each word in result.unstable:
        set embedding_store[word] to nil
        set removed to removed + 1
    log "Removed " + string(removed) + " unstable embeddings"
    respond with removed

// ── Exponential Decay ─────────────────────────────────────

to apply_decay with memory_usage:
    purpose: "Apply exponential decay to all memory usage scores"
    set decayed to 0
    repeat for each entry in memory_usage:
        set memory_usage[entry.key] to entry.value * decay_factor
        set decayed to decayed + 1
    respond with decayed

// ── Memory Budget Enforcement ─────────────────────────────

to enforce_budget with store, budget:
    purpose: "Prune store to stay within memory budget"
    set current_size to len(store)
    if current_size is below budget + 1:
        respond with 0
    // Remove lowest-frequency items
    set to_remove to current_size - budget
    set removed to 0
    // Find and remove lowest items
    repeat while removed is below to_remove:
        set min_val to 999999
        set min_key to nil
        repeat for each entry in store:
            if entry.value is not equal nil:
                if entry.value is below min_val:
                    set min_val to entry.value
                    set min_key to entry.key
        if min_key is not equal nil:
            set store[min_key] to nil
            set removed to removed + 1
        otherwise:
            set removed to to_remove
    log "Pruned " + string(removed) + " items to enforce budget"
    respond with removed

// ── Deduplication ─────────────────────────────────────────

to deduplicate with items:
    purpose: "Remove duplicate items from a list"
    set seen to {}
    set unique to []
    repeat for each item in items:
        set key to string(item)
        if get(seen, key) is equal nil:
            set seen[key] to true
            append item to unique
    respond with unique

// ── Stop Word Filter ──────────────────────────────────────

to remove_stop_words with tokens:
    purpose: "Remove common stop words"
    set stop_words to ["the", "a", "an", "is", "are", "was", "were", "be", "been", "being", "have", "has", "had", "do", "does", "did", "will", "would", "could", "should", "may", "might", "shall", "can", "to", "of", "in", "for", "on", "with", "at", "by", "from", "as", "into", "about", "it", "its", "this", "that", "these", "those", "i", "you", "he", "she", "we", "they", "me", "him", "her", "us", "them"]
    set filtered to []
    repeat for each t in tokens:
        set is_stop to false
        repeat for each sw in stop_words:
            if lower(t) is equal sw:
                set is_stop to true
        if is_stop is equal false:
            append t to filtered
    respond with filtered

// ── Quality Score ─────────────────────────────────────────

to quality_score with tokens:
    purpose: "Overall quality score for a token set"
    set h to run entropy with tokens
    set avg_freq to 0.0
    set count to 0
    repeat for each t in tokens:
        set f to get(token_counts, t, 0)
        set avg_freq to avg_freq + f
        set count to count + 1
    if count is above 0:
        set avg_freq to avg_freq / count
    // Quality = low entropy + high avg frequency
    set quality to avg_freq / (1.0 + h)
    respond with quality

// ── Full Cleaning Pipeline ────────────────────────────────

to clean_tokens with tokens:
    purpose: "Full noise removal pipeline"
    // Step 1: Remove stop words
    set cleaned to run remove_stop_words with tokens
    // Step 2: Frequency filter
    set cleaned to run filter_by_frequency with cleaned, min_token_freq
    // Step 3: Deduplicate
    set cleaned to run deduplicate with cleaned
    log "Cleaned: " + string(len(tokens)) + " → " + string(len(cleaned)) + " tokens"
    respond with cleaned

// ── Filter Stats ──────────────────────────────────────────

to filter_stats:
    purpose: "Return noise filter statistics"
    respond with {
        "total_tokens": total_tokens,
        "unique_tokens": len(token_counts),
        "memory_budget": memory_budget
    }
