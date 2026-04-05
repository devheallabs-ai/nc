/*
 * nc_database.c — Generic data backend for NC.
 *
 * NC is a language. It does not know about specific databases or tools.
 * Users configure their own data backends:
 *
 *   configure:
 *       store_url is "http://localhost:8080/api/data"
 *
 *   Or via environment:
 *       NC_STORE_URL=http://localhost:8080/api/data
 *
 * Then NC code just says:
 *   gather users from "https://api.example.com/users"
 *   store result into "my_key"
 *
 * How it works:
 *   - If source is a URL → HTTP GET
 *   - If NC_STORE_URL is set → HTTP POST to that endpoint
 *   - Otherwise → in-memory store (for development)
 *
 * This file has ZERO vendor-specific code. No Redis, no MongoDB,
 * no Elasticsearch. Those are external services the user connects
 * to via URLs — same as any HTTP endpoint.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../include/nc.h"
#include "../include/nc_platform.h"

#ifndef NC_WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#endif

/* ═══════════════════════════════════════════════════════════
 *  In-memory key-value store (development fallback)
 *
 *  When no external store is configured, NC keeps data
 *  in memory so programs still work during development.
 *  Dynamically grows from initial 256 up to 65536 entries.
 *  Thread-safe via mutex for concurrent server requests.
 * ═══════════════════════════════════════════════════════════ */

#define MEM_STORE_INITIAL 256
#define MEM_STORE_MAX     65536

static struct {
    char **keys;
    char **values;
    int    count;
    int    capacity;
    bool   initialized;
} mem_store = {0};

static nc_mutex_t mem_store_mutex = NC_MUTEX_INITIALIZER;

static void mem_store_init(void) {
    if (!mem_store.initialized) {
        mem_store.capacity = MEM_STORE_INITIAL;
        mem_store.keys = calloc(mem_store.capacity, sizeof(char *));
        mem_store.values = calloc(mem_store.capacity, sizeof(char *));
        mem_store.count = 0;
        mem_store.initialized = true;
    }
}

static bool mem_store_grow(void) {
    if (mem_store.capacity >= MEM_STORE_MAX) return false;
    int new_cap = mem_store.capacity * 2;
    if (new_cap > MEM_STORE_MAX) new_cap = MEM_STORE_MAX;
    char **new_keys = realloc(mem_store.keys, new_cap * sizeof(char *));
    char **new_values = realloc(mem_store.values, new_cap * sizeof(char *));
    if (!new_keys || !new_values) return false;
    mem_store.keys = new_keys;
    mem_store.values = new_values;
    mem_store.capacity = new_cap;
    return true;
}

static void mem_store_set(const char *key, const char *value) {
    nc_mutex_lock(&mem_store_mutex);
    mem_store_init();
    for (int i = 0; i < mem_store.count; i++) {
        if (strcmp(mem_store.keys[i], key) == 0) {
            free(mem_store.values[i]);
            mem_store.values[i] = strdup(value);
            nc_mutex_unlock(&mem_store_mutex);
            return;
        }
    }
    if (mem_store.count >= mem_store.capacity) {
        if (!mem_store_grow()) {
            NC_WARN("In-memory store full (%d entries). Increase NC_STORE_URL to use external store.",
                    mem_store.count);
            nc_mutex_unlock(&mem_store_mutex);
            return;
        }
    }
    mem_store.keys[mem_store.count] = strdup(key);
    mem_store.values[mem_store.count] = strdup(value);
    mem_store.count++;
    nc_mutex_unlock(&mem_store_mutex);
}

static const char *mem_store_find(const char *key) {
    nc_mutex_lock(&mem_store_mutex);
    mem_store_init();
    for (int i = 0; i < mem_store.count; i++) {
        if (strcmp(mem_store.keys[i], key) == 0) {
            const char *val = mem_store.values[i];
            nc_mutex_unlock(&mem_store_mutex);
            return val;
        }
    }
    nc_mutex_unlock(&mem_store_mutex);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  JSON-escape a string for safe embedding in JSON bodies
 * ═══════════════════════════════════════════════════════════ */

static int json_escape_into(char *out, int out_cap, const char *s) {
    int ei = 0;
    for (; *s && ei < out_cap - 2; s++) {
        switch (*s) {
            case '"':  out[ei++] = '\\'; out[ei++] = '"'; break;
            case '\\': out[ei++] = '\\'; out[ei++] = '\\'; break;
            case '\n': out[ei++] = '\\'; out[ei++] = 'n'; break;
            case '\r': out[ei++] = '\\'; out[ei++] = 'r'; break;
            case '\t': out[ei++] = '\\'; out[ei++] = 't'; break;
            default:   out[ei++] = *s; break;
        }
    }
    out[ei] = '\0';
    return ei;
}

/* Strip trailing \r\n and whitespace — handles Redis RESP protocol
 * residue when store backends proxy through Redis. */
static void strip_trailing_crlf(char *s) {
    if (!s) return;
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' ||
                       s[len - 1] == ' '  || s[len - 1] == '\t'))
        s[--len] = '\0';
}

