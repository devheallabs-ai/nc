/*
 * nc_ai_enterprise.h — Enterprise AI Engine Layer for NC.
 *
 * Production-grade patterns for the NC AI subsystem:
 *   - Environment-driven configuration
 *   - Model scale presets
 *   - Circuit breaker (thread-safe)
 *   - LRU cache with TTL (thread-safe)
 *   - Sliding-window rate limiter (thread-safe)
 *   - Health and readiness checks
 *   - Structured JSON logging
 *   - Request tracing with correlation IDs
 *
 * Copyright 2026 DevHeal Labs AI. All rights reserved.
 */

#ifndef NC_AI_ENTERPRISE_H
#define NC_AI_ENTERPRISE_H

#include "../include/nc_platform.h"

/* ═══════════════════════════════════════════════════════════
 *  Log Levels
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_AI_LOG_DEBUG = 0,
    NC_AI_LOG_INFO  = 1,
    NC_AI_LOG_WARN  = 2,
    NC_AI_LOG_ERROR = 3
} NCAILogLevel;

/* ═══════════════════════════════════════════════════════════
 *  Environment-Driven Configuration
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char env_name[32];           /* e.g. "production", "staging", "dev" */
    int  log_level;              /* NCAILogLevel value */
    bool enable_circuit_breaker;
    bool enable_rate_limiter;
    bool enable_cache;
    bool enable_tracing;
    bool enable_health_checks;
} NCAIEnvConfig;

/* ═══════════════════════════════════════════════════════════
 *  Model Scale Presets
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_SCALE_SMALL  = 0,
    NC_SCALE_MEDIUM = 1,
    NC_SCALE_LARGE  = 2,
    NC_SCALE_XL     = 3
} NCAIModelScale;

typedef struct {
    int         dim;
    int         n_layers;
    int         n_heads;
    int         vocab_size;
    int         max_seq;
    int         hidden_dim;
    const char *name;
    long        estimated_params;
} NCAIScalePreset;

/* ═══════════════════════════════════════════════════════════
 *  Circuit Breaker
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_CB_CLOSED    = 0,   /* Normal operation */
    NC_CB_OPEN      = 1,   /* Blocking requests */
    NC_CB_HALF_OPEN = 2    /* Testing recovery */
} NCAICircuitState;

typedef struct {
    int failure_threshold;   /* consecutive failures before opening (default 5)  */
    int cooldown_sec;        /* seconds to wait in OPEN before half-open (default 30) */
    int success_threshold;   /* successes in HALF_OPEN to close (default 2) */
} NCAICircuitBreakerConfig;

typedef struct {
    NCAICircuitState        state;
    NCAICircuitBreakerConfig config;
    int                     failure_count;
    int                     success_count;
    double                  last_failure_time;   /* monotonic ms */
    nc_mutex_t              lock;
} NCAICircuitBreaker;

/* ═══════════════════════════════════════════════════════════
 *  LRU Cache (hash table + doubly-linked list, TTL-based)
 * ═══════════════════════════════════════════════════════════ */

typedef struct NCAICacheEntry {
    char                   *key;
    char                   *value;
    double                  expires_at;   /* monotonic ms; 0 = no expiry */
    struct NCAICacheEntry  *prev;
    struct NCAICacheEntry  *next;
    struct NCAICacheEntry  *hash_next;    /* chaining within bucket */
} NCAICacheEntry;

typedef struct {
    int              hits;
    int              misses;
    int              evictions;
    int              current_size;
} NCAICacheStats;

typedef struct {
    NCAICacheEntry **buckets;
    int              bucket_count;
    NCAICacheEntry  *head;         /* most recently used */
    NCAICacheEntry  *tail;         /* least recently used */
    int              size;
    int              max_entries;
    double           ttl_ms;       /* default TTL in ms; 0 = no expiry */
    NCAICacheStats   stats;
    nc_mutex_t       lock;
} NCAICache;

/* ═══════════════════════════════════════════════════════════
 *  Rate Limiter (sliding window, circular buffer)
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    double *timestamps;     /* circular buffer of request timestamps (monotonic ms) */
    int     capacity;       /* max requests per window */
    int     head;           /* write position */
    int     count;          /* current number of timestamps */
    double  window_ms;      /* sliding window size in ms */
    nc_mutex_t lock;
} NCAIRateLimiter;

/* ═══════════════════════════════════════════════════════════
 *  Health Status
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    bool              model_loaded;
    bool              cache_operational;
    bool              rate_limiter_ok;
    NCAICircuitState  circuit_state;
    long              total_requests;
    long              cache_hits;
    long              cache_misses;
    double            uptime_sec;
    double            avg_latency_ms;
} NCAIHealthStatus;

/* ═══════════════════════════════════════════════════════════
 *  Request Tracing
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char   correlation_id[64];
    double start_time;     /* monotonic ms */
    double end_time;       /* monotonic ms; 0 if still running */
    char   operation[64];
    bool   success;
} NCAITrace;

/* ═══════════════════════════════════════════════════════════
 *  Function Prototypes
 * ═══════════════════════════════════════════════════════════ */

/* --- Configuration ------------------------------------------------ */
NCAIEnvConfig   nc_ai_env_config_load(void);
NCAIScalePreset nc_ai_get_scale_preset(NCAIModelScale scale);

/* --- Circuit Breaker ---------------------------------------------- */
NCAICircuitBreaker *nc_ai_cb_create(const NCAICircuitBreakerConfig *cfg);
bool                nc_ai_cb_allow_request(NCAICircuitBreaker *cb);
void                nc_ai_cb_record_success(NCAICircuitBreaker *cb);
void                nc_ai_cb_record_failure(NCAICircuitBreaker *cb);
void                nc_ai_cb_free(NCAICircuitBreaker *cb);

/* --- LRU Cache ---------------------------------------------------- */
NCAICache *nc_ai_cache_create(int max_entries, double ttl_ms);
char      *nc_ai_cache_get(NCAICache *cache, const char *key);
void       nc_ai_cache_put(NCAICache *cache, const char *key, const char *value);
void       nc_ai_cache_free(NCAICache *cache);
NCAICacheStats nc_ai_cache_stats(NCAICache *cache);

/* --- Rate Limiter ------------------------------------------------- */
NCAIRateLimiter *nc_ai_rl_create(int max_requests, double window_ms);
bool             nc_ai_rl_allow(NCAIRateLimiter *rl);
void             nc_ai_rl_free(NCAIRateLimiter *rl);

/* --- Health ------------------------------------------------------- */
NCAIHealthStatus nc_ai_health_check(void);
bool             nc_ai_readiness_check(void);

/* --- Logging ------------------------------------------------------ */
void nc_ai_log(NCAILogLevel level, const char *component,
               const char *correlation_id, const char *fmt, ...);

/* --- Tracing ------------------------------------------------------ */
NCAITrace nc_ai_trace_start(const char *operation, const char *correlation_id);
void      nc_ai_trace_end(NCAITrace *trace, bool success);
void      nc_ai_trace_generate_id(char *buf, size_t buf_size);

/* --- Lifecycle ---------------------------------------------------- */
int  nc_ai_enterprise_init(void);
void nc_ai_enterprise_shutdown(void);

#endif /* NC_AI_ENTERPRISE_H */
