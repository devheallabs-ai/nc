/*
 * nc_http.h — HTTP client and universal AI bridge for NC.
 *
 * NC knows nothing about any AI company, model, or API format.
 * It only knows how to:
 *   1. Fill a JSON template with {{placeholders}}
 *   2. POST the result to a URL
 *   3. Extract the AI response by a dot-path (e.g. "choices.0.message.content")
 *
 * ALL provider-specific knowledge lives in configuration:
 *   - nc_ai_providers.json  (ships with NC, user-editable)
 *   - NC configure block    (per-project)
 *   - Environment variables (per-session)
 *
 * If a new AI company launches tomorrow, users add one JSON block.
 * No C code changes. No recompile. No new NC version.
 */

#ifndef NC_HTTP_H
#define NC_HTTP_H

#include "nc_value.h"

/* ── Dynamic buffer (replaces fixed char arrays) ───────────── */

typedef struct {
    char *data;
    int   len;
    int   cap;
} NcDynBuf;

void nc_dbuf_init(NcDynBuf *b, int initial_cap);
void nc_dbuf_append(NcDynBuf *b, const char *s);
void nc_dbuf_append_len(NcDynBuf *b, const char *s, int n);
void nc_dbuf_append_escaped(NcDynBuf *b, const char *s);
void nc_dbuf_free(NcDynBuf *b);

/* ── Universal AI Engine (zero hardcoded providers) ────────── */

char   *nc_ai_fill_template(const char *tpl, NcMap *vars);
NcValue nc_ai_extract_by_path(NcValue json, const char *dot_path);
void    nc_ai_load_config(const char *path);

/* ── HTTP configuration helpers ─────────────────────────── */

const char *nc_get_user_agent(void);

/* ── Core HTTP ────────────────────────────────────────────── */

void     nc_http_init(void);
void     nc_http_cleanup(void);
char    *nc_http_get(const char *url, const char *auth);
char    *nc_http_post(const char *url, const char *body,
                      const char *content_type, const char *auth);
char    *nc_http_post_stream(const char *url, const char *body,
                             const char *auth, bool print_tokens);

/* ── AI bridge ────────────────────────────────────────────── */

NcValue  nc_ai_call(const char *prompt, NcMap *context, const char *model);
NcValue  nc_ai_call_ex(const char *prompt, NcMap *context, NcMap *options);

/* ── AI response contract ─────────────────────────────────── */

NcValue  nc_ai_wrap_result(NcValue extracted, const char *raw_json,
                           const char *model, bool ok);

/* ── MCP bridge ───────────────────────────────────────────── */

NcValue  nc_mcp_call(const char *source, NcMap *options);

/* ── Generic gather ───────────────────────────────────────── */

NcValue  nc_gather_from(const char *source, NcMap *options);

#endif /* NC_HTTP_H */
