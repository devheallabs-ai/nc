/*
 * nc_compiler.c — Complete bytecode compiler for NC.
 *
 * Compiles the full AST into bytecodes that the VM executes.
 *
 * Pipeline:  Source → Lexer → Parser → AST → [Compiler] → Bytecode → VM
 *
 * Every NC statement and expression is compiled to a sequence of
 * VM opcodes. The VM is a stack machine: operands are pushed onto
 * the stack, operations pop them and push results.
 */

#include "../include/nc.h"

typedef struct {
    NcChunk *chunk;
    bool     had_error;
    char     error_msg[2048];
    int      scope_depth;
} Compiler;

/* ── Emit helpers ──────────────────────────────────────────── */

static void emit_byte(Compiler *c, uint8_t byte, int line) {
    nc_chunk_write(c->chunk, byte, line);
}

static void emit_bytes(Compiler *c, uint8_t b1, uint8_t b2, int line) {
    nc_chunk_write(c->chunk, b1, line);
    nc_chunk_write(c->chunk, b2, line);
}

static int emit_jump(Compiler *c, uint8_t op, int line) {
    emit_byte(c, op, line);
    emit_byte(c, 0xFF, line);
    emit_byte(c, 0xFF, line);
    return c->chunk->count - 2;
}

static void patch_jump(Compiler *c, int offset) {
    if (offset < 0 || offset + 1 >= c->chunk->capacity) {
        snprintf(c->error_msg, sizeof(c->error_msg), "Internal error: jump offset out of bounds.");
        c->had_error = true;
        return;
    }
    int jump = c->chunk->count - offset - 2;
    if (jump > 0xFFFF) {
        snprintf(c->error_msg, sizeof(c->error_msg), "This behavior is too long. Split it into smaller steps.");
        c->had_error = true;
        return;
    }
    c->chunk->code[offset] = (jump >> 8) & 0xFF;
    c->chunk->code[offset + 1] = jump & 0xFF;
}

static int emit_loop_start(Compiler *c) {
    return c->chunk->count;
}

static void emit_loop(Compiler *c, int loop_start, int line) {
    emit_byte(c, OP_LOOP, line);
    int offset = c->chunk->count - loop_start + 2;
    emit_byte(c, (offset >> 8) & 0xFF, line);
    emit_byte(c, offset & 0xFF, line);
}

static int make_constant_idx(Compiler *c, NcValue val) {
    if (c->chunk->const_count >= 65535) {
        snprintf(c->error_msg, sizeof(c->error_msg), "This behavior has too many values. Simplify the logic or split it into smaller behaviors.");
        c->had_error = true;
        return 0;
    }
    int idx = nc_chunk_add_constant(c->chunk, val);
    if (idx < 0) {
        snprintf(c->error_msg, sizeof(c->error_msg), "Internal error: failed to allocate constant.");
        c->had_error = true;
        return 0;
    }
    return idx;
}

static uint8_t make_constant(Compiler *c, NcValue val) {
    int idx = make_constant_idx(c, val);
    if (idx > 255 && !c->had_error) {
        snprintf(c->error_msg, sizeof(c->error_msg),
                 "Too many constants in one behavior (max 256 for short encoding). Split into smaller behaviors.");
        c->had_error = true;
        return 0;
    }
    return (uint8_t)idx;
}

static int make_var_idx(Compiler *c, NcString *name) {
    if (c->chunk->var_count >= 65535) {
        snprintf(c->error_msg, sizeof(c->error_msg), "Too many variables (max 65536 per behavior)");
        c->had_error = true;
        return 0;
    }
    int idx = nc_chunk_add_var(c->chunk, name);
    if (idx < 0) {
        snprintf(c->error_msg, sizeof(c->error_msg), "Internal error: failed to allocate variable name.");
        c->had_error = true;
        return 0;
    }
    return idx;
}

static uint8_t make_var(Compiler *c, NcString *name) {
    int idx = make_var_idx(c, name);
    if (idx > 255 && !c->had_error) {
        snprintf(c->error_msg, sizeof(c->error_msg),
                 "Too many variable names in one behavior (max 256 for short encoding). Split into smaller behaviors.");
        c->had_error = true;
        return 0;
    }
    return (uint8_t)idx;
}

static void emit_constant(Compiler *c, NcValue val, int line) {
    int idx = make_constant_idx(c, val);
    if (idx <= 255) {
        emit_bytes(c, OP_CONSTANT, (uint8_t)idx, line);
    } else {
        emit_byte(c, OP_CONSTANT_LONG, line);
        emit_byte(c, (idx >> 8) & 0xFF, line);
        emit_byte(c, idx & 0xFF, line);
    }
}

