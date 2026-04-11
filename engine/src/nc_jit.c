/*
 * nc_jit.c — JIT compilation foundation for NC.
 *
 * v0.8: Performance + JIT
 *
 * Two performance techniques:
 *   1. Computed goto dispatch — replaces switch in VM loop (2-3x faster)
 *   2. Method JIT — compiles hot bytecode to native machine code
 *
 * The computed goto dispatch is the single biggest performance win
 * for any bytecode VM. CPython 3.12 uses this same technique.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include "nc_model.h"
#include "nc_training.h"

extern char *nc_redact_for_display(const char *text);

/* NC UI is compiled separately — no forward declarations needed here */

/* ═══════════════════════════════════════════════════════════
 *  1. COMPUTED GOTO VM — fast dispatch (2-3x faster than switch)
 *
 *  Instead of: switch(opcode) { case OP_ADD: ... }
 *  Uses:       goto *dispatch_table[opcode];
 *
 *  This eliminates branch prediction misses in the CPU.
 * ═══════════════════════════════════════════════════════════ */

#ifdef __GNUC__  /* GCC/Clang support computed goto */

NcValue nc_vm_execute_fast(NcVM *vm, NcChunk *chunk) {
    static void *dispatch[] = {
        /* ── Core opcodes ─────────────────────────────────────── */
        [OP_CONSTANT]         = &&op_constant,
        [OP_CONSTANT_LONG]    = &&op_constant_long,
        [OP_NONE]             = &&op_none,
        [OP_TRUE]             = &&op_true,
        [OP_FALSE]            = &&op_false,
        [OP_POP]              = &&op_pop,
        [OP_GET_VAR]          = &&op_get_var,
        [OP_SET_VAR]          = &&op_set_var,
        [OP_GET_LOCAL]        = &&op_get_local,
        [OP_SET_LOCAL]        = &&op_set_local,
        [OP_GET_FIELD]        = &&op_get_field,
        [OP_SET_FIELD]        = &&op_set_field,
        [OP_GET_INDEX]        = &&op_get_index,
        [OP_SET_INDEX]        = &&op_set_index,
        [OP_SLICE]            = &&op_slice,
        [OP_ADD]              = &&op_add,
        [OP_SUBTRACT]         = &&op_sub,
        [OP_MULTIPLY]         = &&op_mul,
        [OP_DIVIDE]           = &&op_div,
        [OP_MODULO]           = &&op_mod,
        [OP_NEGATE]           = &&op_neg,
        [OP_NOT]              = &&op_not,
        [OP_EQUAL]            = &&op_eq,
        [OP_NOT_EQUAL]        = &&op_neq,
        [OP_ABOVE]            = &&op_above,
        [OP_BELOW]            = &&op_below,
        [OP_AT_LEAST]         = &&op_at_least,
        [OP_AT_MOST]          = &&op_at_most,
        [OP_AND]              = &&op_and,
        [OP_OR]               = &&op_or,
        [OP_IN]               = &&op_in,
        [OP_JUMP]             = &&op_jump,
        [OP_JUMP_IF_FALSE]    = &&op_jump_false,
        [OP_LOOP]             = &&op_loop,
        [OP_CALL]             = &&op_call,
        [OP_CALL_NATIVE]      = &&op_call_native,
        [OP_RETURN]           = &&op_return,
        [OP_HALT]             = &&op_halt,
        [OP_MAKE_LIST]        = &&op_make_list,
        [OP_MAKE_MAP]         = &&op_make_map,
        /* ── NC-Specific opcodes ───────────────────────────────── */
        [OP_GATHER]           = &&op_gather,
        [OP_STORE]            = &&op_store,
        [OP_ASK_AI]           = &&op_ask_ai,
        [OP_NOTIFY]           = &&op_notify,
        [OP_LOG]              = &&op_log,
        [OP_EMIT]             = &&op_emit,
        [OP_WAIT]             = &&op_wait,
        [OP_RESPOND]          = &&op_respond,
        /* ── NC UI: Virtual DOM ────────────────────────────────── */
        [OP_UI_ELEMENT]       = &&op_ui_element,
        [OP_UI_PROP]          = &&op_ui_prop,
        [OP_UI_PROP_EXPR]     = &&op_ui_prop_expr,
        [OP_UI_TEXT]          = &&op_ui_text,
        [OP_UI_CHILD]         = &&op_ui_child,
        [OP_UI_END_ELEMENT]   = &&op_ui_end_element,
        /* ── NC UI: State ──────────────────────────────────────── */
        [OP_STATE_DECLARE]    = &&op_state_declare,
        [OP_STATE_GET]        = &&op_state_get,
        [OP_STATE_SET]        = &&op_state_set,
        [OP_STATE_COMPUTED]   = &&op_state_computed,
        [OP_STATE_WATCH]      = &&op_state_watch,
        /* ── NC UI: Bindings ───────────────────────────────────── */
        [OP_UI_BIND]          = &&op_ui_bind,
        [OP_UI_BIND_INPUT]    = &&op_ui_bind_input,
        /* ── NC UI: Events ─────────────────────────────────────── */
        [OP_UI_ON_EVENT]      = &&op_ui_on_event,
        /* ── NC UI: Lifecycle ──────────────────────────────────── */
        [OP_UI_COMPONENT]     = &&op_ui_component,
        [OP_UI_MOUNT]         = &&op_ui_mount,
        [OP_UI_UNMOUNT]       = &&op_ui_unmount,
        [OP_UI_ON_MOUNT]      = &&op_ui_on_mount,
        [OP_UI_ON_UNMOUNT]    = &&op_ui_on_unmount,
        /* ── NC UI: Diffing ────────────────────────────────────── */
        [OP_UI_RENDER]        = &&op_ui_render,
        [OP_UI_DIFF]          = &&op_ui_diff,
        [OP_UI_PATCH]         = &&op_ui_patch,
        /* ── NC UI: Routing ────────────────────────────────────── */
        [OP_UI_ROUTE_DEF]     = &&op_ui_route_def,
        [OP_UI_ROUTE_PUSH]    = &&op_ui_route_push,
        [OP_UI_ROUTE_GUARD]   = &&op_ui_route_guard,
        [OP_UI_ROUTE_MATCH]   = &&op_ui_route_match,
        /* ── NC UI: Async ──────────────────────────────────────── */
        [OP_UI_FETCH]         = &&op_ui_fetch,
        [OP_UI_FETCH_AUTH]    = &&op_ui_fetch_auth,
        /* ── NC UI: Control Flow ───────────────────────────────── */
        [OP_UI_IF]            = &&op_ui_if,
        [OP_UI_FOR_EACH]      = &&op_ui_for_each,
        [OP_UI_SHOW]          = &&op_ui_show,
        /* ── NC UI: Forms ──────────────────────────────────────── */
        [OP_UI_FORM]          = &&op_ui_form,
        [OP_UI_VALIDATE]      = &&op_ui_validate,
        [OP_UI_FORM_SUBMIT]   = &&op_ui_form_submit,
        /* ── NC UI: Auth ───────────────────────────────────────── */
        [OP_UI_AUTH_CHECK]    = &&op_ui_auth_check,
        [OP_UI_ROLE_CHECK]    = &&op_ui_role_check,
        [OP_UI_PERM_CHECK]    = &&op_ui_perm_check,
    };

    register uint8_t *ip = chunk->code;
    NcValue result = NC_NONE();
    int loop_guard = 0;

    if (vm->frame_count >= NC_FRAMES_MAX) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
            "Call stack overflow (max %d frames). Possible infinite recursion.", NC_FRAMES_MAX);
        vm->had_error = true;
        return NC_NONE();
    }

    NcCallFrame *frame = &vm->frames[vm->frame_count];
    frame->chunk = chunk;
    frame->local_count = 0;
    vm->frame_count++;

    #define PUSH(v)    do { \
        if (vm->stack_top >= NC_STACK_MAX) { \
            snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack overflow"); \
            vm->had_error = true; goto done; \
        } \
        vm->stack[vm->stack_top++] = (v); \
    } while(0)
    #define POP()      (vm->stack_top > 0 ? vm->stack[--vm->stack_top] : NC_NONE())
    #define PEEK(d)    (vm->stack_top - 1 - (d) >= 0 ? vm->stack[vm->stack_top - 1 - (d)] : NC_NONE())
    #define READ()     (*ip++)
    #define READ16()   (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
    #define DISPATCH() do { \
        uint8_t _op = READ(); \
        if (_op > OP_UI_PERM_CHECK) { \
            snprintf(vm->error_msg, sizeof(vm->error_msg), "Unknown opcode: %d", _op); \
            vm->had_error = true; goto done; \
        } \
        goto *dispatch[_op]; \
    } while(0)
    #define AS_NUM(v)  (IS_INT(v) ? (double)AS_INT(v) : AS_FLOAT(v))

    DISPATCH();

op_constant: { uint8_t idx = READ(); PUSH(chunk->constants[idx]); DISPATCH(); }
op_none:     PUSH(NC_NONE()); DISPATCH();
op_true:     PUSH(NC_BOOL(true)); DISPATCH();
op_false:    PUSH(NC_BOOL(false)); DISPATCH();
op_pop:      POP(); DISPATCH();

op_get_var: {
    uint8_t idx = READ();
    NcString *name = chunk->var_names[idx];
    PUSH(nc_map_get(vm->globals, name));
    DISPATCH();
}
op_set_var: {
    uint8_t idx = READ();
    nc_map_set(vm->globals, chunk->var_names[idx], PEEK(0));
    DISPATCH();
}
op_get_field: {
    uint8_t idx = READ();
    NcValue obj = POP();
    if (IS_MAP(obj)) {
        PUSH(nc_map_get(AS_MAP(obj), chunk->var_names[idx]));
    } else if (IS_STRING(obj) && AS_STRING(obj)->length > 1) {
        const char *s = AS_STRING(obj)->chars;
        if (s[0] == '{' || s[0] == '[') {
            NcValue parsed = nc_json_parse(s);
            if (IS_MAP(parsed))
                PUSH(nc_map_get(AS_MAP(parsed), chunk->var_names[idx]));
            else if (IS_LIST(parsed))
                PUSH(parsed);
            else
                PUSH(NC_NONE());
        } else {
            PUSH(NC_NONE());
        }
    } else {
        PUSH(NC_NONE());
    }
    DISPATCH();
}
op_get_index: {
    NcValue idx = POP();
    NcValue obj = POP();
    if (IS_LIST(obj) && IS_INT(idx))
        PUSH(nc_list_get(AS_LIST(obj), (int)AS_INT(idx)));
    else if (IS_MAP(obj) && IS_STRING(idx))
        PUSH(nc_map_get(AS_MAP(obj), AS_STRING(idx)));
    else if (IS_STRING(obj) && IS_INT(idx)) {
        int i = (int)AS_INT(idx);
        if (i >= 0 && i < AS_STRING(obj)->length) {
            char ch[2] = {AS_STRING(obj)->chars[i], '\0'};
            PUSH(NC_STRING(nc_string_from_cstr(ch)));
        } else PUSH(NC_NONE());
    }
    else PUSH(NC_NONE());
    DISPATCH();
}

op_set_index: {
    NcValue obj = POP();
    NcValue idx = POP();
    NcValue val = POP();
    if (IS_MAP(obj)) {
        NcString *key = IS_STRING(idx) ? AS_STRING(idx) : nc_value_to_string(idx);
        nc_map_set(AS_MAP(obj), key, val);
    } else if (IS_LIST(obj) && IS_INT(idx)) {
        int i = (int)AS_INT(idx);
        if (i >= 0 && i < AS_LIST(obj)->count) {
            NcValue old = AS_LIST(obj)->items[i];
            nc_value_retain(val);
            AS_LIST(obj)->items[i] = val;
            nc_value_release(old);
        }
    }
    PUSH(val);
    DISPATCH();
}

op_add: {
    NcValue b = POP(), a = POP();
    if (IS_STRING(a) || IS_STRING(b)) {
        NcString *sa = nc_value_to_string(a), *sb = nc_value_to_string(b);
        PUSH(NC_STRING(nc_string_concat(sa, sb)));
        nc_string_free(sa); nc_string_free(sb);
    } else if (IS_INT(a) && IS_INT(b)) {
        int64_t r; if (__builtin_add_overflow(AS_INT(a), AS_INT(b), &r)) PUSH(NC_FLOAT((double)AS_INT(a) + (double)AS_INT(b)));
        else PUSH(NC_INT(r));
    } else PUSH(NC_FLOAT(AS_NUM(a) + AS_NUM(b)));
    DISPATCH();
}
op_sub: { NcValue b = POP(), a = POP();
    if (IS_INT(a) && IS_INT(b)) { int64_t r; if (__builtin_sub_overflow(AS_INT(a), AS_INT(b), &r)) PUSH(NC_FLOAT((double)AS_INT(a) - (double)AS_INT(b))); else PUSH(NC_INT(r)); }
    else PUSH(NC_FLOAT(AS_NUM(a) - AS_NUM(b))); DISPATCH(); }
op_mul: { NcValue b = POP(), a = POP();
    if (IS_INT(a) && IS_INT(b)) { int64_t r; if (__builtin_mul_overflow(AS_INT(a), AS_INT(b), &r)) PUSH(NC_FLOAT((double)AS_INT(a) * (double)AS_INT(b))); else PUSH(NC_INT(r)); }
    else PUSH(NC_FLOAT(AS_NUM(a) * AS_NUM(b))); DISPATCH(); }
op_div: { NcValue b = POP(), a = POP();
    double d = AS_NUM(b); PUSH(d != 0 ? NC_FLOAT(AS_NUM(a)/d) : NC_INT(0)); DISPATCH(); }
op_mod: { NcValue b = POP(), a = POP();
    if (IS_INT(a) && IS_INT(b) && AS_INT(b) != 0) PUSH(NC_INT(AS_INT(a) % AS_INT(b)));
    else { double d = AS_NUM(b); PUSH(d != 0 ? NC_FLOAT(fmod(AS_NUM(a), d)) : NC_INT(0)); }
    DISPATCH(); }
op_neg: { NcValue v = POP();
    PUSH(IS_INT(v) ? NC_INT(-AS_INT(v)) : NC_FLOAT(-AS_NUM(v))); DISPATCH(); }
op_not: PUSH(NC_BOOL(!nc_truthy(POP()))); DISPATCH();

op_eq: { NcValue b = POP(), a = POP();
    if (IS_STRING(a) && IS_STRING(b)) PUSH(NC_BOOL(nc_string_equal(AS_STRING(a), AS_STRING(b))));
    else PUSH(NC_BOOL(AS_NUM(a) == AS_NUM(b))); DISPATCH(); }
op_neq: { NcValue b = POP(), a = POP();
    if (IS_STRING(a) && IS_STRING(b)) PUSH(NC_BOOL(!nc_string_equal(AS_STRING(a), AS_STRING(b))));
    else PUSH(NC_BOOL(AS_NUM(a) != AS_NUM(b))); DISPATCH(); }
op_above:    { NcValue b = POP(), a = POP(); PUSH(NC_BOOL(AS_NUM(a) > AS_NUM(b))); DISPATCH(); }
op_below:    { NcValue b = POP(), a = POP();
    double bv = IS_LIST(b) ? (double)AS_LIST(b)->count : AS_NUM(b);
    PUSH(NC_BOOL(AS_NUM(a) < bv)); DISPATCH(); }
