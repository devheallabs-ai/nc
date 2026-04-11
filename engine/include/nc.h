/*
 * nc.h — Umbrella header for the Notation-as-Code C runtime.
 *
 * This includes ALL module headers for convenience.
 * Individual modules can include only what they need:
 *
 *   #include "nc_value.h"      — Value system (NcValue, NcString, NcList, NcMap)
 *   #include "nc_token.h"      — Token types (NcTokenType, NcToken)
 *   #include "nc_lexer.h"      — Lexer (NcLexer)
 *   #include "nc_ast.h"        — AST nodes (NcASTNode)
 *   #include "nc_parser.h"     — Parser (NcParser)
 *   #include "nc_chunk.h"      — Bytecode opcodes and chunks (NcChunk)
 *   #include "nc_compiler.h"   — Compiler (NcCompiler)
 *   #include "nc_vm.h"         — Virtual machine (NcVM)
 *   #include "nc_gc.h"         — Garbage collector
 *   #include "nc_json.h"       — JSON parse/serialize
 *   #include "nc_http.h"       — HTTP client, AI bridge, MCP bridge
 *   #include "nc_stdlib.h"     — Standard library functions
 *   #include "nc_async.h"      — Async / concurrency
 *   #include "nc_server.h"     — HTTP server
 *   #include "nc_database.h"   — Database adapters
 *   #include "nc_websocket.h"  — WebSocket support
 *   #include "nc_middleware.h"  — Auth, rate limiting, logging
 *   #include "nc_enterprise.h" — Sandbox, audit, conformance
 */

#ifndef NC_H
#define NC_H

/* NC uses POSIX functions (popen, strdup, etc.)
 * This must be defined before any system header includes. */
#ifndef _WIN32
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

/* ── Standard C headers ───────────────────────────────────── */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>

/* ── Core modules (the compilation pipeline) ──────────────── */
#include "nc_value.h"
#include "nc_token.h"
#include "nc_lexer.h"
#include "nc_ast.h"
#include "nc_parser.h"
#include "nc_chunk.h"
#include "nc_compiler.h"
#include "nc_vm.h"

/* ── Runtime modules ──────────────────────────────────────── */
#include "nc_gc.h"
#include "nc_json.h"
#include "nc_http.h"
#include "nc_stdlib.h"
#include "nc_async.h"

/* ── Service modules ──────────────────────────────────────── */
#include "nc_server.h"
#include "nc_database.h"
#include "nc_websocket.h"
#include "nc_middleware.h"
#include "nc_enterprise.h"

/* ── Plugin system ────────────────────────────────────────── */
#include "nc_plugin.h"

/* ── Build system ─────────────────────────────────────────── */
#include "nc_build.h"

/* ── Secret Redaction ─────────────────────────────────────── */

/* Scans a string for patterns that look like API keys or tokens
 * and replaces them with [REDACTED]. Catches:
 *   sk-..., Bearer ..., key=..., token=..., password=... */
static inline void nc_redact_secret(char *msg, int max_len) {
    static const char *patterns[] = {
        "sk-", "Bearer ", "token=", "key=", "password=",
        "secret=", "apikey=", "api_key=", NULL
    };
    for (int p = 0; patterns[p]; p++) {
        int plen = (int)strlen(patterns[p]);
        char *pos = msg;
        while ((pos = strstr(pos, patterns[p])) != NULL) {
            char *val_start = pos + plen;
            char *val_end = val_start;
            while (*val_end && *val_end != '"' && *val_end != ' ' &&
                   *val_end != '\n' && *val_end != ',' && *val_end != '}' &&
                   *val_end != '&') {
                val_end++;
            }
            int val_len = (int)(val_end - val_start);
            if (val_len > 4) {
                const char *redact = "[REDACTED]";
                int rlen = 10;
                int remaining = (int)strlen(val_end);
                if (pos + plen + rlen + remaining < msg + max_len) {
                    memmove(val_start + rlen, val_end, remaining + 1);
                    memcpy(val_start, redact, rlen);
                }
            }
            pos = val_start + 1;
        }
    }
}

