/*
 * nc_ai_router.c — Local-Only Router with Offline Distillation + Feedback Learning
 *
 * Feature 1: 100% local AI code generation — No External Dependencies.
 * Feature 2: User feedback loop for self-improving code generation.
 * Feature 3: Enterprise patterns — circuit breaker, cache, rate limiting.
 * Feature 4: Offline distillation for training data accumulation.
 *
 * Inspired by T-P2AI Brain architecture, adapted for the NC engine.
 *
 * Copyright 2026 DevHeal Labs AI. All rights reserved.
 */

#include "nc_ai_router.h"
#include "nc_ai_enterprise.h"
#include "nc_terminal_ui.h"
#include "nc_generate.h"
#include "../include/nc_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════
 *  Enterprise singletons (lazily initialized)
 * ═══════════════════════════════════════════════════════════ */

static NCAICircuitBreaker *g_circuit      = NULL;
static NCAICache          *g_cache        = NULL;
static NCAIRateLimiter    *g_rate_limiter = NULL;
static bool                g_enterprise_init = false;

static void router_enterprise_init(void) {
    if (g_enterprise_init) return;

    NCAICircuitBreakerConfig cb_cfg;
    cb_cfg.failure_threshold = 5;
    cb_cfg.cooldown_sec      = 30;
    cb_cfg.success_threshold = 2;
    g_circuit = nc_ai_cb_create(&cb_cfg);

    /* 256 entries, 5-minute TTL */
    g_cache = nc_ai_cache_create(256, 300000.0);

    /* 60 requests per 60-second window */
    g_rate_limiter = nc_ai_rl_create(60, 60000.0);

    g_enterprise_init = true;

    nc_ai_log(NC_AI_LOG_INFO, "router", NULL,
              "enterprise patterns initialized (cb + cache + rate-limiter)");
}

/* ═══════════════════════════════════════════════════════════
 *  Internal helpers
 * ═══════════════════════════════════════════════════════════ */

/* Portable millisecond timer */
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Simple running-average update */
static void update_avg(double *avg, double sample, int n) {
    if (n <= 1) {
        *avg = sample;
    } else {
        *avg = *avg + (sample - *avg) / n;
    }
}

