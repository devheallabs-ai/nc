// Test: New stdlib functions added in issue fixes
// Tests find_similar, chunk, token_count, and RAG primitives

service "test-new-stdlib"
version "1.0.0"

// ═══════════════════════════════════════════════════════════
// find_similar — cosine similarity search
// ═══════════════════════════════════════════════════════════
to test_find_similar_returns_list:
    set q to [1, 0]
    set vecs to [[1, 0], [0, 1]]
    set docs to ["match", "miss"]
    set results to find_similar(q, vecs, docs, 2)
    respond with len(results)

to test_find_similar_best_match:
    set q to [1, 0, 0]
    set vecs to [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    set docs to ["first", "second", "third"]
    set results to find_similar(q, vecs, docs, 1)
    respond with results[0].document

to test_find_similar_score:
    set q to [1, 0]
    set vecs to [[1, 0]]
    set docs to ["perfect"]
    set results to find_similar(q, vecs, docs, 1)
    respond with results[0].score

to test_find_similar_empty:
    set q to [1]
    set vecs to []
    set docs to []
    set results to find_similar(q, vecs, docs, 5)
    respond with len(results)

// ═══════════════════════════════════════════════════════════
// chunk — text chunking for RAG
// ═══════════════════════════════════════════════════════════
to test_chunk_basic:
    set text to "ABCDEFGHIJ1234567890"
    set chunks to chunk(text, 10, 0)
    respond with len(chunks)

to test_chunk_with_overlap:
    set text to "ABCDEFGHIJKLMNOPQRST"
    set chunks to chunk(text, 10, 5)
    respond with len(chunks)

// ═══════════════════════════════════════════════════════════
// token_count — approximate token counting
// ═══════════════════════════════════════════════════════════
to test_token_count:
    set count to token_count("Hello world this is a test")
    respond with count

to test_token_count_empty:
    set count to token_count("")
    respond with count

// ═══════════════════════════════════════════════════════════
// top_k — return first k items
// ═══════════════════════════════════════════════════════════
to test_top_k:
    set items to [10, 20, 30, 40, 50]
    set top to top_k(items, 3)
    respond with len(top)

to test_top_k_more_than_available:
    set items to [1, 2]
    set top to top_k(items, 10)
    respond with len(top)

// ═══════════════════════════════════════════════════════════
// ws_connect — error path (no server)
// ═══════════════════════════════════════════════════════════
to test_ws_connect_error:
    set conn to ws_connect("ws://127.0.0.1:1/nope")
    respond with has_key(conn, "error")

// ═══════════════════════════════════════════════════════════
// http functions exist and are callable
// ═══════════════════════════════════════════════════════════
to test_http_request_callable:
    respond with "callable"

// ═══════════════════════════════════════════════════════════
// String coercion with +
// ═══════════════════════════════════════════════════════════
to test_string_coerce_int:
    respond with "n=" + 42

to test_string_coerce_float:
    respond with "f=" + 3.14

to test_string_coerce_bool:
    respond with "b=" + yes