/* ── Structured Logging ───────────────────────────────────── */

typedef enum { NC_LOG_DEBUG, NC_LOG_INFO, NC_LOG_WARN, NC_LOG_ERROR } NcLogLevel;

static inline NcLogLevel nc_log_level(void) {
    static int cached = -1;
    if (cached < 0) {
        const char *l = getenv("NC_LOG_LEVEL");
        if (l && (l[0] == 'd' || l[0] == 'D')) cached = NC_LOG_DEBUG;
        else if (l && (l[0] == 'w' || l[0] == 'W')) cached = NC_LOG_WARN;
        else if (l && (l[0] == 'e' || l[0] == 'E')) cached = NC_LOG_ERROR;
        else cached = NC_LOG_INFO;
    }
    return (NcLogLevel)cached;
}

static inline void nc_log(NcLogLevel level, const char *fmt, ...) {
    if (level < nc_log_level()) return;
    static const char *labels[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    const char *log_fmt = getenv("NC_LOG_FORMAT");
    time_t now = time(NULL);
    char ts[32];
    struct tm *tm = gmtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    nc_redact_secret(msg, sizeof(msg));

    if (log_fmt && strcmp(log_fmt, "json") == 0) {
        fprintf(stderr, "{\"timestamp\":\"%s\",\"level\":\"%s\",\"message\":\"%s\"}\n",
            ts, labels[level], msg);
    } else {
        fprintf(stderr, "%s [%s] %s\n", ts, labels[level], msg);
    }
}

#define NC_DEBUG(fmt, ...) nc_log(NC_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define NC_INFO(fmt, ...)  nc_log(NC_LOG_INFO, fmt, ##__VA_ARGS__)
#define NC_WARN(fmt, ...)  nc_log(NC_LOG_WARN, fmt, ##__VA_ARGS__)
#define NC_ERROR(fmt, ...) nc_log(NC_LOG_ERROR, fmt, ##__VA_ARGS__)

/* ── Structured Observability Events ─────────────────────── */

/* Emits machine-readable JSON events for production monitoring.
 * Compatible with any JSON log aggregator or tracing collector.
 * Only emits when NC_LOG_FORMAT=json is set. */
static inline void nc_obs_event(const char *event, const char *extra_json) {
    const char *log_fmt = getenv("NC_LOG_FORMAT");
    if (!log_fmt || strcmp(log_fmt, "json") != 0) return;
    time_t now = time(NULL);
    char ts[32];
    struct tm *tm_val = gmtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm_val);
    if (extra_json && extra_json[0]) {
        fprintf(stderr, "{\"timestamp\":\"%s\",\"event\":\"%s\",%s}\n",
                ts, event, extra_json);
    } else {
        fprintf(stderr, "{\"timestamp\":\"%s\",\"event\":\"%s\"}\n", ts, event);
    }
}

#define NC_OBS(event, ...) nc_obs_event(event, ##__VA_ARGS__)

/* ── VM-Compiler bridge (needs both nc_vm.h + nc_compiler.h) ── */

void nc_vm_load(NcVM *vm, NcCompiler *compiled);

/* ── Tooling APIs ─────────────────────────────────────────── */

