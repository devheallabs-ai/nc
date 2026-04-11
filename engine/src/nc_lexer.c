/*
 * nc_lexer.c — Tokenizer for the NC language, written in C.
 *
 * Reads .nc source text and produces a token array.
 * Handles indentation-based blocks, plain English keywords,
 * template expressions {{…}}, strings, numbers, and symbols.
 */

#include "../include/nc.h"

/* ── Keyword table ─────────────────────────────────────────── */

typedef struct { const char *word; NcTokenType type; } Keyword;

static const Keyword keywords[] = {
    {"service",TOK_SERVICE},{"module",TOK_SERVICE},
    {"version",TOK_VERSION},{"author",TOK_AUTHOR},
    {"description",TOK_DESCRIPTION},{"model",TOK_MODEL},
    {"import",TOK_IMPORT},
    {"define",TOK_DEFINE},{"as",TOK_AS},{"to",TOK_TO},{"with",TOK_WITH},
    {"from",TOK_FROM},{"using",TOK_USING},{"where",TOK_WHERE},
    {"save",TOK_SAVE},{"into",TOK_INTO},{"called",TOK_CALLED},
    {"purpose",TOK_PURPOSE},{"configure",TOK_CONFIGURE},
    {"ask",TOK_ASK},{"AI",TOK_AI},
    {"gather",TOK_GATHER},{"check",TOK_CHECK},{"analyze",TOK_ANALYZE},
    {"if",TOK_IF},{"otherwise",TOK_OTHERWISE},
    {"repeat",TOK_REPEAT},{"for",TOK_FOR},{"each",TOK_EACH},{"in",TOK_IN},
    {"while",TOK_WHILE},{"stop",TOK_STOP},{"skip",TOK_SKIP},
    {"match",TOK_MATCH},{"when",TOK_WHEN},
    {"is",TOK_IS},{"are",TOK_ARE},{"above",TOK_ABOVE},{"below",TOK_BELOW},
    {"equal",TOK_EQUAL},{"greater",TOK_GREATER},{"less",TOK_LESS},
    {"than",TOK_THAN},{"at",TOK_AT},{"least",TOK_LEAST},{"most",TOK_MOST},
    {"not",TOK_NOT},{"between",TOK_BETWEEN},
    {"and",TOK_AND},{"or",TOK_OR},
    {"run",TOK_RUN},{"respond",TOK_RESPOND},{"notify",TOK_NOTIFY},
    {"wait",TOK_WAIT},{"store",TOK_STORE},{"emit",TOK_EMIT},
    {"log",TOK_LOG},{"set",TOK_SET},{"show",TOK_SHOW},{"send",TOK_SEND},
    {"apply",TOK_APPLY},{"needs",TOK_NEEDS},{"approval",TOK_APPROVAL},
    {"then",TOK_THEN},
    {"middleware",TOK_MIDDLEWARE},{"proxy",TOK_PROXY},{"forward",TOK_FORWARD},
    {"try",TOK_TRY},{"on_error",TOK_ON_ERROR},{"finally",TOK_FINALLY},{"catch",TOK_CATCH},
    {"assert",TOK_ASSERT},{"test",TOK_TEST},
    {"async",TOK_ASYNC},{"await",TOK_AWAIT},{"yield",TOK_YIELD},
    {"agent",TOK_AGENT},
    {"text",TOK_TEXT},{"number",TOK_NUMBER},{"yesno",TOK_YESNO},
    {"list",TOK_LIST_TYPE},{"record",TOK_RECORD},{"optional",TOK_OPTIONAL},
    {"milliseconds",TOK_MILLISECONDS},{"millisecond",TOK_MILLISECONDS},{"ms",TOK_MILLISECONDS},
    {"seconds",TOK_SECONDS},{"second",TOK_SECONDS},
    {"minutes",TOK_MINUTES},{"minute",TOK_MINUTES},
    {"hours",TOK_HOURS},{"hour",TOK_HOURS},
    {"days",TOK_DAYS},{"day",TOK_DAYS},
    {"api",TOK_API},{"runs",TOK_RUNS},{"does",TOK_DOES},
    {"GET",TOK_HTTP_GET},{"POST",TOK_HTTP_POST},{"PUT",TOK_HTTP_PUT},
    {"DELETE",TOK_HTTP_DELETE},{"PATCH",TOK_HTTP_PATCH},
    {"on",TOK_ON},{"event",TOK_EVENT},{"every",TOK_EVERY},
    {"yes",TOK_TRUE},{"true",TOK_TRUE},{"no",TOK_FALSE},{"false",TOK_FALSE},
    {"nothing",TOK_NONE_LIT},{"none",TOK_NONE_LIT},
    /* Generics, interfaces, decorators */
    {"interface",TOK_INTERFACE},{"implements",TOK_IMPLEMENTS},
    {"requires",TOK_REQUIRES},{"of",TOK_OF},{"type",TOK_TYPE},
    {"operator",TOK_OPERATOR},
    {NULL, TOK_EOF},
};

