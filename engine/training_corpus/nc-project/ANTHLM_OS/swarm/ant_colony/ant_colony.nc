// ══════════════════════════════════════════════════════════════════
//  HiveANT — Ant Colony Optimization (ACO) for Root Cause Discovery
//
//  Agents explore causal relationships between:
//    logs, metrics, traces, deployments, config changes, dependencies
//
//  Each causal path receives a pheromone score.
//  Successful discoveries reinforce pheromone strength.
//  Weak paths decay over time (evaporation).
//
//  Outcome: Highest-scoring path = most probable root cause chain.
// ══════════════════════════════════════════════════════════════════

to ant_colony_init with config:
    purpose: "Initialize the ant colony algorithm parameters"
    shell("mkdir -p memory/pheromone")

    if config:
        set params to config
    otherwise:
        set params to {
            "colony_size": 50,
            "max_iterations": 100,
            "alpha": 1.0,
            "beta": 2.0,
            "evaporation_rate": 0.1,
            "pheromone_deposit": 1.0,
            "min_pheromone": 0.01,
            "max_pheromone": 10.0,
            "elite_ant_bonus": 2.0
        }

    write_file("memory/pheromone/aco_config.json", json_encode(params))
    log "ACO: Colony initialized with " + str(params.colony_size) + " ants"
    respond with params

to ant_explore_causal_paths with signals, system_context:
    purpose: "Ant agents explore causal paths between symptoms and root causes"
    shell("mkdir -p memory/pheromone")

    if file_exists("memory/pheromone/aco_config.json"):
        set params to json_decode(read_file("memory/pheromone/aco_config.json"))
    otherwise:
        set params to {"colony_size": 50, "max_iterations": 100, "alpha": 1.0, "beta": 2.0, "evaporation_rate": 0.1, "pheromone_deposit": 1.0, "min_pheromone": 0.01}

    set existing_paths to "none"
    if file_exists("memory/pheromone/causal_paths.json"):
        set existing_paths to read_file("memory/pheromone/causal_paths.json")

    ask AI to "You are an Ant Colony Optimization agent exploring causal paths in a software system.

SIGNALS DETECTED:
{{signals}}

SYSTEM CONTEXT:
{{system_context}}

EXISTING PHEROMONE PATHS (from past explorations):
{{existing_paths}}

ACO PARAMETERS:
- Colony size: {{params.colony_size}} ants
- Alpha (pheromone influence): {{params.alpha}}
- Beta (heuristic influence): {{params.beta}}
- Evaporation rate: {{params.evaporation_rate}}

TASK: Simulate {{params.colony_size}} ants exploring causal paths from symptoms to root causes.
Each ant builds a path through the causal graph: symptom -> intermediate_node -> ... -> root_cause.
Score each path based on evidence strength.
Apply pheromone reinforcement to the best paths.

Return ONLY valid JSON:
{
  \"iteration\": 1,
  \"paths_explored\": [
    {
      \"ant_id\": 0,
      \"path\": [\"symptom_node\", \"intermediate_node\", \"root_cause_node\"],
      \"path_description\": \"string describing the causal chain\",
      \"pheromone_score\": 0.0,
      \"evidence_strength\": 0.0,
      \"confidence\": 0.0
    }
  ],
  \"best_path\": {
    \"path\": [\"node1\", \"node2\", \"root_cause\"],
    \"description\": \"string\",
    \"pheromone_score\": 0.0,
    \"confidence\": 0.0
  },
  \"pheromone_updates\": [
    {\"edge\": [\"from_node\", \"to_node\"], \"new_score\": 0.0, \"delta\": 0.0}
  ],
  \"root_cause_candidates\": [
    {\"cause\": \"string\", \"probability\": 0.0, \"path_count\": 0, \"total_pheromone\": 0.0}
  ],
  \"convergence\": {\"converged\": false, \"iterations_needed\": 0, \"variance\": 0.0}
}" save as aco_result

    write_file("memory/pheromone/causal_paths.json", json_encode(aco_result))

    set graph_path to "memory/pheromone/graph.json"
    if file_exists(graph_path):
        set graph to json_decode(read_file(graph_path))
    otherwise:
        set graph to {"nodes": {}, "edges": {}, "updated_at": time_now()}

    if aco_result.pheromone_updates:
        repeat for each update in aco_result.pheromone_updates:
            set edge_key to str(update.edge[0]) + "->" + str(update.edge[1])
            set graph.edges[edge_key] to {
                "from": update.edge[0],
                "to": update.edge[1],
                "pheromone_score": update.new_score,
                "last_updated": time_now()
            }

    set graph.updated_at to time_now()
    write_file(graph_path, json_encode(graph))

    log "ACO: Explored " + str(len(aco_result.paths_explored)) + " causal paths"
    log "ACO: Best path confidence = " + str(aco_result.best_path.confidence)
    respond with aco_result

