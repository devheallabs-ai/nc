#ifndef NC_AI_ROUTER_H
#define NC_AI_ROUTER_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  NC AI Local-Only Router — No External Dependencies
 *
 *  100% local AI code generation. No runtime LLM calls.
 *  Distillation is retained as an offline training tool only.
 *
 *  Copyright 2026 DevHeal Labs AI. All rights reserved.
 * ═══════════════════════════════════════════════════════════ */

/* Router configuration (local-only, no external API fields) */
typedef struct {
    float   confidence_threshold;   /* min confidence to accept local (0.0-1.0, default 0.5) */
    bool    auto_distill;           /* save prompt+response pairs for offline training */
    char    distill_path[256];      /* path to save distilled pairs */
    int     max_local_tokens;       /* max tokens for local generation */
    float   local_temperature;      /* temperature for local model */
    bool    enable_cache;           /* response caching (default true) */
    bool    enable_circuit_breaker; /* circuit breaker (default true) */
} NCRouterConfig;

/* Router statistics (local-only, no LLM metrics) */
typedef struct {
    int     total_queries;
    int     local_hits;             /* answered locally */
    int     reasoning_queries;      /* reserved for future reasoning engine */
    int     distilled;              /* pairs saved for offline training */
    double  avg_local_latency_ms;
    double  estimated_cost_saved;   /* $ saved by being fully local */
    int     cache_hits;
    int     cache_misses;
    int     rate_limited;
    int     circuit_opens;
} NCRouterStats;

/* Router result */
typedef struct {
    char   *output;                 /* generated text (caller must free) */
    float   confidence;             /* 0.0-1.0 */
    bool    from_local;             /* always true — fully local */
    double  latency_ms;
} NCRouterResult;

/* Feedback record */
typedef struct {
    char    prompt[512];
    char    output[2048];
    bool    positive;               /* true = good, false = bad */
    float   weight_delta;           /* how much to adjust (auto-computed) */
} NCFeedback;

/* Feedback store */
typedef struct {
    NCFeedback *records;
    int         n_records;
    int         capacity;
    int         positive_count;
    int         negative_count;
    char        feedback_path[256]; /* persistence path */
} NCFeedbackStore;

/* === Local-Only Router API === */
NCRouterConfig nc_router_default_config(void);
NCRouterResult nc_router_generate(NCRouterConfig *cfg, NCRouterStats *stats,
                                   const char *prompt);
void nc_router_result_free(NCRouterResult *r);
void nc_router_stats_print(NCRouterStats *stats);

/* === Feedback Learning API === */
NCFeedbackStore *nc_feedback_create(const char *path);
void nc_feedback_free(NCFeedbackStore *store);
void nc_feedback_record(NCFeedbackStore *store, const char *prompt,
                        const char *output, bool positive);
void nc_feedback_apply(NCFeedbackStore *store);  /* apply accumulated feedback */
void nc_feedback_save(NCFeedbackStore *store);
void nc_feedback_load(NCFeedbackStore *store);
void nc_feedback_stats(NCFeedbackStore *store);

#endif /* NC_AI_ROUTER_H */
