/*
 * nc_interp.c — Tree-walking interpreter for NC.
 *
 * Walks the AST directly and executes it.  This is how CPython
 * started before adding bytecode compilation.  The NC VM (nc_vm.c)
 * can be wired in later for performance.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include "../include/nc_version.h"
#include "../include/nc_plugin.h"
#include "nc_nova_reasoning.h"
#include <time.h>
#ifndef NC_WINDOWS
#include <regex.h>
#endif

#define printf nc_printf

extern char *nc_redact_for_display(const char *text);

/* ── Interpreter state ─────────────────────────────────────── */

typedef struct Scope {
    NcMap *vars;
    struct Scope *parent;
    bool is_function;  /* true = behavior call scope; prevents write-through to parent */
} Scope;

/* ── Call stack for tracebacks ──────────────────────────────── */

#define NC_TRACE_MAX 32

typedef struct {
    const char *behavior_name;
    const char *filename;
    int         line;
} NcTraceEntry;

typedef struct {
    NcASTNode *program;
    Scope     *global;
    NcMap     *behaviors;
    char     **output;
    int        output_count;
    int        output_cap;
    bool       had_error;
    char       error_msg[2048];

    /* Traceback — linked call stack for error reporting */
    NcTraceEntry trace[NC_TRACE_MAX];
    int          trace_depth;
    const char  *current_file;

    /* Pluggable handlers */
    NcValue (*ai_handler)(const char *prompt, NcMap *context, const char *model);
    NcValue (*mcp_handler)(const char *source, NcMap *options);
} NCInterp;

/* Signals for control flow */
typedef enum { SIG_NONE, SIG_RESPOND, SIG_STOP, SIG_SKIP } SignalType;
typedef struct { SignalType type; NcValue value; } Signal;

#define NO_SIGNAL ((Signal){SIG_NONE, NC_NONE()})

/* Forward declarations */
static double as_num(NcValue v);

/* ── Scope management ──────────────────────────────────────── */

static Scope *scope_new(Scope *parent) {
    Scope *s = malloc(sizeof(Scope));
    if (!s) return NULL;
    s->vars = nc_map_new();
    if (!s->vars) { free(s); return NULL; }
    s->parent = parent;
    s->is_function = false;
    return s;
}

static void scope_free(Scope *s) {
    nc_map_free(s->vars);
    free(s);
}

/* nc_value_retain is now public in nc_value.c */

static Scope *scope_find_owner(Scope *s, NcString *name) {
    while (s) {
        if (nc_map_has(s->vars, name)) return s;
        s = s->parent;
    }
    return NULL;
}

static void scope_set(Scope *s, NcString *name, NcValue val) {
    /* If this variable is already in the current scope, update it here */
    if (nc_map_has(s->vars, name)) {
        nc_map_set(s->vars, name, val);
        return;
    }
    /* Update an existing ancestor binding when one already exists.
     * This keeps new locals isolated while allowing nested behaviors
     * to update shared runtime/service globals. */
    Scope *owner = scope_find_owner(s->parent, name);
    if (owner) {
        nc_map_set(owner->vars, name, val);
    } else {
        nc_map_set(s->vars, name, val);
    }
}

static NcValue scope_get(Scope *s, NcString *name) {
    if (nc_map_has(s->vars, name)) return nc_map_get(s->vars, name);
    if (s->parent) return scope_get(s->parent, name);

    /* Built-in constants that resolve by name */
    if (strcmp(name->chars, "success") == 0)
        return NC_STRING(nc_string_from_cstr("success"));
    if (strcmp(name->chars, "failure") == 0)
        return NC_STRING(nc_string_from_cstr("failure"));

    return NC_NONE();
}

/*
 * Auto-correct: try to find a close match in scope.
 * Returns the corrected value and sets *corrected_name if found.
 * If distance ≤ threshold → auto-correct (high confidence).
 * If distance > threshold but within range → just suggest.
 */
static NcValue scope_autocorrect(Scope *s, const char *name, const char **corrected_name, int *distance) {
    int dist = 999;
    const char *match = nc_autocorrect_from_map(name, s->vars, &dist);

    if (match && dist <= *distance) {
        *distance = dist;
        *corrected_name = match;
    }

    if (s->parent) {
        int parent_dist = 999;
        const char *parent_name = NULL;
        NcValue parent_val = scope_autocorrect(s->parent, name, &parent_name, &parent_dist);
        if (parent_name && parent_dist < *distance) {
            *distance = parent_dist;
            *corrected_name = parent_name;
            return parent_val;
        }
    }

    if (*corrected_name) {
        NcString *ck = nc_string_from_cstr(*corrected_name);
        NcValue val = scope_get(s, ck);
        nc_string_free(ck);
        return val;
    }

    return NC_NONE();
}

static bool scope_has(Scope *s, NcString *name) {
    if (nc_map_has(s->vars, name)) return true;
    if (s->parent) return scope_has(s->parent, name);
    return false;
}

/* ── Value equality (deep comparison) ─────────────────────── */

static bool nc_value_equal(NcValue a, NcValue b) {
    if (a.type != b.type) {
        /* Allow int/float cross-comparison */
        if ((IS_INT(a) || IS_FLOAT(a)) && (IS_INT(b) || IS_FLOAT(b)))
            return as_num(a) == as_num(b);
        return false;
    }
    if (IS_INT(a))    return AS_INT(a) == AS_INT(b);
    if (IS_FLOAT(a))  return AS_FLOAT(a) == AS_FLOAT(b);
    if (IS_BOOL(a))   return AS_BOOL(a) == AS_BOOL(b);
    if (IS_STRING(a))  return nc_string_equal(AS_STRING(a), AS_STRING(b));
    if (IS_NONE(a))   return true;
    return false;  /* lists/maps: reference equality */
}

/* ── Output helper ─────────────────────────────────────────── */

static void interp_output(NCInterp *interp, const char *msg) {
    if (interp->output_count >= interp->output_cap) {
        int new_cap = interp->output_cap * 2;
        char **new_output = realloc(interp->output, sizeof(char *) * new_cap);
        if (!new_output) return;
        interp->output = new_output;
        interp->output_cap = new_cap;
    }
    interp->output[interp->output_count++] = strdup(msg);
}

/* ── Traceback helpers ─────────────────────────────────────── */

static void trace_push(NCInterp *interp, const char *behavior, int line) {
    if (interp->trace_depth < NC_TRACE_MAX) {
        NcTraceEntry *e = &interp->trace[interp->trace_depth];
        e->behavior_name = behavior;
        e->filename = interp->current_file;
        e->line = line;
    }
    interp->trace_depth++;
}

static void trace_pop(NCInterp *interp) {
    if (interp->trace_depth > 0) interp->trace_depth--;
}

static void trace_print(NCInterp *interp) {
    if (interp->trace_depth <= 0) return;

    interp_output(interp, "");
    interp_output(interp, "  Call trace (most recent last):");

    int depth = interp->trace_depth < NC_TRACE_MAX ? interp->trace_depth : NC_TRACE_MAX;
    for (int i = 0; i < depth; i++) {
        NcTraceEntry *e = &interp->trace[i];
        char buf[256];
        const char *file = e->filename ? e->filename : "<input>";
        snprintf(buf, sizeof(buf), "    in '%s' at %s, line %d",
                 e->behavior_name ? e->behavior_name : "<main>",
                 file, e->line);
        interp_output(interp, buf);
    }
}

/* ── Template resolution ───────────────────────────────────── */

static char *resolve_templates(const char *text, Scope *scope) {
    size_t result_cap = strlen(text) * 2 + 256;
    char *result = malloc(result_cap);
    if (!result) return strdup(text);
    result[0] = '\0';
    const char *p = text;
    while (*p) {
        if (*p == '{' && *(p + 1) == '{') {
            p += 2;
            char expr[128] = {0};
            int ei = 0;
            while (*p && !(*p == '}' && *(p + 1) == '}')) {
                if (ei < 126) expr[ei++] = *p;
                p++;
            }
            if (*p == '}') p += 2;
            expr[ei] = '\0';

            /* Resolve dotted expression */
            char *dot = strchr(expr, '.');
            NcValue val;
            if (dot) {
                char varname[64] = {0};
                { int vlen = (int)(dot - expr); if (vlen > 63) vlen = 63; memcpy(varname, expr, vlen); varname[vlen] = '\0'; }
                NcString *vn = nc_string_from_cstr(varname);
                val = scope_get(scope, vn);
                nc_string_free(vn);
                char *field = dot + 1;
                while (field && IS_MAP(val)) {
                    char *next_dot = strchr(field, '.');
                    char fname[64] = {0};
                    if (next_dot) { int flen = (int)(next_dot - field); if (flen > 63) flen = 63; memcpy(fname, field, flen); fname[flen] = '\0'; field = next_dot + 1; }
                    else { strncpy(fname, field, sizeof(fname) - 1); fname[sizeof(fname) - 1] = '\0'; field = NULL; }
                    NcString *fk = nc_string_from_cstr(fname);
                    val = nc_map_get(AS_MAP(val), fk);
                    nc_string_free(fk);
                }
            } else {
                while (ei > 0 && expr[ei - 1] == ' ') expr[--ei] = '\0';
                char *start = expr;
                while (*start == ' ') start++;
                NcString *vn = nc_string_from_cstr(start);
                val = scope_get(scope, vn);
                nc_string_free(vn);
            }

            NcString *vs = nc_value_to_string(val);
            size_t cur_len = strlen(result);
            size_t add_len = vs->length;
            if (cur_len + add_len >= result_cap) {
                result_cap = (cur_len + add_len) * 2 + 256;
                char *new_result = realloc(result, result_cap);
                if (!new_result) { nc_string_free(vs); break; }
                result = new_result;
            }
            memcpy(result + cur_len, vs->chars, add_len + 1);
            nc_string_free(vs);
        } else {
            int len = strlen(result);
            result[len] = *p;
            result[len + 1] = '\0';
            p++;
        }
    }
    return result;
}

/* ── Parallel AI call worker ────────────────────────────────── */

typedef struct {
    const char *prompt;
    NcMap      *context;
    NcValue    *out;
} NcParallelAICtx;

