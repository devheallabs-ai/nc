// ═══════════════════════════════════════════════════════════
//  NC ML — Multi-Agent Swarm System
//
//  Multiple reasoning agents try different approaches in
//  parallel, and the swarm selects the best result.
//
//  Inspired by:
//    - Ant Colony Optimization (ACO)
//    - Particle Swarm Optimization (PSO)
//    - Ensemble methods
//
//  Features:
//    - Agent creation with different strategies
//    - Parallel candidate generation
//    - Swarm scoring + selection
//    - Pheromone trails (ACO-style reinforcement)
//    - Agent specialization
//    - Consensus voting
//    - Swarm memory (shared knowledge)
//
//  Usage:
//    nc nc-ai/nc/ml/swarm.nc
// ═══════════════════════════════════════════════════════════

service "nc-ml-swarm"
version "1.0.0"

configure:
    num_agents is 5
    pheromone_decay is 0.9
    pheromone_deposit is 0.2
    exploration_rate is 0.3

// ── Swarm State ───────────────────────────────────────────

set agents to []
set pheromone_map to {}
set swarm_memory to {}
set swarm_history to []

// ── Agent Strategies ──────────────────────────────────────

set strategies to [
    "greedy",
    "explorative",
    "conservative",
    "deep",
    "random"
]

// ── Create Agent ──────────────────────────────────────────

to create_agent with id, strategy:
    purpose: "Create a reasoning agent with a specific strategy"
    set agent to {
        "id": id,
        "strategy": strategy,
        "score": 0.0,
        "path": [],
        "success_count": 0,
        "fail_count": 0
    }
    append agent to agents
    log "Created agent " + string(id) + " with strategy: " + strategy
    respond with agent

// ── Initialize Swarm ──────────────────────────────────────

to init_swarm:
    purpose: "Initialize all agents with different strategies"
    set agents to []
    set i to 0
    repeat while i is below num_agents:
        set strategy to strategies[i % len(strategies)]
        run create_agent with i, strategy
        set i to i + 1
    log "Swarm initialized with " + string(num_agents) + " agents"
    respond with agents

// ── Agent: Greedy Path ────────────────────────────────────

to greedy_path with start, graph, depth:
    purpose: "Greedy agent: always pick highest-weight edge"
    set path to [start]
    set current to start
    set d to 0
    repeat while d is below depth:
        set edges to get(graph, current)
        if edges is equal nil:
            set d to depth
        otherwise:
            if len(edges) is equal 0:
                set d to depth
            otherwise:
                // Pick highest weight
                set best_edge to edges[0]
                repeat for each edge in edges:
                    if edge.weight is above best_edge.weight:
                        set best_edge to edge
                // Check pheromone
                set pkey to current + "→" + best_edge.target
                set phero to get(pheromone_map, pkey, 0.0)
                set best_edge_score to best_edge.weight + phero
                append best_edge.target to path
                set current to best_edge.target
        set d to d + 1
    respond with path

// ── Agent: Explorative Path ───────────────────────────────

to explorative_path with start, graph, depth:
    purpose: "Explorative agent: prefer less-visited paths"
    set path to [start]
    set current to start
    set visited to {}
    set visited[start] to true
    set d to 0
    repeat while d is below depth:
        set edges to get(graph, current)
        if edges is equal nil:
            set d to depth
        otherwise:
            // Find unvisited neighbors
            set unvisited to []
            repeat for each edge in edges:
                if get(visited, edge.target) is equal nil:
                    append edge to unvisited
            if len(unvisited) is above 0:
                // Pick least-pheromone path (exploration)
                set best to unvisited[0]
                set min_phero to 999.0
                repeat for each edge in unvisited:
                    set pkey to current + "→" + edge.target
                    set phero to get(pheromone_map, pkey, 0.0)
                    if phero is below min_phero:
                        set min_phero to phero
                        set best to edge
                append best.target to path
                set visited[best.target] to true
                set current to best.target
            otherwise:
                set d to depth
        set d to d + 1
    respond with path

// ── Agent: Conservative Path ──────────────────────────────

to conservative_path with start, graph, depth:
    purpose: "Conservative agent: short, high-confidence paths"
    set path to [start]
    set current to start
    set max_d to 2
    if depth is below max_d:
        set max_d to depth
    set d to 0
    repeat while d is below max_d:
        set edges to get(graph, current)
        if edges is equal nil:
            set d to max_d
        otherwise:
            if len(edges) is above 0:
                // Pick highest-weight edge
                set best to edges[0]
                repeat for each edge in edges:
                    if edge.weight is above best.weight:
                        set best to edge
                if best.weight is above 0.5:
                    append best.target to path
                    set current to best.target
                otherwise:
                    set d to max_d
            otherwise:
                set d to max_d
        set d to d + 1
    respond with path

// ── Agent: Deep Path ──────────────────────────────────────

to deep_path with start, graph, depth:
    purpose: "Deep agent: explore longest possible path"
    set path to [start]
    set current to start
    set visited to {}
    set visited[start] to true
    set d to 0
    set max_d to depth + 2
    repeat while d is below max_d:
        set edges to get(graph, current)
        if edges is equal nil:
            set d to max_d
        otherwise:
            set candidates to []
            repeat for each edge in edges:
                if get(visited, edge.target) is equal nil:
                    append edge to candidates
            if len(candidates) is above 0:
                // Pick first available
                set chosen to candidates[0]
                append chosen.target to path
                set visited[chosen.target] to true
                set current to chosen.target
            otherwise:
                set d to max_d
        set d to d + 1
    respond with path

