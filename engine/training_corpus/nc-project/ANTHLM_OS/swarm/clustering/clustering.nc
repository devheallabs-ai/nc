// ══════════════════════════════════════════════════════════════════
//  HiveANT — Swarm Clustering (Hierarchical Coordination)
//
//  To support thousands of agents, organize them in clusters:
//    Global Coordinator → Cluster Managers → Worker Agent Swarms
//
//  Scale: 10,000 agents / 100 clusters / 100 agents per cluster
//  Clusters operate independently and share summarized results.
// ══════════════════════════════════════════════════════════════════

to cluster_init with cluster_count, agents_per_cluster:
    purpose: "Initialize hierarchical swarm clustering"
    shell("mkdir -p agents_state/clusters")

    if cluster_count:
        set num_clusters to cluster_count
    otherwise:
        set num_clusters to 10

    if agents_per_cluster:
        set per_cluster to agents_per_cluster
    otherwise:
        set per_cluster to 100

    set coordinator to {
        "role": "global_coordinator",
        "cluster_count": num_clusters,
        "agents_per_cluster": per_cluster,
        "total_capacity": num_clusters * per_cluster,
        "clusters": [],
        "status": "active",
        "created_at": time_now()
    }

    set idx to 0
    repeat for each i in range(num_clusters):
        set cluster_id to "CLUSTER-" + str(idx)
        set cluster to {
            "id": cluster_id,
            "manager_id": "MGR-" + str(idx),
            "agent_capacity": per_cluster,
            "active_agents": 0,
            "status": "ready",
            "specialization": "general",
            "created_at": time_now()
        }
        write_file("agents_state/clusters/" + cluster_id + ".json", json_encode(cluster))
        set coordinator.clusters to coordinator.clusters + [cluster_id]
        set idx to idx + 1

    write_file("agents_state/coordinator.json", json_encode(coordinator))
    log "CLUSTERING: Initialized " + str(num_clusters) + " clusters (capacity=" + str(coordinator.total_capacity) + " agents)"
    respond with coordinator

to cluster_assign_agent with agent_id, agent_type, preferred_cluster:
    purpose: "Assign an agent to a cluster (load-balanced)"
    if file_exists("agents_state/coordinator.json"):
        set coord to json_decode(read_file("agents_state/coordinator.json"))

        set target_cluster to ""
        if preferred_cluster:
            set target_cluster to preferred_cluster
        otherwise:
            set min_load to 999999
            repeat for each cid in coord.clusters:
                set cpath to "agents_state/clusters/" + cid + ".json"
                if file_exists(cpath):
                    set c to json_decode(read_file(cpath))
                    if c.active_agents is below min_load:
                        set min_load to c.active_agents
                        set target_cluster to cid

        if target_cluster:
            set cpath to "agents_state/clusters/" + target_cluster + ".json"
            if file_exists(cpath):
                set c to json_decode(read_file(cpath))
                if c.active_agents is below c.agent_capacity:
                    set c.active_agents to c.active_agents + 1
                    write_file(cpath, json_encode(c))
                    log "CLUSTERING: Assigned agent " + agent_id + " to " + target_cluster
                    respond with {"agent_id": agent_id, "cluster": target_cluster, "cluster_load": c.active_agents}
                otherwise:
                    respond with {"error": "Cluster at capacity", "cluster": target_cluster}
        otherwise:
            respond with {"error": "No available cluster found"}
    otherwise:
        respond with {"error": "Clustering not initialized"}

to cluster_status:
    purpose: "Get status of all clusters"
    if file_exists("agents_state/coordinator.json"):
        set coord to json_decode(read_file("agents_state/coordinator.json"))
        set clusters to []
        repeat for each cid in coord.clusters:
            set cpath to "agents_state/clusters/" + cid + ".json"
            if file_exists(cpath):
                set c to json_decode(read_file(cpath))
                set clusters to clusters + [c]

        respond with {
            "coordinator": "active",
            "cluster_count": coord.cluster_count,
            "total_capacity": coord.total_capacity,
            "clusters": clusters
        }
    otherwise:
        respond with {"error": "Clustering not initialized"}

to cluster_broadcast with message, target_clusters:
    purpose: "Broadcast a message to all or selected clusters"
    if file_exists("agents_state/coordinator.json"):
        set coord to json_decode(read_file("agents_state/coordinator.json"))
        set delivered to 0

        if target_clusters:
            set targets to target_clusters
        otherwise:
            set targets to coord.clusters

        repeat for each cid in targets:
            set msg_path to "agents_state/clusters/" + cid + "-inbox.json"
            set msg to {
                "type": "broadcast",
                "content": message,
                "from": "global_coordinator",
                "sent_at": time_now()
            }
            write_file(msg_path, json_encode(msg))
            set delivered to delivered + 1

        log "CLUSTERING: Broadcast delivered to " + str(delivered) + " clusters"
        respond with {"delivered": delivered, "message": message}
    otherwise:
        respond with {"error": "Clustering not initialized"}

to cluster_collect_results:
    purpose: "Collect summarized results from all clusters"
    if file_exists("agents_state/coordinator.json"):
        set coord to json_decode(read_file("agents_state/coordinator.json"))
        set results to []
        repeat for each cid in coord.clusters:
            set outbox_path to "agents_state/clusters/" + cid + "-outbox.json"
            if file_exists(outbox_path):
                set r to json_decode(read_file(outbox_path))
                set results to results + [{"cluster": cid, "result": r}]

        respond with {"results": results, "cluster_count": len(results)}
    otherwise:
        respond with {"error": "Clustering not initialized"}