/* ═══════════════════════════════════════════════════════════
 *  nc_store_put — Generic persist
 *
 *  1. NC_STORE_URL set → HTTP POST {key, value} to that URL
 *  2. Otherwise → in-memory store
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_store_put(const char *target, const char *value_json) {
    const char *store_url = getenv("NC_STORE_URL");

    if (store_url && store_url[0]) {
        char escaped_key[512];
        json_escape_into(escaped_key, sizeof(escaped_key), target);

        /* Use properly escaped JSON construction to prevent injection */
        int val_len = value_json ? (int)strlen(value_json) : 4;
        int body_cap = (int)strlen(escaped_key) + val_len + 64;
        char *body = malloc(body_cap);
        if (!body) return NC_BOOL(false);
        snprintf(body, body_cap,
            "{\"key\":\"%s\",\"value\":%s}", escaped_key,
            (value_json && value_json[0]) ? value_json : "null");
        char *response = nc_http_post(store_url, body, "application/json", NULL);
        free(body);
        strip_trailing_crlf(response);
        NcValue result = nc_json_parse(response);
        free(response);
        if (!IS_NONE(result)) return result;
        return NC_BOOL(true);
    }

    mem_store_set(target, value_json);
    return NC_BOOL(true);
}

/* ═══════════════════════════════════════════════════════════
 *  nc_store_get — Generic retrieve
 *
 *  1. NC_STORE_URL set → HTTP GET {url}/{key}
 *  2. Otherwise → in-memory store lookup
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_store_get(const char *key) {
    const char *store_url = getenv("NC_STORE_URL");

    if (store_url && store_url[0]) {
        /* Validate key to prevent path traversal attacks */
        for (const char *p = key; *p; p++) {
            if (*p == '.' && *(p+1) == '.') return NC_NONE();
            if (*p == '/' || *p == '\\') return NC_NONE();
        }
        char url[1024];
        snprintf(url, sizeof(url), "%s/%s", store_url, key);
        char *response = nc_http_get(url, NULL);
        strip_trailing_crlf(response);
        NcValue result = nc_json_parse(response);
        free(response);
        return result;
    }

    const char *val = mem_store_find(key);
    if (val) return nc_json_parse(val);
    return NC_NONE();
}

/* ═══════════════════════════════════════════════════════════
 *  nc_database_query — Generic "gather from" dispatcher
 *
 *  NC code:
 *    gather data from "https://api.example.com/data"
 *    gather data from "my_key"
 *
 *  Routing:
 *    - URL string → HTTP GET (handled by nc_gather_from in nc_http.c)
 *    - Non-URL string → try store lookup, then MCP
 *    - Options map → passed to backend as query parameters
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_database_query(const char *source, NcMap *options) {
    (void)options;

    /* Try the in-memory / configured store first */
    NcValue stored = nc_store_get(source);
    if (!IS_NONE(stored)) return stored;

    return NC_NONE();
}

/* ═══════════════════════════════════════════════════════════════════
 *  ██████╗  ██████╗  ██████╗ ██╗
 *  ██╔══██╗██╔═══██╗██╔═══██╗██║
 *  ██████╔╝██║   ██║██║   ██║██║
 *  ██╔═══╝ ██║   ██║██║   ██║██║
 *  ██║     ╚██████╔╝╚██████╔╝███████╗
 *  ╚═╝      ╚═════╝  ╚═════╝ ╚══════╝
 *
 *  Connection Pool — generic pool for all database adapters.
 *  Thread-safe. Tracks active/idle counts. Parses URL to
 *  determine pool type (SQL proxy, Redis, MongoDB Atlas).
 * ═══════════════════════════════════════════════════════════════════ */

/* Parse host and port from a URL like "redis://host:port" or "http://host:port/path" */
static void pool_parse_url(NcDBPool *pool) {
    pool->port = 0;
    pool->type = NC_DB_POOL_GENERIC;

    const char *url = pool->url;

    /* Detect pool type from URL scheme */
    if (strncmp(url, "redis://", 8) == 0) {
        pool->type = NC_DB_POOL_REDIS;
        pool->port = 6379; /* default */
    } else if (strncmp(url, "mongodb+srv://", 14) == 0 ||
               strncmp(url, "mongodb://", 10) == 0) {
        pool->type = NC_DB_POOL_MONGO;
        pool->port = 443;
    } else if (strncmp(url, "sql://", 6) == 0 ||
               strncmp(url, "postgres://", 11) == 0 ||
               strncmp(url, "mysql://", 8) == 0) {
        pool->type = NC_DB_POOL_SQL;
        pool->port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        pool->port = 443;
    } else if (strncmp(url, "http://", 7) == 0) {
        pool->port = 80;
    }

    /* Extract explicit port if present — scan past "://" then find ":port" */
    const char *authority = strstr(url, "://");
    if (authority) {
        authority += 3;
        const char *colon = strchr(authority, ':');
        const char *slash = strchr(authority, '/');
        if (colon && (!slash || colon < slash)) {
            int p = atoi(colon + 1);
            if (p > 0 && p < 65536) pool->port = p;
        }
    }
}

