/*
 * nc_parser.c — Recursive descent parser for NC.
 *
 * Converts a token stream into an AST.  This is the grammar engine
 * that understands plain English NC syntax:
 *
 *   service "hello"
 *   to greet with name:
 *       respond with "Hello, " + name
 */

#include "../include/nc.h"

/* ── Parser helpers ────────────────────────────────────────── */

static NcToken *cur(NcParser *p) {
    static NcToken eof_sentinel = { .type = TOK_EOF, .start = "", .length = 0, .line = 0 };
    if (p->count == 0) return &eof_sentinel;
    return (p->pos < p->count) ? &p->tokens[p->pos] : &p->tokens[p->count - 1];
}

static NcToken *peek(NcParser *p, int offset) {
    int idx = p->pos + offset;
    return (idx < p->count) ? &p->tokens[idx] : &p->tokens[p->count - 1];
}

static bool at_end(NcParser *p) { return cur(p)->type == TOK_EOF; }

static NcToken *advance(NcParser *p) {
    NcToken *t = cur(p);
    if (!at_end(p)) p->pos++;
    return t;
}

static bool check(NcParser *p, NcTokenType type) { return cur(p)->type == type; }

static bool match(NcParser *p, NcTokenType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static NcToken *expect(NcParser *p, NcTokenType type, const char *what) {
    if (check(p, type)) return advance(p);
    NcToken *got = cur(p);

    const char *got_desc = got->length > 0 ? "" : "end of line";
    char got_word[64] = {0};
    if (got->length > 0 && got->length < 60) {
        memcpy(got_word, got->start, got->length);
        got_word[got->length] = '\0';
        got_desc = got_word;
    }

    const char *suggestion = nc_suggest_builtin(got_desc);
    if (suggestion) {
        snprintf(p->error_msg, sizeof(p->error_msg),
                 "Line %d: I expected %s here, but found '%s'.\n"
                 "  Is this what you meant → '%s'?",
                 got->line, what, got_desc, suggestion);
    } else {
        snprintf(p->error_msg, sizeof(p->error_msg),
                 "Line %d: I expected %s here, but found '%s'.\n"
                 "  Check your spelling, or try 'nc help' for valid syntax.",
                 got->line, what, got_desc);
    }
    p->had_error = true;
    return cur(p);
}

static void skip_newlines(NcParser *p) {
    while (!at_end(p) && check(p, TOK_NEWLINE)) advance(p);
}

static NcString *tok_string(NcToken *t) {
    return nc_string_new(t->start, t->length);
}

/* ── Forward declarations ──────────────────────────────────── */

static NcASTNode *parse_expression(NcParser *p);
static NcASTNode *parse_statement(NcParser *p);

/* ── Config block parsing ──────────────────────────────────── */

static NcMap *parse_kv_block(NcParser *p);

static NcValue parse_simple_value(NcParser *p) {
    NcToken *t = cur(p);
    if (t->type == TOK_STRING)    { advance(p); return NC_STRING(tok_string(t)); }
    if (t->type == TOK_INTEGER)   { advance(p); return NC_INT(t->literal.int_val); }
    if (t->type == TOK_FLOAT_LIT) { advance(p); return NC_FLOAT(t->literal.float_val); }
    if (t->type == TOK_TRUE)      { advance(p); return NC_BOOL(true); }
    if (t->type == TOK_FALSE)     { advance(p); return NC_BOOL(false); }
    if (t->type == TOK_NONE_LIT)  { advance(p); return NC_NONE(); }
    if (t->type == TOK_IDENTIFIER){ advance(p); return NC_STRING(tok_string(t)); }
    if (t->type == TOK_LBRACKET) {
        advance(p);
        NcList *list = nc_list_new();
        while (!check(p, TOK_RBRACKET) && !at_end(p)) {
            while (check(p, TOK_COMMA) || check(p, TOK_NEWLINE)) advance(p);
            if (check(p, TOK_RBRACKET) || at_end(p)) break;
            nc_list_push(list, parse_simple_value(p));
            match(p, TOK_COMMA);
        }
        expect(p, TOK_RBRACKET, "]");
        return NC_LIST(list);
    }
    advance(p);
    return NC_STRING(tok_string(t));
}

static NcMap *parse_kv_block(NcParser *p) {
    NcMap *map = nc_map_new();
    if (!match(p, TOK_INDENT)) return map;
    skip_newlines(p);
    while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
        if (check(p, TOK_NEWLINE)) { advance(p); continue; }
        NcToken *key_tok = advance(p);
        NcString *key = tok_string(key_tok);

        /* Compound key: "save as: X" → key="save as", val=X
         * Handles multi-word keys where the colon/is follows the Nth word.
         * Accumulate words until we find one whose NEXT token is : or is. */
        while (!check(p, TOK_COLON) && !check(p, TOK_IS) && !check(p, TOK_EQUALS_SIGN) &&
               !check(p, TOK_NEWLINE) && !check(p, TOK_DEDENT) && !at_end(p)) {
            NcToken *after = peek(p, 1);
            if (after->type == TOK_COLON || after->type == TOK_IS) {
                NcToken *next = cur(p);
                size_t needed = strlen(key->chars) + 1 + next->length + 1;
                char compound[512];
                if (needed > sizeof(compound)) {
                    snprintf(p->error_msg, sizeof(p->error_msg),
                             "Line %d: Configuration key is too long (max 510 characters).",
                             next->line);
                    p->had_error = true;
                    break;
                }
                snprintf(compound, sizeof(compound), "%.*s %.*s",
                         (int)strlen(key->chars), key->chars,
                         next->length, next->start);
                nc_string_free(key);
                key = nc_string_from_cstr(compound);
                advance(p);
                break;
            }
            /* More than 2-word key: keep accumulating */
            NcToken *next = cur(p);
            size_t needed = strlen(key->chars) + 1 + next->length + 1;
            char compound[512];
            if (needed > sizeof(compound)) {
                snprintf(p->error_msg, sizeof(p->error_msg),
                         "Line %d: Configuration key is too long (max 510 characters).",
                         next->line);
                p->had_error = true;
                break;
            }
            snprintf(compound, sizeof(compound), "%.*s %.*s",
                     (int)strlen(key->chars), key->chars,
                     next->length, next->start);
            nc_string_free(key);
            key = nc_string_from_cstr(compound);
            advance(p);
        }

        if (match(p, TOK_COLON) || match(p, TOK_EQUALS_SIGN)) {
            skip_newlines(p);
            if (check(p, TOK_INDENT)) {
                NcMap *nested = parse_kv_block(p);
                nc_map_set(map, key, NC_MAP(nested));
            } else {
                nc_map_set(map, key, parse_simple_value(p));
            }
        } else if (match(p, TOK_IS)) {
            nc_map_set(map, key, parse_simple_value(p));
        }
        skip_newlines(p);
    }
    match(p, TOK_DEDENT);
    return map;
}

/* ── Define block ──────────────────────────────────────────── */

static NcASTNode *parse_definition(NcParser *p) {
    NcToken *start = advance(p); /* define */
    NcToken *name_tok = expect(p, TOK_IDENTIFIER, "type name");

    /* Parse "implements InterfaceA, InterfaceB" before "as:" */
    NcString **impl_list = NULL;
    int impl_count = 0;
    if (match(p, TOK_IMPLEMENTS)) {
        impl_list = malloc(sizeof(NcString *) * 8);
        do {
            NcToken *iface = expect(p, TOK_IDENTIFIER, "interface name");
            if (impl_count < 8) impl_list[impl_count++] = tok_string(iface);
        } while (match(p, TOK_COMMA));
    }

    expect(p, TOK_AS, "as");
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_DEFINITION, start->line);
    node->as.definition.name = tok_string(name_tok);
    node->as.definition.fields = NULL;
    node->as.definition.field_count = 0;

    int cap = 16;
    NcASTNode **fields = malloc(sizeof(NcASTNode *) * cap);
    int count = 0;

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            NcToken *fname_tok = advance(p);
            expect(p, TOK_IS, "is");
            NcToken *ftype_tok = advance(p);
            bool optional = match(p, TOK_OPTIONAL);

            NcASTNode *field = nc_ast_new(NODE_FIELD, fname_tok->line);
            field->as.field.name = tok_string(fname_tok);
            field->as.field.type_name = tok_string(ftype_tok);
            field->as.field.optional = optional;

            if (count >= cap) { cap *= 2; NcASTNode **tmp = realloc(fields, sizeof(NcASTNode *) * cap); if (!tmp) { p->had_error = true; break; } fields = tmp; }
            fields[count++] = field;
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }

    node->as.definition.fields = fields;
    node->as.definition.field_count = count;
    node->as.definition.implements = impl_list;
    node->as.definition.impl_count = impl_count;
    node->as.definition.operators = NULL;
    node->as.definition.op_count = 0;
    return node;
}

/* ── Interface ─────────────────────────────────────────────── */

static NcASTNode *parse_interface(NcParser *p) {
    NcToken *start = advance(p); /* interface */
    NcToken *name_tok = expect(p, TOK_IDENTIFIER, "interface name");
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_INTERFACE, start->line);
    node->as.interface_def.name = tok_string(name_tok);
    node->as.interface_def.required_behaviors = malloc(sizeof(NcString *) * 16);
    node->as.interface_def.required_count = 0;

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            /* Parse "requires behavior_name" */
            if (match(p, TOK_REQUIRES)) {
                NcToken *req = expect(p, TOK_IDENTIFIER, "required behavior name");
                if (node->as.interface_def.required_count < 16)
                    node->as.interface_def.required_behaviors[node->as.interface_def.required_count++] = tok_string(req);
            } else {
                advance(p); /* skip unknown tokens in interface body */
            }
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    return node;
}

/* ── Decorator parsing ────────────────────────────────────── */

static NcASTNode *parse_decorator(NcParser *p) {
    NcToken *start = advance(p); /* @ */
    NcToken *name_tok = expect(p, TOK_IDENTIFIER, "decorator name");

    NcASTNode *node = nc_ast_new(NODE_DECORATOR, start->line);
    node->as.decorator.name = tok_string(name_tok);
    node->as.decorator.options = NULL;

    /* Parse optional decorator arguments: @cache ttl 60 */
    if (!check(p, TOK_NEWLINE) && !check(p, TOK_AT_SIGN) && !at_end(p)) {
        NcMap *opts = nc_map_new();
        while (!check(p, TOK_NEWLINE) && !check(p, TOK_AT_SIGN) && !at_end(p)) {
            NcToken *key = advance(p);
            NcString *key_str = tok_string(key);
            if (check(p, TOK_INTEGER)) {
                NcToken *val = advance(p);
                nc_map_set(opts, key_str, NC_INT(val->literal.int_val));
            } else if (check(p, TOK_STRING)) {
                NcToken *val = advance(p);
                nc_map_set(opts, key_str, NC_STRING(tok_string(val)));
            } else if (check(p, TOK_TRUE) || check(p, TOK_FALSE)) {
                NcToken *val = advance(p);
                nc_map_set(opts, key_str, NC_BOOL(val->type == TOK_TRUE));
            } else {
                nc_map_set(opts, key_str, NC_BOOL(true));
            }
        }
        node->as.decorator.options = opts;
    }
    skip_newlines(p);
    return node;
}

/* ── Behavior ──────────────────────────────────────────────── */

