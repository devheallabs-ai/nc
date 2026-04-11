/*
 * nc_build.h — Native binary build system for NC.
 *
 * Pipeline:  NC source -> bytecode -> embedded C stub -> system cc -> native binary
 *
 * No LLVM library dependency. Uses the system C compiler (cc/gcc/clang)
 * to produce standalone executables with embedded bytecode.
 */

#ifndef NC_BUILD_H
#define NC_BUILD_H

#include "nc_compiler.h"
#include "nc_ast.h"

/* ── Build configuration ──────────────────────────────────── */

typedef struct {
    char input[512];        /* input .nc file path                  */
    char output[512];       /* output binary path                   */
    char target[64];        /* e.g., "darwin-arm64", "linux-x64"    */
    bool optimize;          /* -O2 vs -O0                           */
    bool strip;             /* strip debug symbols                  */
    bool static_link;       /* produce static binary                */
} NcBuildConfig;

/* ── Bytecode embedding ───────────────────────────────────── */

/*
 * Generates a C source file with:
 *   - Bytecode as static const uint8_t arrays (one per behavior)
 *   - String/int/float/bool constants for reconstruction
 *   - Variable name tables
 *   - Behavior name routing table
 *   - A main() that creates a VM and executes behaviors
 *
 * Returns 0 on success, non-zero on error.
 */
int nc_build_embed_bytecode(NcCompiler *comp, NcASTNode *program,
                            const char *output_c_path);

/* ── Runtime stub generation ──────────────────────────────── */

/*
 * Locates the NC engine source directory and enumerates the
 * minimal set of .c files required for standalone execution:
 *   nc_vm.c, nc_value.c, nc_stdlib.c, nc_gc.c, nc_json.c,
 *   nc_http.c, nc_compiler.c, nc_lexer.c, nc_parser.c, etc.
 *
 * Populates nc_dir_out with the resolved engine root path.
 * Returns 0 on success, non-zero if the engine directory cannot be found.
 */
int nc_build_generate_runtime(const char *self_path, char *nc_dir_out,
                              int nc_dir_size);

/* ── Native compilation ───────────────────────────────────── */

/*
 * Shells out to the system C compiler to build the embedded C stub
 * and all runtime source files into a native binary.
 *
 * Auto-detects cc/gcc/clang.
 * Platform-specific flags:
 *   Linux:   -static (when static_link is true), -lpthread, -ldl, -lm
 *   macOS:   framework flags, -lm, -lcurl
 *   Windows: -lwinhttp, -lws2_32, .exe suffix
 *
 * Supports cross-compilation targets: linux-x64, darwin-arm64, windows-x64.
 *
 * Returns 0 on success, non-zero on error.
 */
int nc_build_compile_native(const char *stub_path, const char *nc_dir,
                            const char *output_binary,
                            const NcBuildConfig *config);

/* ── Full pipeline ────────────────────────────────────────── */

/*
 * Runs the complete build pipeline:
 *   1. Lex + parse + compile + optimize the NC source
 *   2. Generate C stub with embedded bytecode
 *   3. Compile all runtime .c files (incremental, cached .o)
 *   4. Link everything into a native binary
 *
 * Returns 0 on success, non-zero on error.
 */
int nc_build_run(const char *self_path, const NcBuildConfig *config);

/* ── Config helpers ───────────────────────────────────────── */

/*
 * Initialize a build config with defaults.
 * Derives output name from input filename (strips .nc extension).
 */
void nc_build_config_init(NcBuildConfig *cfg, const char *input_file);

/*
 * Parse command-line arguments into a build config.
 * Expects argv starting after "nc build <file>".
 * Returns 0 on success, non-zero on invalid arguments.
 */
int nc_build_config_parse(NcBuildConfig *cfg, int argc, char *argv[],
                          int start_idx);

#endif /* NC_BUILD_H */
