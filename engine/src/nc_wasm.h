/*
 * nc_wasm.h — WebAssembly compilation target for NC bytecode.
 *
 * Translates NC bytecode to WebAssembly text format (.wat) which can
 * then be compiled to .wasm using wat2wasm (from the wabt toolkit).
 *
 * Pipeline: NC source -> bytecode -> .wat -> .wasm -> HTML wrapper
 */

#ifndef NC_WASM_H
#define NC_WASM_H

#include "../include/nc_chunk.h"
#include "../include/nc_value.h"
#include "../include/nc_vm.h"

/* ── Compiler state ─────────────────────────────────────────── */

typedef struct {
    char *output;           /* Generated .wat text                     */
    int   output_len;
    int   output_cap;
    int   local_count;      /* WASM locals needed for current function */
    int   label_count;      /* For branching / block labels            */

    /* Accumulated function bodies (appended before module close)      */
    char **func_bodies;
    int    func_body_count;
    int    func_body_cap;

    /* Export names for all compiled behaviors                         */
    char **export_names;
    int    export_count;
    int    export_cap;

    /* Error reporting                                                 */
    bool   had_error;
    char   error_msg[1024];
} NcWasmCompiler;

/* ── Lifecycle ──────────────────────────────────────────────── */

NcWasmCompiler *nc_wasm_new(void);
void            nc_wasm_free(NcWasmCompiler *w);

/* ── Compilation ────────────────────────────────────────────── */

/*
 * nc_wasm_compile_chunk — Convert one behavior's bytecode to a WASM
 * function in WAT text format. The result is stored internally and
 * emitted when nc_wasm_emit_module is called.
 *
 * Returns 0 on success, -1 on error (check w->error_msg).
 */
int nc_wasm_compile_chunk(NcWasmCompiler *w, NcChunk *chunk,
                          const char *func_name);

/*
 * nc_wasm_emit_module — Write the complete .wat module to a file.
 * Includes all previously compiled functions, host imports, memory
 * declarations, and export directives.
 *
 * Returns 0 on success, -1 on error.
 */
int nc_wasm_emit_module(NcWasmCompiler *w, const char *output_path);

/* ── HTML wrapper ───────────────────────────────────────────── */

/*
 * nc_wasm_generate_html — Generate a standalone HTML file that loads
 * and runs the .wasm module with a minimal JavaScript runtime providing
 * the host import stubs (nc_print, nc_http_get, etc.).
 *
 * Returns 0 on success, -1 on error.
 */
int nc_wasm_generate_html(const char *wasm_path, const char *html_path);

/* ── Full build pipeline ────────────────────────────────────── */

/*
 * nc_wasm_build — End-to-end compilation:
 *   1. Parse NC source to bytecode (uses existing compiler)
 *   2. Generate .wat from bytecode
 *   3. Shell out to wat2wasm if available
 *   4. Generate HTML wrapper
 *
 * output_dir receives: module.wat, module.wasm (if wat2wasm found),
 * and index.html.
 *
 * Returns 0 on success, -1 on error.
 */
int nc_wasm_build(const char *nc_source, const char *output_dir);

#endif /* NC_WASM_H */