static NcASTNode **parse_body(NcParser *p, int *out_count);

static NcASTNode *parse_behavior(NcParser *p) {
    NcToken *start = advance(p); /* to */

    /* Collect phrase words until : or 'with' */
    char phrase[1024] = {0};
    char name[1024] = {0};
    int name_len = 0;

    while (!check(p, TOK_COLON) && !check(p, TOK_WITH) &&
           !check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
        NcToken *t = advance(p);
        size_t prem = sizeof(phrase) - strlen(phrase) - 1;
        if (phrase[0] && prem > 0) { strncat(phrase, " ", prem); prem--; }
        if (prem > 0) {
            size_t copy = prem < (size_t)t->length ? prem : (size_t)t->length;
            strncat(phrase, t->start, copy);
        }
        if (isalnum((unsigned char)t->start[0]) || t->start[0] == '_') {
            int needed = (name_len > 0 ? 1 : 0) + t->length;
            if (name_len + needed < (int)sizeof(name) - 1) {
                if (name_len > 0) name[name_len++] = '_';
                memcpy(name + name_len, t->start, t->length);
                name_len += t->length;
            }
        }
    }
    name[name_len] = '\0';

    /* Parameters */
    int param_cap = 8, param_count = 0;
    NcASTNode **params = malloc(sizeof(NcASTNode *) * param_cap);

    if (match(p, TOK_WITH)) {
        while (!check(p, TOK_COLON) && !check(p, TOK_EOF) && !check(p, TOK_NEWLINE)) {
            /* Any word token is valid as a param name — reserved words like
             * description, model, event, name, type, status, etc. all work. */
            NcToken *pname = cur(p);
            if (pname->type != TOK_COLON && pname->type != TOK_NEWLINE &&
                pname->type != TOK_EOF && pname->type != TOK_COMMA &&
                pname->type != TOK_AND && pname->type != TOK_INDENT &&
                pname->type != TOK_DEDENT) {
                advance(p);
            } else {
                pname = expect(p, TOK_IDENTIFIER, "parameter name");
            }
            NcASTNode *param = nc_ast_new(NODE_PARAM, pname->line);
            param->as.param.name = tok_string(pname);
            if (param_count >= param_cap) {
                param_cap *= 2; NcASTNode **tmp = realloc(params, sizeof(NcASTNode *) * param_cap); if (!tmp) { p->had_error = true; break; } params = tmp;
            }
            params[param_count++] = param;
            if (!match(p, TOK_COMMA) && !match(p, TOK_AND)) break;
        }
    }

    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_BEHAVIOR, start->line);
    node->as.behavior.name = nc_string_from_cstr(name);
    node->as.behavior.purpose = NULL;
    node->as.behavior.params = params;
    node->as.behavior.param_count = param_count;
    node->as.behavior.needs_approval = NULL;
    node->as.behavior.body = NULL;
    node->as.behavior.body_count = 0;
    node->as.behavior.decorators = NULL;
    node->as.behavior.decorator_count = 0;
    node->as.behavior.type_params = NULL;
    node->as.behavior.type_param_count = 0;

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);

        /* Purpose line */
        if (check(p, TOK_PURPOSE)) {
            advance(p);
            match(p, TOK_COLON);
            NcToken *purpose_tok = expect(p, TOK_STRING, "purpose string");
            node->as.behavior.purpose = tok_string(purpose_tok);
            skip_newlines(p);
        }

        /* Needs approval */
        if (check(p, TOK_NEEDS)) {
            advance(p);
            match(p, TOK_APPROVAL);
            if (match(p, TOK_WHEN)) {
                node->as.behavior.needs_approval = parse_expression(p);
            }
            skip_newlines(p);
        }

        int body_count = 0;
        int body_cap = 32;
        NcASTNode **body = malloc(sizeof(NcASTNode *) * body_cap);

        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            skip_newlines(p);
            if (check(p, TOK_DEDENT) || at_end(p)) break;
            int before = p->pos;
            NcASTNode *stmt = parse_statement(p);
            if (stmt) {
                if (body_count >= body_cap) {
                    body_cap *= 2; body = realloc(body, sizeof(NcASTNode *) * body_cap);
                }
                body[body_count++] = stmt;
            }
            if (p->pos == before) advance(p);
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
        node->as.behavior.body = body;
        node->as.behavior.body_count = body_count;
    }

    return node;
}

/* ── Statements ────────────────────────────────────────────── */

static NcASTNode **parse_indented_body(NcParser *p, int *out_count) {
    int cap = 16, count = 0;
    NcASTNode **stmts = malloc(sizeof(NcASTNode *) * cap);
    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            int before = p->pos;
            NcASTNode *s = parse_statement(p);
            if (s) {
                if (count >= cap) { cap *= 2; NcASTNode **tmp = realloc(stmts, sizeof(NcASTNode *) * cap); if (!tmp) { p->had_error = true; break; } stmts = tmp; }
                stmts[count++] = s;
            }
            if (p->pos == before) advance(p);
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    *out_count = count;
    return stmts;
}

static NcASTNode *parse_gather(NcParser *p) {
    NcToken *start = advance(p); /* gather */
    NcToken *target = expect(p, TOK_IDENTIFIER, "target name");
    expect(p, TOK_FROM, "from");
    NcToken *source;
    if (check(p, TOK_STRING))
        source = advance(p);
    else
        source = expect(p, TOK_IDENTIFIER, "source name");

    NcASTNode *node = nc_ast_new(NODE_GATHER, start->line);
    node->as.gather.target = tok_string(target);
    node->as.gather.source = tok_string(source);
    node->as.gather.options = NULL;

    /* Accept additional string argument: gather x from store "name" */
    if (check(p, TOK_STRING)) {
        NcToken *arg = advance(p);
        NcString *combined = nc_string_concat(node->as.gather.source, nc_string_from_cstr(" "));
        NcString *full = nc_string_concat(combined, tok_string(arg));
        nc_string_free(combined);
        nc_string_free(node->as.gather.source);
        node->as.gather.source = full;
    }

    if (match(p, TOK_COLON)) {
        skip_newlines(p);
        if (check(p, TOK_INDENT))
            node->as.gather.options = parse_kv_block(p);
    } else if (match(p, TOK_WHERE)) {
        NcMap *opts = nc_map_new();
        while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF) && !check(p, TOK_DEDENT)) {
            NcToken *k = expect(p, TOK_IDENTIFIER, "key");
            expect(p, TOK_IS, "is");
            NcValue v = parse_simple_value(p);
            nc_map_set(opts, tok_string(k), v);
            match(p, TOK_AND); match(p, TOK_COMMA);
        }
        node->as.gather.options = opts;
    }
    return node;
}

static NcASTNode *parse_ask_ai(NcParser *p) {
    NcToken *start = advance(p); /* ask */
    /* Accept both TOK_AI ("AI") and identifier "ai" (case-insensitive) */
    if (!match(p, TOK_AI)) {
        if (check(p, TOK_IDENTIFIER) && cur(p)->length == 2 &&
            (cur(p)->start[0] == 'a' || cur(p)->start[0] == 'A') &&
            (cur(p)->start[1] == 'i' || cur(p)->start[1] == 'I')) {
            advance(p);
        }
    }
    match(p, TOK_TO);

    NcASTNode *node = nc_ast_new(NODE_ASK_AI, start->line);
    node->as.ask_ai.using = NULL;
    node->as.ask_ai.using_count = 0;
    node->as.ask_ai.save_as = NULL;
    node->as.ask_ai.model = NULL;
    node->as.ask_ai.confidence = 0;
    node->as.ask_ai.options = NULL;

    /* Prompt — string or words */
    if (check(p, TOK_STRING)) {
        node->as.ask_ai.prompt = tok_string(advance(p));
    } else {
        char buf[2048] = {0};
        while (!check(p, TOK_COLON) && !check(p, TOK_USING) &&
               !check(p, TOK_SAVE) && !check(p, TOK_CALLED) &&
               !check(p, TOK_NEWLINE) && !at_end(p)) {
            NcToken *t = advance(p);
            size_t rem = sizeof(buf) - strlen(buf) - 1;
            if (buf[0] && rem > 0) { strncat(buf, " ", rem); rem--; }
            if (rem > 0) strncat(buf, t->start, rem < (size_t)t->length ? rem : (size_t)t->length);
        }
        node->as.ask_ai.prompt = nc_string_from_cstr(buf);
    }

    /* using — can be "using VAR, VAR" or "using model STRING" or "using context VAR" */
    int using_cap = 8, using_count = 0;
    NcString **using_names = malloc(sizeof(NcString *) * using_cap);

    if (match(p, TOK_USING)) {
        /* "using model STRING" — inline model specification */
        if (check(p, TOK_MODEL) ||
            (check(p, TOK_IDENTIFIER) && cur(p)->length == 5 &&
             strncmp(cur(p)->start, "model", 5) == 0)) {
            advance(p); /* consume 'model' */
            if (check(p, TOK_STRING)) {
                node->as.ask_ai.model = tok_string(advance(p));
            }
        } else {
            /* "using VAR, VAR, ..." — context variables */
            while (check(p, TOK_IDENTIFIER) || check(p, TOK_TEXT) ||
                   check(p, TOK_EVENT) || check(p, TOK_RECORD)) {
                if (using_count >= using_cap) {
                    using_cap *= 2;
                    using_names = realloc(using_names, sizeof(NcString *) * using_cap);
                }
                using_names[using_count++] = tok_string(advance(p));
                match(p, TOK_COMMA); match(p, TOK_AND);
            }
        }
    }
    node->as.ask_ai.using = using_names;
    node->as.ask_ai.using_count = using_count;

    /* Check for a second "using model" after context variables:
     * e.g. ask AI to "..." using context using model "my-model" save as result */
    if (match(p, TOK_USING)) {
        if (check(p, TOK_MODEL) ||
            (check(p, TOK_IDENTIFIER) && cur(p)->length == 5 &&
             strncmp(cur(p)->start, "model", 5) == 0)) {
            advance(p);
            if (check(p, TOK_STRING))
                node->as.ask_ai.model = tok_string(advance(p));
        }
    }

    /* save as / called */
    while (!check(p, TOK_NEWLINE) && !check(p, TOK_COLON) && !at_end(p) && !check(p, TOK_DEDENT)) {
        if (match(p, TOK_SAVE)) {
            match(p, TOK_AS);
            node->as.ask_ai.save_as = tok_string(expect(p, TOK_IDENTIFIER, "variable name"));
        } else if (match(p, TOK_CALLED)) {
            node->as.ask_ai.save_as = tok_string(expect(p, TOK_IDENTIFIER, "variable name"));
        } else {
            advance(p);
        }
    }

    /* Indented options */
    if (match(p, TOK_COLON)) {
        skip_newlines(p);
        if (check(p, TOK_INDENT)) {
            NcMap *raw = parse_kv_block(p);
            NcString *save_key = nc_string_from_cstr("save");
            NcString *as_key = nc_string_from_cstr("as");
            NcString *save_as_key = nc_string_from_cstr("save as");
            NcString *conf_key = nc_string_from_cstr("confidence");
            NcString *model_key = nc_string_from_cstr("model");
            /* "save as: X" gets parsed as key "as" with value "X" by kv parser */
            if (nc_map_has(raw, save_key) && !node->as.ask_ai.save_as) {
                NcValue sv = nc_map_get(raw, save_key);
                if (IS_STRING(sv)) node->as.ask_ai.save_as = AS_STRING(sv);
            }
            if (nc_map_has(raw, as_key) && !node->as.ask_ai.save_as) {
                NcValue av = nc_map_get(raw, as_key);
                if (IS_STRING(av)) node->as.ask_ai.save_as = AS_STRING(av);
            }
            if (nc_map_has(raw, save_as_key) && !node->as.ask_ai.save_as) {
                NcValue sav = nc_map_get(raw, save_as_key);
                if (IS_STRING(sav)) node->as.ask_ai.save_as = AS_STRING(sav);
            }
            if (nc_map_has(raw, conf_key)) {
                NcValue cv = nc_map_get(raw, conf_key);
                if (IS_FLOAT(cv)) node->as.ask_ai.confidence = AS_FLOAT(cv);
            }
            if (nc_map_has(raw, model_key)) {
                NcValue mv = nc_map_get(raw, model_key);
                if (IS_STRING(mv)) node->as.ask_ai.model = AS_STRING(mv);
            }
            node->as.ask_ai.options = raw;
            nc_string_free(save_key);
            nc_string_free(as_key);
            nc_string_free(save_as_key);
            nc_string_free(conf_key);
            nc_string_free(model_key);
        }
    }

    return node;
}