#ifdef NC_WINDOWS
static unsigned __stdcall nc_parallel_ai_worker(void *arg) {
#else
static void *nc_parallel_ai_worker(void *arg) {
#endif
    NcParallelAICtx *c = (NcParallelAICtx *)arg;
    if (c->prompt) *c->out = nc_ai_call(c->prompt, c->context, NULL);
#ifdef NC_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

/* ── Forward declarations ──────────────────────────────────── */

static NcValue eval_expr_impl(NCInterp *interp, NcASTNode *node, Scope *scope);
static Signal exec_stmt(NCInterp *interp, NcASTNode *node, Scope *scope);
static Signal exec_body(NCInterp *interp, NcASTNode **stmts, int count, Scope *scope);

#define NC_MAX_EVAL_DEPTH 1000
static nc_thread_local int eval_depth = 0;

static nc_thread_local double nc_request_deadline_ms = 0;

void nc_set_request_deadline(double deadline_ms) {
    nc_request_deadline_ms = deadline_ms;
}

static bool nc_deadline_exceeded(void) {
    if (nc_request_deadline_ms <= 0) return false;
    return nc_realtime_ms() > nc_request_deadline_ms;
}

static NcValue eval_expr(NCInterp *interp, NcASTNode *node, Scope *scope) {
    if (!node) return NC_NONE();
    if (++eval_depth > NC_MAX_EVAL_DEPTH) {
        eval_depth--;
        snprintf(interp->error_msg, sizeof(interp->error_msg),
            "Maximum recursion depth (%d) exceeded", NC_MAX_EVAL_DEPTH);
        interp->had_error = true;
        return NC_NONE();
    }
    NcValue result = eval_expr_impl(interp, node, scope);
    eval_depth--;
    return result;
}

/* ── Expression evaluation ─────────────────────────────────── */

static double as_num(NcValue v) {
    if (IS_INT(v)) return (double)AS_INT(v);
    if (IS_FLOAT(v)) return AS_FLOAT(v);
    if (IS_STRING(v) && AS_STRING(v)) {
        const char *s = AS_STRING(v)->chars;
        char *end;
        double d = strtod(s, &end);
        if (end != s) return d;
    }
    return 0.0;
}

static NcValue eval_expr_impl(NCInterp *interp, NcASTNode *node, Scope *scope) {
    if (!node) return NC_NONE();

    switch (node->type) {
    case NODE_INT_LIT:    return NC_INT(node->as.int_lit.value);
    case NODE_FLOAT_LIT:  return NC_FLOAT(node->as.float_lit.value);
    case NODE_BOOL_LIT:   return NC_BOOL(node->as.bool_lit.value);
    case NODE_NONE_LIT:   return NC_NONE();

    case NODE_STRING_LIT: {
        char *resolved = resolve_templates(node->as.string_lit.value->chars, scope);
        NcString *s = nc_string_from_cstr(resolved);
        free(resolved);
        return NC_STRING(s);
    }

    case NODE_TEMPLATE: {
        char buf[512];
        snprintf(buf, sizeof(buf), "{{%s}}", node->as.template_lit.expr->chars);
        char *resolved = resolve_templates(buf, scope);
        NcString *s = nc_string_from_cstr(resolved);
        free(resolved);
        return NC_STRING(s);
    }

    case NODE_IDENT: {
        NcValue val = scope_get(scope, node->as.ident.name);
        if (IS_NONE(val) && !scope_has(scope, node->as.ident.name)) {
            const char *corrected = NULL;
            int dist = 999;
            NcValue corrected_val = scope_autocorrect(scope, node->as.ident.name->chars, &corrected, &dist);

            if (corrected && dist <= NC_AUTOCORRECT_THRESHOLD) {
                char hint[512];
                snprintf(hint, sizeof(hint),
                    "[line %d] Auto-corrected '%s' → '%s'",
                    node->line, node->as.ident.name->chars, corrected);
                interp_output(interp, hint);
                return corrected_val;
            } else if (corrected) {
                char hint[512];
                snprintf(hint, sizeof(hint),
                    "[line %d] I don't recognize '%s'. Is this what you meant → '%s'?",
                    node->line, node->as.ident.name->chars, corrected);
                interp_output(interp, hint);
            }
        }
        return val;
    }

    case NODE_RUN: {
        NcString *name = node->as.run_stmt.name;
        NcASTNode *prog = interp->program;
        NcASTNode *found_beh = NULL;

        for (int b = 0; b < prog->as.program.beh_count; b++) {
            if (nc_string_equal(prog->as.program.behaviors[b]->as.behavior.name, name)) {
                found_beh = prog->as.program.behaviors[b];
                break;
            }
        }

        if (!found_beh) {
            for (int m = 0; m < prog->as.program.import_count; m++) {
                NcASTNode *mod_ast = nc_module_load_file(
                    prog->as.program.imports[m]->as.import_decl.module->chars, NULL);
                if (mod_ast) {
                    for (int b = 0; b < mod_ast->as.program.beh_count; b++) {
                        if (nc_string_equal(mod_ast->as.program.behaviors[b]->as.behavior.name, name)) {
                            found_beh = mod_ast->as.program.behaviors[b];
                            break;
                        }
                    }
                }
                if (found_beh) break;
            }
        }

        if (!found_beh) {
            const char *suggestion = nc_suggest_builtin(name->chars);
            if (suggestion) {
                char hint[512];
                snprintf(hint, sizeof(hint),
                    "[line %d] I don't know the behavior '%s'. Is this what you meant → '%s'?",
                    node->line, name->chars, suggestion);
                interp_output(interp, hint);
            }
            return NC_NONE();
        }

        if (getenv("NC_TRACE_RUNS")) {
            fprintf(stderr, "[NC RUN expr] %s\n", name->chars);
        }
        Scope *call_scope = scope_new(interp->global);
        if (!call_scope) return NC_NONE();
        call_scope->is_function = true;

        for (int i = 0; i < node->as.run_stmt.arg_count && i < found_beh->as.behavior.param_count; i++) {
            NcValue arg = eval_expr(interp, node->as.run_stmt.args[i], scope);
            nc_map_set(call_scope->vars, found_beh->as.behavior.params[i]->as.param.name, arg);
        }
        if (node->as.run_stmt.arg_count == 0 && found_beh->as.behavior.param_count > 0) {
            for (int i = 0; i < found_beh->as.behavior.param_count; i++) {
                NcString *param = found_beh->as.behavior.params[i]->as.param.name;
                if (scope_has(scope, param)) {
                    nc_map_set(call_scope->vars, param, scope_get(scope, param));
                }
            }
        }

        trace_push(interp, name->chars, node->line);
        Signal sig = exec_body(interp, found_beh->as.behavior.body,
                               found_beh->as.behavior.body_count, call_scope);
        trace_pop(interp);

        NcValue run_result = NC_NONE();
        if (sig.type == SIG_RESPOND) {
            run_result = sig.value;
            nc_value_retain(run_result);
        }

        scope_free(call_scope);
        return run_result;
    }

    case NODE_LIST_LIT: {
        NcList *list = nc_list_new();
        for (int i = 0; i < node->as.list_lit.count; i++)
            nc_list_push(list, eval_expr(interp, node->as.list_lit.elements[i], scope));
        return NC_LIST(list);
    }

    case NODE_MAP_LIT: {
        NcMap *map = nc_map_new();
        for (int i = 0; i < node->as.map_lit.count; i++) {
            NcValue val = eval_expr(interp, node->as.map_lit.values[i], scope);
            /* Resolve {{templates}} in keys */
            NcString *raw_key = node->as.map_lit.keys[i];
            char *resolved_key = resolve_templates(raw_key->chars, scope);
            NcString *key = nc_string_from_cstr(resolved_key);
            free(resolved_key);
            nc_map_set(map, key, val);
        }
        return NC_MAP(map);
    }

    case NODE_DOT: {
        NcValue obj = eval_expr(interp, node->as.dot.object, scope);
        if (IS_MAP(obj)) return nc_map_get(AS_MAP(obj), node->as.dot.member);
        /* Auto-parse JSON strings for dot access:
         * If obj is a string that looks like JSON, parse it first.
         * This handles cases where http_get returns raw JSON as a string
         * (e.g., when Accept header wasn't sent or response was text/plain). */
        if (IS_STRING(obj) && AS_STRING(obj)->length > 1) {
            const char *s = AS_STRING(obj)->chars;
            if (s[0] == '{' || s[0] == '[') {
                NcValue parsed = nc_json_parse(s);
                if (IS_MAP(parsed))
                    return nc_map_get(AS_MAP(parsed), node->as.dot.member);
                if (IS_LIST(parsed))
                    return parsed;
            }
        }
        return NC_NONE();
    }

    case NODE_CALL: {
        NcString *name = node->as.call.name;
        NcValue fn = scope_get(scope, name);

        /* Built-in functions */
        if (strcmp(name->chars, "len") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) return NC_INT(AS_LIST(arg)->count);
            if (IS_STRING(arg)) return NC_INT(AS_STRING(arg)->length);
            if (IS_MAP(arg)) return NC_INT(AS_MAP(arg)->count);
            return NC_INT(0);
        }
        if (strcmp(name->chars, "str") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            return NC_STRING(nc_value_to_string(arg));
        }
        if (strcmp(name->chars, "int") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            return NC_INT((int64_t)as_num(arg));
        }
        if (strcmp(name->chars, "print") == 0) {
            for (int i = 0; i < node->as.call.arg_count; i++) {
                NcValue arg = eval_expr(interp, node->as.call.args[i], scope);
                NcString *s = nc_value_to_string(arg);
                interp_output(interp, s->chars);
                nc_string_free(s);
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "keys") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_MAP(arg)) {
                NcList *list = nc_list_new();
                for (int i = 0; i < AS_MAP(arg)->count; i++)
                    nc_list_push(list, NC_STRING(nc_string_ref(AS_MAP(arg)->keys[i])));
                return NC_LIST(list);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "get") == 0 &&
            node->as.call.arg_count >= 2 &&
            node->as.call.arg_count <= 3) {
            NcValue container = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key = eval_expr(interp, node->as.call.args[1], scope);
            NcValue fallback = node->as.call.arg_count == 3
                ? eval_expr(interp, node->as.call.args[2], scope)
                : NC_NONE();

            if (IS_MAP(container)) {
                NcString *map_key = IS_STRING(key)
                    ? nc_string_ref(AS_STRING(key))
                    : nc_value_to_string(key);
                NcValue found = nc_map_get(AS_MAP(container), map_key);
                nc_string_free(map_key);
                if (IS_NONE(found) && node->as.call.arg_count == 3) return fallback;
                return found;
            }
            if (IS_LIST(container) && IS_INT(key)) {
                NcList *list = AS_LIST(container);
                int64_t idx = AS_INT(key);
                if (idx >= 0 && idx < list->count) return list->items[idx];
                if (node->as.call.arg_count == 3) return fallback;
                return NC_NONE();
            }
            if (node->as.call.arg_count == 3) return fallback;
            return NC_NONE();
        }
        if (strcmp(name->chars, "values") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_MAP(arg)) {
                NcList *list = nc_list_new();
                for (int i = 0; i < AS_MAP(arg)->count; i++)
                    nc_list_push(list, AS_MAP(arg)->values[i]);
                return NC_LIST(list);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "upper") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_upper(AS_STRING(arg));
            return arg;
        }
        if (strcmp(name->chars, "lower") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_lower(AS_STRING(arg));
            return arg;
        }
        if (strcmp(name->chars, "trim") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_trim(AS_STRING(arg));
            return arg;
        }
        if (strcmp(name->chars, "split") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue d = eval_expr(interp, node->as.call.args[1], scope);
            /* Auto-coerce non-string to string for robustness */
            if (!IS_STRING(s) && !IS_NONE(s)) {
                NcString *str = nc_value_to_string(s);
                s = NC_STRING(str);
            }
            if (IS_STRING(s) && IS_STRING(d))
                return nc_stdlib_split(AS_STRING(s), AS_STRING(d));
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "join") == 0 && node->as.call.arg_count == 2) {
            NcValue l = eval_expr(interp, node->as.call.args[0], scope);
            NcValue s = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(l) && IS_STRING(s))
                return nc_stdlib_join(AS_LIST(l), AS_STRING(s));
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "contains") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_STRING(n))
                return nc_stdlib_contains(AS_STRING(s), AS_STRING(n));
            if (IS_LIST(s)) {
                NcList *list = AS_LIST(s);
                for (int i = 0; i < list->count; i++) {
                    if (nc_value_equal(list->items[i], n))
                        return NC_BOOL(true);
                }
                return NC_BOOL(false);
            }
            if (IS_MAP(s) && IS_STRING(n))
                return NC_BOOL(nc_map_has(AS_MAP(s), AS_STRING(n)));
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "replace") == 0 && node->as.call.arg_count == 3) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue o = eval_expr(interp, node->as.call.args[1], scope);
            NcValue n = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_STRING(s) && IS_STRING(o) && IS_STRING(n))
                return nc_stdlib_replace(AS_STRING(s), AS_STRING(o), AS_STRING(n));
            return s;
        }
        if (strcmp(name->chars, "abs") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            double val = as_num(arg);
            double result = fabs(val);
            if (IS_INT(arg) && result == (int64_t)result)
                return NC_INT((int64_t)result);
            return NC_FLOAT(result);
        }
        if (strcmp(name->chars, "sqrt") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_sqrt(IS_INT(arg) ? (double)AS_INT(arg) : AS_FLOAT(arg));
        }
        if (strcmp(name->chars, "time_now") == 0 && node->as.call.arg_count == 0)
            return nc_stdlib_time_now();
        if (strcmp(name->chars, "random") == 0 && node->as.call.arg_count == 0)
            return nc_stdlib_random();
        if (strcmp(name->chars, "cos") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_FLOAT(cos(IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a)));
        }
        if (strcmp(name->chars, "sin") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_FLOAT(sin(IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a)));
        }
        if (strcmp(name->chars, "tan") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_FLOAT(tan(IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a)));
        }
        if (strcmp(name->chars, "log") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            double v = IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a);
            return v > 0 ? NC_FLOAT(log(v)) : NC_FLOAT(0);
        }
        if (strcmp(name->chars, "exp") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_FLOAT(exp(IS_FLOAT(a) ? AS_FLOAT(a) : (double)AS_INT(a)));
        }
        if (strcmp(name->chars, "enumerate") == 0 && node->as.call.arg_count == 1) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            if (!IS_LIST(list)) return NC_LIST(nc_list_new());
            NcList *src = AS_LIST(list);
            NcList *out = nc_list_new();
            for (int i = 0; i < src->count; i++) {
                NcList *pair = nc_list_new();
                nc_list_push(pair, NC_INT(i));
                nc_list_push(pair, src->items[i]);
                nc_list_push(out, NC_LIST(pair));
            }
            return NC_LIST(out);
        }
        if (strcmp(name->chars, "zip") == 0 && node->as.call.arg_count == 2) {
            NcValue la = eval_expr(interp, node->as.call.args[0], scope);
            NcValue lb = eval_expr(interp, node->as.call.args[1], scope);
            if (!IS_LIST(la) || !IS_LIST(lb)) return NC_LIST(nc_list_new());
            NcList *a = AS_LIST(la), *b = AS_LIST(lb);
            NcList *out = nc_list_new();
            int n = a->count < b->count ? a->count : b->count;
            for (int i = 0; i < n; i++) {
                NcList *pair = nc_list_new();
                nc_list_push(pair, a->items[i]);
                nc_list_push(pair, b->items[i]);
                nc_list_push(out, NC_LIST(pair));
            }
            return NC_LIST(out);
        }
        if (strcmp(name->chars, "filter") == 0 && node->as.call.arg_count >= 1) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            if (!IS_LIST(list)) return NC_LIST(nc_list_new());
            NcList *src = AS_LIST(list);
            NcList *out = nc_list_new();
            for (int i = 0; i < src->count; i++) {
                if (nc_truthy(src->items[i]))
                    nc_list_push(out, src->items[i]);
            }
            return NC_LIST(out);
        }
        if (strcmp(name->chars, "uuid") == 0 && node->as.call.arg_count == 0) {
            char ubuf[40];
            snprintf(ubuf, sizeof(ubuf), "%08x-%04x-%04x-%04x-%04x%08x",
                (uint32_t)rand(), (uint16_t)rand()&0xffff, (uint16_t)(0x4000|(rand()&0x0fff)),
                (uint16_t)(0x8000|(rand()&0x3fff)), (uint16_t)rand()&0xffff, (uint32_t)rand());
            return NC_STRING(nc_string_from_cstr(ubuf));
        }
        if (strcmp(name->chars, "hash") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcString *s = nc_value_to_string(a);
            uint32_t h = nc_hash_string(s->chars, s->length);
            char hbuf[16]; snprintf(hbuf, sizeof(hbuf), "%08x", h);
            nc_string_free(s);
            return NC_STRING(nc_string_from_cstr(hbuf));
        }
        if (strcmp(name->chars, "hex") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "0x%llx", (long long)AS_INT(a));
            return NC_STRING(nc_string_from_cstr(hbuf));
        }
        if (strcmp(name->chars, "bin") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            int64_t v = AS_INT(a);
            char bbuf[72] = "0b"; int bi = 2;
            if (v == 0) { bbuf[bi++] = '0'; }
            else { for (int bit = 63; bit >= 0; bit--) { if (bi > 2 || (v >> bit) & 1) bbuf[bi++] = ((v >> bit) & 1) ? '1' : '0'; } }
            bbuf[bi] = '\0';
            return NC_STRING(nc_string_from_cstr(bbuf));
        }
        /* Legacy re_match/re_find/re_replace — now use custom engine (cross-platform) */
        /* These are kept for backward compatibility with (text, pattern) arg order */
        if (strcmp(name->chars, "re_find") == 0 && node->as.call.arg_count == 2) {
            NcValue text = eval_expr(interp, node->as.call.args[0], scope);
            NcValue pat = eval_expr(interp, node->as.call.args[1], scope);
            if (!IS_STRING(text) || !IS_STRING(pat)) return NC_NONE();
            return nc_stdlib_re_search(AS_STRING(pat)->chars, AS_STRING(text)->chars);
        }
        if (strcmp(name->chars, "json_encode") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            char *json = nc_json_serialize(arg, false);
            NcValue r = NC_STRING(nc_string_from_cstr(json));
            free(json);
            return r;
        }
        if (strcmp(name->chars, "json_decode") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            /* If already parsed (list/map/int/float/bool), return as-is */
            if (IS_LIST(arg) || IS_MAP(arg) || IS_INT(arg) || IS_FLOAT(arg) || IS_BOOL(arg))
                return arg;
            if (IS_STRING(arg)) {
                NcValue parsed = nc_json_parse(AS_STRING(arg)->chars);
                if (IS_NONE(parsed) && AS_STRING(arg)->length > 0) {
                    /* JSON parse failed — signal error for try/on_error */
                    snprintf(interp->error_msg, sizeof(interp->error_msg),
                        "json_decode: invalid JSON input");
                    interp->had_error = true;
                }
                return parsed;
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "env") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_env_get(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "sort") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) return nc_stdlib_list_sort(AS_LIST(arg));
            return arg;
        }
        if ((strcmp(name->chars, "sort_by") == 0 || strcmp(name->chars, "sort") == 0)
            && node->as.call.arg_count == 2) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(arg) && IS_STRING(field))
                return nc_stdlib_list_sort_by(AS_LIST(arg), AS_STRING(field));
            return arg;
        }
        if (strcmp(name->chars, "max_by") == 0 && node->as.call.arg_count == 2) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(arg) && IS_STRING(field))
                return nc_stdlib_list_max_by(AS_LIST(arg), AS_STRING(field));
            return NC_NONE();
        }
        if (strcmp(name->chars, "min_by") == 0 && node->as.call.arg_count == 2) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(arg) && IS_STRING(field))
                return nc_stdlib_list_min_by(AS_LIST(arg), AS_STRING(field));
            return NC_NONE();
        }
        if (strcmp(name->chars, "sum_by") == 0 && node->as.call.arg_count == 2) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(arg) && IS_STRING(field))
                return nc_stdlib_list_sum_by(AS_LIST(arg), AS_STRING(field));
            return NC_FLOAT(0);
        }
        if (strcmp(name->chars, "map_field") == 0 && node->as.call.arg_count == 2) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(arg) && IS_STRING(field))
                return nc_stdlib_list_map_field(AS_LIST(arg), AS_STRING(field));
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "filter_by") == 0 && node->as.call.arg_count == 4) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcValue field = eval_expr(interp, node->as.call.args[1], scope);
            NcValue op = eval_expr(interp, node->as.call.args[2], scope);
            NcValue thresh = eval_expr(interp, node->as.call.args[3], scope);
            if (IS_LIST(arg) && IS_STRING(field) && IS_STRING(op)) {
                if (IS_STRING(thresh))
                    return nc_stdlib_list_filter_by_str(AS_LIST(arg), AS_STRING(field),
                        AS_STRING(op)->chars, AS_STRING(thresh));
                return nc_stdlib_list_filter_by(AS_LIST(arg), AS_STRING(field),
                    AS_STRING(op)->chars, as_num(thresh));
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "reverse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) return nc_stdlib_list_reverse(AS_LIST(arg));
            return arg;
        }
        if (strcmp(name->chars, "range") == 0) {
            int64_t start = 0, end = 0, step = 1;
            if (node->as.call.arg_count >= 1) {
                NcValue a0 = eval_expr(interp, node->as.call.args[0], scope);
                end = IS_INT(a0) ? AS_INT(a0) : (int64_t)AS_FLOAT(a0);
            }
            if (node->as.call.arg_count >= 2) {
                NcValue a1 = eval_expr(interp, node->as.call.args[1], scope);
                start = end;
                end = IS_INT(a1) ? AS_INT(a1) : (int64_t)AS_FLOAT(a1);
            }
            NcList *l = nc_list_new();
            for (int64_t i = start; i < end; i += step)
                nc_list_push(l, NC_INT(i));
            return NC_LIST(l);
        }
        if (strcmp(name->chars, "remove") == 0 && node->as.call.arg_count == 2) {
            NcValue list_val = eval_expr(interp, node->as.call.args[0], scope);
            NcValue item = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(list_val)) {
                NcList *list = AS_LIST(list_val);
                for (int i = 0; i < list->count; i++) {
                    bool match = false;
                    if (IS_STRING(item) && IS_STRING(list->items[i]))
                        match = nc_string_equal(AS_STRING(item), AS_STRING(list->items[i]));
                    else if (IS_INT(item) && IS_INT(list->items[i]))
                        match = AS_INT(item) == AS_INT(list->items[i]);
                    else if (IS_FLOAT(item) && IS_FLOAT(list->items[i]))
                        match = AS_FLOAT(item) == AS_FLOAT(list->items[i]);
                    if (match) {
                        NcValue removed = list->items[i];
                        for (int j = i; j < list->count - 1; j++)
                            list->items[j] = list->items[j + 1];
                        list->count--;
                        nc_value_release(removed);
                        break;
                    }
                }
                return NC_LIST(list);
            }
            return list_val;
        }
        if (strcmp(name->chars, "type") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            const char *t = "none";
            if (IS_INT(arg)) t = "number";
            else if (IS_FLOAT(arg)) t = "number";
            else if (IS_BOOL(arg)) t = "yesno";
            else if (IS_STRING(arg)) t = "text";
            else if (IS_LIST(arg)) t = "list";
            else if (IS_MAP(arg)) t = "record";
            return NC_STRING(nc_string_from_cstr(t));
        }

        /* ── format("Hello {name}, age {age}", vars_map) ──── */
        if (strcmp(name->chars, "format") == 0 && node->as.call.arg_count >= 1) {
            NcValue tpl = eval_expr(interp, node->as.call.args[0], scope);
            if (!IS_STRING(tpl)) return tpl;
            const char *src = AS_STRING(tpl)->chars;
            int src_len = AS_STRING(tpl)->length;
            /* Collect format args — either a map or positional args */
            NcMap *vars = NULL;
            NcValue *positional = NULL;
            int pos_count = 0;
            if (node->as.call.arg_count == 2) {
                NcValue a1 = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_MAP(a1)) vars = AS_MAP(a1);
            }
            if (!vars && node->as.call.arg_count > 1) {
                pos_count = node->as.call.arg_count - 1;
                positional = malloc(sizeof(NcValue) * pos_count);
                for (int fi = 0; fi < pos_count; fi++)
                    positional[fi] = eval_expr(interp, node->as.call.args[fi + 1], scope);
            }
            /* Build result string */
            char *out = malloc(src_len * 4 + 4096);
            int oi = 0, pi = 0;
            for (int si = 0; si < src_len; si++) {
                if (src[si] == '{') {
                    int end = si + 1;
                    while (end < src_len && src[end] != '}') end++;
                    if (end < src_len) {
                        char key[128] = {0};
                        int klen = end - si - 1;
                        if (klen > 127) klen = 127;
                        memcpy(key, src + si + 1, klen);
                        key[klen] = '\0';
                        NcValue val = NC_NONE();
                        if (vars) {
                            NcString *ks = nc_string_from_cstr(key);
                            val = nc_map_get(vars, ks);
                            nc_string_free(ks);
                        } else if (positional && pi < pos_count) {
                            val = positional[pi++];
                        }
                        NcString *vs = nc_value_to_string(val);
                        memcpy(out + oi, vs->chars, vs->length);
                        oi += vs->length;
                        nc_string_free(vs);
                        si = end; /* skip past } */
                        continue;
                    }
                }
                out[oi++] = src[si];
            }
            out[oi] = '\0';
            if (positional) free(positional);
            NcString *result = nc_string_from_cstr(out);
            free(out);
            return NC_STRING(result);
        }

        /* ── set_new(list) — create set (unique values) ───── */
        if (strcmp(name->chars, "set_new") == 0) {
            NcMap *set_map = nc_map_new();
            if (node->as.call.arg_count >= 1) {
                NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
                if (IS_LIST(arg)) {
                    NcList *l = AS_LIST(arg);
                    for (int i = 0; i < l->count; i++) {
                        NcString *ks = nc_value_to_string(l->items[i]);
                        nc_map_set(set_map, ks, NC_BOOL(true));
                    }
                }
            }
            return NC_MAP(set_map);
        }
        /* ── set_add(set, value) ──────────────────────────── */
        if (strcmp(name->chars, "set_add") == 0 && node->as.call.arg_count == 2) {
            NcValue set = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(set)) {
                NcString *ks = nc_value_to_string(val);
                nc_map_set(AS_MAP(set), ks, NC_BOOL(true));
            }
            return set;
        }
        /* ── set_has(set, value) ──────────────────────────── */
        if (strcmp(name->chars, "set_has") == 0 && node->as.call.arg_count == 2) {
            NcValue set = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(set)) {
                NcString *ks = nc_value_to_string(val);
                bool has = nc_map_has(AS_MAP(set), ks);
                nc_string_free(ks);
                return NC_BOOL(has);
            }
            return NC_BOOL(false);
        }
        /* ── set_remove(set, value) ───────────────────────── */
        if (strcmp(name->chars, "set_remove") == 0 && node->as.call.arg_count == 2) {
            NcValue set = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(set)) {
                NcString *ks = nc_value_to_string(val);
                nc_map_set(AS_MAP(set), ks, NC_NONE());
                nc_string_free(ks);
            }
            return set;
        }
        /* ── set_values(set) — get list of set members ────── */
        if (strcmp(name->chars, "set_values") == 0 && node->as.call.arg_count == 1) {
            NcValue set = eval_expr(interp, node->as.call.args[0], scope);
            NcList *result = nc_list_new();
            if (IS_MAP(set)) {
                NcMap *m = AS_MAP(set);
                for (int i = 0; i < m->count; i++) {
                    if (!IS_NONE(m->values[i]))
                        nc_list_push(result, NC_STRING(nc_string_ref(m->keys[i])));
                }
            }
            return NC_LIST(result);
        }

        /* ── tuple(a, b, ...) — immutable list ────────────── */
        if (strcmp(name->chars, "tuple") == 0) {
            NcList *t = nc_list_new();
            for (int i = 0; i < node->as.call.arg_count; i++) {
                NcValue v = eval_expr(interp, node->as.call.args[i], scope);
                nc_list_push(t, v);
            }
            return NC_LIST(t);
        }

        /* ── deque() — double-ended queue ─────────────────── */
        if (strcmp(name->chars, "deque") == 0) {
            NcList *d = nc_list_new();
            if (node->as.call.arg_count >= 1) {
                NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
                if (IS_LIST(arg)) {
                    NcList *l = AS_LIST(arg);
                    for (int i = 0; i < l->count; i++)
                        nc_list_push(d, l->items[i]);
                }
            }
            return NC_LIST(d);
        }
        /* ── deque_push_front(deque, value) ───────────────── */
        if (strcmp(name->chars, "deque_push_front") == 0 && node->as.call.arg_count == 2) {
            NcValue dq = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(dq)) {
                NcList *l = AS_LIST(dq);
                /* Shift elements right and insert at front */
                nc_list_push(l, NC_NONE()); /* expand capacity */
                for (int i = l->count - 1; i > 0; i--)
                    l->items[i] = l->items[i - 1];
                nc_value_retain(val);
                l->items[0] = val;
            }
            return dq;
        }
        /* ── deque_pop_front(deque) ───────────────────────── */
        if (strcmp(name->chars, "deque_pop_front") == 0 && node->as.call.arg_count == 1) {
            NcValue dq = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(dq) && AS_LIST(dq)->count > 0) {
                NcList *l = AS_LIST(dq);
                NcValue front = l->items[0];
                for (int i = 0; i < l->count - 1; i++)
                    l->items[i] = l->items[i + 1];
                l->count--;
                return front;
            }
            return NC_NONE();
        }

        /* ── counter(list) — count occurrences ────────────── */
        if (strcmp(name->chars, "counter") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcMap *counts = nc_map_new();
            if (IS_LIST(arg)) {
                NcList *l = AS_LIST(arg);
                for (int i = 0; i < l->count; i++) {
                    NcString *ks = nc_value_to_string(l->items[i]);
                    NcValue existing = nc_map_get(counts, ks);
                    int64_t cnt = IS_INT(existing) ? AS_INT(existing) : 0;
                    nc_map_set(counts, ks, NC_INT(cnt + 1));
                }
            }
            return NC_MAP(counts);
        }

        /* ── default_map(default_value) ───────────────────── */
        if (strcmp(name->chars, "default_map") == 0) {
            NcMap *dm = nc_map_new();
            if (node->as.call.arg_count >= 1) {
                NcValue def = eval_expr(interp, node->as.call.args[0], scope);
                nc_map_set(dm, nc_string_from_cstr("_default"), def);
            }
            return NC_MAP(dm);
        }

        /* ── enumerate(list) — returns [[0,item],[1,item],...] */
        if (strcmp(name->chars, "enumerate") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcList *result = nc_list_new();
            if (IS_LIST(arg)) {
                NcList *l = AS_LIST(arg);
                for (int i = 0; i < l->count; i++) {
                    NcList *pair = nc_list_new();
                    nc_list_push(pair, NC_INT(i));
                    nc_list_push(pair, l->items[i]);
                    nc_list_push(result, NC_LIST(pair));
                }
            }
            return NC_LIST(result);
        }

        /* ── zip(list1, list2) ────────────────────────────── */
        if (strcmp(name->chars, "zip") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            NcList *result = nc_list_new();
            if (IS_LIST(a) && IS_LIST(b)) {
                NcList *la = AS_LIST(a), *lb = AS_LIST(b);
                int len = la->count < lb->count ? la->count : lb->count;
                for (int i = 0; i < len; i++) {
                    NcList *pair = nc_list_new();
                    nc_list_push(pair, la->items[i]);
                    nc_list_push(pair, lb->items[i]);
                    nc_list_push(result, NC_LIST(pair));
                }
            }
            return NC_LIST(result);
        }

        /* ── error_type(msg) — classify error string ──────── */
        if (strcmp(name->chars, "error_type") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) {
                const char *msg = AS_STRING(arg)->chars;
                if (strstr(msg, "timeout") || strstr(msg, "Timeout"))
                    return NC_STRING(nc_string_from_cstr("TimeoutError"));
                if (strstr(msg, "connect") || strstr(msg, "HTTP"))
                    return NC_STRING(nc_string_from_cstr("ConnectionError"));
                if (strstr(msg, "parse") || strstr(msg, "JSON"))
                    return NC_STRING(nc_string_from_cstr("ParseError"));
                if (strstr(msg, "index") || strstr(msg, "range"))
                    return NC_STRING(nc_string_from_cstr("IndexError"));
                if (strstr(msg, "type") || strstr(msg, "convert"))
                    return NC_STRING(nc_string_from_cstr("ValueError"));
                if (strstr(msg, "file") || strstr(msg, "read"))
                    return NC_STRING(nc_string_from_cstr("IOError"));
            }
            return NC_STRING(nc_string_from_cstr("Error"));
        }

        /* ── traceback() — returns current call trace ─────── */
        if (strcmp(name->chars, "traceback") == 0 && node->as.call.arg_count == 0) {
            NcList *tb = nc_list_new();
            int depth = interp->trace_depth < NC_TRACE_MAX ? interp->trace_depth : NC_TRACE_MAX;
            for (int i = 0; i < depth; i++) {
                NcTraceEntry *e = &interp->trace[i];
                NcMap *frame = nc_map_new();
                nc_map_set(frame, nc_string_from_cstr("behavior"),
                    NC_STRING(nc_string_from_cstr(e->behavior_name ? e->behavior_name : "<main>")));
                nc_map_set(frame, nc_string_from_cstr("file"),
                    NC_STRING(nc_string_from_cstr(e->filename ? e->filename : "<input>")));
                nc_map_set(frame, nc_string_from_cstr("line"), NC_INT(e->line));
                nc_list_push(tb, NC_MAP(frame));
            }
            return NC_LIST(tb);
        }
        if (strcmp(name->chars, "starts_with") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue p = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_STRING(p))
                return nc_stdlib_starts_with(AS_STRING(s), AS_STRING(p));
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "ends_with") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue p = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_STRING(p))
                return nc_stdlib_ends_with(AS_STRING(s), AS_STRING(p));
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "read_file") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_read_file(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "write_file") == 0 && node->as.call.arg_count == 2) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue content = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(path) && IS_STRING(content))
                return nc_stdlib_write_file(AS_STRING(path)->chars, AS_STRING(content)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "write_file_atomic") == 0 && node->as.call.arg_count == 2) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue content = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(path) && IS_STRING(content))
                return nc_stdlib_write_file_atomic(AS_STRING(path)->chars, AS_STRING(content)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "file_exists") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_file_exists(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if ((strcmp(name->chars, "delete_file") == 0 || strcmp(name->chars, "remove_file") == 0)
            && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_delete_file(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if ((strcmp(name->chars, "mkdir") == 0 || strcmp(name->chars, "create_directory") == 0)
            && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_mkdir(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "exec") == 0 && node->as.call.arg_count >= 1) {
            NcList *args = nc_list_new();
            for (int i = 0; i < node->as.call.arg_count; i++)
                nc_list_push(args, eval_expr(interp, node->as.call.args[i], scope));
            return nc_stdlib_exec(args);
        }
        if (strcmp(name->chars, "shell") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_shell(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "shell_exec") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_shell_exec(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "jwt_generate") == 0 && node->as.call.arg_count >= 2) {
            NcValue user = eval_expr(interp, node->as.call.args[0], scope);
            NcValue role = eval_expr(interp, node->as.call.args[1], scope);
            int exp_secs = 3600;
            NcMap *extra = NULL;
            if (node->as.call.arg_count >= 3) {
                NcValue e = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_INT(e)) exp_secs = (int)AS_INT(e);
            }
            if (node->as.call.arg_count >= 4) {
                NcValue x = eval_expr(interp, node->as.call.args[3], scope);
                if (IS_MAP(x)) extra = AS_MAP(x);
            }
            return nc_jwt_generate(
                IS_STRING(user) ? AS_STRING(user)->chars : "anonymous",
                IS_STRING(role) ? AS_STRING(role)->chars : "user",
                exp_secs, extra);
        }
        if (strcmp(name->chars, "jwt_verify") == 0 && node->as.call.arg_count >= 1) {
            NcValue token_v = eval_expr(interp, node->as.call.args[0], scope);
            if (!IS_STRING(token_v)) return NC_BOOL(false);
            const char *token = AS_STRING(token_v)->chars;

            /* Optional second arg: secret (overrides NC_JWT_SECRET) */
            if (node->as.call.arg_count >= 2) {
                NcValue secret_v = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(secret_v))
                    nc_setenv("NC_JWT_SECRET", AS_STRING(secret_v)->chars, 1);
            }

            char auth_header[8192];
            snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
            NcAuthContext ctx = nc_mw_auth_check(auth_header);
            if (!ctx.authenticated) return NC_BOOL(false);

            NcMap *claims = nc_map_new();
            nc_map_set(claims, nc_string_from_cstr("sub"),
                       NC_STRING(nc_string_from_cstr(ctx.user_id)));
            nc_map_set(claims, nc_string_from_cstr("role"),
                       NC_STRING(nc_string_from_cstr(ctx.role)));
            nc_map_set(claims, nc_string_from_cstr("tenant_id"),
                       NC_STRING(nc_string_from_cstr(ctx.tenant_id)));
            nc_map_set(claims, nc_string_from_cstr("authenticated"),
                       NC_BOOL(true));
            return NC_MAP(claims);
        }

        /* ── Cryptographic Hashing ─────────────────────────── */

        if (strcmp(name->chars, "hash_sha256") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcString *s = nc_value_to_string(arg);
            NcValue result = nc_stdlib_hash_sha256(s->chars);
            nc_string_free(s);
            return result;
        }
        if (strcmp(name->chars, "hash_password") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_hash_password(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "verify_password") == 0 && node->as.call.arg_count == 2) {
            NcValue pw = eval_expr(interp, node->as.call.args[0], scope);
            NcValue stored = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(pw) && IS_STRING(stored))
                return nc_stdlib_verify_password(AS_STRING(pw)->chars, AS_STRING(stored)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "hash_hmac") == 0 && node->as.call.arg_count == 2) {
            NcValue data = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(data) && IS_STRING(key_val))
                return nc_stdlib_hash_hmac(AS_STRING(data)->chars, AS_STRING(key_val)->chars);
            return NC_NONE();
        }

        /* ── Request Header Access ─────────────────────────── */

        if (strcmp(name->chars, "request_header") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_request_header(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "request_ip") == 0 && node->as.call.arg_count == 0) {
            NcRequestContext *ctx = nc_request_ctx_get();
            if (ctx) return NC_STRING(nc_string_from_cstr(ctx->client_ip));
            return NC_NONE();
        }
        if (strcmp(name->chars, "request_method") == 0 && node->as.call.arg_count == 0) {
            NcRequestContext *ctx = nc_request_ctx_get();
            if (ctx) return NC_STRING(nc_string_from_cstr(ctx->method));
            return NC_NONE();
        }
        if (strcmp(name->chars, "request_path") == 0 && node->as.call.arg_count == 0) {
            NcRequestContext *ctx = nc_request_ctx_get();
            if (ctx) return NC_STRING(nc_string_from_cstr(ctx->path));
            return NC_NONE();
        }
        if (strcmp(name->chars, "request_headers") == 0 && node->as.call.arg_count == 0) {
            NcRequestContext *ctx = nc_request_ctx_get();
            if (!ctx) return NC_NONE();
            NcMap *hdrs = nc_map_new();
            for (int hi = 0; hi < ctx->header_count; hi++)
                nc_map_set(hdrs, nc_string_from_cstr(ctx->headers[hi][0]),
                           NC_STRING(nc_string_from_cstr(ctx->headers[hi][1])));
            return NC_MAP(hdrs);
        }

        /* ── Session Management ────────────────────────────── */

        if (strcmp(name->chars, "session_create") == 0 && node->as.call.arg_count == 0)
            return nc_stdlib_session_create();
        if (strcmp(name->chars, "session_set") == 0 && node->as.call.arg_count == 3) {
            NcValue sid = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_v = eval_expr(interp, node->as.call.args[1], scope);
            NcValue val = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_STRING(sid) && IS_STRING(key_v))
                return nc_stdlib_session_set(AS_STRING(sid)->chars,
                    AS_STRING(key_v)->chars, val);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "session_get") == 0 && node->as.call.arg_count == 2) {
            NcValue sid = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_v = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(sid) && IS_STRING(key_v))
                return nc_stdlib_session_get(AS_STRING(sid)->chars,
                    AS_STRING(key_v)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "session_destroy") == 0 && node->as.call.arg_count == 1) {
            NcValue sid = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(sid))
                return nc_stdlib_session_destroy(AS_STRING(sid)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "session_exists") == 0 && node->as.call.arg_count == 1) {
            NcValue sid = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(sid))
                return nc_stdlib_session_exists(AS_STRING(sid)->chars);
            return NC_BOOL(false);
        }

        /* ── Feature Flags ─────────────────────────────────── */

        if (strcmp(name->chars, "feature") == 0 && node->as.call.arg_count >= 1) {
            NcValue flag = eval_expr(interp, node->as.call.args[0], scope);
            const char *tenant = NULL;
            if (node->as.call.arg_count >= 2) {
                NcValue t = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(t)) tenant = AS_STRING(t)->chars;
            }
            if (IS_STRING(flag))
                return NC_BOOL(nc_ff_is_enabled(AS_STRING(flag)->chars, tenant));
            return NC_BOOL(false);
        }

        /* ── Circuit Breaker (for downstream calls) ────────── */

        if (strcmp(name->chars, "circuit_open") == 0 && node->as.call.arg_count == 1) {
            NcValue name_v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(name_v))
                return NC_BOOL(!nc_cb_allow(AS_STRING(name_v)->chars));
            return NC_BOOL(false);
        }

        if ((strcmp(name->chars, "cache") == 0) && node->as.call.arg_count == 2) {
            NcValue key = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(key)) return nc_stdlib_cache_set(AS_STRING(key)->chars, val);
            return val;
        }
        if ((strcmp(name->chars, "cached") == 0 || strcmp(name->chars, "cache_get") == 0)
            && node->as.call.arg_count == 1) {
            NcValue key = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(key)) return nc_stdlib_cache_get(AS_STRING(key)->chars);
            return NC_NONE();
        }
        if ((strcmp(name->chars, "is_cached") == 0 || strcmp(name->chars, "cache_has") == 0)
            && node->as.call.arg_count == 1) {
            NcValue key = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(key)) return NC_BOOL(nc_stdlib_cache_has(AS_STRING(key)->chars));
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "find_similar") == 0 && node->as.call.arg_count >= 3) {
            NcValue query = eval_expr(interp, node->as.call.args[0], scope);
            NcValue vecs = eval_expr(interp, node->as.call.args[1], scope);
            NcValue docs = eval_expr(interp, node->as.call.args[2], scope);
            int k = 5;
            if (node->as.call.arg_count >= 4) {
                NcValue kv = eval_expr(interp, node->as.call.args[3], scope);
                if (IS_INT(kv)) k = (int)AS_INT(kv);
            }
            if (IS_LIST(query) && IS_LIST(vecs) && IS_LIST(docs))
                return nc_stdlib_find_similar(AS_LIST(query), AS_LIST(vecs), AS_LIST(docs), k);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "chunk") == 0 && node->as.call.arg_count >= 2) {
            NcValue text = eval_expr(interp, node->as.call.args[0], scope);
            NcValue size = eval_expr(interp, node->as.call.args[1], scope);
            int overlap = 0;
            if (node->as.call.arg_count >= 3) {
                NcValue ov = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_INT(ov)) overlap = (int)AS_INT(ov);
            }
            if (IS_STRING(text) && IS_INT(size))
                return nc_stdlib_chunk(AS_STRING(text), (int)AS_INT(size), overlap);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "top_k") == 0 && node->as.call.arg_count == 2) {
            NcValue items = eval_expr(interp, node->as.call.args[0], scope);
            NcValue k = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(items) && IS_INT(k))
                return nc_stdlib_top_k(AS_LIST(items), (int)AS_INT(k));
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "token_count") == 0 && node->as.call.arg_count == 1) {
            NcValue text = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(text))
                return NC_INT(nc_stdlib_token_count(AS_STRING(text)->chars));
            return NC_INT(0);
        }
        if (strcmp(name->chars, "http_request") == 0 && node->as.call.arg_count >= 2) {
            NcValue method = eval_expr(interp, node->as.call.args[0], scope);
            NcValue url = eval_expr(interp, node->as.call.args[1], scope);
            NcMap *hdrs = NULL;
            NcValue body = NC_NONE();
            if (node->as.call.arg_count >= 3) {
                NcValue h = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_MAP(h)) hdrs = AS_MAP(h);
            }
            if (node->as.call.arg_count >= 4)
                body = eval_expr(interp, node->as.call.args[3], scope);
            if (IS_STRING(method) && IS_STRING(url))
                return nc_stdlib_http_request(AS_STRING(method)->chars,
                                              AS_STRING(url)->chars, hdrs, body);
            return NC_NONE();
        }
        if (strcmp(name->chars, "http_get") == 0 && node->as.call.arg_count >= 1) {
            NcValue url = eval_expr(interp, node->as.call.args[0], scope);
            NcMap *hdrs = NULL;
            if (node->as.call.arg_count >= 2) {
                NcValue h = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_MAP(h)) hdrs = AS_MAP(h);
            }
            if (IS_STRING(url))
                return nc_stdlib_http_request("GET", AS_STRING(url)->chars, hdrs, NC_NONE());
            return NC_NONE();
        }
        if (strcmp(name->chars, "http_post") == 0 && node->as.call.arg_count >= 2) {
            NcValue url = eval_expr(interp, node->as.call.args[0], scope);
            NcValue body = eval_expr(interp, node->as.call.args[1], scope);
            NcMap *hdrs = NULL;
            if (node->as.call.arg_count >= 3) {
                NcValue h = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_MAP(h)) hdrs = AS_MAP(h);
            }
            if (IS_STRING(url))
                return nc_stdlib_http_request("POST", AS_STRING(url)->chars, hdrs, body);
            return NC_NONE();
        }
        /* ── Time functions ─────────────────────────── */
        if (strcmp(name->chars, "time_ms") == 0 && node->as.call.arg_count == 0)
            return nc_stdlib_time_ms();
        if (strcmp(name->chars, "time_format") == 0 && node->as.call.arg_count >= 1) {
            NcValue ts = eval_expr(interp, node->as.call.args[0], scope);
            const char *fmt = NULL;
            if (node->as.call.arg_count >= 2) {
                NcValue fv = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(fv)) fmt = AS_STRING(fv)->chars;
            }
            double t = IS_FLOAT(ts) ? AS_FLOAT(ts) : IS_INT(ts) ? (double)AS_INT(ts) : 0;
            return nc_stdlib_time_format(t, fmt);
        }
        if (strcmp(name->chars, "time_iso") == 0) {
            double t = 0;
            if (node->as.call.arg_count >= 1) {
                NcValue ts = eval_expr(interp, node->as.call.args[0], scope);
                t = IS_FLOAT(ts) ? AS_FLOAT(ts) : IS_INT(ts) ? (double)AS_INT(ts) : 0;
            } else {
                t = (double)time(NULL);
            }
            return nc_stdlib_time_iso(t);
        }
        /* ── Data format parsers ───────────────────── */
        if (strcmp(name->chars, "yaml_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_yaml_parse(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "xml_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_xml_parse(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "csv_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_csv_parse(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "toml_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_toml_parse(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "ini_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_ini_parse(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        /* ── List helpers ──────────────────────────── */
        if (strcmp(name->chars, "append") == 0 && node->as.call.arg_count == 2) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            NcValue val = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(list)) return nc_stdlib_list_append(AS_LIST(list), val);
            return list;
        }
        if (strcmp(name->chars, "ws_connect") == 0 && node->as.call.arg_count == 1) {
            NcValue url = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(url)) {
                nc_socket_t fd = nc_ws_client_connect(AS_STRING(url)->chars);
                if (fd == NC_INVALID_SOCKET) {
                    NcMap *err = nc_map_new();
                    nc_map_set(err, nc_string_from_cstr("error"),
                        NC_STRING(nc_string_from_cstr("WebSocket connection failed")));
                    return NC_MAP(err);
                }
                NcMap *conn = nc_map_new();
                nc_map_set(conn, nc_string_from_cstr("_ws_fd"), NC_INT((int64_t)fd));
                nc_map_set(conn, nc_string_from_cstr("connected"), NC_BOOL(true));
                nc_map_set(conn, nc_string_from_cstr("url"), url);
                return NC_MAP(conn);
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "ws_send") == 0 && node->as.call.arg_count == 2) {
            NcValue conn = eval_expr(interp, node->as.call.args[0], scope);
            NcValue msg = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(conn) && IS_STRING(msg)) {
                NcString *fk = nc_string_from_cstr("_ws_fd");
                NcValue fv = nc_map_get(AS_MAP(conn), fk);
                nc_string_free(fk);
                if (IS_INT(fv)) {
                    nc_ws_client_send((nc_socket_t)AS_INT(fv), AS_STRING(msg)->chars);
                    return NC_BOOL(true);
                }
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "ws_receive") == 0 && node->as.call.arg_count == 1) {
            NcValue conn = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_MAP(conn)) {
                NcString *fk = nc_string_from_cstr("_ws_fd");
                NcValue fv = nc_map_get(AS_MAP(conn), fk);
                nc_string_free(fk);
                if (IS_INT(fv)) {
                    char buf[8192];
                    int n = nc_ws_read((nc_socket_t)AS_INT(fv), buf, sizeof(buf) - 1);
                    if (n > 0) {
                        buf[n] = '\0';
                        NcValue parsed = nc_json_parse(buf);
                        if (!IS_NONE(parsed)) return parsed;
                        return NC_STRING(nc_string_from_cstr(buf));
                    }
                }
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "ws_close") == 0 && node->as.call.arg_count == 1) {
            NcValue conn = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_MAP(conn)) {
                NcString *fk = nc_string_from_cstr("_ws_fd");
                NcValue fv = nc_map_get(AS_MAP(conn), fk);
                nc_string_free(fk);
                if (IS_INT(fv)) nc_ws_client_close((nc_socket_t)AS_INT(fv));
            }
            return NC_BOOL(true);
        }
        if (strcmp(name->chars, "memory_new") == 0) {
            int max = 50;
            if (node->as.call.arg_count >= 1) {
                NcValue a = eval_expr(interp, node->as.call.args[0], scope);
                if (IS_INT(a)) max = (int)AS_INT(a);
            }
            return nc_stdlib_memory_new(max);
        }
        if (strcmp(name->chars, "memory_add") == 0 && node->as.call.arg_count == 3) {
            NcValue mem = eval_expr(interp, node->as.call.args[0], scope);
            NcValue role = eval_expr(interp, node->as.call.args[1], scope);
            NcValue content = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_STRING(role) && IS_STRING(content))
                return nc_stdlib_memory_add(mem, AS_STRING(role)->chars, AS_STRING(content)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "memory_get") == 0 && node->as.call.arg_count == 1) {
            NcValue mem = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_memory_get(mem);
        }
        if (strcmp(name->chars, "memory_clear") == 0 && node->as.call.arg_count == 1) {
            NcValue mem = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_memory_clear(mem);
        }
        if (strcmp(name->chars, "memory_summary") == 0 && node->as.call.arg_count == 1) {
            NcValue mem = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_memory_summary(mem);
        }
        if (strcmp(name->chars, "memory_save") == 0 && node->as.call.arg_count == 2) {
            NcValue mem = eval_expr(interp, node->as.call.args[0], scope);
            NcValue path = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(path)) return nc_stdlib_memory_save(mem, AS_STRING(path)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "memory_load") == 0 && node->as.call.arg_count >= 1 && node->as.call.arg_count <= 2) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            int max_turns = 0;
            if (node->as.call.arg_count == 2) {
                NcValue maxv = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_INT(maxv)) max_turns = (int)AS_INT(maxv);
            }
            if (IS_STRING(path)) return nc_stdlib_memory_load(AS_STRING(path)->chars, max_turns);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "memory_store") == 0 && node->as.call.arg_count >= 3 && node->as.call.arg_count <= 5) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue kind = eval_expr(interp, node->as.call.args[1], scope);
            NcValue content = eval_expr(interp, node->as.call.args[2], scope);
            NcValue metadata = node->as.call.arg_count >= 4 ? eval_expr(interp, node->as.call.args[3], scope) : NC_NONE();
            double reward = 0.0;
            if (node->as.call.arg_count == 5) {
                NcValue rewardv = eval_expr(interp, node->as.call.args[4], scope);
                if (IS_FLOAT(rewardv)) reward = AS_FLOAT(rewardv);
                else if (IS_INT(rewardv)) reward = (double)AS_INT(rewardv);
            }
            if (IS_STRING(path) && IS_STRING(kind) && IS_STRING(content)) {
                return nc_stdlib_memory_store(AS_STRING(path)->chars, AS_STRING(kind)->chars,
                                              AS_STRING(content)->chars, metadata, reward);
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "memory_search") == 0 && node->as.call.arg_count >= 2 && node->as.call.arg_count <= 3) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue query = eval_expr(interp, node->as.call.args[1], scope);
            int top_k = 5;
            if (node->as.call.arg_count == 3) {
                NcValue kv = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_INT(kv)) top_k = (int)AS_INT(kv);
            }
            if (IS_STRING(path) && IS_STRING(query)) {
                return nc_stdlib_memory_search(AS_STRING(path)->chars, AS_STRING(query)->chars, top_k);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "memory_context") == 0 && node->as.call.arg_count >= 2 && node->as.call.arg_count <= 3) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue query = eval_expr(interp, node->as.call.args[1], scope);
            int top_k = 5;
            if (node->as.call.arg_count == 3) {
                NcValue kv = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_INT(kv)) top_k = (int)AS_INT(kv);
            }
            if (IS_STRING(path) && IS_STRING(query)) {
                return nc_stdlib_memory_context(AS_STRING(path)->chars, AS_STRING(query)->chars, top_k);
            }
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "memory_reflect") == 0 && node->as.call.arg_count == 6) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue task = eval_expr(interp, node->as.call.args[1], scope);
            NcValue worked = eval_expr(interp, node->as.call.args[2], scope);
            NcValue failed = eval_expr(interp, node->as.call.args[3], scope);
            NcValue confidence = eval_expr(interp, node->as.call.args[4], scope);
            NcValue next_action = eval_expr(interp, node->as.call.args[5], scope);
            double conf = 0.0;
            if (IS_FLOAT(confidence)) conf = AS_FLOAT(confidence);
            else if (IS_INT(confidence)) conf = (double)AS_INT(confidence);
            if (IS_STRING(path) && IS_STRING(task) && IS_STRING(worked) && IS_STRING(failed) && IS_STRING(next_action)) {
                return nc_stdlib_memory_reflect(AS_STRING(path)->chars, AS_STRING(task)->chars,
                                                AS_STRING(worked)->chars, AS_STRING(failed)->chars,
                                                conf, AS_STRING(next_action)->chars);
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "policy_update") == 0 && node->as.call.arg_count == 3) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue action = eval_expr(interp, node->as.call.args[1], scope);
            NcValue reward = eval_expr(interp, node->as.call.args[2], scope);
            double r = 0.0;
            if (IS_FLOAT(reward)) r = AS_FLOAT(reward);
            else if (IS_INT(reward)) r = (double)AS_INT(reward);
            if (IS_STRING(path) && IS_STRING(action)) {
                return nc_stdlib_policy_update(AS_STRING(path)->chars, AS_STRING(action)->chars, r);
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "policy_choose") == 0 && node->as.call.arg_count >= 2 && node->as.call.arg_count <= 3) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue actions = eval_expr(interp, node->as.call.args[1], scope);
            double epsilon = 0.0;
            if (node->as.call.arg_count == 3) {
                NcValue epsv = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_FLOAT(epsv)) epsilon = AS_FLOAT(epsv);
                else if (IS_INT(epsv)) epsilon = (double)AS_INT(epsv);
            }
            if (IS_STRING(path) && IS_LIST(actions)) {
                return nc_stdlib_policy_choose(AS_STRING(path)->chars, AS_LIST(actions), epsilon);
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "policy_stats") == 0 && node->as.call.arg_count == 1) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(path)) return nc_stdlib_policy_stats(AS_STRING(path)->chars);
            return NC_MAP(nc_map_new());
        }
        if (strcmp(name->chars, "validate") == 0 && node->as.call.arg_count == 2) {
            NcValue resp = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fields = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(fields)) return nc_stdlib_validate(resp, AS_LIST(fields));
            return NC_NONE();
        }
        if (strcmp(name->chars, "ai_with_fallback") == 0 && node->as.call.arg_count == 3) {
            NcValue prompt = eval_expr(interp, node->as.call.args[0], scope);
            NcValue ctx = eval_expr(interp, node->as.call.args[1], scope);
            NcValue models = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_STRING(prompt) && IS_LIST(models)) {
                NcMap *context = IS_MAP(ctx) ? AS_MAP(ctx) : nc_map_new();
                return nc_stdlib_ai_with_fallback(AS_STRING(prompt)->chars, context, AS_LIST(models));
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "parallel_ask") == 0 && node->as.call.arg_count >= 1) {
            NcValue prompts_val = eval_expr(interp, node->as.call.args[0], scope);
            if (!IS_LIST(prompts_val)) return NC_LIST(nc_list_new());
            NcList *prompts = AS_LIST(prompts_val);
            NcMap *context = NULL;
            if (node->as.call.arg_count >= 2) {
                NcValue ctx = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_MAP(ctx)) context = AS_MAP(ctx);
            }
            if (!context) context = nc_map_new();
            int n = prompts->count;
            if (n == 0) return NC_LIST(nc_list_new());

            NcValue *results = calloc(n, sizeof(NcValue));
            NcParallelAICtx *ctxs = calloc(n, sizeof(NcParallelAICtx));
            nc_thread_t *threads = calloc(n, sizeof(nc_thread_t));
            if (!results || !ctxs || !threads) {
                free(results); free(ctxs); free(threads);
                return NC_LIST(nc_list_new());
            }

            for (int i = 0; i < n; i++) {
                results[i] = NC_NONE();
                ctxs[i].prompt = NULL;
                ctxs[i].context = context;
                ctxs[i].out = &results[i];
                if (IS_STRING(prompts->items[i]))
                    ctxs[i].prompt = AS_STRING(prompts->items[i])->chars;
            }

            for (int i = 0; i < n; i++) {
                if (ctxs[i].prompt)
                    nc_thread_create(&threads[i], (nc_thread_func_t)nc_parallel_ai_worker, &ctxs[i]);
            }
            for (int i = 0; i < n; i++) {
                if (ctxs[i].prompt) nc_thread_join(threads[i]);
            }

            NcList *result_list = nc_list_new();
            for (int i = 0; i < n; i++) nc_list_push(result_list, results[i]);
            free(results); free(ctxs); free(threads);
            return NC_LIST(result_list);
        }
        if (strcmp(name->chars, "load_model") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) return nc_stdlib_load_model(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "predict") == 0 && node->as.call.arg_count == 2) {
            NcValue model = eval_expr(interp, node->as.call.args[0], scope);
            NcValue features = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(features))
                return nc_stdlib_predict(model, AS_LIST(features));
            return NC_NONE();
        }
        if (strcmp(name->chars, "unload_model") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_INT(arg)) nc_stdlib_unload_model((int)AS_INT(arg));
            else if (IS_MAP(arg)) {
                NcString *hk = nc_string_from_cstr("_handle");
                NcValue hv = nc_map_get(AS_MAP(arg), hk);
                nc_string_free(hk);
                if (IS_INT(hv)) nc_stdlib_unload_model((int)AS_INT(hv));
            }
            return NC_NONE();
        }

        /* ── Math functions ────────────────────────────── */
        if (strcmp(name->chars, "pow") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            return NC_FLOAT(pow(as_num(a), as_num(b)));
        }
        if (strcmp(name->chars, "round") == 0 && node->as.call.arg_count == 2) {
            double val = as_num(eval_expr(interp, node->as.call.args[0], scope));
            int decimals = (int)as_num(eval_expr(interp, node->as.call.args[1], scope));
            double factor = pow(10.0, decimals);
            return NC_FLOAT(round(val * factor) / factor);
        }
        if (strcmp(name->chars, "round") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_INT((int64_t)round(as_num(a)));
        }
        if (strcmp(name->chars, "ceil") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_INT((int64_t)ceil(as_num(a)));
        }
        if (strcmp(name->chars, "floor") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_INT((int64_t)floor(as_num(a)));
        }
        if (strcmp(name->chars, "min") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                NcList *l = AS_LIST(arg);
                if (l->count == 0) return NC_NONE();
                double best = as_num(l->items[0]); int bi = 0;
                for (int i = 1; i < l->count; i++) {
                    double v = as_num(l->items[i]);
                    if (v < best) { best = v; bi = i; }
                }
                return l->items[bi];
            }
            return NC_FLOAT(as_num(arg));
        }
        if (strcmp(name->chars, "min") == 0 && node->as.call.arg_count == 2) {
            double a = as_num(eval_expr(interp, node->as.call.args[0], scope));
            double b = as_num(eval_expr(interp, node->as.call.args[1], scope));
            return (a < b) ? NC_FLOAT(a) : NC_FLOAT(b);
        }
        if (strcmp(name->chars, "max") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                NcList *l = AS_LIST(arg);
                if (l->count == 0) return NC_NONE();
                double best = as_num(l->items[0]); int bi = 0;
                for (int i = 1; i < l->count; i++) {
                    double v = as_num(l->items[i]);
                    if (v > best) { best = v; bi = i; }
                }
                return l->items[bi];
            }
            return NC_FLOAT(as_num(arg));
        }
        if (strcmp(name->chars, "max") == 0 && node->as.call.arg_count == 2) {
            double a = as_num(eval_expr(interp, node->as.call.args[0], scope));
            double b = as_num(eval_expr(interp, node->as.call.args[1], scope));
            return (a > b) ? NC_FLOAT(a) : NC_FLOAT(b);
        }
        if (strcmp(name->chars, "float") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return NC_FLOAT(as_num(a));
        }

        /* ── List functions ───────────────────────────── */
        if (strcmp(name->chars, "sum") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                NcList *l = AS_LIST(arg); double s = 0;
                for (int i = 0; i < l->count; i++) s += as_num(l->items[i]);
                return (s == (int64_t)s) ? NC_INT((int64_t)s) : NC_FLOAT(s);
            }
            return NC_INT(0);
        }
        if (strcmp(name->chars, "average") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg) && AS_LIST(arg)->count > 0) {
                NcList *l = AS_LIST(arg); double s = 0;
                for (int i = 0; i < l->count; i++) s += as_num(l->items[i]);
                return NC_FLOAT(s / l->count);
            }
            return NC_INT(0);
        }
        if (strcmp(name->chars, "first") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg) && AS_LIST(arg)->count > 0) return AS_LIST(arg)->items[0];
            return NC_NONE();
        }
        if (strcmp(name->chars, "last") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg) && AS_LIST(arg)->count > 0) return AS_LIST(arg)->items[AS_LIST(arg)->count - 1];
            return NC_NONE();
        }
        if (strcmp(name->chars, "unique") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                NcList *src = AS_LIST(arg);
                NcList *out = nc_list_new();
                for (int i = 0; i < src->count; i++) {
                    bool found = false;
                    for (int j = 0; j < out->count; j++) {
                        NcString *a = nc_value_to_string(src->items[i]);
                        NcString *b = nc_value_to_string(out->items[j]);
                        if (strcmp(a->chars, b->chars) == 0) found = true;
                        nc_string_free(a); nc_string_free(b);
                        if (found) break;
                    }
                    if (!found) nc_list_push(out, src->items[i]);
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "flatten") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                NcList *src = AS_LIST(arg);
                NcList *out = nc_list_new();
                for (int i = 0; i < src->count; i++) {
                    if (IS_LIST(src->items[i])) {
                        NcList *inner = AS_LIST(src->items[i]);
                        for (int j = 0; j < inner->count; j++)
                            nc_list_push(out, inner->items[j]);
                    } else {
                        nc_list_push(out, src->items[i]);
                    }
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "index_of") == 0 && node->as.call.arg_count == 2) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            NcValue item = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(list)) {
                NcString *is = nc_value_to_string(item);
                for (int i = 0; i < AS_LIST(list)->count; i++) {
                    NcString *js = nc_value_to_string(AS_LIST(list)->items[i]);
                    bool eq = (strcmp(is->chars, js->chars) == 0);
                    nc_string_free(js);
                    if (eq) { nc_string_free(is); return NC_INT(i); }
                }
                nc_string_free(is);
            }
            return NC_INT(-1);
        }
        if (strcmp(name->chars, "count") == 0 && node->as.call.arg_count == 2) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            NcValue item = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(list)) {
                int c = 0;
                NcString *is = nc_value_to_string(item);
                for (int i = 0; i < AS_LIST(list)->count; i++) {
                    NcString *js = nc_value_to_string(AS_LIST(list)->items[i]);
                    if (strcmp(is->chars, js->chars) == 0) c++;
                    nc_string_free(js);
                }
                nc_string_free(is);
                return NC_INT(c);
            }
            return NC_INT(0);
        }
        if (strcmp(name->chars, "slice") == 0 && node->as.call.arg_count >= 2) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(list) && AS_STRING(list)->length > 0 &&
                (AS_STRING(list)->chars[0] == '[' || AS_STRING(list)->chars[0] == '{')) {
                list = nc_json_parse(AS_STRING(list)->chars);
            }
            if (IS_LIST(list)) {
                NcList *src = AS_LIST(list);
                int start = (int)as_num(eval_expr(interp, node->as.call.args[1], scope));
                int end = node->as.call.arg_count >= 3
                    ? (int)as_num(eval_expr(interp, node->as.call.args[2], scope))
                    : src->count;
                if (start < 0) start = src->count + start;
                if (end < 0) end = src->count + end;
                if (start < 0) start = 0;
                if (end > src->count) end = src->count;
                if (start > end) start = end;
                NcList *out = nc_list_new();
                for (int i = start; i < end; i++)
                    nc_list_push(out, src->items[i]);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "any") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                for (int i = 0; i < AS_LIST(arg)->count; i++)
                    if (nc_truthy(AS_LIST(arg)->items[i])) return NC_BOOL(true);
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "all") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(arg)) {
                for (int i = 0; i < AS_LIST(arg)->count; i++)
                    if (!nc_truthy(AS_LIST(arg)->items[i])) return NC_BOOL(false);
                return NC_BOOL(true);
            }
            return NC_BOOL(false);
        }

        /* ── Type check functions ─────────────────────── */
        if (strcmp(name->chars, "is_text") == 0 && node->as.call.arg_count == 1) {
            return NC_BOOL(IS_STRING(eval_expr(interp, node->as.call.args[0], scope)));
        }
        if (strcmp(name->chars, "is_number") == 0 && node->as.call.arg_count == 1) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            return NC_BOOL(IS_INT(v) || IS_FLOAT(v));
        }
        if (strcmp(name->chars, "is_list") == 0 && node->as.call.arg_count == 1) {
            return NC_BOOL(IS_LIST(eval_expr(interp, node->as.call.args[0], scope)));
        }
        if (strcmp(name->chars, "is_record") == 0 && node->as.call.arg_count == 1) {
            return NC_BOOL(IS_MAP(eval_expr(interp, node->as.call.args[0], scope)));
        }
        if (strcmp(name->chars, "is_none") == 0 && node->as.call.arg_count == 1) {
            return NC_BOOL(IS_NONE(eval_expr(interp, node->as.call.args[0], scope)));
        }

        /* ── Map functions ────────────────────────────── */
        if (strcmp(name->chars, "has_key") == 0 && node->as.call.arg_count == 2) {
            NcValue m = eval_expr(interp, node->as.call.args[0], scope);
            NcValue k = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(m) && IS_STRING(k))
                return NC_BOOL(nc_map_has(AS_MAP(m), AS_STRING(k)));
            return NC_BOOL(false);
        }

        /* ── items(record) — returns [[key, val], ...] ───── */
        if (strcmp(name->chars, "items") == 0 && node->as.call.arg_count == 1) {
            NcValue m = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_MAP(m)) {
                NcList *out = nc_list_new();
                NcMap *mp = AS_MAP(m);
                for (int i = 0; i < mp->count; i++) {
                    NcList *pair = nc_list_new();
                    nc_list_push(pair, NC_STRING(nc_string_ref(mp->keys[i])));
                    nc_list_push(pair, mp->values[i]);
                    nc_list_push(out, NC_LIST(pair));
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── merge(record1, record2) — merge two records ─── */
        if (strcmp(name->chars, "merge") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_MAP(a) && IS_MAP(b)) {
                NcMap *out = nc_map_new();
                NcMap *ma = AS_MAP(a), *mb = AS_MAP(b);
                for (int i = 0; i < ma->count; i++)
                    nc_map_set(out, nc_string_ref(ma->keys[i]), ma->values[i]);
                for (int i = 0; i < mb->count; i++)
                    nc_map_set(out, nc_string_ref(mb->keys[i]), mb->values[i]);
                return NC_MAP(out);
            }
            return NC_NONE();
        }

        /* ── find(list, key, value) — first matching record ─ */
        if (strcmp(name->chars, "find") == 0 && node->as.call.arg_count == 3) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key = eval_expr(interp, node->as.call.args[1], scope);
            NcValue val = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_LIST(lst) && IS_STRING(key)) {
                NcList *l = AS_LIST(lst);
                for (int i = 0; i < l->count; i++) {
                    if (IS_MAP(l->items[i])) {
                        NcValue got = nc_map_get(AS_MAP(l->items[i]), AS_STRING(key));
                        if (got.type == val.type) {
                            bool eq = false;
                            if (IS_INT(got) && AS_INT(got) == AS_INT(val)) eq = true;
                            else if (IS_FLOAT(got) && AS_FLOAT(got) == AS_FLOAT(val)) eq = true;
                            else if (IS_STRING(got) && IS_STRING(val) &&
                                     nc_string_equal(AS_STRING(got), AS_STRING(val))) eq = true;
                            else if (IS_BOOL(got) && AS_BOOL(got) == AS_BOOL(val)) eq = true;
                            if (eq) return l->items[i];
                        }
                    }
                }
            }
            return NC_NONE();
        }

        /* ── group_by(list, field) — group records by field ── */
        if (strcmp(name->chars, "group_by") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fld = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_STRING(fld)) {
                NcMap *out = nc_map_new();
                NcList *l = AS_LIST(lst);
                for (int i = 0; i < l->count; i++) {
                    if (IS_MAP(l->items[i])) {
                        NcValue v = nc_map_get(AS_MAP(l->items[i]), AS_STRING(fld));
                        NcString *k = nc_value_to_string(v);
                        NcValue existing = nc_map_get(out, k);
                        NcList *grp;
                        if (IS_LIST(existing)) { grp = AS_LIST(existing); }
                        else { grp = nc_list_new(); nc_map_set(out, nc_string_ref(k), NC_LIST(grp)); }
                        nc_list_push(grp, l->items[i]);
                        nc_string_free(k);
                    }
                }
                return NC_MAP(out);
            }
            return NC_MAP(nc_map_new());
        }

        /* ── take(list, n) — first n elements ───────────── */
        if (strcmp(name->chars, "take") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_INT(n)) {
                NcList *out = nc_list_new();
                int cnt = (int)AS_INT(n);
                if (cnt > AS_LIST(lst)->count) cnt = AS_LIST(lst)->count;
                for (int i = 0; i < cnt; i++)
                    nc_list_push(out, AS_LIST(lst)->items[i]);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── drop(list, n) — skip first n elements ──────── */
        if (strcmp(name->chars, "drop") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_INT(n)) {
                NcList *out = nc_list_new();
                int start = (int)AS_INT(n);
                if (start < 0) start = 0;
                for (int i = start; i < AS_LIST(lst)->count; i++)
                    nc_list_push(out, AS_LIST(lst)->items[i]);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── compact(list) — remove none values ─────────── */
        if (strcmp(name->chars, "compact") == 0 && node->as.call.arg_count == 1) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(lst)) {
                NcList *out = nc_list_new();
                for (int i = 0; i < AS_LIST(lst)->count; i++)
                    if (!IS_NONE(AS_LIST(lst)->items[i]))
                        nc_list_push(out, AS_LIST(lst)->items[i]);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── reduce(list, op) — fold with operation ──────── */
        if (strcmp(name->chars, "reduce") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue op  = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_STRING(op) && AS_LIST(lst)->count > 0) {
                NcList *l = AS_LIST(lst);
                const char *o = AS_STRING(op)->chars;
                NcValue acc = l->items[0];
                for (int i = 1; i < l->count; i++) {
                    NcValue cur = l->items[i];
                    if (strcmp(o, "+") == 0 || strcmp(o, "add") == 0) {
                        if (IS_INT(acc) && IS_INT(cur)) acc = NC_INT(AS_INT(acc) + AS_INT(cur));
                        else if (IS_NUMBER(acc) && IS_NUMBER(cur)) {
                            double a = IS_INT(acc) ? (double)AS_INT(acc) : AS_FLOAT(acc);
                            double b = IS_INT(cur) ? (double)AS_INT(cur) : AS_FLOAT(cur);
                            acc = NC_FLOAT(a + b);
                        } else if (IS_STRING(acc) && IS_STRING(cur))
                            acc = NC_STRING(nc_string_concat(AS_STRING(acc), AS_STRING(cur)));
                    } else if (strcmp(o, "*") == 0 || strcmp(o, "multiply") == 0) {
                        if (IS_INT(acc) && IS_INT(cur)) acc = NC_INT(AS_INT(acc) * AS_INT(cur));
                        else if (IS_NUMBER(acc) && IS_NUMBER(cur)) {
                            double a = IS_INT(acc) ? (double)AS_INT(acc) : AS_FLOAT(acc);
                            double b = IS_INT(cur) ? (double)AS_INT(cur) : AS_FLOAT(cur);
                            acc = NC_FLOAT(a * b);
                        }
                    } else if (strcmp(o, "min") == 0) {
                        if (IS_INT(acc) && IS_INT(cur)) { if (AS_INT(cur) < AS_INT(acc)) acc = cur; }
                        else if (IS_NUMBER(acc) && IS_NUMBER(cur)) {
                            double a = IS_INT(acc) ? (double)AS_INT(acc) : AS_FLOAT(acc);
                            double b = IS_INT(cur) ? (double)AS_INT(cur) : AS_FLOAT(cur);
                            if (b < a) acc = cur;
                        }
                    } else if (strcmp(o, "max") == 0) {
                        if (IS_INT(acc) && IS_INT(cur)) { if (AS_INT(cur) > AS_INT(acc)) acc = cur; }
                        else if (IS_NUMBER(acc) && IS_NUMBER(cur)) {
                            double a = IS_INT(acc) ? (double)AS_INT(acc) : AS_FLOAT(acc);
                            double b = IS_INT(cur) ? (double)AS_INT(cur) : AS_FLOAT(cur);
                            if (b > a) acc = cur;
                        }
                    } else if (strcmp(o, "join") == 0 || strcmp(o, "concat") == 0) {
                        NcString *sa = nc_value_to_string(acc);
                        NcString *sb = nc_value_to_string(cur);
                        acc = NC_STRING(nc_string_concat(sa, sb));
                    }
                }
                return acc;
            }
            return NC_NONE();
        }

        /* ── map(list, field) — alias for map_field ──────── */
        if (strcmp(name->chars, "map") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fld = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_STRING(fld)) {
                NcList *out = nc_list_new();
                NcList *l = AS_LIST(lst);
                for (int i = 0; i < l->count; i++) {
                    if (IS_MAP(l->items[i]))
                        nc_list_push(out, nc_map_get(AS_MAP(l->items[i]), AS_STRING(fld)));
                    else
                        nc_list_push(out, NC_NONE());
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── title_case(str) — Title Case String ─────────── */
        if (strcmp(name->chars, "title_case") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) {
                NcString *src = AS_STRING(s);
                char *buf = malloc(src->length + 1);
                bool next_upper = true;
                for (int i = 0; i < src->length; i++) {
                    if (src->chars[i] == ' ' || src->chars[i] == '\t') {
                        buf[i] = src->chars[i];
                        next_upper = true;
                    } else if (next_upper) {
                        buf[i] = (char)toupper((unsigned char)src->chars[i]);
                        next_upper = false;
                    } else {
                        buf[i] = (char)tolower((unsigned char)src->chars[i]);
                    }
                }
                buf[src->length] = '\0';
                NcString *r = nc_string_new(buf, src->length);
                free(buf);
                return NC_STRING(r);
            }
            return NC_NONE();
        }

        /* ── capitalize(str) — capitalize first letter ───── */
        if (strcmp(name->chars, "capitalize") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) {
                NcString *src = AS_STRING(s);
                if (src->length == 0) return NC_STRING(nc_string_from_cstr(""));
                char *buf = malloc(src->length + 1);
                buf[0] = (char)toupper((unsigned char)src->chars[0]);
                for (int i = 1; i < src->length; i++)
                    buf[i] = (char)tolower((unsigned char)src->chars[i]);
                buf[src->length] = '\0';
                NcString *r = nc_string_new(buf, src->length);
                free(buf);
                return NC_STRING(r);
            }
            return NC_NONE();
        }

        /* ── pad_left(str, width, fill) ──────────────────── */
        if (strcmp(name->chars, "pad_left") == 0 && node->as.call.arg_count >= 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue w = eval_expr(interp, node->as.call.args[1], scope);
            char fill = ' ';
            if (node->as.call.arg_count >= 3) {
                NcValue f = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_STRING(f) && AS_STRING(f)->length > 0) fill = AS_STRING(f)->chars[0];
            }
            if (IS_STRING(s) && IS_INT(w)) {
                int width = (int)AS_INT(w);
                NcString *src = AS_STRING(s);
                if (src->length >= width) return NC_STRING(nc_string_ref(src));
                int pad = width - src->length;
                char *buf = malloc(width + 1);
                for (int i = 0; i < pad; i++) buf[i] = fill;
                memcpy(buf + pad, src->chars, src->length);
                buf[width] = '\0';
                NcString *r = nc_string_new(buf, width);
                free(buf);
                return NC_STRING(r);
            }
            return NC_NONE();
        }

        /* ── pad_right(str, width, fill) ─────────────────── */
        if (strcmp(name->chars, "pad_right") == 0 && node->as.call.arg_count >= 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue w = eval_expr(interp, node->as.call.args[1], scope);
            char fill = ' ';
            if (node->as.call.arg_count >= 3) {
                NcValue f = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_STRING(f) && AS_STRING(f)->length > 0) fill = AS_STRING(f)->chars[0];
            }
            if (IS_STRING(s) && IS_INT(w)) {
                int width = (int)AS_INT(w);
                NcString *src = AS_STRING(s);
                if (src->length >= width) return NC_STRING(nc_string_ref(src));
                char *buf = malloc(width + 1);
                memcpy(buf, src->chars, src->length);
                for (int i = src->length; i < width; i++) buf[i] = fill;
                buf[width] = '\0';
                NcString *r = nc_string_new(buf, width);
                free(buf);
                return NC_STRING(r);
            }
            return NC_NONE();
        }

        /* ── char_at(str, index) — character at position ─── */
        if (strcmp(name->chars, "char_at") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue idx = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_INT(idx)) {
                int i = (int)AS_INT(idx);
                NcString *src = AS_STRING(s);
                if (i >= 0 && i < src->length) {
                    char buf[2] = { src->chars[i], '\0' };
                    return NC_STRING(nc_string_new(buf, 1));
                }
            }
            return NC_NONE();
        }

        /* ── repeat_string(str, n) — repeat n times ────── */
        if (strcmp(name->chars, "repeat_string") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_INT(n)) {
                int cnt = (int)AS_INT(n);
                NcString *src = AS_STRING(s);
                if (cnt <= 0) return NC_STRING(nc_string_from_cstr(""));
                int total = src->length * cnt;
                char *buf = malloc(total + 1);
                for (int i = 0; i < cnt; i++)
                    memcpy(buf + i * src->length, src->chars, src->length);
                buf[total] = '\0';
                NcString *r = nc_string_new(buf, total);
                free(buf);
                return NC_STRING(r);
            }
            return NC_NONE();
        }

        /* ── isinstance(val, type_name) — type check ─────── */
        if (strcmp(name->chars, "isinstance") == 0 && node->as.call.arg_count == 2) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            NcValue t = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(t)) {
                const char *tn = AS_STRING(t)->chars;
                bool match = false;
                if (strcmp(tn, "text") == 0 || strcmp(tn, "string") == 0) match = IS_STRING(v);
                else if (strcmp(tn, "number") == 0 || strcmp(tn, "int") == 0) match = IS_INT(v);
                else if (strcmp(tn, "float") == 0) match = IS_FLOAT(v);
                else if (strcmp(tn, "list") == 0) match = IS_LIST(v);
                else if (strcmp(tn, "record") == 0 || strcmp(tn, "map") == 0) match = IS_MAP(v);
                else if (strcmp(tn, "bool") == 0 || strcmp(tn, "yesno") == 0) match = IS_BOOL(v);
                else if (strcmp(tn, "none") == 0) match = IS_NONE(v);
                return NC_BOOL(match);
            }
            return NC_BOOL(false);
        }

        /* ── is_empty(val) — check if empty ──────────────── */
        if (strcmp(name->chars, "is_empty") == 0 && node->as.call.arg_count == 1) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(v)) return NC_BOOL(AS_STRING(v)->length == 0);
            if (IS_LIST(v)) return NC_BOOL(AS_LIST(v)->count == 0);
            if (IS_MAP(v)) return NC_BOOL(AS_MAP(v)->count == 0);
            if (IS_NONE(v)) return NC_BOOL(true);
            return NC_BOOL(false);
        }

        /* ── clamp(val, lo, hi) — clamp to range ─────────── */
        if (strcmp(name->chars, "clamp") == 0 && node->as.call.arg_count == 3) {
            NcValue v  = eval_expr(interp, node->as.call.args[0], scope);
            NcValue lo = eval_expr(interp, node->as.call.args[1], scope);
            NcValue hi = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_INT(v) && IS_INT(lo) && IS_INT(hi)) {
                int64_t x = AS_INT(v), a = AS_INT(lo), b = AS_INT(hi);
                if (x < a) return NC_INT(a);
                if (x > b) return NC_INT(b);
                return NC_INT(x);
            }
            if (IS_NUMBER(v) && IS_NUMBER(lo) && IS_NUMBER(hi)) {
                double x = IS_INT(v) ? (double)AS_INT(v) : AS_FLOAT(v);
                double a = IS_INT(lo) ? (double)AS_INT(lo) : AS_FLOAT(lo);
                double b = IS_INT(hi) ? (double)AS_INT(hi) : AS_FLOAT(hi);
                if (x < a) return NC_FLOAT(a);
                if (x > b) return NC_FLOAT(b);
                return NC_FLOAT(x);
            }
            return NC_NONE();
        }

        /* ── sign(val) — return -1, 0, or 1 ──────────────── */
        if (strcmp(name->chars, "sign") == 0 && node->as.call.arg_count == 1) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_INT(v)) {
                int64_t x = AS_INT(v);
                return NC_INT(x > 0 ? 1 : (x < 0 ? -1 : 0));
            }
            if (IS_FLOAT(v)) {
                double x = AS_FLOAT(v);
                return NC_INT(x > 0 ? 1 : (x < 0 ? -1 : 0));
            }
            return NC_INT(0);
        }

        /* ── lerp(a, b, t) — linear interpolation ────────── */
        if (strcmp(name->chars, "lerp") == 0 && node->as.call.arg_count == 3) {
            NcValue va = eval_expr(interp, node->as.call.args[0], scope);
            NcValue vb = eval_expr(interp, node->as.call.args[1], scope);
            NcValue vt = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_NUMBER(va) && IS_NUMBER(vb) && IS_NUMBER(vt)) {
                double a = IS_INT(va) ? (double)AS_INT(va) : AS_FLOAT(va);
                double b = IS_INT(vb) ? (double)AS_INT(vb) : AS_FLOAT(vb);
                double t = IS_INT(vt) ? (double)AS_INT(vt) : AS_FLOAT(vt);
                return NC_FLOAT(a + (b - a) * t);
            }
            return NC_NONE();
        }

        /* ── gcd(a, b) — greatest common divisor ──────────── */
        if (strcmp(name->chars, "gcd") == 0 && node->as.call.arg_count == 2) {
            NcValue va = eval_expr(interp, node->as.call.args[0], scope);
            NcValue vb = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_INT(va) && IS_INT(vb)) {
                int64_t a = AS_INT(va), b = AS_INT(vb);
                if (a < 0) a = -a;
                if (b < 0) b = -b;
                while (b) { int64_t t = b; b = a % b; a = t; }
                return NC_INT(a);
            }
            return NC_INT(0);
        }

        /* ── sorted(list) — non-mutating sort alias ──────── */
        if (strcmp(name->chars, "sorted") == 0 && node->as.call.arg_count == 1) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(lst)) {
                NcList *l = AS_LIST(lst);
                NcList *out = nc_list_new();
                for (int i = 0; i < l->count; i++) nc_list_push(out, l->items[i]);
                /* Simple insertion sort for correctness */
                for (int i = 1; i < out->count; i++) {
                    NcValue key = out->items[i];
                    int j = i - 1;
                    while (j >= 0) {
                        bool gt = false;
                        if (IS_INT(out->items[j]) && IS_INT(key))
                            gt = AS_INT(out->items[j]) > AS_INT(key);
                        else if (IS_FLOAT(out->items[j]) && IS_FLOAT(key))
                            gt = AS_FLOAT(out->items[j]) > AS_FLOAT(key);
                        else if (IS_NUMBER(out->items[j]) && IS_NUMBER(key)) {
                            double a = IS_INT(out->items[j]) ? (double)AS_INT(out->items[j]) : AS_FLOAT(out->items[j]);
                            double b = IS_INT(key) ? (double)AS_INT(key) : AS_FLOAT(key);
                            gt = a > b;
                        } else if (IS_STRING(out->items[j]) && IS_STRING(key))
                            gt = strcmp(AS_STRING(out->items[j])->chars, AS_STRING(key)->chars) > 0;
                        if (!gt) break;
                        out->items[j+1] = out->items[j];
                        j--;
                    }
                    out->items[j+1] = key;
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── reversed(list) — non-mutating reverse ────────── */
        if (strcmp(name->chars, "reversed") == 0 && node->as.call.arg_count == 1) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(lst)) {
                NcList *l = AS_LIST(lst);
                NcList *out = nc_list_new();
                for (int i = l->count - 1; i >= 0; i--) nc_list_push(out, l->items[i]);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── to_json(val) / from_json(str) — aliases ─────── */
        if (strcmp(name->chars, "to_json") == 0 && node->as.call.arg_count == 1) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            return NC_STRING(nc_value_to_string(v));
        }
        if (strcmp(name->chars, "from_json") == 0 && node->as.call.arg_count == 1) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(v)) return nc_json_parse(AS_STRING(v)->chars);
            return NC_NONE();
        }

        /* ── pluck(list, field) — extract field values ─────── */
        if (strcmp(name->chars, "pluck") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fld = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_STRING(fld)) {
                NcList *out = nc_list_new();
                NcList *l = AS_LIST(lst);
                for (int i = 0; i < l->count; i++) {
                    if (IS_MAP(l->items[i]))
                        nc_list_push(out, nc_map_get(AS_MAP(l->items[i]), AS_STRING(fld)));
                    else
                        nc_list_push(out, NC_NONE());
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── chunk(list, size) — split list into chunks ────── */
        if (strcmp(name->chars, "chunk_list") == 0 && node->as.call.arg_count == 2) {
            NcValue lst = eval_expr(interp, node->as.call.args[0], scope);
            NcValue sz  = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(lst) && IS_INT(sz) && AS_INT(sz) > 0) {
                NcList *out = nc_list_new();
                NcList *l = AS_LIST(lst);
                int csz = (int)AS_INT(sz);
                for (int i = 0; i < l->count; i += csz) {
                    NcList *c = nc_list_new();
                    for (int j = i; j < i + csz && j < l->count; j++)
                        nc_list_push(c, l->items[j]);
                    nc_list_push(out, NC_LIST(c));
                }
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── repeat_value(val, n) — list of n copies ────── */
        if (strcmp(name->chars, "repeat_value") == 0 && node->as.call.arg_count == 2) {
            NcValue v = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_INT(n)) {
                NcList *out = nc_list_new();
                for (int i = 0; i < (int)AS_INT(n); i++)
                    nc_list_push(out, v);
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ── dot_product(list1, list2) — vector dot product ── */
        if (strcmp(name->chars, "dot_product") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(a) && IS_LIST(b)) {
                NcList *la = AS_LIST(a), *lb = AS_LIST(b);
                int n = la->count < lb->count ? la->count : lb->count;
                double sum = 0.0;
                for (int i = 0; i < n; i++) {
                    double x = IS_INT(la->items[i]) ? (double)AS_INT(la->items[i]) : (IS_FLOAT(la->items[i]) ? AS_FLOAT(la->items[i]) : 0);
                    double y = IS_INT(lb->items[i]) ? (double)AS_INT(lb->items[i]) : (IS_FLOAT(lb->items[i]) ? AS_FLOAT(lb->items[i]) : 0);
                    sum += x * y;
                }
                return NC_FLOAT(sum);
            }
            return NC_FLOAT(0.0);
        }

        /* ── linspace(start, end, n) — evenly spaced values ─ */
        if (strcmp(name->chars, "linspace") == 0 && node->as.call.arg_count == 3) {
            NcValue vs = eval_expr(interp, node->as.call.args[0], scope);
            NcValue ve = eval_expr(interp, node->as.call.args[1], scope);
            NcValue vn = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_NUMBER(vs) && IS_NUMBER(ve) && IS_INT(vn)) {
                double start = IS_INT(vs) ? (double)AS_INT(vs) : AS_FLOAT(vs);
                double end   = IS_INT(ve) ? (double)AS_INT(ve) : AS_FLOAT(ve);
                int n = (int)AS_INT(vn);
                NcList *out = nc_list_new();
                if (n <= 0) return NC_LIST(out);
                if (n == 1) { nc_list_push(out, NC_FLOAT(start)); return NC_LIST(out); }
                double step = (end - start) / (n - 1);
                for (int i = 0; i < n; i++)
                    nc_list_push(out, NC_FLOAT(start + step * i));
                return NC_LIST(out);
            }
            return NC_LIST(nc_list_new());
        }

        /* ═══════════════════════════════════════════════════════
         *  Module builtins: re, csv, os, datetime, base64,
         *                   hashlib (md5), uuid_v4
         * ═══════════════════════════════════════════════════════ */

        /* ── Regex module ─────────────────────────────────── */
        if (strcmp(name->chars, "re_match") == 0 && node->as.call.arg_count == 2) {
            NcValue pat = eval_expr(interp, node->as.call.args[0], scope);
            NcValue str = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(pat) && IS_STRING(str))
                return nc_stdlib_re_match(AS_STRING(pat)->chars, AS_STRING(str)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "re_search") == 0 && node->as.call.arg_count == 2) {
            NcValue pat = eval_expr(interp, node->as.call.args[0], scope);
            NcValue str = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(pat) && IS_STRING(str))
                return nc_stdlib_re_search(AS_STRING(pat)->chars, AS_STRING(str)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "re_findall") == 0 && node->as.call.arg_count == 2) {
            NcValue pat = eval_expr(interp, node->as.call.args[0], scope);
            NcValue str = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(pat) && IS_STRING(str))
                return nc_stdlib_re_findall(AS_STRING(pat)->chars, AS_STRING(str)->chars);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "re_replace") == 0 && node->as.call.arg_count == 3) {
            NcValue pat = eval_expr(interp, node->as.call.args[0], scope);
            NcValue repl = eval_expr(interp, node->as.call.args[1], scope);
            NcValue str = eval_expr(interp, node->as.call.args[2], scope);
            if (IS_STRING(pat) && IS_STRING(repl) && IS_STRING(str))
                return nc_stdlib_re_replace(AS_STRING(pat)->chars, AS_STRING(repl)->chars, AS_STRING(str)->chars);
            return IS_STRING(str) ? str : NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "re_split") == 0 && node->as.call.arg_count == 2) {
            NcValue pat = eval_expr(interp, node->as.call.args[0], scope);
            NcValue str = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(pat) && IS_STRING(str))
                return nc_stdlib_re_split(AS_STRING(pat)->chars, AS_STRING(str)->chars);
            return NC_LIST(nc_list_new());
        }

        /* ── CSV module ───────────────────────────────────── */
        if (strcmp(name->chars, "csv_parse") == 0 && node->as.call.arg_count >= 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            char delim = ',';
            if (node->as.call.arg_count >= 2) {
                NcValue d = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(d) && AS_STRING(d)->length > 0)
                    delim = AS_STRING(d)->chars[0];
            }
            if (IS_STRING(arg))
                return nc_stdlib_csv_parse_delim(AS_STRING(arg)->chars, delim);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "csv_stringify") == 0 && node->as.call.arg_count >= 1) {
            NcValue data = eval_expr(interp, node->as.call.args[0], scope);
            char delim = ',';
            if (node->as.call.arg_count >= 2) {
                NcValue d = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(d) && AS_STRING(d)->length > 0)
                    delim = AS_STRING(d)->chars[0];
            }
            return nc_stdlib_csv_stringify(data, delim);
        }

        /* ── OS module ────────────────────────────────────── */
        if (strcmp(name->chars, "os_env") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_env(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "os_cwd") == 0 && node->as.call.arg_count == 0) {
            return nc_stdlib_os_cwd();
        }
        if (strcmp(name->chars, "os_listdir") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_listdir(AS_STRING(arg)->chars);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "os_exists") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_exists(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_mkdir") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_mkdir_p(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_remove") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_remove(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        /* ── New OS functions: glob, walk, is_dir, is_file, file_size, path_join, basename, dirname, setenv ── */
        if (strcmp(name->chars, "os_glob") == 0 && node->as.call.arg_count == 2) {
            NcValue dir = eval_expr(interp, node->as.call.args[0], scope);
            NcValue pat = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(dir) && IS_STRING(pat))
                return nc_stdlib_os_glob(AS_STRING(dir)->chars, AS_STRING(pat)->chars);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "os_walk") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_walk(AS_STRING(arg)->chars);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "os_is_dir") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_is_dir(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_is_file") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_is_file(AS_STRING(arg)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_file_size") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_file_size(AS_STRING(arg)->chars);
            return NC_INT(0);
        }
        if (strcmp(name->chars, "os_path_join") == 0 && node->as.call.arg_count >= 1) {
            NcList *parts = nc_list_new();
            for (int ai = 0; ai < node->as.call.arg_count; ai++) {
                NcValue part = eval_expr(interp, node->as.call.args[ai], scope);
                nc_list_push(parts, part);
            }
            NcValue result = nc_stdlib_os_path_join(parts);
            return result;
        }
        if (strcmp(name->chars, "os_basename") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_basename(AS_STRING(arg)->chars);
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "os_dirname") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_dirname(AS_STRING(arg)->chars);
            return NC_STRING(nc_string_from_cstr("."));
        }
        if (strcmp(name->chars, "os_setenv") == 0 && node->as.call.arg_count >= 1) {
            NcValue n = eval_expr(interp, node->as.call.args[0], scope);
            NcValue v = node->as.call.arg_count >= 2 ? eval_expr(interp, node->as.call.args[1], scope) : NC_NONE();
            if (IS_STRING(n))
                return nc_stdlib_os_setenv(AS_STRING(n)->chars, IS_STRING(v) ? AS_STRING(v)->chars : NULL);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_read_file") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_read_file(AS_STRING(arg)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "os_write_file") == 0 && node->as.call.arg_count == 2) {
            NcValue path = eval_expr(interp, node->as.call.args[0], scope);
            NcValue content = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(path) && IS_STRING(content))
                return nc_stdlib_os_write_file(AS_STRING(path)->chars, AS_STRING(content)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "os_exec") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_os_exec(AS_STRING(arg)->chars);
            return NC_NONE();
        }

        /* ── DateTime module ──────────────────────────────── */
        if (strcmp(name->chars, "datetime_now") == 0 && node->as.call.arg_count == 0) {
            return nc_stdlib_datetime_now();
        }
        if (strcmp(name->chars, "datetime_parse") == 0 && node->as.call.arg_count == 2) {
            NcValue str = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fmt = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(str) && IS_STRING(fmt))
                return nc_stdlib_datetime_parse(AS_STRING(str)->chars, AS_STRING(fmt)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "datetime_format") == 0 && node->as.call.arg_count == 2) {
            NcValue ts = eval_expr(interp, node->as.call.args[0], scope);
            NcValue fmt = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_NUMBER(ts) && IS_STRING(fmt)) {
                double t = IS_INT(ts) ? (double)AS_INT(ts) : AS_FLOAT(ts);
                return nc_stdlib_datetime_format(t, AS_STRING(fmt)->chars);
            }
            return NC_NONE();
        }
        if (strcmp(name->chars, "datetime_diff") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            return nc_stdlib_datetime_diff(da, db);
        }
        if (strcmp(name->chars, "datetime_add") == 0 && node->as.call.arg_count == 2) {
            NcValue ts = eval_expr(interp, node->as.call.args[0], scope);
            NcValue secs = eval_expr(interp, node->as.call.args[1], scope);
            double t = IS_INT(ts) ? (double)AS_INT(ts) : AS_FLOAT(ts);
            double s = IS_INT(secs) ? (double)AS_INT(secs) : AS_FLOAT(secs);
            return nc_stdlib_datetime_add(t, s);
        }

        /* ── File DB module ─────────────────────────────────── */
        if (strcmp(name->chars, "db_put") == 0 && node->as.call.arg_count >= 2) {
            NcValue url_v = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_v = eval_expr(interp, node->as.call.args[1], scope);
            NcValue val_v = node->as.call.arg_count >= 3
                          ? eval_expr(interp, node->as.call.args[2], scope) : NC_NONE();
            if (IS_STRING(url_v) && IS_STRING(key_v)) {
                char *val_json = nc_json_serialize(val_v, false);
                NcValue r = nc_filedb_put(AS_STRING(url_v)->chars, AS_STRING(key_v)->chars,
                                          val_json ? val_json : "null");
                free(val_json);
                return r;
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "db_get") == 0 && node->as.call.arg_count == 2) {
            NcValue url_v = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_v = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(url_v) && IS_STRING(key_v))
                return nc_filedb_get(AS_STRING(url_v)->chars, AS_STRING(key_v)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "db_delete") == 0 && node->as.call.arg_count == 2) {
            NcValue url_v = eval_expr(interp, node->as.call.args[0], scope);
            NcValue key_v = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(url_v) && IS_STRING(key_v))
                return nc_filedb_delete(AS_STRING(url_v)->chars, AS_STRING(key_v)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "db_list") == 0 && node->as.call.arg_count == 1) {
            NcValue url_v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(url_v))
                return nc_filedb_list(AS_STRING(url_v)->chars);
            return NC_LIST(nc_list_new());
        }
        if (strcmp(name->chars, "db_all") == 0 && node->as.call.arg_count == 1) {
            NcValue url_v = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(url_v))
                return nc_filedb_all(AS_STRING(url_v)->chars);
            return NC_MAP(nc_map_new());
        }

        /* ── Plugin system ─────────────────────────────────── */
        if (strcmp(name->chars, "plugin_load") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg)) {
                int ok = nc_plugin_load(AS_STRING(arg)->chars);
                return NC_BOOL(ok == 0);
            }
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "plugin_has") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return NC_BOOL(nc_plugin_has(AS_STRING(arg)->chars));
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "plugin_call") == 0 && node->as.call.arg_count >= 1) {
            NcValue fname = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(fname)) {
                int argc = node->as.call.arg_count - 1;
                NcValue args[16];
                for (int ai = 0; ai < argc && ai < 16; ai++)
                    args[ai] = eval_expr(interp, node->as.call.args[ai + 1], scope);
                return nc_plugin_call(AS_STRING(fname)->chars, argc, args);
            }
            return NC_NONE();
        }

        /* ── Base64 module ────────────────────────────────── */
        if (strcmp(name->chars, "base64_encode") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_base64_encode(AS_STRING(arg)->chars);
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "base64_decode") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(arg))
                return nc_stdlib_base64_decode(AS_STRING(arg)->chars);
            return NC_STRING(nc_string_from_cstr(""));
        }

        /* ── Hashlib module ───────────────────────────────── */
        if (strcmp(name->chars, "hash_md5") == 0 && node->as.call.arg_count == 1) {
            NcValue arg = eval_expr(interp, node->as.call.args[0], scope);
            NcString *s = nc_value_to_string(arg);
            NcValue result = nc_stdlib_hash_md5(s->chars);
            nc_string_free(s);
            return result;
        }

        /* ── UUID module ──────────────────────────────────── */
        if (strcmp(name->chars, "uuid_v4") == 0 && node->as.call.arg_count == 0) {
            return nc_stdlib_uuid_v4();
        }

        /* ── Math module (extended) ───────────────────────── */
        if (strcmp(name->chars, "math_pi")  == 0) return nc_stdlib_math_pi();
        if (strcmp(name->chars, "math_e")   == 0) return nc_stdlib_math_e();
        if (strcmp(name->chars, "math_inf") == 0) return nc_stdlib_math_inf();
        if (strcmp(name->chars, "math_nan") == 0) return nc_stdlib_math_nan();
        if (strcmp(name->chars, "log2") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            double v = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            return nc_stdlib_math_log2(v);
        }
        if (strcmp(name->chars, "log10") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            double v = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            return nc_stdlib_math_log10(v);
        }
        if (strcmp(name->chars, "asin") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_asin(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "acos") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_acos(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "atan") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_atan(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "atan2") == 0 && node->as.call.arg_count == 2) {
            NcValue y = eval_expr(interp, node->as.call.args[0], scope);
            NcValue x = eval_expr(interp, node->as.call.args[1], scope);
            double yd = IS_INT(y) ? (double)AS_INT(y) : AS_FLOAT(y);
            double xd = IS_INT(x) ? (double)AS_INT(x) : AS_FLOAT(x);
            return nc_stdlib_math_atan2(yd, xd);
        }
        if (strcmp(name->chars, "hypot") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            double ad = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double bd = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            return nc_stdlib_math_hypot(ad, bd);
        }
        if (strcmp(name->chars, "degrees") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_degrees(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "radians") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_radians(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "trunc") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_trunc(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "sign") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_sign(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "isnan") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_isnan(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "isinf") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_isinf(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "isfinite") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_isfinite(IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a));
        }
        if (strcmp(name->chars, "gcd") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            return nc_stdlib_math_gcd(IS_INT(a) ? AS_INT(a) : (int64_t)AS_FLOAT(a),
                                      IS_INT(b) ? AS_INT(b) : (int64_t)AS_FLOAT(b));
        }
        if (strcmp(name->chars, "factorial") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_math_factorial(IS_INT(a) ? AS_INT(a) : (int64_t)AS_FLOAT(a));
        }
        if (strcmp(name->chars, "clamp") == 0 && node->as.call.arg_count == 3) {
            NcValue x  = eval_expr(interp, node->as.call.args[0], scope);
            NcValue lo = eval_expr(interp, node->as.call.args[1], scope);
            NcValue hi = eval_expr(interp, node->as.call.args[2], scope);
            double xd  = IS_INT(x)  ? (double)AS_INT(x)  : AS_FLOAT(x);
            double lod = IS_INT(lo) ? (double)AS_INT(lo) : AS_FLOAT(lo);
            double hid = IS_INT(hi) ? (double)AS_INT(hi) : AS_FLOAT(hi);
            return nc_stdlib_math_clamp(xd, lod, hid);
        }
        if (strcmp(name->chars, "lerp") == 0 && node->as.call.arg_count == 3) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            NcValue t = eval_expr(interp, node->as.call.args[2], scope);
            double ad = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double bd = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            double td = IS_INT(t) ? (double)AS_INT(t) : AS_FLOAT(t);
            return nc_stdlib_math_lerp(ad, bd, td);
        }

        /* ── Random module ────────────────────────────────── */
        if (strcmp(name->chars, "rand_seed") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_rand_seed(IS_INT(a) ? AS_INT(a) : (int64_t)AS_FLOAT(a));
        }
        if (strcmp(name->chars, "rand_float") == 0 && node->as.call.arg_count == 0) {
            return nc_stdlib_rand_float();
        }
        if (strcmp(name->chars, "randint") == 0 && node->as.call.arg_count == 2) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            NcValue b = eval_expr(interp, node->as.call.args[1], scope);
            return nc_stdlib_rand_int(IS_INT(a) ? AS_INT(a) : (int64_t)AS_FLOAT(a),
                                      IS_INT(b) ? AS_INT(b) : (int64_t)AS_FLOAT(b));
        }
        if (strcmp(name->chars, "rand_range") == 0) {
            if (node->as.call.arg_count == 1) {
                NcValue s = eval_expr(interp, node->as.call.args[0], scope);
                int64_t stop = IS_INT(s) ? AS_INT(s) : (int64_t)AS_FLOAT(s);
                return nc_stdlib_rand_range(0, stop);
            } else if (node->as.call.arg_count == 2) {
                NcValue a = eval_expr(interp, node->as.call.args[0], scope);
                NcValue b = eval_expr(interp, node->as.call.args[1], scope);
                return nc_stdlib_rand_range(IS_INT(a) ? AS_INT(a) : (int64_t)AS_FLOAT(a),
                                            IS_INT(b) ? AS_INT(b) : (int64_t)AS_FLOAT(b));
            }
        }
        if (strcmp(name->chars, "rand_choice") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(a)) return nc_stdlib_rand_choice(AS_LIST(a));
            return NC_NONE();
        }
        if (strcmp(name->chars, "rand_shuffle") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(a)) return nc_stdlib_rand_shuffle(AS_LIST(a));
            return NC_NONE();
        }
        if (strcmp(name->chars, "rand_sample") == 0 && node->as.call.arg_count == 2) {
            NcValue list = eval_expr(interp, node->as.call.args[0], scope);
            NcValue k    = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_LIST(list))
                return nc_stdlib_rand_sample(AS_LIST(list), IS_INT(k) ? (int)AS_INT(k) : 1);
            return NC_LIST(nc_list_new());
        }

        /* ── String extended module ───────────────────────── */
        if (strcmp(name->chars, "str_count") == 0 && node->as.call.arg_count == 2) {
            NcValue s   = eval_expr(interp, node->as.call.args[0], scope);
            NcValue sub = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s) && IS_STRING(sub))
                return nc_stdlib_str_count(AS_STRING(s)->chars, AS_STRING(sub)->chars);
            return NC_INT(0);
        }
        if (strcmp(name->chars, "str_pad_left") == 0 && node->as.call.arg_count >= 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue w = eval_expr(interp, node->as.call.args[1], scope);
            char fill = ' ';
            if (node->as.call.arg_count >= 3) {
                NcValue fc = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_STRING(fc) && AS_STRING(fc)->length > 0) fill = AS_STRING(fc)->chars[0];
            }
            if (IS_STRING(s))
                return nc_stdlib_str_pad_left(AS_STRING(s)->chars,
                                              IS_INT(w) ? (int)AS_INT(w) : 0, fill);
            return s;
        }
        if (strcmp(name->chars, "str_pad_right") == 0 && node->as.call.arg_count >= 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue w = eval_expr(interp, node->as.call.args[1], scope);
            char fill = ' ';
            if (node->as.call.arg_count >= 3) {
                NcValue fc = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_STRING(fc) && AS_STRING(fc)->length > 0) fill = AS_STRING(fc)->chars[0];
            }
            if (IS_STRING(s))
                return nc_stdlib_str_pad_right(AS_STRING(s)->chars,
                                               IS_INT(w) ? (int)AS_INT(w) : 0, fill);
            return s;
        }
        if (strcmp(name->chars, "str_center") == 0 && node->as.call.arg_count >= 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue w = eval_expr(interp, node->as.call.args[1], scope);
            char fill = ' ';
            if (node->as.call.arg_count >= 3) {
                NcValue fc = eval_expr(interp, node->as.call.args[2], scope);
                if (IS_STRING(fc) && AS_STRING(fc)->length > 0) fill = AS_STRING(fc)->chars[0];
            }
            if (IS_STRING(s))
                return nc_stdlib_str_center(AS_STRING(s)->chars,
                                            IS_INT(w) ? (int)AS_INT(w) : 0, fill);
            return s;
        }
        if (strcmp(name->chars, "str_repeat") == 0 && node->as.call.arg_count == 2) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcValue n = eval_expr(interp, node->as.call.args[1], scope);
            if (IS_STRING(s))
                return nc_stdlib_str_repeat(AS_STRING(s)->chars, IS_INT(n) ? (int)AS_INT(n) : 0);
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "str_lstrip") == 0 && node->as.call.arg_count >= 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            const char *chars = NULL;
            NcValue chv = NC_NONE();
            if (node->as.call.arg_count >= 2) {
                chv = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(chv)) chars = AS_STRING(chv)->chars;
            }
            if (IS_STRING(s)) return nc_stdlib_str_lstrip(AS_STRING(s)->chars, chars);
            return s;
        }
        if (strcmp(name->chars, "str_rstrip") == 0 && node->as.call.arg_count >= 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            const char *chars = NULL;
            NcValue chv = NC_NONE();
            if (node->as.call.arg_count >= 2) {
                chv = eval_expr(interp, node->as.call.args[1], scope);
                if (IS_STRING(chv)) chars = AS_STRING(chv)->chars;
            }
            if (IS_STRING(s)) return nc_stdlib_str_rstrip(AS_STRING(s)->chars, chars);
            return s;
        }
        if (strcmp(name->chars, "str_title") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_str_title(AS_STRING(s)->chars);
            return s;
        }
        if (strcmp(name->chars, "str_is_digit") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_str_is_digit(AS_STRING(s)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "str_is_alpha") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_str_is_alpha(AS_STRING(s)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "str_is_alnum") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_str_is_alnum(AS_STRING(s)->chars);
            return NC_BOOL(false);
        }
        if (strcmp(name->chars, "str_format") == 0 && node->as.call.arg_count >= 1) {
            NcValue tmpl = eval_expr(interp, node->as.call.args[0], scope);
            NcList *args = nc_list_new();
            for (int ai = 1; ai < node->as.call.arg_count; ai++) {
                NcValue av = eval_expr(interp, node->as.call.args[ai], scope);
                nc_list_push(args, av);
            }
            NcValue result = NC_STRING(nc_string_from_cstr(""));
            if (IS_STRING(tmpl))
                result = nc_stdlib_str_format(AS_STRING(tmpl)->chars, args);
            return result;
        }

        /* ── URL module ───────────────────────────────────── */
        if (strcmp(name->chars, "url_encode") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_url_encode(AS_STRING(s)->chars);
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "url_decode") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_url_decode(AS_STRING(s)->chars);
            return NC_STRING(nc_string_from_cstr(""));
        }
        if (strcmp(name->chars, "url_parse") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_url_parse(AS_STRING(s)->chars);
            return NC_NONE();
        }

        /* ── Log module ───────────────────────────────────── */
        if (strcmp(name->chars, "log_set_level") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_STRING(s)) return nc_stdlib_log_set_level(AS_STRING(s)->chars);
            return NC_NONE();
        }
        if (strcmp(name->chars, "log_debug") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcString *sv = nc_value_to_string(s);
            NcValue r = nc_stdlib_log_debug(sv->chars);
            nc_string_free(sv);
            return r;
        }
        if (strcmp(name->chars, "log_info") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcString *sv = nc_value_to_string(s);
            NcValue r = nc_stdlib_log_info(sv->chars);
            nc_string_free(sv);
            return r;
        }
        if (strcmp(name->chars, "log_warn") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcString *sv = nc_value_to_string(s);
            NcValue r = nc_stdlib_log_warn(sv->chars);
            nc_string_free(sv);
            return r;
        }
        if (strcmp(name->chars, "log_error") == 0 && node->as.call.arg_count == 1) {
            NcValue s = eval_expr(interp, node->as.call.args[0], scope);
            NcString *sv = nc_value_to_string(s);
            NcValue r = nc_stdlib_log_error(sv->chars);
            nc_string_free(sv);
            return r;
        }

        /* ── Collections module ───────────────────────────── */
        if (strcmp(name->chars, "counter") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(a)) return nc_stdlib_counter(AS_LIST(a));
            return NC_MAP(nc_map_new());
        }
        if (strcmp(name->chars, "unique") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(a)) return nc_stdlib_unique(AS_LIST(a));
            return a;
        }
        if (strcmp(name->chars, "flatten") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            if (IS_LIST(a)) return nc_stdlib_flatten(AS_LIST(a));
            return a;
        }

        /* ── Type module ──────────────────────────────────── */
        if (strcmp(name->chars, "type_of") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_type_of(a);
        }
        if (strcmp(name->chars, "to_int") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_to_int(a);
        }
        if (strcmp(name->chars, "to_float") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_to_float(a);
        }
        if (strcmp(name->chars, "to_bool") == 0 && node->as.call.arg_count == 1) {
            NcValue a = eval_expr(interp, node->as.call.args[0], scope);
            return nc_stdlib_to_bool(a);
        }

        /* Try user-defined behaviors as callable functions */
        if (interp->program) {
            NcASTNode *prog = interp->program;
            NcASTNode *found_beh = NULL;

            for (int b = 0; b < prog->as.program.beh_count; b++) {
                if (nc_string_equal(prog->as.program.behaviors[b]->as.behavior.name, name)) {
                    found_beh = prog->as.program.behaviors[b];
                    break;
                }
            }

            /* Also search imported modules */
            if (!found_beh) {
                for (int m = 0; m < prog->as.program.import_count; m++) {
                    NcASTNode *mod_ast = nc_module_load_file(
                        prog->as.program.imports[m]->as.import_decl.module->chars, NULL);
                    if (mod_ast) {
                        for (int b = 0; b < mod_ast->as.program.beh_count; b++) {
                            if (nc_string_equal(mod_ast->as.program.behaviors[b]->as.behavior.name, name)) {
                                found_beh = mod_ast->as.program.behaviors[b];
                                break;
                            }
                        }
                    }
                    if (found_beh) break;
                }
            }

            if (found_beh) {
                /* Process decorators */
                bool has_log = false, has_cache = false;
                int cache_ttl = 0;
                for (int d = 0; d < found_beh->as.behavior.decorator_count; d++) {
                    NcASTNode *dec = found_beh->as.behavior.decorators[d];
                    if (strcmp(dec->as.decorator.name->chars, "log") == 0) has_log = true;
                    if (strcmp(dec->as.decorator.name->chars, "cache") == 0) {
                        has_cache = true;
                        if (dec->as.decorator.options) {
                            NcString *ttl_key = nc_string_from_cstr("ttl");
                            NcValue ttl_val = nc_map_get(dec->as.decorator.options, ttl_key);
                            if (IS_INT(ttl_val)) cache_ttl = (int)AS_INT(ttl_val);
                            nc_string_free(ttl_key);
                        }
                    }
                }

                struct timespec dec_start;
                if (has_log) clock_gettime(CLOCK_MONOTONIC, &dec_start);

                Scope *call_scope = scope_new(interp->global);
                if (!call_scope) return NC_NONE();
                call_scope->is_function = true;  /* isolate behavior scope */
                for (int i = 0; i < node->as.call.arg_count && i < found_beh->as.behavior.param_count; i++) {
                    NcValue arg = eval_expr(interp, node->as.call.args[i], scope);
                    nc_map_set(call_scope->vars, nc_string_ref(found_beh->as.behavior.params[i]->as.param.name), arg);
                }
                trace_push(interp, name->chars, node->line);
                Signal sig = exec_body(interp, found_beh->as.behavior.body, found_beh->as.behavior.body_count, call_scope);
                trace_pop(interp);
                NcValue call_result = (sig.type == SIG_RESPOND) ? sig.value : NC_NONE();
                nc_value_retain(call_result);
                scope_free(call_scope);

                /* @log decorator: print timing */
                if (has_log) {
                    struct timespec dec_end;
                    clock_gettime(CLOCK_MONOTONIC, &dec_end);
                    double ms = (dec_end.tv_sec - dec_start.tv_sec) * 1000.0 +
                                (dec_end.tv_nsec - dec_start.tv_nsec) / 1e6;
                    char log_buf[256];
                    snprintf(log_buf, sizeof(log_buf), "[@log] %s completed in %.2fms", name->chars, ms);
                    interp_output(interp, log_buf);
                }
                (void)has_cache; (void)cache_ttl; /* cache impl reserved */

                return call_result;
            }
        }

        /* Function not found — try auto-correct */
        int fn_dist = 999;
        /* ── Plugin function fallback ─────────────────────── */
        /* If function name not found as a builtin, check loaded plugins */
        if (nc_plugin_has(name->chars)) {
            int argc = node->as.call.arg_count;
            NcValue args[16];
            for (int ai = 0; ai < argc && ai < 16; ai++)
                args[ai] = eval_expr(interp, node->as.call.args[ai], scope);
            return nc_plugin_call(name->chars, argc, args);
        }

        const char *fn_corrected = nc_autocorrect_builtin(name->chars, &fn_dist);

        if (fn_corrected &&
            strcmp(fn_corrected, name->chars) != 0 &&
            fn_dist <= NC_AUTOCORRECT_THRESHOLD) {
            char hint[512];
            snprintf(hint, sizeof(hint),
                "[line %d] Auto-corrected '%s' → '%s'",
                node->line, name->chars, fn_corrected);
            interp_output(interp, hint);

            /* Re-invoke with corrected name by creating a modified node */
            NcASTNode corrected_node = *node;
            corrected_node.as.call.name = nc_string_from_cstr(fn_corrected);
            return eval_expr(interp, &corrected_node, scope);
        } else if (fn_corrected && strcmp(fn_corrected, name->chars) != 0) {
            char hint[512];
            snprintf(hint, sizeof(hint),
                "[line %d] I don't know '%s'. Is this what you meant → '%s'?",
                node->line, name->chars, fn_corrected);
            interp_output(interp, hint);
        }
        return NC_NONE();
    }

    case NODE_MATH: {
        NcValue left = eval_expr(interp, node->as.math.left, scope);
        NcValue right = eval_expr(interp, node->as.math.right, scope);
        switch (node->as.math.op) {
        case '+':
            /* List concatenation: [1,2] + [3] → [1,2,3] */
            if (IS_LIST(left) && IS_LIST(right)) {
                NcList *la = AS_LIST(left), *lb = AS_LIST(right);
                NcList *result = nc_list_new();
                for (int li = 0; li < la->count; li++) nc_list_push(result, la->items[li]);
                for (int li = 0; li < lb->count; li++) nc_list_push(result, lb->items[li]);
                return NC_LIST(result);
            }
            /* List append: [1,2] + 3 → [1,2,3] */
            if (IS_LIST(left) && !IS_LIST(right)) {
                NcList *la = AS_LIST(left);
                NcList *result = nc_list_new();
                for (int li = 0; li < la->count; li++) nc_list_push(result, la->items[li]);
                nc_list_push(result, right);
                return NC_LIST(result);
            }
            if (IS_STRING(left) || IS_STRING(right)) {
                NcString *sl = nc_value_to_string(left);
                NcString *sr = nc_value_to_string(right);
                NcString *res = nc_string_concat(sl, sr);
                nc_string_free(sl); nc_string_free(sr);
                return NC_STRING(res);
            }
            if (IS_INT(left) && IS_INT(right)) return NC_INT(AS_INT(left) + AS_INT(right));
            return NC_FLOAT(as_num(left) + as_num(right));
        case '-':
            if (IS_INT(left) && IS_INT(right)) return NC_INT(AS_INT(left) - AS_INT(right));
            return NC_FLOAT(as_num(left) - as_num(right));
        case '*':
            if (IS_INT(left) && IS_INT(right)) return NC_INT(AS_INT(left) * AS_INT(right));
            return NC_FLOAT(as_num(left) * as_num(right));
        case '/':
            { double d = as_num(right); return d != 0 ? NC_FLOAT(as_num(left) / d) : NC_INT(0); }
        case '%':
            if (IS_INT(left) && IS_INT(right) && AS_INT(right) != 0)
                return NC_INT(AS_INT(left) % AS_INT(right));
            { double d = as_num(right); return d != 0 ? NC_FLOAT(fmod(as_num(left), d)) : NC_INT(0); }
        }
        return NC_NONE();
    }

    case NODE_COMPARISON: {
        NcValue left = eval_expr(interp, node->as.comparison.left, scope);
        NcValue right = eval_expr(interp, node->as.comparison.right, scope);
        const char *op = node->as.comparison.op->chars;
        if (strcmp(op, "above") == 0) return NC_BOOL(as_num(left) > as_num(right));
        if (strcmp(op, "below") == 0) return NC_BOOL(as_num(left) < as_num(right));
        if (strcmp(op, "at_least") == 0) return NC_BOOL(as_num(left) >= as_num(right));
        if (strcmp(op, "at_most") == 0) return NC_BOOL(as_num(left) <= as_num(right));
        if (strcmp(op, "equal") == 0) {
            if (IS_STRING(left) && IS_STRING(right))
                return NC_BOOL(nc_string_equal(AS_STRING(left), AS_STRING(right)));
            if (IS_NONE(left) || IS_NONE(right))
                return NC_BOOL(IS_NONE(left) && IS_NONE(right));
            if (IS_BOOL(left) && IS_BOOL(right))
                return NC_BOOL(AS_BOOL(left) == AS_BOOL(right));
            if ((IS_STRING(left) && !IS_NUMBER(right)) ||
                (IS_STRING(right) && !IS_NUMBER(left))) {
                NcString *sl = nc_value_to_string(left);
                NcString *sr = nc_value_to_string(right);
                bool eq = nc_string_equal(sl, sr);
                nc_string_free(sl); nc_string_free(sr);
                return NC_BOOL(eq);
            }
            return NC_BOOL(as_num(left) == as_num(right));
        }
        if (strcmp(op, "not_equal") == 0) {
            if (IS_STRING(left) && IS_STRING(right))
                return NC_BOOL(!nc_string_equal(AS_STRING(left), AS_STRING(right)));
            if (IS_NONE(left) || IS_NONE(right))
                return NC_BOOL(!(IS_NONE(left) && IS_NONE(right)));
            if (IS_BOOL(left) && IS_BOOL(right))
                return NC_BOOL(AS_BOOL(left) != AS_BOOL(right));
            if ((IS_STRING(left) && !IS_NUMBER(right)) ||
                (IS_STRING(right) && !IS_NUMBER(left))) {
                NcString *sl = nc_value_to_string(left);
                NcString *sr = nc_value_to_string(right);
                bool eq = nc_string_equal(sl, sr);
                nc_string_free(sl); nc_string_free(sr);
                return NC_BOOL(!eq);
            }
            return NC_BOOL(as_num(left) != as_num(right));
        }
        if (strcmp(op, "in") == 0 || strcmp(op, "not_in") == 0) {
            bool found = false;
            if (IS_LIST(right) && IS_STRING(left)) {
                NcList *list = AS_LIST(right);
                for (int i = 0; i < list->count; i++) {
                    if (IS_STRING(list->items[i]) &&
                        nc_string_equal(AS_STRING(left), AS_STRING(list->items[i]))) {
                        found = true; break;
                    }
                }
            }
            return NC_BOOL(strcmp(op, "in") == 0 ? found : !found);
        }
        return NC_BOOL(false);
    }

    case NODE_LOGIC: {
        NcValue left = eval_expr(interp, node->as.logic.left, scope);
        if (strcmp(node->as.logic.op->chars, "and") == 0) {
            if (!nc_truthy(left)) return left;
            return eval_expr(interp, node->as.logic.right, scope);
        }
        if (nc_truthy(left)) return left;
        return eval_expr(interp, node->as.logic.right, scope);
    }

    case NODE_NOT:
        return NC_BOOL(!nc_truthy(eval_expr(interp, node->as.logic.left, scope)));

    case NODE_INDEX: {
        NcValue obj = eval_expr(interp, node->as.math.left, scope);
        NcValue idx = eval_expr(interp, node->as.math.right, scope);
        /* Normalize float index to int */
        if (IS_FLOAT(idx)) idx = NC_INT((int64_t)AS_FLOAT(idx));
        if (IS_LIST(obj) && IS_INT(idx)) {
            int i = (int)AS_INT(idx);
            int len = AS_LIST(obj)->count;
            if (i < 0) i = len + i;
            if (i >= 0 && i < len)
                return nc_list_get(AS_LIST(obj), i);
            return NC_NONE();
        }
        if (IS_MAP(obj) && IS_STRING(idx))
            return nc_map_get(AS_MAP(obj), AS_STRING(idx));
        if (IS_MAP(obj) && IS_INT(idx)) {
            NcString *key = nc_value_to_string(idx);
            NcValue result = nc_map_get(AS_MAP(obj), key);
            nc_string_free(key);
            return result;
        }
        if (IS_STRING(obj) && IS_INT(idx)) {
            int i = (int)AS_INT(idx);
            if (i < 0) i = AS_STRING(obj)->length + i;
            if (i >= 0 && i < AS_STRING(obj)->length) {
                char ch[2] = {AS_STRING(obj)->chars[i], '\0'};
                return NC_STRING(nc_string_from_cstr(ch));
            }
        }
        /* Auto-parse JSON strings for bracket access */
        if (IS_STRING(obj) && AS_STRING(obj)->length > 1) {
            const char *s = AS_STRING(obj)->chars;
            if (s[0] == '{' || s[0] == '[') {
                NcValue parsed = nc_json_parse(s);
                if (IS_MAP(parsed) && IS_STRING(idx))
                    return nc_map_get(AS_MAP(parsed), AS_STRING(idx));
                if (IS_LIST(parsed) && IS_INT(idx))
                    return nc_list_get(AS_LIST(parsed), (int)AS_INT(idx));
            }
        }
        return NC_NONE();
    }

    case NODE_SLICE: {
        NcValue obj = eval_expr(interp, node->as.slice.object, scope);
        NcValue start_val = node->as.slice.start ? eval_expr(interp, node->as.slice.start, scope) : NC_NONE();
        NcValue end_val = node->as.slice.end ? eval_expr(interp, node->as.slice.end, scope) : NC_NONE();

        if (IS_STRING(obj)) {
            int len = AS_STRING(obj)->length;
            int start = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
            int end = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
            if (start < 0) start = len + start;
            if (end < 0) end = len + end;
            if (start < 0) start = 0;
            if (end > len) end = len;
            if (start >= end) return NC_STRING(nc_string_from_cstr(""));
            return NC_STRING(nc_string_new(AS_STRING(obj)->chars + start, end - start));
        }
        if (IS_LIST(obj)) {
            int len = AS_LIST(obj)->count;
            int start = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
            int end = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
            if (start < 0) start = len + start;
            if (end < 0) end = len + end;
            if (start < 0) start = 0;
            if (end > len) end = len;
            NcList *result = nc_list_new();
            for (int i = start; i < end; i++)
                nc_list_push(result, AS_LIST(obj)->items[i]);
            return NC_LIST(result);
        }
        return NC_NONE();
    }

    default: return NC_NONE();
    }
}

