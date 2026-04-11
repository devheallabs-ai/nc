/*
 * nc_websocket.c — WebSocket support for the NC engine.
 *
 * Users write:
 *   on event "websocket.message":
 *       ask AI to "process" using event.data
 *       send result to event.connection
 *
 * The engine handles: WebSocket upgrade, framing, ping/pong.
 * Integrates with nc_server.c for the HTTP upgrade path.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/nc_websocket.h"
#include "../include/nc_platform.h"

/* ═══════════════════════════════════════════════════════════
 *  WebSocket frame parsing (RFC 6455)
 * ═══════════════════════════════════════════════════════════ */

/* ── Per-connection outbound message queue ── */
#define WS_QUEUE_MAX 64
typedef struct {
    char *data;
    int   len;
} NcWSQueuedMsg;

/* ── Room membership ── */
#define WS_MAX_ROOMS_PER_CONN 16
#define WS_ROOM_NAME_MAX      64

typedef struct {
    nc_socket_t fd;
    bool   connected;
    char   path[256];
    int    id;
    /* Message queue */
    NcWSQueuedMsg queue[WS_QUEUE_MAX];
    int           queue_head;
    int           queue_tail;
    int           queue_count;
    /* Ping/pong keepalive */
    double last_pong_time;   /* milliseconds (nc_realtime_ms) */
    /* Room membership */
    char   rooms[WS_MAX_ROOMS_PER_CONN][WS_ROOM_NAME_MAX];
    int    room_count;
} NcWSConn;

#define WS_MAX_CONNS 256
static NcWSConn ws_connections[WS_MAX_CONNS];
static int ws_conn_count = 0;
static nc_mutex_t ws_mutex = NC_MUTEX_INITIALIZER;

/* SHA-1 (RFC 3174) — zero-dependency implementation for WebSocket */
static uint32_t sha1_rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_simple(const unsigned char *data, int len, unsigned char *hash) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476, h4 = 0xC3D2E1F0;

    /* Pre-processing: padding */
    uint64_t bit_len = (uint64_t)len * 8;
    int padded_len = ((len + 8) / 64 + 1) * 64;
    unsigned char *msg = calloc(padded_len, 1);
    if (!msg) { memset(hash, 0, 20); return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    for (int i = 0; i < 8; i++)
        msg[padded_len - 1 - i] = (unsigned char)(bit_len >> (i * 8));

    /* Process each 512-bit (64-byte) block */
    for (int offset = 0; offset < padded_len; offset += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[offset + i*4] << 24) | ((uint32_t)msg[offset + i*4+1] << 16) |
                    ((uint32_t)msg[offset + i*4+2] << 8) | (uint32_t)msg[offset + i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = sha1_rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);    k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else              { f = b ^ c ^ d;                k = 0xCA62C1D6; }
            uint32_t temp = sha1_rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = sha1_rotl(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }
    free(msg);

    /* Produce the 20-byte hash */
    uint32_t hh[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        hash[i*4]   = (hh[i] >> 24) & 0xFF;
        hash[i*4+1] = (hh[i] >> 16) & 0xFF;
        hash[i*4+2] = (hh[i] >> 8)  & 0xFF;
        hash[i*4+3] = hh[i] & 0xFF;
    }
}

/* Base64 encode for WebSocket accept header */
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(const unsigned char *in, int len, char *out) {
    int i, j = 0;
    for (i = 0; i < len - 2; i += 3) {
        out[j++] = b64[(in[i] >> 2) & 0x3F];
        out[j++] = b64[((in[i] & 0x3) << 4) | (in[i+1] >> 4)];
        out[j++] = b64[((in[i+1] & 0xF) << 2) | (in[i+2] >> 6)];
        out[j++] = b64[in[i+2] & 0x3F];
    }
    if (i < len) {
        out[j++] = b64[(in[i] >> 2) & 0x3F];
        if (i + 1 < len) {
            out[j++] = b64[((in[i] & 0x3) << 4) | (in[i+1] >> 4)];
            out[j++] = b64[((in[i+1] & 0xF) << 2)];
        } else {
            out[j++] = b64[(in[i] & 0x3) << 4];
            out[j++] = '=';
        }
        out[j++] = '=';
    }
    out[j] = '\0';
}