static NcASTNode *parse_if(NcParser *p) {
    NcToken *start = advance(p); /* if */
    NcASTNode *cond = parse_expression(p);
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_IF, start->line);
    node->as.if_stmt.condition = cond;
    node->as.if_stmt.then_body = parse_indented_body(p, &node->as.if_stmt.then_count);
    node->as.if_stmt.else_body = NULL;
    node->as.if_stmt.else_count = 0;

    skip_newlines(p);
    if (match(p, TOK_OTHERWISE)) {
        if (check(p, TOK_IF)) {
            NcASTNode *elif = parse_if(p);
            node->as.if_stmt.else_body = malloc(sizeof(NcASTNode *));
            node->as.if_stmt.else_body[0] = elif;
            node->as.if_stmt.else_count = 1;
        } else {
            match(p, TOK_COLON);
            skip_newlines(p);
            node->as.if_stmt.else_body = parse_indented_body(p, &node->as.if_stmt.else_count);
        }
    }
    return node;
}

static NcASTNode *parse_while(NcParser *p) {
    NcToken *start = advance(p); /* while */
    NcASTNode *cond = parse_expression(p);
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_WHILE, start->line);
    node->as.while_stmt.condition = cond;
    node->as.while_stmt.body = parse_indented_body(p, &node->as.while_stmt.body_count);
    return node;
}

static NcASTNode *parse_repeat(NcParser *p) {
    NcToken *start = advance(p); /* repeat */

    /* Check for 'repeat while condition:' — synonym for 'while condition:' */
    if (check(p, TOK_WHILE)) {
        advance(p); /* consume 'while' */
        NcASTNode *cond = parse_expression(p);
        match(p, TOK_COLON);
        skip_newlines(p);
        NcASTNode *node = nc_ast_new(NODE_WHILE, start->line);
        node->as.while_stmt.condition = cond;
        node->as.while_stmt.body = parse_indented_body(p, &node->as.while_stmt.body_count);
        return node;
    }

    /* Check for 'repeat N times:' pattern */
    if (check(p, TOK_INTEGER) || check(p, TOK_IDENTIFIER)) {
        NcToken *maybe_count = cur(p);
        if (maybe_count->type == TOK_INTEGER) {
            advance(p);
            /* Expect 'times' keyword (parsed as identifier) */
            if (check(p, TOK_IDENTIFIER) && strncmp(cur(p)->start, "times", 5) == 0) {
                advance(p);
                match(p, TOK_COLON);
                skip_newlines(p);
                NcASTNode *node = nc_ast_new(NODE_FOR_COUNT, start->line);
                NcASTNode *count_expr = nc_ast_new(NODE_INT_LIT, start->line);
                count_expr->as.int_lit.value = maybe_count->literal.int_val;
                node->as.for_count.count_expr = count_expr;
                node->as.for_count.variable = nc_string_from_cstr("__repeat_idx__");
                node->as.for_count.body = parse_indented_body(p, &node->as.for_count.body_count);
                return node;
            }
        }
    }

    match(p, TOK_FOR);
    match(p, TOK_EACH);
    NcToken *var = expect(p, TOK_IDENTIFIER, "variable name");

    NcToken *var2 = NULL;
    if (match(p, TOK_COMMA)) {
        var2 = expect(p, TOK_IDENTIFIER, "value variable name");
    }

    expect(p, TOK_IN, "in");
    NcASTNode *iterable = parse_expression(p);
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_REPEAT, start->line);
    if (var2) {
        node->as.repeat.key_variable = tok_string(var);
        node->as.repeat.variable = tok_string(var2);
    } else {
        node->as.repeat.variable = tok_string(var);
        node->as.repeat.key_variable = NULL;
    }
    node->as.repeat.iterable = iterable;
    node->as.repeat.body = parse_indented_body(p, &node->as.repeat.body_count);
    return node;
}

static NcASTNode *parse_match(NcParser *p) {
    NcToken *start = advance(p); /* match */
    NcASTNode *subj = parse_expression(p);
    if (!subj) return NULL;
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_MATCH, start->line);
    node->as.match_stmt.subject = subj;
    node->as.match_stmt.cases = NULL;
    node->as.match_stmt.case_count = 0;
    node->as.match_stmt.otherwise = NULL;
    node->as.match_stmt.otherwise_count = 0;

    int cap = 8, count = 0;
    NcASTNode **cases = malloc(sizeof(NcASTNode *) * cap);

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            if (match(p, TOK_WHEN)) {
                NcASTNode *val = NULL;

                /* Destructuring: when {name, age}: */
                if (check(p, TOK_LBRACE)) {
                    advance(p);
                    NcASTNode *destr = nc_ast_new(NODE_DESTRUCTURE, cur(p)->line);
                    destr->as.match_guard.fields = malloc(sizeof(NcString *) * 16);
                    destr->as.match_guard.field_count = 0;
                    destr->as.match_guard.guard = NULL;
                    destr->as.match_guard.body = NULL;
                    destr->as.match_guard.body_count = 0;
                    while (!check(p, TOK_RBRACE) && !at_end(p)) {
                        NcToken *f = advance(p);
                        if (destr->as.match_guard.field_count < 16)
                            destr->as.match_guard.fields[destr->as.match_guard.field_count++] = tok_string(f);
                        match(p, TOK_COMMA);
                    }
                    match(p, TOK_RBRACE);
                    val = destr;
                }
                /* List pattern: when [first, ...rest]: */
                else if (check(p, TOK_LBRACKET)) {
                    advance(p);
                    NcASTNode *list_pat = nc_ast_new(NODE_DESTRUCTURE, cur(p)->line);
                    list_pat->as.match_guard.fields = malloc(sizeof(NcString *) * 16);
                    list_pat->as.match_guard.field_count = 0;
                    list_pat->as.match_guard.guard = NULL;
                    list_pat->as.match_guard.body = NULL;
                    list_pat->as.match_guard.body_count = 0;
                    while (!check(p, TOK_RBRACKET) && !at_end(p)) {
                        if (check(p, TOK_SPREAD)) {
                            advance(p);
                            NcToken *rest_name = expect(p, TOK_IDENTIFIER, "rest variable");
                            NcASTNode *rest = nc_ast_new(NODE_REST_PATTERN, rest_name->line);
                            rest->as.rest_pattern.name = tok_string(rest_name);
                            /* Mark rest with special prefix */
                            if (list_pat->as.match_guard.field_count < 16) {
                                char buf[128];
                                snprintf(buf, sizeof(buf), "...%s", rest_name->start);
                                list_pat->as.match_guard.fields[list_pat->as.match_guard.field_count++] = nc_string_from_cstr(buf);
                            }
                        } else {
                            NcToken *f = advance(p);
                            if (list_pat->as.match_guard.field_count < 16)
                                list_pat->as.match_guard.fields[list_pat->as.match_guard.field_count++] = tok_string(f);
                        }
                        match(p, TOK_COMMA);
                    }
                    match(p, TOK_RBRACKET);
                    val = list_pat;
                }
                else {
                    val = parse_expression(p);
                }

                if (!val) { advance(p); skip_newlines(p); continue; }

                /* Guard: when x if x > 10: */
                NcASTNode *guard = NULL;
                if (match(p, TOK_IF)) {
                    guard = parse_expression(p);
                }

                match(p, TOK_COLON);
                skip_newlines(p);
                int bc;
                NcASTNode **body = parse_indented_body(p, &bc);

                if (guard) {
                    /* Wrap in a match_guard node */
                    NcASTNode *guarded = nc_ast_new(NODE_MATCH_GUARD, val->line);
                    guarded->as.match_guard.guard = guard;
                    guarded->as.match_guard.body = body;
                    guarded->as.match_guard.body_count = bc;
                    guarded->as.match_guard.fields = NULL;
                    guarded->as.match_guard.field_count = 0;

                    NcASTNode *when_node = nc_ast_new(NODE_WHEN, val->line);
                    when_node->as.when_clause.value = val;
                    when_node->as.when_clause.body = &guarded;
                    when_node->as.when_clause.body_count = 1;
                    if (count >= cap) { cap *= 2; cases = realloc(cases, sizeof(NcASTNode *) * cap); }
                    cases[count++] = when_node;
                } else {
                    NcASTNode *when_node = nc_ast_new(NODE_WHEN, val->line);
                    when_node->as.when_clause.value = val;
                    when_node->as.when_clause.body = body;
                    when_node->as.when_clause.body_count = bc;
                    if (count >= cap) { cap *= 2; cases = realloc(cases, sizeof(NcASTNode *) * cap); }
                    cases[count++] = when_node;
                }
            } else if (match(p, TOK_OTHERWISE)) {
                match(p, TOK_COLON);
                skip_newlines(p);
                node->as.match_stmt.otherwise = parse_indented_body(p, &node->as.match_stmt.otherwise_count);
            } else { advance(p); }
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    node->as.match_stmt.cases = cases;
    node->as.match_stmt.case_count = count;
    return node;
}