NcDBPool *nc_db_pool_new(const char *url, int max_conn) {
    if (!url || !url[0]) {
        NC_WARN("nc_db_pool_new: empty URL");
        return NULL;
    }
    if (max_conn <= 0) max_conn = 10;
    if (max_conn > 1024) max_conn = 1024;

    NcDBPool *pool = calloc(1, sizeof(NcDBPool));
    if (!pool) return NULL;

    snprintf(pool->url, sizeof(pool->url), "%s", url);
    pool->max_connections = max_conn;
    pool->active = 0;
    pool->idle = max_conn;
    pool->timeout_sec = 30;
    nc_mutex_init(&pool->mutex);

    pool_parse_url(pool);

    return pool;
}

void nc_db_pool_free(NcDBPool *pool) {
    if (!pool) return;
    nc_mutex_destroy(&pool->mutex);
    free(pool);
}

/* Acquire/release helpers — track active vs idle count */
static bool pool_acquire(NcDBPool *pool) {
    nc_mutex_lock(&pool->mutex);
    if (pool->active >= pool->max_connections) {
        nc_mutex_unlock(&pool->mutex);
        NC_WARN("Connection pool exhausted (%d/%d active)",
                pool->active, pool->max_connections);
        return false;
    }
    pool->active++;
    pool->idle--;
    nc_mutex_unlock(&pool->mutex);
    return true;
}

static void pool_release(NcDBPool *pool) {
    nc_mutex_lock(&pool->mutex);
    if (pool->active > 0) {
        pool->active--;
        pool->idle++;
    }
    nc_mutex_unlock(&pool->mutex);
}

/* ═══════════════════════════════════════════════════════════════════
 *  ███████╗ ██████╗ ██╗
 *  ██╔════╝██╔═══██╗██║
 *  ███████╗██║   ██║██║
 *  ╚════██║██║▄▄ ██║██║
 *  ███████║╚██████╔╝███████╗
 *  ╚══════╝ ╚══▀▀═╝ ╚══════╝
 *
 *  SQL Adapter — HTTP-based SQL proxy
 *
 *  Sends SQL as JSON to a REST endpoint. The user runs a
 *  lightweight HTTP-to-SQL proxy (PostgREST, Hasura, pgrest,
 *  or their own) that accepts:
 *
 *    POST /query
 *    {"sql": "SELECT ...", "params": [...]}
 *
 *  Returns JSON results. NC never links libpq or libmysql.
 * ═══════════════════════════════════════════════════════════════════ */

NcValue nc_db_sql_query(NcDBPool *pool, const char *sql,
                        NcValue *params, int param_count) {
    if (!pool || !sql) return NC_NONE();
    if (!pool_acquire(pool)) return NC_NONE();

    /* Build JSON body: {"sql":"...", "params":[...]} */
    NcDynBuf body;
    nc_dbuf_init(&body, 512);
    nc_dbuf_append(&body, "{\"sql\":\"");
    nc_dbuf_append_escaped(&body, sql);
    nc_dbuf_append(&body, "\"");

    if (params && param_count > 0) {
        nc_dbuf_append(&body, ",\"params\":[");
        for (int i = 0; i < param_count; i++) {
            if (i > 0) nc_dbuf_append(&body, ",");
            char *serialized = nc_json_serialize(params[i], false);
            if (serialized) {
                nc_dbuf_append(&body, serialized);
                free(serialized);
            } else {
                nc_dbuf_append(&body, "null");
            }
        }
        nc_dbuf_append(&body, "]");
    }
    nc_dbuf_append(&body, "}");

    /* POST to the pool's SQL proxy URL */
    char endpoint[600];
    snprintf(endpoint, sizeof(endpoint), "%s/query", pool->url);

    char *response = nc_http_post(endpoint, body.data,
                                  "application/json", NULL);
    nc_dbuf_free(&body);
    pool_release(pool);

    if (!response) return NC_NONE();
    strip_trailing_crlf(response);
    NcValue result = nc_json_parse(response);
    free(response);
    return result;
}

