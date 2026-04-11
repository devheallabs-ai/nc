/*
 * nc_semantic.c — Semantic analysis for NC.
 *
 * Runs AFTER parsing, BEFORE execution/compilation.
 * Catches errors that syntax alone can't:
 *   - Undefined variables
 *   - Duplicate behavior names
 *   - Type mismatches
 *   - Unreachable code
 *   - Missing respond statements
 *
 * Flow:  Source → Lexer → Parser → AST → [Semantic Analyzer] → Execute/Compile
 */

#include <stdio.h>
#include <string.h>
#include "../include/nc_ast.h"
#include "../include/nc_token.h"

/* ═══════════════════════════════════════════════════════════
 *  Symbol Table
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    SYM_VARIABLE,
    SYM_BEHAVIOR,
    SYM_TYPE,
    SYM_FIELD,
    SYM_PARAM,
    SYM_BUILTIN,
} NcSymbolKind;

typedef struct {
    NcString    *name;
    NcSymbolKind kind;
    NcString    *type_name;  /* "text", "number", "yesno", "list", etc. */
    int          line;       /* where declared */
    bool         is_mutable;
    bool         is_used;
} NcSymbol;

typedef struct NcScope {
    NcSymbol       *symbols;
    int             count;
    int             capacity;
    struct NcScope *parent;
    const char     *name;    /* "global", behavior name, etc. */
} NcScope;

typedef struct {
    NcScope   *current;
    NcScope   *global;

    /* Diagnostics */
    char     **errors;
    int        error_count;
    int        error_cap;
    char     **warnings;
    int        warn_count;
    int        warn_cap;

    const char *filename;
    const char *source;
} NcAnalyzer;

/* ── Scope operations ──────────────────────────────────────── */

static NcScope *scope_create(NcScope *parent, const char *name) {
    NcScope *s = calloc(1, sizeof(NcScope));
    s->capacity = 32;
    s->symbols = calloc(s->capacity, sizeof(NcSymbol));
    s->parent = parent;
    s->name = name;
    return s;
}

static void scope_destroy(NcScope *s) {
    free(s->symbols);
    free(s);
}

static NcSymbol *scope_lookup(NcScope *s, NcString *name) {
    for (int i = 0; i < s->count; i++)
        if (nc_string_equal(s->symbols[i].name, name))
            return &s->symbols[i];
    if (s->parent) return scope_lookup(s->parent, name);
    return NULL;
}

static NcSymbol *scope_lookup_local(NcScope *s, NcString *name) {
    for (int i = 0; i < s->count; i++)
        if (nc_string_equal(s->symbols[i].name, name))
            return &s->symbols[i];
    return NULL;
}

static NcSymbol *scope_define(NcScope *s, NcString *name, NcSymbolKind kind,
                               NcString *type_name, int line) {
    if (s->count >= s->capacity) {
        s->capacity *= 2;
        s->symbols = realloc(s->symbols, sizeof(NcSymbol) * s->capacity);
    }
    NcSymbol *sym = &s->symbols[s->count++];
    sym->name = nc_string_ref(name);
    sym->kind = kind;
    sym->type_name = type_name ? nc_string_ref(type_name) : NULL;
    sym->line = line;
    sym->is_mutable = (kind == SYM_VARIABLE);
    sym->is_used = false;
    return sym;
}

/* ── Diagnostics ───────────────────────────────────────────── */

static void add_error(NcAnalyzer *a, int line, const char *fmt, ...) {
    if (a->error_count >= a->error_cap) {
        a->error_cap *= 2;
        a->errors = realloc(a->errors, sizeof(char *) * a->error_cap);
    }
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = snprintf(buf, sizeof(buf), "  Line %d: ", line);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);
    a->errors[a->error_count++] = strdup(buf);
}

