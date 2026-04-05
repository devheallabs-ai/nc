service "release-memory-policy-smoke"
version "1.0.0"

to smoke:
    set mem_path to "engine/build/release_memory_policy_smoke_store.json"
    set policy_path to "engine/build/release_policy_smoke_store.json"
    set meta to {"source": "release", "topic": "checkpointing"}

    set stored to memory_store(mem_path, "project", "checkpoint resume autosave optimizer state", meta, 0.9)
    set reflected to memory_reflect(mem_path, "training resume default", "resume by default worked", "manual resume flag caused friction", 0.8, "keep auto-resume enabled")
    set context to memory_context(mem_path, "resume optimizer autosave", 3)

    set use_memory to policy_update(policy_path, "use_memory", 1.0)
    set skip_memory to policy_update(policy_path, "skip_memory", -0.25)
    set choice to policy_choose(policy_path, ["use_memory", "skip_memory"], 0.0)
    set stats to policy_stats(policy_path)

    respond with {
        "stored": stored,
        "reflected": reflected,
        "context": context,
        "choice": choice,
        "stats": stats,
        "use_memory": use_memory,
        "skip_memory": skip_memory
    }