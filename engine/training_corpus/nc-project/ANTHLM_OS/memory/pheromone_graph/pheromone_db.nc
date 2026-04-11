// ══════════════════════════════════════════════════════════════════
//  HiveANT — Pheromone Graph Database
//
//  Stores causal relationships between system events as a graph.
//  Nodes: services, metrics, errors, deployments, config changes
//  Edges: pheromone_score, confidence, success_rate, timestamp
//
//  Agents query this graph during investigation.
//  Successful solutions reinforce paths; weak paths decay.
//
//  NOTE: Now uses map[variable] for dynamic key access (NC engine fix BUG-3)
// ══════════════════════════════════════════════════════════════════

to pheromone_db_init:
    purpose: "Initialize the pheromone graph database"
    shell("mkdir -p memory/pheromone/edges")

    set db to {
        "version": "1.0.0",
        "nodes": {},
        "edges": {},
        "node_count": 0,
        "edge_count": 0,
        "total_reinforcements": 0,
        "total_evaporations": 0,
        "created_at": time_iso(time_now()),
        "updated_at": time_iso(time_now())
    }

    write_file("memory/pheromone/graph_db.json", json_encode(db))
    log "PHEROMONE_DB: Initialized"
    respond with db

to pheromone_add_node with node_id, node_type, metadata:
    purpose: "Add a node to the pheromone graph"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
        otherwise:
            set db to {"nodes": {}, "edges": {}, "node_count": 0, "edge_count": 0, "total_reinforcements": 0, "total_evaporations": 0, "created_at": time_iso(time_now()), "updated_at": time_iso(time_now())}
    otherwise:
        set db to {"nodes": {}, "edges": {}, "node_count": 0, "edge_count": 0, "total_reinforcements": 0, "total_evaporations": 0, "created_at": time_iso(time_now()), "updated_at": time_iso(time_now())}

    set db.nodes[node_id] to {
        "id": node_id,
        "type": node_type,
        "metadata": metadata,
        "connections": 0,
        "total_pheromone_in": 0.0,
        "total_pheromone_out": 0.0,
        "created_at": time_iso(time_now()),
        "last_visited": time_iso(time_now())
    }
    set db.node_count to db.node_count + 1
    set db.updated_at to time_iso(time_now())
    write_file(db_path, json_encode(db))
    respond with {"node_id": node_id, "type": node_type, "status": "added"}

to pheromone_add_edge with from_node, to_node, initial_score, metadata:
    purpose: "Add an edge (causal relationship) to the pheromone graph"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
        otherwise:
            respond with {"error": "Corrupt graph DB file"}
    otherwise:
        respond with {"error": "Graph DB not initialized"}

    set edge_id to from_node + "->" + to_node

    if initial_score:
        set score to initial_score
    otherwise:
        set score to 0.5

    set db.edges[edge_id] to {
        "id": edge_id,
        "from": from_node,
        "to": to_node,
        "pheromone_score": score,
        "confidence": 0.5,
        "success_rate": 0.0,
        "visit_count": 0,
        "reinforcement_count": 0,
        "metadata": metadata,
        "created_at": time_iso(time_now()),
        "last_updated": time_iso(time_now())
    }
    set db.edge_count to db.edge_count + 1
    set db.updated_at to time_iso(time_now())
    write_file(db_path, json_encode(db))

    shell("mkdir -p memory/pheromone/edges")
    write_file("memory/pheromone/edges/" + from_node + "__" + to_node + ".json", json_encode(db.edges[edge_id]))

    respond with {"edge_id": edge_id, "pheromone_score": score, "status": "added"}

to pheromone_reinforce with from_node, to_node, reward, success:
    purpose: "Reinforce pheromone on an edge after successful discovery"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
        otherwise:
            respond with {"error": "Corrupt graph DB"}

        set edge_id to from_node + "->" + to_node

        if db.edges[edge_id]:
            set edge to db.edges[edge_id]
            set edge.pheromone_score to edge.pheromone_score + reward
            if edge.pheromone_score is above 10.0:
                set edge.pheromone_score to 10.0
            set edge.visit_count to edge.visit_count + 1
            set edge.reinforcement_count to edge.reinforcement_count + 1

            if success is equal "yes":
                set total to edge.visit_count
                set old_rate to edge.success_rate
                set edge.success_rate to old_rate + (1.0 - old_rate) / total
            otherwise:
                set total to edge.visit_count
                set old_rate to edge.success_rate
                set edge.success_rate to old_rate - old_rate / total

            set edge.confidence to edge.pheromone_score * edge.success_rate
            set edge.last_updated to time_iso(time_now())
            set db.edges[edge_id] to edge
            set db.total_reinforcements to db.total_reinforcements + 1
        otherwise:
            set db.edges[edge_id] to {
                "id": edge_id,
                "from": from_node,
                "to": to_node,
                "pheromone_score": reward,
                "confidence": reward * 0.5,
                "success_rate": 0.5,
                "visit_count": 1,
                "reinforcement_count": 1,
                "created_at": time_iso(time_now()),
                "last_updated": time_iso(time_now())
            }
            set db.edge_count to db.edge_count + 1

        set db.updated_at to time_iso(time_now())
        write_file(db_path, json_encode(db))

        write_file("memory/pheromone/edges/" + from_node + "__" + to_node + ".json", json_encode(db.edges[edge_id]))

        respond with {"edge_id": edge_id, "new_score": db.edges[edge_id].pheromone_score, "confidence": db.edges[edge_id].confidence}
    otherwise:
        respond with {"error": "Graph DB not initialized"}