/* Simple DJB2 hash for cache keys */
static unsigned long hash_prompt(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/* ═══════════════════════════════════════════════════════════
 *  Confidence heuristic
 *
 *  Scores local output on how likely it is valid NC code.
 *  Each signal adds 0.1; base is 0.3.  Capped at 1.0.
 * ═══════════════════════════════════════════════════════════ */

static float compute_confidence(const char *output) {
    if (!output || output[0] == '\0') return 0.0f;

    float conf = 0.3f;
    size_t len = strlen(output);

    if (len > 50)  conf += 0.1f;
    if (len > 200) conf += 0.1f;
    if (strstr(output, "to "))          conf += 0.1f;  /* NC function def  */
    if (strstr(output, "set ") ||
        strstr(output, "show "))        conf += 0.1f;  /* NC keywords      */
    if (strstr(output, "respond with") ||
        strstr(output, "api:"))         conf += 0.1f;  /* NC API patterns  */

    return conf > 1.0f ? 1.0f : conf;
}

/* ═══════════════════════════════════════════════════════════
 *  Offline distillation: save prompt+response pairs
 *
 *  These are accumulated from user feedback (positive examples)
 *  and can be used for offline fine-tuning via:
 *    nc ai train --distill
 *
 *  NO runtime LLM calls — this is purely a training tool.
 * ═══════════════════════════════════════════════════════════ */

static void auto_distill_save(const char *path, const char *prompt,
                               const char *response) {
    if (!path || path[0] == '\0') return;

    FILE *f = fopen(path, "a");
    if (!f) {
        fprintf(stderr, "[nc_router] warning: could not open distill file: %s\n", path);
        return;
    }

    /* Write prompt (replace newlines with \\n for single-line storage) */
    for (const char *p = prompt; *p; p++) {
        if (*p == '\n') fprintf(f, "\\n");
        else if (*p == '\t') fprintf(f, "\\t");
        else fputc(*p, f);
    }

    fputc('\t', f);

    /* Write response */
    for (const char *p = response; *p; p++) {
        if (*p == '\n') fprintf(f, "\\n");
        else if (*p == '\t') fprintf(f, "\\t");
        else fputc(*p, f);
    }

    fputc('\n', f);
    fclose(f);
}

/* ═══════════════════════════════════════════════════════════
 *  Local-Only Router API — No External Dependencies
 * ═══════════════════════════════════════════════════════════ */

NCRouterConfig nc_router_default_config(void) {
    NCRouterConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.confidence_threshold  = 0.5f;
    cfg.auto_distill          = true;
    cfg.max_local_tokens      = 512;
    cfg.local_temperature     = 0.7f;
    cfg.enable_cache          = true;
    cfg.enable_circuit_breaker = true;
    snprintf(cfg.distill_path, sizeof(cfg.distill_path),
             "%s/.nc/training_data/auto_distilled.txt",
             getenv("HOME") ? getenv("HOME") : ".");
    return cfg;
}

NCRouterResult nc_router_generate(NCRouterConfig *cfg, NCRouterStats *stats,
                                   const char *prompt) {
    NCRouterResult result;
    memset(&result, 0, sizeof(result));

    if (!prompt || prompt[0] == '\0') {
        result.output     = NULL;
        result.confidence = 0.0f;
        result.from_local = true;
        result.latency_ms = 0.0;
        return result;
    }

    /* ── Lazy-init enterprise singletons ───────────────── */
    router_enterprise_init();

    stats->total_queries++;

    /* ── Step 0: Rate limiting ─────────────────────────── */
    if (g_rate_limiter && !nc_ai_rl_allow(g_rate_limiter)) {
        nc_ai_log(NC_AI_LOG_WARN, "router", NULL,
                  "rate limited — rejecting query");
        stats->rate_limited++;
        result.output     = strdup("[nc_router] rate limited — try again shortly");
        result.confidence = 0.0f;
        result.from_local = true;
        result.latency_ms = 0.0;
        return result;
    }

    /* ── Step 0b: Cache check ──────────────────────────── */
    if (cfg->enable_cache && g_cache) {
        char cache_key[32];
        snprintf(cache_key, sizeof(cache_key), "%lu", hash_prompt(prompt));

        char *cached = nc_ai_cache_get(g_cache, cache_key);
        if (cached) {
            nc_ai_log(NC_AI_LOG_DEBUG, "router", NULL,
                      "cache hit for prompt hash %s", cache_key);
            stats->cache_hits++;
            result.output     = cached;  /* nc_ai_cache_get returns malloc'd copy */
            result.confidence = 1.0f;
            result.from_local = true;
            result.latency_ms = 0.0;
            return result;
        }
        stats->cache_misses++;
    }

    /* ── Step 1: Try local model ────────────────────────── */
    double t0 = now_ms();
    char *local_output = nc_generate_code(NULL, prompt);
    double local_ms = now_ms() - t0;

    float confidence = compute_confidence(local_output);

    /* ── Step 2: Score and return ────────────────────────── */
    stats->local_hits++;
    update_avg(&stats->avg_local_latency_ms, local_ms, stats->local_hits);
    stats->estimated_cost_saved += 0.002;  /* ~$0.002 per query (no LLM cost) */

    result.output     = local_output;
    result.confidence = confidence;
    result.from_local = true;
    result.latency_ms = local_ms;

    if (confidence >= cfg->confidence_threshold) {
        /* Confident result — cache it */
        if (cfg->enable_cache && g_cache && local_output) {
            char cache_key[32];
            snprintf(cache_key, sizeof(cache_key), "%lu", hash_prompt(prompt));
            nc_ai_cache_put(g_cache, cache_key, local_output);
        }

        /* Auto-distill confident local results for offline training */
        if (cfg->auto_distill && local_output) {
            auto_distill_save(cfg->distill_path, prompt, local_output);
            stats->distilled++;
        }
    } else {
        /* Low confidence — still return local result (no fallback!) */
        nc_ai_log(NC_AI_LOG_WARN, "router", NULL,
                  "low confidence (%.2f < %.2f) — no external fallback available, "
                  "returning local result. Consider retraining: nc ai train --distill",
                  confidence, cfg->confidence_threshold);
    }

    return result;
}

void nc_router_result_free(NCRouterResult *r) {
    if (r && r->output) {
        free(r->output);
        r->output = NULL;
    }
}

void nc_router_stats_print(NCRouterStats *stats) {
    if (!stats) return;

    int total = stats->total_queries;
    double local_pct = total > 0 ? (stats->local_hits * 100.0 / total) : 0.0;

    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║    NC AI Local-Only Router Statistics        ║\n");
    printf("║    No External Dependencies                  ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Total queries:      %6d                  ║\n", total);
    printf("║  Local hits:         %6d  (%5.1f%%)         ║\n", stats->local_hits, local_pct);
    printf("║  Reasoning queries:  %6d                  ║\n", stats->reasoning_queries);
    printf("║  Distilled (offline):%6d                  ║\n", stats->distilled);
    printf("║  Cache hits:         %6d                  ║\n", stats->cache_hits);
    printf("║  Cache misses:       %6d                  ║\n", stats->cache_misses);
    printf("║  Rate limited:       %6d                  ║\n", stats->rate_limited);
    printf("║  Circuit opens:      %6d                  ║\n", stats->circuit_opens);
    printf("║  Avg local latency:  %6.1f ms               ║\n", stats->avg_local_latency_ms);
    printf("║  Est. cost saved:    $%5.2f                 ║\n", stats->estimated_cost_saved);
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════
 *  Feedback Learning API
 * ═══════════════════════════════════════════════════════════ */

#define FEEDBACK_INITIAL_CAP 64

NCFeedbackStore *nc_feedback_create(const char *path) {
    NCFeedbackStore *store = (NCFeedbackStore *)calloc(1, sizeof(NCFeedbackStore));
    if (!store) return NULL;

    store->records  = (NCFeedback *)malloc(FEEDBACK_INITIAL_CAP * sizeof(NCFeedback));
    if (!store->records) { free(store); return NULL; }

    store->n_records      = 0;
    store->capacity       = FEEDBACK_INITIAL_CAP;
    store->positive_count = 0;
    store->negative_count = 0;

    if (path) {
        snprintf(store->feedback_path, sizeof(store->feedback_path), "%s", path);
    } else {
        snprintf(store->feedback_path, sizeof(store->feedback_path),
                 "%s/.nc/feedback/feedback_log.txt",
                 getenv("HOME") ? getenv("HOME") : ".");
    }

    return store;
}

void nc_feedback_free(NCFeedbackStore *store) {
    if (!store) return;
    free(store->records);
    free(store);
}

void nc_feedback_record(NCFeedbackStore *store, const char *prompt,
                        const char *output, bool positive) {
    if (!store || !prompt || !output) return;

    /* Grow array 2x when full */
    if (store->n_records >= store->capacity) {
        int new_cap = store->capacity * 2;
        NCFeedback *tmp = (NCFeedback *)realloc(store->records,
                                                  new_cap * sizeof(NCFeedback));
        if (!tmp) {
            fprintf(stderr, "[nc_feedback] warning: could not grow feedback store\n");
            return;
        }
        store->records  = tmp;
        store->capacity = new_cap;
    }

    NCFeedback *rec = &store->records[store->n_records];
    memset(rec, 0, sizeof(NCFeedback));

    snprintf(rec->prompt, sizeof(rec->prompt), "%s", prompt);
    snprintf(rec->output, sizeof(rec->output), "%s", output);
    rec->positive = positive;

    /* Weight delta: positive examples reinforce (+0.1), negative weaken (-0.05) */
    rec->weight_delta = positive ? 0.1f : -0.05f;

    store->n_records++;
    if (positive) store->positive_count++;
    else          store->negative_count++;
}

void nc_feedback_apply(NCFeedbackStore *store) {
    if (!store || store->n_records == 0) {
        if (nc_get_log_level() >= NC_LOG_VERBOSE)
            printf("[nc_feedback] No feedback records to apply.\n");
        return;
    }

    /* Build path for the training corpus file */
    char corpus_path[512];
    snprintf(corpus_path, sizeof(corpus_path),
             "%s/.nc/training_data/feedback_corpus.txt",
             getenv("HOME") ? getenv("HOME") : ".");

    /* Build path for negative examples log */
    char negative_path[512];
    snprintf(negative_path, sizeof(negative_path),
             "%s/.nc/training_data/negative_examples.txt",
             getenv("HOME") ? getenv("HOME") : ".");

    int applied_positive = 0;
    int applied_negative = 0;

    FILE *corpus_f   = fopen(corpus_path, "a");
    FILE *negative_f = fopen(negative_path, "a");

    for (int i = 0; i < store->n_records; i++) {
        NCFeedback *rec = &store->records[i];

        if (rec->positive) {
            /* Positive: append to training corpus for reinforcement */
            if (corpus_f) {
                fprintf(corpus_f, "### Prompt: ");
                /* Write prompt, escaping newlines */
                for (const char *p = rec->prompt; *p; p++) {
                    if (*p == '\n') fprintf(corpus_f, "\\n");
                    else fputc(*p, corpus_f);
                }
                fprintf(corpus_f, "\n### Code:\n%s\n### End\n\n", rec->output);
                applied_positive++;
            }
        } else {
            /* Negative: log for analysis, do not add to training data */
            if (negative_f) {
                fprintf(negative_f, "--- NEGATIVE (weight_delta=%.3f) ---\n",
                        rec->weight_delta);
                fprintf(negative_f, "Prompt: %s\n", rec->prompt);
                fprintf(negative_f, "Output: %s\n", rec->output);
                fprintf(negative_f, "---\n\n");
                applied_negative++;
            }
        }
    }

    if (corpus_f)   fclose(corpus_f);
    if (negative_f) fclose(negative_f);

    if (nc_get_log_level() >= NC_LOG_VERBOSE) {
        printf("[nc_feedback] Applied %d positive examples to training corpus.\n",
               applied_positive);
        printf("[nc_feedback] Logged %d negative examples for review.\n",
               applied_negative);
    }

    if (applied_positive > 0 && nc_get_log_level() >= NC_LOG_VERBOSE) {
        printf("[nc_feedback] Corpus updated: %s\n", corpus_path);
        printf("[nc_feedback] Run 'nc ai train' to retrain with new feedback.\n");
    }
}

void nc_feedback_save(NCFeedbackStore *store) {
    if (!store) return;

    FILE *f = fopen(store->feedback_path, "w");
    if (!f) {
        fprintf(stderr, "[nc_feedback] error: could not write %s\n",
                store->feedback_path);
        return;
    }

    for (int i = 0; i < store->n_records; i++) {
        NCFeedback *rec = &store->records[i];

        /* Format: +/-<TAB>prompt<TAB>output */
        fputc(rec->positive ? '+' : '-', f);
        fputc('\t', f);

        /* Write prompt (escape tabs and newlines) */
        for (const char *p = rec->prompt; *p; p++) {
            if (*p == '\n')      fprintf(f, "\\n");
            else if (*p == '\t') fprintf(f, "\\t");
            else                 fputc(*p, f);
        }

        fputc('\t', f);

        /* Write output (escape tabs and newlines) */
        for (const char *p = rec->output; *p; p++) {
            if (*p == '\n')      fprintf(f, "\\n");
            else if (*p == '\t') fprintf(f, "\\t");
            else                 fputc(*p, f);
        }

        fputc('\n', f);
    }

    fclose(f);
    if (nc_get_log_level() >= NC_LOG_VERBOSE)
        printf("[nc_feedback] Saved %d records to %s\n",
               store->n_records, store->feedback_path);
}

void nc_feedback_load(NCFeedbackStore *store) {
    if (!store) return;

    FILE *f = fopen(store->feedback_path, "r");
    if (!f) {
        /* File doesn't exist yet — that's fine for first run */
        return;
    }

    char line[4096];
    int loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len < 3) continue;  /* minimum: "+\t\t" */

        /* Parse: first char is +/- */
        bool positive = (line[0] == '+');

        /* Find first tab (start of prompt) */
        char *tab1 = strchr(line + 1, '\t');
        if (!tab1) continue;
        tab1++; /* skip the tab */

        /* Find second tab (start of output) */
        char *tab2 = strchr(tab1, '\t');
        if (!tab2) continue;
        *tab2 = '\0';  /* terminate prompt */
        tab2++; /* skip the tab */

        /* Unescape and record */
        char prompt[512], output[2048];
        size_t pi = 0, oi = 0;

        /* Unescape prompt */
        for (size_t j = 0; tab1[j] && pi < sizeof(prompt) - 1; j++) {
            if (tab1[j] == '\\' && tab1[j + 1] == 'n') {
                prompt[pi++] = '\n'; j++;
            } else if (tab1[j] == '\\' && tab1[j + 1] == 't') {
                prompt[pi++] = '\t'; j++;
            } else {
                prompt[pi++] = tab1[j];
            }
        }
        prompt[pi] = '\0';

        /* Unescape output */
        for (size_t j = 0; tab2[j] && oi < sizeof(output) - 1; j++) {
            if (tab2[j] == '\\' && tab2[j + 1] == 'n') {
                output[oi++] = '\n'; j++;
            } else if (tab2[j] == '\\' && tab2[j + 1] == 't') {
                output[oi++] = '\t'; j++;
            } else {
                output[oi++] = tab2[j];
            }
        }
        output[oi] = '\0';

        nc_feedback_record(store, prompt, output, positive);
        loaded++;
    }

    fclose(f);
    if (nc_get_log_level() >= NC_LOG_VERBOSE)
        printf("[nc_feedback] Loaded %d records from %s\n",
               loaded, store->feedback_path);
}

void nc_feedback_stats(NCFeedbackStore *store) {
    if (!store) return;

    int total = store->positive_count + store->negative_count;
    double reinf_rate = total > 0
        ? (store->positive_count * 100.0 / total)
        : 0.0;

    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║         NC AI Feedback Statistics            ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  Total feedback:     %6d                  ║\n", total);
    printf("║  Positive (good):    %6d                  ║\n", store->positive_count);
    printf("║  Negative (bad):     %6d                  ║\n", store->negative_count);
    printf("║  Reinforcement rate: %5.1f%%                 ║\n", reinf_rate);
    printf("║  Pending records:    %6d                  ║\n", store->n_records);
    printf("║  Store capacity:     %6d                  ║\n", store->capacity);
    printf("║  Persistence file:                          ║\n");
    printf("║    %s\n", store->feedback_path);
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n");
}