/* ── Statement execution ───────────────────────────────────── */

static Signal exec_stmt(NCInterp *interp, NcASTNode *node, Scope *scope) {
    if (!node || interp->had_error) return NO_SIGNAL;

    switch (node->type) {

    case NODE_SET: {
        NcValue val = eval_expr(interp, node->as.set_stmt.value, scope);
        if (node->as.set_stmt.field) {
            NcValue obj = scope_get(scope, node->as.set_stmt.target);
            if (IS_MAP(obj)) {
                if (node->as.set_stmt.subfield) {
                    /* 2-level deep set: x.a.b = val */
                    NcValue inner = nc_map_get(AS_MAP(obj), node->as.set_stmt.field);
                    if (IS_MAP(inner)) {
                        nc_map_set(AS_MAP(inner), node->as.set_stmt.subfield, val);
                    }
                } else {
                    nc_map_set(AS_MAP(obj), node->as.set_stmt.field, val);
                }
            }
        } else {
            scope_set(scope, node->as.set_stmt.target, val);
        }
        nc_value_release(val); /* release creation ref; container retained its own */
        return NO_SIGNAL;
    }

    case NODE_SET_INDEX: {
        NcValue val = eval_expr(interp, node->as.set_index.value, scope);
        NcValue idx = eval_expr(interp, node->as.set_index.index, scope);
        NcString *key = NULL;
        if (IS_STRING(idx)) key = AS_STRING(idx);
        else { NcString *tmp = nc_value_to_string(idx); key = tmp; }

        if (node->as.set_index.field) {
            NcValue obj = scope_get(scope, node->as.set_index.target);
            if (IS_MAP(obj)) {
                NcValue sub = nc_map_get(AS_MAP(obj), node->as.set_index.field);
                if (IS_MAP(sub)) {
                    nc_map_set(AS_MAP(sub), key, val);
                } else {
                    NcMap *new_map = nc_map_new();
                    nc_map_set(new_map, key, val);
                    nc_map_set(AS_MAP(obj), node->as.set_index.field, NC_MAP(new_map));
                }
            }
        } else {
            NcValue obj = scope_get(scope, node->as.set_index.target);
            if (IS_MAP(obj)) {
                nc_map_set(AS_MAP(obj), key, val);
            } else if (IS_LIST(obj) && IS_INT(idx)) {
                int i = (int)AS_INT(idx);
                if (i >= 0 && i < AS_LIST(obj)->count) {
                    NcValue old = AS_LIST(obj)->items[i];
                    nc_value_retain(val);
                    AS_LIST(obj)->items[i] = val;
                    nc_value_release(old);
                }
            } else {
                NcMap *new_map = nc_map_new();
                nc_map_set(new_map, key, val);
                scope_set(scope, node->as.set_index.target, NC_MAP(new_map));
            }
        }
        nc_value_release(val); /* release creation ref; container retained its own */
        return NO_SIGNAL;
    }

    case NODE_RESPOND: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        return (Signal){SIG_RESPOND, val};
    }

    case NODE_LOG: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        NcString *s = nc_value_to_string(val);
        char *safe = nc_redact_for_display(s->chars);
        char buf[1024];
        snprintf(buf, sizeof(buf), "[LOG] %s", safe ? safe : s->chars);
        interp_output(interp, buf);
        free(safe);
        nc_string_free(s);
        return NO_SIGNAL;
    }

    case NODE_SHOW: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        NcString *s = nc_value_to_string(val);
        char *redacted = nc_redact_for_display(s->chars);
        interp_output(interp, redacted ? redacted : s->chars);
        if (redacted) free(redacted);
        nc_string_free(s);
        return NO_SIGNAL;
    }

    case NODE_NOTIFY: {
        NcValue ch = eval_expr(interp, node->as.notify.channel, scope);
        NcValue msg = node->as.notify.message ?
            eval_expr(interp, node->as.notify.message, scope) : NC_NONE();
        NcString *ch_s = nc_value_to_string(ch);
        NcString *msg_s = nc_value_to_string(msg);
        char *resolved = resolve_templates(msg_s->chars, scope);
        char buf[1024];
        snprintf(buf, sizeof(buf), "[NOTIFY -> %s] %s", ch_s->chars, resolved);
        interp_output(interp, buf);

        /* Send webhook notification if NC_NOTIFY_URL or NC_NOTIFY_<CHANNEL>_URL is set */
        char env_key[256];
        snprintf(env_key, sizeof(env_key), "NC_NOTIFY_%s_URL", ch_s->chars);
        for (char *p = env_key + 10; *p; p++) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
            else if (*p == '-' || *p == ' ') *p = '_';
        }
        const char *webhook_url = getenv(env_key);
        if (!webhook_url) webhook_url = getenv("NC_NOTIFY_URL");

        if (webhook_url && webhook_url[0]) {
            char json_body[2048];
            snprintf(json_body, sizeof(json_body),
                "{\"channel\":\"%s\",\"message\":\"%s\",\"source\":\"nc\"}",
                ch_s->chars, resolved);
            char *resp = nc_http_post(webhook_url, json_body, "application/json", NULL);
            free(resp);
        }

        nc_string_free(ch_s); nc_string_free(msg_s); free(resolved);
        return NO_SIGNAL;
    }

    case NODE_WAIT: {
        double secs = node->as.wait_stmt.amount;
        if (secs > 0 && secs < 300) {
            nc_sleep_ms((int)(secs * 1000));
        }
        return NO_SIGNAL;
    }

    case NODE_GATHER: {
        /* Deep-copy options so we don't mutate the AST on repeated calls */
        NcMap *opts = nc_map_new();
        if (node->as.gather.options) {
            NcMap *src_opts = node->as.gather.options;
            for (int oi = 0; oi < src_opts->count; oi++)
                nc_map_set(opts, nc_string_ref(src_opts->keys[oi]), src_opts->values[oi]);
        }

        /* Resolve source: if it's a variable name, use its value.
         * Otherwise resolve {{templates}} in the string. */
        char *resolved_source;
        if (scope_has(scope, node->as.gather.source)) {
            NcValue var_val = scope_get(scope, node->as.gather.source);
            if (IS_STRING(var_val)) {
                resolved_source = resolve_templates(AS_STRING(var_val)->chars, scope);
            } else {
                NcString *vs = nc_value_to_string(var_val);
                resolved_source = strdup(vs->chars);
                nc_string_free(vs);
            }
        } else {
            resolved_source = resolve_templates(node->as.gather.source->chars, scope);
        }
        for (int oi = 0; oi < opts->count; oi++) {
            if (IS_STRING(opts->values[oi])) {
                NcString *raw = AS_STRING(opts->values[oi]);

                /* First try as a variable name (bare identifier from kv block) */
                if (scope_has(scope, raw)) {
                    opts->values[oi] = scope_get(scope, raw);
                    continue;
                }

                /* Then resolve {{templates}} */
                char *rv = resolve_templates(raw->chars, scope);
                opts->values[oi] = NC_STRING(nc_string_from_cstr(rv));
                free(rv);
            }
        }

        NcValue result;
        if (interp->mcp_handler) {
            result = interp->mcp_handler(resolved_source, opts);
        } else {
            result = nc_gather_from(resolved_source, opts);
        }
        free(resolved_source);
        /* Report errors with line info and set had_error for try/on_error */
        if (IS_MAP(result)) {
            NcString *ek = nc_string_from_cstr("error");
            NcValue ev = nc_map_get(AS_MAP(result), ek);
            nc_string_free(ek);
            if (IS_STRING(ev)) {
                snprintf(interp->error_msg, sizeof(interp->error_msg),
                    "Line %d: Could not fetch data — %s", node->line, AS_STRING(ev)->chars);
                interp->had_error = true;
                char buf[1024];
                snprintf(buf, sizeof(buf), "Line %d: Could not fetch data — %s",
                    node->line, AS_STRING(ev)->chars);
                interp_output(interp, buf);
                trace_print(interp);
            }
        }
        scope_set(scope, node->as.gather.target, result);
        nc_value_release(result); /* release creation ref */
        return NO_SIGNAL;
    }

    case NODE_ASK_AI: {
        NcMap *context = nc_map_new();
        for (int i = 0; i < node->as.ask_ai.using_count; i++) {
            NcString *name = node->as.ask_ai.using[i];
            nc_map_set(context, name, scope_get(scope, name));
        }

        char *resolved_prompt = resolve_templates(node->as.ask_ai.prompt->chars, scope);

        /* Build options map from AST + inline model */
        NcMap *ai_options = node->as.ask_ai.options ? node->as.ask_ai.options : nc_map_new();
        if (node->as.ask_ai.model) {
            NcString *mk = nc_string_from_cstr("model");
            if (!nc_map_has(ai_options, mk))
                nc_map_set(ai_options, mk, NC_STRING(nc_string_ref(node->as.ask_ai.model)));
            else
                nc_string_free(mk);
        }

        /* Resolve variable references and {{templates}} in option values */
        for (int oi = 0; oi < ai_options->count; oi++) {
            if (IS_STRING(ai_options->values[oi])) {
                NcString *raw = AS_STRING(ai_options->values[oi]);
                if (scope_has(scope, raw)) {
                    ai_options->values[oi] = scope_get(scope, raw);
                } else {
                    char *rv = resolve_templates(raw->chars, scope);
                    ai_options->values[oi] = NC_STRING(nc_string_from_cstr(rv));
                    free(rv);
                }
            }
        }

        /* Inject intent-aware system prompt via NOVA prompt engineering.
         * Only if the caller hasn't already set a custom system_prompt. */
        {
            NcString *sp_key = nc_string_from_cstr("system_prompt");
            if (!nc_map_has(ai_options, sp_key)) {
                char nova_sys[8192];
                /* Check if web grounding is requested via "web" or "url" option */
                NcString *web_key = nc_string_from_cstr("web");
                bool use_web = nc_map_has(ai_options, web_key);
                const char *url_str = NULL;
                if (!use_web) {
                    nc_string_free(web_key);
                    web_key = nc_string_from_cstr("url");
                    use_web = nc_map_has(ai_options, web_key);
                }
                if (use_web) {
                    NcValue url_val = nc_map_get(ai_options, web_key);
                    url_str = IS_STRING(url_val) ? AS_STRING(url_val)->chars : NULL;
                    nc_reason_build_grounded_prompt(
                        resolved_prompt, url_str,
                        nova_sys, sizeof(nova_sys));
                } else {
                    nc_reason_build_prompt(resolved_prompt, nova_sys,
                                           sizeof(nova_sys), false, true);
                }
                nc_string_free(web_key);
                nc_map_set(ai_options, sp_key,
                           NC_STRING(nc_string_from_cstr(nova_sys)));
            } else {
                nc_string_free(sp_key);
            }
        }

        /* Retry loop — if LLM returns unparseable result, retry up to 2 times */
        NcValue result = NC_NONE();
        int max_retries = 2;
        for (int attempt = 0; attempt <= max_retries; attempt++) {
            if (interp->ai_handler) {
                const char *ai_model = node->as.ask_ai.model ? node->as.ask_ai.model->chars : NULL;
                result = interp->ai_handler(resolved_prompt, context, ai_model);
            } else {
                result = nc_ai_call_ex(resolved_prompt, context, ai_options);
            }

            /* If result is a string, try to parse as JSON */
            if (IS_STRING(result)) {
                NcValue parsed = nc_json_parse(AS_STRING(result)->chars);
                if (!IS_NONE(parsed)) result = parsed;
            }

            /* Report AI errors with line info and set had_error for try/on_error */
            if (IS_MAP(result)) {
                NcString *ok_key = nc_string_from_cstr("ok");
                NcValue ok_val = nc_map_get(AS_MAP(result), ok_key);
                nc_string_free(ok_key);

                NcString *ek = nc_string_from_cstr("error");
                NcValue ev = nc_map_get(AS_MAP(result), ek);
                nc_string_free(ek);

                bool is_ok = IS_BOOL(ok_val) ? AS_BOOL(ok_val) : IS_NONE(ev);

                if (!is_ok && attempt == max_retries) {
                    NcString *err_text = IS_STRING(ev) ? AS_STRING(ev) : nc_string_from_cstr("unknown AI error");
                    snprintf(interp->error_msg, sizeof(interp->error_msg),
                        "line %d: ask AI error: %s", node->line, err_text->chars);
                    interp->had_error = true;
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "[line %d] ask AI error: %s",
                        node->line, err_text->chars);
                    interp_output(interp, buf);
                    trace_print(interp);
                    if (!IS_STRING(ev)) nc_string_free(err_text);
                }
            }

            /* Accept if we got a map or non-empty value */
            if (IS_MAP(result) || IS_LIST(result)) break;
            if (!IS_NONE(result) && !IS_STRING(result)) break;
            if (IS_STRING(result) && AS_STRING(result)->length > 0) break;
        }

        /* The response contract ensures "response" is always present,
         * but also normalize common aliases for backward compatibility */
        if (IS_MAP(result)) {
            NcMap *rm = AS_MAP(result);
            NcString *k_response = nc_string_from_cstr("response");
            NcString *k_content = nc_string_from_cstr("content");
            NcString *k_answer = nc_string_from_cstr("answer");

            NcValue v_response = nc_map_get(rm, k_response);
            NcValue v_content = nc_map_get(rm, k_content);
            NcValue v_answer = nc_map_get(rm, k_answer);

            if (IS_NONE(v_response) && !IS_NONE(v_content))
                nc_map_set(rm, k_response, v_content);
            if (IS_NONE(v_content) && !IS_NONE(v_response))
                nc_map_set(rm, k_content, v_response);
            if (IS_NONE(v_response) && !IS_NONE(v_answer))
                nc_map_set(rm, k_response, v_answer);

            nc_string_free(k_response);
            nc_string_free(k_content);
            nc_string_free(k_answer);
        }

        free(resolved_prompt);

        NcString *save_name = node->as.ask_ai.save_as ?
            node->as.ask_ai.save_as : nc_string_from_cstr("result");
        scope_set(scope, save_name, result);
        return NO_SIGNAL;
    }

    case NODE_IF: {
        NcValue cond = eval_expr(interp, node->as.if_stmt.condition, scope);
        if (nc_truthy(cond)) {
            return exec_body(interp, node->as.if_stmt.then_body, node->as.if_stmt.then_count, scope);
        } else if (node->as.if_stmt.else_body) {
            return exec_body(interp, node->as.if_stmt.else_body, node->as.if_stmt.else_count, scope);
        }
        return NO_SIGNAL;
    }

    case NODE_REPEAT: {
        NcValue iterable = eval_expr(interp, node->as.repeat.iterable, scope);
        if (IS_MAP(iterable) && node->as.repeat.key_variable) {
            NcMap *map = AS_MAP(iterable);
            for (int i = 0; i < map->count; i++) {
                Scope *loop_scope = scope_new(scope);
                if (!loop_scope) return NO_SIGNAL;
                nc_map_set(loop_scope->vars, node->as.repeat.key_variable,
                           NC_STRING(nc_string_ref(map->keys[i])));
                nc_map_set(loop_scope->vars, node->as.repeat.variable, map->values[i]);
                Signal sig = exec_body(interp, node->as.repeat.body, node->as.repeat.body_count, loop_scope);
                scope_free(loop_scope);
                if (sig.type == SIG_RESPOND) return sig;
                if (sig.type == SIG_STOP) break;
            }
        } else if (IS_LIST(iterable)) {
            NcList *list = AS_LIST(iterable);
            for (int i = 0; i < list->count; i++) {
                Scope *loop_scope = scope_new(scope);
                if (!loop_scope) return NO_SIGNAL;
                if (node->as.repeat.key_variable)
                    nc_map_set(loop_scope->vars, node->as.repeat.key_variable, NC_INT(i));
                nc_map_set(loop_scope->vars, node->as.repeat.variable, list->items[i]);
                Signal sig = exec_body(interp, node->as.repeat.body, node->as.repeat.body_count, loop_scope);
                scope_free(loop_scope);
                if (sig.type == SIG_RESPOND) return sig;
                if (sig.type == SIG_STOP) break;
            }
        }
        return NO_SIGNAL;
    }

    case NODE_MATCH: {
        NcValue subj = eval_expr(interp, node->as.match_stmt.subject, scope);
        for (int i = 0; i < node->as.match_stmt.case_count; i++) {
            NcASTNode *when = node->as.match_stmt.cases[i];
            NcASTNode *pattern = when->as.when_clause.value;
            bool matches = false;

            /* Destructuring pattern: when {name, age}: */
            if (pattern->type == NODE_DESTRUCTURE && IS_MAP(subj)) {
                NcMap *map = AS_MAP(subj);
                matches = true;
                Scope *match_scope = scope_new(scope);
                for (int f = 0; f < pattern->as.match_guard.field_count; f++) {
                    NcString *fname = pattern->as.match_guard.fields[f];
                    if (nc_map_has(map, fname)) {
                        scope_set(match_scope, fname, nc_map_get(map, fname));
                    } else {
                        matches = false;
                        break;
                    }
                }
                /* Check guard if present */
                if (matches && pattern->as.match_guard.guard) {
                    NcValue guard_val = eval_expr(interp, pattern->as.match_guard.guard, match_scope);
                    matches = IS_BOOL(guard_val) ? AS_BOOL(guard_val) : !IS_NONE(guard_val);
                }
                if (matches) {
                    Signal sig = exec_body(interp, when->as.when_clause.body, when->as.when_clause.body_count, match_scope);
                    scope_free(match_scope);
                    return sig;
                }
                scope_free(match_scope);
            }
            /* Destructuring pattern on list: when [first, ...rest]: */
            else if (pattern->type == NODE_DESTRUCTURE && IS_LIST(subj)) {
                NcList *list = AS_LIST(subj);
                matches = true;
                Scope *match_scope = scope_new(scope);
                int rest_idx = -1;
                for (int f = 0; f < pattern->as.match_guard.field_count; f++) {
                    NcString *fname = pattern->as.match_guard.fields[f];
                    if (fname->length > 3 && strncmp(fname->chars, "...", 3) == 0) {
                        /* Rest pattern: ...rest */
                        rest_idx = f;
                        NcList *rest = nc_list_new();
                        for (int r = f; r < list->count; r++)
                            nc_list_push(rest, list->items[r]);
                        NcString *rest_name = nc_string_from_cstr(fname->chars + 3);
                        scope_set(match_scope, rest_name, NC_LIST(rest));
                        nc_string_free(rest_name);
                        break;
                    } else if (f < list->count) {
                        scope_set(match_scope, fname, list->items[f]);
                    } else {
                        matches = false;
                        break;
                    }
                }
                if (matches && rest_idx < 0 && pattern->as.match_guard.field_count != list->count)
                    matches = false;
                if (matches) {
                    Signal sig = exec_body(interp, when->as.when_clause.body, when->as.when_clause.body_count, match_scope);
                    scope_free(match_scope);
                    return sig;
                }
                scope_free(match_scope);
            }
            /* Guard node: when x if condition: */
            else if (when->as.when_clause.body_count == 1 &&
                     when->as.when_clause.body[0]->type == NODE_MATCH_GUARD) {
                NcASTNode *guarded = when->as.when_clause.body[0];
                NcValue case_val = eval_expr(interp, pattern, scope);
                if (nc_value_equal(subj, case_val)) {
                    NcValue guard_val = eval_expr(interp, guarded->as.match_guard.guard, scope);
                    if (IS_BOOL(guard_val) ? AS_BOOL(guard_val) : !IS_NONE(guard_val))
                        return exec_body(interp, guarded->as.match_guard.body, guarded->as.match_guard.body_count, scope);
                }
            }
            /* Standard value match */
            else {
                NcValue case_val = eval_expr(interp, pattern, scope);
                if (IS_STRING(subj) && IS_STRING(case_val))
                    matches = nc_string_equal(AS_STRING(subj), AS_STRING(case_val));
                else
                    matches = nc_value_equal(subj, case_val);
                if (matches)
                    return exec_body(interp, when->as.when_clause.body, when->as.when_clause.body_count, scope);
            }
        }
        if (node->as.match_stmt.otherwise)
            return exec_body(interp, node->as.match_stmt.otherwise, node->as.match_stmt.otherwise_count, scope);
        return NO_SIGNAL;
    }

    case NODE_RUN: {
        NcString *name = node->as.run_stmt.name;
        NcASTNode *prog = interp->program;
        NcASTNode *found_beh = NULL;

        /* Search local behaviors first */
        for (int b = 0; b < prog->as.program.beh_count; b++) {
            if (nc_string_equal(prog->as.program.behaviors[b]->as.behavior.name, name)) {
                found_beh = prog->as.program.behaviors[b];
                break;
            }
        }

        /* If not found locally, search imported modules */
        if (!found_beh) {
            for (int m = 0; m < prog->as.program.import_count; m++) {
                NcASTNode *mod_ast = nc_module_load_file(
                    prog->as.program.imports[m]->as.import_decl.module->chars, NULL);
                if (mod_ast) {
                    for (int b = 0; b < mod_ast->as.program.beh_count; b++) {
                        if (nc_string_equal(mod_ast->as.program.behaviors[b]->as.behavior.name, name)) {
                            found_beh = mod_ast->as.program.behaviors[b];
                            break;
                        }
                    }
                }
                if (found_beh) break;
            }
        }

        if (found_beh) {
            if (getenv("NC_TRACE_RUNS")) {
                fprintf(stderr, "[NC RUN stmt] %s\n", name->chars);
            }
            Scope *call_scope = scope_new(interp->global);
            if (!call_scope) return NO_SIGNAL;
            call_scope->is_function = true;  /* isolate behavior scope */
            /* Pass explicit arguments by position */
            for (int i = 0; i < node->as.run_stmt.arg_count && i < found_beh->as.behavior.param_count; i++) {
                NcValue arg = eval_expr(interp, node->as.run_stmt.args[i], scope);
                nc_map_set(call_scope->vars, found_beh->as.behavior.params[i]->as.param.name, arg);
            }
            /* For behaviors with no explicit args, pass matching variables from caller's scope */
            if (node->as.run_stmt.arg_count == 0 && found_beh->as.behavior.param_count > 0) {
                for (int i = 0; i < found_beh->as.behavior.param_count; i++) {
                    NcString *param = found_beh->as.behavior.params[i]->as.param.name;
                    if (scope_has(scope, param)) {
                        nc_map_set(call_scope->vars, param, scope_get(scope, param));
                    }
                }
            }
            trace_push(interp, name->chars, node->line);
            Signal sig = exec_body(interp, found_beh->as.behavior.body, found_beh->as.behavior.body_count, call_scope);
            trace_pop(interp);
            NcValue run_result = NC_NONE();
            if (sig.type == SIG_RESPOND) {
                run_result = sig.value;
                nc_value_retain(run_result);
            }
            scope_free(call_scope);
            if (!IS_NONE(run_result)) {
                scope_set(scope, nc_string_from_cstr("result"), run_result);
            }
        } else {
            const char *suggestion = nc_suggest_builtin(name->chars);
            if (suggestion) {
                char hint[512];
                snprintf(hint, sizeof(hint),
                    "[line %d] I don't know the behavior '%s'. Is this what you meant → '%s'?",
                    node->line, name->chars, suggestion);
                interp_output(interp, hint);
            }
        }
        return NO_SIGNAL;
    }

    /* ── run agent <name> with <prompt> — Agentic AI loop ─── */
    case NODE_RUN_AGENT: {
        NcString *agent_name = node->as.run_agent.agent_name;
        NcASTNode *prog = interp->program;

        /* Find the agent definition */
        NcASTNode *agent_def = NULL;
        for (int a = 0; a < prog->as.program.agent_count; a++) {
            if (nc_string_equal(prog->as.program.agents[a]->as.agent_def.name, agent_name)) {
                agent_def = prog->as.program.agents[a];
                break;
            }
        }
        if (!agent_def) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[line %d] Agent '%s' not found.", node->line, agent_name->chars);
            interp_output(interp, buf);
            return NO_SIGNAL;
        }

        /* Evaluate the user's task prompt */
        NcString *task_prompt = NULL;
        if (node->as.run_agent.prompt) {
            NcValue pv = eval_expr(interp, node->as.run_agent.prompt, scope);
            task_prompt = nc_value_to_string(pv);
        } else {
            task_prompt = nc_string_from_cstr("No task specified.");
        }

        int max_steps = agent_def->as.agent_def.max_steps;
        if (max_steps <= 0) max_steps = 10;

        /* Collect @tool behaviors matching agent's tool list */
        NcList *tool_descs = nc_list_new();  /* list of maps: {name, purpose, params} */
        for (int t = 0; t < agent_def->as.agent_def.tool_count; t++) {
            NcString *tool_name = agent_def->as.agent_def.tools[t];
            for (int b = 0; b < prog->as.program.beh_count; b++) {
                NcASTNode *beh = prog->as.program.behaviors[b];
                if (!nc_string_equal(beh->as.behavior.name, tool_name)) continue;
                /* Check for @tool decorator (optional — allow all listed tools) */
                NcMap *td = nc_map_new();
                nc_map_set(td, nc_string_from_cstr("name"), NC_STRING(nc_string_ref(tool_name)));
                if (beh->as.behavior.purpose)
                    nc_map_set(td, nc_string_from_cstr("purpose"), NC_STRING(nc_string_ref(beh->as.behavior.purpose)));
                /* Collect param names */
                NcList *params = nc_list_new();
                for (int pi = 0; pi < beh->as.behavior.param_count; pi++)
                    nc_list_push(params, NC_STRING(nc_string_ref(beh->as.behavior.params[pi]->as.param.name)));
                nc_map_set(td, nc_string_from_cstr("params"), NC_LIST(params));
                nc_list_push(tool_descs, NC_MAP(td));
                break;
            }
        }

        /* Build tool descriptions string for prompt builder */
        char tool_desc_buf[2048];
        int td_len = 0;
        for (int i = 0; i < tool_descs->count && td_len < (int)sizeof(tool_desc_buf) - 256; i++) {
            NcMap *td = AS_MAP(tool_descs->items[i]);
            NcString *nk = nc_string_from_cstr("name");
            NcString *pk = nc_string_from_cstr("purpose");
            NcString *prk = nc_string_from_cstr("params");
            NcValue name_v = nc_map_get(td, nk);
            NcValue purp_v = nc_map_get(td, pk);
            NcValue params_v = nc_map_get(td, prk);
            td_len += snprintf(tool_desc_buf + td_len, sizeof(tool_desc_buf) - td_len,
                "- %s(%s): %s\n",
                IS_STRING(name_v) ? AS_STRING(name_v)->chars : "?",
                IS_LIST(params_v) && AS_LIST(params_v)->count > 0 ?
                    (IS_STRING(AS_LIST(params_v)->items[0]) ? AS_STRING(AS_LIST(params_v)->items[0])->chars : "") : "",
                IS_STRING(purp_v) ? AS_STRING(purp_v)->chars : "general purpose tool");
            nc_string_free(nk); nc_string_free(pk); nc_string_free(prk);
        }

        /* Build rich intent-aware agent system prompt via NOVA reasoning engine */
        char system_prompt[8192];
        nc_reason_build_agent_prompt(
            agent_def->as.agent_def.purpose ? agent_def->as.agent_def.purpose->chars : NULL,
            task_prompt->chars,
            tool_desc_buf,
            system_prompt, sizeof(system_prompt));

        /* Agent loop: Plan → Act → Observe → Repeat */
        NcValue final_result = NC_NONE();
        char conversation[16384];
        int conv_len = snprintf(conversation, sizeof(conversation),
            "%s\n\nTask: %s", system_prompt, task_prompt->chars);

        char step_buf[512];
        for (int step = 0; step < max_steps; step++) {
            /* Call AI with accumulated conversation */
            NcMap *ai_ctx = nc_map_new();
            const char *model_name = agent_def->as.agent_def.model ?
                agent_def->as.agent_def.model->chars : NULL;

            NcValue ai_response;
            if (interp->ai_handler) {
                ai_response = interp->ai_handler(conversation, ai_ctx, model_name);
            } else {
                NcMap *ai_opts = nc_map_new();
                if (model_name)
                    nc_map_set(ai_opts, nc_string_from_cstr("model"), NC_STRING(nc_string_from_cstr(model_name)));
                ai_response = nc_ai_call_ex(conversation, ai_ctx, ai_opts);
                nc_map_free(ai_opts);
            }
            nc_map_free(ai_ctx);

            /* Parse AI response as JSON */
            NcValue parsed = ai_response;
            if (IS_STRING(ai_response)) {
                NcValue jp = nc_json_parse(AS_STRING(ai_response)->chars);
                if (!IS_NONE(jp)) parsed = jp;
            }

            /* Extract action */
            NcString *ak = nc_string_from_cstr("action");
            NcString *ik = nc_string_from_cstr("input");
            NcString *rk = nc_string_from_cstr("result");
            NcValue action_v = IS_MAP(parsed) ? nc_map_get(AS_MAP(parsed), ak) : NC_NONE();
            NcValue input_v = IS_MAP(parsed) ? nc_map_get(AS_MAP(parsed), ik) : NC_NONE();
            NcValue result_v = IS_MAP(parsed) ? nc_map_get(AS_MAP(parsed), rk) : NC_NONE();

            const char *action = IS_STRING(action_v) ? AS_STRING(action_v)->chars : NULL;

            /* Finish action — agent is done */
            if (action && strcmp(action, "finish") == 0) {
                if (!IS_NONE(result_v)) {
                    final_result = result_v;
                } else {
                    final_result = ai_response;
                }
                snprintf(step_buf, sizeof(step_buf), "  [agent/%s] Step %d: finish", agent_name->chars, step + 1);
                interp_output(interp, step_buf);
                nc_string_free(ak); nc_string_free(ik); nc_string_free(rk);
                break;
            }

            /* Tool call — find and execute the behavior */
            if (action) {
                snprintf(step_buf, sizeof(step_buf), "  [agent/%s] Step %d: %s", agent_name->chars, step + 1, action);
                interp_output(interp, step_buf);

                NcString *tool_name = nc_string_from_cstr(action);
                NcASTNode *tool_beh = NULL;
                for (int b = 0; b < prog->as.program.beh_count; b++) {
                    if (nc_string_equal(prog->as.program.behaviors[b]->as.behavior.name, tool_name)) {
                        tool_beh = prog->as.program.behaviors[b];
                        break;
                    }
                }

                NcValue observation = NC_NONE();
                if (tool_beh) {
                    /* Execute tool behavior with input parameters */
                    Scope *tool_scope = scope_new(interp->global);
                    if (tool_scope) {
                        tool_scope->is_function = true;
                        /* Map AI's input JSON to tool params */
                        if (IS_MAP(input_v)) {
                            for (int pi = 0; pi < tool_beh->as.behavior.param_count; pi++) {
                                NcString *pname = tool_beh->as.behavior.params[pi]->as.param.name;
                                NcValue pval = nc_map_get(AS_MAP(input_v), pname);
                                if (!IS_NONE(pval))
                                    nc_map_set(tool_scope->vars, pname, pval);
                            }
                        } else if (IS_STRING(input_v) && tool_beh->as.behavior.param_count > 0) {
                            nc_map_set(tool_scope->vars, tool_beh->as.behavior.params[0]->as.param.name, input_v);
                        }
                        Signal sig = exec_body(interp, tool_beh->as.behavior.body, tool_beh->as.behavior.body_count, tool_scope);
                        if (sig.type == SIG_RESPOND) {
                            observation = sig.value;
                            nc_value_retain(observation);
                        }
                        scope_free(tool_scope);
                    }
                } else {
                    observation = NC_STRING(nc_string_from_cstr("Error: tool not found"));
                }

                /* Append observation to conversation */
                NcString *obs_str = nc_value_to_string(observation);
                int needed = conv_len + (int)strlen(action) + obs_str->length + 64;
                if (needed < (int)sizeof(conversation)) {
                    conv_len += snprintf(conversation + conv_len, sizeof(conversation) - conv_len,
                        "\n\nTool '%s' returned: %s\n\nContinue with the next step.",
                        action, obs_str->chars);
                }
                nc_string_free(obs_str);
                nc_string_free(tool_name);
            } else {
                /* AI didn't return valid action — treat as final answer */
                final_result = ai_response;
                snprintf(step_buf, sizeof(step_buf), "  [agent/%s] Step %d: done (no action)", agent_name->chars, step + 1);
                interp_output(interp, step_buf);
                nc_string_free(ak); nc_string_free(ik); nc_string_free(rk);
                break;
            }

            nc_string_free(ak); nc_string_free(ik); nc_string_free(rk);
        }

        /* Save result */
        if (node->as.run_agent.save_as) {
            scope_set(scope, node->as.run_agent.save_as, final_result);
            nc_value_release(final_result);
        }

        nc_string_free(task_prompt);
        nc_list_free(tool_descs);
        return NO_SIGNAL;
    }

    case NODE_STORE: {
        NcValue val = eval_expr(interp, node->as.store_stmt.value, scope);

        /* Resolve the target: if it's a variable name in scope, use the variable's value
         * as the actual key. This makes `store value into key` work when key is a variable. */
        const char *raw_target = node->as.store_stmt.target->chars;
        char *resolved_target = NULL;
        if (scope_has(scope, node->as.store_stmt.target)) {
            NcValue target_val = scope_get(scope, node->as.store_stmt.target);
            if (IS_STRING(target_val)) {
                resolved_target = strdup(AS_STRING(target_val)->chars);
            }
        }
        if (!resolved_target) {
            resolved_target = resolve_templates(raw_target, scope);
        }

        char *val_json = nc_json_serialize(val, false);

        /* Persist via generic store backend (NC_STORE_URL or in-memory) */
        nc_store_put(resolved_target, val_json);
        free(val_json);

        /* Also keep in local scope for same-session access */
        NcString *target_str = nc_string_from_cstr(resolved_target);
        scope_set(scope, target_str, val);
        nc_string_free(target_str);
        free(resolved_target);
        return NO_SIGNAL;
    }

    case NODE_EMIT: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        NcString *s = nc_value_to_string(val);
        char buf[256];
        snprintf(buf, sizeof(buf), "[EMIT] %s", s->chars);
        interp_output(interp, buf);

        /* Trigger matching event handlers */
        NcASTNode *prog = interp->program;
        for (int ev = 0; ev < prog->as.program.event_count; ev++) {
            NcASTNode *handler = prog->as.program.events[ev];
            if (handler->type == NODE_EVENT_HANDLER &&
                nc_string_equal(handler->as.event_handler.event_name, s)) {
                Scope *ev_scope = scope_new(scope);
                if (!ev_scope) continue;
                nc_map_set(ev_scope->vars, nc_string_from_cstr("event"),
                    NC_STRING(nc_string_ref(s)));
                exec_body(interp, handler->as.event_handler.body,
                    handler->as.event_handler.body_count, ev_scope);
                scope_free(ev_scope);
            }
        }

        nc_string_free(s);
        return NO_SIGNAL;
    }

    case NODE_APPLY: {
        NcMap *opts = node->as.apply.options ? node->as.apply.options : nc_map_new();
        /* Resolve variable references in option values */
        for (int oi = 0; oi < opts->count; oi++) {
            if (IS_STRING(opts->values[oi])) {
                NcString *raw = AS_STRING(opts->values[oi]);
                if (scope_has(scope, raw)) {
                    opts->values[oi] = scope_get(scope, raw);
                } else {
                    char *rv = resolve_templates(raw->chars, scope);
                    opts->values[oi] = NC_STRING(nc_string_from_cstr(rv));
                    free(rv);
                }
            }
        }
        NcString *save_name = node->as.apply.target ? node->as.apply.target : nc_string_from_cstr("apply_result");
        if (interp->mcp_handler) {
            NcValue result = interp->mcp_handler(node->as.apply.using->chars, opts);
            scope_set(scope, save_name, result);
        } else {
            NcMap *sim = nc_map_new();
            nc_map_set(sim, nc_string_from_cstr("applied"),
                       NC_STRING(nc_string_ref(node->as.apply.target)));
            nc_map_set(sim, nc_string_from_cstr("status"),
                       NC_STRING(nc_string_from_cstr("simulated")));
            scope_set(scope, save_name, NC_MAP(sim));
        }
        return NO_SIGNAL;
    }

    case NODE_CHECK: {
        NcValue result;
        if (node->as.check.using && interp->mcp_handler) {
            result = interp->mcp_handler(node->as.check.using->chars, nc_map_new());
        } else if (node->as.check.using) {
            result = nc_gather_from(node->as.check.using->chars, nc_map_new());
        } else {
            NcMap *sim = nc_map_new();
            nc_map_set(sim, nc_string_from_cstr("check"), NC_STRING(nc_string_ref(node->as.check.desc)));
            nc_map_set(sim, nc_string_from_cstr("healthy"), NC_BOOL(true));
            nc_map_set(sim, nc_string_from_cstr("note"),
                NC_STRING(nc_string_from_cstr("No check tool configured")));
            result = NC_MAP(sim);
        }
        scope_set(scope, node->as.check.save_as, result);
        return NO_SIGNAL;
    }

    case NODE_TRY: {
        bool had_error_before = interp->had_error;
        char saved_error[512] = {0};
        if (had_error_before)
            strncpy(saved_error, interp->error_msg, sizeof(saved_error) - 1);
        interp->had_error = false;
        interp->error_msg[0] = '\0';
        Signal sig = exec_body(interp, node->as.try_stmt.body, node->as.try_stmt.body_count, scope);

        if (interp->had_error) {
            bool should_handle = true;

            /* Specific error type matching (catch "TimeoutError":) */
            if (node->as.try_stmt.error_type) {
                const char *etype = node->as.try_stmt.error_type->chars;
                /* Classify error by searching for keywords in the error message */
                bool type_match = false;
                if (strcasecmp(etype, "TimeoutError") == 0 || strcasecmp(etype, "timeout") == 0)
                    type_match = (strstr(interp->error_msg, "timeout") != NULL ||
                                  strstr(interp->error_msg, "Timeout") != NULL);
                else if (strcasecmp(etype, "ValueError") == 0 || strcasecmp(etype, "value") == 0)
                    type_match = (strstr(interp->error_msg, "type") != NULL ||
                                  strstr(interp->error_msg, "convert") != NULL ||
                                  strstr(interp->error_msg, "invalid") != NULL);
                else if (strcasecmp(etype, "KeyError") == 0 || strcasecmp(etype, "key") == 0)
                    type_match = (strstr(interp->error_msg, "key") != NULL ||
                                  strstr(interp->error_msg, "not found") != NULL);
                else if (strcasecmp(etype, "ConnectionError") == 0 || strcasecmp(etype, "connection") == 0)
                    type_match = (strstr(interp->error_msg, "connect") != NULL ||
                                  strstr(interp->error_msg, "refused") != NULL ||
                                  strstr(interp->error_msg, "HTTP") != NULL);
                else if (strcasecmp(etype, "ParseError") == 0 || strcasecmp(etype, "parse") == 0)
                    type_match = (strstr(interp->error_msg, "parse") != NULL ||
                                  strstr(interp->error_msg, "JSON") != NULL ||
                                  strstr(interp->error_msg, "syntax") != NULL);
                else if (strcasecmp(etype, "IndexError") == 0 || strcasecmp(etype, "index") == 0)
                    type_match = (strstr(interp->error_msg, "index") != NULL ||
                                  strstr(interp->error_msg, "range") != NULL ||
                                  strstr(interp->error_msg, "bounds") != NULL);
                else if (strcasecmp(etype, "IOError") == 0 || strcasecmp(etype, "io") == 0)
                    type_match = (strstr(interp->error_msg, "file") != NULL ||
                                  strstr(interp->error_msg, "read") != NULL ||
                                  strstr(interp->error_msg, "write") != NULL);
                else
                    type_match = (strstr(interp->error_msg, etype) != NULL);

                should_handle = type_match;
            }

            if (should_handle && node->as.try_stmt.error_body) {
                /* Build error context map with type, message, line */
                NcMap *err_ctx = nc_map_new();
                nc_map_set(err_ctx, nc_string_from_cstr("message"),
                    NC_STRING(nc_string_from_cstr(interp->error_msg)));
                /* Classify the error type */
                const char *err_class = "Error";
                if (strstr(interp->error_msg, "timeout") || strstr(interp->error_msg, "Timeout"))
                    err_class = "TimeoutError";
                else if (strstr(interp->error_msg, "connect") || strstr(interp->error_msg, "HTTP"))
                    err_class = "ConnectionError";
                else if (strstr(interp->error_msg, "parse") || strstr(interp->error_msg, "JSON"))
                    err_class = "ParseError";
                else if (strstr(interp->error_msg, "index") || strstr(interp->error_msg, "range"))
                    err_class = "IndexError";
                else if (strstr(interp->error_msg, "type") || strstr(interp->error_msg, "convert"))
                    err_class = "ValueError";
                else if (strstr(interp->error_msg, "file") || strstr(interp->error_msg, "read"))
                    err_class = "IOError";
                else if (strstr(interp->error_msg, "key") || strstr(interp->error_msg, "not found"))
                    err_class = "KeyError";
                nc_map_set(err_ctx, nc_string_from_cstr("type"),
                    NC_STRING(nc_string_from_cstr(err_class)));
                nc_map_set(err_ctx, nc_string_from_cstr("line"), NC_INT(node->line));

                /* Set both "error" (string, backward compat) and "err" (map, rich) */
                scope_set(scope, nc_string_from_cstr("error"),
                    NC_STRING(nc_string_from_cstr(interp->error_msg)));
                scope_set(scope, nc_string_from_cstr("err"), NC_MAP(err_ctx));

                interp->had_error = false;
                interp->error_msg[0] = '\0';
                sig = exec_body(interp, node->as.try_stmt.error_body, node->as.try_stmt.error_count, scope);
            } else if (!should_handle) {
                /* Error type didn't match — leave error intact for outer handler */
            } else {
                interp->had_error = false;
                interp->error_msg[0] = '\0';
            }
        }

        if (node->as.try_stmt.finally_body && node->as.try_stmt.finally_count > 0) {
            bool err_in_handler = interp->had_error;
            interp->had_error = false;
            Signal fin = exec_body(interp, node->as.try_stmt.finally_body, node->as.try_stmt.finally_count, scope);
            if (fin.type == SIG_RESPOND) sig = fin;
            if (err_in_handler) interp->had_error = true;
        }

        if (had_error_before && !interp->had_error) {
            interp->had_error = true;
            strncpy(interp->error_msg, saved_error, sizeof(interp->error_msg) - 1);
        }
        if (sig.type == SIG_RESPOND) return sig;
        return NO_SIGNAL;
    }

    case NODE_STOP: return (Signal){SIG_STOP, NC_NONE()};
    case NODE_SKIP: return (Signal){SIG_SKIP, NC_NONE()};

    case NODE_EXPR_STMT: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        nc_value_release(val); /* discard expression result */
        return NO_SIGNAL;
    }

    case NODE_WHILE: {
        const char *limit_str = getenv("NC_MAX_ITERATIONS");
        int max_iter = limit_str ? atoi(limit_str) : 10000000;
        int iter = 0;
        while (true) {
            if (++iter > max_iter) {
                snprintf(interp->error_msg, sizeof(interp->error_msg),
                    "line %d: while loop exceeded %d iterations (set NC_MAX_ITERATIONS to increase)",
                    node->line, max_iter);
                interp->had_error = true;
                return NO_SIGNAL;
            }
            NcValue cond = eval_expr(interp, node->as.while_stmt.condition, scope);
            if (!nc_truthy(cond)) break;
            Signal sig = exec_body(interp, node->as.while_stmt.body, node->as.while_stmt.body_count, scope);
            if (sig.type == SIG_RESPOND) return sig;
            if (sig.type == SIG_STOP) break;
            if (sig.type == SIG_SKIP) continue;
        }
        return NO_SIGNAL;
    }

    case NODE_FOR_COUNT: {
        NcValue count_val = eval_expr(interp, node->as.for_count.count_expr, scope);
        int count = IS_INT(count_val) ? (int)AS_INT(count_val) : 0;
        for (int i = 0; i < count; i++) {
            if (node->as.for_count.variable)
                scope_set(scope, node->as.for_count.variable, NC_INT(i));
            Signal sig = exec_body(interp, node->as.for_count.body, node->as.for_count.body_count, scope);
            if (sig.type == SIG_RESPOND) return sig;
            if (sig.type == SIG_STOP) break;
        }
        return NO_SIGNAL;
    }

    case NODE_APPEND: {
        NcValue list = scope_get(scope, node->as.append_stmt.target);
        NcValue val = eval_expr(interp, node->as.append_stmt.value, scope);
        if (IS_LIST(list)) {
            nc_list_push(AS_LIST(list), val);
            nc_value_release(val); /* release creation ref; list retained its own */
        } else {
            /* Fallback: numeric/string addition for "add X to number" */
            if ((IS_INT(list) || IS_FLOAT(list)) && (IS_INT(val) || IS_FLOAT(val))) {
                if (IS_INT(list) && IS_INT(val))
                    scope_set(scope, node->as.append_stmt.target, NC_INT(AS_INT(list) + AS_INT(val)));
                else
                    scope_set(scope, node->as.append_stmt.target, NC_FLOAT(as_num(list) + as_num(val)));
            } else if (IS_STRING(list) || IS_STRING(val)) {
                NcString *sl = nc_value_to_string(list);
                NcString *sr = nc_value_to_string(val);
                NcString *res = nc_string_concat(sl, sr);
                scope_set(scope, node->as.append_stmt.target, NC_STRING(res));
                nc_string_free(sl); nc_string_free(sr);
            }
            nc_value_release(val);
        }
        return NO_SIGNAL;
    }

    /* ── assert <condition>, "message" ─────────────────────── */
    case NODE_ASSERT: {
        NcValue cond = eval_expr(interp, node->as.assert_stmt.condition, scope);
        if (!nc_truthy(cond)) {
            char msg[512];
            if (node->as.assert_stmt.message) {
                NcValue m = eval_expr(interp, node->as.assert_stmt.message, scope);
                NcString *ms = nc_value_to_string(m);
                snprintf(msg, sizeof(msg),
                    "Assertion failed at line %d: %s", node->line, ms->chars);
                nc_string_free(ms);
            } else {
                snprintf(msg, sizeof(msg),
                    "Assertion failed at line %d", node->line);
            }
            snprintf(interp->error_msg, sizeof(interp->error_msg), "%s", msg);
            interp->had_error = true;
            trace_print(interp);
        }
        return NO_SIGNAL;
    }

    /* ── test "name": ... body ... ─────────────────────────── */
    case NODE_TEST_BLOCK: {
        const char *name = node->as.test_block.name
            ? node->as.test_block.name->chars : "<unnamed>";

        /* Save error state */
        bool prev_error = interp->had_error;
        char prev_msg[2048];
        strncpy(prev_msg, interp->error_msg, sizeof(prev_msg) - 1);
        prev_msg[sizeof(prev_msg) - 1] = '\0';
        interp->had_error = false;
        interp->error_msg[0] = '\0';

        exec_body(interp, node->as.test_block.body,
                  node->as.test_block.body_count, scope);

        char result_buf[256];
        if (interp->had_error) {
            snprintf(result_buf, sizeof(result_buf),
                "  ✗ FAIL: %s — %s", name, interp->error_msg);
            interp_output(interp, result_buf);
            /* Restore: don't let test failure kill the whole program */
            interp->had_error = prev_error;
            strncpy(interp->error_msg, prev_msg, sizeof(interp->error_msg) - 1);
            /* Set test result variable */
            scope_set(scope, nc_string_from_cstr("_test_failed"),
                NC_BOOL(true));
        } else {
            snprintf(result_buf, sizeof(result_buf), "  ✓ PASS: %s", name);
            interp_output(interp, result_buf);
            /* Restore previous error state */
            if (prev_error) {
                interp->had_error = true;
                strncpy(interp->error_msg, prev_msg, sizeof(interp->error_msg) - 1);
            }
        }
        return NO_SIGNAL;
    }

    /* ── await <expr> — synchronous wait on async result ──── */
    case NODE_AWAIT: {
        NcValue val = eval_expr(interp, node->as.single_expr.value, scope);
        /* For now, await is pass-through (futures resolved synchronously).
         * This establishes the syntax for when real async is added. */
        scope_set(scope, nc_string_from_cstr("_await_result"), val);
        return NO_SIGNAL;
    }

    /* ── yield <value> — emit value for streaming/generators ─ */
    case NODE_YIELD: {
        NcValue val = eval_expr(interp, node->as.yield_stmt.value, scope);
        /* Push to yield accumulator list if one exists */
        NcValue yields = scope_get(scope, nc_string_from_cstr("_yields"));
        if (IS_LIST(yields)) {
            nc_list_push(AS_LIST(yields), val);
        } else {
            NcList *yl = nc_list_new();
            nc_list_push(yl, val);
            scope_set(scope, nc_string_from_cstr("_yields"), NC_LIST(yl));
        }
        /* Also print if streaming to console */
        NcString *vs = nc_value_to_string(val);
        char buf[2048];
        snprintf(buf, sizeof(buf), "%s", vs->chars);
        interp_output(interp, buf);
        nc_string_free(vs);
        return NO_SIGNAL;
    }

    /* ── stream respond — SSE streaming response ─────────── */
    case NODE_STREAM_RESPOND: {
        NcValue val = eval_expr(interp, node->as.stream_respond.value, scope);
        /* Build SSE event frame */
        NcString *vs = nc_value_to_string(val);
        const char *event_type = node->as.stream_respond.event_type
            ? node->as.stream_respond.event_type->chars : "message";
        char sse_buf[4096];
        snprintf(sse_buf, sizeof(sse_buf), "event: %s\ndata: %s\n\n",
                 event_type, vs->chars);
        nc_string_free(vs);
        /* Set SSE frame for server to pick up */
        scope_set(scope, nc_string_from_cstr("_sse_frame"),
            NC_STRING(nc_string_from_cstr(sse_buf)));
        return (Signal){SIG_RESPOND, NC_STRING(nc_string_from_cstr(sse_buf))};
    }

    default: return NO_SIGNAL;
    }
}