static void emit_string(Compiler *c, NcString *s, int line) {
    emit_constant(c, NC_STRING(nc_string_ref(s)), line);
}

static void emit_get_var(Compiler *c, NcString *name, int line) {
    emit_bytes(c, OP_GET_VAR, make_var(c, name), line);
}

static void emit_set_var(Compiler *c, NcString *name, int line) {
    emit_bytes(c, OP_SET_VAR, make_var(c, name), line);
}

/* ── Forward declarations ──────────────────────────────────── */

static void compile_expr(Compiler *c, NcASTNode *node);
static void compile_stmt(Compiler *c, NcASTNode *node);
static void compile_body(Compiler *c, NcASTNode **stmts, int count);

static void compile_run_call(Compiler *c, NcASTNode *node) {
    emit_string(c, node->as.run_stmt.name, node->line);
    for (int i = 0; i < node->as.run_stmt.arg_count; i++)
        compile_expr(c, node->as.run_stmt.args[i]);
    emit_bytes(c, OP_CALL, (uint8_t)node->as.run_stmt.arg_count, node->line);
}

/* ═══════════════════════════════════════════════════════════
 *  Expression compilation  (AST → stack ops)
 * ═══════════════════════════════════════════════════════════ */

static void compile_expr(Compiler *c, NcASTNode *node) {
    if (!node || c->had_error) return;

    switch (node->type) {
    case NODE_INT_LIT:
        emit_constant(c, NC_INT(node->as.int_lit.value), node->line);
        break;

    case NODE_FLOAT_LIT:
        emit_constant(c, NC_FLOAT(node->as.float_lit.value), node->line);
        break;

    case NODE_STRING_LIT:
        emit_string(c, node->as.string_lit.value, node->line);
        break;

    case NODE_BOOL_LIT:
        emit_byte(c, node->as.bool_lit.value ? OP_TRUE : OP_FALSE, node->line);
        break;

    case NODE_NONE_LIT:
        emit_byte(c, OP_NONE, node->line);
        break;

    case NODE_TEMPLATE:
        emit_string(c, node->as.template_lit.expr, node->line);
        break;

    case NODE_IDENT:
        emit_get_var(c, node->as.ident.name, node->line);
        break;

    case NODE_DOT:
        compile_expr(c, node->as.dot.object);
        emit_bytes(c, OP_GET_FIELD, make_var(c, node->as.dot.member), node->line);
        break;

    case NODE_MATH:
        compile_expr(c, node->as.math.left);
        compile_expr(c, node->as.math.right);
        switch (node->as.math.op) {
            case '+': emit_byte(c, OP_ADD, node->line); break;
            case '-': emit_byte(c, OP_SUBTRACT, node->line); break;
            case '*': emit_byte(c, OP_MULTIPLY, node->line); break;
            case '/': emit_byte(c, OP_DIVIDE, node->line); break;
            case '%': emit_byte(c, OP_MODULO, node->line); break;
        }
        break;

    case NODE_COMPARISON: {
        compile_expr(c, node->as.comparison.left);
        compile_expr(c, node->as.comparison.right);
        const char *op = node->as.comparison.op->chars;
        if (strcmp(op, "above") == 0)       emit_byte(c, OP_ABOVE, node->line);
        else if (strcmp(op, "below") == 0)  emit_byte(c, OP_BELOW, node->line);
        else if (strcmp(op, "equal") == 0)  emit_byte(c, OP_EQUAL, node->line);
        else if (strcmp(op, "not_equal") == 0) emit_byte(c, OP_NOT_EQUAL, node->line);
        else if (strcmp(op, "at_least") == 0) emit_byte(c, OP_AT_LEAST, node->line);
        else if (strcmp(op, "at_most") == 0)  emit_byte(c, OP_AT_MOST, node->line);
        else if (strcmp(op, "in") == 0)       emit_byte(c, OP_IN, node->line);
        else if (strcmp(op, "not_in") == 0)   { emit_byte(c, OP_IN, node->line); emit_byte(c, OP_NOT, node->line); }
        break;
    }

    case NODE_LOGIC: {
        compile_expr(c, node->as.logic.left);
        compile_expr(c, node->as.logic.right);
        if (strcmp(node->as.logic.op->chars, "and") == 0)
            emit_byte(c, OP_AND, node->line);
        else
            emit_byte(c, OP_OR, node->line);
        break;
    }

    case NODE_NOT:
        compile_expr(c, node->as.logic.left);
        emit_byte(c, OP_NOT, node->line);
        break;

    case NODE_LIST_LIT:
        for (int i = 0; i < node->as.list_lit.count; i++)
            compile_expr(c, node->as.list_lit.elements[i]);
        emit_bytes(c, OP_MAKE_LIST, (uint8_t)node->as.list_lit.count, node->line);
        break;

    case NODE_MAP_LIT: {
        for (int i = 0; i < node->as.map_lit.count; i++) {
            emit_string(c, node->as.map_lit.keys[i], node->line);
            compile_expr(c, node->as.map_lit.values[i]);
        }
        emit_bytes(c, OP_MAKE_MAP, (uint8_t)node->as.map_lit.count, node->line);
        break;
    }

    case NODE_CALL: {
        /* Push function name as string constant (not variable lookup) */
        emit_string(c, node->as.call.name, node->line);
        for (int i = 0; i < node->as.call.arg_count; i++)
            compile_expr(c, node->as.call.args[i]);
        emit_bytes(c, OP_CALL_NATIVE, (uint8_t)node->as.call.arg_count, node->line);
        break;
    }

    case NODE_RUN:
        compile_run_call(c, node);
        break;

    case NODE_INDEX:
        compile_expr(c, node->as.math.left);
        compile_expr(c, node->as.math.right);
        emit_byte(c, OP_GET_INDEX, node->line);
        break;

    case NODE_SLICE:
        compile_expr(c, node->as.slice.object);
        if (node->as.slice.start) compile_expr(c, node->as.slice.start);
        else emit_byte(c, OP_NONE, node->line);
        if (node->as.slice.end) compile_expr(c, node->as.slice.end);
        else emit_byte(c, OP_NONE, node->line);
        emit_byte(c, OP_SLICE, node->line);
        break;

    default:
        emit_byte(c, OP_NONE, node->line);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Statement compilation
 * ═══════════════════════════════════════════════════════════ */

static void compile_stmt(Compiler *c, NcASTNode *node) {
    if (!node || c->had_error) return;

    switch (node->type) {

    /* ── set x to value / set x.field to value ──────────── */
    case NODE_SET:
        if (node->as.set_stmt.field && node->as.set_stmt.subfield) {
            /* 2-level deep set: set x.a.b to val → get x, get_field a, push val, set_field b */
            emit_get_var(c, node->as.set_stmt.target, node->line);
            emit_bytes(c, OP_GET_FIELD, make_var(c, node->as.set_stmt.field), node->line);
            compile_expr(c, node->as.set_stmt.value);
            emit_bytes(c, OP_SET_FIELD, make_var(c, node->as.set_stmt.subfield), node->line);
            emit_byte(c, OP_POP, node->line);
        } else if (node->as.set_stmt.field) {
            emit_get_var(c, node->as.set_stmt.target, node->line);
            compile_expr(c, node->as.set_stmt.value);
            emit_bytes(c, OP_SET_FIELD, make_var(c, node->as.set_stmt.field), node->line);
            emit_byte(c, OP_POP, node->line);
        } else {
            compile_expr(c, node->as.set_stmt.value);
            emit_set_var(c, node->as.set_stmt.target, node->line);
            emit_byte(c, OP_POP, node->line);
        }
        break;

    /* ── set map[key] to value / set map.field[key] to value ── */
    case NODE_SET_INDEX: {
        compile_expr(c, node->as.set_index.value);
        compile_expr(c, node->as.set_index.index);
        if (node->as.set_index.field) {
            emit_get_var(c, node->as.set_index.target, node->line);
            emit_bytes(c, OP_GET_FIELD, make_var(c, node->as.set_index.field), node->line);
            emit_byte(c, OP_SET_INDEX, node->line);
            emit_byte(c, OP_POP, node->line);
        } else {
            emit_get_var(c, node->as.set_index.target, node->line);
            emit_byte(c, OP_SET_INDEX, node->line);
            emit_byte(c, OP_POP, node->line);
        }
        break;
    }

    /* ── respond with value ────────────────────────────── */
    case NODE_RESPOND:
        compile_expr(c, node->as.single_expr.value);
        emit_byte(c, OP_RESPOND, node->line);
        break;

    /* ── log "message" ─────────────────────────────────── */
    case NODE_LOG:
        compile_expr(c, node->as.single_expr.value);
        emit_byte(c, OP_LOG, node->line);
        break;

    /* ── show value ────────────────────────────────────── */
    case NODE_SHOW:
        compile_expr(c, node->as.single_expr.value);
        emit_byte(c, OP_LOG, node->line);  /* show = log */
        break;

    /* ── emit "event" ──────────────────────────────────── */
    case NODE_EMIT:
        compile_expr(c, node->as.single_expr.value);
        emit_byte(c, OP_EMIT, node->line);
        break;

    /* ── notify channel "message" ──────────────────────── */
    case NODE_NOTIFY:
        compile_expr(c, node->as.notify.channel);
        if (node->as.notify.message)
            compile_expr(c, node->as.notify.message);
        else
            emit_byte(c, OP_NONE, node->line);
        emit_byte(c, OP_NOTIFY, node->line);
        break;

    /* ── wait 30 seconds ───────────────────────────────── */
    case NODE_WAIT:
        emit_constant(c, NC_FLOAT(node->as.wait_stmt.amount), node->line);
        emit_byte(c, OP_WAIT, node->line);
        break;

    /* ── gather target from source ─────────────────────── */
    case NODE_GATHER: {
        emit_string(c, node->as.gather.source, node->line);
        if (node->as.gather.options) {
            uint8_t oi = make_constant(c, NC_MAP(node->as.gather.options));
            emit_bytes(c, OP_CONSTANT, oi, node->line);
        } else {
            emit_byte(c, OP_NONE, node->line);
        }
        emit_byte(c, OP_GATHER, node->line);
        emit_set_var(c, node->as.gather.target, node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── ask AI to "prompt" ────────────────────────────── */
    case NODE_ASK_AI: {
        emit_string(c, node->as.ask_ai.prompt, node->line);
        /* Build context map from 'using' names */
        for (int i = 0; i < node->as.ask_ai.using_count; i++)
            emit_get_var(c, node->as.ask_ai.using[i], node->line);
        emit_bytes(c, OP_MAKE_LIST, (uint8_t)node->as.ask_ai.using_count, node->line);
        emit_byte(c, OP_ASK_AI, node->line);
        NcString *save = node->as.ask_ai.save_as ?
            node->as.ask_ai.save_as : nc_string_from_cstr("result");
        emit_set_var(c, save, node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── if condition: ... otherwise: ... ──────────────── */
    case NODE_IF: {
        compile_expr(c, node->as.if_stmt.condition);
        int then_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
        emit_byte(c, OP_POP, node->line);

        compile_body(c, node->as.if_stmt.then_body, node->as.if_stmt.then_count);

        int else_jump = emit_jump(c, OP_JUMP, node->line);
        patch_jump(c, then_jump);
        emit_byte(c, OP_POP, node->line);

        if (node->as.if_stmt.else_body)
            compile_body(c, node->as.if_stmt.else_body, node->as.if_stmt.else_count);

        patch_jump(c, else_jump);
        break;
    }

    /* ── repeat for each item in list: ─────────────────── */
    case NODE_REPEAT: {
        compile_expr(c, node->as.repeat.iterable);
        uint8_t list_var = make_var(c, nc_string_from_cstr("__iter_list__"));
        emit_set_var(c, nc_string_from_cstr("__iter_list__"), node->line);
        emit_byte(c, OP_POP, node->line);

        emit_constant(c, NC_INT(0), node->line);
        uint8_t idx_var = make_var(c, nc_string_from_cstr("__iter_idx__"));
        emit_set_var(c, nc_string_from_cstr("__iter_idx__"), node->line);
        emit_byte(c, OP_POP, node->line);

        int loop_start_pos = emit_loop_start(c);

        /* Check: idx < len(list) */
        emit_bytes(c, OP_GET_VAR, idx_var, node->line);
        emit_bytes(c, OP_GET_VAR, list_var, node->line);
        emit_byte(c, OP_BELOW, node->line);  /* uses list's count as a number */

        int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
        emit_byte(c, OP_POP, node->line);

        /* Load current item: list[idx] */
        emit_bytes(c, OP_GET_VAR, list_var, node->line);
        emit_bytes(c, OP_GET_VAR, idx_var, node->line);
        emit_byte(c, OP_GET_INDEX, node->line);
        emit_set_var(c, node->as.repeat.variable, node->line);
        emit_byte(c, OP_POP, node->line);

        /* Compile loop body */
        compile_body(c, node->as.repeat.body, node->as.repeat.body_count);

        /* Increment index: idx = idx + 1 */
        emit_bytes(c, OP_GET_VAR, idx_var, node->line);
        emit_constant(c, NC_INT(1), node->line);
        emit_byte(c, OP_ADD, node->line);
        emit_bytes(c, OP_SET_VAR, idx_var, node->line);
        emit_byte(c, OP_POP, node->line);

        emit_loop(c, loop_start_pos, node->line);

        patch_jump(c, exit_jump);
        emit_byte(c, OP_POP, node->line);
        (void)list_var;
        break;
    }

    /* ── match subject: when "x": ... ──────────────────── */
    case NODE_MATCH: {
        compile_expr(c, node->as.match_stmt.subject);
        uint8_t subj_var = make_var(c, nc_string_from_cstr("__match_subj__"));
        emit_set_var(c, nc_string_from_cstr("__match_subj__"), node->line);
        emit_byte(c, OP_POP, node->line);

        int *end_jumps = calloc(node->as.match_stmt.case_count + 1, sizeof(int));
        if (!end_jumps) { c->had_error = true; break; }
        int ej_count = 0;

        for (int i = 0; i < node->as.match_stmt.case_count; i++) {
            NcASTNode *when = node->as.match_stmt.cases[i];
            emit_bytes(c, OP_GET_VAR, subj_var, when->line);
            compile_expr(c, when->as.when_clause.value);
            emit_byte(c, OP_EQUAL, when->line);

            int skip = emit_jump(c, OP_JUMP_IF_FALSE, when->line);
            emit_byte(c, OP_POP, when->line);
            compile_body(c, when->as.when_clause.body, when->as.when_clause.body_count);
            end_jumps[ej_count++] = emit_jump(c, OP_JUMP, when->line);
            patch_jump(c, skip);
            emit_byte(c, OP_POP, when->line);
        }

        if (node->as.match_stmt.otherwise)
            compile_body(c, node->as.match_stmt.otherwise, node->as.match_stmt.otherwise_count);

        for (int i = 0; i < ej_count; i++)
            patch_jump(c, end_jumps[i]);
        free(end_jumps);
        break;
    }

    /* ── run behavior_name with args ───────────────────── */
    case NODE_RUN: {
        compile_run_call(c, node);
        emit_set_var(c, nc_string_from_cstr("result"), node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── store value into "target" ─────────────────────── */
    case NODE_STORE: {
        emit_string(c, node->as.store_stmt.target, node->line);
        compile_expr(c, node->as.store_stmt.value);
        emit_byte(c, OP_STORE, node->line);
        break;
    }

    /* ── apply target using tool ───────────────────────── */
    case NODE_APPLY: {
        emit_string(c, node->as.apply.target, node->line);
        emit_string(c, node->as.apply.using, node->line);
        emit_byte(c, OP_GATHER, node->line);  /* apply = gather (same MCP path) */
        emit_set_var(c, nc_string_from_cstr("apply_result"), node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── check if ... using ... ────────────────────────── */
    case NODE_CHECK: {
        emit_string(c, node->as.check.using, node->line);
        emit_byte(c, OP_NONE, node->line);
        emit_byte(c, OP_GATHER, node->line);
        emit_set_var(c, node->as.check.save_as, node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── try: ... on error: ... finally: ... ─────────── */
    case NODE_TRY: {
        compile_body(c, node->as.try_stmt.body, node->as.try_stmt.body_count);

        /* Jump past the error handler if no error */
        int skip_error = -1;
        if (node->as.try_stmt.error_body && node->as.try_stmt.error_count > 0) {
            skip_error = emit_jump(c, OP_JUMP, node->line);
            /* Error body — VM will jump here on error (future enhancement).
             * For now, compile the error body so it exists in bytecode. */
            compile_body(c, node->as.try_stmt.error_body, node->as.try_stmt.error_count);
        }
        if (skip_error >= 0) patch_jump(c, skip_error);

        /* Finally block always runs */
        if (node->as.try_stmt.finally_body && node->as.try_stmt.finally_count > 0)
            compile_body(c, node->as.try_stmt.finally_body, node->as.try_stmt.finally_count);
        break;
    }

    /* ── stop / skip ───────────────────────────────────── */
    case NODE_STOP:
        emit_byte(c, OP_RETURN, node->line);
        break;
    case NODE_SKIP:
        break;

    /* ── expression statement ──────────────────────────── */
    case NODE_EXPR_STMT:
        compile_expr(c, node->as.single_expr.value);
        emit_byte(c, OP_POP, node->line);
        break;

    /* ── while condition: ... ─────────────────────────── */
    case NODE_WHILE: {
        int loop_start_pos = emit_loop_start(c);
        compile_expr(c, node->as.while_stmt.condition);
        int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
        emit_byte(c, OP_POP, node->line);
        compile_body(c, node->as.while_stmt.body, node->as.while_stmt.body_count);
        emit_loop(c, loop_start_pos, node->line);
        patch_jump(c, exit_jump);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    case NODE_FOR_COUNT: {
        compile_expr(c, node->as.for_count.count_expr);
        uint8_t limit_var = make_var(c, nc_string_from_cstr("__repeat_limit__"));
        emit_set_var(c, nc_string_from_cstr("__repeat_limit__"), node->line);
        emit_byte(c, OP_POP, node->line);
        emit_constant(c, NC_INT(0), node->line);
        uint8_t idx_var = make_var(c, nc_string_from_cstr("__repeat_idx__"));
        emit_set_var(c, nc_string_from_cstr("__repeat_idx__"), node->line);
        emit_byte(c, OP_POP, node->line);
        int loop_start_pos = emit_loop_start(c);
        emit_bytes(c, OP_GET_VAR, idx_var, node->line);
        emit_bytes(c, OP_GET_VAR, limit_var, node->line);
        emit_byte(c, OP_BELOW, node->line);
        int exit_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
        emit_byte(c, OP_POP, node->line);
        if (node->as.for_count.variable) {
            emit_bytes(c, OP_GET_VAR, idx_var, node->line);
            emit_set_var(c, node->as.for_count.variable, node->line);
            emit_byte(c, OP_POP, node->line);
        }
        compile_body(c, node->as.for_count.body, node->as.for_count.body_count);
        emit_bytes(c, OP_GET_VAR, idx_var, node->line);
        emit_constant(c, NC_INT(1), node->line);
        emit_byte(c, OP_ADD, node->line);
        emit_bytes(c, OP_SET_VAR, idx_var, node->line);
        emit_byte(c, OP_POP, node->line);
        emit_loop(c, loop_start_pos, node->line);
        patch_jump(c, exit_jump);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    case NODE_APPEND: {
        emit_string(c, nc_string_from_cstr("append"), node->line);
        emit_get_var(c, node->as.append_stmt.target, node->line);
        compile_expr(c, node->as.append_stmt.value);
        emit_bytes(c, OP_CALL_NATIVE, 2, node->line);
        emit_byte(c, OP_POP, node->line);
        break;
    }

    /* ── assert <condition>, "message" ─────────────────── */
    case NODE_ASSERT: {
        compile_expr(c, node->as.assert_stmt.condition);
        int ok_jump = emit_jump(c, OP_JUMP_IF_FALSE, node->line);
        emit_byte(c, OP_POP, node->line);              /* pop true */
        int end_jump = emit_jump(c, OP_JUMP, node->line);
        patch_jump(c, ok_jump);
        emit_byte(c, OP_POP, node->line);              /* pop false */
        /* Emit assertion failure log */
        if (node->as.assert_stmt.message &&
            node->as.assert_stmt.message->type == NODE_STRING_LIT &&
            node->as.assert_stmt.message->as.string_lit.value) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Assertion failed (line %d): %s",
                     node->line, node->as.assert_stmt.message->as.string_lit.value->chars);
            emit_constant(c, NC_STRING(nc_string_from_cstr(msg)), node->line);
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Assertion failed at line %d", node->line);
            emit_constant(c, NC_STRING(nc_string_from_cstr(msg)), node->line);
        }
        emit_byte(c, OP_LOG, node->line);
        emit_byte(c, OP_HALT, node->line);
        patch_jump(c, end_jump);
        break;
    }

    /* ── test "name": ... body ... ─────────────────────── */
    case NODE_TEST_BLOCK: {
        /* Compile test body — relies on try/on_error for isolation */
        if (node->as.test_block.name) {
            char lbl[512];
            snprintf(lbl, sizeof(lbl), "[TEST] %s", node->as.test_block.name->chars);
            emit_constant(c, NC_STRING(nc_string_from_cstr(lbl)), node->line);
            emit_byte(c, OP_LOG, node->line);
        }
        compile_body(c, node->as.test_block.body, node->as.test_block.body_count);
        break;
    }

    /* ── await <expr> — sync passthrough in bytecode ───── */
    case NODE_AWAIT: {
        compile_expr(c, node->as.yield_stmt.value);
        /* Just evaluate the expression, leave result for SET */
        break;
    }

    /* ── yield <value> — output to console ─────────────── */
    case NODE_YIELD: {
        compile_expr(c, node->as.yield_stmt.value);
        emit_byte(c, OP_LOG, node->line);
        break;
    }

    /* ── stream_respond — SSE event frame ──────────────── */
    case NODE_STREAM_RESPOND: {
        compile_expr(c, node->as.stream_respond.value);
        emit_byte(c, OP_RESPOND, node->line);
        break;
    }

    default:
        break;
    }
}

static void compile_body(Compiler *c, NcASTNode **stmts, int count) {
    for (int i = 0; i < count && !c->had_error; i++)
        compile_stmt(c, stmts[i]);
}

/* ═══════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════ */

NcCompiler *nc_compiler_new(void) {
    NcCompiler *comp = calloc(1, sizeof(NcCompiler));
    if (!comp) return NULL;
    return comp;
}

bool nc_compiler_compile(NcCompiler *comp, NcASTNode *program) {
    if (!program) {
        snprintf(comp->error_msg, sizeof(comp->error_msg), "Cannot compile: program is NULL");
        comp->had_error = true;
        return false;
    }
    if (program->type != NODE_PROGRAM) return false;

    int beh_count = program->as.program.beh_count;
    comp->chunks = calloc(beh_count, sizeof(NcChunk));
    comp->beh_names = calloc(beh_count, sizeof(NcString *));
    if (!comp->chunks || !comp->beh_names) {
        snprintf(comp->error_msg, sizeof(comp->error_msg), "Memory allocation failed");
        comp->had_error = true;
        return false;
    }
    comp->chunk_count = beh_count;

    for (int b = 0; b < beh_count; b++) {
        NcASTNode *beh = program->as.program.behaviors[b];
        Compiler c = {0};
        c.chunk = nc_chunk_new();

        comp->beh_names[b] = nc_string_ref(beh->as.behavior.name);

        /* Register parameters as initial variables */
        for (int p = 0; p < beh->as.behavior.param_count; p++)
            nc_chunk_add_var(c.chunk, beh->as.behavior.params[p]->as.param.name);

        compile_body(&c, beh->as.behavior.body, beh->as.behavior.body_count);
        emit_byte(&c, OP_NONE, beh->line);
        emit_byte(&c, OP_RESPOND, beh->line);
        emit_byte(&c, OP_HALT, beh->line);

        comp->chunks[b] = *c.chunk;
        free(c.chunk);

        if (c.had_error) {
            strncpy(comp->error_msg, c.error_msg, sizeof(comp->error_msg));
            comp->had_error = true;
            return false;
        }
    }

    /* Store service metadata */
    comp->globals = nc_map_new();
    if (program->as.program.service_name)
        nc_map_set(comp->globals, nc_string_from_cstr("__service__"),
                   NC_STRING(nc_string_ref(program->as.program.service_name)));
    if (program->as.program.version)
        nc_map_set(comp->globals, nc_string_from_cstr("__version__"),
                   NC_STRING(nc_string_ref(program->as.program.version)));

    return true;
}

void nc_compiler_free(NcCompiler *comp) {
    if (!comp) return;
    if (comp->chunks) {
        for (int i = 0; i < comp->chunk_count; i++) {
            free(comp->chunks[i].code);
            free(comp->chunks[i].lines);
            free(comp->chunks[i].constants);
            for (int j = 0; j < comp->chunks[i].var_count; j++)
                nc_string_free(comp->chunks[i].var_names[j]);
            free(comp->chunks[i].var_names);
        }
        free(comp->chunks);
    }
    if (comp->beh_names) {
        for (int i = 0; i < comp->chunk_count; i++)
            nc_string_free(comp->beh_names[i]);
        free(comp->beh_names);
    }
    if (comp->globals) nc_map_free(comp->globals);
    free(comp);
}

/* ═══════════════════════════════════════════════════════════
 *  Bytecode disassembler
 * ═══════════════════════════════════════════════════════════ */

static const char *opcode_name(uint8_t op) {
    switch (op) {
        case OP_CONSTANT:      return "CONSTANT";
        case OP_NONE:          return "NONE";
        case OP_TRUE:          return "TRUE";
        case OP_FALSE:         return "FALSE";
        case OP_POP:           return "POP";
        case OP_GET_VAR:       return "GET_VAR";
        case OP_SET_VAR:       return "SET_VAR";
        case OP_GET_FIELD:     return "GET_FIELD";
        case OP_SET_FIELD:     return "SET_FIELD";
        case OP_GET_INDEX:     return "GET_INDEX";
        case OP_ADD:           return "ADD";
        case OP_SUBTRACT:      return "SUBTRACT";
        case OP_MULTIPLY:      return "MULTIPLY";
        case OP_DIVIDE:        return "DIVIDE";
        case OP_MODULO:        return "MODULO";
        case OP_NEGATE:        return "NEGATE";
        case OP_NOT:           return "NOT";
        case OP_EQUAL:         return "EQUAL";
        case OP_NOT_EQUAL:     return "NOT_EQUAL";
        case OP_ABOVE:         return "ABOVE";
        case OP_BELOW:         return "BELOW";
        case OP_AT_LEAST:      return "AT_LEAST";
        case OP_AT_MOST:       return "AT_MOST";
        case OP_IN:            return "IN";
        case OP_AND:           return "AND";
        case OP_OR:            return "OR";
        case OP_JUMP:          return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_LOOP:          return "LOOP";
        case OP_CALL:          return "CALL";
        case OP_CALL_NATIVE:   return "CALL_NATIVE";
        case OP_GATHER:        return "GATHER";
        case OP_ASK_AI:        return "ASK_AI";
        case OP_LOG:           return "LOG";
        case OP_NOTIFY:        return "NOTIFY";
        case OP_RESPOND:       return "RESPOND";
        case OP_WAIT:          return "WAIT";
        case OP_EMIT:          return "EMIT";
        case OP_STORE:         return "STORE";
        case OP_MAKE_LIST:     return "MAKE_LIST";
        case OP_MAKE_MAP:      return "MAKE_MAP";
        case OP_SLICE:         return "SLICE";
        case OP_HALT:          return "HALT";
        case OP_RETURN:        return "RETURN";
        default:               return "???";
    }
}

void nc_disassemble_chunk(NcChunk *chunk, const char *name) {
    printf("\n  === Bytecode: %s (%d bytes) ===\n", name, chunk->count);
    int offset = 0;
    while (offset < chunk->count) {
        uint8_t op = chunk->code[offset];
        printf("  %04d  %-16s", offset, opcode_name(op));

        switch (op) {
        case OP_CONSTANT: {
            uint8_t idx = chunk->code[offset + 1];
            printf("  [%d] = ", idx);
            if (idx < chunk->const_count)
                nc_value_print(chunk->constants[idx], stdout);
            offset += 2;
            break;
        }
        case OP_GET_VAR:
        case OP_SET_VAR:
        case OP_GET_FIELD:
        case OP_SET_FIELD: {
            uint8_t idx = chunk->code[offset + 1];
            if (idx < chunk->var_count)
                printf("  '%s'", chunk->var_names[idx]->chars);
            else
                printf("  [%d]", idx);
            offset += 2;
            break;
        }
        case OP_MAKE_LIST:
        case OP_MAKE_MAP:
        case OP_CALL:
        case OP_CALL_NATIVE:
            printf("  (%d args)", chunk->code[offset + 1]);
            offset += 2;
            break;
        case OP_JUMP:
        case OP_JUMP_IF_FALSE: {
            uint16_t target = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
            printf("  -> %d", offset + 3 + target);
            offset += 3;
            break;
        }
        case OP_LOOP: {
            uint16_t back = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
            printf("  <- %d", offset + 3 - back);
            offset += 3;
            break;
        }
        default:
            offset++;
            break;
        }
        printf("\n");
    }
    printf("  === End ===\n\n");
}

uint32_t nc_source_hash(const char *source, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)source[i];
        hash *= 16777619u;
    }
    return hash;
}

/* Bytecode cache intentionally removed — the format was incomplete
 * (didn't persist constants or variable names, producing broken chunks).
 * NC always compiles from source. Compilation is fast (~1ms) so
 * caching provides negligible benefit for correctness risk. */
bool nc_chunk_cache_save(NcCompiler *comp, const char *path, uint32_t src_hash) {
    (void)comp; (void)path; (void)src_hash;
    return false;
}

bool nc_chunk_cache_load(NcCompiler *comp, const char *path, uint32_t src_hash) {
    (void)comp; (void)path; (void)src_hash;
    return false;
}