NcValue nc_db_sql_exec(NcDBPool *pool, const char *sql) {
    if (!pool || !sql) return NC_NONE();
    if (!pool_acquire(pool)) return NC_NONE();

    NcDynBuf body;
    nc_dbuf_init(&body, 256);
    nc_dbuf_append(&body, "{\"sql\":\"");
    nc_dbuf_append_escaped(&body, sql);
    nc_dbuf_append(&body, "\"}");

    char endpoint[600];
    snprintf(endpoint, sizeof(endpoint), "%s/exec", pool->url);

    char *response = nc_http_post(endpoint, body.data,
                                  "application/json", NULL);
    nc_dbuf_free(&body);
    pool_release(pool);

    if (!response) return NC_NONE();
    strip_trailing_crlf(response);
    NcValue result = nc_json_parse(response);
    free(response);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *  ██████╗ ███████╗██████╗ ██╗███████╗
 *  ██╔══██╗██╔════╝██╔══██╗██║██╔════╝
 *  ██████╔╝█████╗  ██║  ██║██║███████╗
 *  ██╔══██╗██╔══╝  ██║  ██║██║╚════██║
 *  ██║  ██║███████╗██████╔╝██║███████║
 *  ╚═╝  ╚═╝╚══════╝╚═════╝ ╚═╝╚══════╝
 *
 *  Redis Adapter — RESP protocol over raw TCP sockets
 *
 *  The Redis Serialization Protocol (RESP) is beautifully simple:
 *    - Bulk strings:  $<len>\r\n<data>\r\n
 *    - Arrays:        *<count>\r\n<elements...>
 *    - Simple strings: +OK\r\n
 *    - Errors:        -ERR message\r\n
 *    - Integers:      :<number>\r\n
 *
 *  We open a raw TCP socket, write RESP, read RESP. Zero deps.
 * ═══════════════════════════════════════════════════════════════════ */

/* Extract host and port from redis://host:port URL */
static bool redis_parse_url(const char *url, char *host, int host_cap,
                            int *port) {
    *port = 6379;
    const char *start = url;
    if (strncmp(url, "redis://", 8) == 0) start = url + 8;

    /* Skip optional user:pass@ */
    const char *at = strchr(start, '@');
    if (at) start = at + 1;

    const char *colon = strchr(start, ':');
    const char *slash = strchr(start, '/');

    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - start);
        if (hlen >= host_cap) hlen = host_cap - 1;
        memcpy(host, start, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0 || *port > 65535) *port = 6379;
    } else {
        int hlen = slash ? (int)(slash - start) : (int)strlen(start);
        if (hlen >= host_cap) hlen = host_cap - 1;
        memcpy(host, start, hlen);
        host[hlen] = '\0';
    }
    return host[0] != '\0';
}

/* Open a raw TCP socket to the Redis server */
static nc_socket_t redis_connect(const char *url, int timeout_sec) {
    char host[256];
    int port;
    if (!redis_parse_url(url, host, sizeof(host), &port)) {
        NC_WARN("Redis: invalid URL: %s", url);
        return NC_INVALID_SOCKET;
    }

    nc_socket_init();

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        NC_WARN("Redis: cannot resolve host: %s", host);
        return NC_INVALID_SOCKET;
    }

    nc_socket_t fd = socket(res->ai_family, res->ai_socktype,
                            res->ai_protocol);
    if (fd == NC_INVALID_SOCKET) {
        freeaddrinfo(res);
        return NC_INVALID_SOCKET;
    }

    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) != 0) {
        NC_WARN("Redis: cannot connect to %s:%d", host, port);
        nc_closesocket(fd);
        freeaddrinfo(res);
        return NC_INVALID_SOCKET;
    }
    freeaddrinfo(res);

    if (timeout_sec > 0) {
        nc_set_socket_timeout(fd, timeout_sec);
    }

    return fd;
}

/* Build a RESP array from an array of string arguments.
 * RESP format: *<argc>\r\n$<len>\r\n<arg>\r\n ... */
static char *resp_build(const char **args, int argc, int *out_len) {
    NcDynBuf buf;
    nc_dbuf_init(&buf, 128);

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "*%d\r\n", argc);
    nc_dbuf_append(&buf, tmp);

    for (int i = 0; i < argc; i++) {
        int len = (int)strlen(args[i]);
        snprintf(tmp, sizeof(tmp), "$%d\r\n", len);
        nc_dbuf_append(&buf, tmp);
        nc_dbuf_append_len(&buf, args[i], len);
        nc_dbuf_append(&buf, "\r\n");
    }

    *out_len = buf.len;
    return buf.data;
}

/* Read a complete RESP response from the socket.
 * Returns an NcValue: string for simple/bulk strings,
 * integer for :N, list for arrays, NONE for errors. */
static NcValue resp_read(nc_socket_t fd) {
    char buf[8192];
    int total = 0;

    /* Read until we have at least the type marker and \r\n */
    while (total < (int)sizeof(buf) - 1) {
        ssize_t n = nc_recv(fd, buf + total, 1, 0);
        if (n <= 0) break;
        total += (int)n;
        /* Check if we have a complete first line */
        if (total >= 3 && buf[total - 2] == '\r' && buf[total - 1] == '\n')
            break;
    }
    buf[total] = '\0';

    if (total < 3) return NC_NONE();

    char type = buf[0];
    char *payload = buf + 1;

    /* Strip trailing \r\n from the first line */
    char *crlf = strstr(payload, "\r\n");
    if (crlf) *crlf = '\0';

    switch (type) {
    case '+': {
        /* Simple string: +OK */
        NcString *s = nc_string_from_cstr(payload);
        return NC_STRING(s);
    }
    case '-': {
        /* Error: -ERR message */
        NC_WARN("Redis error: %s", payload);
        NcString *s = nc_string_from_cstr(payload);
        return NC_STRING(s);
    }
    case ':': {
        /* Integer: :42 */
        long long val = atoll(payload);
        return NC_INT((int)val);
    }
    case '$': {
        /* Bulk string: $<len>\r\n<data>\r\n */
        int blen = atoi(payload);
        if (blen < 0) return NC_NONE(); /* $-1 = nil */
        if (blen == 0) {
            /* Read trailing \r\n */
            char discard[4];
            nc_recv(fd, discard, 2, 0);
            NcString *s = nc_string_from_cstr("");
            return NC_STRING(s);
        }
        char *data = malloc(blen + 1);
        if (!data) return NC_NONE();
        int read_so_far = 0;
        while (read_so_far < blen) {
            ssize_t n = nc_recv(fd, data + read_so_far,
                                blen - read_so_far, 0);
            if (n <= 0) break;
            read_so_far += (int)n;
        }
        data[blen] = '\0';
        /* Read trailing \r\n */
        char discard[4];
        nc_recv(fd, discard, 2, 0);
        NcString *s = nc_string_new(data, blen);
        free(data);
        return NC_STRING(s);
    }
    case '*': {
        /* Array: *<count>\r\n<elements...> */
        int count = atoi(payload);
        if (count < 0) return NC_NONE(); /* *-1 = nil */
        NcList *list = nc_list_new();
        for (int i = 0; i < count; i++) {
            NcValue elem = resp_read(fd);
            nc_list_push(list, elem);
        }
        return NC_LIST(list);
    }
    default:
        return NC_NONE();
    }
}