static Signal exec_body(NCInterp *interp, NcASTNode **stmts, int count, Scope *scope) {
    for (int i = 0; i < count; i++) {
        if (nc_deadline_exceeded()) {
            interp->had_error = true;
            snprintf(interp->error_msg, sizeof(interp->error_msg),
                "Request timeout exceeded");
            return (Signal){SIG_RESPOND,
                NC_MAP(({
                    NcMap *e = nc_map_new();
                    nc_map_set(e, nc_string_from_cstr("error"),
                        NC_STRING(nc_string_from_cstr("Request timeout exceeded")));
                    nc_map_set(e, nc_string_from_cstr("_status"), NC_INT(504));
                    e;
                }))};
        }
        Signal sig = exec_stmt(interp, stmts[i], scope);
        if (sig.type != SIG_NONE) return sig;
    }
    return NO_SIGNAL;
}

/* ── Public API ────────────────────────────────────────────── */

int nc_run_source(const char *source, const char *filename) {
    /* Lex */
    NcLexer *lex = nc_lexer_new(source, filename);
    nc_lexer_tokenize(lex);

    /* Parse */
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
    NcASTNode *program = nc_parser_parse(parser);

    if (parser->had_error) {
        fprintf(stderr, "[NC Error] %s\n", parser->error_msg);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        return 1;
    }

    /* Version compatibility check — if the .nc file declares a version,
     * verify this runtime can execute it. Supports semver major.minor. */
    if (program->as.program.version) {
        const char *file_ver = program->as.program.version->chars;
        int file_major = 0, file_minor = 0;
        sscanf(file_ver, "%d.%d", &file_major, &file_minor);
        int rt_major = NC_VERSION_MAJOR, rt_minor = NC_VERSION_MINOR;
        if (file_major > rt_major || (file_major == rt_major && file_minor > rt_minor)) {
            fprintf(stderr, "[NC] Error: This file requires NC v%s but runtime is v%d.%d.0\n"
                            "  Upgrade: Visit " NC_INSTALL_URL "\n",
                    file_ver, rt_major, rt_minor);
            nc_parser_free(parser);
            nc_lexer_free(lex);
            return 1;
        }
    }

    /* Set up interpreter */
    NCInterp interp = {0};
    interp.program = program;
    interp.global = scope_new(NULL);
    if (!interp.global) {
        fprintf(stderr, "Error: Out of memory\n");
        nc_parser_free(parser); nc_lexer_free(lex);
        return 1;
    }
    interp.output_cap = 64;
    interp.output = calloc(interp.output_cap, sizeof(char *));
    if (!interp.output) {
        scope_free(interp.global);
        nc_parser_free(parser); nc_lexer_free(lex);
        return 1;
    }

    /* Process imports — load referenced modules */
    for (int i = 0; i < program->as.program.import_count; i++) {
        NcASTNode *imp = program->as.program.imports[i];
        const char *mod_name = imp->as.import_decl.module->chars;

        NcValue stdlib_mod = nc_module_get_stdlib(mod_name);
        if (!IS_NONE(stdlib_mod)) {
            NcString *alias = imp->as.import_decl.alias ?
                imp->as.import_decl.alias : imp->as.import_decl.module;
            scope_set(interp.global, alias, stdlib_mod);
            continue;
        }
    }

    /* Print service info */
    printf("\n");
    printf("  \033[36m╔══════════════════════════════════════════════════╗\033[0m\n");
    if (program->as.program.service_name)
        printf("  \033[36m║\033[0m  \033[1m\033[36m%-48s\033[0m\033[36m║\033[0m\n", program->as.program.service_name->chars);
    printf("  \033[36m╠══════════════════════════════════════════════════╣\033[0m\n");
    if (program->as.program.version)
        printf("  \033[36m║\033[0m  \033[1mVersion:\033[0m  \033[33m%-38s\033[0m\033[36m║\033[0m\n", program->as.program.version->chars);
    if (program->as.program.model)
        printf("  \033[36m║\033[0m  \033[1mModel:\033[0m    %-38s\033[36m║\033[0m\n", program->as.program.model->chars);
    printf("  \033[36m║\033[0m  \033[1mRuntime:\033[0m  \033[90mNC (Notation-as-Code) v1.0.0       \033[0m\033[36m║\033[0m\n");
    printf("  \033[36m╚══════════════════════════════════════════════════╝\033[0m\n");

    if (program->as.program.import_count > 0) {
        printf("\n  Imports: ");
        for (int i = 0; i < program->as.program.import_count; i++) {
            if (i > 0) printf(", ");
            NcASTNode *imp = program->as.program.imports[i];
            printf("%s", imp->as.import_decl.module->chars);
            if (imp->as.import_decl.alias)
                printf(" as %s", imp->as.import_decl.alias->chars);
        }
        printf("\n");
    }

    if (program->as.program.def_count > 0) {
        printf("\n  Types: ");
        for (int i = 0; i < program->as.program.def_count; i++) {
            if (i > 0) printf(", ");
            printf("%s", program->as.program.definitions[i]->as.definition.name->chars);
        }
        printf("\n");
    }

    if (program->as.program.mw_count > 0) {
        printf("\n  Middleware: ");
        for (int i = 0; i < program->as.program.mw_count; i++) {
            if (i > 0) printf(", ");
            printf("%s", program->as.program.middleware[i]->as.middleware.name->chars);
        }
        printf("\n");
    }

    printf("  Behaviors: ");
    for (int i = 0; i < program->as.program.beh_count; i++) {
        if (i > 0) printf(", ");
        printf("%s", program->as.program.behaviors[i]->as.behavior.name->chars);
    }
    printf("\n");

    if (program->as.program.route_count > 0) {
        printf("\n  API Routes:\n");
        for (int i = 0; i < program->as.program.route_count; i++) {
            NcASTNode *r = program->as.program.routes[i];
            printf("    %-6s %s -> %s\n",
                   r->as.route.method->chars,
                   r->as.route.path->chars,
                   r->as.route.handler->chars);
        }
    }

    /* Execute all behaviors that are not called by others (auto-run) */
    /* For now just show parsed info; use -b flag to run a behavior */
    printf("\n");

    /* Cleanup */
    scope_free(interp.global);
    for (int i = 0; i < interp.output_count; i++) free(interp.output[i]);
    free(interp.output);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    return 0;
}

