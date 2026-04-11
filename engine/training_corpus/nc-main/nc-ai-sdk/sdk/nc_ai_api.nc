// nc_ai_api.nc — PUBLIC API for NC AI
//
// This file documents the NC AI functions available to users.
// The implementation is shipped as a pre-compiled binary model
// and C engine inside the NC Lang binary.
//
// Usage:
//   nc run chat.nc          — Interactive AI chat
//   nc ai "your prompt"     — Quick AI generation
//   nc ai train data/       — Train on your data
//
// Copyright (c) 2025–2026 Nuckala Sai Narender
// Licensed under Apache 2.0 (this file only)

// ── Chat / Generation ─────────────────────────────────────

// to ai_generate with prompt, options:
//     Generates text/code/plan from a prompt.
//     Options: { task, max_tokens, temperature, model }
//     Returns: { text, confidence, tokens }

// to ai_complete with code_prefix, language:
//     Code completion from a prefix.
//     Returns: { completion, confidence }

// to ai_explain with code, language:
//     Explains code in plain English.
//     Returns: { explanation }

// to ai_fix with buggy_code, error_message:
//     Fixes buggy code given an error message.
//     Returns: { fixed_code, explanation }

// ── Training ──────────────────────────────────────────────

// to ai_train with data_dir, config:
//     Train/fine-tune the NC AI model on custom data.
//     Config: { epochs, learning_rate, batch_size }
//     Returns: { status, loss, accuracy }

// to ai_train_example with prompt, target, task:
//     Train on a single example (online learning).
//     Returns: { loss, accuracy }

// ── Reasoning ─────────────────────────────────────────────

// to ai_reason with question, context:
//     Multi-step reasoning with chain-of-thought.
//     Returns: { answer, reasoning_steps[], confidence }

// to ai_plan with goal, constraints:
//     Generate a structured plan.
//     Returns: { steps[], estimated_effort }

// ── Agent / Swarm ─────────────────────────────────────────

// to ai_agent with task, tools:
//     Create an autonomous AI agent.
//     Returns: { result, actions_taken[], tokens_used }

// to ai_swarm with task, num_agents, strategy:
//     Multi-agent swarm execution.
//     Strategy: "ant_colony" | "bee_colony" | "consensus"
//     Returns: { result, agent_results[], consensus_score }

// ── Embeddings ────────────────────────────────────────────

// to ai_encode with text:
//     Encode text to a vector embedding.
//     Returns: list of floats

// to ai_similarity with text_a, text_b:
//     Compute semantic similarity between two texts.
//     Returns: float (0.0–1.0)