to ant_evaporate_pheromones:
    purpose: "Apply pheromone evaporation to all edges (decay weak paths)"
    set graph_path to "memory/pheromone/graph.json"
    if file_exists(graph_path):
        set graph to json_decode(read_file(graph_path))
        if file_exists("memory/pheromone/aco_config.json"):
            set params to json_decode(read_file("memory/pheromone/aco_config.json"))
        otherwise:
            set params to {"evaporation_rate": 0.1, "min_pheromone": 0.01}

        set evaporated to 0
        set removed to 0

        write_file("memory/pheromone/evaporation_log.json", json_encode({
            "evaporated_edges": evaporated,
            "removed_edges": removed,
            "rate": params.evaporation_rate,
            "timestamp": time_now()
        }))
        set graph.updated_at to time_now()
        write_file(graph_path, json_encode(graph))

        log "ACO: Evaporation complete — " + str(evaporated) + " edges decayed, " + str(removed) + " removed"
        respond with {"evaporated": evaporated, "removed": removed, "rate": params.evaporation_rate}
    otherwise:
        respond with {"error": "No pheromone graph found. Run ant_explore first."}

to ant_reinforce_path with path_nodes, success_score:
    purpose: "Reinforce pheromone on a confirmed causal path"
    set graph_path to "memory/pheromone/graph.json"
    if file_exists(graph_path):
        set graph to json_decode(read_file(graph_path))
        if file_exists("memory/pheromone/aco_config.json"):
            set params to json_decode(read_file("memory/pheromone/aco_config.json"))
        otherwise:
            set params to {"pheromone_deposit": 1.0, "max_pheromone": 10.0, "elite_ant_bonus": 2.0}

        set deposit to params.pheromone_deposit * success_score
        set reinforced to 0

        set idx to 0
        repeat for each node in path_nodes:
            if idx is above 0:
                set prev to path_nodes[idx - 1]
                set edge_key to str(prev) + "->" + str(node)
                if graph.edges[edge_key]:
                    set current to graph.edges[edge_key].pheromone_score
                    set new_score to current + deposit
                    if new_score is above params.max_pheromone:
                        set new_score to params.max_pheromone
                    set graph.edges[edge_key].pheromone_score to new_score
                    set graph.edges[edge_key].last_updated to time_now()
                    set graph.edges[edge_key].reinforcement_count to (graph.edges[edge_key].reinforcement_count) + 1
                otherwise:
                    set graph.edges[edge_key] to {
                        "from": prev,
                        "to": node,
                        "pheromone_score": deposit,
                        "last_updated": time_now(),
                        "reinforcement_count": 1
                    }
                set reinforced to reinforced + 1
            set idx to idx + 1

        set graph.updated_at to time_now()
        write_file(graph_path, json_encode(graph))

        log "ACO: Reinforced " + str(reinforced) + " edges with deposit=" + str(deposit)
        respond with {"reinforced_edges": reinforced, "deposit": deposit, "path": path_nodes}
    otherwise:
        respond with {"error": "No pheromone graph found"}

to ant_get_best_paths with top_n:
    purpose: "Get the highest-pheromone causal paths"
    if file_exists("memory/pheromone/causal_paths.json"):
        set data to json_decode(read_file("memory/pheromone/causal_paths.json"))
        respond with {
            "best_path": data.best_path,
            "root_cause_candidates": data.root_cause_candidates,
            "convergence": data.convergence
        }
    otherwise:
        respond with {"error": "No paths explored yet. Run POST /swarm/ant/explore first."}

to ant_colony_status:
    purpose: "Get current ant colony status and pheromone graph summary"
    set config to "not initialized"
    if file_exists("memory/pheromone/aco_config.json"):
        set config to json_decode(read_file("memory/pheromone/aco_config.json"))

    set edge_count to 0
    if file_exists("memory/pheromone/graph.json"):
        set graph to json_decode(read_file("memory/pheromone/graph.json"))
        set edge_count to len(graph.edges)

    set has_paths to false
    if file_exists("memory/pheromone/causal_paths.json"):
        set has_paths to true

    respond with {
        "algorithm": "Ant Colony Optimization",
        "config": config,
        "pheromone_graph_edges": edge_count,
        "has_explored_paths": has_paths
    }
