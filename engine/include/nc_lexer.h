/*
 * nc_lexer.h — Lexer (tokenizer) for the NC language.
 *
 * Converts .nc source text into a token array.
 * Include this when you need to tokenize NC source code.
 */

#ifndef NC_LEXER_H
#define NC_LEXER_H

#include "nc_token.h"

typedef struct NcLexer NcLexer;

struct NcLexer {
    const char *source;
    const char *filename;
    const char *current;
    const char *line_start;
    int         line;
    int         column;
    int         indent_stack[64];
    int         indent_depth;
    NcToken    *tokens;
    int         token_count;
    int         token_capacity;
    char      **escaped_bufs;
    int         escaped_count;
    int         escaped_cap;
};

NcLexer *nc_lexer_new(const char *source, const char *filename);
void     nc_lexer_tokenize(NcLexer *lex);
void     nc_lexer_free(NcLexer *lex);

#endif /* NC_LEXER_H */
