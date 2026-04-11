/*
 * nc_parser.h — Recursive descent parser for NC.
 *
 * Converts a token stream into an AST.
 */

#ifndef NC_PARSER_H
#define NC_PARSER_H

#include "nc_token.h"
#include "nc_ast.h"

typedef struct NcParser NcParser;

struct NcParser {
    NcToken *tokens;
    int      count;
    int      pos;
    const char *filename;
    bool     had_error;
    char     error_msg[2048];
    int      depth;          /* recursion depth guard */
};

NcParser  *nc_parser_new(NcToken *tokens, int count, const char *filename);
NcASTNode *nc_parser_parse(NcParser *p);
void       nc_parser_free(NcParser *p);

#endif /* NC_PARSER_H */
