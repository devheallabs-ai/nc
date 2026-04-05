/*
 * nc_compiler.h — AST to bytecode compiler.
 *
 * Compiles an AST program into bytecode chunks (one per behavior).
 */

#ifndef NC_COMPILER_H
#define NC_COMPILER_H

#include "nc_chunk.h"
#include "nc_ast.h"

typedef struct {
    NcChunk   *chunks;
    int        chunk_count;
    NcString **beh_names;
    NcMap     *globals;
    bool       had_error;
    char       error_msg[2048];
} NcCompiler;

NcCompiler *nc_compiler_new(void);
bool        nc_compiler_compile(NcCompiler *comp, NcASTNode *program);
void        nc_compiler_free(NcCompiler *comp);

void nc_optimize_all(NcCompiler *comp);

uint32_t nc_source_hash(const char *source, int length);
bool     nc_chunk_cache_save(NcCompiler *comp, const char *path, uint32_t src_hash);
bool     nc_chunk_cache_load(NcCompiler *comp, const char *path, uint32_t src_hash);

#endif /* NC_COMPILER_H */