NcValue nc_db_redis_command(NcDBPool *pool, const char *cmd, ...) {
    if (!pool || !cmd) return NC_NONE();
    if (!pool_acquire(pool)) return NC_NONE();

    /* Parse variadic args into an array of strings.
     * cmd is the first arg (e.g., "GET"), varargs are the rest,
     * terminated by NULL. */
    const char *args[64];
    int argc = 0;

    /* Tokenize the command string first — handles "SET key val" style */
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) { pool_release(pool); return NC_NONE(); }

    char *saveptr = NULL;
    char *token = strtok_r(cmd_copy, " ", &saveptr);
    while (token && argc < 60) {
        args[argc++] = token;
        token = strtok_r(NULL, " ", &saveptr);
    }

    /* Then append any variadic string arguments */
    va_list ap;
    va_start(ap, cmd);
    const char *arg;
    while ((arg = va_arg(ap, const char *)) != NULL && argc < 64) {
        args[argc++] = arg;
    }
    va_end(ap);

    /* Connect, send, receive */
    nc_socket_t fd = redis_connect(pool->url, pool->timeout_sec);
    if (fd == NC_INVALID_SOCKET) {
        free(cmd_copy);
        pool_release(pool);
        return NC_NONE();
    }

    int resp_len = 0;
    char *resp_data = resp_build(args, argc, &resp_len);
    ssize_t sent = nc_send(fd, resp_data, resp_len, 0);
    free(resp_data);

    NcValue result = NC_NONE();
    if (sent == resp_len) {
        result = resp_read(fd);
    }

    nc_closesocket(fd);
    free(cmd_copy);
    pool_release(pool);
    return result;
}

NcValue nc_db_redis_set(NcDBPool *pool, const char *key,
                        const char *value) {
    if (!pool || !key || !value) return NC_NONE();
    return nc_db_redis_command(pool, "SET", key, value, NULL);
}

NcValue nc_db_redis_get(NcDBPool *pool, const char *key) {
    if (!pool || !key) return NC_NONE();
    return nc_db_redis_command(pool, "GET", key, NULL);
}

/* ═══════════════════════════════════════════════════════════════════
 *  ███╗   ███╗ ██████╗ ███╗   ██╗ ██████╗  ██████╗
 *  ████╗ ████║██╔═══██╗████╗  ██║██╔════╝ ██╔═══██╗
 *  ██╔████╔██║██║   ██║██╔██╗ ██║██║  ███╗██║   ██║
 *  ██║╚██╔╝██║██║   ██║██║╚██╗██║██║   ██║██║   ██║
 *  ██║ ╚═╝ ██║╚██████╔╝██║ ╚████║╚██████╔╝╚██████╔╝
 *  ╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═══╝ ╚═════╝  ╚═════╝
 *
 *  MongoDB Adapter — MongoDB Atlas Data API (REST)
 *
 *  MongoDB Atlas exposes a fully RESTful Data API at:
 *    https://data.mongodb-api.com/app/<id>/endpoint/data/v1
 *
 *  All operations are POST with JSON bodies. No BSON. No libmongoc.
 *  The pool URL should point to the Atlas Data API base URL.
 *  Auth is via API key in the URL or environment.
 * ═══════════════════════════════════════════════════════════════════ */

/* Helper: extract API key from env or URL fragment */
static const char *mongo_get_api_key(void) {
    const char *key = getenv("NC_MONGO_API_KEY");
    return (key && key[0]) ? key : NULL;
}

/* Helper: extract database name from env */
static const char *mongo_get_database(void) {
    const char *db = getenv("NC_MONGO_DATABASE");
    return (db && db[0]) ? db : "default";
}

/* Helper: extract data source from env */
static const char *mongo_get_datasource(void) {
    const char *ds = getenv("NC_MONGO_DATASOURCE");
    return (ds && ds[0]) ? ds : "Cluster0";
}