/* Compute Sec-WebSocket-Accept per RFC 6455 */
void nc_ws_compute_accept(const char *client_key, char *out, int out_size) {
    const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", client_key, magic);

    unsigned char hash[20];
    sha1_simple((unsigned char *)concat, (int)strlen(concat), hash);
    base64_encode(hash, 20, out);
    (void)out_size;
}

/* Send WebSocket text frame */
int nc_ws_send(nc_socket_t fd, const char *message) {
    int len = (int)strlen(message);
    unsigned char header[10];
    int hlen = 0;

    header[0] = 0x81; /* FIN + text opcode */
    if (len < 126) {
        header[1] = (unsigned char)len;
        hlen = 2;
    } else if (len < 65536) {
        header[1] = 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        hlen = 4;
    } else {
        header[1] = 127;
        memset(header + 2, 0, 4);
        header[6] = (len >> 24) & 0xFF;
        header[7] = (len >> 16) & 0xFF;
        header[8] = (len >> 8) & 0xFF;
        header[9] = len & 0xFF;
        hlen = 10;
    }

    nc_send(fd, header, hlen, 0);
    nc_send(fd, message, len, 0);
    return 0;
}

/* Read WebSocket frame */
int nc_ws_read(nc_socket_t fd, char *buf, int buf_size) {
    unsigned char header[2];
    int n = (int)nc_recv(fd, header, 2, 0);
    if (n < 2) return -1;

    int opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    int payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        unsigned char ext[2];
        if (nc_recv(fd, ext, 2, 0) < 2) return -1;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        unsigned char ext[8];
        if (nc_recv(fd, ext, 8, 0) < 8) return -1;
        payload_len = (ext[4] << 24) | (ext[5] << 16) | (ext[6] << 8) | ext[7];
    }

    unsigned char mask[4] = {0};
    if (masked) nc_recv(fd, mask, 4, 0);

    if (payload_len > buf_size - 1) payload_len = buf_size - 1;
    n = (int)nc_recv(fd, buf, payload_len, 0);
    if (n < 0) return -1;

    if (masked) {
        for (int i = 0; i < n; i++) buf[i] ^= mask[i % 4];
    }
    buf[n] = '\0';

    /* Handle close frame */
    if (opcode == 0x08) return -1;
    /* Handle ping — respond with pong echoing received payload */
    if (opcode == 0x09) {
        unsigned char pong_hdr[2];
        pong_hdr[0] = 0x8A;
        pong_hdr[1] = (unsigned char)(n > 125 ? 125 : n);
        nc_send(fd, pong_hdr, 2, 0);
        if (n > 0) nc_send(fd, buf, n > 125 ? 125 : n, 0);
        return 0;
    }
    /* Handle pong — update last_pong_time for the connection */
    if (opcode == 0x0A) {
        nc_mutex_lock(&ws_mutex);
        for (int i = 0; i < ws_conn_count; i++) {
            if (ws_connections[i].fd == fd && ws_connections[i].connected) {
                ws_connections[i].last_pong_time = nc_realtime_ms();
                break;
            }
        }
        nc_mutex_unlock(&ws_mutex);
        return 0;
    }

    return n;
}

/* Broadcast to all WebSocket connections (thread-safe) */
void nc_ws_broadcast(const char *message) {
    nc_mutex_lock(&ws_mutex);
    for (int i = 0; i < ws_conn_count; i++) {
        if (ws_connections[i].connected)
            nc_ws_send(ws_connections[i].fd, message);
    }
    nc_mutex_unlock(&ws_mutex);
}