/* ═══════════════════════════════════════════════════════════
 *  Synonym Engine — NC speaks your language
 *
 *  NC accepts multiple ways of saying the same thing, just
 *  like English does. "return" works like "respond with",
 *  "print" works like "show", "else" works like "otherwise".
 *
 *  This is what makes the "plain English" claim honest —
 *  no other programming language does this.
 * ═══════════════════════════════════════════════════════════ */

typedef struct { const char *synonym; const char *canonical; NcTokenType type; } Synonym;

static const Synonym synonyms[] = {
    /* Control flow — from Python, Java, C, JS, Go, Ruby, Rust */
    {"else",      "otherwise",    TOK_OTHERWISE},
    {"elif",      "otherwise if", TOK_OTHERWISE},   /* multi-token: elif → otherwise + if */
    {"loop",      "repeat",       TOK_REPEAT},
    {"foreach",   "repeat",       TOK_REPEAT},
    {"switch",    "match",        TOK_MATCH},
    {"case",      "when",         TOK_WHEN},
    {"break",     "stop",         TOK_STOP},
    {"continue",  "skip",         TOK_SKIP},

    /* Functions / behaviors — from every language */
    {"def",       "to",           TOK_TO},
    {"function",  "to",           TOK_TO},
    {"func",      "to",           TOK_TO},
    {"fn",        "to",           TOK_TO},

    /* Return — maps to respond (parser handles missing "with") */
    {"return",    "respond",      TOK_RESPOND},
    {"give",      "respond",      TOK_RESPOND},
    {"yield",     "respond",      TOK_RESPOND},

    /* Output — from every language */
    {"print",     "show",         TOK_SHOW},
    {"puts",      "show",         TOK_SHOW},
    {"display",   "show",         TOK_SHOW},
    {"output",    "show",         TOK_SHOW},

    /* Variables — from JS, Go, Rust, etc. */
    {"var",       "set",          TOK_SET},
    {"let",       "set",          TOK_SET},
    {"const",     "set",          TOK_SET},

    /* Null/None — from every language */
    {"null",      "nothing",      TOK_NONE_LIT},
    {"nil",       "nothing",      TOK_NONE_LIT},
    {"None",      "nothing",      TOK_NONE_LIT},
    {"undefined", "nothing",      TOK_NONE_LIT},
    {"void",      "nothing",      TOK_NONE_LIT},

    /* Boolean — from C, Java, etc. */
    {"True",      "yes",          TOK_TRUE},
    {"False",     "no",           TOK_FALSE},
    {"TRUE",      "yes",          TOK_TRUE},
    {"FALSE",     "no",           TOK_FALSE},

    /* Error handling — from Python, Java, JS */
    {"catch",     "on_error",     TOK_ON_ERROR},
    {"except",    "on_error",     TOK_ON_ERROR},
    {"rescue",    "on_error",     TOK_ON_ERROR},

    /* Import — aliases */
    {"require",   "import",       TOK_IMPORT},
    {"include",   "import",       TOK_IMPORT},
    {"use",       "import",       TOK_IMPORT},

    /* Class/struct — from OOP languages */
    {"class",     "define",       TOK_DEFINE},
    {"struct",    "define",       TOK_DEFINE},

    {NULL, NULL, TOK_EOF},
};

static NcTokenType lookup_synonym(const char *start, int len) {
    for (int i = 0; synonyms[i].synonym != NULL; i++) {
        if ((int)strlen(synonyms[i].synonym) == len &&
            memcmp(synonyms[i].synonym, start, len) == 0)
            return synonyms[i].type;
    }
    return TOK_EOF;
}

static const char *synonym_canonical(const char *start, int len) {
    for (int i = 0; synonyms[i].synonym != NULL; i++) {
        if ((int)strlen(synonyms[i].synonym) == len &&
            memcmp(synonyms[i].synonym, start, len) == 0)
            return synonyms[i].canonical;
    }
    return NULL;
}

