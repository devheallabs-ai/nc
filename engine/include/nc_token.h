/*
 * nc_token.h — Token types for the NC lexer.
 *
 * Defines all keyword, literal, symbol, and special tokens.
 * Include this when you need NcTokenType or NcToken.
 */

#ifndef NC_TOKEN_H
#define NC_TOKEN_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* Declarations */
    TOK_SERVICE, TOK_VERSION, TOK_AUTHOR, TOK_DESCRIPTION, TOK_MODEL,
    TOK_IMPORT,
    /* Structure */
    TOK_DEFINE, TOK_AS, TOK_TO, TOK_WITH, TOK_FROM, TOK_USING,
    TOK_WHERE, TOK_SAVE, TOK_INTO, TOK_CALLED, TOK_PURPOSE, TOK_CONFIGURE,
    TOK_MIDDLEWARE, TOK_PROXY, TOK_FORWARD,
    /* AI ops */
    TOK_ASK, TOK_AI, TOK_GATHER, TOK_CHECK, TOK_ANALYZE,
    /* Control */
    TOK_IF, TOK_OTHERWISE, TOK_REPEAT, TOK_FOR, TOK_EACH, TOK_IN,
    TOK_WHILE, TOK_STOP, TOK_SKIP, TOK_MATCH, TOK_WHEN,
    /* Comparisons */
    TOK_IS, TOK_ARE, TOK_ABOVE, TOK_BELOW, TOK_EQUAL,
    TOK_GREATER, TOK_LESS, TOK_THAN, TOK_AT, TOK_LEAST, TOK_MOST,
    TOK_NOT, TOK_BETWEEN,
    /* Logic */
    TOK_AND, TOK_OR,
    /* Actions */
    TOK_RUN, TOK_RESPOND, TOK_NOTIFY, TOK_WAIT, TOK_STORE, TOK_EMIT,
    TOK_LOG, TOK_SET, TOK_SHOW, TOK_SEND, TOK_APPLY,
    TOK_NEEDS, TOK_APPROVAL, TOK_THEN,
    /* Error handling */
    TOK_TRY, TOK_ON_ERROR, TOK_FINALLY, TOK_CATCH,
    /* Testing */
    TOK_ASSERT, TOK_TEST,
    /* Async */
    TOK_ASYNC, TOK_AWAIT, TOK_YIELD,
    /* Agents */
    TOK_AGENT,
    /* Types */
    TOK_TEXT, TOK_NUMBER, TOK_YESNO, TOK_LIST_TYPE, TOK_RECORD, TOK_OPTIONAL,
    /* Time */
    TOK_MILLISECONDS, TOK_SECONDS, TOK_MINUTES, TOK_HOURS, TOK_DAYS,
    /* API */
    TOK_API, TOK_RUNS, TOK_DOES,
    TOK_HTTP_GET, TOK_HTTP_POST, TOK_HTTP_PUT, TOK_HTTP_DELETE, TOK_HTTP_PATCH,
    /* Events */
    TOK_ON, TOK_EVENT, TOK_EVERY,
    /* Literals */
    TOK_STRING, TOK_INTEGER, TOK_FLOAT_LIT,
    TOK_TRUE, TOK_FALSE, TOK_NONE_LIT,
    TOK_TEMPLATE,
    /* Symbols */
    TOK_COLON, TOK_COMMA, TOK_DOT, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
    TOK_LBRACE, TOK_RBRACE,
    TOK_EQUALS_SIGN, TOK_ARROW,
    TOK_GT_SIGN, TOK_LT_SIGN, TOK_GTE_SIGN, TOK_LTE_SIGN, TOK_EQEQ_SIGN, TOK_BANGEQ_SIGN,
    /* Generics & Interfaces */
    TOK_INTERFACE, TOK_IMPLEMENTS, TOK_REQUIRES, TOK_OF, TOK_TYPE,
    /* Decorators */
    TOK_AT_SIGN,
    /* Operator overloading */
    TOK_OPERATOR,
    /* Pattern matching extensions */
    TOK_SPREAD,   /* ... for rest patterns */
    /* Special */
    TOK_IDENTIFIER, TOK_NEWLINE, TOK_INDENT, TOK_DEDENT, TOK_EOF, TOK_ERROR,
} NcTokenType;

typedef struct NcToken NcToken;

struct NcToken {
    NcTokenType type;
    const char *start;
    int         length;
    int         line;
    int         column;
    union {
        int64_t  int_val;
        double   float_val;
        bool     bool_val;
    } literal;
};

#endif /* NC_TOKEN_H */