/* Add connection (thread-safe) */
int nc_ws_add_connection(nc_socket_t fd, const char *path) {
    nc_mutex_lock(&ws_mutex);
    if (ws_conn_count >= WS_MAX_CONNS) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    NcWSConn *c = &ws_connections[ws_conn_count];
    memset(c, 0, sizeof(NcWSConn));
    c->fd = fd;
    c->connected = true;
    strncpy(c->path, path, 255);
    c->path[255] = '\0';
    c->id = ws_conn_count;
    c->last_pong_time = nc_realtime_ms();
    c->queue_head  = 0;
    c->queue_tail  = 0;
    c->queue_count = 0;
    c->room_count  = 0;
    int id = ws_conn_count++;
    nc_mutex_unlock(&ws_mutex);
    return id;
}

/* Remove connection (thread-safe) */
void nc_ws_remove_connection(int id) {
    nc_mutex_lock(&ws_mutex);
    if (id >= 0 && id < ws_conn_count) {
        NcWSConn *c = &ws_connections[id];
        c->connected = false;
        /* Free any queued messages */
        for (int i = 0; i < c->queue_count; i++) {
            int slot = (c->queue_head + i) % WS_QUEUE_MAX;
            free(c->queue[slot].data);
            c->queue[slot].data = NULL;
        }
        c->queue_head = c->queue_tail = c->queue_count = 0;
        c->room_count = 0;
        nc_closesocket(c->fd);
    }
    nc_mutex_unlock(&ws_mutex);
}

/* ═══════════════════════════════════════════════════════════
 *  HTTP Upgrade handler
 *
 *  Parses Sec-WebSocket-Key from the raw HTTP request headers,
 *  sends the 101 Switching Protocols response, and adds the
 *  connection to the pool.  Returns the connection ID or -1.
 * ═══════════════════════════════════════════════════════════ */

int nc_ws_handle_upgrade(int client_fd, const char *request_headers) {
    if (client_fd < 0 || !request_headers) return -1;

    /* Find Sec-WebSocket-Key header (case-insensitive search) */
    const char *key_hdr = NULL;
    const char *p = request_headers;
    while (*p) {
        if (strncasecmp(p, "Sec-WebSocket-Key:", 18) == 0) {
            key_hdr = p + 18;
            break;
        }
        /* Advance to next line */
        const char *nl = strstr(p, "\r\n");
        if (!nl) { nl = strstr(p, "\n"); }
        if (!nl) break;
        p = nl + (nl[0] == '\r' ? 2 : 1);
    }
    if (!key_hdr) return -1;

    /* Skip leading whitespace */
    while (*key_hdr == ' ' || *key_hdr == '\t') key_hdr++;

    /* Extract the key value (up to \r\n or end) */
    char client_key[64];
    int klen = 0;
    while (key_hdr[klen] && key_hdr[klen] != '\r' && key_hdr[klen] != '\n' && klen < 63)
        klen++;
    if (klen == 0) return -1;
    memcpy(client_key, key_hdr, klen);
    client_key[klen] = '\0';
    /* Trim trailing whitespace */
    while (klen > 0 && (client_key[klen-1] == ' ' || client_key[klen-1] == '\t'))
        client_key[--klen] = '\0';

    /* Compute Sec-WebSocket-Accept */
    char accept_key[64];
    nc_ws_compute_accept(client_key, accept_key, sizeof(accept_key));

    /* Build and send 101 Switching Protocols response */
    char response[512];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);

    nc_socket_t fd = (nc_socket_t)client_fd;
    if (nc_send(fd, response, rlen, 0) < 0)
        return -1;

    /* Extract path from the first line: GET /path HTTP/1.1 */
    char path[256] = "/";
    if (strncmp(request_headers, "GET ", 4) == 0) {
        const char *pstart = request_headers + 4;
        const char *pend = strchr(pstart, ' ');
        if (pend) {
            int plen = (int)(pend - pstart);
            if (plen > 255) plen = 255;
            memcpy(path, pstart, plen);
            path[plen] = '\0';
        }
    }

    /* Add to connection pool */
    int conn_id = nc_ws_add_connection(fd, path);
    if (conn_id >= 0) {
        /* Initialize keepalive timestamp */
        nc_mutex_lock(&ws_mutex);
        ws_connections[conn_id].last_pong_time = nc_realtime_ms();
        nc_mutex_unlock(&ws_mutex);
    }
    return conn_id;
}

