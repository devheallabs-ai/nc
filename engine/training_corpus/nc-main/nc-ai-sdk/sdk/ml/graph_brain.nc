// ═══════════════════════════════════════════════════════════
//  NC ML — Graph Brain (Weighted Knowledge Graph)
//
//  Stores knowledge as a weighted directed graph with typed
//  relations. Each edge: (subject, object, weight, relation).
//
//  Features:
//    - Typed edges (subject → relation → object)
//    - Edge weight updates + decay (forgetting)
//    - Alias system (canonicalization)
//    - Relation metadata storage
//    - Graph pruning for scaling
//    - Triplet ingestion from text
//    - Adjacency list + node frequency tracking
//
//  Usage:
//    nc nc-ai/nc/ml/graph_brain.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-graph-brain"
version "1.0.0"

configure:
    decay_rate is 0.995
    prune_threshold is 0.01
    max_nodes is 100000

// ── Graph Storage ─────────────────────────────────────────

set graph to {}
set node_freq to {}
set relation_meta to {}
set alias_map to {}
set edge_count to 0

// ── Normalize Token ───────────────────────────────────────

to normalize with token:
    purpose: "Normalize a token: lowercase + alias resolution"
    set t to lower(trim(token))
    // Check alias map
    set resolved to get(alias_map, t)
    if resolved is not equal nil:
        respond with resolved
    respond with t

// ── Add Alias ─────────────────────────────────────────────

to add_alias with word, canonical:
    purpose: "Register a word as alias for a canonical form"
    set alias_map[lower(word)] to lower(canonical)
    log "Alias: " + word + " → " + canonical
    respond with true

// ── Add Edge ──────────────────────────────────────────────

to add_edge with subject, object, weight, relation:
    purpose: "Add a weighted typed edge to the knowledge graph"
    set s to run normalize with subject
    set o to run normalize with object
    set r to lower(trim(relation))
    // Initialize adjacency list for subject
    if get(graph, s) is equal nil:
        set graph[s] to []
    // Check if edge already exists — update weight
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
        set edge_count to edge_count + 1
    // Update node frequency
    set node_freq[s] to get(node_freq, s, 0) + 1
    set node_freq[o] to get(node_freq, o, 0) + 1
    // Store relation metadata
    set meta_key to s + "|" + r + "|" + o
    set relation_meta[meta_key] to {"subject": s, "relation": r, "object": o, "weight": weight}
    respond with true

// ── Get Neighbors ─────────────────────────────────────────

to get_neighbors with node:
    purpose: "Get all outgoing edges from a node"
    set n to run normalize with node
    set edges to get(graph, n)
    if edges is equal nil:
        respond with []
    respond with edges

// ── Get Neighbors By Relation ─────────────────────────────

to get_by_relation with node, relation:
    purpose: "Get neighbors filtered by relation type"
    set n to run normalize with node
    set r to lower(trim(relation))
    set edges to get(graph, n)
    if edges is equal nil:
        respond with []
    set filtered to []
    repeat for each edge in edges:
        if edge.relation is equal r:
            append edge to filtered
    respond with filtered

// ── Update Edge Weight ────────────────────────────────────

to update_weight with subject, object, delta:
    purpose: "Adjust weight of an existing edge"
    set s to run normalize with subject
    set o to run normalize with object
    set edges to get(graph, s)
    if edges is equal nil:
        respond with false
    set idx to 0
    repeat for each edge in edges:
        if edge.target is equal o:
            set graph[s][idx].weight to edge.weight + delta
            respond with true
        set idx to idx + 1
    respond with false

// ── Reinforce Edge ────────────────────────────────────────

to reinforce_edge with subject, object, positive:
    purpose: "Reinforce or weaken an edge based on feedback"
    set delta to 0.1
    if positive is equal false:
        set delta to -0.1
    run update_weight with subject, object, delta
    respond with true

// ── Decay All Weights ─────────────────────────────────────

to decay_all:
    purpose: "Apply time-based decay to all edge weights (forgetting)"
    set decayed to 0
    repeat for each entry in graph:
        set idx to 0
        repeat for each edge in entry.value:
            set graph[entry.key][idx].weight to edge.weight * decay_rate
            set decayed to decayed + 1
            set idx to idx + 1
    log "Decayed " + string(decayed) + " edges"
    respond with decayed

// ── Prune Weak Edges ──────────────────────────────────────

