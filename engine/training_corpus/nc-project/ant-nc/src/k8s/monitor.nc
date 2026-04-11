// ══════════════════════════════════════════════════════════════════
//  KUBERNETES — Monitor all pods, pull logs, fix issues
//  Works when SwarmOps runs INSIDE K8s with the ServiceAccount
// ══════════════════════════════════════════════════════════════════

to k8s_pods with namespace:
    purpose: "List all pods in a namespace or all namespaces"
    if namespace:
        set result to shell("kubectl get pods -n " + namespace + " -o json 2>&1")
    otherwise:
        set result to shell("kubectl get pods --all-namespaces -o json 2>&1")
    respond with result

to k8s_unhealthy:
    purpose: "Find all unhealthy pods across the cluster"
    set crashloop to shell("kubectl get pods --all-namespaces --field-selector=status.phase!=Running -o wide 2>&1 || echo 'kubectl not available'")
    set restarts to shell("kubectl get pods --all-namespaces -o json 2>&1 | grep -c '\"restartCount\"' || echo 0")
    set not_ready to shell("kubectl get pods --all-namespaces | grep -v Running | grep -v Completed | grep -v NAME 2>&1 || echo 'all healthy'")
    respond with {"unhealthy_pods": not_ready, "crashloop_info": crashloop, "checked_at": time_now()}

to k8s_logs with pod_name, namespace, lines:
    purpose: "Get logs from a specific pod"
    if namespace:
        set result to shell("kubectl logs " + pod_name + " -n " + namespace + " --tail=" + str(lines) + " 2>&1")
    otherwise:
        set result to shell("kubectl logs " + pod_name + " --tail=" + str(lines) + " 2>&1")
    respond with {"pod": pod_name, "namespace": namespace, "logs": result}

to k8s_events with namespace:
    purpose: "Get recent K8s events (warnings, errors)"
    if namespace:
        set result to shell("kubectl get events -n " + namespace + " --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -30")
    otherwise:
        set result to shell("kubectl get events --all-namespaces --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -30")
    respond with {"events": result, "checked_at": time_now()}

to k8s_describe with resource, name, namespace:
    purpose: "Describe a K8s resource (pod, deployment, service)"
    if namespace:
        set result to shell("kubectl describe " + resource + " " + name + " -n " + namespace + " 2>&1")
    otherwise:
        set result to shell("kubectl describe " + resource + " " + name + " 2>&1")
    respond with {"resource": resource, "name": name, "description": result}

to k8s_fix with action, target, namespace, secret:
    purpose: "Fix K8s issues: restart, rollback, scale"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}

    if action is equal "restart":
        set result to shell("kubectl rollout restart deployment/" + target + " -n " + namespace + " 2>&1")
        respond with {"action": "restart", "target": target, "result": result}
    otherwise if action is equal "rollback":
        set result to shell("kubectl rollout undo deployment/" + target + " -n " + namespace + " 2>&1")
        respond with {"action": "rollback", "target": target, "result": result}
    otherwise if action is equal "scale":
        set result to shell("kubectl scale deployment/" + target + " --replicas=3 -n " + namespace + " 2>&1")
        respond with {"action": "scale", "target": target, "result": result}
    otherwise if action is equal "delete-pod":
        set result to shell("kubectl delete pod " + target + " -n " + namespace + " 2>&1")
        respond with {"action": "delete-pod", "target": target, "result": result}
    otherwise:
        respond with {"error": "Unknown action. Use: restart, rollback, scale, delete-pod"}

to k8s_investigate with namespace, description:
    purpose: "Full K8s investigation — pull pods, logs, events, then AI analyzes"
    log "K8S INVESTIGATION: namespace={{namespace}} — {{description}}"

    set pods to shell("kubectl get pods -n " + namespace + " -o wide 2>&1")
    set events to shell("kubectl get events -n " + namespace + " --sort-by=.lastTimestamp --field-selector type!=Normal 2>&1 | tail -20")
    set error_logs to shell("kubectl logs -l app --all-containers --tail=50 -n " + namespace + " 2>&1 | grep -i -E 'error|fatal|exception|fail|timeout|refused' | tail -30")
    set deployments to shell("kubectl get deployments -n " + namespace + " -o wide 2>&1")
    set top_pods to shell("kubectl top pods -n " + namespace + " 2>&1 || echo 'metrics-server not available'")

    set k8s_context to "KUBERNETES INVESTIGATION. Namespace: {{namespace}}. Issue: {{description}}. PODS: {{pods}}. EVENTS: {{events}}. ERROR LOGS: {{error_logs}}. DEPLOYMENTS: {{deployments}}. RESOURCE USAGE: {{top_pods}}."

    // Load memory for better diagnosis
    set semantic_mem to "none"
    if file_exists("knowledge/semantic.json"):
        set semantic_mem to read_file("knowledge/semantic.json")
    set procedural_mem to "none"
    if file_exists("knowledge/procedural.json"):
        set procedural_mem to read_file("knowledge/procedural.json")

    ask AI to "You are SwarmOps K8s Agent. Analyze this Kubernetes cluster data to find the root cause. Check: pod statuses, crash loops, OOMKilled, image pull errors, readiness probe failures, recent events, error logs, resource pressure. Past patterns: {{semantic_mem}}. Known fixes: {{procedural_mem}}. Data: {{k8s_context}}. Return ONLY valid JSON: {\"root_cause\": \"string\", \"confidence_percent\": 0, \"category\": \"string\", \"affected_pods\": [\"string\"], \"evidence\": [\"string\"], \"fix_commands\": [{\"command\": \"string\", \"description\": \"string\", \"risk\": \"low|medium|high\"}], \"timeline\": [{\"time\": \"string\", \"event\": \"string\"}]}" save as diagnosis

    set incident_id to "K8S-" + str(floor(random() * 9000 + 1000))
    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(diagnosis))

    set slack_url to env("SLACK_WEBHOOK")
    if slack_url:
        set slack_msg to "{\"text\":\"*SwarmOps K8s Alert — " + incident_id + "*\\n*Namespace:* " + namespace + "\\n*Root Cause:* " + str(diagnosis.root_cause) + "\\n*Confidence:* " + str(diagnosis.confidence_percent) + "%\"}"
        gather slack_result from slack_url:
            method: "POST"
            body: slack_msg

    respond with {"incident_id": incident_id, "namespace": namespace, "diagnosis": diagnosis}