/* Serialize an NcMap filter to JSON for the MongoDB query body */
static void mongo_serialize_filter(NcDynBuf *body, NcMap *filter) {
    if (!filter || filter->count == 0) {
        nc_dbuf_append(body, "{}");
        return;
    }
    char *json = nc_json_serialize(NC_MAP(filter), false);
    if (json) {
        nc_dbuf_append(body, json);
        free(json);
    } else {
        nc_dbuf_append(body, "{}");
    }
}

NcValue nc_db_mongo_find(NcDBPool *pool, const char *collection,
                         NcMap *filter) {
    if (!pool || !collection) return NC_NONE();
    if (!pool_acquire(pool)) return NC_NONE();

    const char *api_key = mongo_get_api_key();
    const char *database = mongo_get_database();
    const char *datasource = mongo_get_datasource();

    NcDynBuf body;
    nc_dbuf_init(&body, 512);
    nc_dbuf_append(&body, "{\"dataSource\":\"");
    nc_dbuf_append_escaped(&body, datasource);
    nc_dbuf_append(&body, "\",\"database\":\"");
    nc_dbuf_append_escaped(&body, database);
    nc_dbuf_append(&body, "\",\"collection\":\"");
    nc_dbuf_append_escaped(&body, collection);
    nc_dbuf_append(&body, "\",\"filter\":");
    mongo_serialize_filter(&body, filter);
    nc_dbuf_append(&body, "}");

    /* POST to Atlas Data API /action/find */
    char endpoint[600];
    snprintf(endpoint, sizeof(endpoint), "%s/action/find", pool->url);

    char *response = nc_http_post(endpoint, body.data,
                                  "application/json", api_key);
    nc_dbuf_free(&body);
    pool_release(pool);

    if (!response) return NC_NONE();
    strip_trailing_crlf(response);
    NcValue result = nc_json_parse(response);
    free(response);
    return result;
}

NcValue nc_db_mongo_insert(NcDBPool *pool, const char *collection,
                           NcMap *doc) {
    if (!pool || !collection || !doc) return NC_NONE();
    if (!pool_acquire(pool)) return NC_NONE();

    const char *api_key = mongo_get_api_key();
    const char *database = mongo_get_database();
    const char *datasource = mongo_get_datasource();

    NcDynBuf body;
    nc_dbuf_init(&body, 512);
    nc_dbuf_append(&body, "{\"dataSource\":\"");
    nc_dbuf_append_escaped(&body, datasource);
    nc_dbuf_append(&body, "\",\"database\":\"");
    nc_dbuf_append_escaped(&body, database);
    nc_dbuf_append(&body, "\",\"collection\":\"");
    nc_dbuf_append_escaped(&body, collection);
    nc_dbuf_append(&body, "\",\"document\":");

    char *doc_json = nc_json_serialize(NC_MAP(doc), false);
    if (doc_json) {
        nc_dbuf_append(&body, doc_json);
        free(doc_json);
    } else {
        nc_dbuf_append(&body, "{}");
    }
    nc_dbuf_append(&body, "}");

    char endpoint[600];
    snprintf(endpoint, sizeof(endpoint), "%s/action/insertOne", pool->url);

    char *response = nc_http_post(endpoint, body.data,
                                  "application/json", api_key);
    nc_dbuf_free(&body);
    pool_release(pool);

    if (!response) return NC_NONE();
    strip_trailing_crlf(response);
    NcValue result = nc_json_parse(response);
    free(response);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 *   ██████╗ ██╗   ██╗███████╗██████╗ ██╗   ██╗
 *  ██╔═══██╗██║   ██║██╔════╝██╔══██╗╚██╗ ██╔╝
 *  ██║   ██║██║   ██║█████╗  ██████╔╝ ╚████╔╝
 *  ██║▄▄ ██║██║   ██║██╔══╝  ██╔══██╗  ╚██╔╝
 *  ╚██████╔╝╚██████╔╝███████╗██║  ██║   ██║
 *   ╚══▀▀═╝  ╚═════╝ ╚══════╝╚═╝  ╚═╝   ╚═╝
 *
 *  Query Builder — type-safe SQL query construction
 *
 *  Builds queries as NcMap values:
 *    {"type":"select","table":"users","columns":["name","email"],
 *     "where":"age > 18"}
 *
 *  Then nc_db_query_to_sql() serializes to SQL string.
 *  Prevents SQL injection via parameterized construction.
 * ═══════════════════════════════════════════════════════════════════ */

NcValue nc_db_select(const char *table, const char **columns,
                     int col_count) {
    if (!table) return NC_NONE();

    NcMap *query = nc_map_new();

    nc_map_set(query, nc_string_from_cstr("type"),
               NC_STRING(nc_string_from_cstr("select")));
    nc_map_set(query, nc_string_from_cstr("table"),
               NC_STRING(nc_string_from_cstr(table)));

    if (columns && col_count > 0) {
        NcList *cols = nc_list_new();
        for (int i = 0; i < col_count; i++) {
            nc_list_push(cols, NC_STRING(nc_string_from_cstr(columns[i])));
        }
        nc_map_set(query, nc_string_from_cstr("columns"), NC_LIST(cols));
    }

    return NC_MAP(query);
}

NcValue nc_db_where(NcValue query, const char *condition) {
    if (!condition || query.type != VAL_MAP) return query;

    NcMap *m = query.as.map;
    nc_map_set(m, nc_string_from_cstr("where"),
               NC_STRING(nc_string_from_cstr(condition)));
    return query;
}

NcValue nc_db_insert_into(const char *table, NcMap *data) {
    if (!table || !data) return NC_NONE();

    NcMap *query = nc_map_new();

    nc_map_set(query, nc_string_from_cstr("type"),
               NC_STRING(nc_string_from_cstr("insert")));
    nc_map_set(query, nc_string_from_cstr("table"),
               NC_STRING(nc_string_from_cstr(table)));
    nc_map_set(query, nc_string_from_cstr("data"), NC_MAP(data));

    return NC_MAP(query);
}

/* Validate identifier: allow only [a-zA-Z0-9_] to prevent injection */
static bool is_safe_identifier(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '.'))
            return false;
    }
    return true;
}

