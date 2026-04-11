// ══════════════════════════════════════════════════════════════════
//  HiveANT — Vector Store
//
//  Semantic search over incidents, patterns, and documentation.
//  Uses NC's built-in chunk() and top_k() for RAG retrieval.
// ══════════════════════════════════════════════════════════════════

to vector_index with source_type:
    purpose: "Index documents for semantic search"
    shell("mkdir -p docs knowledge")

    if source_type is equal "incidents":
        set content to shell("ls incidents/*.json 2>/dev/null | while read f; do cat \"$f\" 2>/dev/null | head -c 500; echo '---'; done || echo NONE")
    otherwise:
        set content to shell("cat docs/*.md docs/*.txt 2>/dev/null || echo ''")

    if len(content) is above 10:
        set chunks to chunk(content, 800, 100)
        set chunk_count to len(chunks)
        respond with {"status": "indexed", "source": source_type, "chunks": chunk_count}
    otherwise:
        respond with {"status": "empty", "source": source_type, "chunks": 0}

to vector_search with query, source_type, top_n:
    purpose: "Semantic search across indexed content"

    if source_type is equal "incidents":
        set content to shell("ls incidents/*.json 2>/dev/null | tail -30 | while read f; do cat \"$f\" 2>/dev/null | head -c 500; echo '---'; done || echo ''")
    otherwise:
        set content to shell("cat docs/*.md docs/*.txt docs/*.nc 2>/dev/null || echo ''")

    if len(content) is above 10:
        set doc_chunks to chunk(content, 600, 80)
        if top_n:
            set k to top_n
        otherwise:
            set k to 5
        set results to top_k(doc_chunks, k)
        respond with {"query": query, "results": results, "chunks_searched": len(doc_chunks)}
    otherwise:
        respond with {"query": query, "results": [], "message": "No content to search"}
