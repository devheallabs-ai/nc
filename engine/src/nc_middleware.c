/*
 * nc_middleware.c — Middleware system for the NC engine.
 *
 * Users write:
 *   configure:
 *       auth: "bearer"
 *       cors: true
 *       rate_limit: 100
 *       log_requests: true
 *
 * The engine automatically applies these middleware to every request.
 * Users never write middleware code — just configure it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/nc_middleware.h"
#include "../include/nc_json.h"
#include "../include/nc_platform.h"

/* Release heap resources inside an NcValue.
 * NcValue itself is a stack-allocated tagged union, but its inner
 * pointer (map, list, string) may be heap-allocated. */
static void nc_release_value(NcValue v) {
    switch (v.type) {
        case VAL_MAP:    nc_map_free(v.as.map);       break;
        case VAL_LIST:   nc_list_free(v.as.list);     break;
        case VAL_STRING: nc_string_free(v.as.string); break;
        default: break;
    }
}

/* HMAC-SHA256 for JWT signature verification.
 * Minimal implementation — no OpenSSL dependency required. */

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    #define RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
    #define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
    #define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
    #define EP0(x) (RR(x,2)^RR(x,13)^RR(x,22))
    #define EP1(x) (RR(x,6)^RR(x,11)^RR(x,25))
    #define SIG0(x) (RR(x,7)^RR(x,18)^((x)>>3))
    #define SIG1(x) (RR(x,17)^RR(x,19)^((x)>>10))
    uint32_t W[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i = 0; i < 16; i++)
        W[i] = ((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for (int i = 16; i < 64; i++)
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    a=state[0]; b=state[1]; c=state[2]; d=state[3];
    e=state[4]; f=state[5]; g=state[6]; h=state[7];
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + K[i] + W[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    #undef RR
    #undef CH
    #undef MAJ
    #undef EP0
    #undef EP1
    #undef SIG0
    #undef SIG1
}

typedef struct { uint32_t state[8]; uint8_t buf[64]; uint64_t count; } Sha256Ctx;

static void sha256_init(Sha256Ctx *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->count = 0;
}

static void sha256_update(Sha256Ctx *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->buf[ctx->count % 64] = data[i];
        ctx->count++;
        if (ctx->count % 64 == 0)
            sha256_transform(ctx->state, ctx->buf);
    }
}

static void sha256_final(Sha256Ctx *ctx, uint8_t hash[32]) {
    uint64_t bits = ctx->count * 8;
    uint8_t pad = 0x80;
    sha256_update(ctx, &pad, 1);
    pad = 0;
    while (ctx->count % 64 != 56)
        sha256_update(ctx, &pad, 1);
    for (int i = 7; i >= 0; i--) {
        uint8_t b = (uint8_t)(bits >> (i * 8));
        sha256_update(ctx, &b, 1);
    }
    for (int i = 0; i < 8; i++) {
        hash[i*4]   = (uint8_t)(ctx->state[i]>>24);
        hash[i*4+1] = (uint8_t)(ctx->state[i]>>16);
        hash[i*4+2] = (uint8_t)(ctx->state[i]>>8);
        hash[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

static void hmac_sha256(const uint8_t *key, int key_len,
                        const uint8_t *msg, int msg_len,
                        uint8_t out[32]) {
    uint8_t k_ipad[64], k_opad[64], tk[32];
    if (key_len > 64) {
        Sha256Ctx kctx; sha256_init(&kctx);
        sha256_update(&kctx, key, key_len);
        sha256_final(&kctx, tk);
        key = tk; key_len = 32;
    }
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (int i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }
    Sha256Ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, 64);
    sha256_update(&ctx, msg, msg_len);
    uint8_t inner[32];
    sha256_final(&ctx, inner);
    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, out);
}

/* ═══════════════════════════════════════════════════════════
 *  Middleware chain — runs before/after each request
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    MW_CORS,
    MW_AUTH,
    MW_RATE_LIMIT,
    MW_LOGGING,
    MW_TENANT,
    MW_TRACING,
} MiddlewareType;

typedef struct {
    MiddlewareType type;
    bool           enabled;
    NcMap         *config;
} Middleware;

#define MAX_MIDDLEWARE 16
static Middleware middleware_chain[MAX_MIDDLEWARE];
static int middleware_count = 0;

/* ── CORS Middleware ───────────────────────────────────────── */

void nc_mw_cors_apply(char *response_headers, int max_len) {
    const char *origin = getenv("NC_CORS_ORIGIN");
    if (!origin) {
        bool auth_enabled = (getenv("NC_API_KEY") || getenv("NC_API_KEYS") ||
                             getenv("NC_JWT_SECRET"));
        if (auth_enabled) {
            fprintf(stderr, "  [NC] WARNING: CORS origin defaulting to 'null' because "
                    "auth is enabled but NC_CORS_ORIGIN is not set.\n"
                    "  Set NC_CORS_ORIGIN to your frontend domain, e.g.:\n"
                    "    export NC_CORS_ORIGIN=\"https://myapp.example.com\"\n");
        }
        origin = "null";
    }
    const char *methods = getenv("NC_CORS_METHODS");
    if (!methods) methods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
    const char *headers = getenv("NC_CORS_HEADERS");
    if (!headers) headers = "Content-Type, Authorization, X-Tenant-ID";
    const char *max_age = getenv("NC_CORS_MAX_AGE");
    if (!max_age) max_age = "86400";

    /* Sanitize CORS values: reject any containing \r or \n to prevent header injection */
    if (strchr(origin, '\r') || strchr(origin, '\n') ||
        strchr(methods, '\r') || strchr(methods, '\n') ||
        strchr(headers, '\r') || strchr(headers, '\n') ||
        strchr(max_age, '\r') || strchr(max_age, '\n')) {
        fprintf(stderr, "[NC] WARN: CORS env var contains newline, ignoring.\n");
        return;
    }
    char cors[1024];
    snprintf(cors, sizeof(cors),
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Methods: %s\r\n"
        "Access-Control-Allow-Headers: %s\r\n"
        "Access-Control-Max-Age: %s\r\n",
        origin, methods, headers, max_age);
    int remaining = max_len - (int)strlen(response_headers) - 1;
    if (remaining > 0) strncat(response_headers, cors, remaining);
}

/* ── Auth Middleware ───────────────────────────────────────── */

/* Constant-time string comparison to prevent timing attacks.
 * Always compares full length regardless of where mismatch occurs. */
static bool ct_string_equal(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    volatile unsigned char result = (unsigned char)(la ^ lb);
    size_t len = la < lb ? la : lb;
    for (size_t i = 0; i < len; i++)
        result |= (unsigned char)((unsigned char)a[i] ^ (unsigned char)b[i]);
    return result == 0;
}

/* Base64url decode (JWT uses base64url, not standard base64) */
static int b64url_decode(const char *in, int in_len, char *out, int out_max) {
    static const int T[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,
        ['g']=32,['h']=33,['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,
        ['o']=40,['p']=41,['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,
        ['w']=48,['x']=49,['y']=50,['z']=51,['0']=52,['1']=53,['2']=54,['3']=55,
        ['4']=56,['5']=57,['6']=58,['7']=59,['8']=60,['9']=61,['-']=62,['_']=63
    };
    int oi = 0, bits = 0, val = 0;
    for (int i = 0; i < in_len && oi < out_max - 1; i++) {
        if (in[i] == '=' || in[i] == '\0') break;
        val = (val << 6) | T[(unsigned char)in[i]];
        bits += 6;
        if (bits >= 8) { bits -= 8; out[oi++] = (char)((val >> bits) & 0xFF); }
    }
    out[oi] = '\0';
    return oi;
}

/* Verify JWT HMAC-SHA256 signature against NC_JWT_SECRET.
 * Returns true if signature is valid, false otherwise. */
static bool jwt_verify_hs256(const char *token) {
    const char *secret = getenv("NC_JWT_SECRET");
    if (!secret || !secret[0]) {
        fprintf(stderr, "  [NC] JWT verification is off because NC_JWT_SECRET is not set.\n"
                "  To enable secure token verification, run:\n"
                "    export NC_JWT_SECRET=\"your-secret-key\"\n");
        return false;
    }

    const char *dot1 = strchr(token, '.');
    if (!dot1) return false;
    const char *dot2 = strchr(dot1 + 1, '.');
    if (!dot2) return false;

    /* signed_part = header.payload (everything before the second dot) */
    int signed_len = (int)(dot2 - token);
    const char *sig_b64 = dot2 + 1;
    int sig_b64_len = (int)strlen(sig_b64);

    /* Compute expected HMAC-SHA256 */
    uint8_t expected[32];
    hmac_sha256((const uint8_t *)secret, (int)strlen(secret),
                (const uint8_t *)token, signed_len, expected);

    /* Decode the provided signature */
    char sig_decoded[256];
    int sig_len = b64url_decode(sig_b64, sig_b64_len, sig_decoded, sizeof(sig_decoded));
    if (sig_len != 32) return false;

    /* Constant-time comparison */
    volatile unsigned char diff = 0;
    for (int i = 0; i < 32; i++)
        diff |= (unsigned char)((unsigned char)sig_decoded[i] ^ expected[i]);
    return diff == 0;
}

NcAuthContext nc_mw_auth_check(const char *auth_header) {
    NcAuthContext ctx = {0};

    if (!auth_header || !auth_header[0])
        return ctx;

    /* Bearer JWT token */
    if (strncasecmp(auth_header, "Bearer ", 7) == 0) {
        const char *token = auth_header + 7;
        if (strlen(token) < 10) return ctx;

        /* Verify JWT signature before trusting any claims */
        if (!jwt_verify_hs256(token)) return ctx;

        /* Decode JWT payload (header.payload.signature — we want the middle) */
        const char *dot1 = strchr(token, '.');
        if (!dot1) return ctx;
        const char *payload = dot1 + 1;
        const char *dot2 = strchr(payload, '.');
        int payload_len = dot2 ? (int)(dot2 - payload) : (int)strlen(payload);

        char decoded[4096];
        int dlen = b64url_decode(payload, payload_len, decoded, sizeof(decoded));
        if (dlen <= 0) return ctx;

        /* Parse JWT payload JSON to extract claims */
        NcValue claims = nc_json_parse(decoded);
        if (IS_MAP(claims)) {
            NcMap *cm = AS_MAP(claims);

            /* Verify "alg" from header is HS256 — reject "none" or unknown */
            char hdr_decoded[1024];
            int hdr_len = b64url_decode(token, (int)(dot1 - token), hdr_decoded, sizeof(hdr_decoded));
            if (hdr_len > 0) {
                NcValue hdr = nc_json_parse(hdr_decoded);
                if (IS_MAP(hdr)) {
                    NcString *alg_key = nc_string_from_cstr("alg");
                    NcValue alg_val = nc_map_get(AS_MAP(hdr), alg_key);
                    nc_string_free(alg_key);
                    if (!IS_STRING(alg_val) ||
                        strcmp(AS_STRING(alg_val)->chars, "HS256") != 0) {
                        nc_release_value(hdr);    /* free parsed header */
                        nc_release_value(claims); /* free parsed claims */
                        return ctx; /* reject non-HS256 algorithms */
                    }
                }
                nc_release_value(hdr); /* free parsed header after use */
            }

            /* Check expiry: "exp" claim (required — reject tokens without it) */
            NcString *exp_key = nc_string_from_cstr("exp");
            NcValue exp_val = nc_map_get(cm, exp_key);
            nc_string_free(exp_key);
            if (!IS_INT(exp_val)) {
                nc_release_value(claims); /* free parsed claims */
                return ctx; /* missing exp claim — reject */
            }
            time_t now = time(NULL);
            if ((time_t)AS_INT(exp_val) < now) {
                nc_release_value(claims); /* free parsed claims */
                return ctx; /* expired */
            }

            /* Extract user info — claim names configurable via env vars */
            const char *user_claim = getenv("NC_JWT_USER_CLAIM");
            if (!user_claim) user_claim = "sub";
            NcString *sub_key = nc_string_from_cstr(user_claim);
            NcValue sub = nc_map_get(cm, sub_key);
            nc_string_free(sub_key);
            if (IS_STRING(sub))
                strncpy(ctx.user_id, AS_STRING(sub)->chars, 127);

            const char *tenant_claim = getenv("NC_JWT_TENANT_CLAIM");
            if (!tenant_claim) tenant_claim = "tid";
            NcString *tid_key = nc_string_from_cstr(tenant_claim);
            NcValue tid = nc_map_get(cm, tid_key);
            nc_string_free(tid_key);
            if (IS_STRING(tid))
                strncpy(ctx.tenant_id, AS_STRING(tid)->chars, 63);

            /* Extract roles */
            const char *role_claim = getenv("NC_JWT_ROLE_CLAIM");
            if (!role_claim) role_claim = "roles";
            NcString *roles_key = nc_string_from_cstr(role_claim);
            NcValue roles = nc_map_get(cm, roles_key);
            nc_string_free(roles_key);
            if (IS_LIST(roles) && AS_LIST(roles)->count > 0) {
                NcValue first_role = nc_list_get(AS_LIST(roles), 0);
                if (IS_STRING(first_role))
                    strncpy(ctx.role, AS_STRING(first_role)->chars, 31);
            } else if (IS_STRING(roles)) {
                strncpy(ctx.role, AS_STRING(roles)->chars, 31);
            }
            const char *default_role = getenv("NC_DEFAULT_ROLE");
            if (!ctx.role[0]) strncpy(ctx.role, default_role ? default_role : "user", 31);
        }
        nc_release_value(claims); /* free parsed claims after extracting data */
        ctx.authenticated = true;
        return ctx;
    }

    /* API Key — validate against NC_API_KEYS (comma-separated list) */
    if (strncasecmp(auth_header, "ApiKey ", 7) == 0 ||
        strncasecmp(auth_header, "X-Api-Key ", 10) == 0) {
        const char *provided_key = auth_header;
        if (strncasecmp(provided_key, "ApiKey ", 7) == 0) provided_key += 7;
        else if (strncasecmp(provided_key, "X-Api-Key ", 10) == 0) provided_key += 10;
        while (*provided_key == ' ') provided_key++;

        const char *valid_keys = getenv("NC_API_KEYS");
        if (!valid_keys || !valid_keys[0]) {
            /* No API keys configured — reject all API key auth */
            return ctx;
        }

        /* Check against comma-separated list of valid keys */
        bool key_valid = false;
        char keys_copy[4096];
        strncpy(keys_copy, valid_keys, sizeof(keys_copy) - 1);
        keys_copy[sizeof(keys_copy) - 1] = '\0';
        char *saveptr = NULL;
        char *key = strtok_r(keys_copy, ",", &saveptr);
        while (key) {
            while (*key == ' ') key++;
            char *end = key + strlen(key) - 1;
            while (end > key && *end == ' ') *end-- = '\0';
            if (ct_string_equal(key, provided_key)) {
                key_valid = true;
                break;
            }
            key = strtok_r(NULL, ",", &saveptr);
        }

        if (!key_valid) return ctx;

        ctx.authenticated = true;
        const char *api_user = getenv("NC_API_KEY_USER");
        strncpy(ctx.user_id, api_user ? api_user : "api-key-user", 127);
        const char *api_role = getenv("NC_API_KEY_ROLE");
        strncpy(ctx.role, api_role ? api_role : "service", 31);
    }

    return ctx;
}

/* ── Rate Limiting Middleware ──────────────────────────────── */

/*
 * Sliding window log rate limiter — production-grade algorithm.
 *
 * Unlike fixed-window counters (which allow 2x burst at window boundaries),
 * the sliding window tracks individual request timestamps and counts
 * how many fall within the current window. This provides smooth,
 * accurate rate limiting with no burst spikes.
 *
 * Rate limiting by identity: uses user_id when authenticated, IP when not.
 * Supports per-role limits via NC_RATE_LIMIT_<ROLE> env vars or configure block.
 *
 * configure:
 *     rate_limit: 60                  # default for all users
 *     rate_limit_admin: 1000          # override for admin role
 *     rate_limit_premium: 500         # override for premium role
 *     rate_limit_window: 60           # window in seconds (default 60)
 */

#define RL_TIMESTAMPS_PER_ENTRY 256  /* track last N request times per client */

typedef struct {
    char   key[128];        /* user_id if authenticated, IP if not */
    int    limit;           /* per-window limit for this entry */
    time_t timestamps[RL_TIMESTAMPS_PER_ENTRY]; /* circular buffer of request times */
    int    ts_head;         /* next write position */
    int    ts_count;        /* total timestamps stored */
    time_t last_access;     /* for LRU eviction */
} RateLimitEntry;

#define RL_MAX_CLIENTS 1024
static RateLimitEntry rl_entries[RL_MAX_CLIENTS];
static int rl_count = 0;
static int rl_default_limit = 100;
static int rl_window_seconds = 60;
static nc_mutex_t rl_mutex = NC_MUTEX_INITIALIZER;

/* Per-role overrides from configure block */
#define RL_MAX_ROLES 16
static struct { char role[32]; int limit; } rl_role_limits[RL_MAX_ROLES];
static int rl_role_count = 0;

void nc_mw_rate_limit_set_role(const char *role, int limit) {
    if (rl_role_count >= RL_MAX_ROLES) return;
    strncpy(rl_role_limits[rl_role_count].role, role, 31);
    rl_role_limits[rl_role_count].role[31] = '\0';
    rl_role_limits[rl_role_count].limit = limit;
    rl_role_count++;
}

static int get_limit_for_role(const char *role) {
    if (!role || !role[0]) return rl_default_limit;

    /* Check configure-block role overrides */
    for (int i = 0; i < rl_role_count; i++) {
        if (strcmp(rl_role_limits[i].role, role) == 0)
            return rl_role_limits[i].limit;
    }

    /* Check env var NC_RATE_LIMIT_<ROLE> */
    char env_name[64] = "NC_RATE_LIMIT_";
    int ei = 14;
    for (int i = 0; role[i] && ei < 62; i++)
        env_name[ei++] = (role[i] >= 'a' && role[i] <= 'z') ? role[i] - 32 : role[i];
    env_name[ei] = '\0';
    const char *env_val = getenv(env_name);
    if (env_val) return atoi(env_val);

    return rl_default_limit;
}

static RateLimitEntry *rl_find_or_create(const char *key, int limit) {
    time_t now = time(NULL);

    for (int i = 0; i < rl_count; i++) {
        if (strcmp(rl_entries[i].key, key) == 0) {
            rl_entries[i].last_access = now;
            return &rl_entries[i];
        }
    }

    /* Create new entry */
    RateLimitEntry *entry = NULL;
    if (rl_count < RL_MAX_CLIENTS) {
        entry = &rl_entries[rl_count++];
    } else {
        /* Evict least recently accessed entry */
        time_t oldest = now;
        int oldest_idx = 0;
        for (int i = 0; i < rl_count; i++) {
            if (rl_entries[i].last_access < oldest) {
                oldest = rl_entries[i].last_access;
                oldest_idx = i;
            }
        }
        entry = &rl_entries[oldest_idx];
    }

    memset(entry, 0, sizeof(RateLimitEntry));
    strncpy(entry->key, key, 127);
    entry->key[127] = '\0';
    entry->limit = limit;
    entry->last_access = now;
    return entry;
}

/* Legacy: rate limit by IP only (backward compatible) */
bool nc_mw_rate_limit_check(const char *client_ip) {
    return nc_mw_rate_limit_check_user(client_ip, NULL, NULL);
}

/* Enhanced: sliding window rate limit by user identity with role-based limits.
 * Counts requests within a sliding time window for accurate rate limiting
 * without the 2x burst problem of fixed-window counters. */
bool nc_mw_rate_limit_check_user(const char *client_ip, const char *user_id,
                                  const char *role) {
    time_t now = time(NULL);

    /* Read window config inside mutex to avoid data race */
    nc_mutex_lock(&rl_mutex);

    const char *window_str = getenv("NC_RATE_LIMIT_WINDOW");
    if (window_str) {
        int v = atoi(window_str);
        if (v > 0) rl_window_seconds = v;
    }

    int limit = get_limit_for_role(role);
    const char *key = (user_id && user_id[0]) ? user_id : client_ip;

    RateLimitEntry *entry = rl_find_or_create(key, limit);
    if (!entry) {
        nc_mutex_unlock(&rl_mutex);
        return false;
    }

    /* Update limit in case role configuration changed */
    entry->limit = limit;

    /* Count requests within the sliding window */
    time_t window_start = now - rl_window_seconds;
    int count_in_window = 0;
    for (int i = 0; i < entry->ts_count && i < RL_TIMESTAMPS_PER_ENTRY; i++) {
        int idx = (entry->ts_head - 1 - i + RL_TIMESTAMPS_PER_ENTRY) % RL_TIMESTAMPS_PER_ENTRY;
        if (entry->timestamps[idx] >= window_start) {
            count_in_window++;
        }
    }

    bool allowed = count_in_window < entry->limit;
    if (allowed) {
        /* Record this request timestamp */
        entry->timestamps[entry->ts_head] = now;
        entry->ts_head = (entry->ts_head + 1) % RL_TIMESTAMPS_PER_ENTRY;
        if (entry->ts_count < RL_TIMESTAMPS_PER_ENTRY)
            entry->ts_count++;
    }

    nc_mutex_unlock(&rl_mutex);
    return allowed;
}

/* Generate standard rate limit response headers (sliding window aware) */
void nc_mw_rate_limit_get_headers(const char *key, char *headers, int max_len) {
    nc_mutex_lock(&rl_mutex);
    int limit = rl_default_limit, remaining = rl_default_limit;
    time_t now = time(NULL);
    time_t reset = now + rl_window_seconds;

    for (int i = 0; i < rl_count; i++) {
        if (strcmp(rl_entries[i].key, key) == 0) {
            limit = rl_entries[i].limit;
            /* Count requests in current window */
            time_t window_start = now - rl_window_seconds;
            int count = 0;
            for (int j = 0; j < rl_entries[i].ts_count && j < RL_TIMESTAMPS_PER_ENTRY; j++) {
                int idx = (rl_entries[i].ts_head - 1 - j + RL_TIMESTAMPS_PER_ENTRY)
                          % RL_TIMESTAMPS_PER_ENTRY;
                if (rl_entries[i].timestamps[idx] >= window_start)
                    count++;
            }
            remaining = limit - count;
            if (remaining < 0) remaining = 0;
            /* Find oldest timestamp in window for reset estimate */
            time_t oldest_in_window = now;
            for (int j = 0; j < rl_entries[i].ts_count && j < RL_TIMESTAMPS_PER_ENTRY; j++) {
                int idx = (rl_entries[i].ts_head - 1 - j + RL_TIMESTAMPS_PER_ENTRY)
                          % RL_TIMESTAMPS_PER_ENTRY;
                if (rl_entries[i].timestamps[idx] >= window_start &&
                    rl_entries[i].timestamps[idx] < oldest_in_window)
                    oldest_in_window = rl_entries[i].timestamps[idx];
            }
            reset = oldest_in_window + rl_window_seconds;
            break;
        }
    }
    nc_mutex_unlock(&rl_mutex);

    snprintf(headers, max_len,
        "X-RateLimit-Limit: %d\r\n"
        "X-RateLimit-Remaining: %d\r\n"
        "X-RateLimit-Reset: %lld\r\n",
        limit, remaining, (long long)reset);
}

/* ── JWT Generation ───────────────────────────────────────── */

static void b64url_encode(const uint8_t *in, int in_len, char *out, int out_max) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    int oi = 0;
    for (int i = 0; i < in_len && oi < out_max - 4; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1 < in_len) v |= (uint32_t)in[i+1] << 8;
        if (i + 2 < in_len) v |= (uint32_t)in[i+2];
        out[oi++] = T[(v >> 18) & 0x3F];
        out[oi++] = T[(v >> 12) & 0x3F];
        if (i + 1 < in_len) out[oi++] = T[(v >> 6) & 0x3F];
        if (i + 2 < in_len) out[oi++] = T[v & 0x3F];
    }
    out[oi] = '\0';
}

static void b64url_encode_str(const char *in, char *out, int out_max) {
    b64url_encode((const uint8_t *)in, (int)strlen(in), out, out_max);
}

/*
 * Generate a signed JWT (HS256).
 *
 * NC code:
 *   set token to jwt_generate("user123", "admin", 3600, {"org": "acme"})
 *   respond with {"token": token}
 *
 * Requires NC_JWT_SECRET env var.
 */
NcValue nc_jwt_generate(const char *user_id, const char *role,
                         int expires_in_seconds, NcMap *extra_claims) {
    const char *secret = getenv("NC_JWT_SECRET");
    if (!secret || !secret[0]) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("NC_JWT_SECRET not set. Export it to generate tokens.")));
        return NC_MAP(err);
    }

    /* Header: {"alg":"HS256","typ":"JWT"} */
    char hdr_b64[256];
    b64url_encode_str("{\"alg\":\"HS256\",\"typ\":\"JWT\"}", hdr_b64, sizeof(hdr_b64));

    time_t now = time(NULL);
    NcMap *payload = nc_map_new();
    nc_map_set(payload, nc_string_from_cstr("sub"),
        NC_STRING(nc_string_from_cstr(user_id ? user_id : "anonymous")));
    nc_map_set(payload, nc_string_from_cstr("role"),
        NC_STRING(nc_string_from_cstr(role ? role : "user")));
    nc_map_set(payload, nc_string_from_cstr("iat"), NC_INT((int64_t)now));
    nc_map_set(payload, nc_string_from_cstr("exp"),
        NC_INT((int64_t)(now + (expires_in_seconds > 0 ? expires_in_seconds : 3600))));

    /* Merge extra claims through the JSON serializer so keys and values are escaped safely. */
    if (extra_claims) {
        for (int i = 0; i < extra_claims->count; i++)
            nc_map_set(payload, nc_string_ref(extra_claims->keys[i]), extra_claims->values[i]);
    }
    char *payload_json = nc_json_serialize(NC_MAP(payload), false);
    if (!payload_json) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Could not serialize JWT payload.")));
        return NC_MAP(err);
    }

    char payload_b64[4096];
    b64url_encode_str(payload_json, payload_b64, sizeof(payload_b64));
    free(payload_json);

    /* Signed part: header.payload */
    char signed_part[8192];
    snprintf(signed_part, sizeof(signed_part), "%s.%s", hdr_b64, payload_b64);

    /* HMAC-SHA256 signature */
    uint8_t sig[32];
    hmac_sha256((const uint8_t *)secret, (int)strlen(secret),
                (const uint8_t *)signed_part, (int)strlen(signed_part), sig);

    char sig_b64[256];
    b64url_encode(sig, 32, sig_b64, sizeof(sig_b64));

    /* Final token: header.payload.signature */
    char token[8192];
    snprintf(token, sizeof(token), "%s.%s", signed_part, sig_b64);

    return NC_STRING(nc_string_from_cstr(token));
}

