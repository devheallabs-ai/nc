/*
 * nc_server.h — HTTP/HTTPS server for NC services.
 *
 * TLS support via configure block:
 *   configure:
 *       tls_cert: "/path/to/cert.pem"
 *       tls_key: "/path/to/key.pem"
 *
 * Or via environment:
 *   NC_TLS_CERT=/path/to/cert.pem
 *   NC_TLS_KEY=/path/to/key.pem
 */

#ifndef NC_SERVER_H
#define NC_SERVER_H

int nc_serve(const char *filename, int port);

/* Thread-local request context — gives behaviors access to
 * the current HTTP request's headers, IP, method, etc.
 * Set by the server before invoking each behavior. */
typedef struct {
    char headers[64][2][256];
    int  header_count;
    char client_ip[48];
    char method[16];
    char path[512];
    char trace_id[64];
} NcRequestContext;

void              nc_request_ctx_set(NcRequestContext *ctx);
NcRequestContext *nc_request_ctx_get(void);

#endif /* NC_SERVER_H */
