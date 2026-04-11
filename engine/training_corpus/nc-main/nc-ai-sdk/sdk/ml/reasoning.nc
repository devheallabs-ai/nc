// ═══════════════════════════════════════════════════════════
//  NC ML — Multi-Hop Weighted Reasoning Engine
//
//  Graph-based reasoning with weighted path scoring,
//  constraint satisfaction, transitive closure, and
//  Bayesian confidence updates.
//
//  Math:
//    Score(path) = sum(edge_weight * relation_importance)
//                  - lambda * path_length
//    P(A|B) = P(B|A)*P(A) / P(B)  (Bayesian update)
//
//  Features:
//    - BFS/DFS hybrid multi-hop traversal
//    - Weighted path scoring with penalties
//    - Constraint filtering
//    - Relation-based prioritization
//    - Transitive closure (A→B, B→C ⇒ A→C)
//    - Energy minimization ranking
//    - Symbolic logic (modus ponens)
//
//  Usage:
//    nc nc-ai/nc/ml/reasoning.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-reasoning"
version "1.0.0"

configure:
    max_depth is 5
    length_penalty is 0.1
    min_path_score is 0.0
    beam_width is 10

// ── Constraints ───────────────────────────────────────────

set constraints to []

to add_constraint with name, min_len:
    purpose: "Add a constraint for path validation"
    append {"name": name, "min_len": min_len} to constraints
    respond with true

to valid_node with node:
    purpose: "Check if a node passes all constraints"
    if len(node) is below 2:
        respond with false
    set stop_words to ["the", "is", "a", "an", "of", "to", "in", "for", "and", "or", "it"]
    repeat for each sw in stop_words:
        if node is equal sw:
            respond with false
    respond with true

// ── Relation Importance ───────────────────────────────────

set relation_weights to {
    "causes": 1.5,
    "leads": 1.4,
    "produces": 1.3,
    "creates": 1.3,
    "contains": 1.2,
    "gives": 1.1,
    "has": 1.0,
    "is": 0.9,
    "uses": 1.0,
    "needs": 1.0,
    "makes": 1.2,
    "eats": 1.0,
    "drinks": 1.0
}

to relation_importance with relation:
    purpose: "Get importance weight for a relation type"
    set w to get(relation_weights, relation)
    if w is equal nil:
        respond with 1.0
    respond with w

// ── Generate Reasoning Paths ──────────────────────────────

to generate_paths with start, graph, depth:
    purpose: "Generate all paths from start node up to given depth"
    set paths to [[{"node": start, "weight": 1.0, "relation": "start"}]]
    set d to 0
    repeat while d is below depth:
        set new_paths to []
        repeat for each path in paths:
            set last to path[len(path) - 1]
            set edges to get(graph, last.node)
            if edges is not equal nil:
                repeat for each edge in edges:
                    if run valid_node with edge.target:
                        // Check not already in path (no cycles)
                        set in_path to false
                        repeat for each step in path:
                            if step.node is equal edge.target:
                                set in_path to true
                        if in_path is equal false:
                            set new_step to {"node": edge.target, "weight": edge.weight, "relation": edge.relation}
                            set new_path to []
                            repeat for each s in path:
                                append s to new_path
                            append new_step to new_path
                            append new_path to new_paths
            // Keep path even if no extension (for shorter results)
            if len(path) is above 1:
                append path to new_paths
        if len(new_paths) is above 0:
            set paths to new_paths
        set d to d + 1
    respond with paths

// ── Score a Path ──────────────────────────────────────────

to path_score with path:
    purpose: "Score a reasoning path: sum(weight * relation_importance) - penalty"
    set score to 0.0
    set i to 1
    repeat while i is below len(path):
        set step to path[i]
        set rel_w to run relation_importance with step.relation
        set score to score + (step.weight * rel_w)
        set i to i + 1
    // Length penalty
    set penalty to length_penalty * len(path)
    set final_score to score - penalty
    respond with final_score

// ── Find Best Path ────────────────────────────────────────

to best_path with start, graph, depth:
    purpose: "Find the highest-scoring reasoning path"
    set all_paths to run generate_paths with start, graph, depth
    if len(all_paths) is equal 0:
        respond with [{"node": start, "weight": 1.0, "relation": "start"}]
    set best to nil
    set best_score to -999.0
    repeat for each path in all_paths:
        if len(path) is above 1:
            set s to run path_score with path
            if s is above best_score:
                set best_score to s
                set best to path
    if best is equal nil:
        respond with [{"node": start, "weight": 1.0, "relation": "start"}]
    respond with best

// ── Extract Path Nodes ────────────────────────────────────

to path_nodes with path:
    purpose: "Extract just the node names from a path"
    set nodes to []
    repeat for each step in path:
        append step.node to nodes
    respond with nodes

// ── Beam Search Reasoning ─────────────────────────────────