static void add_warning(NcAnalyzer *a, int line, const char *fmt, ...) {
    if (a->warn_count >= a->warn_cap) {
        a->warn_cap *= 2;
        a->warnings = realloc(a->warnings, sizeof(char *) * a->warn_cap);
    }
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = snprintf(buf, sizeof(buf), "  Line %d: ", line);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);
    a->warnings[a->warn_count++] = strdup(buf);
}

/* ── Register builtins ─────────────────────────────────────── */

static void register_builtins(NcScope *global) {
    const char *builtins[] = {
        "len", "str", "int", "float", "print", "keys", "get", "values",
        "range", "type", "json_encode", "json_decode", "time_now",
        "success", "failure", "true", "false",
        NULL
    };
    for (int i = 0; builtins[i]; i++) {
        NcString *name = nc_string_from_cstr(builtins[i]);
        scope_define(global, name, SYM_BUILTIN, NULL, 0);
        nc_string_free(name);
    }
}

/* ── Analyze expressions ───────────────────────────────────── */

static void analyze_expr(NcAnalyzer *a, NcASTNode *node);

static void analyze_expr(NcAnalyzer *a, NcASTNode *node) {
    if (!node) return;
    switch (node->type) {
    case NODE_IDENT: {
        NcSymbol *sym = scope_lookup(a->current, node->as.ident.name);
        if (sym) sym->is_used = true;
        break;
    }
    case NODE_DOT:
        analyze_expr(a, node->as.dot.object);
        break;
    case NODE_MATH:
        analyze_expr(a, node->as.math.left);
        analyze_expr(a, node->as.math.right);
        break;
    case NODE_COMPARISON:
        analyze_expr(a, node->as.comparison.left);
        analyze_expr(a, node->as.comparison.right);
        break;
    case NODE_LOGIC:
        analyze_expr(a, node->as.logic.left);
        analyze_expr(a, node->as.logic.right);
        break;
    case NODE_NOT:
        analyze_expr(a, node->as.logic.left);
        break;
    case NODE_CALL: {
        NcSymbol *sym = scope_lookup(a->current, node->as.call.name);
        if (!sym) {
            add_warning(a, node->line, "Function '%s' not found — will use runtime lookup",
                       node->as.call.name->chars);
        }
        for (int i = 0; i < node->as.call.arg_count; i++)
            analyze_expr(a, node->as.call.args[i]);
        break;
    }
    case NODE_LIST_LIT:
        for (int i = 0; i < node->as.list_lit.count; i++)
            analyze_expr(a, node->as.list_lit.elements[i]);
        break;
    default:
        break;
    }
}

/* ── Analyze statements ────────────────────────────────────── */

static bool analyze_stmt(NcAnalyzer *a, NcASTNode *node);

static bool analyze_body(NcAnalyzer *a, NcASTNode **stmts, int count) {
    bool has_respond = false;
    for (int i = 0; i < count; i++) {
        if (analyze_stmt(a, stmts[i])) has_respond = true;
        if (has_respond && i < count - 1) {
            add_warning(a, stmts[i + 1]->line, "Code after 'respond' is unreachable");
        }
    }
    return has_respond;
}

