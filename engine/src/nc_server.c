/*
 * nc_server.c — Production-grade HTTP server for NC.
 *
 * When you write:
 *   api:
 *       POST /diagnose runs diagnose
 *       GET /health runs health_check
 *
 * And run: nc serve service.nc
 *
 * NC starts a real HTTP server on the configured port,
 * routes requests to behaviors, and returns JSON responses.
 *
 * Pure NC — single binary, zero external dependencies.
 *
 * Architecture:
 *   - Thread pool with bounded request queue (no thread-per-request churn)
 *   - HTTP/1.1 keep-alive for connection reuse
 *   - epoll/kqueue I/O multiplexing on supported platforms
 *   - Configurable backlog, workers, timeouts, and buffer sizes
 *   - Graceful shutdown with connection draining
 *   - Chunked transfer encoding for streaming responses
 *
 * Handles: GET, POST, PUT, DELETE, PATCH, OPTIONS (CORS).
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include "../include/nc_version.h"

static int nc_get_socket_timeout(void) {
    const char *t = getenv("NC_SOCKET_TIMEOUT");
    return (t && t[0]) ? atoi(t) : 30;
}

/* I/O multiplexing headers */
#ifndef NC_WINDOWS
#  if defined(__linux__)
#    include <sys/epoll.h>
#    define NC_USE_EPOLL 1
#  elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#    include <sys/event.h>
#    define NC_USE_KQUEUE 1
#  endif
#endif

#define printf nc_printf

/* ═══════════════════════════════════════════════════════════
 *  Server configuration — all tunable via environment variables.
 *  No hardcoded values; every parameter has a sensible default
 *  that can be overridden without recompilation.
 * ═══════════════════════════════════════════════════════════ */

#define NC_DEFAULT_MAX_REQUEST    (1024 * 1024)       /* 1 MB  */
#define NC_DEFAULT_MAX_RESPONSE   (4 * 1024 * 1024)   /* 4 MB  */
#define NC_DEFAULT_LISTEN_BACKLOG 512
#define NC_DEFAULT_MAX_WORKERS    64
#define NC_DEFAULT_KEEPALIVE_MAX  100
#define NC_DEFAULT_KEEPALIVE_SEC  30
#define NC_DEFAULT_REQUEST_QUEUE  4096
#define NC_MAX_HEADERS            64

static int nc_max_request(void) {
    const char *e = getenv("NC_MAX_REQUEST_SIZE");
    if (e) { int v = atoi(e); if (v > 0) return v; }
    return NC_DEFAULT_MAX_REQUEST;
}

static int nc_max_response(void) {
    const char *e = getenv("NC_MAX_RESPONSE_SIZE");
    if (e) { int v = atoi(e); if (v > 0) return v; }
    return NC_DEFAULT_MAX_RESPONSE;
}

static int nc_listen_backlog(void) {
    const char *e = getenv("NC_LISTEN_BACKLOG");
    if (e) { int v = atoi(e); if (v > 0) return v; }
    return NC_DEFAULT_LISTEN_BACKLOG;
}

static int nc_keepalive_max(void) {
    const char *e = getenv("NC_KEEPALIVE_MAX");
    if (e) { int v = atoi(e); if (v >= 0) return v; }
    return NC_DEFAULT_KEEPALIVE_MAX;
}

static int nc_keepalive_timeout(void) {
    const char *e = getenv("NC_KEEPALIVE_TIMEOUT");
    if (e) { int v = atoi(e); if (v > 0) return v; }
    return NC_DEFAULT_KEEPALIVE_SEC;
}

/* Backward compat for stack-allocated buffers that can't use the function */
#define NC_MAX_RESPONSE NC_DEFAULT_MAX_RESPONSE

static volatile bool server_running = true;
static nc_atomic_int active_workers;
static int server_port = 8080;

/* ═══════════════════════════════════════════════════════════
 *  Response buffer pool — eliminates malloc/free per request.
 *
 *  Thread-safe freelist of pre-allocated response buffers.
 *  Under high load, avoids heap fragmentation from repeated
 *  4MB allocations. Pool size grows on demand but buffers
 *  are reused rather than freed.
 * ═══════════════════════════════════════════════════════════ */

#define RESP_POOL_MAX 64  /* max pooled buffers */

typedef struct {
    char *bufs[RESP_POOL_MAX];
    int   sizes[RESP_POOL_MAX]; /* capacity of each buffer */
    int   count;
    nc_mutex_t lock;
} RespBufPool;

static RespBufPool resp_pool = { .count = 0, .lock = NC_MUTEX_INITIALIZER };

static char *resp_pool_acquire(int needed_size) {
    nc_mutex_lock(&resp_pool.lock);
    for (int i = 0; i < resp_pool.count; i++) {
        if (resp_pool.sizes[i] >= needed_size) {
            char *buf = resp_pool.bufs[i];
            resp_pool.bufs[i] = resp_pool.bufs[--resp_pool.count];
            resp_pool.sizes[i] = resp_pool.sizes[resp_pool.count];
            nc_mutex_unlock(&resp_pool.lock);
            return buf;
        }
    }
    nc_mutex_unlock(&resp_pool.lock);
    return malloc(needed_size);
}

static void resp_pool_release(char *buf, int size) {
    nc_mutex_lock(&resp_pool.lock);
    if (resp_pool.count < RESP_POOL_MAX) {
        resp_pool.bufs[resp_pool.count] = buf;
        resp_pool.sizes[resp_pool.count] = size;
        resp_pool.count++;
        nc_mutex_unlock(&resp_pool.lock);
    } else {
        nc_mutex_unlock(&resp_pool.lock);
        free(buf);
    }
}

/* Prometheus-compatible metrics counters */
static nc_atomic_int nc_metrics_requests_total;
static nc_atomic_int nc_metrics_errors_total;
static nc_atomic_int nc_metrics_active_connections;
static nc_atomic_int nc_metrics_queue_depth;
static nc_atomic_int nc_metrics_keepalive_reuse;
static nc_atomic_int nc_metrics_queue_full_rejects;
static nc_atomic_int nc_metrics_requests_latency_sum; /* ms, for avg calc */

/* ═══════════════════════════════════════════════════════════
 *  Bounded request queue — replaces immediate 503 rejection.
 *  Accepted connections are queued here and consumed by the
 *  thread pool workers, providing backpressure under load.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    nc_socket_t  client_fd;
    char         client_ip[48];
    char        *raw;
    int          raw_len;
} QueuedRequest;

static QueuedRequest *request_queue = NULL;
static int rq_capacity = 0;
static int rq_head = 0;
static int rq_tail = 0;
static int rq_count = 0;
static nc_mutex_t rq_mutex = NC_MUTEX_INITIALIZER;
static nc_cond_t  rq_not_empty;
static nc_cond_t  rq_not_full;
static bool       rq_shutdown = false;

static void rq_init(int capacity) {
    rq_capacity = capacity > 0 ? capacity : NC_DEFAULT_REQUEST_QUEUE;
    request_queue = calloc(rq_capacity, sizeof(QueuedRequest));
    rq_head = rq_tail = rq_count = 0;
    rq_shutdown = false;
    nc_cond_init(&rq_not_empty);
    nc_cond_init(&rq_not_full);
}

static void rq_destroy(void) {
    /* Drain remaining queued requests */
    for (int i = 0; i < rq_count; i++) {
        int idx = (rq_head + i) % rq_capacity;
        free(request_queue[idx].raw);
        nc_closesocket(request_queue[idx].client_fd);
    }
    free(request_queue);
    request_queue = NULL;
    nc_cond_destroy(&rq_not_empty);
    nc_cond_destroy(&rq_not_full);
}

/* Enqueue with bounded wait. Returns false if queue is full after timeout. */
static bool rq_enqueue(QueuedRequest *req, int timeout_ms) {
    nc_mutex_lock(&rq_mutex);
    while (rq_count >= rq_capacity && !rq_shutdown) {
        if (timeout_ms <= 0) {
            nc_mutex_unlock(&rq_mutex);
            return false;
        }
        int rc = nc_cond_timedwait(&rq_not_full, &rq_mutex, timeout_ms);
        if (rc != 0) { /* timeout */
            nc_mutex_unlock(&rq_mutex);
            return false;
        }
    }
    if (rq_shutdown) { nc_mutex_unlock(&rq_mutex); return false; }
    request_queue[rq_tail] = *req;
    rq_tail = (rq_tail + 1) % rq_capacity;
    rq_count++;
    nc_atomic_store(&nc_metrics_queue_depth, rq_count);
    nc_cond_signal(&rq_not_empty);
    nc_mutex_unlock(&rq_mutex);
    return true;
}

/* Dequeue with blocking wait. Returns false on shutdown. */
static bool rq_dequeue(QueuedRequest *out) {
    nc_mutex_lock(&rq_mutex);
    while (rq_count == 0 && !rq_shutdown) {
        nc_cond_wait(&rq_not_empty, &rq_mutex);
    }
    if (rq_count == 0 && rq_shutdown) {
        nc_mutex_unlock(&rq_mutex);
        return false;
    }
    *out = request_queue[rq_head];
    request_queue[rq_head].raw = NULL; /* ownership transferred */
    rq_head = (rq_head + 1) % rq_capacity;
    rq_count--;
    nc_atomic_store(&nc_metrics_queue_depth, rq_count);
    nc_cond_signal(&rq_not_full);
    nc_mutex_unlock(&rq_mutex);
    return true;
}

static void signal_handler(int sig) {
    (void)sig;
    server_running = false;
    /* Wake up any threads blocked on the request queue */
    rq_shutdown = true;
    nc_cond_broadcast(&rq_not_empty);
    nc_cond_broadcast(&rq_not_full);
}

/* ═══════════════════════════════════════════════════════════
 *  HTTP Request parsing
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char  method[16];
    char  path[512];
    char  query[512];
    char *body;
    int   body_len;
    int   body_cap;
    char  content_type[128];
    char  auth[256];
    char  headers[NC_MAX_HEADERS][2][256];
    int   header_count;
    bool  keep_alive;    /* HTTP/1.1 default: true; set false by "Connection: close" */
    int   content_length; /* parsed Content-Length, -1 if absent */
} HttpRequest;