int nc_run_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Error: Cannot open '%s'\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return 1; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); fprintf(stderr, "Error: Out of memory\n"); return 1; }
    size_t nread = fread(buf, 1, size, f);
    buf[nread] = '\0';
    fclose(f);
    if (nread == 0) { free(buf); fprintf(stderr, "Error: Empty file '%s'\n", filename); return 1; }
    int result = nc_run_source(buf, filename);
    free(buf);
    return result;
}

/* ── AST Cache — parse once, reuse across requests ─────── */

typedef struct {
    char        filename[512];
    uint32_t    source_hash;
    NcASTNode  *program;
    NcLexer    *lex;
    NcParser   *parser;
} NcASTCacheEntry;

#define NC_AST_CACHE_SIZE 16
static NcASTCacheEntry ast_cache[NC_AST_CACHE_SIZE];
static int ast_cache_count = 0;

void nc_ast_cache_flush(void) {
    for (int i = 0; i < ast_cache_count; i++) {
        nc_ast_free(ast_cache[i].program);
        nc_parser_free(ast_cache[i].parser);
        nc_lexer_free(ast_cache[i].lex);
    }
    ast_cache_count = 0;
}

/* Server-wide persistent scope — survives across requests */
static NcMap *nc_server_globals = NULL;

void nc_server_globals_set(NcMap *globals) {
    nc_server_globals = globals;
}