/* ═══════════════════════════════════════════════════════════
 *  Per-connection outbound message queue
 *
 *  Enqueue messages so sends don't block the caller.
 *  nc_ws_flush_queue() drains them onto the wire.
 *  Both are thread-safe via ws_mutex.
 * ═══════════════════════════════════════════════════════════ */

int nc_ws_queue_message(int conn_id, const char *msg, int len) {
    if (!msg || len <= 0) return -1;

    nc_mutex_lock(&ws_mutex);
    if (conn_id < 0 || conn_id >= ws_conn_count || !ws_connections[conn_id].connected) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    NcWSConn *c = &ws_connections[conn_id];
    if (c->queue_count >= WS_QUEUE_MAX) {
        nc_mutex_unlock(&ws_mutex);
        return -1; /* queue full */
    }

    char *copy = (char *)malloc(len);
    if (!copy) { nc_mutex_unlock(&ws_mutex); return -1; }
    memcpy(copy, msg, len);

    int slot = c->queue_tail;
    c->queue[slot].data = copy;
    c->queue[slot].len  = len;
    c->queue_tail = (c->queue_tail + 1) % WS_QUEUE_MAX;
    c->queue_count++;
    nc_mutex_unlock(&ws_mutex);
    return 0;
}

int nc_ws_flush_queue(int conn_id) {
    nc_mutex_lock(&ws_mutex);
    if (conn_id < 0 || conn_id >= ws_conn_count || !ws_connections[conn_id].connected) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    NcWSConn *c = &ws_connections[conn_id];
    nc_socket_t fd = c->fd;

    /* Drain queued messages while holding the mutex for queue state,
     * but copy them out to send after releasing (to avoid blocking). */
    NcWSQueuedMsg batch[WS_QUEUE_MAX];
    int count = c->queue_count;
    for (int i = 0; i < count; i++) {
        int slot = (c->queue_head + i) % WS_QUEUE_MAX;
        batch[i] = c->queue[slot];
        c->queue[slot].data = NULL;
        c->queue[slot].len  = 0;
    }
    c->queue_head  = c->queue_tail;
    c->queue_count = 0;
    nc_mutex_unlock(&ws_mutex);

    /* Send each message outside the lock */
    int sent = 0;
    for (int i = 0; i < count; i++) {
        if (batch[i].data) {
            /* Null-terminate temporarily for nc_ws_send */
            char *tmp = (char *)malloc(batch[i].len + 1);
            if (tmp) {
                memcpy(tmp, batch[i].data, batch[i].len);
                tmp[batch[i].len] = '\0';
                nc_ws_send(fd, tmp);
                free(tmp);
                sent++;
            }
            free(batch[i].data);
        }
    }
    return sent;
}

/* ═══════════════════════════════════════════════════════════
 *  Ping/Pong keepalive
 *
 *  nc_ws_ping() sends an RFC 6455 ping frame (opcode 0x09).
 *  The remote side should reply with pong (handled in nc_ws_read).
 *  We track last_pong_time so callers can detect stale connections.
 * ═══════════════════════════════════════════════════════════ */

