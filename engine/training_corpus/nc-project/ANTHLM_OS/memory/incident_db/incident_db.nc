// ══════════════════════════════════════════════════════════════════
//  HiveANT — Incident Database
//
//  Stores and retrieves incident records including:
//    symptoms, root causes, solutions, confidence scores
// ══════════════════════════════════════════════════════════════════

to incident_store with incident_id, data:
    purpose: "Store an incident record"
    shell("mkdir -p incidents")
    write_file("incidents/" + incident_id + ".json", json_encode(data))
    respond with {"stored": true, "incident_id": incident_id}

to incident_get with incident_id:
    purpose: "Retrieve an incident by ID"
    set path to "incidents/" + incident_id + ".json"
    if file_exists(path):
        respond with json_decode(read_file(path))
    otherwise:
        respond with {"error": "Not found", "_status": 404}

to incident_list with limit:
    purpose: "List recent incidents"
    if limit:
        set lim to str(limit)
    otherwise:
        set lim to "50"
    set result to shell("ls -t incidents/*.json 2>/dev/null | head -" + lim + " | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"incidents": [], "count": 0}
    set ids to split(trim(result), "\n")
    respond with {"incidents": ids, "count": len(ids)}

to incident_search with query:
    purpose: "Search incidents by keyword"
    set result to shell("grep -rl '\"" + query + "\"' incidents/*.json 2>/dev/null | while read f; do basename \"$f\" .json; done || echo NONE")
    if result is equal "NONE":
        respond with {"query": query, "matches": [], "count": 0}
    set ids to split(trim(result), "\n")
    respond with {"query": query, "matches": ids, "count": len(ids)}

to incident_stats:
    purpose: "Get incident statistics"
    set total to shell("ls incidents/*.json 2>/dev/null | wc -l || echo 0")
    set categories to shell("cat incidents/*.json 2>/dev/null | grep -o '\"category\":\"[^\"]*\"' | sort | uniq -c | sort -rn | head -10 || echo 'none'")
    respond with {"total": trim(total), "categories": categories}