static NcASTNode *parse_set(NcParser *p) {
    NcToken *start = advance(p); /* set */
    NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");

    if (check(p, TOK_DOT)) {
        advance(p);
        NcToken *field_tok = advance(p);

        /* Support set map.field[expr] to val */
        if (check(p, TOK_LBRACKET)) {
            advance(p);
            NcASTNode *idx = parse_expression(p);
            expect(p, TOK_RBRACKET, "]");
            expect(p, TOK_TO, "to");
            NcASTNode *val = parse_expression(p);
            NcASTNode *node = nc_ast_new(NODE_SET_INDEX, start->line);
            node->as.set_index.target = tok_string(target);
            node->as.set_index.field = tok_string(field_tok);
            node->as.set_index.index = idx;
            node->as.set_index.value = val;
            return node;
        }

        /* Support 2-level deep set: set x.a.b to val */
        if (check(p, TOK_DOT)) {
            advance(p);
            NcToken *subfield_tok = advance(p);
            expect(p, TOK_TO, "to");
            NcASTNode *val = parse_expression(p);
            NcASTNode *node = nc_ast_new(NODE_SET, start->line);
            node->as.set_stmt.target = tok_string(target);
            node->as.set_stmt.field = tok_string(field_tok);
            node->as.set_stmt.subfield = tok_string(subfield_tok);
            node->as.set_stmt.value = val;
            return node;
        }

        expect(p, TOK_TO, "to");
        NcASTNode *val = parse_expression(p);
        NcASTNode *node = nc_ast_new(NODE_SET, start->line);
        node->as.set_stmt.target = tok_string(target);
        node->as.set_stmt.field = tok_string(field_tok);
        node->as.set_stmt.subfield = NULL;
        node->as.set_stmt.value = val;
        return node;
    }

    /* Support set map[expr] to val  AND  set map[expr].field to val */
    if (check(p, TOK_LBRACKET)) {
        advance(p);
        NcASTNode *idx = parse_expression(p);
        expect(p, TOK_RBRACKET, "]");

        /* Check for trailing .field after bracket access */
        NcString *trailing_field = NULL;
        if (check(p, TOK_DOT)) {
            advance(p);
            NcToken *field_tok = advance(p);
            trailing_field = tok_string(field_tok);
        }

        expect(p, TOK_TO, "to");
        NcASTNode *val = parse_expression(p);
        NcASTNode *node = nc_ast_new(NODE_SET_INDEX, start->line);
        node->as.set_index.target = tok_string(target);
        node->as.set_index.field = trailing_field;
        node->as.set_index.index = idx;
        node->as.set_index.value = val;
        return node;
    }

    expect(p, TOK_TO, "to");
    NcASTNode *val = parse_expression(p);
    NcASTNode *node = nc_ast_new(NODE_SET, start->line);
    node->as.set_stmt.target = tok_string(target);
    node->as.set_stmt.field = NULL;
    node->as.set_stmt.value = val;
    return node;
}

static NcASTNode *parse_respond(NcParser *p) {
    NcToken *start = advance(p); /* respond */
    match(p, TOK_WITH);
    NcASTNode *node = nc_ast_new(NODE_RESPOND, start->line);
    node->as.single_expr.value = parse_expression(p);
    return node;
}

/* ── agent <name>: ... ─────────────────────────────────────── */
static NcASTNode *parse_agent_def(NcParser *p) {
    NcToken *start = advance(p); /* agent */
    NcToken *name_tok = expect(p, TOK_IDENTIFIER, "agent name");
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_AGENT_DEF, start->line);
    node->as.agent_def.name = tok_string(name_tok);
    node->as.agent_def.purpose = NULL;
    node->as.agent_def.model = NULL;
    node->as.agent_def.tools = NULL;
    node->as.agent_def.tool_count = 0;
    node->as.agent_def.max_steps = 10;

    if (match(p, TOK_INDENT)) {
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            skip_newlines(p);
            if (check(p, TOK_DEDENT) || at_end(p)) break;
            NcToken *key = cur(p);

            /* purpose: "..." */
            if (key->type == TOK_PURPOSE) {
                advance(p); match(p, TOK_COLON);
                node->as.agent_def.purpose = tok_string(expect(p, TOK_STRING, "purpose string"));
            }
            /* model: "..." */
            else if ((key->type == TOK_MODEL) ||
                     (key->type == TOK_IDENTIFIER && key->length == 5 && memcmp(key->start, "model", 5) == 0)) {
                advance(p); match(p, TOK_COLON);
                node->as.agent_def.model = tok_string(expect(p, TOK_STRING, "model name"));
            }
            /* tools: [search, analyze, summarize] */
            else if (key->type == TOK_IDENTIFIER && key->length == 5 && memcmp(key->start, "tools", 5) == 0) {
                advance(p); match(p, TOK_COLON);
                expect(p, TOK_LBRACKET, "[");
                int tcap = 8, tcount = 0;
                NcString **tools = malloc(sizeof(NcString *) * tcap);
                while (!check(p, TOK_RBRACKET) && !at_end(p)) {
                    NcToken *tool_tok = cur(p);
                    advance(p);
                    if (tcount >= tcap) { tcap *= 2; tools = realloc(tools, sizeof(NcString *) * tcap); }
                    tools[tcount++] = nc_string_new(tool_tok->start, tool_tok->length);
                    if (!match(p, TOK_COMMA)) break;
                }
                expect(p, TOK_RBRACKET, "]");
                node->as.agent_def.tools = tools;
                node->as.agent_def.tool_count = tcount;
            }
            /* max_steps: N */
            else if (key->type == TOK_IDENTIFIER && key->length == 9 && memcmp(key->start, "max_steps", 9) == 0) {
                advance(p); match(p, TOK_COLON);
                NcToken *num = expect(p, TOK_INTEGER, "max steps number");
                node->as.agent_def.max_steps = (int)num->literal.int_val;
            }
            else {
                advance(p); /* skip unknown key */
            }
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    return node;
}

static NcASTNode *parse_run(NcParser *p) {
    NcToken *start = advance(p); /* run */

    /* run agent <name> with <prompt> save as <var> */
    if (check(p, TOK_AGENT)) {
        advance(p); /* agent */
        NcToken *agent_name = expect(p, TOK_IDENTIFIER, "agent name");
        NcASTNode *node = nc_ast_new(NODE_RUN_AGENT, start->line);
        node->as.run_agent.agent_name = tok_string(agent_name);
        node->as.run_agent.prompt = NULL;
        node->as.run_agent.save_as = NULL;
        if (match(p, TOK_WITH)) {
            node->as.run_agent.prompt = parse_expression(p);
        }
        skip_newlines(p);
        /* save as: <var>  OR  save as <var> */
        if (check(p, TOK_INDENT)) {
            advance(p); skip_newlines(p);
            if (match(p, TOK_SAVE)) {
                match(p, TOK_AS);
                match(p, TOK_COLON);
                NcToken *var = expect(p, TOK_IDENTIFIER, "variable name");
                node->as.run_agent.save_as = tok_string(var);
            }
            skip_newlines(p);
            match(p, TOK_DEDENT);
        } else if (match(p, TOK_SAVE)) {
            match(p, TOK_AS);
            match(p, TOK_COLON);
            NcToken *var = expect(p, TOK_IDENTIFIER, "variable name");
            node->as.run_agent.save_as = tok_string(var);
        }
        return node;
    }

    NcToken *name = expect(p, TOK_IDENTIFIER, "behavior name");
    NcASTNode *node = nc_ast_new(NODE_RUN, start->line);
    node->as.run_stmt.name = tok_string(name);
    int cap = 8, count = 0;
    NcASTNode **args = malloc(sizeof(NcASTNode *) * cap);
    if (match(p, TOK_WITH)) {
        while (!check(p, TOK_NEWLINE) && !check(p, TOK_EOF) && !check(p, TOK_DEDENT)) {
            NcASTNode *arg = parse_expression(p);
            if (count >= cap) { cap *= 2; args = realloc(args, sizeof(NcASTNode *) * cap); }
            args[count++] = arg;
            if (!match(p, TOK_COMMA) && !match(p, TOK_AND)) break;
        }
    }
    node->as.run_stmt.args = args;
    node->as.run_stmt.arg_count = count;
    return node;
}

static NcASTNode *parse_log(NcParser *p) {
    NcToken *start = advance(p);
    NcASTNode *node = nc_ast_new(NODE_LOG, start->line);
    node->as.single_expr.value = parse_expression(p);
    return node;
}

static NcASTNode *parse_show(NcParser *p) {
    NcToken *start = advance(p);
    NcASTNode *node = nc_ast_new(NODE_SHOW, start->line);
    node->as.single_expr.value = parse_expression(p);
    return node;
}

static NcASTNode *parse_notify(NcParser *p) {
    NcToken *start = advance(p); /* notify */
    NcASTNode *channel = parse_expression(p);
    NcASTNode *message = NULL;
    if (check(p, TOK_STRING)) {
        message = nc_ast_new(NODE_STRING_LIT, cur(p)->line);
        message->as.string_lit.value = tok_string(advance(p));
    } else if (check(p, TOK_TEMPLATE)) {
        message = nc_ast_new(NODE_TEMPLATE, cur(p)->line);
        message->as.template_lit.expr = tok_string(advance(p));
    } else if (match(p, TOK_COLON)) {
        message = nc_ast_new(NODE_STRING_LIT, cur(p)->line);
        message->as.string_lit.value = tok_string(expect(p, TOK_STRING, "message"));
    }
    NcASTNode *node = nc_ast_new(NODE_NOTIFY, start->line);
    node->as.notify.channel = channel;
    node->as.notify.message = message;
    return node;
}

static NcASTNode *parse_wait(NcParser *p) {
    NcToken *start = advance(p);
    NcToken *amount_tok = cur(p);
    double amount = 0;
    if (amount_tok->type == TOK_INTEGER) { amount = (double)amount_tok->literal.int_val; advance(p); }
    else if (amount_tok->type == TOK_FLOAT_LIT) { amount = amount_tok->literal.float_val; advance(p); }

    const char *unit = "seconds";
    if (match(p, TOK_MILLISECONDS)) { unit = "milliseconds"; amount /= 1000.0; }
    else if (match(p, TOK_SECONDS)) unit = "seconds";
    else if (match(p, TOK_MINUTES)) { unit = "minutes"; amount *= 60; }
    else if (match(p, TOK_HOURS)) { unit = "hours"; amount *= 3600; }
    else if (match(p, TOK_DAYS)) { unit = "days"; amount *= 86400; }

    NcASTNode *node = nc_ast_new(NODE_WAIT, start->line);
    node->as.wait_stmt.amount = amount;
    node->as.wait_stmt.unit = nc_string_from_cstr(unit);
    return node;
}

static NcASTNode *parse_store(NcParser *p) {
    NcToken *start = advance(p);
    NcASTNode *val = parse_expression(p);
    NcString *target = nc_string_from_cstr("store");
    if (match(p, TOK_INTO)) {
        if (check(p, TOK_STRING)) target = tok_string(advance(p));
        else if (check(p, TOK_IDENTIFIER)) target = tok_string(advance(p));
    }
    NcASTNode *node = nc_ast_new(NODE_STORE, start->line);
    node->as.store_stmt.value = val;
    node->as.store_stmt.target = target;
    return node;
}

static NcASTNode *parse_emit(NcParser *p) {
    NcToken *start = advance(p);
    NcASTNode *node = nc_ast_new(NODE_EMIT, start->line);
    node->as.single_expr.value = parse_expression(p);
    return node;
}

static NcASTNode *parse_apply(NcParser *p) {
    NcToken *start = advance(p);
    char target_buf[1024] = {0};
    while (!check(p, TOK_USING) && !check(p, TOK_COLON) &&
           !check(p, TOK_NEWLINE) && !at_end(p)) {
        NcToken *t = advance(p);
        size_t rem = sizeof(target_buf) - strlen(target_buf) - 1;
        if (target_buf[0] && rem > 0) { strncat(target_buf, " ", rem); rem--; }
        if (rem > 0) strncat(target_buf, t->start, rem < (size_t)t->length ? rem : (size_t)t->length);
    }
    NcString *using = nc_string_from_cstr("");
    if (match(p, TOK_USING)) {
        NcToken *u = expect(p, TOK_IDENTIFIER, "tool name");
        using = tok_string(u);
    }
    NcMap *opts = NULL;
    if (match(p, TOK_COLON)) {
        skip_newlines(p);
        if (check(p, TOK_INDENT)) opts = parse_kv_block(p);
    }
    NcASTNode *node = nc_ast_new(NODE_APPLY, start->line);
    node->as.apply.target = nc_string_from_cstr(target_buf);
    node->as.apply.using = using;
    node->as.apply.options = opts;
    return node;
}