int nc_ws_ping(int conn_id) {
    nc_mutex_lock(&ws_mutex);
    if (conn_id < 0 || conn_id >= ws_conn_count || !ws_connections[conn_id].connected) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    nc_socket_t fd = ws_connections[conn_id].fd;
    nc_mutex_unlock(&ws_mutex);

    /* Ping frame: FIN=1, opcode=0x09, payload_len=0 */
    unsigned char ping_frame[2] = { 0x89, 0x00 };
    if (nc_send(fd, ping_frame, 2, 0) < 0)
        return -1;
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Connection groups / rooms (pub/sub)
 *
 *  Each connection can belong to up to WS_MAX_ROOMS_PER_CONN
 *  named rooms.  nc_ws_broadcast_room sends to every connected
 *  member of the specified room.
 * ═══════════════════════════════════════════════════════════ */

int nc_ws_join_room(int conn_id, const char *room) {
    if (!room || !room[0]) return -1;

    nc_mutex_lock(&ws_mutex);
    if (conn_id < 0 || conn_id >= ws_conn_count || !ws_connections[conn_id].connected) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    NcWSConn *c = &ws_connections[conn_id];

    /* Check if already in the room */
    for (int i = 0; i < c->room_count; i++) {
        if (strcmp(c->rooms[i], room) == 0) {
            nc_mutex_unlock(&ws_mutex);
            return 0; /* already joined */
        }
    }
    if (c->room_count >= WS_MAX_ROOMS_PER_CONN) {
        nc_mutex_unlock(&ws_mutex);
        return -1; /* room list full */
    }
    strncpy(c->rooms[c->room_count], room, WS_ROOM_NAME_MAX - 1);
    c->rooms[c->room_count][WS_ROOM_NAME_MAX - 1] = '\0';
    c->room_count++;
    nc_mutex_unlock(&ws_mutex);
    return 0;
}

int nc_ws_leave_room(int conn_id, const char *room) {
    if (!room || !room[0]) return -1;

    nc_mutex_lock(&ws_mutex);
    if (conn_id < 0 || conn_id >= ws_conn_count || !ws_connections[conn_id].connected) {
        nc_mutex_unlock(&ws_mutex);
        return -1;
    }
    NcWSConn *c = &ws_connections[conn_id];

    for (int i = 0; i < c->room_count; i++) {
        if (strcmp(c->rooms[i], room) == 0) {
            /* Swap with last and shrink */
            if (i < c->room_count - 1) {
                memcpy(c->rooms[i], c->rooms[c->room_count - 1], WS_ROOM_NAME_MAX);
            }
            c->room_count--;
            nc_mutex_unlock(&ws_mutex);
            return 0;
        }
    }
    nc_mutex_unlock(&ws_mutex);
    return -1; /* not in that room */
}

int nc_ws_broadcast_room(const char *room, const char *msg, int len) {
    if (!room || !room[0] || !msg || len <= 0) return -1;

    /* Null-terminate the message for nc_ws_send */
    char *tmp = (char *)malloc(len + 1);
    if (!tmp) return -1;
    memcpy(tmp, msg, len);
    tmp[len] = '\0';

    int sent = 0;
    nc_mutex_lock(&ws_mutex);
    for (int i = 0; i < ws_conn_count; i++) {
        if (!ws_connections[i].connected) continue;
        for (int r = 0; r < ws_connections[i].room_count; r++) {
            if (strcmp(ws_connections[i].rooms[r], room) == 0) {
                nc_ws_send(ws_connections[i].fd, tmp);
                sent++;
                break; /* don't send twice to the same connection */
            }
        }
    }
    nc_mutex_unlock(&ws_mutex);
    free(tmp);
    return sent;
}

/* ═══════════════════════════════════════════════════════════
 *  WebSocket CLIENT — connect to external ws:// or wss:// endpoints
 *
 *  NC code:
 *    set conn to ws_connect("ws://localhost:9090/events")
 *    ws_send(conn, "hello")
 *    set msg to ws_receive(conn)
 *    ws_close(conn)
 *
 *  Uses plain TCP for ws:// — wss:// would require TLS,
 *  which we delegate to a TLS proxy or curl.
 * ═══════════════════════════════════════════════════════════ */

#ifndef NC_WINDOWS
#include <netdb.h>
#endif

nc_socket_t nc_ws_client_connect(const char *url) {
    if (!url) return NC_INVALID_SOCKET;
    nc_socket_init();

    /* Parse ws://host:port/path */
    const char *p = url;
    bool is_wss = false;
    if (strncmp(p, "wss://", 6) == 0) { is_wss = true; p += 6; }
    else if (strncmp(p, "ws://", 5) == 0) { p += 5; }
    else return NC_INVALID_SOCKET;

    char host[256] = {0};
    int port = is_wss ? 443 : 80;
    char path[512] = "/";

    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        int hlen = (int)(colon - p);
        if (hlen > 255) hlen = 255;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        port = atoi(colon + 1);
    } else if (slash) {
        int hlen = (int)(slash - p);
        if (hlen > 255) hlen = 255;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
    } else {
        strncpy(host, p, 255);
    }
    if (slash) {
        strncpy(path, slash, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }

    if (is_wss) {
        /* wss:// requires TLS — not supported in plain sockets.
         * Return error; user should use a TLS termination proxy. */
        return NC_INVALID_SOCKET;
    }

    /* Resolve hostname */
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res)
        return NC_INVALID_SOCKET;

    nc_socket_t fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd == NC_INVALID_SOCKET) { freeaddrinfo(res); return NC_INVALID_SOCKET; }

    if (connect(fd, res->ai_addr, (int)res->ai_addrlen) < 0) {
        nc_closesocket(fd);
        freeaddrinfo(res);
        return NC_INVALID_SOCKET;
    }
    freeaddrinfo(res);

    /* WebSocket handshake */
    char ws_key[25];
    unsigned long seed = (unsigned long)time(NULL) ^ (unsigned long)fd;
    for (int i = 0; i < 16; i++) {
        seed = seed * 1103515245 + 12345;
        ws_key[i] = (char)(32 + (seed >> 16) % 95);
    }
    ws_key[16] = '\0';

    char b64_key[32];
    base64_encode((unsigned char *)ws_key, 16, b64_key);

    char req[1024];
    int rlen = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        path, host, port, b64_key);

    nc_send(fd, req, rlen, 0);

    /* Read upgrade response */
    char resp[2048];
    int n = (int)nc_recv(fd, resp, sizeof(resp) - 1, 0);
    if (n <= 0) { nc_closesocket(fd); return NC_INVALID_SOCKET; }
    resp[n] = '\0';

    if (!strstr(resp, "101")) {
        nc_closesocket(fd);
        return NC_INVALID_SOCKET;
    }

    return fd;
}