to pheromone_evaporate_all with rate:
    purpose: "Apply evaporation to all edges"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
        otherwise:
            respond with {"error": "Corrupt graph DB"}

        if rate:
            set evap_rate to rate
        otherwise:
            set evap_rate to 0.05

        set min_threshold to 0.01
        set evaporated to 0
        set removed to 0

        set edge_files to shell("ls memory/pheromone/edges/*.json 2>/dev/null || echo NONE")
        if edge_files is not equal "NONE":
            set files to split(edge_files, "\n")
            repeat for each f in files:
                if file_exists(f):
                    try:
                        set edge to json_decode(read_file(f))
                        set new_score to edge.pheromone_score * (1.0 - evap_rate)
                        if new_score is below min_threshold:
                            shell("rm -f '" + f + "'")
                            set removed to removed + 1
                        otherwise:
                            set edge.pheromone_score to new_score
                            set edge.last_updated to time_iso(time_now())
                            write_file(f, json_encode(edge))
                            set evaporated to evaporated + 1
                    otherwise:
                        log "WARN: Corrupt edge file " + f
                        continue

        set db.total_evaporations to db.total_evaporations + 1
        set db.updated_at to time_iso(time_now())
        set db.edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
        write_file(db_path, json_encode(db))

        log "PHEROMONE_DB: Evaporation cycle complete, rate=" + str(evap_rate) + " decayed=" + str(evaporated) + " removed=" + str(removed)
        respond with {"evaporation_rate": evap_rate, "edges_decayed": evaporated, "edges_removed": removed, "cycle": db.total_evaporations}
    otherwise:
        respond with {"error": "Graph DB not initialized"}

to pheromone_query with from_node, min_score:
    purpose: "Query edges from a node above a minimum pheromone threshold"
    if min_score:
        set threshold to min_score
    otherwise:
        set threshold to 0.01

    set safe_from to replace(replace(str(from_node), ";", ""), "|", "")
    set result to shell("ls memory/pheromone/edges/" + safe_from + "__*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"from": from_node, "threshold": threshold, "edges": [], "count": 0}

    set edges to []
    set files to split(result, "\n")
    repeat for each f in files:
        if file_exists(f):
            try:
                set edge to json_decode(read_file(f))
                if edge.pheromone_score is above threshold:
                    set edges to edges + [edge]
            otherwise:
                continue

    respond with {"from": from_node, "threshold": threshold, "edges": edges, "count": len(edges)}

to pheromone_get_strongest_paths with top_n:
    purpose: "Get the strongest pheromone paths in the graph"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
            respond with {
                "node_count": db.node_count,
                "edge_count": db.edge_count,
                "total_reinforcements": db.total_reinforcements,
                "total_evaporations": db.total_evaporations,
                "updated_at": db.updated_at
            }
        otherwise:
            respond with {"error": "Corrupt graph DB"}
    otherwise:
        respond with {"error": "Graph DB not initialized"}

to pheromone_db_status:
    purpose: "Get pheromone graph database statistics"
    set db_path to "memory/pheromone/graph_db.json"
    if file_exists(db_path):
        try:
            set db to json_decode(read_file(db_path))
            set edge_count to shell("ls memory/pheromone/edges/*.json 2>/dev/null | wc -l || echo 0")
            respond with {
                "version": db.version,
                "node_count": db.node_count,
                "edge_count": edge_count,
                "total_reinforcements": db.total_reinforcements,
                "total_evaporations": db.total_evaporations,
                "created_at": db.created_at,
                "updated_at": db.updated_at
            }
        otherwise:
            respond with {"error": "Corrupt graph DB"}
    otherwise:
        respond with {"status": "not_initialized", "message": "Call POST /pheromone/init to initialize"}