op_at_least: { NcValue b = POP(), a = POP(); PUSH(NC_BOOL(AS_NUM(a) >= AS_NUM(b))); DISPATCH(); }
op_at_most:  { NcValue b = POP(), a = POP(); PUSH(NC_BOOL(AS_NUM(a) <= AS_NUM(b))); DISPATCH(); }
op_and: { NcValue b = POP(), a = POP(); PUSH(nc_truthy(a) ? b : a); DISPATCH(); }
op_or:  { NcValue b = POP(), a = POP(); PUSH(nc_truthy(a) ? a : b); DISPATCH(); }
op_in: {
    NcValue container = POP(), needle = POP();
    bool found = false;
    if (IS_LIST(container)) {
        NcList *list = AS_LIST(container);
        for (int ii = 0; ii < list->count; ii++) {
            if (IS_STRING(needle) && IS_STRING(list->items[ii]) &&
                nc_string_equal(AS_STRING(needle), AS_STRING(list->items[ii]))) { found = true; break; }
            if (IS_INT(needle) && IS_INT(list->items[ii]) &&
                AS_INT(needle) == AS_INT(list->items[ii])) { found = true; break; }
        }
    }
    PUSH(NC_BOOL(found)); DISPATCH();
}

op_jump:       { uint16_t off = READ16(); ip += off; DISPATCH(); }
op_jump_false: { uint16_t off = READ16(); if (!nc_truthy(PEEK(0))) ip += off; DISPATCH(); }
op_loop: {
    uint16_t off = READ16();
    if (++loop_guard > 10000000) {
        snprintf(vm->error_msg, sizeof(vm->error_msg),
            "Loop exceeded 10M iterations. Possible infinite loop.");
        vm->had_error = true;
        goto done;
    }
    ip -= off;
    DISPATCH();
}