static bool parse_request(const char *raw, int raw_len, HttpRequest *req) {
    /* Preserve body pointer — caller allocates it before calling us */
    char *saved_body = req->body;
    int saved_cap = req->body_cap;
    memset(req, 0, sizeof(HttpRequest));
    req->body = saved_body;
    req->body_cap = saved_cap;
    req->keep_alive = true;  /* HTTP/1.1 default */
    req->content_length = -1;
    if (req->body) req->body[0] = '\0';

    const char *p = raw;
    int i = 0;
    while (*p && *p != ' ' && i < 15) req->method[i++] = *p++;
    req->method[i] = '\0';
    while (*p == ' ') p++;

    i = 0;
    while (*p && *p != ' ' && *p != '?' && i < 510) req->path[i++] = *p++;
    req->path[i] = '\0';
    if (*p == '?') {
        p++;
        i = 0;
        while (*p && *p != ' ' && i < 510) req->query[i++] = *p++;
        req->query[i] = '\0';
    }

    const char *headers_start = strstr(p, "\r\n");
    if (!headers_start) return false;
    headers_start += 2;

    const char *h = headers_start;
    while (*h && !(h[0] == '\r' && h[1] == '\n')) {
        const char *colon = strchr(h, ':');
        const char *eol = strstr(h, "\r\n");
        if (!colon || !eol || colon > eol) break;

        int klen = (int)(colon - h);
        const char *val = colon + 1;
        while (*val == ' ') val++;
        int vlen = (int)(eol - val);

        if (req->header_count < NC_MAX_HEADERS) {
            int hi = req->header_count;
            if (klen > 255) klen = 255;
            if (vlen > 255) vlen = 255;
            memcpy(req->headers[hi][0], h, klen);
            req->headers[hi][0][klen] = '\0';
            memcpy(req->headers[hi][1], val, vlen);
            req->headers[hi][1][vlen] = '\0';

            if (strncasecmp(h, "Content-Type", 12) == 0)
                { int clen = vlen < 127 ? vlen : 127; memcpy(req->content_type, val, clen); req->content_type[clen] = '\0'; }
            if (strncasecmp(h, "Authorization", 13) == 0)
                { int alen = vlen < 255 ? vlen : 255; memcpy(req->auth, val, alen); req->auth[alen] = '\0'; }
            if (strncasecmp(h, "Connection", 10) == 0) {
                if (strncasecmp(val, "close", 5) == 0) req->keep_alive = false;
                else if (strncasecmp(val, "keep-alive", 10) == 0) req->keep_alive = true;
            }
            if (strncasecmp(h, "Content-Length", 14) == 0) {
                long cl = strtol(val, NULL, 10);
                if (cl < 0) cl = 0;
                if (cl > nc_max_request()) cl = nc_max_request();
                req->content_length = (int)cl;
            }

            req->header_count++;
        }
        h = eol + 2;
    }

    const char *body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req->body_len = raw_len - (int)(body_start - raw);
        if (req->body_len < 0) req->body_len = 0; /* prevent integer underflow */
        if (req->body_len > req->body_cap - 1) req->body_len = req->body_cap - 1;
        if (req->body_len > 0) memcpy(req->body, body_start, req->body_len);
        req->body[req->body_len] = '\0';
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  HTTP Response building — keep-alive aware
 * ═══════════════════════════════════════════════════════════ */

static const char *detect_content_type(const char *body) {
    if (!body) return "application/json";
    while (*body == ' ' || *body == '\n' || *body == '\r' || *body == '\t') body++;
    if (strncasecmp(body, "<!doctype", 9) == 0 || strncasecmp(body, "<html", 5) == 0)
        return "text/html; charset=utf-8";
    if (body[0] == '<')
        return "text/xml; charset=utf-8";
    return "application/json";
}

/* Keep-alive-aware response builder */
static int build_response_ct_ka(char *buf, int buf_size, int status,
                                const char *status_text, const char *content_type,
                                const char *body, bool keep_alive) {
    int body_len = body ? (int)strlen(body) : 0;

    const char *cors_origin = getenv("NC_CORS_ORIGIN");
    if (!cors_origin || !cors_origin[0]) cors_origin = NULL;
    const char *cors_methods = getenv("NC_CORS_METHODS");
    if (!cors_methods) cors_methods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
    const char *cors_headers = getenv("NC_CORS_HEADERS");
    if (!cors_headers) cors_headers = "Content-Type, Authorization";

    const char *conn_header = keep_alive ? "keep-alive" : "close";
    int ka_timeout = nc_keepalive_timeout();

    /* Enterprise security headers */
    const char *security_headers =
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";

    char ka_params[64] = "";
    if (keep_alive)
        snprintf(ka_params, sizeof(ka_params),
            "Keep-Alive: timeout=%d, max=%d\r\n", ka_timeout, nc_keepalive_max());

    if (cors_origin) {
        return snprintf(buf, buf_size,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Cache-Control: no-cache\r\n"
            "%s"
            "%s"
            "Access-Control-Allow-Origin: %s\r\n"
            "Access-Control-Allow-Methods: %s\r\n"
            "Access-Control-Allow-Headers: %s\r\n"
            "Connection: %s\r\n"
            "\r\n"
            "%s",
            status, status_text, content_type, body_len,
            security_headers,
            ka_params,
            cors_origin, cors_methods, cors_headers,
            conn_header,
            body ? body : "");
    }
    return snprintf(buf, buf_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Cache-Control: no-cache\r\n"
        "%s"
        "%s"
        "Connection: %s\r\n"
        "\r\n"
        "%s",
        status, status_text, content_type, body_len,
        security_headers, ka_params,
        conn_header,
        body ? body : "");
}

static int build_response_ct(char *buf, int buf_size, int status,
                             const char *status_text, const char *content_type,
                             const char *body) {
    return build_response_ct_ka(buf, buf_size, status, status_text,
                                content_type, body, false);
}

static int build_response(char *buf, int buf_size, int status,
                          const char *status_text, const char *body) {
    return build_response_ct(buf, buf_size, status, status_text,
                             "application/json", body);
}

/* ═══════════════════════════════════════════════════════════
 *  Chunked Transfer Encoding — for large response bodies
 *
 *  When the body exceeds NC_CHUNK_THRESHOLD (default 64 KB),
 *  send with Transfer-Encoding: chunked instead of
 *  Content-Length.  Benefits:
 *    1. No need to buffer the entire response before sending
 *    2. Reduces memory pressure for large payloads
 *    3. Enables streaming responses to clients
 *    4. Standard HTTP/1.1 (RFC 7230 §4.1)
 * ═══════════════════════════════════════════════════════════ */

#define NC_DEFAULT_CHUNK_THRESHOLD  (64 * 1024)   /* 64 KB */
#define NC_DEFAULT_CHUNK_SIZE       (16 * 1024)    /* 16 KB per chunk */

static int nc_chunk_threshold(void) {
    const char *e = getenv("NC_CHUNK_THRESHOLD");
    return e ? atoi(e) : NC_DEFAULT_CHUNK_THRESHOLD;
}

static int nc_chunk_size(void) {
    const char *e = getenv("NC_CHUNK_SIZE");
    int v = e ? atoi(e) : NC_DEFAULT_CHUNK_SIZE;
    if (v < 512) v = 512;
    if (v > 1024 * 1024) v = 1024 * 1024;
    return v;
}

/* Send a complete response using chunked transfer encoding.
 * Returns true on success, false on send error. */
static bool send_chunked_response(nc_socket_t fd, int status,
                                  const char *status_text,
                                  const char *content_type,
                                  const char *body, int body_len,
                                  bool keep_alive) {
    const char *cors_origin = getenv("NC_CORS_ORIGIN");
    if (!cors_origin || !cors_origin[0]) cors_origin = NULL;
    const char *cors_methods = getenv("NC_CORS_METHODS");
    if (!cors_methods) cors_methods = "GET, POST, PUT, DELETE, PATCH, OPTIONS";
    const char *cors_headers = getenv("NC_CORS_HEADERS");
    if (!cors_headers) cors_headers = "Content-Type, Authorization";

    const char *conn_header = keep_alive ? "keep-alive" : "close";
    char ka_params[64] = "";
    if (keep_alive)
        snprintf(ka_params, sizeof(ka_params),
            "Keep-Alive: timeout=%d, max=%d\r\n",
            nc_keepalive_timeout(), nc_keepalive_max());

    const char *security_headers =
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n";

    /* Build headers — Transfer-Encoding: chunked instead of Content-Length */
    char hdr[2048];
    int hlen;
    if (cors_origin) {
        hlen = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Cache-Control: no-cache\r\n"
            "%s"
            "%s"
            "Access-Control-Allow-Origin: %s\r\n"
            "Access-Control-Allow-Methods: %s\r\n"
            "Access-Control-Allow-Headers: %s\r\n"
            "Connection: %s\r\n"
            "\r\n",
            status, status_text, content_type,
            security_headers, ka_params,
            cors_origin, cors_methods, cors_headers, conn_header);
    } else {
        hlen = snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: %s\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Cache-Control: no-cache\r\n"
            "%s"
            "%s"
            "Connection: %s\r\n"
            "\r\n",
            status, status_text, content_type,
            security_headers, ka_params, conn_header);
    }

    /* Send headers */
    if (nc_send(fd, hdr, hlen, 0) <= 0) return false;

    /* Send body in chunks */
    int chunk_sz = nc_chunk_size();
    int offset = 0;
    while (offset < body_len) {
        int remaining = body_len - offset;
        int this_chunk = remaining < chunk_sz ? remaining : chunk_sz;

        /* Chunk header: hex size + CRLF */
        char chunk_hdr[32];
        int ch_len = snprintf(chunk_hdr, sizeof(chunk_hdr), "%x\r\n", this_chunk);
        if (nc_send(fd, chunk_hdr, ch_len, 0) <= 0) return false;

        /* Chunk data */
        if (nc_send(fd, body + offset, this_chunk, 0) <= 0) return false;

        /* Chunk trailer: CRLF */
        if (nc_send(fd, "\r\n", 2, 0) <= 0) return false;

        offset += this_chunk;
    }

    /* Final chunk: 0-length + CRLF + CRLF */
    if (nc_send(fd, "0\r\n\r\n", 5, 0) <= 0) return false;

    return true;
}