static NcASTNode *parse_check(NcParser *p) {
    NcToken *start = advance(p);
    match(p, TOK_IF);
    char desc[1024] = {0};
    while (!check(p, TOK_USING) && !check(p, TOK_SAVE) && !check(p, TOK_CALLED) &&
           !check(p, TOK_COLON) && !check(p, TOK_NEWLINE) && !at_end(p)) {
        NcToken *t = advance(p);
        size_t rem = sizeof(desc) - strlen(desc) - 1;
        if (desc[0] && rem > 0) { strncat(desc, " ", rem); rem--; }
        if (rem > 0) strncat(desc, t->start, rem < (size_t)t->length ? rem : (size_t)t->length);
    }
    NcString *using = nc_string_from_cstr("");
    NcString *save_as = nc_string_from_cstr("check_result");
    if (match(p, TOK_USING)) using = tok_string(expect(p, TOK_IDENTIFIER, "source"));
    if (match(p, TOK_SAVE)) { match(p, TOK_AS); save_as = tok_string(expect(p, TOK_IDENTIFIER, "name")); }
    else if (match(p, TOK_CALLED)) save_as = tok_string(expect(p, TOK_IDENTIFIER, "name"));
    NcMap *opts = NULL;
    if (match(p, TOK_COLON)) { skip_newlines(p); if (check(p, TOK_INDENT)) opts = parse_kv_block(p); }
    NcASTNode *node = nc_ast_new(NODE_CHECK, start->line);
    node->as.check.desc = nc_string_from_cstr(desc);
    node->as.check.using = using;
    node->as.check.save_as = save_as;
    return node;
}

static NcASTNode *parse_try(NcParser *p) {
    NcToken *start = advance(p);
    match(p, TOK_COLON);
    skip_newlines(p);

    NcASTNode *node = nc_ast_new(NODE_TRY, start->line);
    node->as.try_stmt.body = parse_indented_body(p, &node->as.try_stmt.body_count);
    node->as.try_stmt.error_body = NULL;
    node->as.try_stmt.error_count = 0;
    node->as.try_stmt.error_type = NULL;

    skip_newlines(p);
    if (match(p, TOK_CATCH)) {
        /* catch "ErrorType": — specific error type handling */
        if (check(p, TOK_STRING)) {
            NcToken *err_type = advance(p);
            node->as.try_stmt.error_type = tok_string(err_type);
        }
        match(p, TOK_COLON);
        skip_newlines(p);
        node->as.try_stmt.error_body = parse_indented_body(p, &node->as.try_stmt.error_count);
    } else if (match(p, TOK_ON_ERROR)) {
        /* on_error: (single keyword) */
        match(p, TOK_COLON);
        skip_newlines(p);
        node->as.try_stmt.error_body = parse_indented_body(p, &node->as.try_stmt.error_count);
    } else if (match(p, TOK_ON)) {
        /* on error: (two words) — "error" may be any token type */
        if (!check(p, TOK_COLON) && !check(p, TOK_NEWLINE) && !at_end(p))
            advance(p);
        match(p, TOK_COLON);
        skip_newlines(p);
        node->as.try_stmt.error_body = parse_indented_body(p, &node->as.try_stmt.error_count);
    } else if (match(p, TOK_OTHERWISE)) {
        /* otherwise: as error handler (alternative syntax) */
        match(p, TOK_COLON);
        skip_newlines(p);
        node->as.try_stmt.error_body = parse_indented_body(p, &node->as.try_stmt.error_count);
    }

    node->as.try_stmt.finally_body = NULL;
    node->as.try_stmt.finally_count = 0;
    skip_newlines(p);
    if (match(p, TOK_FINALLY)) {
        match(p, TOK_COLON);
        skip_newlines(p);
        node->as.try_stmt.finally_body = parse_indented_body(p, &node->as.try_stmt.finally_count);
    }

    return node;
}

/* ── assert <condition>, "message" ──────────────────────────── */
static NcASTNode *parse_assert(NcParser *p) {
    NcToken *start = advance(p);  /* consume 'assert' */
    NcASTNode *cond = parse_expression(p);
    NcASTNode *msg = NULL;
    if (match(p, TOK_COMMA)) {
        msg = parse_expression(p);
    }
    NcASTNode *node = nc_ast_new(NODE_ASSERT, start->line);
    node->as.assert_stmt.condition = cond;
    node->as.assert_stmt.message = msg;
    return node;
}

/* ── test "name": ... body ... ──────────────────────────────── */
static NcASTNode *parse_test_block(NcParser *p) {
    NcToken *start = advance(p);  /* consume 'test' */
    NcString *name = NULL;
    if (check(p, TOK_STRING)) {
        NcToken *n = advance(p);
        name = tok_string(n);
    }
    match(p, TOK_COLON);
    skip_newlines(p);
    NcASTNode *node = nc_ast_new(NODE_TEST_BLOCK, start->line);
    node->as.test_block.name = name;
    node->as.test_block.body = parse_indented_body(p, &node->as.test_block.body_count);
    return node;
}

/* ── await <expression> ─────────────────────────────────────── */
static NcASTNode *parse_await(NcParser *p) {
    NcToken *start = advance(p);  /* consume 'await' */
    NcASTNode *value = parse_expression(p);
    NcASTNode *node = nc_ast_new(NODE_AWAIT, start->line);
    node->as.single_expr.value = value;
    return node;
}

/* ── yield <expression> ─────────────────────────────────────── */
static NcASTNode *parse_yield(NcParser *p) {
    NcToken *start = advance(p);  /* consume 'yield' */
    NcASTNode *value = parse_expression(p);
    NcASTNode *node = nc_ast_new(NODE_YIELD, start->line);
    node->as.yield_stmt.value = value;
    return node;
}

static NcASTNode *parse_statement(NcParser *p) {
    skip_newlines(p);
    if (p->had_error || at_end(p)) return NULL;
    NcToken *t = cur(p);

    switch (t->type) {
        case TOK_GATHER:    return parse_gather(p);
        case TOK_ASK:       return parse_ask_ai(p);
        case TOK_IF:        return parse_if(p);
        case TOK_WHILE:     return parse_while(p);
        case TOK_REPEAT:    return parse_repeat(p);
        case TOK_MATCH:     return parse_match(p);
        case TOK_SET:       return parse_set(p);
        case TOK_RESPOND:   return parse_respond(p);
        case TOK_RUN:       return parse_run(p);
        case TOK_LOG:       return parse_log(p);
        case TOK_SHOW:      return parse_show(p);
        case TOK_NOTIFY: case TOK_SEND: return parse_notify(p);
        case TOK_WAIT:      return parse_wait(p);
        case TOK_STORE:     return parse_store(p);
        case TOK_EMIT:      return parse_emit(p);
        case TOK_APPLY:     return parse_apply(p);
        case TOK_CHECK:     return parse_check(p);
        case TOK_TRY:       return parse_try(p);
        case TOK_ASSERT:    return parse_assert(p);
        case TOK_TEST:      return parse_test_block(p);
        case TOK_AWAIT:     return parse_await(p);
        case TOK_YIELD:     return parse_yield(p);
        case TOK_STOP:      advance(p); return nc_ast_new(NODE_STOP, t->line);
        case TOK_SKIP:      advance(p); return nc_ast_new(NODE_SKIP, t->line);
        default: {
            /* ── Natural English aliases ──────────────────── */
            
            /* "add VALUE to VARIABLE" → list append (if target is list) or numeric add */
            if (t->type == TOK_IDENTIFIER && t->length == 3 && memcmp(t->start, "add", 3) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_TO, "to");
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                NcASTNode *node = nc_ast_new(NODE_APPEND, start->line);
                node->as.append_stmt.value = val;
                node->as.append_stmt.target = tok_string(target);
                return node;
            }
            
            /* "increase VARIABLE by VALUE" → set var to var + value */
            if (t->type == TOK_IDENTIFIER && t->length == 8 && memcmp(t->start, "increase", 8) == 0) {
                NcToken *start = advance(p);
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                /* accept "by" as identifier */
                if (check(p, TOK_IDENTIFIER)) advance(p);
                NcASTNode *val = parse_expression(p);
                NcASTNode *node = nc_ast_new(NODE_SET, start->line);
                node->as.set_stmt.target = tok_string(target);
                node->as.set_stmt.field = NULL;
                NcASTNode *math = nc_ast_new(NODE_MATH, start->line);
                math->as.math.left = nc_ast_new(NODE_IDENT, start->line);
                math->as.math.left->as.ident.name = tok_string(target);
                math->as.math.right = val;
                math->as.math.op = '+';
                node->as.set_stmt.value = math;
                return node;
            }
            
            /* "decrease VARIABLE by VALUE" → set var to var - value */
            if (t->type == TOK_IDENTIFIER && t->length == 8 && memcmp(t->start, "decrease", 8) == 0) {
                NcToken *start = advance(p);
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                if (check(p, TOK_IDENTIFIER)) advance(p);
                NcASTNode *val = parse_expression(p);
                NcASTNode *node = nc_ast_new(NODE_SET, start->line);
                node->as.set_stmt.target = tok_string(target);
                node->as.set_stmt.field = NULL;
                NcASTNode *math = nc_ast_new(NODE_MATH, start->line);
                math->as.math.left = nc_ast_new(NODE_IDENT, start->line);
                math->as.math.left->as.ident.name = tok_string(target);
                math->as.math.right = val;
                math->as.math.op = '-';
                node->as.set_stmt.value = math;
                return node;
            }
            
            /* "remove VALUE from LIST" → call node: remove(list, value) */
            if (t->type == TOK_IDENTIFIER && t->length == 6 && memcmp(t->start, "remove", 6) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_FROM, "from");
                NcToken *target = expect(p, TOK_IDENTIFIER, "list name");
                NcASTNode *node = nc_ast_new(NODE_EXPR_STMT, start->line);
                NcASTNode *call = nc_ast_new(NODE_CALL, start->line);
                call->as.call.name = nc_string_from_cstr("remove");
                call->as.call.args = calloc(2, sizeof(NcASTNode *));
                NcASTNode *list_ref = nc_ast_new(NODE_IDENT, start->line);
                list_ref->as.ident.name = tok_string(target);
                call->as.call.args[0] = list_ref;
                call->as.call.args[1] = val;
                call->as.call.arg_count = 2;
                node->as.single_expr.value = call;
                return node;
            }

            /* "push VALUE to LIST" → append (synonym from JS/Ruby) */
            if (t->type == TOK_IDENTIFIER && t->length == 4 && memcmp(t->start, "push", 4) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_TO, "to");
                NcToken *target = expect(p, TOK_IDENTIFIER, "list name");
                NcASTNode *node = nc_ast_new(NODE_APPEND, start->line);
                node->as.append_stmt.value = val;
                node->as.append_stmt.target = tok_string(target);
                return node;
            }

            /* "insert VALUE into LIST" → append (synonym) */
            if (t->type == TOK_IDENTIFIER && t->length == 6 && memcmp(t->start, "insert", 6) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                if (match(p, TOK_INTO) || match(p, TOK_TO)) { /* accept "into" or "to" */ }
                NcToken *target = expect(p, TOK_IDENTIFIER, "list name");
                NcASTNode *node = nc_ast_new(NODE_APPEND, start->line);
                node->as.append_stmt.value = val;
                node->as.append_stmt.target = tok_string(target);
                return node;
            }

            /* "delete VALUE from LIST" → remove (synonym) */
            if (t->type == TOK_IDENTIFIER && t->length == 6 && memcmp(t->start, "delete", 6) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_FROM, "from");
                NcToken *target = expect(p, TOK_IDENTIFIER, "list name");
                NcASTNode *node = nc_ast_new(NODE_EXPR_STMT, start->line);
                NcASTNode *call = nc_ast_new(NODE_CALL, start->line);
                call->as.call.name = nc_string_from_cstr("remove");
                call->as.call.args = calloc(2, sizeof(NcASTNode *));
                NcASTNode *list_ref = nc_ast_new(NODE_IDENT, start->line);
                list_ref->as.ident.name = tok_string(target);
                call->as.call.args[0] = list_ref;
                call->as.call.args[1] = val;
                call->as.call.arg_count = 2;
                node->as.single_expr.value = call;
                return node;
            }

            /* "subtract VALUE from VARIABLE" → set var to var - value */
            if (t->type == TOK_IDENTIFIER && t->length == 8 && memcmp(t->start, "subtract", 8) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_FROM, "from");
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                NcASTNode *node = nc_ast_new(NODE_SET, start->line);
                node->as.set_stmt.target = tok_string(target);
                node->as.set_stmt.field = NULL;
                NcASTNode *math = nc_ast_new(NODE_MATH, start->line);
                math->as.math.left = nc_ast_new(NODE_IDENT, start->line);
                math->as.math.left->as.ident.name = tok_string(target);
                math->as.math.right = val;
                math->as.math.op = '-';
                node->as.set_stmt.value = math;
                return node;
            }

            /* "multiply VARIABLE by VALUE" → set var to var * value */
            if (t->type == TOK_IDENTIFIER && t->length == 8 && memcmp(t->start, "multiply", 8) == 0) {
                NcToken *start = advance(p);
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                if (check(p, TOK_IDENTIFIER)) advance(p); /* skip "by" */
                NcASTNode *val = parse_expression(p);
                NcASTNode *node = nc_ast_new(NODE_SET, start->line);
                node->as.set_stmt.target = tok_string(target);
                node->as.set_stmt.field = NULL;
                NcASTNode *math = nc_ast_new(NODE_MATH, start->line);
                math->as.math.left = nc_ast_new(NODE_IDENT, start->line);
                math->as.math.left->as.ident.name = tok_string(target);
                math->as.math.right = val;
                math->as.math.op = '*';
                node->as.set_stmt.value = math;
                return node;
            }

            /* "divide VARIABLE by VALUE" → set var to var / value */
            if (t->type == TOK_IDENTIFIER && t->length == 6 && memcmp(t->start, "divide", 6) == 0) {
                NcToken *start = advance(p);
                NcToken *target = expect(p, TOK_IDENTIFIER, "variable name");
                if (check(p, TOK_IDENTIFIER)) advance(p); /* skip "by" */
                NcASTNode *val = parse_expression(p);
                NcASTNode *node = nc_ast_new(NODE_SET, start->line);
                node->as.set_stmt.target = tok_string(target);
                node->as.set_stmt.field = NULL;
                NcASTNode *math = nc_ast_new(NODE_MATH, start->line);
                math->as.math.left = nc_ast_new(NODE_IDENT, start->line);
                math->as.math.left->as.ident.name = tok_string(target);
                math->as.math.right = val;
                math->as.math.op = '/';
                node->as.set_stmt.value = math;
                return node;
            }

            /* "append VALUE to LIST" */
            if (t->type == TOK_IDENTIFIER && t->length == 6 && memcmp(t->start, "append", 6) == 0) {
                NcToken *start = advance(p);
                NcASTNode *val = parse_expression(p);
                expect(p, TOK_TO, "to");
                NcToken *target = expect(p, TOK_IDENTIFIER, "list name");
                NcASTNode *node = nc_ast_new(NODE_APPEND, start->line);
                node->as.append_stmt.value = val;
                node->as.append_stmt.target = tok_string(target);
                return node;
            }
            
            NcASTNode *expr = parse_expression(p);
            NcASTNode *stmt = nc_ast_new(NODE_EXPR_STMT, t->line);
            stmt->as.single_expr.value = expr;
            return stmt;
        }
    }
}