to prune:
    purpose: "Remove edges below weight threshold"
    set pruned to 0
    repeat for each entry in graph:
        set kept to []
        repeat for each edge in entry.value:
            if edge.weight is above prune_threshold:
                append edge to kept
            otherwise:
                set pruned to pruned + 1
        set graph[entry.key] to kept
        set edge_count to edge_count - pruned
    log "Pruned " + string(pruned) + " weak edges"
    respond with pruned

// ── Ingest Triplet ────────────────────────────────────────

to ingest_triplet with subject, relation, object:
    purpose: "Ingest a single knowledge triplet into the graph"
    run add_edge with subject, object, 1.0, relation
    respond with true

// ── Ingest Triplets Batch ─────────────────────────────────

to ingest_batch with triplets:
    purpose: "Ingest a list of triplets [{s, r, o}, ...]"
    set count to 0
    repeat for each t in triplets:
        run ingest_triplet with t.subject, t.relation, t.object
        set count to count + 1
    log "Ingested " + string(count) + " triplets"
    respond with count

// ── Extract Triplets From Text ────────────────────────────

to extract_triplets with tokens:
    purpose: "Extract simple SRO triplets from token sequence"
    set triplets to []
    set relation_words to ["is", "has", "gives", "causes", "contains", "makes", "leads", "produces", "creates", "uses", "needs", "eats", "drinks", "runs", "does"]
    set i to 0
    repeat while i is below len(tokens) - 2:
        set word to lower(tokens[i + 1])
        set is_relation to false
        repeat for each rw in relation_words:
            if word is equal rw:
                set is_relation to true
        if is_relation is equal true:
            set triplet to {"subject": tokens[i], "relation": word, "object": tokens[i + 2]}
            append triplet to triplets
        set i to i + 1
    respond with triplets

// ── Get All Nodes ─────────────────────────────────────────

to get_all_nodes:
    purpose: "Return list of all nodes in the graph"
    set nodes to []
    repeat for each entry in graph:
        append entry.key to nodes
    respond with nodes

// ── Get Node Degree ───────────────────────────────────────

to node_degree with node:
    purpose: "Return number of outgoing edges"
    set n to run normalize with node
    set edges to get(graph, n)
    if edges is equal nil:
        respond with 0
    respond with len(edges)

// ── Check Edge Exists ─────────────────────────────────────

to edge_exists with subject, object:
    purpose: "Check if a direct edge exists between two nodes"
    set s to run normalize with subject
    set o to run normalize with object
    set edges to get(graph, s)
    if edges is equal nil:
        respond with false
    repeat for each edge in edges:
        if edge.target is equal o:
            respond with true
    respond with false

// ── Get Top Nodes ─────────────────────────────────────────

to top_nodes with k:
    purpose: "Get top-k most connected nodes"
    set scored to []
    repeat for each entry in node_freq:
        append {"node": entry.key, "freq": entry.value} to scored
    // Selection sort for top-k
    set results to []
    set found to 0
    repeat while found is below k:
        set best to -1
        set best_idx to -1
        set idx to 0
        repeat for each s in scored:
            if s.freq is above best:
                set best to s.freq
                set best_idx to idx
            set idx to idx + 1
        if best_idx is above -1:
            append scored[best_idx] to results
            set scored[best_idx].freq to -2
            set found to found + 1
        otherwise:
            set found to k
    respond with results

// ── Export / Import Graph ─────────────────────────────────

to export_graph with filepath:
    purpose: "Save graph to file"
    set data to {"graph": graph, "node_freq": node_freq, "aliases": alias_map, "meta": relation_meta}
    set json_str to json_encode(data)
    store json_str to filepath
    log "Exported graph with " + string(edge_count) + " edges"
    respond with true

to import_graph with filepath:
    purpose: "Load graph from file"
    try:
        gather data from filepath
        set parsed to json_decode(data)
        set graph to parsed.graph
        set node_freq to parsed.node_freq
        set alias_map to parsed.aliases
        set relation_meta to parsed.meta
        log "Imported graph from " + filepath
        respond with true
    on error e:
        log "Failed to import graph: " + string(e)
        respond with false

// ── Graph Stats ───────────────────────────────────────────

to graph_stats:
    purpose: "Return graph statistics"
    set total_nodes to len(node_freq)
    set total_aliases to len(alias_map)
    respond with {"nodes": total_nodes, "edges": edge_count, "aliases": total_aliases}