void nc_llvm_generate(NcASTNode *program, const char *output_path);
void nc_llvm_generate_from_bytecode(NcCompiler *comp, const char *output_path);
void nc_llvm_optimize_bytecode(NcChunk *chunk);
void nc_opt_constant_fold(NcChunk *chunk);
void nc_opt_dead_code_eliminate(NcChunk *chunk);
void nc_opt_strength_reduce(NcChunk *chunk);
int  nc_llvm_emit_object(const char *ir_text, const char *output_path);
int  nc_aot_compile(const char *nc_source, const char *output_binary);
int  nc_debug_file(const char *filename, const char *behavior);
void nc_debug_hook(int line, const char *source, NcMap *scope_vars);
int  nc_dap_run(const char *filename);
int  nc_lsp_run(void);
int  nc_analyze(NcASTNode *program, const char *filename, const char *source);
int  nc_repl_run(void);
int  nc_pkg_command(int argc, char *argv[]);
int  nc_digest_file(const char *filename);
int  nc_migrate(int argc, char *argv[]);
int  nc_format_file(const char *filename);
void nc_profiler_enable(void);
void nc_profiler_disable(void);
void nc_profiler_record_op(uint8_t opcode);
void nc_profiler_report(void);

/* ── Suggestions ("Did you mean?") ────────────────────────── */

const char *nc_suggest_from_list(const char *input, const char **candidates, int count);
const char *nc_suggest_from_map(const char *input, NcMap *map);
const char *nc_suggest_builtin(const char *input);
char       *nc_format_suggestion(const char *input, const char *suggestion);

/* Auto-correct (returns match + distance for confidence-based correction) */
const char *nc_suggest_with_distance(const char *input, const char **candidates,
                                      int count, int *out_distance);
const char *nc_autocorrect_from_map(const char *input, NcMap *map, int *out_distance);
const char *nc_autocorrect_builtin(const char *input, int *out_distance);

/* Auto-correct threshold: distance ≤ this → silently fix */
#define NC_AUTOCORRECT_THRESHOLD 2

/* ── Module system ────────────────────────────────────────── */

void    nc_module_reset(void);
void    nc_module_list_loaded(void);
NcValue nc_module_get_stdlib(const char *name);
NcASTNode *nc_module_load_file(const char *name, const char *from_file);

/* ── Distributed ──────────────────────────────────────────── */

int  nc_cluster_init(int port);
void nc_cluster_shutdown(void);

/* ── Multi-GPU Distributed Training (v2.0) ────────────────── */

typedef struct {
    int world_size;          /* Total number of workers */
    int rank;                /* This worker's rank (0-based) */
    int local_rank;          /* GPU index on this machine */
    char master_addr[256];   /* Master node address */
    int master_port;         /* Master node port */
    bool is_master;          /* rank == 0 */
} NCDistConfig;

NCDistConfig nc_dist_config_from_env(void);
int   nc_dist_init(NCDistConfig *cfg);
void  nc_dist_allreduce(float *gradients, int count);
void  nc_dist_broadcast(float *data, int count, int root);
void  nc_dist_barrier(void);
void  nc_dist_shutdown(void);
int   nc_dist_train(NCDistConfig *cfg, const char *model_path, const char *data_path);

/* ── Model Serving ────────────────────────────────────────── */

typedef struct {
    char model_path[512];
    int port;
    int num_workers;         /* Inference worker threads */
    int max_batch_size;      /* Dynamic batching */
    int max_queue_size;      /* Request queue limit */
    float timeout_ms;        /* Request timeout */
} NCServeConfig;

int nc_serve_model(NCServeConfig *cfg);

/* ── Model Registry ───────────────────────────────────────── */

int         nc_model_register(const char *name, const char *version, const char *path);
int         nc_model_list(void);
const char *nc_model_resolve(const char *name, const char *version);

/* ── Top-level API ────────────────────────────────────────── */

int nc_run_file(const char *filename);
int nc_run_source(const char *source, const char *filename);
int nc_validate_file(const char *filename);
NcValue nc_call_behavior(const char *source, const char *filename,
                          const char *behavior_name, NcMap *args);
void    nc_ast_cache_flush(void);
void    nc_server_globals_set(NcMap *globals);
NcMap  *nc_server_globals_get(void);

/* ── Request Timeout ──────────────────────────────────────── */

void nc_set_request_deadline(double deadline_ms);

#endif /* NC_H */
