// ══════════════════════════════════════════════════════════════════
//  HiveANT — Task Graph Engine
//
//  All work is structured as directed acyclic graphs (DAGs).
//  Each investigation creates a goal node that decomposes into
//  subtasks. Agents claim tasks from the graph rather than
//  generating arbitrary work.
// ══════════════════════════════════════════════════════════════════

to create_task_graph with goal, description, subtasks:
    purpose: "Create a new task graph rooted at a goal"
    shell("mkdir -p memory/tasks")

    set graph_id to "TG-" + str(floor(random() * 90000 + 10000))

    set nodes to []
    set goal_node to {
        "id": graph_id + "-ROOT",
        "type": "goal",
        "description": goal,
        "status": "pending",
        "children": [],
        "created_at": time_now()
    }

    if subtasks:
        set idx to 0
        repeat for each st in subtasks:
            set child_id to graph_id + "-T" + str(idx)
            set child to {
                "id": child_id,
                "type": "task",
                "description": st.description,
                "agent_type": st.agent_type,
                "status": "pending",
                "parent": goal_node.id,
                "dependencies": st.dependencies,
                "priority": st.priority,
                "created_at": time_now()
            }
            set nodes to nodes + [child]
            set goal_node.children to goal_node.children + [child_id]
            set idx to idx + 1

    set graph to {
        "graph_id": graph_id,
        "goal": goal,
        "description": description,
        "status": "active",
        "root": goal_node,
        "nodes": nodes,
        "created_at": time_now(),
        "completed_at": ""
    }

    write_file("memory/tasks/" + graph_id + ".json", json_encode(graph))
    log "TASK_GRAPH: Created " + graph_id + " with " + str(len(nodes)) + " subtasks"
    respond with graph

to get_task_graph with graph_id:
    purpose: "Retrieve a task graph by ID"
    set path to "memory/tasks/" + graph_id + ".json"
    if file_exists(path):
        respond with json_decode(read_file(path))
    otherwise:
        respond with {"error": "Task graph not found", "_status": 404}

to claim_task with graph_id, task_id, agent_id:
    purpose: "Agent claims a task from the graph"
    set path to "memory/tasks/" + graph_id + ".json"
    if file_exists(path):
        set graph to json_decode(read_file(path))
        set found to false
        set idx to 0
        repeat for each node in graph.nodes:
            if node.id is equal task_id:
                if node.status is equal "pending":
                    set node.status to "in_progress"
                    set node.claimed_by to agent_id
                    set node.claimed_at to time_now()
                    set graph.nodes[idx] to node
                    set found to true
                otherwise:
                    respond with {"error": "Task already claimed", "status": node.status}
            set idx to idx + 1

        if found:
            write_file(path, json_encode(graph))
            log "TASK_GRAPH: Agent " + agent_id + " claimed " + task_id
            respond with {"task_id": task_id, "agent_id": agent_id, "status": "in_progress"}
        otherwise:
            respond with {"error": "Task not found in graph"}
    otherwise:
        respond with {"error": "Graph not found", "_status": 404}

to complete_graph_task with graph_id, task_id, result:
    purpose: "Mark a task in the graph as completed"
    set path to "memory/tasks/" + graph_id + ".json"
    if file_exists(path):
        set graph to json_decode(read_file(path))
        set idx to 0
        repeat for each node in graph.nodes:
            if node.id is equal task_id:
                set node.status to "completed"
                set node.result to result
                set node.completed_at to time_now()
                set graph.nodes[idx] to node
            set idx to idx + 1

        set all_done to true
        repeat for each node in graph.nodes:
            if node.status is not equal "completed":
                set all_done to false

        if all_done:
            set graph.status to "completed"
            set graph.completed_at to time_now()
            log "TASK_GRAPH: All tasks complete for " + graph_id

        write_file(path, json_encode(graph))
        respond with {"task_id": task_id, "status": "completed", "graph_complete": all_done}
    otherwise:
        respond with {"error": "Graph not found", "_status": 404}

to generate_investigation_graph with service_name, description:
    purpose: "Auto-generate a standard investigation task graph"
    set subtasks to [
        {"description": "Analyze system logs for errors and anomalies", "agent_type": "investigation", "dependencies": [], "priority": 1},
        {"description": "Collect and analyze metrics (latency, error rate, CPU, memory)", "agent_type": "investigation", "dependencies": [], "priority": 1},
        {"description": "Inspect service dependency graph for cascading failures", "agent_type": "investigation", "dependencies": [], "priority": 2},
        {"description": "Review recent deployments and configuration changes", "agent_type": "investigation", "dependencies": [], "priority": 2},
        {"description": "Run ant colony root cause analysis on causal paths", "agent_type": "root_cause", "dependencies": ["T0", "T1", "T2", "T3"], "priority": 3},
        {"description": "Generate remediation strategies via bee colony optimization", "agent_type": "fix_generation", "dependencies": ["T4"], "priority": 4},
        {"description": "Validate proposed fix in sandbox environment", "agent_type": "validation", "dependencies": ["T5"], "priority": 5},
        {"description": "Store incident knowledge and update pheromone scores", "agent_type": "learning", "dependencies": ["T6"], "priority": 6}
    ]

    set goal to "Investigate anomaly in " + service_name + ": " + description
    set graph_id to "TG-" + str(floor(random() * 90000 + 10000))

    set nodes to []
    set goal_node to {
        "id": graph_id + "-ROOT",
        "type": "goal",
        "description": goal,
        "status": "pending",
        "children": [],
        "created_at": time_now()
    }

    set idx to 0
    repeat for each st in subtasks:
        set child_id to graph_id + "-T" + str(idx)
        set child to {
            "id": child_id,
            "type": "task",
            "description": st.description,
            "agent_type": st.agent_type,
            "status": "pending",
            "parent": goal_node.id,
            "dependencies": st.dependencies,
            "priority": st.priority,
            "created_at": time_now()
        }
        set nodes to nodes + [child]
        set goal_node.children to goal_node.children + [child_id]
        set idx to idx + 1

    set graph to {
        "graph_id": graph_id,
        "goal": goal,
        "description": description,
        "service": service_name,
        "status": "active",
        "root": goal_node,
        "nodes": nodes,
        "created_at": time_now(),
        "completed_at": ""
    }

    write_file("memory/tasks/" + graph_id + ".json", json_encode(graph))
    log "TASK_GRAPH: Generated investigation graph " + graph_id + " for " + service_name
    respond with graph

to list_task_graphs:
    purpose: "List all task graphs"
    set result to shell("ls memory/tasks/TG-*.json 2>/dev/null || echo NONE")
    if result is equal "NONE":
        respond with {"graphs": [], "count": 0}
    set graphs to []
    set files to split(trim(result), "\n")
    repeat for each f in files:
        set data to json_decode(read_file(f))
        set graphs to graphs + [{"graph_id": data.graph_id, "goal": data.goal, "status": data.status, "node_count": len(data.nodes), "created_at": data.created_at}]
    respond with {"graphs": graphs, "count": len(graphs)}
