// ═══════════════════════════════════════════════════════════
//  RAG Knowledge Base (Retrieval Augmented Generation)
//
//  Replaces: 80+ lines of Python with RAG framework + vector store
//
//  Upload documents, then ask questions about them.
//
//  curl -X POST http://localhost:8000/ask \
//    -d '{"question": "What is our refund policy?", "knowledge_base": "company_docs"}'
// ═══════════════════════════════════════════════════════════

service "rag-knowledge-base"
version "1.0.0"

configure:
    ai_model is "openai/gpt-4o-mini"

to ask_document with question, file_path:
    purpose: "Answer a question from a single document"

    set document to read_file(file_path)

    if type(document) is equal "none":
        respond with {"error": "File not found", "path": file_path}

    set doc_chunks to chunk(document, 1000)
    set relevant to top_k(doc_chunks, question, 5)

    ask AI to "Based on these document excerpts, answer the question accurately. If the answer is not in the documents, say so.\n\nQuestion: {{question}}\n\nDocument excerpts:\n{{relevant}}" save as answer

    respond with {"answer": answer, "source": file_path, "chunks_searched": len(doc_chunks)}

to ask_multiple_docs with question, file_paths:
    purpose: "Answer from multiple documents — like a knowledge base"

    set all_text to ""
    set sources to []

    repeat for each path in file_paths:
        set content to read_file(path)
        if type(content) is not equal "none":
            set all_text to all_text + "\n---\nSource: " + path + "\n" + content
            append path to sources

    set doc_chunks to chunk(all_text, 1000)
    set relevant to top_k(doc_chunks, question, 5)

    ask AI to "Answer this question using ONLY the provided documents. Cite which source document the answer comes from.\n\nQuestion: {{question}}\n\nDocuments:\n{{relevant}}" save as answer

    respond with {"answer": answer, "sources": sources, "total_chunks": len(doc_chunks)}

to summarize_document with file_path:
    purpose: "Generate a summary of a document"

    set document to read_file(file_path)
    ask AI to "Summarize this document in 3-5 bullet points:\n\n{{document}}" save as summary

    respond with {"summary": summary, "source": file_path, "word_count": len(split(document, " "))}

to compare_documents with file_path_1, file_path_2:
    purpose: "Compare two documents and find differences"

    set doc1 to read_file(file_path_1)
    set doc2 to read_file(file_path_2)

    ask AI to "Compare these two documents. Return JSON with: similarities (list), differences (list), recommendation (string).\n\nDocument 1:\n{{doc1}}\n\nDocument 2:\n{{doc2}}" save as comparison

    respond with comparison

api:
    POST /ask runs ask_document
    POST /ask/multi runs ask_multiple_docs
    POST /summarize runs summarize_document
    POST /compare runs compare_documents
