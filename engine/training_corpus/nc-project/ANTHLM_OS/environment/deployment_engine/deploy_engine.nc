// ══════════════════════════════════════════════════════════════════
//  HiveANT — Deployment Engine (Environment Interface)
//
//  Interacts with deployment infrastructure:
//    - GitHub / GitLab
//    - Kubernetes deployments
//    - Container orchestration
// ══════════════════════════════════════════════════════════════════

to pull_deploys:
    purpose: "Get recent deployments from GitHub"
    set gh_token to env("GITHUB_TOKEN")
    set gh_repo to env("GITHUB_REPO")
    if gh_token:
        set deploy_url to "https://api.github.com/repos/" + gh_repo + "/deployments?per_page=10"
        gather result from deploy_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        set commits_url to "https://api.github.com/repos/" + gh_repo + "/commits?per_page=10"
        gather commits from commits_url:
            headers: {"Authorization": "Bearer " + gh_token, "Accept": "application/vnd.github.v3+json"}
        respond with {"source": "github", "deployments": result, "recent_commits": commits}
    otherwise:
        respond with {"source": "github", "error": "GITHUB_TOKEN not configured"}

to k8s_deployments with namespace:
    purpose: "List Kubernetes deployments"
    if namespace:
        set result to shell("kubectl get deployments -n " + namespace + " -o json 2>&1")
    otherwise:
        set result to shell("kubectl get deployments --all-namespaces -o json 2>&1")
    respond with result

to k8s_rollback with deployment, namespace, secret:
    purpose: "Rollback a Kubernetes deployment"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}
    set result to shell("kubectl rollout undo deployment/" + deployment + " -n " + namespace + " 2>&1")
    respond with {"action": "rollback", "deployment": deployment, "namespace": namespace, "result": result}

to k8s_restart with deployment, namespace, secret:
    purpose: "Restart a Kubernetes deployment"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}
    set result to shell("kubectl rollout restart deployment/" + deployment + " -n " + namespace + " 2>&1")
    respond with {"action": "restart", "deployment": deployment, "namespace": namespace, "result": result}

to k8s_scale with deployment, namespace, replicas, secret:
    purpose: "Scale a Kubernetes deployment"
    set expected to env("HEAL_SECRET")
    if expected:
        if secret is not equal expected:
            respond with {"error": "Unauthorized", "_status": 401}
    if replicas:
        set r to str(replicas)
    otherwise:
        set r to "3"
    set result to shell("kubectl scale deployment/" + deployment + " --replicas=" + r + " -n " + namespace + " 2>&1")
    respond with {"action": "scale", "deployment": deployment, "namespace": namespace, "replicas": r, "result": result}