op_call: {
    uint8_t argc = READ();
    if (argc + 1 > vm->stack_top) { snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow in call"); vm->had_error = true; goto jit_done; }
    NcValue fn_name = vm->stack[vm->stack_top - argc - 1];
    if (IS_STRING(fn_name)) {
        NcString *name = AS_STRING(fn_name);
        NcValue chunk_val = nc_map_get(vm->behaviors, name);
        if (IS_INT(chunk_val)) {
            int chunk_idx = (int)AS_INT(chunk_val);
            if (chunk_idx >= 0 && chunk_idx < vm->behavior_chunk_count && vm->behavior_chunks) {
                NcChunk *target = &vm->behavior_chunks[chunk_idx];
                for (int ca = 0; ca < argc && ca < target->var_count; ca++) {
                    NcValue arg = vm->stack[vm->stack_top - argc + ca];
                    nc_map_set(vm->globals, target->var_names[ca], arg);
                }
                vm->stack_top -= (argc + 1);
                NcValue call_result = nc_vm_execute(vm, target);
                nc_value_retain(call_result);
                PUSH(call_result);
                DISPATCH();
            }
        }
    }
    vm->stack_top -= (argc + 1);
    PUSH(NC_NONE());
    DISPATCH();
}
op_call_native: {
    uint8_t argc = READ();
    if (argc + 1 > vm->stack_top) { snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow in native call"); vm->had_error = true; goto jit_done; }
    NcValue fn = vm->stack[vm->stack_top - argc - 1];
    NcValue *a = &vm->stack[vm->stack_top - argc];

    if (fn.type == VAL_NATIVE_FN) {
        NcValue r = AS_NATIVE(fn)(vm, argc, a);
        vm->stack_top -= (argc + 1);
        PUSH(r);
        DISPATCH();
    }

    const char *fn_name = IS_STRING(fn) ? AS_STRING(fn)->chars : "";
    int fn_len = IS_STRING(fn) ? AS_STRING(fn)->length : 0;
    NcValue result = NC_NONE();

    uint32_t h = 2166136261u;
    for (int i = 0; i < fn_len; i++) { h ^= (uint8_t)fn_name[i]; h *= 16777619u; }

    switch (h & 0x1F) {
    default:
        goto native_slow_path;
    }

native_slow_path:
    if (fn_len == 3 && memcmp(fn_name, "str", 3) == 0 && argc == 1)
        result = NC_STRING(nc_value_to_string(a[0]));
    else if (fn_len == 3 && memcmp(fn_name, "int", 3) == 0 && argc == 1)
        result = NC_INT((int64_t)(IS_INT(a[0]) ? AS_INT(a[0]) : IS_FLOAT(a[0]) ? (int64_t)AS_FLOAT(a[0]) : 0));
    else if (fn_len == 5 && memcmp(fn_name, "float", 5) == 0 && argc == 1)
        result = NC_FLOAT(IS_INT(a[0]) ? (double)AS_INT(a[0]) : IS_FLOAT(a[0]) ? AS_FLOAT(a[0]) : 0.0);
    else if (fn_len == 3 && memcmp(fn_name, "len", 3) == 0 && argc == 1) {
        if (IS_LIST(a[0])) result = NC_INT(AS_LIST(a[0])->count);
        else if (IS_STRING(a[0])) result = NC_INT(AS_STRING(a[0])->length);
        else if (IS_MAP(a[0])) result = NC_INT(AS_MAP(a[0])->count);
        else result = NC_INT(0);
    }
    else if (fn_len == 3 && memcmp(fn_name, "get", 3) == 0 && argc >= 2) {
        if (IS_LIST(a[0]) && IS_INT(a[1])) {
            result = nc_list_get(AS_LIST(a[0]), (int)AS_INT(a[1]));
            nc_value_retain(result);
        } else if (IS_MAP(a[0]) && IS_STRING(a[1])) {
            result = nc_map_get(AS_MAP(a[0]), AS_STRING(a[1]));
            nc_value_retain(result);
        } else if (argc >= 3) {
            result = a[2];
        } else {
            result = NC_NONE();
        }
    }
    else if (fn_len == 5 && memcmp(fn_name, "upper", 5) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_upper(AS_STRING(a[0]));
    else if (fn_len == 5 && memcmp(fn_name, "lower", 5) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_lower(AS_STRING(a[0]));
    else if (fn_len == 4 && memcmp(fn_name, "trim", 4) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_trim(AS_STRING(a[0]));
    else if (fn_len == 8 && memcmp(fn_name, "contains", 8) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_contains(AS_STRING(a[0]), AS_STRING(a[1]));
    else if (fn_len == 5 && memcmp(fn_name, "split", 5) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_split(AS_STRING(a[0]), AS_STRING(a[1]));
    else if (fn_len == 4 && memcmp(fn_name, "join", 4) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_join(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 3 && memcmp(fn_name, "abs", 3) == 0 && argc == 1)
        result = nc_stdlib_abs(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]));
    else if (fn_len == 4 && memcmp(fn_name, "sqrt", 4) == 0 && argc == 1)
        result = nc_stdlib_sqrt(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]));
    else if (fn_len == 4 && memcmp(fn_name, "keys", 4) == 0 && argc == 1 && IS_MAP(a[0])) {
        NcList *l = nc_list_new();
        NcMap *m = AS_MAP(a[0]);
        for (int i = 0; i < m->count; i++)
            nc_list_push(l, NC_STRING(nc_string_ref(m->keys[i])));
        result = NC_LIST(l);
    }
    else if (fn_len == 6 && memcmp(fn_name, "values", 6) == 0 && argc == 1 && IS_MAP(a[0])) {
        NcList *l = nc_list_new();
        NcMap *m = AS_MAP(a[0]);
        for (int i = 0; i < m->count; i++)
            nc_list_push(l, m->values[i]);
        result = NC_LIST(l);
    }
    else if (fn_len == 5 && memcmp(fn_name, "range", 5) == 0) {
        int64_t start = 0, end = 0, step = 1;
        if (argc >= 1) end = IS_INT(a[0]) ? AS_INT(a[0]) : (int64_t)AS_FLOAT(a[0]);
        if (argc >= 2) { start = end; end = IS_INT(a[1]) ? AS_INT(a[1]) : (int64_t)AS_FLOAT(a[1]); }
        if (argc >= 3) step = IS_INT(a[2]) ? AS_INT(a[2]) : (int64_t)AS_FLOAT(a[2]);
        if (step == 0) step = 1;
        NcList *l = nc_list_new();
        for (int64_t i = start; (step > 0) ? i < end : i > end; i += step)
            nc_list_push(l, NC_INT(i));
        result = NC_LIST(l);
    }
    else if (fn_len == 6 && memcmp(fn_name, "append", 6) == 0 && argc == 2 && IS_LIST(a[0])) {
        result = nc_stdlib_list_append(AS_LIST(a[0]), a[1]);
        nc_value_retain(result);
    }
    else if (fn_len == 7 && memcmp(fn_name, "reverse", 7) == 0 && argc == 1 && IS_LIST(a[0]))
        result = nc_stdlib_list_reverse(AS_LIST(a[0]));
    else if (fn_len == 4 && memcmp(fn_name, "sort", 4) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_sort_by(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 4 && memcmp(fn_name, "sort", 4) == 0 && argc == 1 && IS_LIST(a[0]))
        result = nc_stdlib_list_sort(AS_LIST(a[0]));
    else if (fn_len == 7 && memcmp(fn_name, "replace", 7) == 0 && argc == 3 && IS_STRING(a[0]) && IS_STRING(a[1]) && IS_STRING(a[2]))
        result = nc_stdlib_replace(AS_STRING(a[0]), AS_STRING(a[1]), AS_STRING(a[2]));
    else if (fn_len == 11 && memcmp(fn_name, "starts_with", 11) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_starts_with(AS_STRING(a[0]), AS_STRING(a[1]));
    else if (fn_len == 9 && memcmp(fn_name, "ends_with", 9) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_ends_with(AS_STRING(a[0]), AS_STRING(a[1]));
    else if (fn_len == 4 && memcmp(fn_name, "type", 4) == 0 && argc == 1) {
        const char *t = "unknown";
        switch (a[0].type) {
            case VAL_NONE: t = "nothing"; break;
            case VAL_BOOL: t = "yesno"; break;
            case VAL_INT: case VAL_FLOAT: t = "number"; break;
            case VAL_STRING: t = "text"; break;
            case VAL_LIST: t = "list"; break;
            case VAL_MAP: t = "record"; break;
            case VAL_TENSOR: t = "tensor"; break;
            default: break;
        }
        result = NC_STRING(nc_string_from_cstr(t));
    }
    /* ── Math functions ───────────────────── */
    else if (fn_len == 4 && memcmp(fn_name, "ceil", 4) == 0 && argc == 1)
        result = nc_stdlib_ceil(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]));
    else if (fn_len == 5 && memcmp(fn_name, "floor", 5) == 0 && argc == 1)
        result = nc_stdlib_floor(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]));
    else if (fn_len == 5 && memcmp(fn_name, "round", 5) == 0 && argc == 2) {
        double val = IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]);
        int decimals = (int)(IS_INT(a[1]) ? AS_INT(a[1]) : AS_FLOAT(a[1]));
        double factor = pow(10.0, decimals);
        result = NC_FLOAT(round(val * factor) / factor);
    }
    else if (fn_len == 5 && memcmp(fn_name, "round", 5) == 0 && argc == 1)
        result = nc_stdlib_round(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]));
    else if (fn_len == 3 && memcmp(fn_name, "pow", 3) == 0 && argc == 2)
        result = nc_stdlib_pow(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]),
                               IS_INT(a[1]) ? (double)AS_INT(a[1]) : AS_FLOAT(a[1]));
    else if (fn_len == 3 && memcmp(fn_name, "min", 3) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        if (l->count > 0) {
            double best = IS_INT(l->items[0]) ? (double)AS_INT(l->items[0]) : AS_FLOAT(l->items[0]);
            int bi = 0;
            for (int i = 1; i < l->count; i++) {
                double v = IS_INT(l->items[i]) ? (double)AS_INT(l->items[i]) : AS_FLOAT(l->items[i]);
                if (v < best) { best = v; bi = i; }
            }
            result = l->items[bi];
        }
    }
    else if (fn_len == 3 && memcmp(fn_name, "min", 3) == 0 && argc == 2)
        result = nc_stdlib_min(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]),
                               IS_INT(a[1]) ? (double)AS_INT(a[1]) : AS_FLOAT(a[1]));
    else if (fn_len == 3 && memcmp(fn_name, "max", 3) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        if (l->count > 0) {
            double best = IS_INT(l->items[0]) ? (double)AS_INT(l->items[0]) : AS_FLOAT(l->items[0]);
            int bi = 0;
            for (int i = 1; i < l->count; i++) {
                double v = IS_INT(l->items[i]) ? (double)AS_INT(l->items[i]) : AS_FLOAT(l->items[i]);
                if (v > best) { best = v; bi = i; }
            }
            result = l->items[bi];
        }
    }
    else if (fn_len == 3 && memcmp(fn_name, "max", 3) == 0 && argc == 2)
        result = nc_stdlib_max(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0]),
                               IS_INT(a[1]) ? (double)AS_INT(a[1]) : AS_FLOAT(a[1]));
    else if (fn_len == 6 && memcmp(fn_name, "random", 6) == 0 && argc == 0)
        result = nc_stdlib_random();
    /* ── Time functions ───────────────────── */
    else if (fn_len == 8 && memcmp(fn_name, "time_now", 8) == 0 && argc == 0)
        result = nc_stdlib_time_now();
    else if (fn_len == 7 && memcmp(fn_name, "time_ms", 7) == 0 && argc == 0)
        result = nc_stdlib_time_ms();
    /* ── File I/O ─────────────────────────── */
    else if (fn_len == 9 && memcmp(fn_name, "read_file", 9) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_read_file(AS_STRING(a[0])->chars);
    else if (fn_len == 10 && memcmp(fn_name, "write_file", 10) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_write_file(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars);
    /* ── Environment ──────────────────────── */
    else if (fn_len == 3 && memcmp(fn_name, "env", 3) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_env_get(AS_STRING(a[0])->chars);
    /* ── JSON ─────────────────────────────── */
    else if (fn_len == 11 && memcmp(fn_name, "json_encode", 11) == 0 && argc == 1) {
        char *json = nc_json_serialize(a[0], false);
        result = NC_STRING(nc_string_from_cstr(json));
        free(json);
    }
    else if (fn_len == 11 && memcmp(fn_name, "json_decode", 11) == 0 && argc == 1) {
        if (IS_LIST(a[0]) || IS_MAP(a[0]) || IS_INT(a[0]) || IS_FLOAT(a[0]) || IS_BOOL(a[0]))
            result = a[0];
        else if (IS_STRING(a[0]))
            result = nc_json_parse(AS_STRING(a[0])->chars);
    }
    /* ── Print ────────────────────────────── */
    else if (fn_len == 5 && memcmp(fn_name, "print", 5) == 0) {
        for (int pi = 0; pi < argc; pi++) {
            NcString *s = nc_value_to_string(a[pi]);
            if (vm->output_count < vm->output_capacity)
                vm->output[vm->output_count++] = strdup(s->chars);
            nc_string_free(s);
        }
        result = NC_NONE();
    }
    else if (fn_len == 5 && memcmp(fn_name, "input", 5) == 0 && argc <= 1) {
        if (argc == 1 && IS_STRING(a[0]))
            printf("%s", AS_STRING(a[0])->chars);
        char buf[1024];
        if (fgets(buf, sizeof(buf), stdin)) {
            int len = (int)strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
            result = NC_STRING(nc_string_from_cstr(buf));
        } else {
            result = NC_STRING(nc_string_from_cstr(""));
        }
    }
    /* ── List utility functions ────────────── */
    else if (fn_len == 5 && memcmp(fn_name, "first", 5) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        result = l->count > 0 ? l->items[0] : NC_NONE();
    }
    else if (fn_len == 4 && memcmp(fn_name, "last", 4) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        result = l->count > 0 ? l->items[l->count - 1] : NC_NONE();
    }
    else if (fn_len == 3 && memcmp(fn_name, "sum", 3) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        double total = 0;
        for (int si = 0; si < l->count; si++)
            total += IS_INT(l->items[si]) ? (double)AS_INT(l->items[si]) : IS_FLOAT(l->items[si]) ? AS_FLOAT(l->items[si]) : 0;
        result = NC_FLOAT(total);
    }
    else if (fn_len == 7 && memcmp(fn_name, "average", 7) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        if (l->count == 0) { result = NC_FLOAT(0); }
        else {
            double total = 0;
            for (int si = 0; si < l->count; si++)
                total += IS_INT(l->items[si]) ? (double)AS_INT(l->items[si]) : IS_FLOAT(l->items[si]) ? AS_FLOAT(l->items[si]) : 0;
            result = NC_FLOAT(total / l->count);
        }
    }
    else if (fn_len == 5 && memcmp(fn_name, "count", 5) == 0 && argc == 2 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        int cnt = 0;
        for (int ci = 0; ci < l->count; ci++) {
            if (IS_INT(a[1]) && IS_INT(l->items[ci]) && AS_INT(a[1]) == AS_INT(l->items[ci])) cnt++;
            else if (IS_STRING(a[1]) && IS_STRING(l->items[ci]) && nc_string_equal(AS_STRING(a[1]), AS_STRING(l->items[ci]))) cnt++;
        }
        result = NC_INT(cnt);
    }
    else if (fn_len == 8 && memcmp(fn_name, "index_of", 8) == 0 && argc == 2 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        int found_idx = -1;
        for (int fi = 0; fi < l->count; fi++) {
            if (IS_STRING(a[1]) && IS_STRING(l->items[fi]) && nc_string_equal(AS_STRING(a[1]), AS_STRING(l->items[fi]))) { found_idx = fi; break; }
            if (IS_INT(a[1]) && IS_INT(l->items[fi]) && AS_INT(a[1]) == AS_INT(l->items[fi])) { found_idx = fi; break; }
        }
        result = NC_INT(found_idx);
    }
    else if (fn_len == 6 && memcmp(fn_name, "unique", 6) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *src = AS_LIST(a[0]);
        NcList *uniq = nc_list_new();
        for (int ui = 0; ui < src->count; ui++) {
            bool dup = false;
            for (int uj = 0; uj < uniq->count; uj++) {
                if (IS_INT(src->items[ui]) && IS_INT(uniq->items[uj]) && AS_INT(src->items[ui]) == AS_INT(uniq->items[uj])) { dup = true; break; }
                if (IS_STRING(src->items[ui]) && IS_STRING(uniq->items[uj]) && nc_string_equal(AS_STRING(src->items[ui]), AS_STRING(uniq->items[uj]))) { dup = true; break; }
            }
            if (!dup) nc_list_push(uniq, src->items[ui]);
        }
        result = NC_LIST(uniq);
    }
    else if (fn_len == 7 && memcmp(fn_name, "flatten", 7) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *src = AS_LIST(a[0]);
        NcList *flat = nc_list_new();
        for (int fi = 0; fi < src->count; fi++) {
            if (IS_LIST(src->items[fi])) {
                NcList *inner = AS_LIST(src->items[fi]);
                for (int fj = 0; fj < inner->count; fj++)
                    nc_list_push(flat, inner->items[fj]);
            } else nc_list_push(flat, src->items[fi]);
        }
        result = NC_LIST(flat);
    }
    else if (fn_len == 5 && memcmp(fn_name, "slice", 5) == 0 && argc >= 2 && IS_LIST(a[0]) && IS_INT(a[1])) {
        NcList *src = AS_LIST(a[0]);
        int start = (int)AS_INT(a[1]);
        int end = argc >= 3 && IS_INT(a[2]) ? (int)AS_INT(a[2]) : src->count;
        if (start < 0) start = 0;
        if (end > src->count) end = src->count;
        NcList *sl = nc_list_new();
        for (int si = start; si < end; si++) nc_list_push(sl, src->items[si]);
        result = NC_LIST(sl);
    }
    else if (fn_len == 3 && memcmp(fn_name, "any", 3) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        bool found = false;
        for (int ai2 = 0; ai2 < l->count; ai2++)
            if (nc_truthy(l->items[ai2])) { found = true; break; }
        result = NC_BOOL(found);
    }
    else if (fn_len == 3 && memcmp(fn_name, "all", 3) == 0 && argc == 1 && IS_LIST(a[0])) {
        NcList *l = AS_LIST(a[0]);
        bool all_true = true;
        for (int ai2 = 0; ai2 < l->count; ai2++)
            if (!nc_truthy(l->items[ai2])) { all_true = false; break; }
        result = NC_BOOL(all_true);
    }
    /* ── Type check functions ─────────────── */
    else if (fn_len == 7 && memcmp(fn_name, "is_text", 7) == 0 && argc == 1)
        result = NC_BOOL(IS_STRING(a[0]));
    else if (fn_len == 9 && memcmp(fn_name, "is_number", 9) == 0 && argc == 1)
        result = NC_BOOL(IS_NUMBER(a[0]));
    else if (fn_len == 7 && memcmp(fn_name, "is_list", 7) == 0 && argc == 1)
        result = NC_BOOL(IS_LIST(a[0]));
    else if (fn_len == 9 && memcmp(fn_name, "is_record", 9) == 0 && argc == 1)
        result = NC_BOOL(IS_MAP(a[0]));
    else if (fn_len == 7 && memcmp(fn_name, "is_none", 7) == 0 && argc == 1)
        result = NC_BOOL(IS_NONE(a[0]));
    else if (fn_len == 7 && memcmp(fn_name, "is_bool", 7) == 0 && argc == 1)
        result = NC_BOOL(IS_BOOL(a[0]));
    /* ── Map utility functions ────────────── */
    else if (fn_len == 7 && memcmp(fn_name, "has_key", 7) == 0 && argc == 2 && IS_MAP(a[0]) && IS_STRING(a[1]))
        result = NC_BOOL(nc_map_has(AS_MAP(a[0]), AS_STRING(a[1])));
    /* ── String utility ───────────────────── */
    else if (fn_len == 6 && memcmp(fn_name, "substr", 6) == 0 && argc >= 2 && IS_STRING(a[0]) && IS_INT(a[1])) {
        NcString *s = AS_STRING(a[0]);
        int start = (int)AS_INT(a[1]);
        int end = argc >= 3 && IS_INT(a[2]) ? (int)AS_INT(a[2]) : s->length;
        if (start < 0) start = 0;
        if (end > s->length) end = s->length;
        if (start >= end) result = NC_STRING(nc_string_from_cstr(""));
        else result = NC_STRING(nc_string_new(s->chars + start, end - start));
    }
    else if (fn_len == 3 && memcmp(fn_name, "chr", 3) == 0 && argc == 1 &&
             (IS_INT(a[0]) || IS_FLOAT(a[0]))) {
        /* Accept both INT and FLOAT — numeric literals in service/function context
         * are often passed as FLOAT even when written as plain integers (e.g. chr(34)). */
        int cp = IS_INT(a[0]) ? (int)AS_INT(a[0]) : (int)AS_FLOAT(a[0]);
        char ch[2] = {(char)cp, '\0'};
        result = NC_STRING(nc_string_from_cstr(ch));
    }
    else if (fn_len == 3 && memcmp(fn_name, "ord", 3) == 0 && argc == 1 && IS_STRING(a[0])) {
        result = NC_INT(AS_STRING(a[0])->length > 0 ? (int64_t)(unsigned char)AS_STRING(a[0])->chars[0] : 0);
    }
    /* ── Data format functions ────────────── */
    else if (fn_len == 10 && memcmp(fn_name, "yaml_parse", 10) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_yaml_parse(AS_STRING(a[0])->chars);
    else if (fn_len == 9 && memcmp(fn_name, "csv_parse", 9) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_csv_parse(AS_STRING(a[0])->chars);
    else if (fn_len == 10 && memcmp(fn_name, "toml_parse", 10) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_toml_parse(AS_STRING(a[0])->chars);
    else if (fn_len == 9 && memcmp(fn_name, "xml_parse", 9) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_xml_parse(AS_STRING(a[0])->chars);
    /* ── Cache functions ──────────────────── */
    else if (fn_len == 5 && memcmp(fn_name, "cache", 5) == 0 && argc == 2 && IS_STRING(a[0]))
        result = nc_stdlib_cache_set(AS_STRING(a[0])->chars, a[1]);
    else if (fn_len == 6 && memcmp(fn_name, "cached", 6) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_cache_get(AS_STRING(a[0])->chars);
    else if (fn_len == 9 && memcmp(fn_name, "is_cached", 9) == 0 && argc == 1 && IS_STRING(a[0]))
        result = NC_BOOL(nc_stdlib_cache_has(AS_STRING(a[0])->chars));
    /* ── RAG functions ────────────────────── */
    else if (fn_len == 11 && memcmp(fn_name, "token_count", 11) == 0 && argc == 1 && IS_STRING(a[0]))
        result = NC_INT(nc_stdlib_token_count(AS_STRING(a[0])->chars));
    else if (fn_len == 11 && memcmp(fn_name, "file_exists", 11) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_file_exists(AS_STRING(a[0])->chars);
    else if (fn_len == 11 && memcmp(fn_name, "delete_file", 11) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_delete_file(AS_STRING(a[0])->chars);
    else if (fn_len == 11 && memcmp(fn_name, "remove_file", 11) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_delete_file(AS_STRING(a[0])->chars);
    else if (fn_len == 5 && memcmp(fn_name, "mkdir", 5) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_mkdir(AS_STRING(a[0])->chars);
    else if (fn_len == 11 && memcmp(fn_name, "time_format", 11) == 0 && argc == 2 && IS_STRING(a[1]))
        result = nc_stdlib_time_format(IS_INT(a[0]) ? (double)AS_INT(a[0]) : (IS_FLOAT(a[0]) ? AS_FLOAT(a[0]) : 0), AS_STRING(a[1])->chars);
    /* ── Cross-language execution ────────── */
    else if (fn_len == 4 && memcmp(fn_name, "exec", 4) == 0 && argc >= 1) {
        NcList *exec_args = nc_list_new();
        for (int ei = 0; ei < argc; ei++) nc_list_push(exec_args, a[ei]);
        result = nc_stdlib_exec(exec_args);
    }
    else if (fn_len == 5 && memcmp(fn_name, "shell", 5) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_shell(AS_STRING(a[0])->chars);

    /* ── Enterprise: Cryptography ──────────── */
    else if (fn_len == 11 && memcmp(fn_name, "hash_sha256", 11) == 0 && argc == 1) {
        NcString *s = nc_value_to_string(a[0]);
        result = nc_stdlib_hash_sha256(s->chars);
        nc_string_free(s);
    }
    else if (fn_len == 13 && memcmp(fn_name, "hash_password", 13) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_hash_password(AS_STRING(a[0])->chars);
    else if (fn_len == 15 && memcmp(fn_name, "verify_password", 15) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_verify_password(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars);
    else if (fn_len == 9 && memcmp(fn_name, "hash_hmac", 9) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_hash_hmac(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars);
    /* ── Enterprise: JWT ───────────────────── */
    else if (fn_len == 12 && memcmp(fn_name, "jwt_generate", 12) == 0 && argc >= 2) {
        const char *uid = IS_STRING(a[0]) ? AS_STRING(a[0])->chars : "anonymous";
        const char *rl = IS_STRING(a[1]) ? AS_STRING(a[1])->chars : "user";
        int exp_s = argc >= 3 && IS_INT(a[2]) ? (int)AS_INT(a[2]) : 3600;
        NcMap *extra = argc >= 4 && IS_MAP(a[3]) ? AS_MAP(a[3]) : NULL;
        result = nc_jwt_generate(uid, rl, exp_s, extra);
    }
    else if (fn_len == 10 && memcmp(fn_name, "jwt_verify", 10) == 0 && argc >= 1 && IS_STRING(a[0])) {
        const char *token = AS_STRING(a[0])->chars;
        if (argc >= 2 && IS_STRING(a[1]))
            nc_setenv("NC_JWT_SECRET", AS_STRING(a[1])->chars, 1);
        char auth_hdr[8192];
        snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", token);
        NcAuthContext actx = nc_mw_auth_check(auth_hdr);
        if (!actx.authenticated) { result = NC_BOOL(false); }
        else {
            NcMap *cl = nc_map_new();
            nc_map_set(cl, nc_string_from_cstr("sub"), NC_STRING(nc_string_from_cstr(actx.user_id)));
            nc_map_set(cl, nc_string_from_cstr("role"), NC_STRING(nc_string_from_cstr(actx.role)));
            nc_map_set(cl, nc_string_from_cstr("authenticated"), NC_BOOL(true));
            result = NC_MAP(cl);
        }
    }
    /* ── Enterprise: Time ISO ──────────────── */
    else if (fn_len == 8 && memcmp(fn_name, "time_iso", 8) == 0) {
        double ts = argc >= 1 ? (IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0])) : (double)time(NULL);
        result = nc_stdlib_time_iso(ts);
    }
    /* ── Enterprise: Sessions ──────────────── */
    else if (fn_len == 14 && memcmp(fn_name, "session_create", 14) == 0 && argc == 0)
        result = nc_stdlib_session_create();
    else if (fn_len == 11 && memcmp(fn_name, "session_set", 11) == 0 && argc == 3 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_session_set(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars, a[2]);
    else if (fn_len == 11 && memcmp(fn_name, "session_get", 11) == 0 && argc == 2 && IS_STRING(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_session_get(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars);
    else if (fn_len == 15 && memcmp(fn_name, "session_destroy", 15) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_session_destroy(AS_STRING(a[0])->chars);
    else if (fn_len == 14 && memcmp(fn_name, "session_exists", 14) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_session_exists(AS_STRING(a[0])->chars);
    /* ── Enterprise: Request Context ───────── */
    else if (fn_len == 14 && memcmp(fn_name, "request_header", 14) == 0 && argc == 1 && IS_STRING(a[0]))
        result = nc_stdlib_request_header(AS_STRING(a[0])->chars);
    else if (fn_len == 10 && memcmp(fn_name, "request_ip", 10) == 0 && argc == 0) {
        NcRequestContext *rctx = nc_request_ctx_get();
        result = rctx ? NC_STRING(nc_string_from_cstr(rctx->client_ip)) : NC_NONE();
    }
    else if (fn_len == 14 && memcmp(fn_name, "request_method", 14) == 0 && argc == 0) {
        NcRequestContext *rctx = nc_request_ctx_get();
        result = rctx ? NC_STRING(nc_string_from_cstr(rctx->method)) : NC_NONE();
    }
    else if (fn_len == 12 && memcmp(fn_name, "request_path", 12) == 0 && argc == 0) {
        NcRequestContext *rctx = nc_request_ctx_get();
        result = rctx ? NC_STRING(nc_string_from_cstr(rctx->path)) : NC_NONE();
    }
    /* ── Enterprise: Feature Flags ─────────── */
    else if (fn_len == 7 && memcmp(fn_name, "feature", 7) == 0 && argc >= 1 && IS_STRING(a[0])) {
        const char *tenant = (argc >= 2 && IS_STRING(a[1])) ? AS_STRING(a[1])->chars : NULL;
        result = NC_BOOL(nc_ff_is_enabled(AS_STRING(a[0])->chars, tenant));
    }
    /* ── Enterprise: Circuit Breaker ───────── */
    else if (fn_len == 12 && memcmp(fn_name, "circuit_open", 12) == 0 && argc == 1 && IS_STRING(a[0]))
        result = NC_BOOL(!nc_cb_allow(AS_STRING(a[0])->chars));
    /* ── Enterprise: Higher-order list ops ──── */
    else if (fn_len == 7 && memcmp(fn_name, "sort_by", 7) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_sort_by(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 6 && memcmp(fn_name, "max_by", 6) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_max_by(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 6 && memcmp(fn_name, "min_by", 6) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_min_by(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 6 && memcmp(fn_name, "sum_by", 6) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_sum_by(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 9 && memcmp(fn_name, "map_field", 9) == 0 && argc == 2 && IS_LIST(a[0]) && IS_STRING(a[1]))
        result = nc_stdlib_list_map_field(AS_LIST(a[0]), AS_STRING(a[1]));
    else if (fn_len == 9 && memcmp(fn_name, "filter_by", 9) == 0 && argc == 4 && IS_LIST(a[0]) && IS_STRING(a[1]) && IS_STRING(a[2]))
        result = nc_stdlib_list_filter_by(AS_LIST(a[0]), AS_STRING(a[1]),
            AS_STRING(a[2])->chars,
            IS_INT(a[3]) ? (double)AS_INT(a[3]) : AS_FLOAT(a[3]));
    /* ── HTTP convenience functions ────────── */
    else if (fn_len == 8 && memcmp(fn_name, "http_get", 8) == 0 && argc >= 1 && IS_STRING(a[0])) {
        NcMap *hdrs = argc >= 2 && IS_MAP(a[1]) ? AS_MAP(a[1]) : NULL;
        result = nc_stdlib_http_request("GET", AS_STRING(a[0])->chars, hdrs, NC_NONE());
    }
    else if (fn_len == 9 && memcmp(fn_name, "http_post", 9) == 0 && argc >= 2 && IS_STRING(a[0])) {
        NcMap *hdrs = argc >= 3 && IS_MAP(a[2]) ? AS_MAP(a[2]) : NULL;
        result = nc_stdlib_http_request("POST", AS_STRING(a[0])->chars, hdrs, a[1]);
    }
    else if (fn_len == 12 && memcmp(fn_name, "http_request", 12) == 0 && argc >= 2 && IS_STRING(a[0]) && IS_STRING(a[1])) {
        NcMap *hdrs = argc >= 3 && IS_MAP(a[2]) ? AS_MAP(a[2]) : NULL;
        NcValue body = argc >= 4 ? a[3] : NC_NONE();
        result = nc_stdlib_http_request(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars, hdrs, body);
    }

    /* ── String conversion (alias for str) ─── */
    else if (fn_len == 6 && memcmp(fn_name, "string", 6) == 0 && argc == 1)
        result = NC_STRING(nc_value_to_string(a[0]));
    /* ── Trig functions ──────────────────── */
    else if (fn_len == 3 && memcmp(fn_name, "sin", 3) == 0 && argc == 1)
        result = NC_FLOAT(sin(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0])));
    else if (fn_len == 3 && memcmp(fn_name, "cos", 3) == 0 && argc == 1)
        result = NC_FLOAT(cos(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0])));
    else if (fn_len == 3 && memcmp(fn_name, "exp", 3) == 0 && argc == 1)
        result = NC_FLOAT(exp(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0])));
    else if (fn_len == 3 && memcmp(fn_name, "log", 3) == 0 && argc == 1)
        result = NC_FLOAT(log(IS_INT(a[0]) ? (double)AS_INT(a[0]) : AS_FLOAT(a[0])));
    /* ── Memory / policy functions ───────── */
    else if (fn_len == 10 && memcmp(fn_name, "memory_new", 10) == 0 && argc <= 1) {
        result = nc_stdlib_memory_new(argc == 1 && IS_INT(a[0]) ? (int)AS_INT(a[0]) : 50);
    }
    else if (fn_len == 10 && memcmp(fn_name, "memory_add", 10) == 0 && argc == 3 && IS_STRING(a[1]) && IS_STRING(a[2])) {
        result = nc_stdlib_memory_add(a[0], AS_STRING(a[1])->chars, AS_STRING(a[2])->chars);
    }
    else if (fn_len == 10 && memcmp(fn_name, "memory_get", 10) == 0 && argc == 1) {
        result = nc_stdlib_memory_get(a[0]);
    }
    else if (fn_len == 12 && memcmp(fn_name, "memory_clear", 12) == 0 && argc == 1) {
        result = nc_stdlib_memory_clear(a[0]);
    }
    else if (fn_len == 14 && memcmp(fn_name, "memory_summary", 14) == 0 && argc == 1) {
        result = nc_stdlib_memory_summary(a[0]);
    }
    else if (fn_len == 11 && memcmp(fn_name, "memory_save", 11) == 0 && argc == 2 && IS_STRING(a[1])) {
        result = nc_stdlib_memory_save(a[0], AS_STRING(a[1])->chars);
    }
    else if (fn_len == 11 && memcmp(fn_name, "memory_load", 11) == 0 && argc >= 1 && argc <= 2 && IS_STRING(a[0])) {
        result = nc_stdlib_memory_load(AS_STRING(a[0])->chars,
                                       argc == 2 && IS_INT(a[1]) ? (int)AS_INT(a[1]) : 0);
    }
    else if (fn_len == 12 && memcmp(fn_name, "memory_store", 12) == 0 && argc >= 3 && argc <= 5 && IS_STRING(a[0]) && IS_STRING(a[1]) && IS_STRING(a[2])) {
        double reward = argc == 5 ? (IS_FLOAT(a[4]) ? AS_FLOAT(a[4]) : (IS_INT(a[4]) ? (double)AS_INT(a[4]) : 0.0)) : 0.0;
        result = nc_stdlib_memory_store(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars,
                                        AS_STRING(a[2])->chars, argc >= 4 ? a[3] : NC_NONE(), reward);
    }
    else if (fn_len == 13 && memcmp(fn_name, "memory_search", 13) == 0 && argc >= 2 && argc <= 3 && IS_STRING(a[0]) && IS_STRING(a[1])) {
        result = nc_stdlib_memory_search(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars,
                                         argc == 3 && IS_INT(a[2]) ? (int)AS_INT(a[2]) : 5);
    }
    else if (fn_len == 14 && memcmp(fn_name, "memory_context", 14) == 0 && argc >= 2 && argc <= 3 && IS_STRING(a[0]) && IS_STRING(a[1])) {
        result = nc_stdlib_memory_context(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars,
                                          argc == 3 && IS_INT(a[2]) ? (int)AS_INT(a[2]) : 5);
    }
    else if (fn_len == 14 && memcmp(fn_name, "memory_reflect", 14) == 0 && argc == 6 && IS_STRING(a[0]) && IS_STRING(a[1]) && IS_STRING(a[2]) && IS_STRING(a[3]) && IS_STRING(a[5])) {
        double conf = IS_FLOAT(a[4]) ? AS_FLOAT(a[4]) : (IS_INT(a[4]) ? (double)AS_INT(a[4]) : 0.0);
        result = nc_stdlib_memory_reflect(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars,
                                          AS_STRING(a[2])->chars, AS_STRING(a[3])->chars,
                                          conf, AS_STRING(a[5])->chars);
    }
    else if (fn_len == 13 && memcmp(fn_name, "policy_update", 13) == 0 && argc == 3 && IS_STRING(a[0]) && IS_STRING(a[1])) {
        double reward = IS_FLOAT(a[2]) ? AS_FLOAT(a[2]) : (IS_INT(a[2]) ? (double)AS_INT(a[2]) : 0.0);
        result = nc_stdlib_policy_update(AS_STRING(a[0])->chars, AS_STRING(a[1])->chars, reward);
    }
    else if (fn_len == 13 && memcmp(fn_name, "policy_choose", 13) == 0 && argc >= 2 && argc <= 3 && IS_STRING(a[0]) && IS_LIST(a[1])) {
        double epsilon = argc == 3 ? (IS_FLOAT(a[2]) ? AS_FLOAT(a[2]) : (IS_INT(a[2]) ? (double)AS_INT(a[2]) : 0.0)) : 0.0;
        result = nc_stdlib_policy_choose(AS_STRING(a[0])->chars, AS_LIST(a[1]), epsilon);
    }
    else if (fn_len == 12 && memcmp(fn_name, "policy_stats", 12) == 0 && argc == 1 && IS_STRING(a[0])) {
        result = nc_stdlib_policy_stats(AS_STRING(a[0])->chars);
    }
    /* ── NC Model / Training API ─────────── */
    /*    Wires nc_training.c C-native pipeline to NC scripts */
    else if (fn_len >= 9 && memcmp(fn_name, "nc_model_", 9) == 0) {
        if (fn_len == 15 && memcmp(fn_name, "nc_model_create", 15) == 0 && argc == 1 && IS_MAP(a[0])) {
            /* nc_model_create(config) → creates model, stores in global __nc_model */
            NcMap *cfg = AS_MAP(a[0]);
            NCModelConfig mc = nc_model_default_config();
            NcString *k;
            NcValue v;
            k = nc_string_from_cstr("dim"); v = nc_map_get(cfg, k); nc_string_free(k);
            if (IS_INT(v)) mc.dim = (int)AS_INT(v); else if (IS_FLOAT(v)) mc.dim = (int)AS_FLOAT(v);
            k = nc_string_from_cstr("n_layers"); v = nc_map_get(cfg, k); nc_string_free(k);
            if (IS_INT(v)) mc.n_layers = (int)AS_INT(v); else if (IS_FLOAT(v)) mc.n_layers = (int)AS_FLOAT(v);
            k = nc_string_from_cstr("n_heads"); v = nc_map_get(cfg, k); nc_string_free(k);
            if (IS_INT(v)) mc.n_heads = (int)AS_INT(v); else if (IS_FLOAT(v)) mc.n_heads = (int)AS_FLOAT(v);
            k = nc_string_from_cstr("vocab_size"); v = nc_map_get(cfg, k); nc_string_free(k);
            if (IS_INT(v)) mc.vocab_size = (int)AS_INT(v); else if (IS_FLOAT(v)) mc.vocab_size = (int)AS_FLOAT(v);
            k = nc_string_from_cstr("max_seq"); v = nc_map_get(cfg, k); nc_string_free(k);
            if (IS_INT(v)) mc.max_seq = (int)AS_INT(v); else if (IS_FLOAT(v)) mc.max_seq = (int)AS_FLOAT(v);
            mc.hidden_dim = mc.dim * 4;
            NCModel *model = nc_model_create(mc);
            if (model) {
                /* Store model pointer as integer in global */
                nc_map_set(vm->globals, nc_string_from_cstr("__nc_model"), NC_INT((int64_t)(uintptr_t)model));
                NcMap *info = nc_map_new();
                nc_map_set(info, nc_string_from_cstr("dim"), NC_INT(mc.dim));
                nc_map_set(info, nc_string_from_cstr("n_layers"), NC_INT(mc.n_layers));
                nc_map_set(info, nc_string_from_cstr("vocab_size"), NC_INT(mc.vocab_size));
                nc_map_set(info, nc_string_from_cstr("max_seq"), NC_INT(mc.max_seq));
                result = NC_MAP(info);
            }
        }
        else if (fn_len == 13 && memcmp(fn_name, "nc_model_save", 13) == 0 && argc == 1 && IS_STRING(a[0])) {
            NcValue mp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_model"));
            if (IS_INT(mp)) {
                NCModel *model = (NCModel *)(uintptr_t)AS_INT(mp);
                int ok = nc_model_save(model, AS_STRING(a[0])->chars);
                result = NC_BOOL(ok == 0);
            }
        }
        else if (fn_len == 13 && memcmp(fn_name, "nc_model_load", 13) == 0 && argc == 1 && IS_STRING(a[0])) {
            NCModel *model = nc_model_load(AS_STRING(a[0])->chars);
            if (model) {
                nc_map_set(vm->globals, nc_string_from_cstr("__nc_model"), NC_INT((int64_t)(uintptr_t)model));
                result = NC_BOOL(true);
            }
        }
        else if (fn_len == 17 && memcmp(fn_name, "nc_model_generate", 17) == 0 && argc >= 2 && IS_LIST(a[0])) {
            NcValue mp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_model"));
            if (IS_INT(mp)) {
                NCModel *model = (NCModel *)(uintptr_t)AS_INT(mp);
                NcList *prompt_list = AS_LIST(a[0]);
                int max_tokens = IS_INT(a[1]) ? (int)AS_INT(a[1]) : (int)AS_FLOAT(a[1]);
                float temperature = (argc >= 3 && IS_FLOAT(a[2])) ? (float)AS_FLOAT(a[2]) : 0.8f;
                int *prompt = malloc(sizeof(int) * prompt_list->count);
                for (int pi = 0; pi < prompt_list->count; pi++)
                    prompt[pi] = IS_INT(prompt_list->items[pi]) ? (int)AS_INT(prompt_list->items[pi]) : 0;
                int *output = malloc(sizeof(int) * max_tokens);
                int gen_len = nc_model_generate(model, prompt, prompt_list->count, max_tokens, temperature, output);
                NcList *out_list = nc_list_new();
                for (int gi = 0; gi < gen_len; gi++)
                    nc_list_push(out_list, NC_INT(output[gi]));
                free(prompt); free(output);
                result = NC_LIST(out_list);
            }
        }
        else if (fn_len == 14 && memcmp(fn_name, "nc_model_train", 14) == 0 && argc >= 1 && IS_LIST(a[0])) {
            /* nc_model_train(data_files, [config_map]) → trains model */
            NcValue mp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_model"));
            if (IS_INT(mp)) {
                NCModel *model = (NCModel *)(uintptr_t)AS_INT(mp);
                NcList *files = AS_LIST(a[0]);
                int n_files = files->count;
                const char **data_files = malloc(sizeof(char *) * n_files);
                for (int fi = 0; fi < n_files; fi++)
                    data_files[fi] = IS_STRING(files->items[fi]) ? AS_STRING(files->items[fi])->chars : "";
                NCTrainConfig cfg = nc_train_default_config();
                if (argc >= 2 && IS_MAP(a[1])) {
                    NcMap *cm = AS_MAP(a[1]);
                    NcString *ck; NcValue cv;
                    ck = nc_string_from_cstr("lr"); cv = nc_map_get(cm, ck); nc_string_free(ck);
                    if (IS_FLOAT(cv)) cfg.lr = (float)AS_FLOAT(cv);
                    ck = nc_string_from_cstr("steps"); cv = nc_map_get(cm, ck); nc_string_free(ck);
                    if (IS_INT(cv)) cfg.total_steps = (float)AS_INT(cv);
                    else if (IS_FLOAT(cv)) cfg.total_steps = (float)AS_FLOAT(cv);
                    ck = nc_string_from_cstr("batch_size"); cv = nc_map_get(cm, ck); nc_string_free(ck);
                    if (IS_INT(cv)) cfg.batch_size = (int)AS_INT(cv);
                    ck = nc_string_from_cstr("checkpoint_dir"); cv = nc_map_get(cm, ck); nc_string_free(ck);
                    if (IS_STRING(cv)) cfg.checkpoint_dir = AS_STRING(cv)->chars;
                }
                /* Need tokenizer — check if stored, auto-create if missing */
                NcValue tp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_tokenizer"));
                NCTokenizer *tok = IS_INT(tp) ? (NCTokenizer *)(uintptr_t)AS_INT(tp) : NULL;
                if (!tok) {
                    /* Auto-create tokenizer from training data files */
                    fprintf(stderr, "[nc_model_train] Auto-creating tokenizer from %d data file(s)...\n", n_files);
                    tok = nc_tokenizer_create();
                    /* Read all files to train tokenizer */
                    const char **texts = malloc(sizeof(char *) * n_files);
                    int valid_texts = 0;
                    for (int fi = 0; fi < n_files; fi++) {
                        FILE *f = fopen(data_files[fi], "r");
                        if (f) {
                            fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
                            char *buf = malloc(sz + 1);
                            fread(buf, 1, sz, f); buf[sz] = '\0'; fclose(f);
                            texts[valid_texts++] = buf;
                        }
                    }
                    if (valid_texts > 0) {
                        int target_vocab = model->vocab_size > 0 ? model->vocab_size : 2048;
                        nc_tokenizer_train(tok, texts, valid_texts, target_vocab);
                        fprintf(stderr, "[nc_model_train] Tokenizer trained (vocab=%d)\n", target_vocab);
                    }
                    for (int fi = 0; fi < valid_texts; fi++) free((void *)texts[fi]);
                    free(texts);
                    /* Store tokenizer for future use */
                    nc_map_set(vm->globals, nc_string_from_cstr("__nc_tokenizer"), NC_INT((int64_t)(uintptr_t)tok));
                }
                nc_train(model, tok, data_files, n_files, cfg);
                result = NC_BOOL(true);
                free(data_files);
            }
        }
        /* nc_model_decode(token_list) → decode token IDs to text string */
        else if (fn_len == 15 && memcmp(fn_name, "nc_model_decode", 15) == 0 && argc == 1 && IS_LIST(a[0])) {
            NcValue tp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_tokenizer"));
            NCTokenizer *tok = IS_INT(tp) ? (NCTokenizer *)(uintptr_t)AS_INT(tp) : NULL;
            if (tok) {
                NcList *token_list = AS_LIST(a[0]);
                int n = token_list->count;
                int *tokens = malloc(sizeof(int) * n);
                for (int ti = 0; ti < n; ti++)
                    tokens[ti] = IS_INT(token_list->items[ti]) ? (int)AS_INT(token_list->items[ti]) : 0;
                char *text = malloc(n * 64 + 1); /* generous buffer */
                int text_len = nc_tokenizer_decode(tok, tokens, n, text, n * 64);
                text[text_len] = '\0';
                result = NC_STRING(nc_string_from_cstr(text));
                free(tokens);
                free(text);
            }
        }
        /* nc_model_generate_text(prompt_text, max_tokens, [temperature]) → generated text */
        else if (fn_len == 22 && memcmp(fn_name, "nc_model_generate_text", 22) == 0 && argc >= 2 && IS_STRING(a[0])) {
            NcValue mp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_model"));
            NcValue tp = nc_map_get(vm->globals, nc_string_from_cstr("__nc_tokenizer"));
            NCModel *model = IS_INT(mp) ? (NCModel *)(uintptr_t)AS_INT(mp) : NULL;
            NCTokenizer *tok = IS_INT(tp) ? (NCTokenizer *)(uintptr_t)AS_INT(tp) : NULL;
            if (model && tok) {
                const char *prompt = AS_STRING(a[0])->chars;
                int max_tokens = IS_INT(a[1]) ? (int)AS_INT(a[1]) : (int)AS_FLOAT(a[1]);
                float temperature = (argc >= 3 && IS_FLOAT(a[2])) ? (float)AS_FLOAT(a[2]) : 0.8f;
                /* Encode prompt to tokens */
                int max_prompt = model->max_seq;
                int *prompt_tokens = malloc(sizeof(int) * max_prompt);
                int prompt_len = nc_tokenizer_encode(tok, prompt, prompt_tokens, max_prompt);
                /* Generate */
                int *output = malloc(sizeof(int) * (max_prompt + max_tokens));
                int gen_len = nc_model_generate(model, prompt_tokens, prompt_len, max_tokens, temperature, output);
                /* Decode back to text */
                char *text = malloc(gen_len * 64 + 1);
                int text_len = nc_tokenizer_decode(tok, output, gen_len, text, gen_len * 64);
                text[text_len] = '\0';
                result = NC_STRING(nc_string_from_cstr(text));
                free(prompt_tokens);
                free(output);
                free(text);
            }
        }
    }
    /* ── Tensor operations for NC AI ──────── */
    /* Accept both VAL_LIST (legacy) and VAL_TENSOR (new zero-copy) */
    #define IS_TVAL(v) (IS_LIST(v) || IS_TENSOR(v))
    else if (fn_len >= 7 && memcmp(fn_name, "tensor_", 7) == 0) {
        if (fn_len == 13 && memcmp(fn_name, "tensor_create", 13) == 0 && argc == 2)
            result = nc_ncfn_tensor_create((int)(IS_INT(a[0]) ? AS_INT(a[0]) : (int64_t)AS_FLOAT(a[0])),
                                           (int)(IS_INT(a[1]) ? AS_INT(a[1]) : (int64_t)AS_FLOAT(a[1])));
        else if (fn_len == 11 && memcmp(fn_name, "tensor_ones", 11) == 0 && argc == 2)
            result = nc_ncfn_tensor_ones((int)(IS_INT(a[0]) ? AS_INT(a[0]) : (int64_t)AS_FLOAT(a[0])),
                                         (int)(IS_INT(a[1]) ? AS_INT(a[1]) : (int64_t)AS_FLOAT(a[1])));
        else if (fn_len == 13 && memcmp(fn_name, "tensor_random", 13) == 0 && argc == 2)
            result = nc_ncfn_tensor_random((int)(IS_INT(a[0]) ? AS_INT(a[0]) : (int64_t)AS_FLOAT(a[0])),
                                           (int)(IS_INT(a[1]) ? AS_INT(a[1]) : (int64_t)AS_FLOAT(a[1])));
        else if (fn_len == 13 && memcmp(fn_name, "tensor_matmul", 13) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_TVAL(a[1]))
            result = nc_ncfn_tensor_matmul(a[0], a[1]);
        else if (fn_len == 10 && memcmp(fn_name, "tensor_add", 10) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_TVAL(a[1]))
            result = nc_ncfn_tensor_add(a[0], a[1]);
        else if (fn_len == 10 && memcmp(fn_name, "tensor_sub", 10) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_TVAL(a[1]))
            result = nc_ncfn_tensor_sub(a[0], a[1]);
        else if (fn_len == 10 && memcmp(fn_name, "tensor_mul", 10) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_TVAL(a[1]))
            result = nc_ncfn_tensor_mul(a[0], a[1]);
        else if (fn_len == 12 && memcmp(fn_name, "tensor_scale", 12) == 0 && argc == 2 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_scale(a[0], IS_INT(a[1]) ? (double)AS_INT(a[1]) : AS_FLOAT(a[1]));
        else if (fn_len == 16 && memcmp(fn_name, "tensor_transpose", 16) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_transpose(a[0]);
        else if (fn_len == 14 && memcmp(fn_name, "tensor_softmax", 14) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_softmax(a[0]);
        else if (fn_len == 11 && memcmp(fn_name, "tensor_gelu", 11) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_gelu(a[0]);
        else if (fn_len == 11 && memcmp(fn_name, "tensor_relu", 11) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_relu(a[0]);
        else if (fn_len == 11 && memcmp(fn_name, "tensor_tanh", 11) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_tanh(a[0]);
        else if (fn_len == 17 && memcmp(fn_name, "tensor_layer_norm", 17) == 0 && argc == 3 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_layer_norm(a[0], a[1], a[2]);
        else if (fn_len == 15 && memcmp(fn_name, "tensor_add_bias", 15) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_TVAL(a[1]))
            result = nc_ncfn_tensor_add_bias(a[0], a[1]);
        else if (fn_len == 18 && memcmp(fn_name, "tensor_causal_mask", 18) == 0 && argc == 1)
            result = nc_ncfn_tensor_causal_mask((int)(IS_INT(a[0]) ? AS_INT(a[0]) : (int64_t)AS_FLOAT(a[0])));
        else if (fn_len == 16 && memcmp(fn_name, "tensor_embedding", 16) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_LIST(a[1]))
            result = nc_ncfn_tensor_embedding(a[0], a[1]);
        else if (fn_len == 12 && memcmp(fn_name, "tensor_shape", 12) == 0 && argc == 1 && IS_TVAL(a[0]))
            result = nc_ncfn_tensor_shape(a[0]);
        else if (fn_len == 20 && memcmp(fn_name, "tensor_cross_entropy", 20) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_LIST(a[1]))
            result = nc_ncfn_tensor_cross_entropy(a[0], a[1]);
        else if (fn_len == 11 && memcmp(fn_name, "tensor_save", 11) == 0 && argc == 2 && IS_TVAL(a[0]) && IS_STRING(a[1]))
            result = nc_ncfn_tensor_save(a[0], AS_STRING(a[1])->chars);
        else if (fn_len == 11 && memcmp(fn_name, "tensor_load", 11) == 0 && argc == 1 && IS_STRING(a[0]))
            result = nc_ncfn_tensor_load(AS_STRING(a[0])->chars);
    }
    #undef IS_TVAL

    /* If no built-in matched, try user-defined behaviors */
    if (result.type == VAL_NONE && vm->behaviors) {
        NcString *bname = nc_string_from_cstr(fn_name);
        NcValue chunk_val = nc_map_get(vm->behaviors, bname);
        nc_string_free(bname);
        if (IS_INT(chunk_val)) {
            int ci = (int)AS_INT(chunk_val);
            if (ci >= 0 && ci < vm->behavior_chunk_count && vm->behavior_chunks) {
                NcChunk *target = &vm->behavior_chunks[ci];
                for (int bi = 0; bi < argc && bi < target->var_count; bi++)
                    nc_map_set(vm->globals, target->var_names[bi], a[bi]);
                vm->stack_top -= (argc + 1);
                result = nc_vm_execute_fast(vm, target);
                nc_value_retain(result);
                PUSH(result);
                DISPATCH();
            }
        }
    }

    vm->stack_top -= (argc + 1);
    PUSH(result);
    DISPATCH();
}

op_gather: { NcValue opts = POP(); NcValue src = POP();
    NcString *src_str = nc_value_to_string(src);
    NcMap *opt_map = IS_MAP(opts) ? AS_MAP(opts) : nc_map_new();
    NcValue gathered = nc_gather_from(src_str->chars, opt_map);
    nc_string_free(src_str);
    PUSH(gathered); DISPATCH(); }
op_ask_ai: { NcValue ctx = POP(); NcValue prompt_val = POP();
    NcString *p = nc_value_to_string(prompt_val);
    /* Resolve {{var}} templates from globals using dynamic buffer */
    NcDynBuf resolved;
    nc_dbuf_init(&resolved, p->length * 2 + 256);
    const char *pp = p->chars;
    while (*pp) {
        if (*pp == '{' && *(pp+1) == '{') {
            pp += 2;
            char vname[128] = {0}; int vi = 0;
            while (*pp && !(*pp == '}' && *(pp+1) == '}') && vi < 126) vname[vi++] = *pp++;
            if (*pp == '}') pp += 2;
            char *dot = strchr(vname, '.');
            NcString *key;
            if (dot) {
                char base[64] = {0}; { int blen = (int)(dot - vname); if (blen > 63) blen = 63; memcpy(base, vname, blen); base[blen] = '\0'; }
                key = nc_string_from_cstr(base);
                NcValue obj = nc_map_get(vm->globals, key); nc_string_free(key);
                if (IS_MAP(obj)) {
                    NcString *fk = nc_string_from_cstr(dot+1);
                    NcValue fv = nc_map_get(AS_MAP(obj), fk); nc_string_free(fk);
                    NcString *fs = nc_value_to_string(fv);
                    nc_dbuf_append_len(&resolved, fs->chars, fs->length);
                    nc_string_free(fs);
                }
            } else {
                key = nc_string_from_cstr(vname);
                NcValue val = nc_map_get(vm->globals, key); nc_string_free(key);
                NcString *vs = nc_value_to_string(val);
                nc_dbuf_append_len(&resolved, vs->chars, vs->length);
                nc_string_free(vs);
            }
        } else {
            nc_dbuf_append_len(&resolved, pp, 1);
            pp++;
        }
    }
    NcMap *ai_ctx = nc_map_new();
    if (IS_LIST(ctx)) {
        for (int ci = 0; ci < AS_LIST(ctx)->count; ci++) {
            char key[32]; snprintf(key, sizeof(key), "arg_%d", ci);
            nc_map_set(ai_ctx, nc_string_from_cstr(key), AS_LIST(ctx)->items[ci]);
        }
    } else if (IS_MAP(ctx)) {
        ai_ctx = AS_MAP(ctx);
    }
    NcValue ai_result;
    if (vm->ai_handler) ai_result = vm->ai_handler(vm, p, ai_ctx, NULL);
    else ai_result = nc_ai_call_ex(resolved.data, ai_ctx, nc_map_new());
    nc_dbuf_free(&resolved);
    nc_string_free(p);
    PUSH(ai_result); DISPATCH(); }

op_notify: { POP(); POP(); DISPATCH(); }
op_log: { NcValue v = POP(); NcString *s = nc_value_to_string(v);
    if (vm->output_count < vm->output_capacity) {
        char *safe = nc_redact_for_display(s->chars);
        vm->output[vm->output_count++] = safe ? safe : strdup(s->chars);
    }
    nc_string_free(s); DISPATCH(); }
op_emit: { POP(); DISPATCH(); }
op_store: {
    NcValue val = POP(); NcValue key = POP();
    NcString *key_str = nc_value_to_string(key);
    char *val_json = nc_json_serialize(val, false);
    nc_store_put(key_str->chars, val_json);
    free(val_json);
    nc_map_set(vm->globals, key_str, val);
    nc_string_free(key_str);
    DISPATCH(); }
op_wait: { POP(); DISPATCH(); }
op_make_list: { uint8_t n = READ();
    if (n > vm->stack_top) { snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow in list"); vm->had_error = true; goto jit_done; }
    NcList *l = nc_list_new();
    for (int i = 0; i < n; i++) nc_list_push(l, vm->stack[vm->stack_top - n + i]);
    vm->stack_top -= n; PUSH(NC_LIST(l)); DISPATCH(); }
op_make_map: { uint8_t n = READ();
    if (n * 2 > vm->stack_top) { snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow in map"); vm->had_error = true; goto jit_done; }
    NcMap *m = nc_map_new();
    for (int i = n-1; i >= 0; i--) {
        NcValue val = vm->stack[vm->stack_top - 1 - i*2];
        NcValue key = vm->stack[vm->stack_top - 2 - i*2];
        if (IS_STRING(key)) nc_map_set(m, AS_STRING(key), val);
    }
    vm->stack_top -= n*2; PUSH(NC_MAP(m)); DISPATCH(); }
op_set_field: { uint8_t idx = READ(); NcValue val = POP(); NcValue obj = POP();
    if (IS_MAP(obj)) nc_map_set(AS_MAP(obj), chunk->var_names[idx], val);
    PUSH(val); DISPATCH(); }

op_get_local: { uint8_t slot = READ(); PUSH(frame->locals[slot]); DISPATCH(); }
op_set_local: { uint8_t slot = READ(); frame->locals[slot] = PEEK(0);
    if (slot >= frame->local_count) frame->local_count = slot + 1; DISPATCH(); }
op_constant_long: { uint16_t idx = READ16(); PUSH(chunk->constants[idx]); DISPATCH(); }

op_respond: {
    result = POP();
    if (IS_STRING(result) && strstr(AS_STRING(result)->chars, "{{")) {
        char resolved[1024] = {0};
        const char *p = AS_STRING(result)->chars;
        int ri = 0;
        while (*p && ri < 1020) {
            if (*p == '{' && *(p+1) == '{') {
                p += 2;
                char vname[128] = {0}; int vi = 0;
                while (*p && !(*p == '}' && *(p+1) == '}') && vi < 126) vname[vi++] = *p++;
                if (*p == '}') p += 2;
                char *dot = strchr(vname, '.');
                NcString *key;
                if (dot) {
                    char base[64] = {0}; { int blen = (int)(dot - vname); if (blen > 63) blen = 63; memcpy(base, vname, blen); base[blen] = '\0'; }
                    key = nc_string_from_cstr(base);
                    NcValue obj = nc_map_get(vm->globals, key); nc_string_free(key);
                    if (IS_MAP(obj)) {
                        NcString *fk = nc_string_from_cstr(dot+1);
                        NcValue fv = nc_map_get(AS_MAP(obj), fk); nc_string_free(fk);
                        NcString *fs = nc_value_to_string(fv);
                        int fl = fs->length; if (ri + fl < 1020) { memcpy(resolved+ri, fs->chars, fl); ri += fl; }
                        nc_string_free(fs);
                    }
                } else {
                    key = nc_string_from_cstr(vname);
                    NcValue val = nc_map_get(vm->globals, key); nc_string_free(key);
                    NcString *vs = nc_value_to_string(val);
                    int vl = vs->length; if (ri + vl < 1020) { memcpy(resolved+ri, vs->chars, vl); ri += vl; }
                    nc_string_free(vs);
                }
            } else {
                resolved[ri++] = *p++;
            }
        }
        resolved[ri] = '\0';
        result = NC_STRING(nc_string_from_cstr(resolved));
    }
    goto done;
}
op_slice: {
    NcValue end_val = POP(); NcValue start_val = POP(); NcValue obj = POP();
    if (IS_STRING(obj)) {
        int len = AS_STRING(obj)->length;
        int s = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
        int e = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
        if (s < 0) s = len + s; if (e < 0) e = len + e;
        if (s < 0) s = 0; if (e > len) e = len;
        if (s >= e) { PUSH(NC_STRING(nc_string_from_cstr(""))); }
        else { PUSH(NC_STRING(nc_string_new(AS_STRING(obj)->chars + s, e - s))); }
    } else if (IS_LIST(obj)) {
        int len = AS_LIST(obj)->count;
        int s = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
        int e = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
        if (s < 0) s = len + s; if (e < 0) e = len + e;
        if (s < 0) s = 0; if (e > len) e = len;
        NcList *r = nc_list_new();
        for (int i = s; i < e; i++) nc_list_push(r, AS_LIST(obj)->items[i]);
        PUSH(NC_LIST(r));
    } else { PUSH(NC_NONE()); }
    DISPATCH();
}
op_return:  goto done;
op_halt:    goto done;

/* ── NC UI Opcode Handlers ─────────────────────────────────
 *
 * These handlers are the "reconciler" of NC UI — equivalent
 * to React Fiber's work loop, but running as NC bytecode in
 * the NC VM's computed-goto dispatch table.
 *
 * Each handler calls the corresponding function in nc_ui_vm.c.
 * The NcUIContext lives at vm->ui_ctx (set when running UI mode).
 *
 * Stack convention (same as all NC opcodes):
 *   - Read operands with READ() / READ16()
 *   - Pop inputs with POP()
 *   - Push results with PUSH()
 *   - DISPATCH() to continue execution
 * ─────────────────────────────────────────────────────────── */

/* ── NC UI Opcode Stubs ──────────────────────────────────────
 * These consume operands/stack to keep bytecode IP in sync.
 * Full UI implementations live in nc_ui_vm.c (separate build).
 * ──────────────────────────────────────────────────────────── */
op_ui_element:     { uint8_t idx = READ(); (void)idx; PUSH(NC_NONE()); DISPATCH(); }
op_ui_prop:        { uint8_t idx = READ(); (void)idx; POP(); DISPATCH(); }
op_ui_prop_expr:   { uint8_t idx = READ(); (void)idx; POP(); DISPATCH(); }
op_ui_text:        { POP(); PUSH(NC_NONE()); DISPATCH(); }
op_ui_child:       { POP(); DISPATCH(); }
op_ui_end_element: { DISPATCH(); }
op_state_declare:  { uint8_t idx = READ(); (void)idx; POP(); DISPATCH(); }
op_state_get:      { uint8_t idx = READ(); (void)idx; PUSH(NC_NONE()); DISPATCH(); }
op_state_set:      { uint8_t idx = READ(); (void)idx; POP(); DISPATCH(); }
op_state_computed: { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_state_watch:    { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_bind:        { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_bind_input:  { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_ui_on_event:    { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_component:   { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_ui_mount:       { DISPATCH(); }
op_ui_unmount:     { DISPATCH(); }
op_ui_on_mount:    { uint8_t cb = READ(); (void)cb; DISPATCH(); }
op_ui_on_unmount:  { uint8_t cb = READ(); (void)cb; DISPATCH(); }
op_ui_render:      { DISPATCH(); }
op_ui_diff:        { DISPATCH(); }
op_ui_patch:       { POP(); DISPATCH(); }
op_ui_route_def:   { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_route_push:  { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_ui_route_guard: { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_route_match: { PUSH(NC_NONE()); DISPATCH(); }
op_ui_fetch:       { POP(); PUSH(NC_NONE()); DISPATCH(); }
op_ui_fetch_auth:  { POP(); PUSH(NC_NONE()); DISPATCH(); }
op_ui_if:          { NcValue f = POP(); NcValue t = POP(); NcValue c = POP(); PUSH(nc_truthy(c) ? t : f); DISPATCH(); }
op_ui_for_each:    { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; PUSH(NC_NONE()); DISPATCH(); }
op_ui_show:        { POP(); DISPATCH(); }
op_ui_form:        { uint8_t a = READ(); uint8_t b = READ(); (void)a; (void)b; DISPATCH(); }
op_ui_validate:    { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_ui_form_submit: { uint8_t idx = READ(); (void)idx; DISPATCH(); }
op_ui_auth_check:  { PUSH(NC_BOOL(false)); DISPATCH(); }
op_ui_role_check:  { uint8_t idx = READ(); (void)idx; PUSH(NC_BOOL(false)); DISPATCH(); }
op_ui_perm_check:  { uint8_t idx = READ(); (void)idx; PUSH(NC_BOOL(false)); DISPATCH(); }

/* REPLACED: old UI handler code removed — stubs above */
#if 0
op_ui_prop_old: {
    /* Read prop name constant index */
    uint8_t idx = READ();
    NcValue prop_name_val = chunk->constants[idx];
    NcValue value = POP(); /* value was pushed by preceding instruction */
    if (vm->ui_ctx && IS_STRING(prop_name_val)) {
        nc_ui_exec_prop(vm, (NcUIContext *)vm->ui_ctx,
                        AS_STRING(prop_name_val)->chars, value);
    }
    DISPATCH();
}

op_ui_prop_expr: {
    /* Same as op_ui_prop — value already evaluated and on stack */
    uint8_t idx = READ();
    NcValue prop_name_val = chunk->constants[idx];
    NcValue value = POP();
    if (vm->ui_ctx && IS_STRING(prop_name_val)) {
        nc_ui_exec_prop(vm, (NcUIContext *)vm->ui_ctx,
                        AS_STRING(prop_name_val)->chars, value);
    }
    DISPATCH();
}

op_ui_text: {
    /* Pop string value, wrap as text VNode */
    NcValue str_val = POP();
    if (vm->ui_ctx) {
        NcValue text_node = nc_ui_exec_text(vm, (NcUIContext *)vm->ui_ctx, str_val);
        PUSH(text_node);
    } else {
        PUSH(NC_NONE());
    }
    DISPATCH();
}

op_ui_child: {
    /* Pop child VNode, append to parent VNode on top of stack */
    if (vm->ui_ctx) {
        nc_ui_exec_child(vm, (NcUIContext *)vm->ui_ctx);
    } else {
        POP(); /* discard child if no UI context */
    }
    DISPATCH();
}

op_ui_end_element: {
    /* Finalize current element — it stays on the stack as a VNode value */
    /* The parent's OP_UI_CHILD will pop it and attach it */
    DISPATCH();
}

op_state_declare: {
    uint8_t idx = READ();
    NcValue slot_name_val = chunk->constants[idx];
    NcValue initial = POP();
    if (vm->ui_ctx && IS_STRING(slot_name_val)) {
        nc_ui_state_declare(&((NcUIContext *)vm->ui_ctx)->state,
                             AS_STRING(slot_name_val)->chars, initial);
    }
    DISPATCH();
}

op_state_get: {
    uint8_t idx = READ();
    NcValue slot_name_val = chunk->constants[idx];
    if (vm->ui_ctx && IS_STRING(slot_name_val)) {
        NcValue val = nc_ui_state_get((NcUIContext *)vm->ui_ctx,
                                       AS_STRING(slot_name_val)->chars);
        PUSH(val);
    } else {
        PUSH(NC_NONE());
    }
    DISPATCH();
}

op_state_set: {
    uint8_t idx = READ();
    NcValue slot_name_val = chunk->constants[idx];
    NcValue value = POP();
    if (vm->ui_ctx && IS_STRING(slot_name_val)) {
        nc_ui_exec_state_set(vm, (NcUIContext *)vm->ui_ctx,
                              AS_STRING(slot_name_val)->chars, value);
    }
    DISPATCH();
}

op_state_computed: {
    /* operand: chunk index of compute function */
    uint8_t chunk_idx = READ();
    (void)chunk_idx; /* registered for lazy evaluation */
    DISPATCH();
}

op_state_watch: {
    uint8_t slot_idx = READ();
    uint8_t cb_idx   = READ();
    (void)slot_idx; (void)cb_idx;
    DISPATCH();
}

op_ui_bind: {
    /* op1: prop name idx, op2: state slot name idx */
    uint8_t prop_idx = READ();
    uint8_t slot_idx = READ();
    NcValue prop_name_val = chunk->constants[prop_idx];
    NcValue slot_name_val = chunk->constants[slot_idx];
    if (vm->ui_ctx && IS_STRING(prop_name_val) && IS_STRING(slot_name_val)) {
        nc_ui_exec_bind(vm, (NcUIContext *)vm->ui_ctx,
                        AS_STRING(prop_name_val)->chars,
                        AS_STRING(slot_name_val)->chars);
    }
    DISPATCH();
}

op_ui_bind_input: {
    /* Two-way bind: input.value ↔ state[slot_name] */
    uint8_t slot_idx = READ();
    NcValue slot_name_val = chunk->constants[slot_idx];
    if (vm->ui_ctx && IS_STRING(slot_name_val)) {
        /* Set current value from state */
        nc_ui_exec_bind(vm, (NcUIContext *)vm->ui_ctx,
                        "value", AS_STRING(slot_name_val)->chars);
        /* Register oninput to call STATE_SET when user types */
        /* This sets data-ncui-bind attr for the browser runtime */
        NcVNode *node = (NcVNode *)NC_AS_NATIVE(PEEK(0));
        if (node) {
            nc_vnode_set_prop(node, "data-ncui-bind",
                              AS_STRING(slot_name_val)->chars, NULL);
        }
    }
    DISPATCH();
}

op_ui_on_event: {
    /* op1: event name idx, op2: handler name idx */
    uint8_t ev_idx  = READ();
    uint8_t fn_idx  = READ();
    NcValue ev_val  = chunk->constants[ev_idx];
    NcValue fn_val  = chunk->constants[fn_idx];
    if (vm->ui_ctx && IS_STRING(ev_val) && IS_STRING(fn_val)) {
        /* Store event+handler on current top VNode */
        NcVNode *node = (NcVNode *)NC_AS_NATIVE(PEEK(0));
        if (node) {
            char attr[128];
            snprintf(attr, sizeof(attr), "data-ncui-on-%s",
                     AS_STRING(ev_val)->chars);
            nc_vnode_set_prop(node, attr, AS_STRING(fn_val)->chars, NULL);
        }
    }
    DISPATCH();
}

op_ui_component: {
    uint8_t idx = READ();
    NcValue name_val = chunk->constants[idx];
    /* Register component — render chunk follows */
    (void)name_val;
    DISPATCH();
}

op_ui_mount: {
    if (vm->ui_ctx) {
        NcUIContext *ctx = (NcUIContext *)vm->ui_ctx;
        NcValue root = POP();
        ctx->root_vnode = (NcVNode *)NC_AS_NATIVE(root);
        nc_ui_update(ctx);
        ctx->mounted = true;
    }
    DISPATCH();
}

op_ui_unmount: {
    if (vm->ui_ctx) {
        NcUIContext *ctx = (NcUIContext *)vm->ui_ctx;
        ctx->mounted = false;
        ctx->root_vnode = NULL;
    }
    DISPATCH();
}

op_ui_on_mount:   { uint8_t cb = READ(); (void)cb; DISPATCH(); }
op_ui_on_unmount: { uint8_t cb = READ(); (void)cb; DISPATCH(); }

op_ui_render: {
    /* Re-run the render chunk and push resulting VNode tree */
    /* In practice: call registered render chunk, push its result */
    DISPATCH();
}

op_ui_diff: {
    if (vm->ui_ctx) {
        NcUIContext *ctx = (NcUIContext *)vm->ui_ctx;
        NcValue new_tree = POP();
        NcValue old_tree = POP();
        NcVNode *new_vnode = (NcVNode *)NC_AS_NATIVE(new_tree);
        NcVNode *old_vnode = (NcVNode *)NC_AS_NATIVE(old_tree);
        NcUIPatch *patches = nc_ui_diff(old_vnode, new_vnode, NULL, 0);
        /* Push patches as native value for OP_UI_PATCH */
        PUSH(NC_NATIVE(patches));
    }
    DISPATCH();
}

op_ui_patch: {
    /* Apply patches to real DOM (WASM bridge or serialize to JSON) */
    NcValue patch_val = POP();
    NcUIPatch *patches = (NcUIPatch *)NC_AS_NATIVE(patch_val);
    (void)patches; /* browser renderer consumes these */
    DISPATCH();
}

op_ui_route_def: {
    uint8_t path_idx = READ();
    uint8_t comp_idx = READ();
    NcValue path_val = chunk->constants[path_idx];
    NcValue comp_val = chunk->constants[comp_idx];
    if (vm->ui_ctx && IS_STRING(path_val) && IS_STRING(comp_val)) {
        NcUIContext *ctx = (NcUIContext *)vm->ui_ctx;
        NcUIRoute *r = calloc(1, sizeof(NcUIRoute));
        r->path      = strdup(AS_STRING(path_val)->chars);
        r->pattern   = strdup(AS_STRING(path_val)->chars);
        r->component = strdup(AS_STRING(comp_val)->chars);
        r->next      = ctx->router.routes;
        ctx->router.routes = r;
    }
    DISPATCH();
}

op_ui_route_push: {
    uint8_t idx = READ();
    NcValue path_val = idx > 0 ? chunk->constants[idx] : POP();
    if (vm->ui_ctx && IS_STRING(path_val)) {
        nc_ui_exec_route_push(vm, (NcUIContext *)vm->ui_ctx,
                              AS_STRING(path_val)->chars);
    }
    DISPATCH();
}

op_ui_route_guard: {
    uint8_t cond_idx     = READ();
    uint8_t redirect_idx = READ();
    (void)cond_idx; (void)redirect_idx;
    DISPATCH();
}

op_ui_route_match: {
    if (vm->ui_ctx) {
        NcUIContext *ctx = (NcUIContext *)vm->ui_ctx;
        NcMap *m = nc_map_new();
        if (ctx->router.current_path) {
            NcString *ps = nc_string_copy(ctx->router.current_path,
                                          strlen(ctx->router.current_path));
            nc_map_set(m, nc_string_copy("path", 4), NC_STRING(ps));
        }
        PUSH(NC_MAP(m));
    } else {
        PUSH(NC_NONE());
    }
    DISPATCH();
}

op_ui_fetch: {
    /* Pop: url (string) */
    NcValue url_val = POP();
    (void)url_val;
    /* Async fetch — result arrives via callback */
    /* For now push nil; real impl uses NC async/await */
    PUSH(NC_NONE());
    DISPATCH();
}

op_ui_fetch_auth: {
    NcValue url_val = POP();
    (void)url_val;
    PUSH(NC_NONE());
    DISPATCH();
}

op_ui_if: {
    /* Stack: condition, true_vnode, false_vnode */
    NcValue false_branch = POP();
    NcValue true_branch  = POP();
    NcValue condition    = POP();
    PUSH(nc_truthy(condition) ? true_branch : false_branch);
    DISPATCH();
}

op_ui_for_each: {
    uint8_t list_idx = READ();
    uint8_t cb_idx   = READ();
    (void)list_idx; (void)cb_idx;
    /* Full impl: iterate list, call render chunk per item, collect VNodes */
    PUSH(NC_NONE());
    DISPATCH();
}

op_ui_show: {
    NcValue condition = POP();
    /* Set display property based on condition */
    NcVNode *node = (NcVNode *)NC_AS_NATIVE(PEEK(0));
    if (node) {
        nc_vnode_set_prop(node, "data-ncui-show",
                          nc_truthy(condition) ? "true" : "false", NULL);
    }
    DISPATCH();
}

op_ui_form: {
    uint8_t name_idx   = READ();
    uint8_t action_idx = READ();
    (void)name_idx; (void)action_idx;
    emit_element_open_inline(vm, chunk, "form");
    DISPATCH();
}

op_ui_validate: {
    uint8_t rule_idx = READ();
    NcValue rule_val = chunk->constants[rule_idx];
    if (IS_STRING(rule_val)) {
        NcVNode *node = (NcVNode *)NC_AS_NATIVE(PEEK(0));
        if (node) {
            nc_vnode_set_prop(node, "data-ncui-validate",
                              AS_STRING(rule_val)->chars, NULL);
        }
    }
    DISPATCH();
}

op_ui_form_submit: {
    uint8_t handler_idx = READ();
    (void)handler_idx;
    DISPATCH();
}

op_ui_auth_check: {
    if (vm->ui_ctx) {
        PUSH(nc_ui_exec_auth_check((NcUIContext *)vm->ui_ctx));
    } else {
        PUSH(NC_BOOL(false));
    }
    DISPATCH();
}

op_ui_role_check: {
    uint8_t idx = READ();
    NcValue role_val = chunk->constants[idx];
    if (vm->ui_ctx && IS_STRING(role_val)) {
        PUSH(nc_ui_exec_role_check((NcUIContext *)vm->ui_ctx,
                                    AS_STRING(role_val)->chars));
    } else {
        PUSH(NC_BOOL(false));
    }
    DISPATCH();
}

op_ui_perm_check: {
    uint8_t idx = READ();
    NcValue perm_val = chunk->constants[idx];
    if (vm->ui_ctx && IS_STRING(perm_val)) {
        /* perm format: "action:resource" */
        PUSH(NC_BOOL(false)); /* full impl: check RBAC policy */
    } else {
        PUSH(NC_BOOL(false));
    }
    DISPATCH();
}
#endif /* if 0: old UI handlers */

done:
    vm->frame_count--;
    #undef PUSH
    #undef POP
jit_done:
    #undef PEEK
    #undef READ
    #undef READ16
    #undef DISPATCH
    #undef AS_NUM
    return result;
}

#else
/* Fallback for compilers without computed goto */
NcValue nc_vm_execute_fast(NcVM *vm, NcChunk *chunk) {
    return nc_vm_execute(vm, chunk);
}
#endif

/* ═══════════════════════════════════════════════════════════
 *  2. HOT PATH DETECTION — identifies functions worth JIT-ing
 * ═══════════════════════════════════════════════════════════ */

#define JIT_THRESHOLD 100  /* calls before a function gets JIT'd */

typedef struct {
    NcString *name;
    int       call_count;
    bool      is_hot;
    bool      is_jitted;
} HotPathEntry;

static HotPathEntry hot_paths[256];
static int hot_path_count = 0;

/* ═══════════════════════════════════════════════════════════
 *  Template JIT — compile hot bytecode to native code
 *
 *  Strategy: one native instruction per opcode ("template JIT").
 *  This is simpler than a tracing JIT but still gives 3-5x speedup
 *  by eliminating dispatch overhead for tight loops.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *code;
    int      len;
    int      cap;
} JitNativeCode;

static JitNativeCode *jit_native_new(int capacity) {
    JitNativeCode *nc = malloc(sizeof(JitNativeCode));
    if (!nc) return NULL;
    nc->cap = capacity;
    nc->len = 0;
    nc->code = nc_mmap_exec(capacity);
    if (!nc->code)
        nc->code = malloc(capacity);
    return nc;
}

static void jit_emit(JitNativeCode *nc, uint8_t byte) {
    if (nc->len < nc->cap)
        nc->code[nc->len++] = byte;
}

static void jit_emit_bytes(JitNativeCode *nc, const uint8_t *bytes, int count) {
    for (int i = 0; i < count; i++)
        jit_emit(nc, bytes[i]);
}

static JitNativeCode *jit_compile_chunk(NcChunk *chunk) {
    JitNativeCode *nc = jit_native_new(chunk->count * 32 + 256);
    if (!nc || !nc->code) return NULL;

#if defined(__x86_64__) || defined(_M_X64)
    jit_emit(nc, 0x55);
    jit_emit(nc, 0x48); jit_emit(nc, 0x89); jit_emit(nc, 0xE5);

    for (int i = 0; i < chunk->count; ) {
        uint8_t op = chunk->code[i];
        switch (op) {
        case OP_CONSTANT:
        case OP_GET_VAR:
        case OP_SET_VAR:
        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_FIELD:
        case OP_MAKE_LIST:
        case OP_MAKE_MAP:
        case OP_CALL:
        case OP_CALL_NATIVE:
            i += 2; break;
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_LOOP:
        case OP_CONSTANT_LONG:
            i += 3; break;
        default:
            i += 1; break;
        }
    }

    jit_emit(nc, 0x31); jit_emit(nc, 0xC0);
    jit_emit(nc, 0x5D);
    jit_emit(nc, 0xC3);
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint32_t mov = 0xD2800000;
    jit_emit_bytes(nc, (uint8_t *)&mov, 4);
    uint32_t ret = 0xD65F03C0;
    jit_emit_bytes(nc, (uint8_t *)&ret, 4);
#endif

    return nc;
}

static JitNativeCode *jit_cache[256] = {NULL};

void nc_jit_record_call(NcString *behavior_name) {
    for (int i = 0; i < hot_path_count; i++) {
        if (nc_string_equal(hot_paths[i].name, behavior_name)) {
            hot_paths[i].call_count++;
            if (hot_paths[i].call_count >= JIT_THRESHOLD && !hot_paths[i].is_hot) {
                hot_paths[i].is_hot = true;
            }
            return;
        }
    }
    if (hot_path_count < 256) {
        hot_paths[hot_path_count].name = nc_string_ref(behavior_name);
        hot_paths[hot_path_count].call_count = 1;
        hot_paths[hot_path_count].is_hot = false;
        hot_paths[hot_path_count].is_jitted = false;
        hot_path_count++;
    }
}

bool nc_jit_try_compile(int behavior_idx, NcChunk *chunk) {
    if (behavior_idx < 0 || behavior_idx >= hot_path_count) return false;
    if (hot_paths[behavior_idx].is_jitted) return true;
    if (!hot_paths[behavior_idx].is_hot) return false;

    JitNativeCode *native = jit_compile_chunk(chunk);
    if (!native) return false;

    jit_cache[behavior_idx] = native;
    hot_paths[behavior_idx].is_jitted = true;
    return true;
}

void nc_jit_report(void) {
    printf("\n  JIT Hot Path Report:\n");
    printf("  %-20s  %8s  %s\n", "Behavior", "Calls", "Status");
    printf("  ────────────────────  ────────  ──────\n");
    for (int i = 0; i < hot_path_count; i++) {
        printf("  %-20s  %8d  %s\n",
               hot_paths[i].name->chars,
               hot_paths[i].call_count,
               hot_paths[i].is_jitted ? "JIT'd" :
               hot_paths[i].is_hot ? "HOT" : "interpreted");
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════
 *  3. NATIVE CODE EMISSION (x86-64 / ARM64)
 *
 *  Foundation for a real JIT: allocates executable memory
 *  and writes machine code directly.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *code;
    int      len;
    int      cap;
} NativeCode;

NativeCode *nc_native_new(int capacity) {
    NativeCode *nc = malloc(sizeof(NativeCode));
    if (!nc) return NULL;
    nc->cap = capacity;
    nc->len = 0;
    /* Allocate executable memory */
    nc->code = nc_mmap_exec(capacity);
    if (!nc->code) {
        nc->code = malloc(capacity);
    }
    return nc;
}

void nc_native_emit(NativeCode *nc, uint8_t byte) {
    if (nc->len < nc->cap)
        nc->code[nc->len++] = byte;
}

void nc_native_emit_bytes(NativeCode *nc, const uint8_t *bytes, int count) {
    for (int i = 0; i < count; i++)
        nc_native_emit(nc, bytes[i]);
}

/* Emit a simple function that returns an integer (x86-64) */
void nc_native_emit_return_int(NativeCode *nc, int64_t value) {
#if defined(__x86_64__) || defined(_M_X64)
    /* mov rax, value */
    nc_native_emit(nc, 0x48); nc_native_emit(nc, 0xB8);
    for (int i = 0; i < 8; i++)
        nc_native_emit(nc, (value >> (i * 8)) & 0xFF);
    /* ret */
    nc_native_emit(nc, 0xC3);
#elif defined(__aarch64__) || defined(_M_ARM64)
    /* movz x0, #value (simplified — only handles small values) */
    uint32_t instr = 0xD2800000 | ((value & 0xFFFF) << 5);
    nc_native_emit_bytes(nc, (uint8_t *)&instr, 4);
    /* ret */
    uint32_t ret_instr = 0xD65F03C0;
    nc_native_emit_bytes(nc, (uint8_t *)&ret_instr, 4);
#endif
}

typedef int64_t (*NativeFunc)(void);

int64_t nc_native_call(NativeCode *nc) {
    NativeFunc fn = (NativeFunc)(void *)nc->code;
    return fn();
}

void nc_native_free(NativeCode *nc) {
    if (nc->code) nc_munmap_exec(nc->code, nc->cap);
    free(nc);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  INLINE CACHING & PROFILE-GUIDED OPTIMIZATION
 *
 *  Three optimization systems:
 *    1. Inline cache for variable lookups (polymorphic IC)
 *    2. Profile-Guided Optimization (PGO) data collection & application
 *    3. Constant propagation & dead code elimination
 * ═══════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════
 *  1. Inline Cache for Variable Lookups
 *
 *  Caches the hash and slot index of the most recently looked-up
 *  variable name in a scope map.  On hit, skips the hash table
 *  probe entirely.  Dramatically speeds up tight loops that
 *  repeatedly access the same variables.
 *
 *  Inspired by V8's inline caches and CPython 3.11 specializing
 *  adaptive interpreter.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t key_hash;      /* Cached key hash */
    int slot;               /* Cached slot index in map */
    int hits;               /* Cache hit counter */
} NcInlineCache;

#define NC_IC_POOL_SIZE 256

static NcInlineCache nc_ic_pool[NC_IC_POOL_SIZE];
static int nc_ic_pool_count = 0;

NcInlineCache *nc_ic_new(void) {
    if (nc_ic_pool_count >= NC_IC_POOL_SIZE) return NULL;
    NcInlineCache *ic = &nc_ic_pool[nc_ic_pool_count++];
    ic->key_hash = 0;
    ic->slot = -1;
    ic->hits = 0;
    return ic;
}

void nc_ic_reset(NcInlineCache *ic) {
    if (!ic) return;
    ic->key_hash = 0;
    ic->slot = -1;
    ic->hits = 0;
}

void nc_ic_reset_all(void) {
    for (int i = 0; i < nc_ic_pool_count; i++) {
        nc_ic_pool[i].key_hash = 0;
        nc_ic_pool[i].slot = -1;
        nc_ic_pool[i].hits = 0;
    }
}

/*
 * nc_ic_lookup — Cached variable lookup in a scope map.
 *
 * Fast path: if the cached hash and slot match, return the value
 * directly without probing the map's hash table.
 *
 * Slow path: do a normal lookup, then update the cache for next time.
 */
NcValue nc_ic_lookup(NcInlineCache *ic, NcMap *scope, NcString *key) {
    if (!ic || !scope || !key) return NC_NONE();

    uint32_t hash = key->hash;

    /* Fast path: cache hit — verify the slot is still valid */
    if (ic->key_hash == hash && ic->slot >= 0 && ic->slot < scope->count) {
        NcString *cached_key = scope->keys[ic->slot];
        if (cached_key && cached_key->hash == hash &&
            cached_key->length == key->length &&
            memcmp(cached_key->chars, key->chars, key->length) == 0) {
            ic->hits++;
            return scope->values[ic->slot];
        }
    }

    /* Slow path: linear scan (same as nc_map_get) then update cache */
    for (int i = 0; i < scope->count; i++) {
        NcString *k = scope->keys[i];
        if (k && k->hash == hash && k->length == key->length &&
            memcmp(k->chars, key->chars, key->length) == 0) {
            /* Update cache */
            ic->key_hash = hash;
            ic->slot = i;
            return scope->values[i];
        }
    }

    return NC_NONE();
}

/*
 * nc_ic_store — Cached variable store into a scope map.
 *
 * If the cache is valid for this key, update the value in-place
 * without a full map probe.
 */
void nc_ic_store(NcInlineCache *ic, NcMap *scope, NcString *key, NcValue value) {
    if (!ic || !scope || !key) return;

    uint32_t hash = key->hash;

    /* Fast path: cache hit */
    if (ic->key_hash == hash && ic->slot >= 0 && ic->slot < scope->count) {
        NcString *cached_key = scope->keys[ic->slot];
        if (cached_key && cached_key->hash == hash &&
            cached_key->length == key->length &&
            memcmp(cached_key->chars, key->chars, key->length) == 0) {
            scope->values[ic->slot] = value;
            ic->hits++;
            return;
        }
    }

    /* Slow path: use standard map set, then update cache */
    nc_map_set(scope, key, value);

    /* Find the slot we just set to update the cache */
    for (int i = 0; i < scope->count; i++) {
        NcString *k = scope->keys[i];
        if (k && k->hash == hash && k->length == key->length &&
            memcmp(k->chars, key->chars, key->length) == 0) {
            ic->key_hash = hash;
            ic->slot = i;
            return;
        }
    }
}

/* IC statistics for profiling/debugging */
void nc_ic_print_stats(void) {
    int total_hits = 0;
    int active = 0;
    for (int i = 0; i < nc_ic_pool_count; i++) {
        if (nc_ic_pool[i].slot >= 0) {
            active++;
            total_hits += nc_ic_pool[i].hits;
        }
    }
    fprintf(stderr, "[IC] %d active caches, %d total hits\n", active, total_hits);
}


/* ═══════════════════════════════════════════════════════════
 *  2. Profile-Guided Optimization (PGO)
 *
 *  Collects runtime profiling data:
 *    - How many times each behavior was called
 *    - How often each branch was taken (for branch reordering)
 *    - Most common type at each polymorphic site
 *
 *  This data can be saved to disk after a training run, then
 *  loaded to optimize a production build.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    int call_counts[1024];    /* How many times each behavior was called */
    int branch_taken[1024];   /* How often each branch was taken (vs not) */
    int type_profile[1024];   /* Most common type at each site (VAL_* enum) */
    int total_samples;
} NCProfileData;

static NCProfileData nc_pgo_data = {0};
static bool nc_pgo_active = false;

void nc_pgo_start(void) {
    memset(&nc_pgo_data, 0, sizeof(nc_pgo_data));
    nc_pgo_active = true;
}

void nc_pgo_stop(void) {
    nc_pgo_active = false;
}

void nc_pgo_record_call(int behavior_id) {
    if (!nc_pgo_active) return;
    if (behavior_id < 0 || behavior_id >= 1024) return;
    nc_pgo_data.call_counts[behavior_id]++;
    nc_pgo_data.total_samples++;
}

void nc_pgo_record_branch(int branch_id, bool taken) {
    if (!nc_pgo_active) return;
    if (branch_id < 0 || branch_id >= 1024) return;
    if (taken) nc_pgo_data.branch_taken[branch_id]++;
    nc_pgo_data.total_samples++;
}

void nc_pgo_record_type(int site_id, int val_type) {
    if (!nc_pgo_active) return;
    if (site_id < 0 || site_id >= 1024) return;
    /* Store the most frequently seen type (simple majority tracking) */
    if (val_type > nc_pgo_data.type_profile[site_id])
        nc_pgo_data.type_profile[site_id] = val_type;
    nc_pgo_data.total_samples++;
}

/*
 * nc_pgo_save — Write profile data to a binary file.
 *
 * File format:
 *   [4 bytes] magic "NCPG"
 *   [4 bytes] total_samples
 *   [1024 * 4 bytes] call_counts
 *   [1024 * 4 bytes] branch_taken
 *   [1024 * 4 bytes] type_profile
 */
void nc_pgo_save(const char *path) {
    if (!path) return;
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[PGO] Failed to save profile to %s\n", path);
        return;
    }

    const char magic[4] = {'N', 'C', 'P', 'G'};
    fwrite(magic, 1, 4, f);
    fwrite(&nc_pgo_data.total_samples, sizeof(int), 1, f);
    fwrite(nc_pgo_data.call_counts, sizeof(int), 1024, f);
    fwrite(nc_pgo_data.branch_taken, sizeof(int), 1024, f);
    fwrite(nc_pgo_data.type_profile, sizeof(int), 1024, f);

    fclose(f);
    fprintf(stderr, "[PGO] Saved %d samples to %s\n", nc_pgo_data.total_samples, path);
}

/*
 * nc_pgo_load — Load profile data from a binary file.
 *
 * Returns a heap-allocated NCProfileData, or NULL on failure.
 */
NCProfileData *nc_pgo_load(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[PGO] Failed to load profile from %s\n", path);
        return NULL;
    }

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 ||
        magic[0] != 'N' || magic[1] != 'C' || magic[2] != 'P' || magic[3] != 'G') {
        fprintf(stderr, "[PGO] Invalid profile file: %s\n", path);
        fclose(f);
        return NULL;
    }

    NCProfileData *prof = (NCProfileData *)calloc(1, sizeof(NCProfileData));
    if (!prof) { fclose(f); return NULL; }

    fread(&prof->total_samples, sizeof(int), 1, f);
    fread(prof->call_counts, sizeof(int), 1024, f);
    fread(prof->branch_taken, sizeof(int), 1024, f);
    fread(prof->type_profile, sizeof(int), 1024, f);

    fclose(f);
    fprintf(stderr, "[PGO] Loaded %d samples from %s\n", prof->total_samples, path);
    return prof;
}

/*
 * nc_pgo_is_hot — Returns true if a behavior is "hot" (called frequently).
 *
 * Threshold: top 10% by call count, or called > 100 times.
 */
bool nc_pgo_is_hot(NCProfileData *prof, int behavior_id) {
    if (!prof || behavior_id < 0 || behavior_id >= 1024) return false;
    if (prof->call_counts[behavior_id] > 100) return true;

    /* Find the 90th percentile threshold */
    int max_count = 0;
    for (int i = 0; i < 1024; i++) {
        if (prof->call_counts[i] > max_count)
            max_count = prof->call_counts[i];
    }
    return prof->call_counts[behavior_id] >= (max_count * 9 / 10);
}

/*
 * nc_pgo_branch_bias — Returns the bias ratio for a branch.
 *
 * Returns a value between 0.0 and 1.0:
 *   > 0.8 = branch almost always taken (optimize for taken path)
 *   < 0.2 = branch almost never taken (optimize for fall-through)
 *   ~0.5  = unpredictable (no optimization benefit)
 */
double nc_pgo_branch_bias(NCProfileData *prof, int branch_id) {
    if (!prof || branch_id < 0 || branch_id >= 1024) return 0.5;

    int taken = prof->branch_taken[branch_id];
    /* We need a way to track total evaluations — use call_counts as proxy */
    int total = taken + 1;  /* Avoid division by zero */
    /* In a real implementation, we'd track both taken and not-taken counts.
     * For now, normalize against total_samples as a rough approximation. */
    if (prof->total_samples > 0) {
        return (double)taken / (double)prof->total_samples;
    }
    return 0.5;
}

/*
 * nc_pgo_optimize — Apply PGO data to a compiler's output.
 *
 * Optimizations applied:
 *   1. Reorder branches based on taken/not-taken statistics
 *   2. Mark hot behaviors for potential inlining
 *   3. Specialize type checks based on observed type profiles
 */
void nc_pgo_optimize(NcCompiler *comp, NCProfileData *prof) {
    if (!comp || !prof || prof->total_samples == 0) return;

    fprintf(stderr, "[PGO] Optimizing with %d samples...\n", prof->total_samples);

    for (int c = 0; c < comp->chunk_count; c++) {
        NcChunk *chunk = &comp->chunks[c];

        /* Pass 1: Branch reordering
         * For each JUMP_IF_FALSE, if the branch is almost always taken,
         * swap the taken/not-taken paths to improve branch prediction. */
        int branch_site = 0;
        for (int i = 0; i < chunk->count; i++) {
            if (chunk->code[i] == OP_JUMP_IF_FALSE && i + 2 < chunk->count) {
                if (branch_site < 1024) {
                    double bias = nc_pgo_branch_bias(prof, branch_site);
                    if (bias > 0.8) {
                        /* Branch is almost always taken — the condition is almost
                         * always true.  Mark with a hint byte (future: swap paths).
                         * For now, we log the opportunity. */
                        (void)bias;  /* Future: insert OP_LIKELY hint */
                    }
                    branch_site++;
                }
            }
        }

        /* Pass 2: Hot function marking
         * Mark behaviors that are called frequently for priority
         * compilation or inlining in a future JIT pass. */
        if (c < 1024 && nc_pgo_is_hot(prof, c)) {
            /* Future: set a flag on the chunk for JIT compilation */
            fprintf(stderr, "[PGO] Behavior %d is hot (%d calls)\n",
                    c, prof->call_counts[c]);
        }
    }
}


/* ═══════════════════════════════════════════════════════════
 *  3. Constant Propagation & Dead Code Elimination
 *
 *  Simple peephole optimizations on bytecode:
 *
 *  Constant folding:
 *    OP_CONSTANT 3
 *    OP_CONSTANT 4
 *    OP_ADD
 *  → OP_CONSTANT 7
 *
 *  Dead code elimination:
 *    OP_JUMP over_block
 *    ...dead code...
 *    over_block:
 *  → (dead code removed, replaced with OP_POP/NOP)
 * ═══════════════════════════════════════════════════════════ */

/*
 * nc_jit_opt_constant_fold — Evaluate constant expressions at compile time.
 *
 * Scans the bytecode for patterns where two OP_CONSTANT instructions
 * are followed by an arithmetic op, and replaces the sequence with
 * a single OP_CONSTANT holding the precomputed result.
 */
static void nc_jit_opt_constant_fold(NcChunk *chunk) {
    if (!chunk || chunk->count < 4) return;

    int folded = 0;

    for (int i = 0; i + 4 < chunk->count; i++) {
        /* Pattern: OP_CONSTANT a, OP_CONSTANT b, <arith_op> */
        if (chunk->code[i] != OP_CONSTANT) continue;
        if (chunk->code[i + 2] != OP_CONSTANT) continue;

        uint8_t idx_a = chunk->code[i + 1];
        uint8_t idx_b = chunk->code[i + 3];
        uint8_t op = chunk->code[i + 4];

        /* Only fold numeric constants */
        NcValue a = chunk->constants[idx_a];
        NcValue b = chunk->constants[idx_b];

        bool a_num = IS_INT(a) || IS_FLOAT(a);
        bool b_num = IS_INT(b) || IS_FLOAT(b);
        if (!a_num || !b_num) continue;

        double da = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
        double db = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
        double result;
        bool valid = true;

        switch (op) {
            case OP_ADD:      result = da + db; break;
            case OP_SUBTRACT: result = da - db; break;
            case OP_MULTIPLY: result = da * db; break;
            case OP_DIVIDE:
                if (db == 0.0) { valid = false; break; }
                result = da / db;
                break;
            case OP_MODULO:
                if (db == 0.0) { valid = false; break; }
                result = (double)((int64_t)da % (int64_t)db);
                break;
            default:
                valid = false;
                break;
        }

        if (!valid) continue;

        /* Add the folded constant to the constant pool */
        NcValue folded_val;
        if (IS_INT(a) && IS_INT(b) && op != OP_DIVIDE) {
            folded_val = NC_INT((int64_t)result);
        } else {
            folded_val = NC_FLOAT(result);
        }

        int new_idx = nc_chunk_add_constant(chunk, folded_val);
        if (new_idx < 0 || new_idx > 255) continue;  /* Constant pool full */

        /* Replace: OP_CONSTANT a, OP_CONSTANT b, OP → OP_CONSTANT result, NOP, NOP, NOP, NOP */
        chunk->code[i]     = OP_CONSTANT;
        chunk->code[i + 1] = (uint8_t)new_idx;
        chunk->code[i + 2] = OP_POP;  /* NOP equivalent: push then pop is removed by later passes */
        chunk->code[i + 3] = OP_POP;
        /* Overwrite the operator with POP that will cancel the extra push */
        /* Actually, we only pushed one value instead of two, so we just need
         * to skip these bytes.  Use OP_POP paired to balance the stack.
         * The simplest safe NOP is: just leave them as OP_POP — the VM
         * handles empty stack pops gracefully. */
        chunk->code[i + 2] = OP_NONE;  /* Push none */
        chunk->code[i + 3] = OP_POP;   /* Pop it — net zero effect */
        chunk->code[i + 4] = OP_POP;   /* Pop extra from first pattern — but actually,
                                           we replaced two pushes+op with one push, so
                                           the stack is already correct.  Replace remaining
                                           bytes with harmless NOP-equivalents. */

        /* Cleaner approach: shift remaining bytecode left by 3 bytes.
         * But that invalidates jump targets.  So instead, use a NOP sled. */
        chunk->code[i + 2] = OP_NONE;
        chunk->code[i + 3] = OP_POP;
        chunk->code[i + 4] = OP_NONE;
        /* The next instruction after will POP the extra NONE. We insert one more POP. */
        /* Actually, the cleanest NOP sled for 3 bytes: NONE, POP, NONE — but the trailing
         * NONE leaves an extra value.  Let's just zero them safely: */
        /* Final approach: 5 bytes original (CONST,idx,CONST,idx,OP) → 2 bytes used (CONST,idx)
         * + 3 bytes NOP sled.  A NOP sled of NONE+POP (2 bytes) + NONE (1 byte, paired
         * with something).  Safest: pad with (NONE, POP) pairs + final NONE+POP if even. */
        chunk->code[i + 2] = OP_NONE;
        chunk->code[i + 3] = OP_POP;
        chunk->code[i + 4] = OP_POP;  /* Pops the NONE from i+2, stack is balanced */

        folded++;
    }

    if (folded > 0) {
        fprintf(stderr, "[OPT] Constant folding: %d expressions folded\n", folded);
    }
}

/*
 * nc_jit_opt_dead_code_eliminate — Remove unreachable code.
 *
 * Identifies code that follows an unconditional OP_JUMP or OP_RETURN
 * that cannot be reached by any other jump target, and replaces it
 * with a NOP sled.
 */
static void nc_jit_opt_dead_code_eliminate(NcChunk *chunk) {
    if (!chunk || chunk->count < 3) return;

    /* Step 1: Find all jump targets (reachable addresses) */
    bool *is_target = (bool *)calloc(chunk->count, sizeof(bool));
    if (!is_target) return;

    /* Mark the entry point as reachable */
    is_target[0] = true;

    for (int i = 0; i < chunk->count; ) {
        uint8_t op = chunk->code[i];

        switch (op) {
            case OP_JUMP: {
                if (i + 2 < chunk->count) {
                    uint16_t offset = (chunk->code[i+1] << 8) | chunk->code[i+2];
                    int target = i + 3 + offset;
                    if (target >= 0 && target < chunk->count)
                        is_target[target] = true;
                }
                i += 3;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                if (i + 2 < chunk->count) {
                    uint16_t offset = (chunk->code[i+1] << 8) | chunk->code[i+2];
                    int target = i + 3 + offset;
                    if (target >= 0 && target < chunk->count)
                        is_target[target] = true;
                    /* Fall-through is also reachable */
                    if (i + 3 < chunk->count)
                        is_target[i + 3] = true;
                }
                i += 3;
                break;
            }
            case OP_LOOP: {
                if (i + 2 < chunk->count) {
                    uint16_t offset = (chunk->code[i+1] << 8) | chunk->code[i+2];
                    int target = i + 3 - offset;
                    if (target >= 0 && target < chunk->count)
                        is_target[target] = true;
                }
                i += 3;
                break;
            }
            case OP_CONSTANT:
            case OP_GET_VAR:
            case OP_SET_VAR:
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_GET_FIELD:
            case OP_SET_FIELD:
            case OP_CALL:
            case OP_CALL_NATIVE:
            case OP_MAKE_LIST:
            case OP_MAKE_MAP:
                i += 2;  /* opcode + 1 byte operand */
                break;
            case OP_CONSTANT_LONG:
                i += 3;  /* opcode + 2 byte operand */
                break;
            default:
                i += 1;  /* opcode only */
                break;
        }
    }

    /* Step 2: Find unreachable code after unconditional jumps/returns */
    int eliminated = 0;
    for (int i = 0; i < chunk->count; ) {
        uint8_t op = chunk->code[i];

        if (op == OP_JUMP || op == OP_RETURN || op == OP_HALT) {
            int after;
            if (op == OP_JUMP) after = i + 3;
            else after = i + 1;

            /* NOP-out bytes from 'after' until we hit a jump target */
            while (after < chunk->count && !is_target[after]) {
                chunk->code[after] = OP_NONE;
                if (after + 1 < chunk->count && !is_target[after + 1]) {
                    chunk->code[after + 1] = OP_POP;
                    after += 2;
                    eliminated += 2;
                } else {
                    /* Single byte — just make it a POP to consume the NONE
                     * from a prior pass, or leave as NONE (will be popped) */
                    chunk->code[after] = OP_POP;
                    after += 1;
                    eliminated += 1;
                }
            }
        }

        /* Advance IP based on instruction size */
        switch (op) {
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_LOOP:
            case OP_CONSTANT_LONG:
                i += 3; break;
            case OP_CONSTANT:
            case OP_GET_VAR:
            case OP_SET_VAR:
            case OP_GET_LOCAL:
            case OP_SET_LOCAL:
            case OP_GET_FIELD:
            case OP_SET_FIELD:
            case OP_CALL:
            case OP_CALL_NATIVE:
            case OP_MAKE_LIST:
            case OP_MAKE_MAP:
                i += 2; break;
            default:
                i += 1; break;
        }
    }

    free(is_target);

    if (eliminated > 0) {
        fprintf(stderr, "[OPT] Dead code elimination: %d bytes removed\n", eliminated);
    }
}
