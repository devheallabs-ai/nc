/*
 * nc_llvm.h — LLVM IR code generation and AOT compilation for NC.
 *
 * NC generates LLVM IR as text and shells out to llc/clang for
 * native code generation. No LLVM C API library linkage required.
 *
 * Pipeline:
 *   NC Source -> Parser -> AST -> Compiler -> Bytecode -> IR Text -> clang -> Binary
 */

#ifndef NC_LLVM_H
#define NC_LLVM_H

#include "nc_ast.h"
#include "nc_chunk.h"
#include "nc_compiler.h"

/* ── IR Generation (AST-based) ─────────────────────────────── */

/* Generate LLVM IR from an NC AST program.
 * Writes .ll file to output_path. */
void nc_llvm_generate(NcASTNode *program, const char *output_path);

/* ── IR Generation (Bytecode-based) ────────────────────────── */

/* Generate LLVM IR from compiled NC bytecode.
 * This covers ALL NC opcodes and produces optimized IR. */
void nc_llvm_generate_from_bytecode(NcCompiler *comp, const char *output_path);

/* ── Bytecode Optimization Passes ──────────────────────────── */

/* Run all NC-specific optimization passes on bytecode before IR gen.
 *   - Constant folding
 *   - Dead code elimination
 *   - Strength reduction (mul by power-of-2 -> shift)
 *   - Common subexpression elimination */
void nc_llvm_optimize_bytecode(NcChunk *chunk);

/* Individual optimization passes */
void nc_opt_constant_fold(NcChunk *chunk);
void nc_opt_dead_code_eliminate(NcChunk *chunk);
void nc_opt_strength_reduce(NcChunk *chunk);

/* ── Native Code Emission ──────────────────────────────────── */

/* Compile LLVM IR text to a native object file (.o).
 * Uses clang or llc as subprocess.
 * Returns 0 on success, non-zero on failure. */
int nc_llvm_emit_object(const char *ir_text, const char *output_path);

/* ── AOT Compilation Pipeline ──────────────────────────────── */

/* Full pipeline: NC source -> parse -> compile -> optimize -> IR -> native binary.
 * Links with a minimal NC runtime stub.
 * Returns 0 on success, non-zero on failure. */
int nc_aot_compile(const char *nc_source, const char *output_binary);

#endif /* NC_LLVM_H */