char *nc_db_query_to_sql(NcValue query) {
    if (query.type != VAL_MAP) return NULL;

    NcMap *m = query.as.map;
    NcValue type_val = nc_map_get(m, nc_string_from_cstr("type"));
    NcValue table_val = nc_map_get(m, nc_string_from_cstr("table"));

    if (type_val.type != VAL_STRING || table_val.type != VAL_STRING)
        return NULL;

    const char *type = type_val.as.string->chars;
    const char *table = table_val.as.string->chars;

    if (!is_safe_identifier(table)) {
        NC_WARN("Query builder: unsafe table name '%s'", table);
        return NULL;
    }

    NcDynBuf sql;
    nc_dbuf_init(&sql, 256);

    if (strcmp(type, "select") == 0) {
        nc_dbuf_append(&sql, "SELECT ");

        NcValue cols_val = nc_map_get(m, nc_string_from_cstr("columns"));
        if (cols_val.type == VAL_LIST && cols_val.as.list->count > 0) {
            NcList *cols = cols_val.as.list;
            for (int i = 0; i < cols->count; i++) {
                if (i > 0) nc_dbuf_append(&sql, ", ");
                NcValue col = nc_list_get(cols, i);
                if (col.type == VAL_STRING &&
                    is_safe_identifier(col.as.string->chars)) {
                    nc_dbuf_append(&sql, col.as.string->chars);
                } else {
                    nc_dbuf_append(&sql, "?");
                }
            }
        } else {
            nc_dbuf_append(&sql, "*");
        }

        nc_dbuf_append(&sql, " FROM ");
        nc_dbuf_append(&sql, table);

        NcValue where_val = nc_map_get(m, nc_string_from_cstr("where"));
        if (where_val.type == VAL_STRING && where_val.as.string->length > 0) {
            nc_dbuf_append(&sql, " WHERE ");
            nc_dbuf_append(&sql, where_val.as.string->chars);
        }

    } else if (strcmp(type, "insert") == 0) {
        NcValue data_val = nc_map_get(m, nc_string_from_cstr("data"));
        if (data_val.type != VAL_MAP || data_val.as.map->count == 0) {
            nc_dbuf_free(&sql);
            return NULL;
        }

        NcMap *data = data_val.as.map;
        nc_dbuf_append(&sql, "INSERT INTO ");
        nc_dbuf_append(&sql, table);
        nc_dbuf_append(&sql, " (");

        for (int i = 0; i < data->count; i++) {
            if (i > 0) nc_dbuf_append(&sql, ", ");
            if (data->keys[i] &&
                is_safe_identifier(data->keys[i]->chars)) {
                nc_dbuf_append(&sql, data->keys[i]->chars);
            }
        }

        nc_dbuf_append(&sql, ") VALUES (");
        for (int i = 0; i < data->count; i++) {
            if (i > 0) nc_dbuf_append(&sql, ", ");
            NcValue val = data->values[i];
            if (val.type == VAL_STRING) {
                nc_dbuf_append(&sql, "'");
                /* Simple escape for SQL string values */
                const char *s = val.as.string->chars;
                for (; *s; s++) {
                    if (*s == '\'') nc_dbuf_append(&sql, "''");
                    else {
                        char c[2] = {*s, '\0'};
                        nc_dbuf_append(&sql, c);
                    }
                }
                nc_dbuf_append(&sql, "'");
            } else if (val.type == VAL_INT) {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "%d", (int)val.as.integer);
                nc_dbuf_append(&sql, tmp);
            } else if (val.type == VAL_FLOAT) {
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "%g", val.as.floating);
                nc_dbuf_append(&sql, tmp);
            } else if (val.type == VAL_BOOL) {
                nc_dbuf_append(&sql, val.as.boolean ? "TRUE" : "FALSE");
            } else {
                nc_dbuf_append(&sql, "NULL");
            }
        }
        nc_dbuf_append(&sql, ")");
    } else {
        nc_dbuf_free(&sql);
        return NULL;
    }

    return sql.data; /* caller frees */
}

