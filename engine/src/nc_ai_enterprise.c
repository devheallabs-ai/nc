/*
 * nc_ai_enterprise.c — Enterprise AI Engine Layer for NC.
 *
 * Production-grade patterns for the NC AI subsystem:
 *   - Environment-driven configuration
 *   - Model scale presets
 *   - Circuit breaker (thread-safe)
 *   - LRU cache with TTL (thread-safe)
 *   - Sliding-window rate limiter (thread-safe)
 *   - Health and readiness checks
 *   - Structured JSON logging to stderr
 *   - Request tracing with correlation IDs
 *
 * Copyright 2026 DevHeal Labs AI. All rights reserved.
 */

#include "nc_ai_enterprise.h"
#include <inttypes.h>
#include "../include/nc_platform.h"

/* ═══════════════════════════════════════════════════════════
 *  Singleton Enterprise Context
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    bool               initialized;
    NCAIEnvConfig      config;
    NCAICircuitBreaker *circuit_breaker;
    NCAICache          *cache;
    NCAIRateLimiter    *rate_limiter;
    double             start_time;       /* monotonic ms */
    long               total_requests;
    double             total_latency_ms;
    nc_mutex_t         stats_lock;
} NCAIEnterpriseCtx;

static NCAIEnterpriseCtx g_enterprise;

/* ═══════════════════════════════════════════════════════════
 *  FNV-1a Hash (32-bit)
 * ═══════════════════════════════════════════════════════════ */

static uint32_t fnv1a_hash(const char *key) {
    uint32_t h = 2166136261u;
    for (const char *p = key; *p; p++) {
        h ^= (uint32_t)(unsigned char)*p;
        h *= 16777619u;
    }
    return h;
}

/* ═══════════════════════════════════════════════════════════
 *  xorshift64 for trace ID generation
 * ═══════════════════════════════════════════════════════════ */

static nc_thread_local uint64_t s_xor_state = 0;

static uint64_t xorshift64(void) {
    if (s_xor_state == 0) {
        s_xor_state = (uint64_t)nc_clock_ms() ^ (uint64_t)0xDEADBEEFCAFE;
    }
    uint64_t x = s_xor_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    s_xor_state = x;
    return x;
}

/* ═══════════════════════════════════════════════════════════
 *  Log Level Names
 * ═══════════════════════════════════════════════════════════ */