int nc_ws_client_send(nc_socket_t fd, const char *message) {
    if (fd == NC_INVALID_SOCKET || !message) return -1;
    int len = (int)strlen(message);
    unsigned char header[14];
    int hlen = 0;

    header[0] = 0x81; /* FIN + text opcode */

    /* Client frames MUST be masked (RFC 6455 Section 5.1) */
    unsigned long mask_seed = (unsigned long)time(NULL) ^ (unsigned long)len;
    unsigned char mask[4];
    for (int i = 0; i < 4; i++) {
        mask_seed = mask_seed * 1103515245 + 12345;
        mask[i] = (unsigned char)(mask_seed >> 16);
    }

    if (len < 126) {
        header[1] = 0x80 | (unsigned char)len;
        memcpy(header + 2, mask, 4);
        hlen = 6;
    } else if (len < 65536) {
        header[1] = 0x80 | 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        memcpy(header + 4, mask, 4);
        hlen = 8;
    } else {
        header[1] = 0x80 | 127;
        memset(header + 2, 0, 4);
        header[6] = (len >> 24) & 0xFF;
        header[7] = (len >> 16) & 0xFF;
        header[8] = (len >> 8) & 0xFF;
        header[9] = len & 0xFF;
        memcpy(header + 10, mask, 4);
        hlen = 14;
    }

    nc_send(fd, header, hlen, 0);

    /* Mask the payload */
    char *masked = malloc(len);
    if (!masked) return -1;
    for (int i = 0; i < len; i++)
        masked[i] = message[i] ^ mask[i % 4];
    nc_send(fd, masked, len, 0);
    free(masked);
    return 0;
}

void nc_ws_client_close(nc_socket_t fd) {
    if (fd == NC_INVALID_SOCKET) return;
    unsigned char close_frame[2] = { 0x88, 0x00 };
    nc_send(fd, close_frame, 2, 0);
    nc_closesocket(fd);
}
