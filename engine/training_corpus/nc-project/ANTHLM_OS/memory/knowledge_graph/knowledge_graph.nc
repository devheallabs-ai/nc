// ══════════════════════════════════════════════════════════════════
//  HiveANT — Knowledge Graph
//
//  Stores service relationships, dependencies, and learned
//  system knowledge as a queryable graph structure.
// ══════════════════════════════════════════════════════════════════

to kg_init:
    purpose: "Initialize the knowledge graph"
    shell("mkdir -p memory/knowledge_graph")
    set kg to {"nodes": [], "edges": [], "node_count": 0, "edge_count": 0, "created_at": time_now()}
    write_file("memory/knowledge_graph/graph.json", json_encode(kg))
    respond with kg

to kg_add_entity with entity_id, entity_type, properties:
    purpose: "Add an entity (node) to the knowledge graph"
    shell("mkdir -p memory/knowledge_graph/entities")
    set entity to {"id": entity_id, "type": entity_type, "properties": properties, "created_at": time_now()}
    write_file("memory/knowledge_graph/entities/" + entity_id + ".json", json_encode(entity))
    respond with {"entity_id": entity_id, "type": entity_type, "status": "added"}

to kg_add_relation with from_entity, to_entity, relation_type, properties:
    purpose: "Add a relationship (edge) between entities"
    shell("mkdir -p memory/knowledge_graph/relations")
    set rel_id to from_entity + "__" + relation_type + "__" + to_entity
    set relation to {"id": rel_id, "from": from_entity, "to": to_entity, "type": relation_type, "properties": properties, "created_at": time_now()}
    write_file("memory/knowledge_graph/relations/" + rel_id + ".json", json_encode(relation))
    respond with {"relation_id": rel_id, "status": "added"}

to kg_query with entity_id:
    purpose: "Query all relationships for an entity"
    set entity_path to "memory/knowledge_graph/entities/" + entity_id + ".json"
    if file_exists(entity_path):
        set entity to json_decode(read_file(entity_path))
        set relations to shell("ls memory/knowledge_graph/relations/" + entity_id + "__*.json memory/knowledge_graph/relations/*__" + entity_id + ".json 2>/dev/null || echo NONE")
        respond with {"entity": entity, "relations": relations}
    otherwise:
        respond with {"error": "Entity not found"}

to kg_status:
    purpose: "Get knowledge graph statistics"
    set entities to shell("ls memory/knowledge_graph/entities/*.json 2>/dev/null | wc -l || echo 0")
    set relations to shell("ls memory/knowledge_graph/relations/*.json 2>/dev/null | wc -l || echo 0")
    respond with {"entities": trim(entities), "relations": trim(relations)}
