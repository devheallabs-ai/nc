/*
 * nc_ui_compiler.h — Native NC UI Compiler
 *
 * This compiler is responsible for taking a plain-English `.ncui` file
 * and compiling it into standard HTML/CSS/JS that any web browser can run.
 * 
 * Replaces the previous JavaScript-based compiler to achieve zero-dependency.
 */

#ifndef NC_UI_COMPILER_H
#define NC_UI_COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════ */

/**
 * Compiles an NCUI file into an HTML file.
 * Returns true if successful, false otherwise.
 */
bool nc_ui_compile_file(const char *input_path, const char *output_dir);

/**
 * Run the NC UI dev server, watching for changes and serving HTML.
 * (Future feature for 'nc ui dev')
 */
bool nc_ui_dev_server(const char *input_path, int port);

#endif /* NC_UI_COMPILER_H */