/* Smart send — uses chunked for large bodies, regular for small ones */
static void send_response_smart(nc_socket_t fd, int status,
                                const char *status_text,
                                const char *content_type,
                                const char *body, bool keep_alive) {
    int body_len = body ? (int)strlen(body) : 0;
    int threshold = nc_chunk_threshold();

    if (body_len > threshold) {
        send_chunked_response(fd, status, status_text, content_type,
                              body, body_len, keep_alive);
    } else {
        if (body_len < 0 || body_len > 100 * 1024 * 1024) return; /* bounds check */
        int resp_cap = body_len + 2048;
        char *response = malloc(resp_cap);
        if (!response) return;
        build_response_ct_ka(response, resp_cap, status, status_text,
                             content_type, body, keep_alive);
        nc_send(fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Route matching
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char method[8];
    char path[256];
    char handler[128];
} ServerRoute;

static bool path_matches(const char *pattern, const char *actual) {
    if (strcmp(pattern, actual) == 0) return true;
    int plen = (int)strlen(pattern);
    int alen = (int)strlen(actual);
    if (alen == plen + 1 && actual[alen - 1] == '/' &&
        strncmp(pattern, actual, plen) == 0) return true;
    if (plen == alen + 1 && pattern[plen - 1] == '/' &&
        strncmp(pattern, actual, alen) == 0) return true;
    return false;
}

static bool route_exists(ServerRoute *routes, int route_count,
                         const char *method, const char *path) {
    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].method, method) == 0 &&
            path_matches(routes[i].path, path)) {
            return true;
        }
    }
    return false;
}

static bool program_has_behavior(NcASTNode *program, const char *name) {
    if (!program || program->type != NODE_PROGRAM || !name) return false;
    for (int i = 0; i < program->as.program.beh_count; i++) {
        NcASTNode *beh = program->as.program.behaviors[i];
        if (beh && strcmp(beh->as.behavior.name->chars, name) == 0) {
            return true;
        }
    }
    return false;
}