/* ── Expressions (precedence climbing) ─────────────────────── */

static NcASTNode *parse_primary(NcParser *p) {
    NcToken *t = cur(p);

    if (t->type == TOK_RUN) {
        return parse_run(p);
    }

    if (t->type == TOK_INTEGER) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_INT_LIT, t->line);
        n->as.int_lit.value = t->literal.int_val;
        return n;
    }
    if (t->type == TOK_FLOAT_LIT) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_FLOAT_LIT, t->line);
        n->as.float_lit.value = t->literal.float_val;
        return n;
    }
    if (t->type == TOK_STRING) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_STRING_LIT, t->line);
        n->as.string_lit.value = tok_string(t);
        return n;
    }
    if (t->type == TOK_TRUE || t->type == TOK_FALSE) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_BOOL_LIT, t->line);
        n->as.bool_lit.value = (t->type == TOK_TRUE);
        return n;
    }
    if (t->type == TOK_NONE_LIT) {
        advance(p);
        return nc_ast_new(NODE_NONE_LIT, t->line);
    }
    if (t->type == TOK_TEMPLATE) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_TEMPLATE, t->line);
        n->as.template_lit.expr = tok_string(t);
        return n;
    }
    if (t->type == TOK_IDENTIFIER || t->type == TOK_EVENT) {
        advance(p);
        NcASTNode *n = nc_ast_new(NODE_IDENT, t->line);
        n->as.ident.name = tok_string(t);
        return n;
    }
    if (t->type == TOK_LBRACKET) {
        advance(p);
        skip_newlines(p);
        while (check(p, TOK_INDENT)) { advance(p); skip_newlines(p); }
        int cap = 8, count = 0;
        NcASTNode **elems = malloc(sizeof(NcASTNode *) * cap);
        while (!check(p, TOK_RBRACKET) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            if (check(p, TOK_INDENT)) { advance(p); continue; }
            if (check(p, TOK_DEDENT)) { advance(p); continue; }
            if (check(p, TOK_COMMA)) { advance(p); continue; }
            if (count >= cap) { cap *= 2; NcASTNode **tmp = realloc(elems, sizeof(NcASTNode *) * cap); if (!tmp) { p->had_error = true; break; } elems = tmp; }
            elems[count++] = parse_expression(p);
            match(p, TOK_COMMA);
            skip_newlines(p);
            while (check(p, TOK_INDENT) || check(p, TOK_DEDENT)) { advance(p); skip_newlines(p); }
        }
        expect(p, TOK_RBRACKET, "]");
        NcASTNode *n = nc_ast_new(NODE_LIST_LIT, t->line);
        n->as.list_lit.elements = elems;
        n->as.list_lit.count = count;
        return n;
    }
    if (t->type == TOK_LBRACE) {
        advance(p);
        skip_newlines(p);
        while (check(p, TOK_INDENT)) { advance(p); skip_newlines(p); }
        int cap = 8, count = 0;
        NcString **keys = malloc(sizeof(NcString *) * cap);
        NcASTNode **vals = malloc(sizeof(NcASTNode *) * cap);
        while (!check(p, TOK_RBRACE) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            if (check(p, TOK_INDENT)) { advance(p); continue; }
            if (check(p, TOK_DEDENT)) { advance(p); continue; }
            if (check(p, TOK_COMMA)) { advance(p); continue; }
            NcToken *key_tok = cur(p);
            NcString *key = tok_string(key_tok);
            advance(p);
            expect(p, TOK_COLON, ":");
            NcASTNode *val_expr = parse_expression(p);
            if (!val_expr) { nc_string_free(key); break; }
            if (count >= cap) {
                cap *= 2;
                NcString **ktmp = realloc(keys, sizeof(NcString *) * cap);
                NcASTNode **vtmp = realloc(vals, sizeof(NcASTNode *) * cap);
                if (!ktmp || !vtmp) { if (ktmp) keys = ktmp; if (vtmp) vals = vtmp; p->had_error = true; nc_string_free(key); break; }
                keys = ktmp; vals = vtmp;
            }
            keys[count] = key;
            vals[count] = val_expr;
            count++;
            match(p, TOK_COMMA);
            skip_newlines(p);
            while (check(p, TOK_INDENT) || check(p, TOK_DEDENT)) { advance(p); skip_newlines(p); }
        }
        expect(p, TOK_RBRACE, "}");
        NcASTNode *n = nc_ast_new(NODE_MAP_LIT, t->line);
        n->as.map_lit.keys = keys;
        n->as.map_lit.values = vals;
        n->as.map_lit.count = count;
        return n;
    }
    if (t->type == TOK_LPAREN) {
        advance(p);
        NcASTNode *expr = parse_expression(p);
        expect(p, TOK_RPAREN, ")");
        return expr;
    }

    /* Fallback: treat keyword as identifier */
    advance(p);
    NcASTNode *n = nc_ast_new(NODE_IDENT, t->line);
    n->as.ident.name = tok_string(t);
    return n;
}

static NcASTNode *parse_postfix(NcParser *p) {
    NcASTNode *expr = parse_primary(p);
    while (true) {
        if (check(p, TOK_DOT)) {
            advance(p);
            NcToken *member = advance(p);
            NcASTNode *dot = nc_ast_new(NODE_DOT, member->line);
            dot->as.dot.object = expr;
            dot->as.dot.member = tok_string(member);
            expr = dot;
        } else if (check(p, TOK_LBRACKET)) {
            advance(p);

            /* Check for slice: x[start:end], x[:end], x[start:], x[:] */
            if (check(p, TOK_COLON)) {
                /* x[:end] or x[:] */
                advance(p);
                NcASTNode *end_expr = NULL;
                if (!check(p, TOK_RBRACKET))
                    end_expr = parse_expression(p);
                expect(p, TOK_RBRACKET, "]");
                NcASTNode *slice_node = nc_ast_new(NODE_SLICE, expr->line);
                slice_node->as.slice.object = expr;
                slice_node->as.slice.start = NULL;
                slice_node->as.slice.end = end_expr;
                expr = slice_node;
            } else {
                NcASTNode *idx = parse_expression(p);
                if (check(p, TOK_COLON)) {
                    /* x[start:end] or x[start:] */
                    advance(p);
                    NcASTNode *end_expr = NULL;
                    if (!check(p, TOK_RBRACKET))
                        end_expr = parse_expression(p);
                    expect(p, TOK_RBRACKET, "]");
                    NcASTNode *slice_node = nc_ast_new(NODE_SLICE, expr->line);
                    slice_node->as.slice.object = expr;
                    slice_node->as.slice.start = idx;
                    slice_node->as.slice.end = end_expr;
                    expr = slice_node;
                } else {
                    /* Normal index: x[idx] */
                    expect(p, TOK_RBRACKET, "]");
                    NcASTNode *index_node = nc_ast_new(NODE_INDEX, expr->line);
                    index_node->as.math.left = expr;
                    index_node->as.math.right = idx;
                    expr = index_node;
                }
            }
        } else if (check(p, TOK_LPAREN)) {
            advance(p);
            int cap = 4, count = 0;
            NcASTNode **args = malloc(sizeof(NcASTNode *) * cap);
            while (!check(p, TOK_RPAREN) && !at_end(p)) {
                if (count >= cap) { cap *= 2; args = realloc(args, sizeof(NcASTNode *) * cap); }
                args[count++] = parse_expression(p);
                match(p, TOK_COMMA);
            }
            expect(p, TOK_RPAREN, ")");
            NcString *name = NULL;
            if (expr->type == NODE_IDENT) name = nc_string_ref(expr->as.ident.name);
            else name = nc_string_from_cstr("anonymous");
            NcASTNode *call = nc_ast_new(NODE_CALL, expr->line);
            call->as.call.name = name;
            call->as.call.args = args;
            call->as.call.arg_count = count;
            expr = call;
        } else break;
    }
    return expr;
}