static const char *log_level_str(NCAILogLevel level) {
    switch (level) {
        case NC_AI_LOG_DEBUG: return "DEBUG";
        case NC_AI_LOG_INFO:  return "INFO";
        case NC_AI_LOG_WARN:  return "WARN";
        case NC_AI_LOG_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Structured JSON Logging
 * ═══════════════════════════════════════════════════════════ */

void nc_ai_log(NCAILogLevel level, const char *component,
               const char *correlation_id, const char *fmt, ...) {
    if ((int)level < g_enterprise.config.log_level) return;

    /* Format the user message */
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* Escape quotes in the message for valid JSON */
    char escaped[2048];
    int j = 0;
    for (int i = 0; msg[i] && j < (int)sizeof(escaped) - 2; i++) {
        if (msg[i] == '"' || msg[i] == '\\') {
            escaped[j++] = '\\';
        }
        if (msg[i] == '\n') {
            escaped[j++] = '\\';
            escaped[j++] = 'n';
            continue;
        }
        escaped[j++] = msg[i];
    }
    escaped[j] = '\0';

    /* ISO-8601 timestamp */
    time_t now = time(NULL);
    struct tm tm_buf;
#ifdef NC_WINDOWS
    gmtime_s(&tm_buf, &now);
#else
    gmtime_r(&now, &tm_buf);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);

    /* Emit JSON to stderr */
    const char *cid = (correlation_id && correlation_id[0]) ? correlation_id : "";
    const char *comp = (component && component[0]) ? component : "nc-ai";

    fprintf(stderr,
            "{\"ts\":\"%s\",\"level\":\"%s\",\"component\":\"%s\","
            "\"correlation_id\":\"%s\",\"msg\":\"%s\"}\n",
            ts, log_level_str(level), comp, cid, escaped);
}

/* ═══════════════════════════════════════════════════════════
 *  Environment-Driven Configuration
 * ═══════════════════════════════════════════════════════════ */

static bool env_bool(const char *name, bool fallback) {
    const char *v = getenv(name);
    if (!v) return fallback;
    return (v[0] == '1' || v[0] == 't' || v[0] == 'T' ||
            v[0] == 'y' || v[0] == 'Y');
}

static int env_int(const char *name, int fallback) {
    const char *v = getenv(name);
    if (!v || !v[0]) return fallback;
    return atoi(v);
}

NCAIEnvConfig nc_ai_env_config_load(void) {
    NCAIEnvConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    const char *env = getenv("NC_AI_ENV");
    if (env && env[0]) {
        strncpy(cfg.env_name, env, sizeof(cfg.env_name) - 1);
    } else {
        strncpy(cfg.env_name, "production", sizeof(cfg.env_name) - 1);
    }
    cfg.env_name[sizeof(cfg.env_name) - 1] = '\0';

    cfg.log_level              = env_int("NC_AI_LOG_LEVEL", NC_AI_LOG_INFO);
    cfg.enable_circuit_breaker = env_bool("NC_AI_CIRCUIT_BREAKER", true);
    cfg.enable_rate_limiter    = env_bool("NC_AI_RATE_LIMITER", true);
    cfg.enable_cache           = env_bool("NC_AI_CACHE", true);
    cfg.enable_tracing         = env_bool("NC_AI_TRACING", true);
    cfg.enable_health_checks   = env_bool("NC_AI_HEALTH_CHECKS", true);

    return cfg;
}

/* ═══════════════════════════════════════════════════════════
 *  Model Scale Presets
 * ═══════════════════════════════════════════════════════════ */

static const NCAIScalePreset s_presets[] = {
    { 512,  8,  8,  16384, 1024, 2048, "small",  128000000L  },
    { 1024, 16, 16, 32768, 2048, 4096, "medium", 350000000L  },
    { 1536, 20, 24, 32768, 4096, 6144, "large",  1300000000L },
    { 2048, 24, 32, 32768, 4096, 8192, "xl",     2000000000L },
};

NCAIScalePreset nc_ai_get_scale_preset(NCAIModelScale scale) {
    if (scale < NC_SCALE_SMALL || scale > NC_SCALE_XL) {
        return s_presets[NC_SCALE_MEDIUM];
    }
    return s_presets[scale];
}

/* ═══════════════════════════════════════════════════════════
 *  Circuit Breaker
 * ═══════════════════════════════════════════════════════════ */

NCAICircuitBreaker *nc_ai_cb_create(const NCAICircuitBreakerConfig *cfg) {
    NCAICircuitBreaker *cb = calloc(1, sizeof(NCAICircuitBreaker));
    if (!cb) return NULL;

    if (cfg) {
        cb->config = *cfg;
    } else {
        cb->config.failure_threshold = 5;
        cb->config.cooldown_sec      = 30;
        cb->config.success_threshold = 2;
    }

    cb->state         = NC_CB_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->last_failure_time = 0.0;
    nc_mutex_init(&cb->lock);

    nc_ai_log(NC_AI_LOG_INFO, "circuit-breaker", "",
              "created (threshold=%d, cooldown=%ds, recovery=%d)",
              cb->config.failure_threshold,
              cb->config.cooldown_sec,
              cb->config.success_threshold);
    return cb;
}

bool nc_ai_cb_allow_request(NCAICircuitBreaker *cb) {
    if (!cb) return true;

    nc_mutex_lock(&cb->lock);

    double now = nc_clock_ms();

    switch (cb->state) {
    case NC_CB_CLOSED:
        nc_mutex_unlock(&cb->lock);
        return true;

    case NC_CB_OPEN: {
        double elapsed = (now - cb->last_failure_time) / 1000.0;
        if (elapsed >= (double)cb->config.cooldown_sec) {
            cb->state = NC_CB_HALF_OPEN;
            cb->success_count = 0;
            nc_ai_log(NC_AI_LOG_INFO, "circuit-breaker", "",
                      "transitioning OPEN -> HALF_OPEN after %.1fs cooldown",
                      elapsed);
            nc_mutex_unlock(&cb->lock);
            return true;
        }
        nc_mutex_unlock(&cb->lock);
        return false;
    }

    case NC_CB_HALF_OPEN:
        nc_mutex_unlock(&cb->lock);
        return true;
    }

    nc_mutex_unlock(&cb->lock);
    return false;
}

void nc_ai_cb_record_success(NCAICircuitBreaker *cb) {
    if (!cb) return;

    nc_mutex_lock(&cb->lock);

    cb->failure_count = 0;

    if (cb->state == NC_CB_HALF_OPEN) {
        cb->success_count++;
        if (cb->success_count >= cb->config.success_threshold) {
            cb->state = NC_CB_CLOSED;
            cb->success_count = 0;
            nc_ai_log(NC_AI_LOG_INFO, "circuit-breaker", "",
                      "HALF_OPEN -> CLOSED (recovered)");
        }
    }

    nc_mutex_unlock(&cb->lock);
}

void nc_ai_cb_record_failure(NCAICircuitBreaker *cb) {
    if (!cb) return;

    nc_mutex_lock(&cb->lock);

    cb->failure_count++;
    cb->last_failure_time = nc_clock_ms();

    if (cb->state == NC_CB_HALF_OPEN) {
        cb->state = NC_CB_CLOSED;   /* will re-open below */
        cb->failure_count = cb->config.failure_threshold;
    }

    if (cb->failure_count >= cb->config.failure_threshold) {
        cb->state = NC_CB_OPEN;
        nc_ai_log(NC_AI_LOG_WARN, "circuit-breaker", "",
                  "OPEN — %d consecutive failures", cb->failure_count);
    }

    nc_mutex_unlock(&cb->lock);
}

void nc_ai_cb_free(NCAICircuitBreaker *cb) {
    if (!cb) return;
    nc_mutex_destroy(&cb->lock);
    free(cb);
}

/* ═══════════════════════════════════════════════════════════
 *  LRU Cache — internal helpers
 * ═══════════════════════════════════════════════════════════ */

static void cache_detach(NCAICache *c, NCAICacheEntry *e) {
    if (e->prev) e->prev->next = e->next;
    else         c->head = e->next;
    if (e->next) e->next->prev = e->prev;
    else         c->tail = e->prev;
    e->prev = NULL;
    e->next = NULL;
}

static void cache_push_front(NCAICache *c, NCAICacheEntry *e) {
    e->prev = NULL;
    e->next = c->head;
    if (c->head) c->head->prev = e;
    c->head = e;
    if (!c->tail) c->tail = e;
}

static void cache_entry_free(NCAICacheEntry *e) {
    if (!e) return;
    free(e->key);
    free(e->value);
    free(e);
}

static int cache_bucket(NCAICache *c, const char *key) {
    return (int)(fnv1a_hash(key) % (uint32_t)c->bucket_count);
}

static void cache_remove_from_bucket(NCAICache *c, NCAICacheEntry *e) {
    int b = cache_bucket(c, e->key);
    NCAICacheEntry **pp = &c->buckets[b];
    while (*pp) {
        if (*pp == e) {
            *pp = e->hash_next;
            e->hash_next = NULL;
            return;
        }
        pp = &(*pp)->hash_next;
    }
}

static void cache_evict_tail(NCAICache *c) {
    if (!c->tail) return;
    NCAICacheEntry *victim = c->tail;
    cache_detach(c, victim);
    cache_remove_from_bucket(c, victim);
    cache_entry_free(victim);
    c->size--;
    c->stats.evictions++;
}

/* ═══════════════════════════════════════════════════════════
 *  LRU Cache — public API
 * ═══════════════════════════════════════════════════════════ */

NCAICache *nc_ai_cache_create(int max_entries, double ttl_ms) {
    if (max_entries <= 0) max_entries = 256;

    NCAICache *c = calloc(1, sizeof(NCAICache));
    if (!c) return NULL;

    c->max_entries  = max_entries;
    c->ttl_ms       = ttl_ms;
    c->bucket_count = max_entries * 2;   /* load factor ~0.5 */
    c->buckets      = calloc((size_t)c->bucket_count, sizeof(NCAICacheEntry *));
    if (!c->buckets) { free(c); return NULL; }

    nc_mutex_init(&c->lock);

    nc_ai_log(NC_AI_LOG_INFO, "cache", "",
              "created (max=%d, ttl=%.0fms)", max_entries, ttl_ms);
    return c;
}

char *nc_ai_cache_get(NCAICache *cache, const char *key) {
    if (!cache || !key) return NULL;

    nc_mutex_lock(&cache->lock);

    int b = cache_bucket(cache, key);
    NCAICacheEntry *e = cache->buckets[b];
    double now = nc_clock_ms();

    while (e) {
        if (strcmp(e->key, key) == 0) {
            /* Check TTL */
            if (e->expires_at > 0.0 && now > e->expires_at) {
                /* Expired — remove */
                cache_detach(cache, e);
                cache_remove_from_bucket(cache, e);
                cache_entry_free(e);
                cache->size--;
                cache->stats.misses++;
                nc_mutex_unlock(&cache->lock);
                return NULL;
            }
            /* Move to front (most recently used) */
            cache_detach(cache, e);
            cache_push_front(cache, e);
            cache->stats.hits++;
            char *result = strdup(e->value);
            nc_mutex_unlock(&cache->lock);
            return result;
        }
        e = e->hash_next;
    }

    cache->stats.misses++;
    nc_mutex_unlock(&cache->lock);
    return NULL;
}

void nc_ai_cache_put(NCAICache *cache, const char *key, const char *value) {
    if (!cache || !key || !value) return;

    nc_mutex_lock(&cache->lock);

    /* Check if key already exists — update in place */
    int b = cache_bucket(cache, key);
    NCAICacheEntry *e = cache->buckets[b];
    double now = nc_clock_ms();

    while (e) {
        if (strcmp(e->key, key) == 0) {
            free(e->value);
            e->value = strdup(value);
            e->expires_at = (cache->ttl_ms > 0.0) ? now + cache->ttl_ms : 0.0;
            cache_detach(cache, e);
            cache_push_front(cache, e);
            nc_mutex_unlock(&cache->lock);
            return;
        }
        e = e->hash_next;
    }

    /* Evict if at capacity */
    while (cache->size >= cache->max_entries) {
        cache_evict_tail(cache);
    }

    /* Insert new entry */
    NCAICacheEntry *entry = calloc(1, sizeof(NCAICacheEntry));
    if (!entry) { nc_mutex_unlock(&cache->lock); return; }

    entry->key       = strdup(key);
    entry->value     = strdup(value);
    entry->expires_at = (cache->ttl_ms > 0.0) ? now + cache->ttl_ms : 0.0;

    if (!entry->key || !entry->value) {
        cache_entry_free(entry);
        nc_mutex_unlock(&cache->lock);
        return;
    }

    /* Insert into hash bucket */
    entry->hash_next = cache->buckets[b];
    cache->buckets[b] = entry;

    /* Insert at front of LRU list */
    cache_push_front(cache, entry);
    cache->size++;

    nc_mutex_unlock(&cache->lock);
}

void nc_ai_cache_free(NCAICache *cache) {
    if (!cache) return;

    nc_mutex_lock(&cache->lock);

    NCAICacheEntry *e = cache->head;
    while (e) {
        NCAICacheEntry *next = e->next;
        cache_entry_free(e);
        e = next;
    }

    free(cache->buckets);
    nc_mutex_unlock(&cache->lock);
    nc_mutex_destroy(&cache->lock);
    free(cache);
}

NCAICacheStats nc_ai_cache_stats(NCAICache *cache) {
    NCAICacheStats s = {0};
    if (!cache) return s;

    nc_mutex_lock(&cache->lock);
    s = cache->stats;
    s.current_size = cache->size;
    nc_mutex_unlock(&cache->lock);
    return s;
}

/* ═══════════════════════════════════════════════════════════
 *  Rate Limiter (Sliding Window)
 * ═══════════════════════════════════════════════════════════ */

NCAIRateLimiter *nc_ai_rl_create(int max_requests, double window_ms) {
    if (max_requests <= 0) max_requests = 100;
    if (window_ms <= 0.0) window_ms = 60000.0;   /* 1 minute default */

    NCAIRateLimiter *rl = calloc(1, sizeof(NCAIRateLimiter));
    if (!rl) return NULL;

    rl->timestamps = calloc((size_t)max_requests, sizeof(double));
    if (!rl->timestamps) { free(rl); return NULL; }

    rl->capacity  = max_requests;
    rl->window_ms = window_ms;
    rl->head      = 0;
    rl->count     = 0;
    nc_mutex_init(&rl->lock);

    nc_ai_log(NC_AI_LOG_INFO, "rate-limiter", "",
              "created (max=%d, window=%.0fms)", max_requests, window_ms);
    return rl;
}

bool nc_ai_rl_allow(NCAIRateLimiter *rl) {
    if (!rl) return true;

    nc_mutex_lock(&rl->lock);

    double now = nc_clock_ms();
    double cutoff = now - rl->window_ms;

    /* Expire old timestamps — count active entries in window */
    int active = 0;
    for (int i = 0; i < rl->count; i++) {
        int idx = (rl->head - rl->count + i + rl->capacity) % rl->capacity;
        if (rl->timestamps[idx] >= cutoff) {
            active++;
        }
    }

    if (active >= rl->capacity) {
        nc_mutex_unlock(&rl->lock);
        return false;
    }

    /* Record this request */
    rl->timestamps[rl->head] = now;
    rl->head = (rl->head + 1) % rl->capacity;
    if (rl->count < rl->capacity) {
        rl->count++;
    }

    nc_mutex_unlock(&rl->lock);
    return true;
}

void nc_ai_rl_free(NCAIRateLimiter *rl) {
    if (!rl) return;
    nc_mutex_destroy(&rl->lock);
    free(rl->timestamps);
    free(rl);
}

/* ═══════════════════════════════════════════════════════════
 *  Health and Readiness Checks
 * ═══════════════════════════════════════════════════════════ */

NCAIHealthStatus nc_ai_health_check(void) {
    NCAIHealthStatus h;
    memset(&h, 0, sizeof(h));

    if (!g_enterprise.initialized) return h;

    double now = nc_clock_ms();
    h.uptime_sec = (now - g_enterprise.start_time) / 1000.0;

    /* Cache status */
    if (g_enterprise.cache) {
        h.cache_operational = true;
        NCAICacheStats cs = nc_ai_cache_stats(g_enterprise.cache);
        h.cache_hits   = cs.hits;
        h.cache_misses = cs.misses;
    }

    /* Rate limiter status */
    h.rate_limiter_ok = (g_enterprise.rate_limiter != NULL);

    /* Circuit breaker status */
    if (g_enterprise.circuit_breaker) {
        nc_mutex_lock(&g_enterprise.circuit_breaker->lock);
        h.circuit_state = g_enterprise.circuit_breaker->state;
        nc_mutex_unlock(&g_enterprise.circuit_breaker->lock);
    } else {
        h.circuit_state = NC_CB_CLOSED;
    }

    /* Global stats */
    nc_mutex_lock(&g_enterprise.stats_lock);
    h.total_requests = g_enterprise.total_requests;
    if (g_enterprise.total_requests > 0) {
        h.avg_latency_ms = g_enterprise.total_latency_ms
                           / (double)g_enterprise.total_requests;
    }
    nc_mutex_unlock(&g_enterprise.stats_lock);

    /* model_loaded is set externally; default false here */
    h.model_loaded = false;

    return h;
}

bool nc_ai_readiness_check(void) {
    if (!g_enterprise.initialized) return false;

    NCAIHealthStatus h = nc_ai_health_check();

    /* Ready if circuit is not fully open */
    if (h.circuit_state == NC_CB_OPEN) return false;

    /* Ready if cache is working (when enabled) */
    if (g_enterprise.config.enable_cache && !h.cache_operational) return false;

    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Request Tracing
 * ═══════════════════════════════════════════════════════════ */

void nc_ai_trace_generate_id(char *buf, size_t buf_size) {
    if (!buf || buf_size < 32) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }

    uint64_t ts_ms = (uint64_t)nc_clock_ms();
    uint64_t r1 = xorshift64();
    uint64_t r2 = xorshift64();

    snprintf(buf, buf_size, "%08" PRIx32 "-%04" PRIx32 "-%04" PRIx32
             "-%04" PRIx32 "-%012" PRIx64,
             (uint32_t)(ts_ms & 0xFFFFFFFF),
             (uint32_t)((r1 >> 16) & 0xFFFF),
             (uint32_t)(r1 & 0xFFFF),
             (uint32_t)((r2 >> 16) & 0xFFFF),
             (uint64_t)(r2 & 0xFFFFFFFFFFFFULL));
}

NCAITrace nc_ai_trace_start(const char *operation, const char *correlation_id) {
    NCAITrace t;
    memset(&t, 0, sizeof(t));

    if (correlation_id && correlation_id[0]) {
        strncpy(t.correlation_id, correlation_id, sizeof(t.correlation_id) - 1);
    } else {
        nc_ai_trace_generate_id(t.correlation_id, sizeof(t.correlation_id));
    }
    t.correlation_id[sizeof(t.correlation_id) - 1] = '\0';

    if (operation) {
        strncpy(t.operation, operation, sizeof(t.operation) - 1);
    }
    t.operation[sizeof(t.operation) - 1] = '\0';

    t.start_time = nc_clock_ms();
    t.end_time   = 0.0;
    t.success    = false;

    if (g_enterprise.config.enable_tracing) {
        nc_ai_log(NC_AI_LOG_DEBUG, "trace", t.correlation_id,
                  "START %s", t.operation);
    }

    return t;
}

void nc_ai_trace_end(NCAITrace *trace, bool success) {
    if (!trace) return;

    trace->end_time = nc_clock_ms();
    trace->success  = success;

    double duration = trace->end_time - trace->start_time;

    /* Update global stats */
    if (g_enterprise.initialized) {
        nc_mutex_lock(&g_enterprise.stats_lock);
        g_enterprise.total_requests++;
        g_enterprise.total_latency_ms += duration;
        nc_mutex_unlock(&g_enterprise.stats_lock);
    }

    if (g_enterprise.config.enable_tracing) {
        nc_ai_log(success ? NC_AI_LOG_DEBUG : NC_AI_LOG_WARN,
                  "trace", trace->correlation_id,
                  "END %s (%.2fms, %s)",
                  trace->operation, duration,
                  success ? "ok" : "failed");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Lifecycle
 * ═══════════════════════════════════════════════════════════ */

int nc_ai_enterprise_init(void) {
    if (g_enterprise.initialized) {
        nc_ai_log(NC_AI_LOG_WARN, "enterprise", "",
                  "already initialized — skipping");
        return 0;
    }

    memset(&g_enterprise, 0, sizeof(g_enterprise));
    g_enterprise.config     = nc_ai_env_config_load();
    g_enterprise.start_time = nc_clock_ms();
    nc_mutex_init(&g_enterprise.stats_lock);

    nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
              "initializing (env=%s, log_level=%d)",
              g_enterprise.config.env_name,
              g_enterprise.config.log_level);

    /* Circuit breaker */
    if (g_enterprise.config.enable_circuit_breaker) {
        g_enterprise.circuit_breaker = nc_ai_cb_create(NULL);
        if (!g_enterprise.circuit_breaker) {
            nc_ai_log(NC_AI_LOG_ERROR, "enterprise", "",
                      "failed to create circuit breaker");
            return -1;
        }
    }

    /* Cache */
    if (g_enterprise.config.enable_cache) {
        int max = env_int("NC_AI_CACHE_MAX", 512);
        double ttl = (double)env_int("NC_AI_CACHE_TTL_SEC", 300) * 1000.0;
        g_enterprise.cache = nc_ai_cache_create(max, ttl);
        if (!g_enterprise.cache) {
            nc_ai_log(NC_AI_LOG_ERROR, "enterprise", "",
                      "failed to create cache");
            return -1;
        }
    }

    /* Rate limiter */
    if (g_enterprise.config.enable_rate_limiter) {
        int max_rps = env_int("NC_AI_RATE_LIMIT", 100);
        double window = (double)env_int("NC_AI_RATE_WINDOW_SEC", 60) * 1000.0;
        g_enterprise.rate_limiter = nc_ai_rl_create(max_rps, window);
        if (!g_enterprise.rate_limiter) {
            nc_ai_log(NC_AI_LOG_ERROR, "enterprise", "",
                      "failed to create rate limiter");
            return -1;
        }
    }

    g_enterprise.initialized = true;

    nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
              "initialized successfully (cb=%s, cache=%s, rl=%s, trace=%s, health=%s)",
              g_enterprise.config.enable_circuit_breaker ? "on" : "off",
              g_enterprise.config.enable_cache           ? "on" : "off",
              g_enterprise.config.enable_rate_limiter    ? "on" : "off",
              g_enterprise.config.enable_tracing         ? "on" : "off",
              g_enterprise.config.enable_health_checks   ? "on" : "off");

    return 0;
}

void nc_ai_enterprise_shutdown(void) {
    if (!g_enterprise.initialized) {
        nc_ai_log(NC_AI_LOG_WARN, "enterprise", "",
                  "not initialized — nothing to shut down");
        return;
    }

    nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
              "shutting down enterprise layer");

    /* Report final stats before teardown */
    nc_mutex_lock(&g_enterprise.stats_lock);
    long total = g_enterprise.total_requests;
    double avg = (total > 0)
                     ? g_enterprise.total_latency_ms / (double)total
                     : 0.0;
    nc_mutex_unlock(&g_enterprise.stats_lock);

    nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
              "final stats: requests=%ld, avg_latency=%.2fms", total, avg);

    /* Tear down components in reverse order */
    if (g_enterprise.rate_limiter) {
        nc_ai_rl_free(g_enterprise.rate_limiter);
        g_enterprise.rate_limiter = NULL;
    }

    if (g_enterprise.cache) {
        NCAICacheStats cs = nc_ai_cache_stats(g_enterprise.cache);
        nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
                  "cache stats: hits=%d, misses=%d, evictions=%d, size=%d",
                  cs.hits, cs.misses, cs.evictions, cs.current_size);
        nc_ai_cache_free(g_enterprise.cache);
        g_enterprise.cache = NULL;
    }

    if (g_enterprise.circuit_breaker) {
        nc_ai_cb_free(g_enterprise.circuit_breaker);
        g_enterprise.circuit_breaker = NULL;
    }

    nc_mutex_destroy(&g_enterprise.stats_lock);

    g_enterprise.initialized = false;

    nc_ai_log(NC_AI_LOG_INFO, "enterprise", "",
              "shutdown complete");
}