static void nc_apply_config_env(NcMap *cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->count; i++) {
        if (IS_STRING(cfg->values[i])) {
            const char *val = AS_STRING(cfg->values[i])->chars;
            if (strncmp(val, "env:", 4) == 0) {
                const char *env_val = getenv(val + 4);
                if (env_val) {
                    NcString *resolved = nc_string_from_cstr(env_val);
                    cfg->values[i] = NC_STRING(resolved);
                }
            }
        }
    }
    for (int i = 0; i < cfg->count; i++) {
        if (IS_STRING(cfg->values[i]) || IS_INT(cfg->values[i])) {
            const char *key = cfg->keys[i]->chars;
            char env_name[128] = "NC_";
            int ei = 3;
            for (int k = 0; key[k] && ei < 126; k++) {
                env_name[ei++] = (key[k] >= 'a' && key[k] <= 'z')
                    ? key[k] - 32 : key[k];
            }
            env_name[ei] = '\0';
            if (!getenv(env_name)) {
                NcString *vs = nc_value_to_string(cfg->values[i]);
                nc_setenv(env_name, vs->chars, 0);
                nc_string_free(vs);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Request handler — runs NC behavior for matched route
 * ═══════════════════════════════════════════════════════════ */

static void handle_request(nc_socket_t client_fd, HttpRequest *req,
                           ServerRoute *routes, int route_count,
                           const char *source, const char *filename,
                           NcASTNode *program, const char *client_ip) {
    int resp_cap = nc_max_response();
    char *response = resp_pool_acquire(resp_cap);
    if (!response) {
        const char *oom = "HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\nConnection:close\r\n\r\n";
        nc_send(client_fd, oom, (int)strlen(oom), 0);
        return;
    }

    if (strcmp(req->method, "OPTIONS") == 0) {
        build_response(response, resp_cap, 204, "No Content", "");
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    const char *health_path = getenv("NC_HEALTH_PATH");
    if (!health_path) health_path = "/health";
    if (strcmp(req->path, health_path) == 0 &&
        !route_exists(routes, route_count, "GET", req->path)) {
        char body[512];
        snprintf(body, sizeof(body),
            "{\"status\":\"healthy\",\"version\":\"1.0.0\","
            "\"active_connections\":%d,\"total_requests\":%d}",
            nc_atomic_load(&nc_metrics_active_connections),
            nc_atomic_load(&nc_metrics_requests_total));
        build_response(response, resp_cap, 200, "OK", body);
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    /* Readiness probe — checks if the service can handle requests */
    if (strcmp(req->path, "/ready") == 0) {
        int workers = nc_atomic_load(&active_workers);
        const char *max_str = getenv("NC_MAX_WORKERS");
        int max_w = max_str ? atoi(max_str) : 16;
        if (workers < max_w) {
            build_response(response, resp_cap, 200, "OK",
                "{\"status\":\"ready\"}");
        } else {
            build_response(response, resp_cap, 503, "Service Unavailable",
                "{\"status\":\"not_ready\",\"reason\":\"all workers busy\"}");
        }
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    /* Liveness probe */
    if (strcmp(req->path, "/live") == 0) {
        build_response(response, resp_cap, 200, "OK",
            "{\"status\":\"alive\"}");
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    /* Prometheus-compatible metrics endpoint */
    if (strcmp(req->path, "/metrics") == 0) {
        char body[8192];
        int pos = 0;
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_requests_total Total HTTP requests.\n"
            "# TYPE nc_requests_total counter\n"
            "nc_requests_total %d\n",
            nc_atomic_load(&nc_metrics_requests_total));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_errors_total Total HTTP errors.\n"
            "# TYPE nc_errors_total counter\n"
            "nc_errors_total %d\n",
            nc_atomic_load(&nc_metrics_errors_total));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_active_connections Currently active connections.\n"
            "# TYPE nc_active_connections gauge\n"
            "nc_active_connections %d\n",
            nc_atomic_load(&nc_metrics_active_connections));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_active_workers Currently active worker threads.\n"
            "# TYPE nc_active_workers gauge\n"
            "nc_active_workers %d\n",
            nc_atomic_load(&active_workers));
        const char *max_str = getenv("NC_MAX_WORKERS");
        int max_w = max_str ? atoi(max_str) : NC_DEFAULT_MAX_WORKERS;
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_max_workers Maximum worker threads.\n"
            "# TYPE nc_max_workers gauge\n"
            "nc_max_workers %d\n", max_w);
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_queue_depth Current request queue depth.\n"
            "# TYPE nc_queue_depth gauge\n"
            "nc_queue_depth %d\n",
            nc_atomic_load(&nc_metrics_queue_depth));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_keepalive_reuse_total Connections reused via keep-alive.\n"
            "# TYPE nc_keepalive_reuse_total counter\n"
            "nc_keepalive_reuse_total %d\n",
            nc_atomic_load(&nc_metrics_keepalive_reuse));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_queue_full_rejects_total Requests rejected due to full queue.\n"
            "# TYPE nc_queue_full_rejects_total counter\n"
            "nc_queue_full_rejects_total %d\n",
            nc_atomic_load(&nc_metrics_queue_full_rejects));
        pos += snprintf(body + pos, sizeof(body) - pos,
            "# HELP nc_info NC runtime information.\n"
            "# TYPE nc_info gauge\n"
            "nc_info{version=\"1.0.0\",runtime=\"c\",concurrency=\"thread_pool\"} 1\n");
        int body_len = (int)strlen(body);
        snprintf(response, resp_cap,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n%s", body_len, body);
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    /* OpenAPI 3.1 specification endpoint */
    if (strcmp(req->path, "/openapi.json") == 0 ||
        strcmp(req->path, "/swagger.json") == 0) {
        char spec[8192];
        int sp = 0;
        sp += snprintf(spec + sp, sizeof(spec) - sp,
            "{\"openapi\":\"3.1.0\",\"info\":{\"title\":\"%s\",\"version\":\"%s\","
            "\"description\":\"Auto-generated OpenAPI specification\"},",
            program && program->as.program.service_name
                ? program->as.program.service_name->chars : "NC Service",
            program && program->as.program.version
                ? program->as.program.version->chars : "1.0.0");
        sp += snprintf(spec + sp, sizeof(spec) - sp,
            "\"servers\":[{\"url\":\"http://localhost:%d\"}],\"paths\":{",
            server_port > 0 ? server_port : 8080);
        for (int i = 0; i < route_count; i++) {
            if (i > 0) sp += snprintf(spec + sp, sizeof(spec) - sp, ",");
            char method_lower[8];
            for (int j = 0; routes[i].method[j] && j < 7; j++)
                method_lower[j] = routes[i].method[j] >= 'A' && routes[i].method[j] <= 'Z'
                    ? routes[i].method[j] + 32 : routes[i].method[j];
            method_lower[strlen(routes[i].method)] = '\0';
            sp += snprintf(spec + sp, sizeof(spec) - sp,
                "\"%s\":{\"%s\":{\"operationId\":\"%s\","
                "\"summary\":\"Runs %s behavior\","
                "\"responses\":{\"200\":{\"description\":\"Success\","
                "\"content\":{\"application/json\":{\"schema\":{\"type\":\"object\"}}}}}}}",
                routes[i].path, method_lower, routes[i].handler, routes[i].handler);
        }
        sp += snprintf(spec + sp, sizeof(spec) - sp,
            "},\"components\":{\"securitySchemes\":{"
            "\"bearerAuth\":{\"type\":\"http\",\"scheme\":\"bearer\",\"bearerFormat\":\"JWT\"},"
            "\"apiKeyAuth\":{\"type\":\"apiKey\",\"in\":\"header\",\"name\":\"X-Api-Key\"}}}}");
        build_response(response, resp_cap, 200, "OK", spec);
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    nc_atomic_inc(&nc_metrics_requests_total);

    const char *match_path = req->path;
    const char *base_path = getenv("NC_BASE_PATH");
    if (base_path && base_path[0]) {
        int blen = (int)strlen(base_path);
        if (strncmp(match_path, base_path, blen) == 0) {
            match_path = req->path + blen;
            if (match_path[0] == '\0') {
                match_path = "/";
            } else if (match_path[0] != '/' && match_path > req->path) {
                match_path--;
            } else if (match_path[0] != '/') {
                match_path = req->path;
            }
        }
    }

    const char *handler_name = NULL;
    for (int i = 0; i < route_count; i++) {
        if (strcmp(routes[i].method, req->method) == 0 &&
            path_matches(routes[i].path, match_path)) {
            handler_name = routes[i].handler;
            break;
        }
    }

    if (!handler_name) {
        /* Serve a discovery response on root path instead of 404 */
        if (strcmp(match_path, "/") == 0) {
            char root_body[4096];
            int rpos = snprintf(root_body, sizeof(root_body),
                "{\"status\":\"running\",\"routes\":[");
            for (int i = 0; i < route_count; i++) {
                rpos += snprintf(root_body + rpos, sizeof(root_body) - rpos,
                    "%s{\"method\":\"%s\",\"path\":\"%s\"}",
                    i > 0 ? "," : "", routes[i].method, routes[i].path);
            }
            snprintf(root_body + rpos, sizeof(root_body) - rpos, "]}");
            build_response(response, resp_cap, 200, "OK", root_body);
            nc_send(client_fd, response, (int)strlen(response), 0);
            resp_pool_release(response, resp_cap);
            return;
        }
        build_response(response, resp_cap, 404, "Not Found",
            "{\"error\":\"Not Found\"}");
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }

    NcMap *args = nc_map_new();
    if (!args) {
        build_response(response, resp_cap, 500, "Internal Server Error",
            "{\"error\":\"Memory allocation failed\"}");
        nc_send(client_fd, response, (int)strlen(response), 0);
        resp_pool_release(response, resp_cap);
        return;
    }
    if (req->body_len > 0) {
        NcValue body_val = nc_json_parse(req->body);
        if (IS_MAP(body_val)) {
            NcMap *body_map = AS_MAP(body_val);
            for (int i = 0; i < body_map->count; i++)
                nc_map_set(args, body_map->keys[i], body_map->values[i]);
        } else if (IS_NONE(body_val) && req->body[0] != '\0') {
            build_response(response, resp_cap, 400, "Bad Request",
                "{\"error\":\"The request body is not valid JSON. Make sure you're sending a JSON object like {\\\"key\\\": \\\"value\\\"}\"}");
            nc_send(client_fd, response, (int)strlen(response), 0);
            nc_map_free(args);
            resp_pool_release(response, resp_cap);
            return;
        }
    }

    NcMap *req_map = nc_map_new();
    nc_map_set(req_map, nc_string_from_cstr("method"),
               NC_STRING(nc_string_from_cstr(req->method)));
    nc_map_set(req_map, nc_string_from_cstr("path"),
               NC_STRING(nc_string_from_cstr(match_path)));
    nc_map_set(req_map, nc_string_from_cstr("body"),
               NC_STRING(nc_string_from_cstr(req->body)));
    nc_map_set(req_map, nc_string_from_cstr("query"),
               NC_STRING(nc_string_from_cstr(req->query)));
    nc_map_set(args, nc_string_from_cstr("request"), NC_MAP(req_map));

    if (req->query[0]) {
        nc_map_set(args, nc_string_from_cstr("query"),
                   NC_STRING(nc_string_from_cstr(req->query)));

        /* Parse query string into individual args: ?name=World&age=30
         * becomes args["name"] = "World", args["age"] = "30"
         * This lets behaviors use params directly:
         *   to greet with name:
         *     respond with "Hello " + name
         * Works with GET /greet?name=World */
        /* Dynamic allocation for query string to handle long URLs */
        int qlen = (int)strlen(req->query);
        char *qcopy = malloc(qlen + 1);
        if (!qcopy) goto skip_query_parse;
        memcpy(qcopy, req->query, qlen + 1);
        char *qp = qcopy;
        while (*qp) {
            char *eq = strchr(qp, '=');
            if (!eq) break;
            *eq = '\0';
            char *val = eq + 1;
            char *amp = strchr(val, '&');
            if (amp) *amp = '\0';

            /* URL-decode key and value (handle %XX and +) */
            char dk[256], dv[256];
            int di = 0;
            for (char *s = qp; *s && di < 255; s++) {
                if (*s == '+') dk[di++] = ' ';
                else if (*s == '%' && s[1] && s[2]) {
                    char hex[3] = { s[1], s[2], 0 };
                    dk[di++] = (char)strtol(hex, NULL, 16);
                    s += 2;
                } else dk[di++] = *s;
            }
            dk[di] = '\0';
            di = 0;
            for (char *s = val; *s && di < 255; s++) {
                if (*s == '+') dv[di++] = ' ';
                else if (*s == '%' && s[1] && s[2]) {
                    char hex[3] = { s[1], s[2], 0 };
                    dv[di++] = (char)strtol(hex, NULL, 16);
                    s += 2;
                } else dv[di++] = *s;
            }
            dv[di] = '\0';

            /* Only set if behavior doesn't already have this key from POST body */
            NcString *qkey = nc_string_from_cstr(dk);
            if (!nc_map_has(args, qkey))
                nc_map_set(args, qkey, NC_STRING(nc_string_from_cstr(dv)));
            else
                nc_string_free(qkey);

            if (amp) qp = amp + 1; else break;
        }
        free(qcopy);
    }
    skip_query_parse:

    if (req->auth[0]) {
        NcAuthContext auth_ctx = nc_mw_auth_check(req->auth);
        if (auth_ctx.authenticated) {
            NcMap *auth_map = nc_map_new();
            nc_map_set(auth_map, nc_string_from_cstr("user_id"),
                       NC_STRING(nc_string_from_cstr(auth_ctx.user_id)));
            nc_map_set(auth_map, nc_string_from_cstr("tenant_id"),
                       NC_STRING(nc_string_from_cstr(auth_ctx.tenant_id)));
            nc_map_set(auth_map, nc_string_from_cstr("role"),
                       NC_STRING(nc_string_from_cstr(auth_ctx.role)));
            nc_map_set(auth_map, nc_string_from_cstr("authenticated"),
                       NC_BOOL(true));
            nc_map_set(args, nc_string_from_cstr("auth"), NC_MAP(auth_map));
        }
    }

    if (program && program->as.program.mw_count > 0) {
        for (int mw = 0; mw < program->as.program.mw_count; mw++) {
            NcASTNode *mw_node = program->as.program.middleware[mw];
            const char *mw_name = mw_node->as.middleware.name->chars;

            if (strcmp(mw_name, "rate_limit") == 0 || strcmp(mw_name, "rate-limit") == 0) {
                /* Use authenticated user_id + role for per-user rate limiting.
                 * Falls back to IP-based limiting for unauthenticated requests. */
                const char *rl_user = NULL;
                const char *rl_role = NULL;
                if (req->auth[0]) {
                    NcAuthContext rl_ctx = nc_mw_auth_check(req->auth);
                    if (rl_ctx.authenticated) {
                        rl_user = rl_ctx.user_id;
                        rl_role = rl_ctx.role;
                    }
                }
                if (!nc_mw_rate_limit_check_user(client_ip, rl_user, rl_role)) {
                    const char *rl_key = (rl_user && rl_user[0]) ? rl_user : client_ip;
                    char rl_headers[256] = "";
                    nc_mw_rate_limit_get_headers(rl_key, rl_headers, sizeof(rl_headers));
                    char rl_body[512];
                    snprintf(rl_body, sizeof(rl_body),
                        "{\"error\":\"Rate limit exceeded. Please wait and try again.\","
                        "\"retry_after\":%d}", 60);
                    char rl_resp[2048];
                    snprintf(rl_resp, sizeof(rl_resp),
                        "HTTP/1.1 429 Too Many Requests\r\n"
                        "Content-Type: application/json\r\n"
                        "%s"
                        "Content-Length: %d\r\n\r\n%s",
                        rl_headers, (int)strlen(rl_body), rl_body);
                    nc_send(client_fd, rl_resp, (int)strlen(rl_resp), 0);
                    nc_map_free(args);
                    resp_pool_release(response, resp_cap);
                    return;
                }
            }

            if (strcmp(mw_name, "auth") == 0 || strcmp(mw_name, "authentication") == 0) {
                if (!req->auth[0]) {
                    build_response(response, resp_cap, 401,
                        "Unauthorized", "{\"error\":\"You need to provide an authentication token. Include an Authorization header with your request.\"}");
                    nc_send(client_fd, response, (int)strlen(response), 0);
                    nc_map_free(args);
                    resp_pool_release(response, resp_cap);
                    return;
                }
                NcAuthContext auth_ctx = nc_mw_auth_check(req->auth);
                if (!auth_ctx.authenticated) {
                    build_response(response, resp_cap, 403,
                        "Forbidden", "{\"error\":\"Your credentials are not valid. Check your API key or JWT token.\"}");
                    nc_send(client_fd, response, (int)strlen(response), 0);
                    nc_map_free(args);
                    resp_pool_release(response, resp_cap);
                    return;
                }
            }

            if (strcmp(mw_name, "cors") == 0) {
                /* CORS headers are added in the response builder */
            }
        }
    }

    bool is_stream = (strstr(match_path, "stream") != NULL);
    for (int i = 0; i < req->header_count && !is_stream; i++) {
        if (strcasecmp(req->headers[i][0], "Accept") == 0 &&
            strstr(req->headers[i][1], "text/event-stream"))
            is_stream = true;
    }

    if (is_stream) {
        const char *sse_cors = getenv("NC_CORS_ORIGIN");
        char sse_headers[512];
        if (sse_cors && sse_cors[0]) {
            snprintf(sse_headers, sizeof(sse_headers),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "Access-Control-Allow-Origin: %s\r\n"
                "\r\n", sse_cors);
        } else {
            snprintf(sse_headers, sizeof(sse_headers),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/event-stream\r\n"
                "Cache-Control: no-cache\r\n"
                "Connection: keep-alive\r\n"
                "\r\n");
        }
        nc_send(client_fd, sse_headers, (int)strlen(sse_headers), 0);

        const char *ack = "data: {\"type\":\"ack\",\"queue_ms\":0}\n\n";
        nc_send(client_fd, ack, (int)strlen(ack), 0);

        NcValue result = nc_call_behavior(source, filename, handler_name, args);

        char *result_json = nc_json_serialize(result, false);
        char *event = malloc(resp_cap);
        if (!event) { free(result_json); nc_map_free(args); resp_pool_release(response, resp_cap); return; }

        if (IS_STRING(result)) {
            snprintf(event, resp_cap,
                "data: {\"type\":\"content\",\"content\":%s}\n\n", result_json);
        } else {
            char *content = nc_json_serialize(result, false);
            snprintf(event, resp_cap,
                "data: {\"type\":\"content\",\"content\":%s}\n\n", content);
            free(content);
        }
        nc_send(client_fd, event, (int)strlen(event), 0);

        snprintf(event, resp_cap,
            "data: {\"type\":\"done\"}\n\n");
        nc_send(client_fd, event, (int)strlen(event), 0);

        free(event);
        free(result_json);
        nc_map_free(args);
        resp_pool_release(response, resp_cap);
        return;
    }

    if (program) {
        for (int b = 0; b < program->as.program.beh_count; b++) {
            NcASTNode *beh = program->as.program.behaviors[b];
            if (strcmp(beh->as.behavior.name->chars, handler_name) == 0) {
                for (int p = 0; p < beh->as.behavior.param_count; p++) {
                    NcString *param_name = beh->as.behavior.params[p]->as.param.name;
                    if (!nc_map_has(args, param_name)) {
                        NC_WARN("Missing param '%s' for behavior '%s'",
                            param_name->chars, handler_name);
                    }
                }
                break;
            }
        }
    }

    /* Set thread-local request context so behaviors can call
     * request_header(), request_ip(), request_method(), etc. */
    NcRequestContext req_ctx = {0};
    strncpy(req_ctx.client_ip, client_ip ? client_ip : "", 47);
    strncpy(req_ctx.method, req->method, 15);
    strncpy(req_ctx.path, req->path, 511);
    req_ctx.header_count = req->header_count < 64 ? req->header_count : 64;
    for (int i = 0; i < req_ctx.header_count; i++) {
        strncpy(req_ctx.headers[i][0], req->headers[i][0], 255);
        strncpy(req_ctx.headers[i][1], req->headers[i][1], 255);
    }
    nc_request_ctx_set(&req_ctx);

    /* Set request timeout deadline */
    const char *timeout_env = getenv("NC_REQUEST_TIMEOUT");
    int timeout_sec = (timeout_env && atoi(timeout_env) > 0) ? atoi(timeout_env) : 60;
    nc_set_request_deadline(nc_realtime_ms() + timeout_sec * 1000.0);

    NcValue result = nc_call_behavior(source, filename, handler_name, args);

    nc_set_request_deadline(0);
    nc_request_ctx_set(NULL);

    int http_status = 200;
    const char *http_status_text = "OK";
    if (IS_MAP(result)) {
        NcString *status_key = nc_string_from_cstr("_status");
        NcValue status_val = nc_map_get(AS_MAP(result), status_key);
        if (IS_INT(status_val)) {
            http_status = (int)AS_INT(status_val);
            switch (http_status) {
                case 200: http_status_text = "OK"; break;
                case 201: http_status_text = "Created"; break;
                case 204: http_status_text = "No Content"; break;
                case 400: http_status_text = "Bad Request"; break;
                case 401: http_status_text = "Unauthorized"; break;
                case 403: http_status_text = "Forbidden"; break;
                case 404: http_status_text = "Not Found"; break;
                case 409: http_status_text = "Conflict"; break;
                case 422: http_status_text = "Unprocessable Entity"; break;
                case 429: http_status_text = "Too Many Requests"; break;
                case 500: http_status_text = "Internal Server Error"; break;
                case 502: http_status_text = "Bad Gateway"; break;
                case 503: http_status_text = "Service Unavailable"; break;
                case 504: http_status_text = "Gateway Timeout"; break;
                default:  http_status_text = "Custom"; break;
            }
        }
        nc_string_free(status_key);
    }

    /* Check for string result — use both the macro and a direct type field check
     * to guard against the type tag being lost during behavior execution */
    if (IS_STRING(result) || result.type == VAL_STRING) {
        const char *raw_str = AS_STRING(result)->chars;
        if (raw_str) {
            const char *ct = detect_content_type(raw_str);
            if (strcmp(ct, "application/json") != 0) {
                build_response_ct(response, resp_cap, http_status, http_status_text, ct, raw_str);
                nc_send(client_fd, response, (int)strlen(response), 0);
                resp_pool_release(response, resp_cap);
                nc_map_free(args);
                return;
            }
        }
    }
    /* Also check if result is AI_RESULT type wrapping a string (some paths set this) */
    if (result.type == VAL_AI_RESULT && result.as.string && result.as.string->chars) {
        const char *raw_str = result.as.string->chars;
        const char *ct = detect_content_type(raw_str);
        if (strcmp(ct, "application/json") != 0) {
            build_response_ct(response, resp_cap, http_status, http_status_text, ct, raw_str);
            nc_send(client_fd, response, (int)strlen(response), 0);
            resp_pool_release(response, resp_cap);
            nc_map_free(args);
            return;
        }
    }

    char *result_json = nc_json_serialize(result, true);
    char *body = malloc(resp_cap);
    if (!body) { free(result_json); nc_map_free(args); resp_pool_release(response, resp_cap); return; }

    /* Fallback HTML detection: if the value is a JSON-encoded string
     * containing HTML (e.g. read_file returned HTML but the type tag
     * was lost during execution), unwrap the JSON string and serve raw. */
    if (!IS_MAP(result) && !IS_LIST(result) && result_json && result_json[0] == '"') {
        int rlen = (int)strlen(result_json);
        if (rlen > 2) {
            char *unquoted = malloc(rlen);
            if (unquoted) {
                int ui = 0;
                for (int ri = 1; ri < rlen - 1; ri++) {
                    if (result_json[ri] == '\\' && ri + 1 < rlen - 1) {
                        ri++;
                        switch (result_json[ri]) {
                            case 'n': unquoted[ui++] = '\n'; break;
                            case 'r': unquoted[ui++] = '\r'; break;
                            case 't': unquoted[ui++] = '\t'; break;
                            case '"': unquoted[ui++] = '"'; break;
                            case '\\': unquoted[ui++] = '\\'; break;
                            default: unquoted[ui++] = result_json[ri]; break;
                        }
                    } else {
                        unquoted[ui++] = result_json[ri];
                    }
                }
                unquoted[ui] = '\0';
                const char *ct = detect_content_type(unquoted);
                if (strcmp(ct, "application/json") != 0) {
                    build_response_ct(response, resp_cap, http_status, http_status_text, ct, unquoted);
                    nc_send(client_fd, response, (int)strlen(response), 0);
                    free(unquoted);
                    free(result_json);
                    free(body);
                    resp_pool_release(response, resp_cap);
                    nc_map_free(args);
                    return;
                }
                free(unquoted);
            }
        }
    }

    if (IS_MAP(result) || IS_LIST(result)) {
        snprintf(body, resp_cap, "%s", result_json);
    } else {
        /* Last-resort HTML detection: if result_json is a quoted string
         * that contains HTML, serve it raw instead of wrapping in JSON.
         * This handles the case where the VM loses the string type tag. */
        if (result_json && result_json[0] == '"') {
            int rjlen = (int)strlen(result_json);
            if (rjlen > 12) {
                /* Quick scan: look for <!doctype or <html inside the JSON string */
                const char *scan = result_json + 1;
                while (*scan == ' ' || *scan == '\\') {
                    if (*scan == '\\' && (*(scan+1) == 'n' || *(scan+1) == 'r' || *(scan+1) == 't'))
                        scan += 2;
                    else if (*scan == ' ')
                        scan++;
                    else
                        break;
                }
                if (strncasecmp(scan, "<!doctype", 9) == 0 || strncasecmp(scan, "<html", 5) == 0) {
                    /* Decode the JSON string to raw HTML */
                    char *raw = malloc(rjlen);
                    if (raw) {
                        int wi = 0;
                        for (int ri = 1; ri < rjlen - 1; ri++) {
                            if (result_json[ri] == '\\' && ri + 1 < rjlen - 1) {
                                ri++;
                                switch (result_json[ri]) {
                                    case 'n': raw[wi++] = '\n'; break;
                                    case 'r': raw[wi++] = '\r'; break;
                                    case 't': raw[wi++] = '\t'; break;
                                    case '"': raw[wi++] = '"'; break;
                                    case '\\': raw[wi++] = '\\'; break;
                                    default: raw[wi++] = result_json[ri]; break;
                                }
                            } else {
                                raw[wi++] = result_json[ri];
                            }
                        }
                        raw[wi] = '\0';
                        build_response_ct(response, resp_cap, 200, "OK",
                            "text/html; charset=utf-8", raw);
                        nc_send(client_fd, response, (int)strlen(response), 0);
                        free(raw);
                        free(result_json);
                        free(body);
                        resp_pool_release(response, resp_cap);
                        nc_map_free(args);
                        return;
                    }
                }
            }
        }
        snprintf(body, resp_cap,
            "{\"status\":\"ok\",\"data\":%s}", result_json);
    }
    free(result_json);

    /* Extract custom headers from _headers map key */
    char custom_headers[2048] = "";
    if (IS_MAP(result)) {
        NcString *hdr_key = nc_string_from_cstr("_headers");
        NcValue hdr_val = nc_map_get(AS_MAP(result), hdr_key);
        nc_string_free(hdr_key);
        if (IS_MAP(hdr_val)) {
            NcMap *hdr_map = AS_MAP(hdr_val);
            int hpos = 0;
            for (int hi = 0; hi < hdr_map->count && hpos < (int)sizeof(custom_headers) - 256; hi++) {
                NcString *hv = nc_value_to_string(hdr_map->values[hi]);
                /* Strip CR/LF from header keys and values to prevent response splitting */
                const char *key = hdr_map->keys[hi]->chars;
                const char *val = hv->chars;
                bool safe = true;
                for (const char *c = key; *c; c++) { if (*c == '\r' || *c == '\n') { safe = false; break; } }
                for (const char *c = val; *c && safe; c++) { if (*c == '\r' || *c == '\n') { safe = false; break; } }
                if (safe) {
                    hpos += snprintf(custom_headers + hpos, sizeof(custom_headers) - hpos,
                        "%s: %s\r\n", key, val);
                }
                nc_string_free(hv);
            }
        }
    }

    if (custom_headers[0]) {
        const char *cors_origin = getenv("NC_CORS_ORIGIN");
        int body_len = body ? (int)strlen(body) : 0;
        int threshold = nc_chunk_threshold();

        if (body_len > threshold) {
            /* Large body with custom headers — use chunked encoding */
            char hdr[4096];
            int hlen = snprintf(hdr, sizeof(hdr),
                "HTTP/1.1 %d %s\r\n"
                "Content-Type: application/json\r\n"
                "Transfer-Encoding: chunked\r\n"
                "%s"
                "Cache-Control: no-cache\r\n"
                "%s%s%s"
                "Connection: close\r\n"
                "\r\n",
                http_status, http_status_text,
                custom_headers,
                cors_origin ? "Access-Control-Allow-Origin: " : "",
                cors_origin ? cors_origin : "",
                cors_origin ? "\r\n" : "");
            nc_send(client_fd, hdr, hlen, 0);

            /* Send body in chunks */
            int chunk_sz = nc_chunk_size();
            int offset = 0;
            while (offset < body_len) {
                int remaining = body_len - offset;
                int this_chunk = remaining < chunk_sz ? remaining : chunk_sz;
                char chunk_hdr[32];
                int ch_len = snprintf(chunk_hdr, sizeof(chunk_hdr), "%x\r\n", this_chunk);
                nc_send(client_fd, chunk_hdr, ch_len, 0);
                nc_send(client_fd, body + offset, this_chunk, 0);
                nc_send(client_fd, "\r\n", 2, 0);
                offset += this_chunk;
            }
            nc_send(client_fd, "0\r\n\r\n", 5, 0);
        } else {
            snprintf(response, resp_cap,
                "HTTP/1.1 %d %s\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %d\r\n"
                "%s"
                "Cache-Control: no-cache\r\n"
                "%s%s%s"
                "Connection: close\r\n"
                "\r\n"
                "%s",
                http_status, http_status_text, body_len,
                custom_headers,
                cors_origin ? "Access-Control-Allow-Origin: " : "",
                cors_origin ? cors_origin : "",
                cors_origin ? "\r\n" : "",
                body ? body : "");
            nc_send(client_fd, response, (int)strlen(response), 0);
        }
    } else {
        int body_len = body ? (int)strlen(body) : 0;
        int threshold = nc_chunk_threshold();

        if (body_len > threshold) {
            send_response_smart(client_fd, http_status, http_status_text,
                                "application/json", body, false);
        } else {
            build_response(response, resp_cap, http_status, http_status_text, body);
            nc_send(client_fd, response, (int)strlen(response), 0);
        }
    }
    free(body);
    resp_pool_release(response, resp_cap);
    nc_map_free(args);
}

/* ═══════════════════════════════════════════════════════════
 *  Thread pool worker — persistent threads pulling from
 *  the bounded request queue. Supports HTTP/1.1 keep-alive:
 *  after processing a request, the worker reads more requests
 *  on the same connection until close or timeout.
 * ═══════════════════════════════════════════════════════════ */

/* Shared server context — set once at startup, read by all workers */
typedef struct {
    ServerRoute *routes;
    int          route_count;
    const char  *source;
    const char  *filename;
    NcASTNode   *program;
} ServerCtx;

static ServerCtx server_ctx;

/* Process a single HTTP request on a connection. Returns true if keep-alive
 * should continue, false if the connection should be closed. */
static bool process_one_request(nc_socket_t client_fd, char *raw, int nread,
                                const char *client_ip) {
    int max_req = nc_max_request();
    HttpRequest req;
    memset(&req, 0, sizeof(req));
    req.body = malloc(max_req);
    req.body_cap = max_req;
    if (!req.body) { free(raw); return false; }
    req.body[0] = '\0';

    bool should_keepalive = false;

    if (parse_request(raw, nread, &req)) {
        /* Basic input validation */
        if (req.path[0] == '\0' || strlen(req.path) > 500) {
            char resp[256];
            build_response(resp, sizeof(resp), 400, "Bad Request",
                "{\"error\":\"Invalid request path\"}");
            nc_send(client_fd, resp, (int)strlen(resp), 0);
            free(req.body);
            return false;
        }

        /* Reject path traversal in URL (including URL-encoded variants) */
        if (strstr(req.path, "..") != NULL ||
            strstr(req.path, "%2e%2e") != NULL || strstr(req.path, "%2E%2E") != NULL ||
            strstr(req.path, "%2e.") != NULL || strstr(req.path, ".%2e") != NULL ||
            strstr(req.path, "%2E.") != NULL || strstr(req.path, ".%2E") != NULL) {
            char resp[256];
            build_response(resp, sizeof(resp), 400, "Bad Request",
                "{\"error\":\"Path traversal not allowed\"}");
            nc_send(client_fd, resp, (int)strlen(resp), 0);
            free(req.body);
            return false;
        }

        /* WebSocket upgrade detection */
        bool is_ws_upgrade = false;
        char ws_key[128] = "";
        for (int i = 0; i < req.header_count; i++) {
            if (strcasecmp(req.headers[i][0], "Upgrade") == 0 &&
                strcasecmp(req.headers[i][1], "websocket") == 0)
                is_ws_upgrade = true;
            if (strcasecmp(req.headers[i][0], "Sec-WebSocket-Key") == 0)
                strncpy(ws_key, req.headers[i][1], 127);
        }

        if (is_ws_upgrade && ws_key[0]) {
            char accept_key[64];
            nc_ws_compute_accept(ws_key, accept_key, sizeof(accept_key));
            char ws_accept[512];
            snprintf(ws_accept, sizeof(ws_accept),
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: %s\r\n"
                "\r\n", accept_key);
            nc_send(client_fd, ws_accept, (int)strlen(ws_accept), 0);

            nc_ws_send((int)client_fd, "{\"type\":\"connected\",\"engine\":\"NC\"}");
            printf("  WS %s -> 101 Upgrade\n", req.path);

            char ws_buf[4096];
            nc_set_socket_timeout(client_fd, nc_get_socket_timeout());
            time_t ws_start = time(NULL);
            int ws_max_idle_pings = 3;
            int ws_idle_pings = 0;
            int ws_max_lifetime = 3600;

            while (server_running) {
                if (time(NULL) - ws_start > ws_max_lifetime) {
                    nc_ws_send((int)client_fd, "{\"type\":\"close\",\"reason\":\"max_lifetime\"}");
                    break;
                }
                int n = nc_ws_read((int)client_fd, ws_buf, sizeof(ws_buf) - 1);
                if (n <= 0) {
                    ws_idle_pings++;
                    if (ws_idle_pings >= ws_max_idle_pings) break;
                    nc_ws_send((int)client_fd, "{\"type\":\"ping\"}");
                    n = nc_ws_read((int)client_fd, ws_buf, sizeof(ws_buf) - 1);
                    if (n <= 0) break;
                }
                ws_idle_pings = 0;
                ws_buf[n] = '\0';

                NcValue msg = nc_json_parse(ws_buf);
                if (IS_MAP(msg)) {
                    NcString *type_key = nc_string_from_cstr("type");
                    NcValue type_val = nc_map_get(AS_MAP(msg), type_key);
                    nc_string_free(type_key);

                    if (IS_STRING(type_val)) {
                        const char *mtype = AS_STRING(type_val)->chars;
                        if (strcmp(mtype, "ping") == 0) {
                            nc_ws_send((int)client_fd, "{\"type\":\"pong\"}");
                        } else if (strcmp(mtype, "subscribe") == 0) {
                            nc_ws_send((int)client_fd, "{\"type\":\"subscribed\"}");
                        } else {
                            char ack[256];
                            snprintf(ack, sizeof(ack),
                                "{\"type\":\"ack\",\"received\":\"%s\"}", mtype);
                            nc_ws_send((int)client_fd, ack);
                        }
                    }
                }
            }
            free(req.body);
            return false; /* WebSocket — don't keep-alive after upgrade */
        }

        /* Trace context */
        char trace_id[33] = "", span_id[17] = "", parent_span[17] = "";
        bool has_trace = false;
        for (int i = 0; i < req.header_count; i++) {
            if (strcasecmp(req.headers[i][0], "traceparent") == 0) {
                const char *tp = req.headers[i][1];
                if (strlen(tp) >= 55) {
                    memcpy(trace_id, tp + 3, 32); trace_id[32] = '\0';
                    memcpy(parent_span, tp + 36, 16); parent_span[16] = '\0';
                    has_trace = true;
                }
            }
        }
        if (!has_trace) {
            unsigned long r1 = (unsigned long)time(NULL) ^ (unsigned long)clock();
            unsigned long r2 = r1 * 6364136223846793005ULL + 1;
            snprintf(trace_id, sizeof(trace_id), "%016lx%016lx", r1, r2);
        }
        unsigned long s = (unsigned long)clock() ^ (unsigned long)time(NULL);
        snprintf(span_id, sizeof(span_id), "%016lx", s);

        double t_start = nc_clock_ms();

        handle_request(client_fd, &req, server_ctx.routes, server_ctx.route_count,
                       server_ctx.source, server_ctx.filename, server_ctx.program,
                       client_ip);

        double duration_ms = nc_clock_ms() - t_start;
        nc_atomic_inc(&nc_metrics_requests_latency_sum);

        /* Redact query string from logged path to avoid leaking tokens/PII */
        char safe_path[512];
        strncpy(safe_path, req.path, sizeof(safe_path) - 1);
        safe_path[sizeof(safe_path) - 1] = '\0';
        char *qmark = strchr(safe_path, '?');
        if (qmark) {
            qmark[0] = '\0';
            strncat(safe_path, "?[REDACTED]", sizeof(safe_path) - strlen(safe_path) - 1);
        }

        const char *log_fmt = getenv("NC_LOG_FORMAT");
        if (log_fmt && strcmp(log_fmt, "json") == 0) {
            time_t now = time(NULL);
            char ts[32];
            strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
            printf("{\"timestamp\":\"%s\",\"method\":\"%s\",\"path\":\"%s\","
                   "\"status\":200,\"duration_ms\":%.1f,"
                   "\"trace_id\":\"%s\",\"span_id\":\"%s\",\"parent_span_id\":\"%s\","
                   "\"keep_alive\":%s,\"engine\":\"nc\"}\n",
                   ts, req.method, safe_path, duration_ms,
                   trace_id, span_id, parent_span,
                   req.keep_alive ? "true" : "false");
        } else {
            printf("  %s %s -> 200 (%.1fms)%s [trace:%s]\n",
                   req.method, safe_path, duration_ms,
                   req.keep_alive ? " [ka]" : "", trace_id);
        }

        const char *otel_ep = getenv("NC_OTEL_ENDPOINT");
        if (otel_ep && otel_ep[0]) {
            time_t now_t = time(NULL);
            char span_json[2048];
            snprintf(span_json, sizeof(span_json),
                "{\"resourceSpans\":[{\"resource\":{\"attributes\":["
                "{\"key\":\"service.name\",\"value\":{\"stringValue\":\"%s\"}}"
                "]},\"scopeSpans\":[{\"spans\":[{"
                "\"traceId\":\"%s\",\"spanId\":\"%s\","
                "\"parentSpanId\":\"%s\","
                "\"name\":\"%s %s\","
                "\"kind\":2,"
                "\"startTimeUnixNano\":%ld000000000,"
                "\"endTimeUnixNano\":%ld000000000,"
                "\"status\":{\"code\":1}"
                "}]}]}]}",
                server_ctx.program->as.program.service_name ?
                    server_ctx.program->as.program.service_name->chars : "unknown-service",
                trace_id, span_id, parent_span,
                req.method, safe_path,
                (long)now_t, (long)now_t);

            char otel_url[512];
            snprintf(otel_url, sizeof(otel_url), "%s/v1/traces", otel_ep);
            char *resp = nc_http_post(otel_url, span_json,
                "application/json", NULL);
            free(resp);
        }

        should_keepalive = req.keep_alive;
    }
    free(req.body);
    return should_keepalive;
}

/* Thread pool worker — long-lived thread that processes requests from the queue.
 * Supports HTTP/1.1 keep-alive: after processing a request, the worker reads
 * more requests on the same connection until close, timeout, or max reuse. */
#ifdef NC_WINDOWS
static unsigned __stdcall pool_worker_thread(void *arg) {
#else
static void *pool_worker_thread(void *arg) {
#endif
    (void)arg;
    int max_req = nc_max_request();
    int ka_max = nc_keepalive_max();
    int ka_timeout_sec = nc_keepalive_timeout();

    while (!rq_shutdown) {
        QueuedRequest qr;
        if (!rq_dequeue(&qr)) break; /* shutdown signal */

        nc_atomic_inc(&active_workers);
        nc_atomic_inc(&nc_metrics_active_connections);

        nc_socket_t client_fd = qr.client_fd;
        char *raw = qr.raw;
        int nread = qr.raw_len;
        int requests_on_conn = 0;

        /* Keep-alive loop: process multiple requests on the same connection */
        while (server_running && requests_on_conn < ka_max) {
            bool ka = process_one_request(client_fd, raw, nread, qr.client_ip);
            raw = NULL; /* ownership transferred to process_one_request or freed */
            requests_on_conn++;

            if (!ka) break; /* client sent Connection: close or error */

            nc_atomic_inc(&nc_metrics_keepalive_reuse);

            /* Wait for next request on the same connection */
            nc_set_socket_timeout(client_fd, ka_timeout_sec);
            raw = malloc(max_req);
            if (!raw) break;
            nread = (int)nc_recv(client_fd, raw, max_req - 1, 0);
            if (nread <= 0) { free(raw); raw = NULL; break; }
            raw[nread] = '\0';

            /* Read full body if Content-Length present */
            const char *cl_hdr = strstr(raw, "Content-Length:");
            if (!cl_hdr) cl_hdr = strstr(raw, "content-length:");
            if (cl_hdr) {
                int content_len = atoi(cl_hdr + 15);
                const char *body_sep = strstr(raw, "\r\n\r\n");
                if (body_sep && content_len > 0) {
                    int header_len = (int)(body_sep + 4 - raw);
                    int body_have = nread - header_len;
                    int body_need = content_len - body_have;
                    while (body_need > 0 && nread < max_req - 1) {
                        int chunk = (int)nc_recv(client_fd, raw + nread,
                                                 max_req - 1 - nread, 0);
                        if (chunk <= 0) break;
                        nread += chunk;
                        raw[nread] = '\0';
                        body_need -= chunk;
                    }
                }
            }
        }

        nc_closesocket(client_fd);
        nc_atomic_dec(&active_workers);
        nc_atomic_dec(&nc_metrics_active_connections);
    }

#ifdef NC_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Scheduler thread — runs periodic tasks
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *source;
    const char *filename;
    NcASTNode  *program;
    int         interval_ms;
} SchedulerCtx;

static int parse_interval_ms(const char *interval) {
    if (!interval) return 60000;
    int value = 0;
    const char *p = interval;
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        p++;
    }
    if (value <= 0) return 60000;
    while (*p == ' ') p++;
    if (strncmp(p, "second", 6) == 0) return value * 1000;
    if (strncmp(p, "minute", 6) == 0) return value * 60 * 1000;
    if (strncmp(p, "hour", 4) == 0)   return value * 3600 * 1000;
    return value * 60 * 1000;
}

#ifdef NC_WINDOWS
static unsigned __stdcall scheduler_thread(void *arg) {
#else
static void *scheduler_thread(void *arg) {
#endif
    SchedulerCtx *ctx = (SchedulerCtx *)arg;
    int sleep_ms = ctx->interval_ms > 0 ? ctx->interval_ms : 60000;
    /* Use shorter poll intervals for sub-minute schedules */
    int poll_ms = sleep_ms < 1000 ? sleep_ms : (sleep_ms < 10000 ? 1000 : 5000);
    int elapsed = 0;
    while (server_running) {
        nc_sleep_ms(poll_ms);
        elapsed += poll_ms;
        if (elapsed >= sleep_ms) {
            elapsed = 0;
            for (int ev = 0; ev < ctx->program->as.program.event_count; ev++) {
                NcASTNode *sch = ctx->program->as.program.events[ev];
                if (sch->type == NODE_SCHEDULE_HANDLER) {
                    /* Execute the schedule handler's body statements directly.
                     * Also try __schedule__ behavior as fallback for compatibility. */
                    NcValue r = nc_call_behavior(ctx->source, ctx->filename, "__schedule__", NULL);
                    if (IS_NONE(r)) {
                        /* No __schedule__ behavior found — try running each
                         * behavior referenced in the schedule body */
                        for (int s = 0; s < sch->as.schedule_handler.body_count; s++) {
                            NcASTNode *stmt = sch->as.schedule_handler.body[s];
                            if (stmt->type == NODE_RUN) {
                                const char *bname = stmt->as.run_stmt.name->chars;
                                nc_call_behavior(ctx->source, ctx->filename, bname, NULL);
                            }
                        }
                    }
                    (void)r;
                }
            }
        }
    }
    free(ctx);
#ifdef NC_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Server main loop
 * ═══════════════════════════════════════════════════════════ */

int nc_serve(const char *filename, int port) {
    nc_socket_init();

    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) { fclose(f); fprintf(stderr, "Error: Empty or unreadable file\n"); return 1; }
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    if (!source) { fclose(f); fprintf(stderr, "Error: Out of memory\n"); return 1; }
    size_t nread = fread(source, 1, size, f);
    source[nread] = '\0';
    fclose(f);
    if (nread == 0) { free(source); fprintf(stderr, "Error: Failed to read file\n"); return 1; }

    NcLexer *lex = nc_lexer_new(source, filename);
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
    NcASTNode *program = nc_parser_parse(parser);

    if (parser->had_error) {
        fprintf(stderr, "[NC] Parse error: %s\n", parser->error_msg);
        nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return 1;
    }

    if (program->as.program.configure)
        nc_apply_config_env(program->as.program.configure);

    if (port <= 0) {
        const char *env_port = getenv("NC_SERVICE_PORT");
        if (env_port && env_port[0]) port = atoi(env_port);
    }
    if (port <= 0 && program->as.program.configure) {
        NcString *port_key = nc_string_from_cstr("port");
        NcValue port_val = nc_map_get(program->as.program.configure, port_key);
        nc_string_free(port_key);
        if (IS_INT(port_val)) {
            port = (int)AS_INT(port_val);
        } else if (IS_STRING(port_val)) {
            const char *ps = AS_STRING(port_val)->chars;
            if (strncmp(ps, "env:", 4) == 0) {
                const char *ev = getenv(ps + 4);
                if (ev) port = atoi(ev);
            } else {
                port = atoi(ps);
            }
        }
    }
    if (port <= 0) port = 8000;
    server_port = port;

    ServerRoute routes[64];
    int route_count = 0;
    for (int i = 0; i < program->as.program.route_count && route_count < 64; i++) {
        NcASTNode *r = program->as.program.routes[i];
        strncpy(routes[route_count].method, r->as.route.method->chars, 7);
        strncpy(routes[route_count].path, r->as.route.path->chars, 255);
        strncpy(routes[route_count].handler, r->as.route.handler->chars, 127);
        route_count++;
    }

    nc_socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == NC_INVALID_SOCKET) { perror("socket"); free(source); return 1; }

    int opt = 1;
    nc_setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    const char *bind_host = getenv("NC_BIND_HOST");
    if (!bind_host || !bind_host[0]) bind_host = "0.0.0.0";

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (strcmp(bind_host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else if (strcmp(bind_host, "127.0.0.1") == 0 || strcmp(bind_host, "localhost") == 0) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind_host = "127.0.0.1";
    } else if (inet_pton(AF_INET, bind_host, &addr.sin_addr) != 1) {
        fprintf(stderr, "bind: unsupported NC_BIND_HOST '%s' (IPv4 only)\n", bind_host);
        nc_closesocket(server_fd); free(source);
        return 1;
    }
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        nc_closesocket(server_fd); free(source);
        return 1;
    }

    if (listen(server_fd, nc_listen_backlog()) < 0) {
        perror("listen");
        nc_closesocket(server_fd); free(source);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#ifndef NC_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif

    if (program->as.program.configure)
        nc_middleware_setup(program->as.program.configure);

    if (!nc_server_globals_get()) {
        NcMap *server_globals = nc_map_new();
        if (server_globals) nc_server_globals_set(server_globals);
    }
    if (program_has_behavior(program, "init"))
        nc_call_behavior(source, filename, "init", nc_server_globals_get());

    /* Initialize enterprise subsystems */
    nc_oidc_init();
    nc_ff_init();
    nc_secret_init();
    nc_api_version_init();
    nc_connpool_init();

    /* TLS configuration — check configure block and env vars.
     * When tls_cert + tls_key are set, the server terminates TLS directly.
     * Otherwise, use a reverse proxy for TLS termination. */
    const char *tls_cert = getenv("NC_TLS_CERT");
    const char *tls_key = getenv("NC_TLS_KEY");
    if (program->as.program.configure) {
        NcString *cert_k = nc_string_from_cstr("tls_cert");
        NcValue cert_v = nc_map_get(program->as.program.configure, cert_k);
        nc_string_free(cert_k);
        if (IS_STRING(cert_v) && !tls_cert) tls_cert = AS_STRING(cert_v)->chars;

        NcString *key_k = nc_string_from_cstr("tls_key");
        NcValue key_v = nc_map_get(program->as.program.configure, key_k);
        nc_string_free(key_k);
        if (IS_STRING(key_v) && !tls_key) tls_key = AS_STRING(key_v)->chars;
    }
    bool tls_enabled = (tls_cert && tls_cert[0] && tls_key && tls_key[0]);
    if (tls_enabled) {
        NC_INFO("TLS enabled: cert=%s", tls_cert);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n");
    printf("  \033[36m _  _  ___\033[0m\n");
    printf("  \033[36m| \\| |/ __|\033[0m   \033[1m\033[32mServer Running\033[0m\n");
    printf("  \033[36m| .` | (__\033[0m    \033[90mNotation-as-Code v1.0.0\033[0m\n");
    printf("  \033[36m|_|\\_|\\___|\033[0m\n");
    printf("\n");
    printf("  \033[36m+------------------------------------------+\033[0m\n");
    if (program->as.program.service_name)
        printf("  \033[36m|\033[0m  \033[1mService:\033[0m  %-29s\033[36m|\033[0m\n", program->as.program.service_name->chars);
    if (program->as.program.version)
        printf("  \033[36m|\033[0m  \033[1mVersion:\033[0m  \033[33m%-29s\033[0m\033[36m|\033[0m\n", program->as.program.version->chars);
    printf("  \033[36m|\033[0m  \033[1mPort:\033[0m     %-29d\033[36m|\033[0m\n", port);
    printf("  \033[36m|\033[0m  \033[1mHost:\033[0m     %-29s\033[36m|\033[0m\n", bind_host);
    printf("  \033[36m|\033[0m  \033[1mURL:\033[0m      \033[4m\033[36m%s://%s:%-13d\033[0m\033[36m|\033[0m\n",
           tls_enabled ? "https" : "http", bind_host, port);
    printf("  \033[36m+------------------------------------------+\033[0m\n");
    printf("  \033[36m|\033[0m  \033[1mRoutes:\033[0m                                  \033[36m|\033[0m\n");
    for (int i = 0; i < route_count; i++)
        printf("  \033[36m|\033[0m    \033[33m%-6s\033[0m %-20s \033[90m->\033[0m %-8s\033[36m|\033[0m\n",
               routes[i].method, routes[i].path, routes[i].handler);
    printf("  \033[36m+------------------------------------------+\033[0m\n");
    printf("\n  \033[90mPress \033[0m\033[33mCtrl+C\033[0m\033[90m to stop.\033[0m\n\n");

    const char *max_workers_str = getenv("NC_MAX_WORKERS");
    int max_workers = max_workers_str ? atoi(max_workers_str) : nc_cpu_count() * 2;
    if (max_workers < 1) max_workers = 1;
    if (max_workers > NC_DEFAULT_MAX_WORKERS) max_workers = NC_DEFAULT_MAX_WORKERS;

    nc_atomic_store(&active_workers, 0);

    int backlog = nc_listen_backlog();
    int queue_cap = NC_DEFAULT_REQUEST_QUEUE;
    const char *queue_str = getenv("NC_REQUEST_QUEUE_SIZE");
    if (queue_str) { int v = atoi(queue_str); if (v > 0) queue_cap = v; }

    printf("  Workers:    %d (thread pool)\n", max_workers);
    printf("  Backlog:    %d\n", backlog);
    printf("  Queue:      %d\n", queue_cap);
    printf("  Keep-alive: timeout=%ds, max=%d requests\n",
           nc_keepalive_timeout(), nc_keepalive_max());

    /* Initialize shared server context for thread pool workers */
    server_ctx.routes = routes;
    server_ctx.route_count = route_count;
    server_ctx.source = source;
    server_ctx.filename = filename;
    server_ctx.program = program;

    /* Start scheduler thread for periodic tasks */
    if (program->as.program.event_count > 0) {
        int sched_interval_ms = 60000;
        for (int ev = 0; ev < program->as.program.event_count; ev++) {
            NcASTNode *sch = program->as.program.events[ev];
            if (sch->type == NODE_SCHEDULE_HANDLER && sch->as.schedule_handler.interval) {
                sched_interval_ms = parse_interval_ms(sch->as.schedule_handler.interval->chars);
                printf("  Schedule: every %s (%dms)\n",
                       sch->as.schedule_handler.interval->chars, sched_interval_ms);
            }
        }

        SchedulerCtx *sctx = malloc(sizeof(SchedulerCtx));
        sctx->source = source;
        sctx->filename = filename;
        sctx->program = program;
        sctx->interval_ms = sched_interval_ms;
        nc_thread_t sched;
        nc_thread_create(&sched, (nc_thread_func_t)scheduler_thread, sctx);
        nc_thread_detach(sched);
    }

    printf("\n");

    /* ── Initialize thread pool and request queue ─────────────
     * Pre-spawn worker threads that consume from a bounded queue.
     * This eliminates thread-per-request overhead (creation/destruction
     * of OS threads per connection) and provides backpressure. */
    rq_init(queue_cap);

    nc_thread_t *pool = malloc(max_workers * sizeof(nc_thread_t));
    for (int i = 0; i < max_workers; i++) {
        if (nc_thread_create(&pool[i], (nc_thread_func_t)pool_worker_thread, NULL) != 0) {
            fprintf(stderr, "  WARNING: Could not create worker thread %d\n", i);
        }
    }

    /* ── Accept loop with epoll/kqueue I/O multiplexing ───────
     * Uses the platform's most efficient I/O notification mechanism:
     *   Linux:  epoll   (O(1) per event, no fd limit)
     *   macOS:  kqueue  (O(1) per event, no fd limit)
     *   Other:  select  (fallback, 1024 fd limit)
     *
     * The accept loop reads the first request in the main thread
     * then enqueues it for a pool worker. If the queue is full,
     * the connection gets a 503 with a short timeout. */
#if defined(NC_USE_EPOLL)
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); goto shutdown; }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);
#elif defined(NC_USE_KQUEUE)
    int kqfd = kqueue();
    if (kqfd < 0) { perror("kqueue"); goto shutdown; }
    struct kevent kev;
    EV_SET(&kev, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kqfd, &kev, 1, NULL, 0, NULL);
#endif

    while (server_running) {
#if defined(NC_USE_EPOLL)
        struct epoll_event events[64];
        int nev = epoll_wait(epfd, events, 64, 1000);
        if (nev < 0) { if (!server_running) break; continue; }
        for (int e = 0; e < nev; e++) {
            if (events[e].data.fd != server_fd) continue;
#elif defined(NC_USE_KQUEUE)
        struct kevent events[64];
        struct timespec kq_timeout = { .tv_sec = 1, .tv_nsec = 0 };
        int nev = kevent(kqfd, NULL, 0, events, 64, &kq_timeout);
        if (nev < 0) { if (!server_running) break; continue; }
        for (int e = 0; e < nev; e++) {
            if ((int)events[e].ident != server_fd) continue;
#else
        /* Fallback: select() with 1-second timeout for shutdown check */
        fd_set accept_fds;
        FD_ZERO(&accept_fds);
        FD_SET(server_fd, &accept_fds);
        struct timeval accept_tv = { .tv_sec = 1, .tv_usec = 0 };
        int sel = select((int)server_fd + 1, &accept_fds, NULL, NULL, &accept_tv);
        if (sel <= 0) continue;
        {
#endif
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            nc_socket_t client_fd = accept(server_fd,
                (struct sockaddr *)&client_addr, &client_len);
            if (client_fd == NC_INVALID_SOCKET) continue;

            /* Set TCP_NODELAY for lower latency on small responses */
            int nodelay = 1;
            nc_setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

            /* Read first request data in main thread */
            nc_set_socket_timeout(client_fd, nc_get_socket_timeout());
            int max_req = nc_max_request();
            char *raw = malloc(max_req);
            if (!raw) { nc_closesocket(client_fd); continue; }
            int nr = (int)nc_recv(client_fd, raw, max_req - 1, 0);
            if (nr <= 0) { free(raw); nc_closesocket(client_fd); continue; }
            raw[nr] = '\0';

            /* Read full body if Content-Length present */
            const char *cl_hdr = strstr(raw, "Content-Length:");
            if (!cl_hdr) cl_hdr = strstr(raw, "content-length:");
            if (cl_hdr) {
                int content_len = atoi(cl_hdr + 15);
                const char *body_sep = strstr(raw, "\r\n\r\n");
                if (body_sep && content_len > 0) {
                    int header_len = (int)(body_sep + 4 - raw);
                    int body_have = nr - header_len;
                    int body_need = content_len - body_have;
                    while (body_need > 0 && nr < max_req - 1) {
                        int chunk = (int)nc_recv(client_fd, raw + nr,
                                                 max_req - 1 - nr, 0);
                        if (chunk <= 0) break;
                        nr += chunk;
                        raw[nr] = '\0';
                        body_need -= chunk;
                    }
                }
            }

            /* Enqueue for pool worker with bounded wait */
            QueuedRequest qr;
            qr.client_fd = client_fd;
            qr.raw = raw;
            qr.raw_len = nr;
            inet_ntop(AF_INET, &client_addr.sin_addr,
                      qr.client_ip, sizeof(qr.client_ip));

            int enqueue_timeout_ms = 5000;
            const char *eq_env = getenv("NC_QUEUE_TIMEOUT");
            if (eq_env) { int v = atoi(eq_env); if (v > 0) enqueue_timeout_ms = v; }

            if (!rq_enqueue(&qr, enqueue_timeout_ms)) {
                /* Queue full — return 503 with Retry-After header */
                nc_atomic_inc(&nc_metrics_queue_full_rejects);
                char busy_resp[512];
                snprintf(busy_resp, sizeof(busy_resp),
                    "HTTP/1.1 503 Service Unavailable\r\n"
                    "Content-Type: application/json\r\n"
                    "Retry-After: 5\r\n"
                    "Connection: close\r\n"
                    "Content-Length: 62\r\n"
                    "\r\n"
                    "{\"error\":\"Server busy\",\"retry_after\":5,\"queue_full\":true}");
                nc_send(client_fd, busy_resp, (int)strlen(busy_resp), 0);
                nc_closesocket(client_fd);
                free(raw);
            }
        }
    }

    /* ── Graceful shutdown with connection draining ────────────
     * 1. Stop accepting new connections (server_running = false)
     * 2. Signal request queue shutdown to wake all workers
     * 3. Wait for in-flight requests to complete (up to drain timeout)
     * 4. Join all thread pool workers */
#if defined(NC_USE_EPOLL) || defined(NC_USE_KQUEUE)
shutdown:
#endif
    printf("\n  Initiating graceful shutdown...\n");

    /* Signal all pool workers to stop */
    rq_shutdown = true;
    nc_cond_broadcast(&rq_not_empty);

    int remaining = nc_atomic_load(&active_workers);
    if (remaining > 0) {
        printf("  Draining %d active request(s)...\n", remaining);
        int drain_timeout_ms = 30000;
        const char *drain_env = getenv("NC_DRAIN_TIMEOUT");
        if (drain_env && atoi(drain_env) > 0) drain_timeout_ms = atoi(drain_env) * 1000;

        int waited = 0;
        while (nc_atomic_load(&active_workers) > 0 && waited < drain_timeout_ms) {
            nc_sleep_ms(200);
            waited += 200;
            int now_remaining = nc_atomic_load(&active_workers);
            if (now_remaining != remaining) {
                printf("  %d request(s) still in flight...\n", now_remaining);
                remaining = now_remaining;
            }
        }

        int final = nc_atomic_load(&active_workers);
        if (final > 0) {
            printf("  WARNING: %d request(s) did not complete within drain timeout.\n", final);
        }
    }

    /* Join thread pool workers */
    for (int i = 0; i < max_workers; i++) {
        nc_cond_broadcast(&rq_not_empty); /* wake any sleeping workers */
        nc_thread_join(pool[i]);
    }
    free(pool);

    rq_destroy();

#if defined(NC_USE_EPOLL)
    close(epfd);
#elif defined(NC_USE_KQUEUE)
    close(kqfd);
#endif

    NC_INFO("server shutdown complete");
    printf("\n  Server stopped.\n\n");
    nc_closesocket(server_fd);
    nc_socket_cleanup();
    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);
    return 0;
}