// ── Agent: Random Path ────────────────────────────────────

to random_path with start, graph, depth:
    purpose: "Random agent: random walk through graph"
    set path to [start]
    set current to start
    set d to 0
    repeat while d is below depth:
        set edges to get(graph, current)
        if edges is equal nil:
            set d to depth
        otherwise:
            if len(edges) is above 0:
                // Pick based on position (pseudo-random)
                set pick to d % len(edges)
                set chosen to edges[pick]
                append chosen.target to path
                set current to chosen.target
            otherwise:
                set d to depth
        set d to d + 1
    respond with path

// ── Run Single Agent ──────────────────────────────────────

to run_agent with agent, start, graph, depth:
    purpose: "Run a single agent with its strategy"
    set strategy to agent.strategy
    if strategy is equal "greedy":
        set path to run greedy_path with start, graph, depth
    if strategy is equal "explorative":
        set path to run explorative_path with start, graph, depth
    if strategy is equal "conservative":
        set path to run conservative_path with start, graph, depth
    if strategy is equal "deep":
        set path to run deep_path with start, graph, depth
    if strategy is equal "random":
        set path to run random_path with start, graph, depth
    respond with path

// ── Score a Path ──────────────────────────────────────────

to score_path with path, graph:
    purpose: "Score a path based on edge weights + pheromones"
    set score to 0.0
    set i to 0
    repeat while i is below len(path) - 1:
        set a to path[i]
        set b to path[i + 1]
        // Edge weight
        set edges to get(graph, a)
        if edges is not equal nil:
            repeat for each edge in edges:
                if edge.target is equal b:
                    set score to score + edge.weight
        // Pheromone bonus
        set pkey to a + "→" + b
        set phero to get(pheromone_map, pkey, 0.0)
        set score to score + phero * 0.5
        set i to i + 1
    // Length penalty
    set score to score - 0.1 * len(path)
    respond with score

// ── Run Swarm ─────────────────────────────────────────────

to run_swarm with start, graph, depth:
    purpose: "Run all agents and select best path"
    if len(agents) is equal 0:
        run init_swarm
    set candidates to []
    // Run each agent
    repeat for each agent in agents:
        set path to run run_agent with agent, start, graph, depth
        set s to run score_path with path, graph
        append {"agent_id": agent.id, "strategy": agent.strategy, "path": path, "score": s} to candidates
    // Select best
    set best to candidates[0]
    repeat for each c in candidates:
        if c.score is above best.score:
            set best to c
    // Deposit pheromones on winning path
    run deposit_pheromone with best.path
    // Record in history
    append {"start": start, "winner": best.strategy, "score": best.score, "path_length": len(best.path)} to swarm_history
    log "Swarm winner: agent " + string(best.agent_id) + " (" + best.strategy + ") score=" + string(best.score)
    respond with best

// ── Deposit Pheromone ─────────────────────────────────────

to deposit_pheromone with path:
    purpose: "Deposit pheromone along a winning path (ACO)"
    set i to 0
    repeat while i is below len(path) - 1:
        set key to path[i] + "→" + path[i + 1]
        set current to get(pheromone_map, key, 0.0)
        set pheromone_map[key] to current + pheromone_deposit
        set i to i + 1
    respond with true

// ── Evaporate Pheromones ──────────────────────────────────

to evaporate:
    purpose: "Apply pheromone decay (prevents over-specialization)"
    repeat for each entry in pheromone_map:
        set pheromone_map[entry.key] to entry.value * pheromone_decay
    respond with true

// ── Consensus Vote ────────────────────────────────────────

to consensus with start, graph, depth:
    purpose: "Run swarm multiple times and vote on best strategy"
    set votes to {}
    set rounds to 3
    set r to 0
    repeat while r is below rounds:
        set result to run run_swarm with start, graph, depth
        set winner to result.strategy
        set votes[winner] to get(votes, winner, 0) + 1
        run evaporate
        set r to r + 1
    // Find most-voted strategy
    set best_strategy to "greedy"
    set best_votes to 0
    repeat for each entry in votes:
        if entry.value is above best_votes:
            set best_votes to entry.value
            set best_strategy to entry.key
    log "Consensus strategy: " + best_strategy + " (" + string(best_votes) + " votes)"
    respond with best_strategy

// ── Agent Performance Stats ───────────────────────────────

to agent_stats:
    purpose: "Return performance statistics for each agent"
    set stats to []
    repeat for each agent in agents:
        append {"id": agent.id, "strategy": agent.strategy, "successes": agent.success_count, "failures": agent.fail_count} to stats
    respond with stats

// ── Swarm Memory (Shared Knowledge) ──────────────────────

to store_swarm_knowledge with key, value:
    purpose: "Store shared knowledge accessible to all agents"
    set swarm_memory[key] to value
    respond with true

to get_swarm_knowledge with key:
    purpose: "Retrieve shared swarm knowledge"
    respond with get(swarm_memory, key)

// ── Swarm Stats ───────────────────────────────────────────

to swarm_stats:
    purpose: "Return swarm system statistics"
    set strategy_wins to {}
    repeat for each h in swarm_history:
        set strategy_wins[h.winner] to get(strategy_wins, h.winner, 0) + 1
    respond with {
        "agents": len(agents),
        "total_runs": len(swarm_history),
        "pheromone_trails": len(pheromone_map),
        "strategy_wins": strategy_wins,
        "shared_knowledge": len(swarm_memory)
    }