to beam_reason with start, graph, depth, width:
    purpose: "Beam search: keep top-k paths at each depth"
    set beams to [[{"node": start, "weight": 1.0, "relation": "start"}]]
    set d to 0
    repeat while d is below depth:
        set candidates to []
        repeat for each beam in beams:
            set last to beam[len(beam) - 1]
            set edges to get(graph, last.node)
            if edges is not equal nil:
                repeat for each edge in edges:
                    if run valid_node with edge.target:
                        set new_beam to []
                        repeat for each s in beam:
                            append s to new_beam
                        append {"node": edge.target, "weight": edge.weight, "relation": edge.relation} to new_beam
                        append new_beam to candidates
            otherwise:
                append beam to candidates
        // Keep top-k by score
        if len(candidates) is above width:
            set scored to []
            repeat for each c in candidates:
                set s to run path_score with c
                append {"path": c, "score": s} to scored
            // Selection sort for top-k
            set beams to []
            set found to 0
            repeat while found is below width:
                set best_s to -999.0
                set best_idx to -1
                set idx to 0
                repeat for each sc in scored:
                    if sc.score is above best_s:
                        set best_s to sc.score
                        set best_idx to idx
                    set idx to idx + 1
                if best_idx is above -1:
                    append scored[best_idx].path to beams
                    set scored[best_idx].score to -9999.0
                    set found to found + 1
                otherwise:
                    set found to width
        otherwise:
            set beams to candidates
        set d to d + 1
    // Return best beam
    if len(beams) is equal 0:
        respond with [{"node": start, "weight": 1.0, "relation": "start"}]
    set best to beams[0]
    set best_s to run path_score with beams[0]
    repeat for each b in beams:
        set s to run path_score with b
        if s is above best_s:
            set best_s to s
            set best to b
    respond with best

// ── Transitive Closure ────────────────────────────────────

to transitive_closure with node, graph, depth:
    purpose: "Find all reachable nodes via transitive inference"
    set visited to {}
    set frontier to [node]
    set d to 0
    repeat while d is below depth:
        set new_frontier to []
        repeat for each n in frontier:
            set edges to get(graph, n)
            if edges is not equal nil:
                repeat for each edge in edges:
                    if get(visited, edge.target) is equal nil:
                        set visited[edge.target] to d + 1
                        append edge.target to new_frontier
        set frontier to new_frontier
        set d to d + 1
    respond with visited

// ── Weighted Multi-Hop Reasoning ──────────────────────────

to weighted_reason with start, graph:
    purpose: "Soft reasoning: propagate scores through graph"
    set scores to {}
    set scores[start] to 1.0
    set depth to 0
    repeat while depth is below 3:
        set new_scores to {}
        repeat for each entry in scores:
            set edges to get(graph, entry.key)
            if edges is not equal nil:
                repeat for each edge in edges:
                    set propagated to entry.value * edge.weight * 0.5
                    set current to get(new_scores, edge.target, 0.0)
                    set new_scores[edge.target] to current + propagated
        set scores to new_scores
        set depth to depth + 1
    // Find highest scored node
    set best_node to start
    set best_score to 0.0
    repeat for each entry in scores:
        if entry.value is above best_score:
            set best_score to entry.value
            set best_node to entry.key
    respond with {"node": best_node, "score": best_score, "all_scores": scores}

// ── Bayesian Confidence ───────────────────────────────────

to bayesian_update with prior, likelihood, evidence:
    purpose: "Bayesian confidence update: P(A|B) = P(B|A)*P(A)/P(B)"
    if evidence is below 0.0001:
        respond with prior
    set posterior to (likelihood * prior) / evidence
    // Clamp to [0, 1]
    if posterior is above 1.0:
        set posterior to 1.0
    if posterior is below 0.0:
        set posterior to 0.0
    respond with posterior

// ── Energy-Based Ranking ──────────────────────────────────

to energy_rank with paths:
    purpose: "Rank paths by energy minimization (lowest energy = best)"
    set ranked to []
    repeat for each path in paths:
        set score to run path_score with path
        set energy to 0.0 - score
        append {"path": path, "energy": energy, "score": score} to ranked
    // Sort by energy ascending (lowest first)
    set sorted to []
    set remaining to len(ranked)
    repeat while remaining is above 0:
        set min_e to 999999.0
        set min_idx to -1
        set idx to 0
        repeat for each r in ranked:
            if r.energy is below min_e:
                set min_e to r.energy
                set min_idx to idx
            set idx to idx + 1
        if min_idx is above -1:
            append ranked[min_idx] to sorted
            set ranked[min_idx].energy to 999999.0
            set remaining to remaining - 1
        otherwise:
            set remaining to 0
    respond with sorted

// ── Symbolic Modus Ponens ─────────────────────────────────

to modus_ponens with rules, fact:
    purpose: "Apply modus ponens: if (A→B) and A, then B"
    set inferred to []
    repeat for each rule in rules:
        if rule.antecedent is equal fact:
            append rule.consequent to inferred
    respond with inferred

// ── Chain Reasoning (multi-step) ──────────────────────────

to chain_reason with rules, start_fact, depth:
    purpose: "Chain multiple modus ponens steps"
    set current_facts to [start_fact]
    set all_inferred to []
    set d to 0
    repeat while d is below depth:
        set new_facts to []
        repeat for each fact in current_facts:
            set results to run modus_ponens with rules, fact
            repeat for each r in results:
                append r to new_facts
                append r to all_inferred
        set current_facts to new_facts
        set d to d + 1
    respond with all_inferred

// ── Combined Score (semantic + structural + logic) ────────

to combined_score with path, semantic_score, structure_match:
    purpose: "Combine semantic, structural, and path scores"
    set p_score to run path_score with path
    set final to 0.5 * semantic_score + 0.3 * structure_match + 0.2 * p_score
    respond with final