static bool nc_synonym_verbose(void) {
    static int cached = -1;
    if (cached < 0) {
        const char *v = getenv("NC_SYNONYM_NOTICES");
        cached = (v && v[0] == '1') ? 1 : 0;
    }
    return cached == 1;
}

static NcTokenType lookup_keyword(const char *start, int len) {
    for (int i = 0; keywords[i].word != NULL; i++) {
        if ((int)strlen(keywords[i].word) == len &&
            memcmp(keywords[i].word, start, len) == 0)
            return keywords[i].type;
    }
    return TOK_IDENTIFIER;
}

/* ── Lexer helpers ─────────────────────────────────────────── */

static void emit(NcLexer *lex, NcTokenType type, const char *start, int len, int line) {
    if (lex->token_count >= lex->token_capacity) {
        int new_cap = lex->token_capacity * 2;
        NcToken *tmp = realloc(lex->tokens, sizeof(NcToken) * new_cap);
        if (!tmp) return;
        lex->tokens = tmp;
        lex->token_capacity = new_cap;
    }
    NcToken *t = &lex->tokens[lex->token_count++];
    t->type = type;
    t->start = start;
    t->length = len;
    t->line = line;
    t->column = (int)(start - lex->line_start) + 1;
    memset(&t->literal, 0, sizeof(t->literal));
}

static void emit_int(NcLexer *lex, const char *start, int len, int line, int64_t val) {
    emit(lex, TOK_INTEGER, start, len, line);
    lex->tokens[lex->token_count - 1].literal.int_val = val;
}

static void emit_float(NcLexer *lex, const char *start, int len, int line, double val) {
    emit(lex, TOK_FLOAT_LIT, start, len, line);
    lex->tokens[lex->token_count - 1].literal.float_val = val;
}

/* ── Lexer implementation ──────────────────────────────────── */

NcLexer *nc_lexer_new(const char *source, const char *filename) {
    NcLexer *lex = calloc(1, sizeof(NcLexer));
    lex->source = source;
    lex->filename = filename;
    lex->current = source;
    lex->line_start = source;
    lex->line = 1;
    lex->column = 1;
    lex->indent_stack[0] = 0;
    lex->indent_depth = 0;
    lex->token_capacity = 512;
    lex->tokens = malloc(sizeof(NcToken) * lex->token_capacity);
    lex->token_count = 0;
    lex->escaped_cap = 32;
    lex->escaped_bufs = malloc(sizeof(char *) * lex->escaped_cap);
    lex->escaped_count = 0;
    return lex;
}