NcMap *nc_server_globals_get(void) {
    return nc_server_globals;
}

static uint32_t nc_hash_source(const char *s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h;
}

/* Call a specific behavior with args */
NcValue nc_call_behavior(const char *source, const char *filename,
                          const char *behavior_name, NcMap *args) {
    /* Check AST cache — skip re-parsing if source unchanged */
    uint32_t src_hash = nc_hash_source(source);
    NcASTNode *program = NULL;
    bool cached = false;

    for (int i = 0; i < ast_cache_count; i++) {
        if (ast_cache[i].source_hash == src_hash &&
            strcmp(ast_cache[i].filename, filename) == 0) {
            program = ast_cache[i].program;
            cached = true;
            break;
        }
    }

    NcLexer *lex = NULL;
    NcParser *parser = NULL;

    if (!cached) {
        lex = nc_lexer_new(source, filename);
        nc_lexer_tokenize(lex);
        parser = nc_parser_new(lex->tokens, lex->token_count, filename);
        program = nc_parser_parse(parser);

        if (parser->had_error) {
            NcMap *err = nc_map_new();
            nc_map_set(err, nc_string_from_cstr("error"),
                NC_STRING(nc_string_from_cstr(parser->error_msg)));
            nc_parser_free(parser);
            nc_lexer_free(lex);
            return NC_MAP(err);
        }

        /* Store in cache */
        if (ast_cache_count < NC_AST_CACHE_SIZE) {
            NcASTCacheEntry *ce = &ast_cache[ast_cache_count++];
            strncpy(ce->filename, filename, 511);
            ce->source_hash = src_hash;
            ce->program = program;
            ce->lex = lex;
            ce->parser = parser;
            cached = true;
        }
    }

    NCInterp interp = {0};
    interp.program = program;
    interp.global = scope_new(NULL);
    if (!interp.global) {
        if (!cached) { nc_parser_free(parser); nc_lexer_free(lex); }
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Out of memory")));
        return NC_MAP(err);
    }
    interp.output_cap = 64;
    interp.output = calloc(interp.output_cap, sizeof(char *));
    if (!interp.output) {
        scope_free(interp.global);
        if (!cached) { nc_parser_free(parser); nc_lexer_free(lex); }
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Out of memory")));
        return NC_MAP(err);
    }

    /* Inject server-wide persistent state if available */
    if (nc_server_globals) {
        for (int i = 0; i < nc_server_globals->count; i++) {
            scope_set(interp.global, nc_server_globals->keys[i],
                      nc_server_globals->values[i]);
        }
    }

    /* Process imports — stdlib first, then .nc files */
    for (int i = 0; i < program->as.program.import_count; i++) {
        NcASTNode *imp = program->as.program.imports[i];
        const char *mod_name = imp->as.import_decl.module->chars;

        NcValue stdlib_mod = nc_module_get_stdlib(mod_name);
        if (!IS_NONE(stdlib_mod)) {
            NcString *alias = imp->as.import_decl.alias ?
                imp->as.import_decl.alias : imp->as.import_decl.module;
            scope_set(interp.global, alias, stdlib_mod);
            continue;
        }

        /* Load .nc file — behaviors from imported module become callable */
        NcASTNode *mod_ast = nc_module_load_file(mod_name, filename);
        if (mod_ast) {
            for (int b = 0; b < mod_ast->as.program.beh_count; b++) {
                NcASTNode *beh = mod_ast->as.program.behaviors[b];
                /* Add to this program's behaviors so `run` can find them */
                if (program->as.program.beh_count < 64) {
                    program->as.program.behaviors[program->as.program.beh_count++] = beh;
                }
            }
        }
    }

    /* Make configure block available as 'config' variable with env: resolution.
     * Auto-converts numeric env values so port: "env:PORT" with PORT=8080
     * yields config.port == 8080 (int), not "8080" (string). */
    if (program->as.program.configure) {
        NcMap *cfg = program->as.program.configure;
        for (int i = 0; i < cfg->count; i++) {
            if (IS_STRING(cfg->values[i])) {
                const char *val = AS_STRING(cfg->values[i])->chars;
                if (strncmp(val, "env:", 4) == 0) {
                    const char *env_val = getenv(val + 4);
                    if (env_val) {
                        char *end = NULL;
                        long long iv = strtoll(env_val, &end, 10);
                        if (end && *end == '\0' && end != env_val) {
                            cfg->values[i] = NC_INT((int64_t)iv);
                        } else {
                            double fv = strtod(env_val, &end);
                            if (end && *end == '\0' && end != env_val) {
                                cfg->values[i] = NC_FLOAT(fv);
                            } else if (strcmp(env_val, "true") == 0 || strcmp(env_val, "yes") == 0) {
                                cfg->values[i] = NC_BOOL(true);
                            } else if (strcmp(env_val, "false") == 0 || strcmp(env_val, "no") == 0) {
                                cfg->values[i] = NC_BOOL(false);
                            } else {
                                cfg->values[i] = NC_STRING(nc_string_from_cstr(env_val));
                            }
                        }
                    }
                }
            }
        }
        /* Map configure keys to NC_ env vars so the AI engine and other
         * subsystems can find them. User writes ai_url → becomes NC_AI_URL.
         * This lets users use any env var name they want via env: prefix
         * while the engine looks up NC_AI_URL internally. */
        for (int i = 0; i < cfg->count; i++) {
            if (IS_STRING(cfg->values[i]) || IS_INT(cfg->values[i])) {
                const char *key = cfg->keys[i]->chars;
                char env_name[128] = "NC_";
                int ei = 3;
                for (int k = 0; key[k] && ei < 126; k++) {
                    env_name[ei++] = (key[k] >= 'a' && key[k] <= 'z')
                        ? key[k] - 32 : key[k];
                }
                env_name[ei] = '\0';
                if (!getenv(env_name)) {
                    NcString *vs = nc_value_to_string(cfg->values[i]);
                    nc_setenv(env_name, vs->chars, 0);
                    nc_string_free(vs);
                }
            }
        }
        NcString *config_key = nc_string_from_cstr("config");
        scope_set(interp.global, config_key, NC_MAP(cfg));
        nc_string_free(config_key);
    }

    NcValue result = NC_NONE();

    for (int b = 0; b < program->as.program.beh_count; b++) {
        NcASTNode *beh = program->as.program.behaviors[b];
        if (strcmp(beh->as.behavior.name->chars, behavior_name) == 0) {
            Scope *scope = scope_new(interp.global);
            if (!scope) break;
            scope->is_function = true;  /* isolate behavior scope */

            if (args) {
                /* Named binding: match args keys to param names.
                 * Primary: nc_map_get (fast, hash-based).
                 * Fallback: linear char-by-char scan if hash-based lookup
                 * fails — this handles edge cases where string interning
                 * produced different hashes for the same text (e.g. race
                 * conditions on Windows with broken mutex init). */
                for (int i = 0; i < beh->as.behavior.param_count; i++) {
                    NcString *param_name = beh->as.behavior.params[i]->as.param.name;
                    NcValue val = nc_map_get(args, param_name);
                    if (IS_NONE(val)) {
                        for (int k = 0; k < args->count; k++) {
                            if (args->keys[k]->length == param_name->length &&
                                memcmp(args->keys[k]->chars, param_name->chars,
                                       param_name->length) == 0) {
                                val = args->values[k];
                                break;
                            }
                        }
                    }
                    /* Always bind the param in local scope — even if missing from args,
                     * set it to none so it doesn't fall through to global scope
                     * (which would resolve to the service config). */
                    nc_map_set(scope->vars, param_name, val);
                }
                /* Also bind all args keys directly so behaviors can
                 * access request fields not declared as params */
                for (int i = 0; i < args->count; i++) {
                    if (!nc_map_has(scope->vars, args->keys[i]))
                        nc_map_set(scope->vars, args->keys[i], args->values[i]);
                }
            }

            interp.current_file = filename;
            trace_push(&interp, behavior_name, beh->line);
            Signal sig = exec_body(&interp, beh->as.behavior.body,
                                    beh->as.behavior.body_count, scope);
            trace_pop(&interp);
            if (sig.type == SIG_RESPOND) {
                result = sig.value;
                nc_value_retain(result);
            }

            /* Print output */
            for (int i = 0; i < interp.output_count; i++) {
                printf("%s\n", interp.output[i]);
            }

            /* Sync variables back to args (for REPL state persistence) */
            if (args && scope->vars) {
                for (int i = 0; i < scope->vars->count; i++) {
                    nc_map_set(args, scope->vars->keys[i], scope->vars->values[i]);
                }
            }

            scope_free(scope);
            break;
        }
    }

    if (nc_server_globals && interp.global && interp.global->vars) {
        for (int i = 0; i < interp.global->vars->count; i++) {
            nc_map_set(nc_server_globals, interp.global->vars->keys[i],
                      interp.global->vars->values[i]);
        }
    }

    /* Full cleanup — reset all interpreter state to prevent leaks
     * across sequential nc_call_behavior invocations */
    scope_free(interp.global);
    for (int i = 0; i < interp.output_count; i++) free(interp.output[i]);
    free(interp.output);
    interp.global = NULL;
    interp.output = NULL;
    interp.output_count = 0;
    eval_depth = 0;
    if (!cached) {
        nc_parser_free(parser);
        nc_lexer_free(lex);
    }
    return result;
}