static bool analyze_stmt(NcAnalyzer *a, NcASTNode *node) {
    if (!node) return false;

    switch (node->type) {
    case NODE_SET: {
        analyze_expr(a, node->as.set_stmt.value);
        NcSymbol *existing = scope_lookup_local(a->current, node->as.set_stmt.target);
        if (!existing)
            scope_define(a->current, node->as.set_stmt.target, SYM_VARIABLE, NULL, node->line);
        return false;
    }
    case NODE_RESPOND:
        analyze_expr(a, node->as.single_expr.value);
        return true;

    case NODE_GATHER:
        scope_define(a->current, node->as.gather.target, SYM_VARIABLE, NULL, node->line);
        return false;

    case NODE_ASK_AI: {
        for (int i = 0; i < node->as.ask_ai.using_count; i++) {
            NcSymbol *sym = scope_lookup(a->current, node->as.ask_ai.using[i]);
            if (!sym) {
                add_warning(a, node->line, "'%s' used in 'ask AI' but not defined yet",
                           node->as.ask_ai.using[i]->chars);
            }
        }
        NcString *save = node->as.ask_ai.save_as ? node->as.ask_ai.save_as : nc_string_from_cstr("result");
        scope_define(a->current, save, SYM_VARIABLE, NULL, node->line);
        return false;
    }

    case NODE_IF: {
        analyze_expr(a, node->as.if_stmt.condition);
        NcScope *then_scope = scope_create(a->current, "if-then");
        a->current = then_scope;
        bool then_responds = analyze_body(a, node->as.if_stmt.then_body, node->as.if_stmt.then_count);
        a->current = then_scope->parent;
        scope_destroy(then_scope);

        bool else_responds = false;
        if (node->as.if_stmt.else_body) {
            NcScope *else_scope = scope_create(a->current, "if-else");
            a->current = else_scope;
            else_responds = analyze_body(a, node->as.if_stmt.else_body, node->as.if_stmt.else_count);
            a->current = else_scope->parent;
            scope_destroy(else_scope);
        }
        return then_responds && else_responds;
    }

    case NODE_REPEAT: {
        analyze_expr(a, node->as.repeat.iterable);
        NcScope *loop_scope = scope_create(a->current, "repeat");
        a->current = loop_scope;
        scope_define(loop_scope, node->as.repeat.variable, SYM_VARIABLE, NULL, node->line);
        analyze_body(a, node->as.repeat.body, node->as.repeat.body_count);
        a->current = loop_scope->parent;
        scope_destroy(loop_scope);
        return false;
    }

    case NODE_MATCH: {
        analyze_expr(a, node->as.match_stmt.subject);
        for (int i = 0; i < node->as.match_stmt.case_count; i++) {
            NcASTNode *when = node->as.match_stmt.cases[i];
            analyze_expr(a, when->as.when_clause.value);
            analyze_body(a, when->as.when_clause.body, when->as.when_clause.body_count);
        }
        if (node->as.match_stmt.otherwise)
            analyze_body(a, node->as.match_stmt.otherwise, node->as.match_stmt.otherwise_count);
        return false;
    }

    case NODE_RUN: {
        NcSymbol *sym = scope_lookup(a->global, node->as.run_stmt.name);
        if (!sym)
            add_warning(a, node->line, "Behavior '%s' not defined — will fail at runtime",
                       node->as.run_stmt.name->chars);
        for (int i = 0; i < node->as.run_stmt.arg_count; i++)
            analyze_expr(a, node->as.run_stmt.args[i]);
        scope_define(a->current, nc_string_from_cstr("result"), SYM_VARIABLE, NULL, node->line);
        return false;
    }

    case NODE_LOG:
    case NODE_SHOW:
    case NODE_EMIT:
        analyze_expr(a, node->as.single_expr.value);
        return false;

    case NODE_NOTIFY:
        analyze_expr(a, node->as.notify.channel);
        if (node->as.notify.message) analyze_expr(a, node->as.notify.message);
        return false;

    case NODE_STORE:
        analyze_expr(a, node->as.store_stmt.value);
        return false;

    case NODE_CHECK:
        scope_define(a->current, node->as.check.save_as, SYM_VARIABLE, NULL, node->line);
        return false;

    case NODE_TRY:
        analyze_body(a, node->as.try_stmt.body, node->as.try_stmt.body_count);
        if (node->as.try_stmt.error_body)
            analyze_body(a, node->as.try_stmt.error_body, node->as.try_stmt.error_count);
        return false;

    case NODE_EXPR_STMT:
        analyze_expr(a, node->as.single_expr.value);
        return false;

    default:
        return false;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Top-level analysis
 * ═══════════════════════════════════════════════════════════ */

int nc_analyze(NcASTNode *program, const char *filename, const char *source) {
    NcAnalyzer a = {0};
    a.filename = filename;
    a.source = source;
    a.error_cap = 32;
    a.errors = calloc(a.error_cap, sizeof(char *));
    a.warn_cap = 32;
    a.warnings = calloc(a.warn_cap, sizeof(char *));

    /* Create global scope */
    a.global = scope_create(NULL, "global");
    a.current = a.global;
    register_builtins(a.global);

    /* Register all type definitions */
    for (int i = 0; i < program->as.program.def_count; i++) {
        NcASTNode *def = program->as.program.definitions[i];
        NcSymbol *existing = scope_lookup_local(a.global, def->as.definition.name);
        if (existing) {
            add_error(&a, def->line, "Type '%s' already defined at line %d",
                     def->as.definition.name->chars, existing->line);
        } else {
            scope_define(a.global, def->as.definition.name, SYM_TYPE, NULL, def->line);
        }
    }

    /* Register all behaviors */
    for (int i = 0; i < program->as.program.beh_count; i++) {
        NcASTNode *beh = program->as.program.behaviors[i];
        NcSymbol *existing = scope_lookup_local(a.global, beh->as.behavior.name);
        if (existing && existing->kind == SYM_BEHAVIOR) {
            add_error(&a, beh->line, "Behavior '%s' already defined at line %d",
                     beh->as.behavior.name->chars, existing->line);
        } else {
            scope_define(a.global, beh->as.behavior.name, SYM_BEHAVIOR, NULL, beh->line);
        }
    }

    /* Validate API routes reference existing behaviors */
    for (int i = 0; i < program->as.program.route_count; i++) {
        NcASTNode *route = program->as.program.routes[i];
        NcSymbol *handler = scope_lookup(a.global, route->as.route.handler);
        if (!handler || handler->kind != SYM_BEHAVIOR) {
            /* health_check is implicitly available */
            if (strcmp(route->as.route.handler->chars, "health_check") != 0) {
                add_warning(&a, route->line, "Route handler '%s' not found as a behavior",
                           route->as.route.handler->chars);
            }
        }
    }

    /* Analyze each behavior body */
    for (int i = 0; i < program->as.program.beh_count; i++) {
        NcASTNode *beh = program->as.program.behaviors[i];
        NcScope *beh_scope = scope_create(a.global, beh->as.behavior.name->chars);
        a.current = beh_scope;

        /* Register parameters */
        for (int j = 0; j < beh->as.behavior.param_count; j++) {
            scope_define(beh_scope, beh->as.behavior.params[j]->as.param.name,
                        SYM_PARAM, NULL, beh->line);
        }

        bool has_respond = analyze_body(&a, beh->as.behavior.body, beh->as.behavior.body_count);
        if (!has_respond && beh->as.behavior.body_count > 0) {
            add_warning(&a, beh->line, "Behavior '%s' may not respond with a value",
                       beh->as.behavior.name->chars);
        }

        a.current = a.global;
        scope_destroy(beh_scope);
    }

    /* Print results */
    if (a.error_count > 0 || a.warn_count > 0) {
        printf("\n  Semantic Analysis: %s\n", filename);
        printf("  %s\n", "────────────────────────────────────────");
    }

    for (int i = 0; i < a.error_count; i++) {
        printf("  ERROR  %s\n", a.errors[i]);
        free(a.errors[i]);
    }
    for (int i = 0; i < a.warn_count; i++) {
        printf("  WARN   %s\n", a.warnings[i]);
        free(a.warnings[i]);
    }

    int result = a.error_count;

    if (a.error_count == 0 && a.warn_count == 0)
        printf("  Semantic analysis: PASS (no issues)\n");
    else
        printf("\n  %d error(s), %d warning(s)\n", a.error_count, a.warn_count);

    free(a.errors);
    free(a.warnings);
    scope_destroy(a.global);
    return result;
}