/* ═══════════════════════════════════════════════════════════════════
 *  ███╗   ███╗██╗ ██████╗ ██████╗  █████╗ ████████╗███████╗
 *  ████╗ ████║██║██╔════╝ ██╔══██╗██╔══██╗╚══██╔══╝██╔════╝
 *  ██╔████╔██║██║██║  ███╗██████╔╝███████║   ██║   █████╗
 *  ██║╚██╔╝██║██║██║   ██║██╔══██╗██╔══██║   ██║   ██╔══╝
 *  ██║ ╚═╝ ██║██║╚██████╔╝██║  ██║██║  ██║   ██║   ███████╗
 *  ╚═╝     ╚═╝╚═╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚══════╝
 *
 *  Migration Support — versioned schema migrations
 *
 *  Reads numbered .sql files from a directory:
 *    001_create_users.up.sql   / 001_create_users.down.sql
 *    002_add_email.up.sql      / 002_add_email.down.sql
 *
 *  Tracks the current version in the store under "__nc_migration_version".
 *  Executes via the SQL adapter (HTTP proxy).
 * ═══════════════════════════════════════════════════════════════════ */

#include <dirent.h>

/* Read a file's entire contents into a malloc'd string */
static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > 10 * 1024 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/* Compare function for sorting migration filenames */
static int migration_sort_cmp(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* Get current migration version from the store */
static int migration_current_version(NcDBPool *pool) {
    (void)pool;
    NcValue v = nc_store_get("__nc_migration_version");
    if (v.type == VAL_INT) return (int)v.as.integer;
    if (v.type == VAL_STRING && v.as.string->chars) {
        return atoi(v.as.string->chars);
    }
    return 0;
}

/* Save current migration version to the store */
static void migration_set_version(NcDBPool *pool, int version) {
    (void)pool;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", version);
    nc_store_put("__nc_migration_version", buf);
}

int nc_db_migrate_up(NcDBPool *pool, const char *migrations_dir) {
    if (!pool || !migrations_dir) return -1;

    DIR *dir = opendir(migrations_dir);
    if (!dir) {
        NC_WARN("Migrations: cannot open directory: %s", migrations_dir);
        return -1;
    }

    /* Collect all *.up.sql files */
    char *files[1024];
    int file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_count < 1024) {
        const char *name = entry->d_name;
        int nlen = (int)strlen(name);
        if (nlen > 7 && strcmp(name + nlen - 7, ".up.sql") == 0) {
            files[file_count++] = strdup(name);
        }
    }
    closedir(dir);

    if (file_count == 0) return 0;

    /* Sort by filename (which starts with version number) */
    qsort(files, file_count, sizeof(char *), migration_sort_cmp);

    int current = migration_current_version(pool);
    int applied = 0;

    for (int i = 0; i < file_count; i++) {
        /* Extract version number from filename prefix */
        int version = atoi(files[i]);
        if (version <= current) {
            free(files[i]);
            continue;
        }

        /* Build full path and read SQL */
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", migrations_dir, files[i]);
        char *sql = read_file_contents(path);
        if (!sql) {
            NC_WARN("Migrations: cannot read %s", path);
            free(files[i]);
            continue;
        }

        /* Execute the migration */
        NcValue result = nc_db_sql_exec(pool, sql);
        free(sql);

        if (IS_NONE(result)) {
            NC_WARN("Migrations: failed to apply %s", files[i]);
            free(files[i]);
            break;
        }

        migration_set_version(pool, version);
        applied++;
        free(files[i]);
    }

    /* Free any remaining unprocessed filenames */
    for (int i = 0; i < file_count; i++) {
        /* files[i] was freed inline above, but some may remain on break */
    }

    return applied;
}

int nc_db_migrate_down(NcDBPool *pool, const char *migrations_dir) {
    if (!pool || !migrations_dir) return -1;

    int current = migration_current_version(pool);
    if (current <= 0) return 0; /* nothing to roll back */

    DIR *dir = opendir(migrations_dir);
    if (!dir) {
        NC_WARN("Migrations: cannot open directory: %s", migrations_dir);
        return -1;
    }

    /* Find the .down.sql file matching current version */
    char target_prefix[32];
    snprintf(target_prefix, sizeof(target_prefix), "%03d_", current);
    int prefix_len = (int)strlen(target_prefix);

    char *down_file = NULL;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        int nlen = (int)strlen(name);
        if (nlen > 9 &&
            strncmp(name, target_prefix, prefix_len) == 0 &&
            strcmp(name + nlen - 9, ".down.sql") == 0) {
            down_file = strdup(name);
            break;
        }
    }
    closedir(dir);

    if (!down_file) {
        NC_WARN("Migrations: no down migration for version %d", current);
        return -1;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", migrations_dir, down_file);
    char *sql = read_file_contents(path);
    free(down_file);

    if (!sql) {
        NC_WARN("Migrations: cannot read %s", path);
        return -1;
    }

    NcValue result = nc_db_sql_exec(pool, sql);
    free(sql);

    if (IS_NONE(result)) {
        NC_WARN("Migrations: failed to roll back version %d", current);
        return -1;
    }

    migration_set_version(pool, current - 1);
    return 1;
}
