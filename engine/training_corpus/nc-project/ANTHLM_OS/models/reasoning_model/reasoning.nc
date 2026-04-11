// ══════════════════════════════════════════════════════════════════
//  HiveANT — Reasoning Model Layer
//
//  Local AI model for generating investigation hypotheses.
//  Abstracts AI calls so models can be swapped (Ollama, vLLM, etc.)
// ══════════════════════════════════════════════════════════════════

to reason_about with context, question:
    purpose: "Use reasoning model to generate hypotheses"
    ask AI to "You are a reasoning engine. Given the context, answer the question with structured analysis. CONTEXT: {{context}}. QUESTION: {{question}}. Return ONLY valid JSON: {\"hypotheses\": [{\"hypothesis\": \"string\", \"confidence\": 0.0, \"evidence\": [\"string\"], \"tests\": [\"string\"]}], \"reasoning_chain\": [\"string\"], \"most_likely\": \"string\"}" save as reasoning
    respond with reasoning

to reason_differential with symptoms, candidates:
    purpose: "Differential diagnosis — rank candidate causes"
    ask AI to "Perform differential diagnosis. SYMPTOMS: {{symptoms}}. CANDIDATE CAUSES: {{candidates}}. For each candidate, score how well it explains the symptoms. Return ONLY valid JSON: {\"differential\": [{\"cause\": \"string\", \"score\": 0.0, \"explains\": [\"string\"], \"does_not_explain\": [\"string\"]}], \"recommended_tests\": [{\"test\": \"string\", \"distinguishes\": [\"string\"]}]}" save as diff
    respond with diff