static NcASTNode *parse_unary(NcParser *p) {
    if (match(p, TOK_NOT)) {
        NcASTNode *n = nc_ast_new(NODE_NOT, cur(p)->line);
        n->as.logic.left = parse_unary(p);
        return n;
    }
    if (check(p, TOK_MINUS)) {
        advance(p);
        NcASTNode *operand = parse_unary(p);
        NcASTNode *zero = nc_ast_new(NODE_INT_LIT, operand->line);
        zero->as.int_lit.value = 0;
        NcASTNode *n = nc_ast_new(NODE_MATH, operand->line);
        n->as.math.left = zero;
        n->as.math.right = operand;
        n->as.math.op = '-';
        return n;
    }
    return parse_postfix(p);
}

static NcASTNode *parse_multiplication(NcParser *p) {
    NcASTNode *left = parse_unary(p);
    while (check(p, TOK_STAR) || check(p, TOK_SLASH) || check(p, TOK_PERCENT)) {
        NcToken *op_tok = advance(p);
        char op = op_tok->type == TOK_STAR ? '*' : op_tok->type == TOK_PERCENT ? '%' : '/';
        NcASTNode *right = parse_unary(p);
        NcASTNode *n = nc_ast_new(NODE_MATH, left->line);
        n->as.math.left = left; n->as.math.right = right; n->as.math.op = op;
        left = n;
    }
    return left;
}

static NcASTNode *parse_addition(NcParser *p) {
    NcASTNode *left = parse_multiplication(p);
    while (check(p, TOK_PLUS) || check(p, TOK_MINUS)) {
        char op = (advance(p)->type == TOK_PLUS) ? '+' : '-';
        NcASTNode *right = parse_multiplication(p);
        NcASTNode *n = nc_ast_new(NODE_MATH, left->line);
        n->as.math.left = left; n->as.math.right = right; n->as.math.op = op;
        left = n;
    }
    return left;
}

static NcASTNode *parse_comparison(NcParser *p) {
    NcASTNode *left = parse_addition(p);

    if (match(p, TOK_IS)) {
        bool negated = match(p, TOK_NOT);
        const char *op = "equal";
        /* "is empty" → len(x) == 0 */
        if (check(p, TOK_IDENTIFIER) && cur(p)->length == 5 && memcmp(cur(p)->start, "empty", 5) == 0) {
            advance(p);
            NcASTNode *len_call = nc_ast_new(NODE_CALL, left->line);
            len_call->as.call.name = nc_string_from_cstr("len");
            len_call->as.call.args = malloc(sizeof(NcASTNode *));
            len_call->as.call.args[0] = left;
            len_call->as.call.arg_count = 1;
            NcASTNode *zero = nc_ast_new(NODE_INT_LIT, left->line);
            zero->as.int_lit.value = 0;
            NcASTNode *n = nc_ast_new(NODE_COMPARISON, left->line);
            n->as.comparison.left = len_call;
            n->as.comparison.right = zero;
            n->as.comparison.op = nc_string_from_cstr(negated ? "not_equal" : "equal");
            return n;
        }
        
        /* "is positive" → x > 0 */
        if (!negated && check(p, TOK_IDENTIFIER) && cur(p)->length == 8 && memcmp(cur(p)->start, "positive", 8) == 0) {
            advance(p);
            NcASTNode *zero = nc_ast_new(NODE_INT_LIT, left->line);
            zero->as.int_lit.value = 0;
            NcASTNode *n = nc_ast_new(NODE_COMPARISON, left->line);
            n->as.comparison.left = left;
            n->as.comparison.right = zero;
            n->as.comparison.op = nc_string_from_cstr("above");
            return n;
        }
        
        /* "is negative" → x < 0 */
        if (!negated && check(p, TOK_IDENTIFIER) && cur(p)->length == 8 && memcmp(cur(p)->start, "negative", 8) == 0) {
            advance(p);
            NcASTNode *zero = nc_ast_new(NODE_INT_LIT, left->line);
            zero->as.int_lit.value = 0;
            NcASTNode *n = nc_ast_new(NODE_COMPARISON, left->line);
            n->as.comparison.left = left;
            n->as.comparison.right = zero;
            n->as.comparison.op = nc_string_from_cstr("below");
            return n;
        }
        if (match(p, TOK_IN)) {
            op = negated ? "not_in" : "in";
        } else if (match(p, TOK_ABOVE) || match(p, TOK_GREATER)) { match(p, TOK_THAN); op = negated ? "below" : "above"; }
        else if (match(p, TOK_BELOW) || match(p, TOK_LESS)) { match(p, TOK_THAN); op = negated ? "above" : "below"; }
        else if (match(p, TOK_EQUAL)) { match(p, TOK_TO); op = negated ? "not_equal" : "equal"; }
        else if (match(p, TOK_AT)) {
            if (match(p, TOK_LEAST)) op = negated ? "below" : "at_least";
            else if (match(p, TOK_MOST)) op = negated ? "above" : "at_most";
        } else {
            op = negated ? "not_equal" : "equal";
        }
        NcASTNode *right = parse_addition(p);
        NcASTNode *n = nc_ast_new(NODE_COMPARISON, left->line);
        n->as.comparison.left = left;
        n->as.comparison.right = right;
        n->as.comparison.op = nc_string_from_cstr(op);
        return n;
    }

    /* Symbol-based comparison operators: > < >= <= == != */
    if (check(p, TOK_GT_SIGN) || check(p, TOK_LT_SIGN) || check(p, TOK_GTE_SIGN) ||
        check(p, TOK_LTE_SIGN) || check(p, TOK_EQEQ_SIGN) || check(p, TOK_BANGEQ_SIGN)) {
        const char *op;
        NcToken *op_tok = advance(p);
        switch (op_tok->type) {
            case TOK_GT_SIGN:     op = "above"; break;
            case TOK_LT_SIGN:     op = "below"; break;
            case TOK_GTE_SIGN:    op = "at_least"; break;
            case TOK_LTE_SIGN:    op = "at_most"; break;
            case TOK_EQEQ_SIGN:   op = "equal"; break;
            case TOK_BANGEQ_SIGN:  op = "not_equal"; break;
            default: op = "equal"; break;
        }
        NcASTNode *right = parse_addition(p);
        NcASTNode *n = nc_ast_new(NODE_COMPARISON, left->line);
        n->as.comparison.left = left;
        n->as.comparison.right = right;
        n->as.comparison.op = nc_string_from_cstr(op);
        return n;
    }

    return left;
}

static NcASTNode *parse_and(NcParser *p) {
    NcASTNode *left = parse_comparison(p);
    while (match(p, TOK_AND)) {
        NcASTNode *right = parse_comparison(p);
        NcASTNode *n = nc_ast_new(NODE_LOGIC, left->line);
        n->as.logic.left = left; n->as.logic.right = right;
        n->as.logic.op = nc_string_from_cstr("and");
        left = n;
    }
    return left;
}

static NcASTNode *parse_or(NcParser *p) {
    NcASTNode *left = parse_and(p);
    while (match(p, TOK_OR)) {
        NcASTNode *right = parse_and(p);
        NcASTNode *n = nc_ast_new(NODE_LOGIC, left->line);
        n->as.logic.left = left; n->as.logic.right = right;
        n->as.logic.op = nc_string_from_cstr("or");
        left = n;
    }
    return left;
}

static NcASTNode *parse_expression(NcParser *p) {
    if (++p->depth > 200) {
        if (!p->had_error) {
            snprintf(p->error_msg, sizeof(p->error_msg),
                "Line %d: Expression nesting too deep (max 200 levels).", cur(p)->line);
            p->had_error = true;
        }
        p->depth--;
        return nc_ast_new(NODE_NONE_LIT, cur(p)->line);
    }
    NcASTNode *result = parse_or(p);
    p->depth--;
    return result;
}

/* ── API block ─────────────────────────────────────────────── */

static void parse_api(NcParser *p, NcASTNode *program) {
    advance(p); /* api */
    match(p, TOK_COLON);
    skip_newlines(p);

    int cap = 16, count = program->as.program.route_count;
    if (!program->as.program.routes)
        program->as.program.routes = malloc(sizeof(NcASTNode *) * cap);

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            NcToken *method_tok = cur(p);
            const char *method = "GET";
            switch (method_tok->type) {
                case TOK_HTTP_GET: method = "GET"; break;
                case TOK_HTTP_POST: method = "POST"; break;
                case TOK_HTTP_PUT: method = "PUT"; break;
                case TOK_HTTP_DELETE: method = "DELETE"; break;
                case TOK_HTTP_PATCH: method = "PATCH"; break;
                default:
                    NC_WARN("Line %d: '%.*s' is not a valid HTTP method "
                            "(expected GET, POST, PUT, DELETE, or PATCH); defaulting to GET",
                            method_tok->line, method_tok->length, method_tok->start);
                    break;
            }
            advance(p);

            /* Read path */
            char path[1024] = {0};
            while (!check(p, TOK_RUNS) && !check(p, TOK_DOES) &&
                   !check(p, TOK_NEWLINE) && !at_end(p)) {
                NcToken *pt = advance(p);
                size_t rem = sizeof(path) - strlen(path) - 1;
                if (rem > 0) {
                    size_t copy = rem < (size_t)pt->length ? rem : (size_t)pt->length;
                    strncat(path, pt->start, copy);
                }
            }
            match(p, TOK_RUNS); match(p, TOK_DOES);
            NcToken *handler = cur(p);
            advance(p);

            NcASTNode *route = nc_ast_new(NODE_ROUTE, method_tok->line);
            route->as.route.method = nc_string_from_cstr(method);
            route->as.route.path = nc_string_from_cstr(path);
            route->as.route.handler = tok_string(handler);

            if (count >= cap) {
                cap *= 2;
                program->as.program.routes = realloc(
                    program->as.program.routes, sizeof(NcASTNode *) * cap);
            }
            program->as.program.routes[count++] = route;
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    program->as.program.route_count = count;
}

/* ── Event / Schedule ──────────────────────────────────────── */