/* ── Logging Middleware ─────────────────────────────────────── */

void nc_mw_log_request(const char *method, const char *path,
                        int status, double duration_ms) {
    time_t now = time(NULL);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&now));
    printf("  %s  %-6s %-30s  %d  %.1fms\n",
           time_str, method, path, status, duration_ms);
}

/* ── Tenant Isolation Middleware ────────────────────────────── */

typedef struct {
    char tenant_id[64];
    char org_id[64];
} NcTenantContext;

NcTenantContext nc_mw_tenant_extract(const char *headers[][2], int header_count) {
    NcTenantContext ctx = {0};
    for (int i = 0; i < header_count; i++) {
        if (strcasecmp(headers[i][0], "X-Tenant-ID") == 0)
            strncpy(ctx.tenant_id, headers[i][1], 63);
        if (strcasecmp(headers[i][0], "X-Org-ID") == 0)
            strncpy(ctx.org_id, headers[i][1], 63);
    }
    return ctx;
}

/* ═══════════════════════════════════════════════════════════
 *  Middleware configuration from NC configure block
 *
 *  configure:
 *      cors: true
 *      auth: "bearer"
 *      rate_limit: 100
 *      log_requests: true
 * ═══════════════════════════════════════════════════════════ */

void nc_middleware_setup(NcMap *config) {
    if (!config) return;
    middleware_count = 0;

    NcString *cors_key = nc_string_from_cstr("cors");
    NcValue cors_val = nc_map_get(config, cors_key);
    nc_string_free(cors_key);
    if (nc_truthy(cors_val)) {
        middleware_chain[middleware_count].type = MW_CORS;
        middleware_chain[middleware_count].enabled = true;
        middleware_count++;
    }

    NcString *auth_key = nc_string_from_cstr("auth");
    NcValue auth_val = nc_map_get(config, auth_key);
    nc_string_free(auth_key);
    if (!IS_NONE(auth_val)) {
        middleware_chain[middleware_count].type = MW_AUTH;
        middleware_chain[middleware_count].enabled = true;
        middleware_count++;
    }

    NcString *rl_key = nc_string_from_cstr("rate_limit");
    NcValue rl_val = nc_map_get(config, rl_key);
    nc_string_free(rl_key);
    if (IS_INT(rl_val)) {
        rl_default_limit = (int)AS_INT(rl_val);
        middleware_chain[middleware_count].type = MW_RATE_LIMIT;
        middleware_chain[middleware_count].enabled = true;
        middleware_count++;
    }

    /* Per-role rate limit overrides from configure block:
     *   rate_limit_admin: 1000
     *   rate_limit_premium: 500
     *   rate_limit_free: 10 */
    rl_role_count = 0;
    const char *rl_prefix = "rate_limit_";
    int rl_prefix_len = 11;
    for (int i = 0; i < config->count; i++) {
        const char *key = config->keys[i]->chars;
        if (strncmp(key, rl_prefix, rl_prefix_len) == 0 && IS_INT(config->values[i])) {
            const char *role = key + rl_prefix_len;
            if (role[0] && strcmp(role, "window") != 0) {
                nc_mw_rate_limit_set_role(role, (int)AS_INT(config->values[i]));
            }
        }
    }

    /* Rate limit window override */
    NcString *rlw_key = nc_string_from_cstr("rate_limit_window");
    NcValue rlw_val = nc_map_get(config, rlw_key);
    nc_string_free(rlw_key);
    if (IS_INT(rlw_val)) {
        rl_window_seconds = (int)AS_INT(rlw_val);
    }

    NcString *log_key = nc_string_from_cstr("log_requests");
    NcValue log_val = nc_map_get(config, log_key);
    nc_string_free(log_key);
    if (nc_truthy(log_val)) {
        middleware_chain[middleware_count].type = MW_LOGGING;
        middleware_chain[middleware_count].enabled = true;
        middleware_count++;
    }
}
