/*
 * nc_embed.h — Embeddable NC Runtime API
 *
 * Use this to run NC code from C, Python (via ctypes/cffi),
 * Node.js (via ffi-napi), Go (via cgo), or any language with C FFI.
 *
 * Build as shared library:
 *   Linux/macOS: cc -shared -o libncrt.so nc/src/*.c -lcurl -lm -lpthread
 *   Windows:     gcc -shared -o ncrt.dll nc/src/*.c -lwinhttp -lws2_32 -lm
 *
 * Usage from C:
 *   #include "nc_embed.h"
 *
 *   nc_runtime_t *rt = nc_runtime_new();
 *   nc_runtime_set_env(rt, "NC_AI_KEY", "sk-...");
 *   nc_result_t res = nc_runtime_eval(rt, "ask AI to \"hello\"\nrespond with result");
 *   if (res.ok) printf("%s\n", res.output);
 *   nc_result_free(&res);
 *   nc_runtime_free(rt);
 */

#ifndef NC_EMBED_H
#define NC_EMBED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Opaque runtime handle */
typedef struct nc_runtime nc_runtime_t;

/* Result of evaluating NC code */
typedef struct {
    bool        ok;
    char       *output;
    char       *error;
    int         exit_code;
} nc_result_t;

/* Context variable for passing data into NC code */
typedef struct {
    const char *name;
    const char *value;
} nc_var_t;

/*
 * Lifecycle
 */

/* Create a new NC runtime instance */
nc_runtime_t *nc_runtime_new(void);

/* Free a runtime instance */
void nc_runtime_free(nc_runtime_t *rt);

/*
 * Configuration
 */

/* Set an environment variable for this runtime (e.g., NC_AI_KEY) */
void nc_runtime_set_env(nc_runtime_t *rt, const char *key, const char *value);

/* Load an AI providers config file */
void nc_runtime_load_providers(nc_runtime_t *rt, const char *json_path);

/*
 * Execution
 */

/* Evaluate NC source code and return the result */
nc_result_t nc_runtime_eval(nc_runtime_t *rt, const char *source);

/* Evaluate NC source with context variables */
nc_result_t nc_runtime_eval_with(nc_runtime_t *rt, const char *source,
                                  nc_var_t *vars, int var_count);

/* Call a specific behavior by name with arguments */
nc_result_t nc_runtime_call(nc_runtime_t *rt, const char *source,
                             const char *behavior_name,
                             nc_var_t *args, int arg_count);

/* Evaluate a file */
nc_result_t nc_runtime_eval_file(nc_runtime_t *rt, const char *filename);

/*
 * Result cleanup
 */

/* Free the strings inside a result (output, error) */
void nc_result_free(nc_result_t *res);

/*
 * Version info
 */

const char *nc_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NC_EMBED_H */