static NcASTNode *parse_event(NcParser *p) {
    NcToken *start = advance(p); /* on */
    match(p, TOK_EVENT);
    NcToken *name = expect(p, TOK_STRING, "event name");
    match(p, TOK_COLON);
    skip_newlines(p);
    NcASTNode *node = nc_ast_new(NODE_EVENT_HANDLER, start->line);
    node->as.event_handler.event_name = tok_string(name);
    node->as.event_handler.body = parse_indented_body(p, &node->as.event_handler.body_count);
    return node;
}

static NcASTNode *parse_schedule(NcParser *p) {
    NcToken *start = advance(p); /* every */
    char interval[64] = {0};
    if (check(p, TOK_INTEGER) || check(p, TOK_FLOAT_LIT)) {
        NcToken *t = advance(p);
        size_t rem = sizeof(interval) - strlen(interval) - 1;
        if (rem > 0) strncat(interval, t->start, rem < (size_t)t->length ? rem : (size_t)t->length);
        strncat(interval, " ", sizeof(interval) - strlen(interval) - 1);
    }
    if (check(p, TOK_MILLISECONDS) || check(p, TOK_SECONDS) || check(p, TOK_MINUTES) || check(p, TOK_HOURS)) {
        NcToken *t = advance(p);
        size_t rem = sizeof(interval) - strlen(interval) - 1;
        if (rem > 0) strncat(interval, t->start, rem < (size_t)t->length ? rem : (size_t)t->length);
    }
    match(p, TOK_COLON);
    skip_newlines(p);
    NcASTNode *node = nc_ast_new(NODE_SCHEDULE_HANDLER, start->line);
    node->as.schedule_handler.interval = nc_string_from_cstr(interval);
    node->as.schedule_handler.body = parse_indented_body(p, &node->as.schedule_handler.body_count);
    return node;
}

/* ── Import ────────────────────────────────────────────────── */

static NcASTNode *parse_import(NcParser *p) {
    NcToken *start = advance(p); /* import */
    NcToken *module_tok = expect(p, TOK_STRING, "module name");
    NcASTNode *node = nc_ast_new(NODE_IMPORT, start->line);
    node->as.import_decl.module = tok_string(module_tok);
    node->as.import_decl.alias = NULL;
    if (match(p, TOK_AS)) {
        NcToken *alias_tok = expect(p, TOK_IDENTIFIER, "alias name");
        node->as.import_decl.alias = tok_string(alias_tok);
    }
    return node;
}

/* ── Middleware block ──────────────────────────────────────── */

static void parse_middleware_block(NcParser *p, NcASTNode *program) {
    advance(p); /* middleware */
    match(p, TOK_COLON);
    skip_newlines(p);

    int cap = 8, count = program->as.program.mw_count;
    if (!program->as.program.middleware)
        program->as.program.middleware = malloc(sizeof(NcASTNode *) * cap);

    if (match(p, TOK_INDENT)) {
        skip_newlines(p);
        while (!check(p, TOK_DEDENT) && !at_end(p) && !p->had_error) {
            if (check(p, TOK_NEWLINE)) { advance(p); continue; }
            NcToken *name_tok = advance(p);
            NcASTNode *mw = nc_ast_new(NODE_MIDDLEWARE, name_tok->line);
            mw->as.middleware.name = tok_string(name_tok);
            mw->as.middleware.options = NULL;
            if (match(p, TOK_COLON)) {
                skip_newlines(p);
                if (check(p, TOK_INDENT))
                    mw->as.middleware.options = parse_kv_block(p);
            }
            if (count >= cap) {
                cap *= 2;
                program->as.program.middleware = realloc(
                    program->as.program.middleware, sizeof(NcASTNode *) * cap);
            }
            program->as.program.middleware[count++] = mw;
            skip_newlines(p);
        }
        match(p, TOK_DEDENT);
    }
    program->as.program.mw_count = count;
}

/* ── Top-level parse ───────────────────────────────────────── */

NcParser *nc_parser_new(NcToken *tokens, int count, const char *filename) {
    NcParser *p = calloc(1, sizeof(NcParser));
    p->tokens = tokens;
    p->count = count;
    p->filename = filename;
    p->pos = 0;
    p->had_error = false;
    return p;
}

NcASTNode *nc_parser_parse(NcParser *p) {
    NcASTNode *prog = nc_ast_new(NODE_PROGRAM, 1);
    memset(&prog->as.program, 0, sizeof(prog->as.program));

    int imp_cap = 8, def_cap = 8, beh_cap = 16, evt_cap = 4;
    prog->as.program.imports = malloc(sizeof(NcASTNode *) * imp_cap);
    prog->as.program.definitions = malloc(sizeof(NcASTNode *) * def_cap);
    prog->as.program.behaviors = malloc(sizeof(NcASTNode *) * beh_cap);
    prog->as.program.routes = malloc(sizeof(NcASTNode *) * 16);
    prog->as.program.events = malloc(sizeof(NcASTNode *) * evt_cap);
    prog->as.program.middleware = NULL;

    skip_newlines(p);
    while (!at_end(p) && !p->had_error) {
        skip_newlines(p);
        if (at_end(p)) break;
        NcToken *t = cur(p);

        switch (t->type) {
        case TOK_IMPORT: {
            NcASTNode *imp = parse_import(p);
            int i = prog->as.program.import_count;
            if (i >= imp_cap) { imp_cap *= 2; prog->as.program.imports = realloc(prog->as.program.imports, sizeof(NcASTNode *) * imp_cap); }
            prog->as.program.imports[i] = imp;
            prog->as.program.import_count++;
            break;
        }
        case TOK_SERVICE:
            advance(p);
            prog->as.program.service_name = tok_string(expect(p, TOK_STRING, "service name"));
            break;
        case TOK_VERSION:
            advance(p);
            prog->as.program.version = tok_string(expect(p, TOK_STRING, "version"));
            break;
        case TOK_MODEL:
            advance(p);
            prog->as.program.model = tok_string(expect(p, TOK_STRING, "model name"));
            break;
        case TOK_IDENTIFIER:
            if (t->length == 5 && strncmp(t->start, "model", 5) == 0) {
                advance(p);
                prog->as.program.model = tok_string(expect(p, TOK_STRING, "model name"));
                break;
            }
            goto top_level_error;
        case TOK_AUTHOR:
            advance(p);
            expect(p, TOK_STRING, "author name");
            break;
        case TOK_DESCRIPTION:
            advance(p);
            prog->as.program.description = tok_string(expect(p, TOK_STRING, "description"));
            break;
        case TOK_CONFIGURE:
            advance(p); match(p, TOK_COLON); skip_newlines(p);
            prog->as.program.configure = parse_kv_block(p);
            break;
        case TOK_MIDDLEWARE:
            parse_middleware_block(p, prog);
            break;
        case TOK_DEFINE: {
            NcASTNode *def = parse_definition(p);
            int i = prog->as.program.def_count;
            if (i >= def_cap) { def_cap *= 2; NcASTNode **tmp = realloc(prog->as.program.definitions, sizeof(NcASTNode *) * def_cap); if (!tmp) { p->had_error = true; break; } prog->as.program.definitions = tmp; }
            prog->as.program.definitions[i] = def;
            prog->as.program.def_count++;
            break;
        }
        case TOK_INTERFACE: {
            NcASTNode *iface = parse_interface(p);
            /* Store interfaces as definitions for simplicity */
            int i = prog->as.program.def_count;
            if (i >= def_cap) { def_cap *= 2; NcASTNode **tmp = realloc(prog->as.program.definitions, sizeof(NcASTNode *) * def_cap); if (!tmp) { p->had_error = true; break; } prog->as.program.definitions = tmp; }
            prog->as.program.definitions[i] = iface;
            prog->as.program.def_count++;
            break;
        }
        case TOK_AT_SIGN: {
            /* Collect decorators, then attach to next behavior */
            int dec_cap = 4, dec_count = 0;
            NcASTNode **decorators = malloc(sizeof(NcASTNode *) * dec_cap);
            while (check(p, TOK_AT_SIGN)) {
                if (dec_count >= dec_cap) { dec_cap *= 2; decorators = realloc(decorators, sizeof(NcASTNode *) * dec_cap); }
                decorators[dec_count++] = parse_decorator(p);
            }
            if (check(p, TOK_TO)) {
                NcASTNode *beh = parse_behavior(p);
                beh->as.behavior.decorators = decorators;
                beh->as.behavior.decorator_count = dec_count;
                int i = prog->as.program.beh_count;
                if (i >= beh_cap) { beh_cap *= 2; NcASTNode **tmp = realloc(prog->as.program.behaviors, sizeof(NcASTNode *) * beh_cap); if (!tmp) { p->had_error = true; break; } prog->as.program.behaviors = tmp; }
                prog->as.program.behaviors[i] = beh;
                prog->as.program.beh_count++;
            } else {
                free(decorators);
            }
            break;
        }
        case TOK_TO: {
            NcASTNode *beh = parse_behavior(p);
            int i = prog->as.program.beh_count;
            if (i >= beh_cap) { beh_cap *= 2; NcASTNode **tmp = realloc(prog->as.program.behaviors, sizeof(NcASTNode *) * beh_cap); if (!tmp) { p->had_error = true; break; } prog->as.program.behaviors = tmp; }
            prog->as.program.behaviors[i] = beh;
            prog->as.program.beh_count++;
            break;
        }
        case TOK_AGENT: {
            NcASTNode *ag = parse_agent_def(p);
            int i = prog->as.program.agent_count;
            int ag_cap = (i < 4) ? 4 : i * 2;
            if (!prog->as.program.agents)
                prog->as.program.agents = malloc(sizeof(NcASTNode *) * ag_cap);
            else if (i >= ag_cap) {
                ag_cap *= 2;
                prog->as.program.agents = realloc(prog->as.program.agents, sizeof(NcASTNode *) * ag_cap);
            }
            prog->as.program.agents[i] = ag;
            prog->as.program.agent_count++;
            break;
        }
        case TOK_API:
            parse_api(p, prog);
            break;
        case TOK_ON: {
            NcASTNode *evt = parse_event(p);
            int i = prog->as.program.event_count;
            if (i >= evt_cap) { evt_cap *= 2; prog->as.program.events = realloc(prog->as.program.events, sizeof(NcASTNode *) * evt_cap); }
            prog->as.program.events[i] = evt;
            prog->as.program.event_count++;
            break;
        }
        case TOK_EVERY: {
            NcASTNode *sch = parse_schedule(p);
            int i = prog->as.program.event_count;
            if (i >= evt_cap) { evt_cap *= 2; prog->as.program.events = realloc(prog->as.program.events, sizeof(NcASTNode *) * evt_cap); }
            prog->as.program.events[i] = sch;
            prog->as.program.event_count++;
            break;
        }
        default:
        top_level_error:
            snprintf(p->error_msg, sizeof(p->error_msg),
                     "Line %d: I don't understand '%.*s' here.\n"
                     "  NC programs start with 'service', 'define', 'to', 'api:', 'configure:', or 'import'.\n"
                     "  Check if this line is inside a behavior (it should be indented).",
                     t->line, t->length, t->start);
            p->had_error = true;
            break;
        }
        skip_newlines(p);
    }
    return prog;
}

void nc_parser_free(NcParser *p) { free(p); }