void nc_lexer_tokenize(NcLexer *lex) {
    const char *src = lex->source;
    int line = 1;
    const char *line_start_ptr = src;

    while (*src != '\0') {
        /* Find start and end of this line */
        const char *eol = src;
        while (*eol != '\0' && *eol != '\n') eol++;
        int line_len = (int)(eol - src);

        /* Skip blank lines and comments — preserve indent context.
         * Inside indented blocks, blank lines must not produce DEDENT tokens
         * that would prematurely end behavior bodies or option blocks. */
        const char *p = src;
        while (p < eol && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
        if (p >= eol || *p == '\r' ||
            (*p == '/' && p + 1 < eol && *(p + 1) == '/') || *p == '#') {
            /* Emit a newline token but do NOT re-evaluate indentation.
             * The key insight: we skip the indent comparison (lines 243-260)
             * by continuing here, so the indent stack stays untouched. */
            emit(lex, TOK_NEWLINE, src, 0, line);
            line++;
            src = (*eol == '\n') ? eol + 1 : eol;
            line_start_ptr = src;
            continue;
        }

        /* Indentation */
        int indent = (int)(p - src);
        int current_indent = lex->indent_stack[lex->indent_depth];
        if (indent > current_indent) {
            if (lex->indent_depth + 1 >= 63) { /* bounds: indent_stack[64], indices 0-63 */
                emit(lex, TOK_ERROR, p, 0, line);
            } else {
                lex->indent_depth++;
                lex->indent_stack[lex->indent_depth] = indent;
                emit(lex, TOK_INDENT, p, 0, line);
            }
        } else if (indent < current_indent) {
            while (lex->indent_depth > 0 &&
                   lex->indent_stack[lex->indent_depth] > indent) {
                lex->indent_depth--;
                emit(lex, TOK_DEDENT, p, 0, line);
            }
        }

        /* Tokenize the rest of the line */
        lex->line_start = src;
        while (p < eol) {
            /* Skip whitespace */
            while (p < eol && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
            if (p >= eol) break;

            /* Line comment */
            if (*p == '/' && p + 1 < eol && *(p + 1) == '/') break;
            if (*p == '#') break;

            /* Triple-quoted multi-line string """...""" or '''...''' */
            if ((*p == '"' && p + 2 < eol && p[1] == '"' && p[2] == '"') ||
                (*p == '\'' && p + 2 < eol && p[1] == '\'' && p[2] == '\'')) {
                char quote = *p;
                p += 3;
                const char *content_start = p;
                const char *end_ptr = NULL;
                /* Scan forward through the entire remaining source for closing """ or ''' */
                const char *scan = p;
                while (*scan) {
                    if (*scan == quote && scan[1] == quote && scan[2] == quote) {
                        end_ptr = scan;
                        break;
                    }
                    if (*scan == '\n') line++;
                    scan++;
                }
                if (end_ptr) {
                    int content_len = (int)(end_ptr - content_start);
                    /* Normalize \r\n to \n for cross-platform consistency */
                    char *buf = malloc(content_len + 1);
                    if (buf) {
                        int bi = 0;
                        for (int si = 0; si < content_len; si++) {
                            if (content_start[si] == '\r' && si + 1 < content_len && content_start[si + 1] == '\n')
                                continue;
                            buf[bi++] = content_start[si];
                        }
                        buf[bi] = '\0';
                        if (lex->escaped_count >= lex->escaped_cap) {
                            int new_cap = lex->escaped_cap * 2;
                            char **tmp = realloc(lex->escaped_bufs, sizeof(char *) * new_cap);
                            if (tmp) { lex->escaped_bufs = tmp; lex->escaped_cap = new_cap; }
                        }
                        if (lex->escaped_count < lex->escaped_cap) {
                            lex->escaped_bufs[lex->escaped_count++] = buf;
                        }
                        emit(lex, TOK_STRING, buf, bi, line);
                    }
                    p = end_ptr + 3;
                    /* Advance eol past the closing triple-quote */
                    while (*p && *p != '\n') p++;
                    eol = p;
                } else {
                    emit(lex, TOK_ERROR, content_start - 3, (int)(scan - content_start + 3), line);
                    p = scan;
                }
                continue;
            }

            /* String — supports backslash continuation across lines */
            if (*p == '"' || *p == '\'') {
                char quote = *p;
                const char *start = p;
                p++;
                bool has_escape = false;
                bool terminated = false;
                const char *scan = p;
                while (*scan && *scan != quote) {
                    if (*scan == '\\') {
                        has_escape = true;
                        if (scan + 1 < eol) { scan++; }
                        else if (*(scan + 1) == '\n' || *(scan + 1) == '\r') {
                            /* Backslash continuation: skip \ and newline */
                            scan++;
                            if (*scan == '\r' && *(scan + 1) == '\n') scan++;
                            if (*scan == '\n') { scan++; line++; }
                            continue;
                        }
                    }
                    if (*scan == '\n') break;
                    scan++;
                }
                terminated = (*scan == quote);
                if (!has_escape) {
                    while (p < eol && *p != quote) p++;
                    if (!terminated) {
                        emit(lex, TOK_ERROR, start, (int)(p - start), line);
                        continue;
                    }
                    if (p < eol) p++;
                    emit(lex, TOK_STRING, start + 1, (int)(p - start - 2), line);
                } else {
                    int raw_len = (int)(scan - p) + 64;
                    int buf_cap = raw_len + 1;
                    char *buf = malloc(buf_cap);
                    int bi = 0;
                    while (*p && *p != quote) {
                        if (bi >= buf_cap - 2) {
                            buf_cap *= 2;
                            char *tmp = realloc(buf, buf_cap);
                            if (!tmp) { free(buf); buf = NULL; break; }
                            buf = tmp;
                        }
                        if (*p == '\\' && *(p + 1) != '\0') {
                            p++;
                            if (*p == '\n' || *p == '\r') {
                                /* Backslash continuation: skip newline */
                                if (*p == '\r' && *(p + 1) == '\n') p++;
                                if (*p == '\n') { p++; line++; }
                                while (*p == ' ' || *p == '\t') p++;
                                continue;
                            }
                            switch (*p) {
                                case 'n':  buf[bi++] = '\n'; break;
                                case 't':  buf[bi++] = '\t'; break;
                                case 'r':  buf[bi++] = '\r'; break;
                                case '"':  buf[bi++] = '"'; break;
                                case '\'': buf[bi++] = '\''; break;
                                case '\\': buf[bi++] = '\\'; break;
                                default:   buf[bi++] = *p; break;
                            }
                        } else if (*p == '\n') {
                            break;
                        } else {
                            buf[bi++] = *p;
                        }
                        p++;
                    }
                    if (!buf) { emit(lex, TOK_ERROR, start, 0, line); continue; }
                    buf[bi] = '\0';
                    if (!terminated) {
                        free(buf);
                        emit(lex, TOK_ERROR, start, (int)(p - start), line);
                        continue;
                    }
                    if (*p == quote) p++;
                    /* If we crossed lines via backslash continuation, update eol */
                    if (p > eol) {
                        while (*p && *p != '\n') p++;
                        eol = p;
                    }
                    if (lex->escaped_count >= lex->escaped_cap) {
                        int new_cap = lex->escaped_cap * 2;
                        char **tmp = realloc(lex->escaped_bufs, sizeof(char *) * new_cap);
                        if (!tmp) { free(buf); continue; }
                        lex->escaped_bufs = tmp;
                        lex->escaped_cap = new_cap;
                    }
                    lex->escaped_bufs[lex->escaped_count++] = buf;
                    emit(lex, TOK_STRING, buf, bi, line);
                }
                continue;
            }

            /* Template {{…}} */
            if (*p == '{' && p + 1 < eol && *(p + 1) == '{') {
                const char *start = p;
                p += 2;
                const char *expr_start = p;
                while (p + 1 < eol && !(*p == '}' && *(p + 1) == '}')) p++;
                int expr_len = (int)(p - expr_start);
                if (*p == '}') p += 2;
                emit(lex, TOK_TEMPLATE, expr_start, expr_len, line);
                continue;
            }

            /* Number */
            if (isdigit(*p)) {
                const char *start = p;
                bool has_dot = false;
                while (p < eol && (isdigit(*p) || *p == '.')) {
                    if (*p == '.') {
                        if (has_dot) break;
                        has_dot = true;
                    }
                    p++;
                }
                int len = (int)(p - start);
                if (has_dot) {
                    emit_float(lex, start, len, line, atof(start));
                } else {
                    emit_int(lex, start, len, line, atoll(start));
                }
                continue;
            }

            /* Word (identifier or keyword) */
            if (isalpha(*p) || *p == '_') {
                const char *start = p;
                while (p < eol && (isalnum(*p) || *p == '_')) p++;
                int len = (int)(p - start);
                NcTokenType type = lookup_keyword(start, len);

                /* Synonym engine: if not a native keyword, check synonyms */
                if (type == TOK_IDENTIFIER) {
                    NcTokenType syn = lookup_synonym(start, len);
                    if (syn != TOK_EOF) {
                        const char *canon = synonym_canonical(start, len);
                        if (nc_synonym_verbose() && canon) {
                            char word[64] = {0};
                            int wl = len < 63 ? len : 63;
                            memcpy(word, start, wl);
                            fprintf(stderr, "  (NC: \"%s\" → \"%s\")\n", word, canon);
                        }
                        /* Special: "elif" → emit OTHERWISE then IF */
                        if (len == 4 && memcmp(start, "elif", 4) == 0) {
                            emit(lex, TOK_OTHERWISE, start, len, line);
                            emit(lex, TOK_IF, start, 0, line);
                            continue;
                        }
                        type = syn;
                    }
                }

                /* Context-sensitive: time words are only keywords after numbers */
                if ((type == TOK_MILLISECONDS || type == TOK_SECONDS || type == TOK_MINUTES || type == TOK_HOURS || type == TOK_DAYS) &&
                    lex->token_count > 0 && 
                    lex->tokens[lex->token_count - 1].type != TOK_INTEGER &&
                    lex->tokens[lex->token_count - 1].type != TOK_FLOAT_LIT) {
                    type = TOK_IDENTIFIER;
                }
                /* 'text', 'number', 'list', 'record' are only type keywords after 'is' */
                if ((type == TOK_TEXT || type == TOK_NUMBER || type == TOK_LIST_TYPE || type == TOK_RECORD) &&
                    lex->token_count > 0 &&
                    lex->tokens[lex->token_count - 1].type != TOK_IS) {
                    type = TOK_IDENTIFIER;
                }
                /* 'check', 'gather', 'analyze', 'model' are only keywords at
                 * statement position (after NEWLINE, INDENT, or at start of
                 * input). In any expression context, treat as identifier so
                 * they can be used as variable names. */
                if ((type == TOK_CHECK || type == TOK_GATHER || type == TOK_ANALYZE || type == TOK_MODEL) &&
                    lex->token_count > 0) {
                    NcTokenType prev = lex->tokens[lex->token_count - 1].type;
                    if (prev != TOK_NEWLINE && prev != TOK_INDENT && prev != TOK_DEDENT) {
                        type = TOK_IDENTIFIER;
                    }
                }
                /* Synonyms that map to 'to' (def/func/fn/method) should only
                 * apply at statement position. Inside KV blocks, 'method' is
                 * just a key name, not a behavior definition. Check if the
                 * original word was NOT 'to' itself. */
                if (type == TOK_TO && lex->token_count > 0) {
                    bool is_native_to = (len == 2 && memcmp(start, "to", 2) == 0);
                    if (!is_native_to) {
                        NcTokenType prev = lex->tokens[lex->token_count - 1].type;
                        if (prev != TOK_NEWLINE && prev != TOK_INDENT &&
                            prev != TOK_DEDENT && lex->token_count != 0) {
                            type = TOK_IDENTIFIER;
                        }
                    }
                }
                emit(lex, type, start, len, line);
                continue;
            }

            /* Symbols */
            if (*p == '-' && p + 1 < eol && *(p + 1) == '>') {
                emit(lex, TOK_ARROW, p, 2, line);
                p += 2;
                continue;
            }

            /* Spread operator ... (before single-char dot) */
            if (*p == '.' && p[1] == '.' && p[2] == '.') {
                emit(lex, TOK_SPREAD, p, 3, line);
                p += 3;
                continue;
            }

            /* Two-char operators: ==, !=, >=, <= (must check before single-char switch) */
            if (*p == '=' && p + 1 < eol && *(p+1) == '=') {
                emit(lex, TOK_EQEQ_SIGN, p, 2, line); p += 2; continue;
            }
            if (*p == '!' && p + 1 < eol && *(p+1) == '=') {
                emit(lex, TOK_BANGEQ_SIGN, p, 2, line); p += 2; continue;
            }
            if (*p == '>' && p + 1 < eol && *(p+1) == '=') {
                emit(lex, TOK_GTE_SIGN, p, 2, line); p += 2; continue;
            }
            if (*p == '<' && p + 1 < eol && *(p+1) == '=') {
                emit(lex, TOK_LTE_SIGN, p, 2, line); p += 2; continue;
            }

            NcTokenType sym = TOK_ERROR;
            switch (*p) {
                case ':': sym = TOK_COLON; break;
                case ',': sym = TOK_COMMA; break;
                case '@': sym = TOK_AT_SIGN; break;
                case '.': sym = TOK_DOT; break;
                case '+': sym = TOK_PLUS; break;
                case '-': sym = TOK_MINUS; break;
                case '*': sym = TOK_STAR; break;
                case '/': sym = TOK_SLASH; break;
                case '%': sym = TOK_PERCENT; break;
                case '(': sym = TOK_LPAREN; break;
                case ')': sym = TOK_RPAREN; break;
                case '[': sym = TOK_LBRACKET; break;
                case ']': sym = TOK_RBRACKET; break;
                case '{': sym = TOK_LBRACE; break;
                case '}': sym = TOK_RBRACE; break;
                case '=': sym = TOK_EQUALS_SIGN; break;
                case '>': sym = TOK_GT_SIGN; break;
                case '<': sym = TOK_LT_SIGN; break;
            }
            if (sym != TOK_ERROR) {
                emit(lex, sym, p, 1, line);
                p++;
                continue;
            }

            /* Unknown character — skip */
            p++;
        }

        emit(lex, TOK_NEWLINE, eol, 0, line);
        line++;
        src = (*eol == '\n') ? eol + 1 : eol;
        line_start_ptr = src;
    }

    /* Close remaining indents */
    while (lex->indent_depth > 0) {
        lex->indent_depth--;
        emit(lex, TOK_DEDENT, src, 0, line);
    }

    emit(lex, TOK_EOF, src, 0, line);
}

void nc_lexer_free(NcLexer *lex) {
    if (!lex) return;
    for (int i = 0; i < lex->escaped_count; i++)
        free(lex->escaped_bufs[i]);
    free(lex->escaped_bufs);
    free(lex->tokens);
    free(lex);
}
