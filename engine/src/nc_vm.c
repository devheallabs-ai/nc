/*
 * nc_vm.c — Stack-based Virtual Machine for NC bytecode.
 *
 * This is the execution engine — like CPython's ceval.c.
 * It reads bytecodes from a chunk and executes them on a stack.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#ifndef NC_WINDOWS
#include <regex.h>
#endif

/* ── VM lifecycle ──────────────────────────────────────────── */

NcVM *nc_vm_new(void) {
    static bool gc_initialized = false;
    if (!gc_initialized) {
        nc_gc_init();
        gc_initialized = true;
    }

    NcVM *vm = calloc(1, sizeof(NcVM));
    if (!vm) return NULL;
    vm->stack_top = 0;
    vm->frame_count = 0;
    vm->globals = nc_map_new();
    vm->behaviors = nc_map_new();
    vm->behavior_chunks = NULL;
    vm->behavior_chunk_count = 0;
    vm->output_capacity = 64;
    vm->output = calloc(vm->output_capacity, sizeof(char *));
    if (!vm->output) { free(vm); return NULL; }
    vm->output_count = 0;
    vm->had_error = false;
    nc_gc_register_vm(vm);
    return vm;
}

static void vm_push(NcVM *vm, NcValue v) {
    if (vm->stack_top >= NC_STACK_MAX) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Too many nested operations. Check for unintended recursion or deeply nested calls.");
        vm->had_error = true;
        return;
    }
    vm->stack[vm->stack_top++] = v;
}

static NcValue vm_pop(NcVM *vm) {
    if (vm->stack_top <= 0) return NC_NONE();
    return vm->stack[--vm->stack_top];
}

static NcValue vm_peek(NcVM *vm, int distance) {
    if (vm->stack_top - 1 - distance < 0) return NC_NONE();
    return vm->stack[vm->stack_top - 1 - distance];
}

/* Public wrappers for external callers (nc_ui_vm.c, plugins) */
void    nc_vm_push(NcVM *vm, NcValue v)       { vm_push(vm, v); }
NcValue nc_vm_pop(NcVM *vm)                   { return vm_pop(vm); }
NcValue nc_vm_peek(NcVM *vm, int distance)    { return vm_peek(vm, distance); }

extern char *nc_redact_for_display(const char *text);

static void vm_output(NcVM *vm, const char *msg) {
    if (vm->output_count >= vm->output_capacity) {
        int new_cap = vm->output_capacity * 2;
        char **new_output = realloc(vm->output, sizeof(char *) * new_cap);
        if (!new_output) return;
        vm->output = new_output;
        vm->output_capacity = new_cap;
    }
    char *redacted = nc_redact_for_display(msg);
    vm->output[vm->output_count++] = redacted ? redacted : strdup(msg);
}

static void vm_error(NcVM *vm, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(vm->error_msg, sizeof(vm->error_msg), fmt, args);
    va_end(args);
    vm->had_error = true;
}

/* ── Read bytecode operands ────────────────────────────────── */

static uint8_t read_byte(NcCallFrame *frame) {
    return *frame->ip++;
}

static uint16_t read_short(NcCallFrame *frame) {
    uint16_t val = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
    frame->ip += 2;
    return val;
}

/* ── Number coercion ───────────────────────────────────────── */

static double as_number(NcValue v) {
    if (IS_INT(v)) return (double)AS_INT(v);
    if (IS_FLOAT(v)) return AS_FLOAT(v);
    if (IS_BOOL(v)) return AS_BOOL(v) ? 1.0 : 0.0;
    if (IS_STRING(v) && AS_STRING(v) && AS_STRING(v)->length > 0) {
        char *end = NULL;
        double d = strtod(AS_STRING(v)->chars, &end);
        if (end && end != AS_STRING(v)->chars) return d;
    }
    return 0.0;
}

/* ── Execution loop (the core — like CPython's eval_frame) ── */

NcValue nc_vm_execute(NcVM *vm, NcChunk *chunk) {
    if (vm->frame_count >= NC_FRAMES_MAX) {
        vm_error(vm, "Behaviors are calling each other without stopping (exceeded %d levels). Check for recursive calls that never end.", NC_FRAMES_MAX);
        return NC_NONE();
    }
    NcCallFrame *frame = &vm->frames[vm->frame_count++];
    frame->chunk = chunk;
    frame->ip = chunk->code;
    frame->slots = vm->stack;

    NcValue result = NC_NONE();

    int gc_counter = 0;
    int loop_guard = 0;
    while (!vm->had_error) {
        uint8_t instruction = read_byte(frame);
        nc_profiler_record_op(instruction);

        /* Run GC at safe points (every 1000 instructions) */
        if (++gc_counter >= 1000) {
            gc_counter = 0;
            nc_gc_maybe_collect();
        }

        switch (instruction) {

        case OP_CONSTANT: {
            uint8_t idx = read_byte(frame);
            if (idx >= chunk->const_count) {
                vm_error(vm, "Internal error: constant index %d out of range (max %d).", idx, chunk->const_count);
                return result;
            }
            vm_push(vm, chunk->constants[idx]);
            break;
        }

        case OP_NONE:  vm_push(vm, NC_NONE()); break;
        case OP_TRUE:  vm_push(vm, NC_BOOL(true)); break;
        case OP_FALSE: vm_push(vm, NC_BOOL(false)); break;
        case OP_POP: { NcValue popped = vm_pop(vm); nc_value_release(popped); break; }

        case OP_GET_VAR: {
            uint8_t idx = read_byte(frame);
            if (idx >= chunk->var_count) {
                vm_error(vm, "Internal error: variable index %d out of range (max %d).", idx, chunk->var_count);
                return result;
            }
            NcString *name = chunk->var_names[idx];
            NcValue val = nc_map_get(vm->globals, name);
            vm_push(vm, val);
            break;
        }

        case OP_SET_VAR: {
            uint8_t idx = read_byte(frame);
            if (idx >= chunk->var_count) {
                vm_error(vm, "Internal error: variable index %d out of range (max %d).", idx, chunk->var_count);
                return result;
            }
            NcString *name = chunk->var_names[idx];
            NcValue val = vm_peek(vm, 0);
            nc_map_set(vm->globals, name, val);
            break;
        }

        case OP_GET_LOCAL: {
            uint8_t slot = read_byte(frame);
            vm_push(vm, frame->locals[slot]);
            break;
        }

        case OP_SET_LOCAL: {
            uint8_t slot = read_byte(frame);
            frame->locals[slot] = vm_peek(vm, 0);
            if (slot >= frame->local_count)
                frame->local_count = slot + 1;
            break;
        }

        case OP_CONSTANT_LONG: {
            uint16_t idx = read_short(frame);
            if (idx >= chunk->const_count) {
                vm_error(vm, "Internal error: long constant index %d out of range (max %d).", idx, chunk->const_count);
                return result;
            }
            vm_push(vm, chunk->constants[idx]);
            break;
        }

        case OP_GET_FIELD: {
            uint8_t idx = read_byte(frame);
            if (idx >= chunk->var_count) {
                vm_error(vm, "Internal error: field index %d out of range (max %d).", idx, chunk->var_count);
                return result;
            }
            NcValue obj = vm_pop(vm);
            NcString *field = chunk->var_names[idx];
            if (!field) { vm_push(vm, NC_NONE()); break; }
            if (IS_MAP(obj)) {
                vm_push(vm, nc_map_get(AS_MAP(obj), field));
            } else if (IS_STRING(obj) && AS_STRING(obj)->length > 1) {
                const char *s = AS_STRING(obj)->chars;
                if (s[0] == '{' || s[0] == '[') {
                    NcValue parsed = nc_json_parse(s);
                    if (IS_MAP(parsed))
                        vm_push(vm, nc_map_get(AS_MAP(parsed), field));
                    else if (IS_LIST(parsed))
                        vm_push(vm, parsed);
                    else
                        vm_push(vm, NC_NONE());
                } else {
                    vm_push(vm, NC_NONE());
                }
            } else {
                vm_push(vm, NC_NONE());
            }
            break;
        }

        case OP_SET_FIELD: {
            uint8_t idx = read_byte(frame);
            if (idx >= chunk->var_count) {
                vm_error(vm, "Internal error: field index %d out of range (max %d).", idx, chunk->var_count);
                return result;
            }
            NcValue val = vm_pop(vm);
            NcValue obj = vm_pop(vm);
            NcString *field = chunk->var_names[idx];
            if (!field) { vm_push(vm, val); break; }
            if (IS_MAP(obj)) {
                nc_map_set(AS_MAP(obj), field, val);
            }
            vm_push(vm, val);
            break;
        }

        case OP_GET_INDEX: {
            NcValue idx = vm_pop(vm);
            NcValue obj = vm_pop(vm);
            if (IS_LIST(obj) && IS_INT(idx)) {
                vm_push(vm, nc_list_get(AS_LIST(obj), (int)AS_INT(idx)));
            } else if (IS_MAP(obj) && IS_STRING(idx)) {
                vm_push(vm, nc_map_get(AS_MAP(obj), AS_STRING(idx)));
            } else if (IS_MAP(obj) && IS_INT(idx)) {
                /* Map iteration: integer index returns the key at that position */
                int i = (int)AS_INT(idx);
                NcMap *m = AS_MAP(obj);
                if (i >= 0 && i < m->count)
                    vm_push(vm, NC_STRING(nc_string_ref(m->keys[i])));
                else
                    vm_push(vm, NC_NONE());
            } else if (IS_STRING(obj) && IS_INT(idx)) {
                int i = (int)AS_INT(idx);
                if (i >= 0 && i < AS_STRING(obj)->length) {
                    char ch[2] = {AS_STRING(obj)->chars[i], '\0'};
                    vm_push(vm, NC_STRING(nc_string_from_cstr(ch)));
                } else {
                    vm_push(vm, NC_NONE());
                }
            } else {
                vm_push(vm, NC_NONE());
            }
            break;
        }

        case OP_ADD: {
            NcValue b = vm_pop(vm);
            NcValue a = vm_pop(vm);
            /* List concatenation: [1,2] + [3] → [1,2,3]
             * Also: [1,2] + 3 → [1,2,3] (append) */
            if (IS_LIST(a) && IS_LIST(b)) {
                NcList *la = AS_LIST(a), *lb = AS_LIST(b);
                NcList *result = nc_list_new();
                for (int li = 0; li < la->count; li++) nc_list_push(result, la->items[li]);
                for (int li = 0; li < lb->count; li++) nc_list_push(result, lb->items[li]);
                vm_push(vm, NC_LIST(result));
                break;
            }
            if (IS_LIST(a) && !IS_LIST(b)) {
                NcList *la = AS_LIST(a);
                NcList *result = nc_list_new();
                for (int li = 0; li < la->count; li++) nc_list_push(result, la->items[li]);
                nc_list_push(result, b);
                vm_push(vm, NC_LIST(result));
                break;
            }
            if (IS_STRING(a) || IS_STRING(b)) {
                NcString *sa = nc_value_to_string(a);
                NcString *sb = nc_value_to_string(b);
                /* Resolve templates in strings */
                char *ra = sa->chars, *rb = sb->chars;
                bool free_ra = false, free_rb = false;
                if (strstr(ra, "{{")) {
                    /* Resolve {{var}} by looking up in globals — use dynamic buffer */
                    NcDynBuf resolved;
                    nc_dbuf_init(&resolved, sa->length * 2 + 256);
                    const char *p = ra;
                    while (*p) {
                        if (*p == '{' && *(p+1) == '{') {
                            p += 2;
                            char vname[128] = {0};
                            int vi = 0;
                            while (*p && !(*p == '}' && *(p+1) == '}') && vi < 126) vname[vi++] = *p++;
                            if (*p == '}') p += 2;
                            /* Look up variable */
                            char *dot = strchr(vname, '.');
                            NcString *key;
                            if (dot) {
                                char base[64] = {0};
                                { int blen = (int)(dot - vname); if (blen > 63) blen = 63; memcpy(base, vname, blen); base[blen] = '\0'; }
                                key = nc_string_from_cstr(base);
                                NcValue obj = nc_map_get(vm->globals, key);
                                nc_string_free(key);
                                if (IS_MAP(obj)) {
                                    NcString *fk = nc_string_from_cstr(dot + 1);
                                    NcValue fv = nc_map_get(AS_MAP(obj), fk);
                                    nc_string_free(fk);
                                    NcString *fs = nc_value_to_string(fv);
                                    nc_dbuf_append_len(&resolved, fs->chars, fs->length);
                                    nc_string_free(fs);
                                } else {
                                    NcString *vs = nc_value_to_string(obj);
                                    nc_dbuf_append_len(&resolved, vs->chars, vs->length);
                                    nc_string_free(vs);
                                }
                            } else {
                                key = nc_string_from_cstr(vname);
                                NcValue val = nc_map_get(vm->globals, key);
                                nc_string_free(key);
                                NcString *vs = nc_value_to_string(val);
                                nc_dbuf_append_len(&resolved, vs->chars, vs->length);
                                nc_string_free(vs);
                            }
                        } else {
                            nc_dbuf_append_len(&resolved, p, 1);
                            p++;
                        }
                    }
                    nc_string_free(sa);
                    sa = nc_string_from_cstr(resolved.data);
                    nc_dbuf_free(&resolved);
                }
                NcString *res = nc_string_concat(sa, sb);
                vm_push(vm, NC_STRING(res));
                nc_string_free(sa);
                nc_string_free(sb);
                (void)free_ra; (void)free_rb;
            } else if (IS_INT(a) && IS_INT(b)) {
                int64_t r;
                if (__builtin_add_overflow(AS_INT(a), AS_INT(b), &r))
                    vm_push(vm, NC_FLOAT((double)AS_INT(a) + (double)AS_INT(b)));
                else
                    vm_push(vm, NC_INT(r));
            } else {
                vm_push(vm, NC_FLOAT(as_number(a) + as_number(b)));
            }
            break;
        }

        case OP_SUBTRACT: {
            NcValue b = vm_pop(vm);
            NcValue a = vm_pop(vm);
            if (IS_INT(a) && IS_INT(b)) {
                int64_t r;
                if (__builtin_sub_overflow(AS_INT(a), AS_INT(b), &r))
                    vm_push(vm, NC_FLOAT((double)AS_INT(a) - (double)AS_INT(b)));
                else
                    vm_push(vm, NC_INT(r));
            } else
                vm_push(vm, NC_FLOAT(as_number(a) - as_number(b)));
            break;
        }

        case OP_MULTIPLY: {
            NcValue b = vm_pop(vm);
            NcValue a = vm_pop(vm);
            if (IS_INT(a) && IS_INT(b)) {
                int64_t r;
                if (__builtin_mul_overflow(AS_INT(a), AS_INT(b), &r))
                    vm_push(vm, NC_FLOAT((double)AS_INT(a) * (double)AS_INT(b)));
                else
                    vm_push(vm, NC_INT(r));
            } else
                vm_push(vm, NC_FLOAT(as_number(a) * as_number(b)));
            break;
        }

        case OP_DIVIDE: {
            NcValue b = vm_pop(vm);
            NcValue a = vm_pop(vm);
            double denom = as_number(b);
            if (denom == 0) { vm_push(vm, NC_INT(0)); }
            else vm_push(vm, NC_FLOAT(as_number(a) / denom));
            break;
        }

        case OP_MODULO: {
            NcValue b = vm_pop(vm);
            NcValue a = vm_pop(vm);
            if (IS_INT(a) && IS_INT(b) && AS_INT(b) != 0)
                vm_push(vm, NC_INT(AS_INT(a) % AS_INT(b)));
            else {
                double denom = as_number(b);
                vm_push(vm, denom != 0 ? NC_FLOAT(fmod(as_number(a), denom)) : NC_INT(0));
            }
            break;
        }

        case OP_NEGATE: {
            NcValue v = vm_pop(vm);
            if (IS_INT(v)) vm_push(vm, NC_INT(-AS_INT(v)));
            else vm_push(vm, NC_FLOAT(-as_number(v)));
            break;
        }

        case OP_NOT:
            vm_push(vm, NC_BOOL(!nc_truthy(vm_pop(vm))));
            break;

        case OP_EQUAL: {
            NcValue b = vm_pop(vm), a = vm_pop(vm);
            if (IS_STRING(a) && IS_STRING(b))
                vm_push(vm, NC_BOOL(nc_string_equal(AS_STRING(a), AS_STRING(b))));
            else if (IS_NONE(a) || IS_NONE(b))
                vm_push(vm, NC_BOOL(IS_NONE(a) && IS_NONE(b)));
            else if (IS_BOOL(a) && IS_BOOL(b))
                vm_push(vm, NC_BOOL(AS_BOOL(a) == AS_BOOL(b)));
            else if (IS_NUMBER(a) && IS_NUMBER(b))
                vm_push(vm, NC_BOOL(as_number(a) == as_number(b)));
            else if ((IS_STRING(a) && !IS_NUMBER(b)) || (IS_STRING(b) && !IS_NUMBER(a))) {
                NcString *sa = nc_value_to_string(a);
                NcString *sb = nc_value_to_string(b);
                vm_push(vm, NC_BOOL(nc_string_equal(sa, sb)));
                nc_string_free(sa); nc_string_free(sb);
            } else
                vm_push(vm, NC_BOOL(a.type == b.type && as_number(a) == as_number(b)));
            break;
        }

        case OP_NOT_EQUAL: {
            NcValue b = vm_pop(vm), a = vm_pop(vm);
            if (IS_STRING(a) && IS_STRING(b))
                vm_push(vm, NC_BOOL(!nc_string_equal(AS_STRING(a), AS_STRING(b))));
            else if (IS_NONE(a) || IS_NONE(b))
                vm_push(vm, NC_BOOL(!(IS_NONE(a) && IS_NONE(b))));
            else if (IS_BOOL(a) && IS_BOOL(b))
                vm_push(vm, NC_BOOL(AS_BOOL(a) != AS_BOOL(b)));
            else if ((IS_STRING(a) && !IS_NUMBER(b)) || (IS_STRING(b) && !IS_NUMBER(a))) {
                NcString *sa = nc_value_to_string(a);
                NcString *sb = nc_value_to_string(b);
                vm_push(vm, NC_BOOL(!nc_string_equal(sa, sb)));
                nc_string_free(sa); nc_string_free(sb);
            } else
                vm_push(vm, NC_BOOL(as_number(a) != as_number(b)));
            break;
        }

        case OP_ABOVE:
            { NcValue b = vm_pop(vm), a = vm_pop(vm);
              vm_push(vm, NC_BOOL(as_number(a) > as_number(b))); break; }

        case OP_BELOW: {
            NcValue b = vm_pop(vm), a = vm_pop(vm);
            double bv = IS_LIST(b) ? (double)AS_LIST(b)->count :
                        IS_MAP(b) ? (double)AS_MAP(b)->count : as_number(b);
            vm_push(vm, NC_BOOL(as_number(a) < bv));
            break;
        }

        case OP_AT_LEAST:
            { NcValue b = vm_pop(vm), a = vm_pop(vm);
              vm_push(vm, NC_BOOL(as_number(a) >= as_number(b))); break; }

        case OP_AT_MOST:
            { NcValue b = vm_pop(vm), a = vm_pop(vm);
              vm_push(vm, NC_BOOL(as_number(a) <= as_number(b))); break; }

        case OP_IN: {
            NcValue container = vm_pop(vm), needle = vm_pop(vm);
            bool found = false;
            if (IS_LIST(container) && IS_STRING(needle)) {
                NcList *list = AS_LIST(container);
                for (int i = 0; i < list->count; i++) {
                    if (IS_STRING(list->items[i]) &&
                        nc_string_equal(AS_STRING(needle), AS_STRING(list->items[i]))) {
                        found = true; break;
                    }
                }
            }
            vm_push(vm, NC_BOOL(found));
            break;
        }

        case OP_AND: {
            NcValue b = vm_pop(vm), a = vm_pop(vm);
            vm_push(vm, nc_truthy(a) ? b : a);
            break;
        }

        case OP_OR: {
            NcValue b = vm_pop(vm), a = vm_pop(vm);
            vm_push(vm, nc_truthy(a) ? a : b);
            break;
        }

        case OP_JUMP: {
            uint16_t offset = read_short(frame);
            frame->ip += offset;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            uint16_t offset = read_short(frame);
            if (!nc_truthy(vm_peek(vm, 0))) {
                frame->ip += offset;
            }
            break;
        }

        case OP_LOOP: {
            uint16_t offset = read_short(frame);
            static int max_loop_iters = 0;
            if (max_loop_iters == 0) {
                const char *env = getenv("NC_MAX_LOOP_ITERATIONS");
                if (env) {
                    long val = strtol(env, NULL, 10);
                    max_loop_iters = (val > 0 && val <= 1000000000) ? (int)val : 10000000;
                } else {
                    max_loop_iters = 10000000;
                }
            }
            if (++loop_guard > max_loop_iters) {
                vm_error(vm, "A loop ran too many times and was stopped. "
                    "Set NC_MAX_LOOP_ITERATIONS to increase the limit.");
                return result;
            }
            frame->ip -= offset;
            break;
        }

        case OP_SLICE: {
            NcValue end_val = vm_pop(vm);
            NcValue start_val = vm_pop(vm);
            NcValue obj = vm_pop(vm);

            if (IS_STRING(obj)) {
                NcString *s = AS_STRING(obj);
                int len = s->length;
                int start = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
                int end = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
                if (start < 0) start = len + start;
                if (end < 0) end = len + end;
                if (start < 0) start = 0;
                if (end > len) end = len;
                if (start >= end) { vm_push(vm, NC_STRING(nc_string_from_cstr(""))); break; }
                vm_push(vm, NC_STRING(nc_string_new(s->chars + start, end - start)));
            } else if (IS_LIST(obj)) {
                NcList *list = AS_LIST(obj);
                int len = list->count;
                int start = IS_INT(start_val) ? (int)AS_INT(start_val) : 0;
                int end = IS_INT(end_val) ? (int)AS_INT(end_val) : len;
                if (start < 0) start = len + start;
                if (end < 0) end = len + end;
                if (start < 0) start = 0;
                if (end > len) end = len;
                NcList *result = nc_list_new();
                for (int i = start; i < end; i++)
                    nc_list_push(result, list->items[i]);
                vm_push(vm, NC_LIST(result));
            } else {
                vm_push(vm, NC_NONE());
            }
            break;
        }

        case OP_SET_INDEX: {
            NcValue obj = vm_pop(vm);
            NcValue idx = vm_pop(vm);
            NcValue val = vm_pop(vm);
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
            vm_push(vm, val);
            break;
        }

        case OP_MAKE_LIST: {
            uint8_t count = read_byte(frame);
            if (count > vm->stack_top) {
                vm_error(vm, "Stack underflow in list construction (need %d, have %d).", count, vm->stack_top);
                return result;
            }
            NcList *list = nc_list_new();
            for (int i = count - 1; i >= 0; i--) {
                nc_list_push(list, vm->stack[vm->stack_top - count + i]);
            }
            vm->stack_top -= count;
            /* Reverse to maintain order */
            for (int i = 0; i < list->count / 2; i++) {
                NcValue tmp = list->items[i];
                list->items[i] = list->items[list->count - 1 - i];
                list->items[list->count - 1 - i] = tmp;
            }
            vm_push(vm, NC_LIST(list));
            break;
        }

        case OP_MAKE_MAP: {
            uint8_t count = read_byte(frame);
            if (count * 2 > vm->stack_top) {
                vm_error(vm, "Stack underflow in map construction (need %d, have %d).", count * 2, vm->stack_top);
                return result;
            }
            NcMap *map = nc_map_new();
            for (int i = count - 1; i >= 0; i--) {
                NcValue val = vm->stack[vm->stack_top - 1 - i * 2];
                NcValue key = vm->stack[vm->stack_top - 2 - i * 2];
                if (IS_STRING(key))
                    nc_map_set(map, AS_STRING(key), val);
            }
            vm->stack_top -= count * 2;
            vm_push(vm, NC_MAP(map));
            break;
        }

        case OP_LOG: {
            NcValue msg = vm_pop(vm);
            NcString *s = nc_value_to_string(msg);
            /* Resolve templates in log messages */
            char resolved[1024] = {0};
            const char *p = s->chars;
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
            /* Redact secrets before logging */
            char *safe = nc_redact_for_display(resolved);
            char buf[1100];
            snprintf(buf, sizeof(buf), "%s", safe ? safe : resolved);
            if (safe) free(safe);
            vm_output(vm, buf);
            nc_string_free(s);
            break;
        }

        case OP_EMIT: {
            NcValue event = vm_pop(vm);
            NcString *s = nc_value_to_string(event);
            char buf[512];
            snprintf(buf, sizeof(buf), "[EMIT] %s", s->chars);
            vm_output(vm, buf);
            nc_string_free(s);
            break;
        }

        case OP_STORE: {
            NcValue val = vm_pop(vm);
            NcValue key = vm_pop(vm);
            NcString *key_str = nc_value_to_string(key);
            char *val_json = nc_json_serialize(val, false);
            nc_store_put(key_str->chars, val_json);
            free(val_json);
            /* Also keep in globals for same-session access */
            nc_map_set(vm->globals, key_str, val);
            nc_string_free(key_str);
            break;
        }

        case OP_NOTIFY: {
            NcValue msg = vm_pop(vm);
            NcValue channel = vm_pop(vm);
            /* If channel is none (unresolved identifier), use the string from constants */
            NcString *ch = nc_value_to_string(channel);
            NcString *m = nc_value_to_string(msg);
            if (vm->notify_handler) {
                vm->notify_handler(vm, ch, m);
            } else {
                char buf[1024];
                snprintf(buf, sizeof(buf), "[NOTIFY -> %s] %s", ch->chars, m->chars);
                vm_output(vm, buf);
            }
            nc_string_free(ch);
            nc_string_free(m);
            break;
        }

        case OP_GATHER: {
            NcValue options = vm_pop(vm);
            NcValue source = vm_pop(vm);
            NcString *src_str = nc_value_to_string(source);
            NcMap *opts = IS_MAP(options) ? AS_MAP(options) : nc_map_new();
            NcValue gathered;
            if (vm->mcp_handler) {
                gathered = vm->mcp_handler(vm, src_str, opts);
            } else {
                /* Real HTTP call via nc_gather_from */
                gathered = nc_gather_from(src_str->chars, opts);
            }
            vm_push(vm, gathered);
            nc_string_free(src_str);
            break;
        }

        case OP_ASK_AI: {
            NcValue context = vm_pop(vm);
            NcValue prompt = vm_pop(vm);
            NcString *p = nc_value_to_string(prompt);

            /* Resolve templates in prompt using dynamic buffer */
            NcDynBuf resolved;
            nc_dbuf_init(&resolved, p->length * 2 + 256);
            const char *pp = p->chars;
            while (*pp) {
                if (*pp == '{' && *(pp+1) == '{') {
                    pp += 2;
                    char vname[128] = {0}; int vi = 0;
                    while (*pp && !(*pp == '}' && *(pp+1) == '}') && vi < 126) vname[vi++] = *pp++;
                    if (*pp == '}') pp += 2;
                    NcString *key = nc_string_from_cstr(vname);
                    NcValue val = nc_map_get(vm->globals, key);
                    nc_string_free(key);
                    NcString *vs = nc_value_to_string(val);
                    nc_dbuf_append_len(&resolved, vs->chars, vs->length);
                    nc_string_free(vs);
                } else {
                    nc_dbuf_append_len(&resolved, pp, 1);
                    pp++;
                }
            }

            /* Build context map from list of values */
            NcMap *ctx = nc_map_new();
            if (IS_LIST(context)) {
                for (int ci = 0; ci < AS_LIST(context)->count; ci++) {
                    char key[32]; snprintf(key, sizeof(key), "arg_%d", ci);
                    nc_map_set(ctx, nc_string_from_cstr(key), AS_LIST(context)->items[ci]);
                }
            } else if (IS_MAP(context)) {
                ctx = AS_MAP(context);
            }

            NcValue ai_result;
            if (vm->ai_handler) {
                ai_result = vm->ai_handler(vm, p, ctx, NULL);
            } else {
                ai_result = nc_ai_call_ex(resolved.data, ctx, nc_map_new());
            }
            vm_push(vm, ai_result);
            nc_dbuf_free(&resolved);
            nc_string_free(p);
            break;
        }

        case OP_WAIT: {
            NcValue seconds = vm_pop(vm);
            double secs = as_number(seconds);
            if (secs > 0 && secs < 300) {
                nc_sleep_ms((int)(secs * 1000));
            }
            break;
        }

        case OP_RESPOND: {
            result = vm_pop(vm);
            /* Resolve templates in string results */
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
            /* respond exits the current behavior immediately */
            vm->frame_count--;
            return result;
        }

        case OP_CALL_NATIVE: {
            uint8_t argc = read_byte(frame);
            if (argc + 1 > vm->stack_top) {
                vm_error(vm, "Stack underflow in native call (need %d, have %d).", argc + 1, vm->stack_top);
                return result;
            }
            NcValue fn = vm->stack[vm->stack_top - argc - 1];
            if (fn.type == VAL_NATIVE_FN) {
                NcValue *args = &vm->stack[vm->stack_top - argc];
                NcValue r = AS_NATIVE(fn)(vm, argc, args);
                vm->stack_top -= argc + 1;
                vm_push(vm, r);
            } else if (IS_STRING(fn)) {
                /* String-named builtin dispatch */
                const char *name = AS_STRING(fn)->chars;
                NcValue *args = &vm->stack[vm->stack_top - argc];
                NcValue r = NC_NONE();

                if (strcmp(name, "len") == 0 && argc == 1) {
                    if (IS_LIST(args[0])) r = NC_INT(AS_LIST(args[0])->count);
                    else if (IS_STRING(args[0])) r = NC_INT(AS_STRING(args[0])->length);
                    else if (IS_MAP(args[0])) r = NC_INT(AS_MAP(args[0])->count);
                } else if (strcmp(name, "get") == 0 && argc >= 2) {
                    if (IS_LIST(args[0]) && IS_INT(args[1])) {
                        r = nc_list_get(AS_LIST(args[0]), (int)AS_INT(args[1]));
                        nc_value_retain(r);
                    } else if (IS_MAP(args[0]) && IS_STRING(args[1])) {
                        r = nc_map_get(AS_MAP(args[0]), AS_STRING(args[1]));
                        nc_value_retain(r);
                    } else if (argc >= 3) {
                        r = args[2];
                    }
                } else if (strcmp(name, "str") == 0 && argc == 1) {
                    r = NC_STRING(nc_value_to_string(args[0]));
                } else if (strcmp(name, "int") == 0 && argc == 1) {
                    r = NC_INT((int64_t)as_number(args[0]));
                } else if (strcmp(name, "print") == 0) {
                    for (int a = 0; a < argc; a++) {
                        NcString *s = nc_value_to_string(args[a]);
                        vm_output(vm, s->chars);
                        nc_string_free(s);
                    }
                } else if (strcmp(name, "keys") == 0 && argc == 1 && IS_MAP(args[0])) {
                    NcList *l = nc_list_new();
                    for (int i = 0; i < AS_MAP(args[0])->count; i++)
                        nc_list_push(l, NC_STRING(nc_string_ref(AS_MAP(args[0])->keys[i])));
                    r = NC_LIST(l);
                } else if (strcmp(name, "values") == 0 && argc == 1 && IS_MAP(args[0])) {
                    NcList *l = nc_list_new();
                    for (int i = 0; i < AS_MAP(args[0])->count; i++)
                        nc_list_push(l, AS_MAP(args[0])->values[i]);
                    r = NC_LIST(l);
                } else if (strcmp(name, "upper") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_upper(AS_STRING(args[0]));
                } else if (strcmp(name, "lower") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_lower(AS_STRING(args[0]));
                } else if (strcmp(name, "trim") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_trim(AS_STRING(args[0]));
                } else if (strcmp(name, "contains") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_contains(AS_STRING(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "replace") == 0 && argc == 3) {
                    if (IS_STRING(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2]))
                        r = nc_stdlib_replace(AS_STRING(args[0]), AS_STRING(args[1]), AS_STRING(args[2]));
                } else if (strcmp(name, "split") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_split(AS_STRING(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "join") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_join(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "abs") == 0 && argc == 1) {
                    double val = as_number(args[0]);
                    double res = fabs(val);
                    r = (IS_INT(args[0]) && res == (int64_t)res) ? NC_INT((int64_t)res) : NC_FLOAT(res);
                } else if (strcmp(name, "sqrt") == 0 && argc == 1) {
                    r = nc_stdlib_sqrt(as_number(args[0]));
                } else if (strcmp(name, "random") == 0 && argc == 0) {
                    r = nc_stdlib_random();
                } else if (strcmp(name, "time_now") == 0 && argc == 0) {
                    r = nc_stdlib_time_now();
                } else if (strcmp(name, "read_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_read_file(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "write_file") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_write_file(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "file_exists") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_file_exists(AS_STRING(args[0])->chars);
                } else if ((strcmp(name, "delete_file") == 0 || strcmp(name, "remove_file") == 0) && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_delete_file(AS_STRING(args[0])->chars);
                } else if ((strcmp(name, "mkdir") == 0 || strcmp(name, "create_directory") == 0) && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_mkdir(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "exec") == 0 && argc >= 1) {
                    NcList *exec_args = nc_list_new();
                    for (int ei = 0; ei < argc; ei++) nc_list_push(exec_args, args[ei]);
                    r = nc_stdlib_exec(exec_args);
                } else if (strcmp(name, "shell") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_shell(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "json_encode") == 0 && argc == 1) {
                    char *j = nc_json_serialize(args[0], false);
                    r = NC_STRING(nc_string_from_cstr(j));
                    free(j);
                } else if (strcmp(name, "json_decode") == 0 && argc == 1) {
                    /* If already parsed, return as-is */
                    if (IS_LIST(args[0]) || IS_MAP(args[0]) || IS_INT(args[0]) ||
                        IS_FLOAT(args[0]) || IS_BOOL(args[0]))
                        r = args[0];
                    else if (IS_STRING(args[0]))
                        r = nc_json_parse(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "env") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_env_get(AS_STRING(args[0])->chars);

                /* ── OS module (glob, walk, is_dir, is_file, etc.) ── */
                } else if (strcmp(name, "os_env") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_env(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_cwd") == 0 && argc == 0) {
                    r = nc_stdlib_os_cwd();
                } else if (strcmp(name, "os_listdir") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_listdir(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_glob") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_os_glob(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "os_walk") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_walk(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_exists") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_exists(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_is_dir") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_is_dir(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_is_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_is_file(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_file_size") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_file_size(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_mkdir") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_mkdir_p(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_remove") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_remove(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_read_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_read_file(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_write_file") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_os_write_file(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "os_exec") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_exec(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_path_join") == 0 && argc >= 1) {
                    NcList *parts = nc_list_new();
                    for (int pi = 0; pi < argc; pi++) nc_list_push(parts, args[pi]);
                    r = nc_stdlib_os_path_join(parts);
                } else if (strcmp(name, "os_basename") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_basename(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_dirname") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_dirname(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "os_setenv") == 0 && argc >= 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_os_setenv(AS_STRING(args[0])->chars,
                        argc >= 2 && IS_STRING(args[1]) ? AS_STRING(args[1])->chars : NULL);

                } else if (strcmp(name, "platform_system") == 0 && argc == 0) {
                    r = nc_stdlib_platform_system();
                } else if (strcmp(name, "platform_architecture") == 0 && argc == 0) {
                    r = nc_stdlib_platform_architecture();
                } else if (strcmp(name, "platform_info") == 0 && argc == 0) {
                    r = nc_stdlib_platform_info();
                } else if (strcmp(name, "sort") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_sort_by(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "sort") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_stdlib_list_sort(AS_LIST(args[0]));
                } else if (strcmp(name, "reverse") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_stdlib_list_reverse(AS_LIST(args[0]));
                } else if (strcmp(name, "chunk") == 0 && argc >= 1 && IS_STRING(args[0])) {
                    int size = argc >= 2 && IS_INT(args[1]) ? (int)AS_INT(args[1]) : 500;
                    r = nc_stdlib_chunk(AS_STRING(args[0]), size, 0);
                } else if (strcmp(name, "top_k") == 0 && argc >= 2 && IS_LIST(args[0])) {
                    int k = IS_INT(args[1]) ? (int)AS_INT(args[1]) : 5;
                    r = nc_stdlib_top_k(AS_LIST(args[0]), k);
                } else if (strcmp(name, "append") == 0 && argc == 2 && IS_LIST(args[0])) {
                    nc_list_push(AS_LIST(args[0]), args[1]);
                    r = NC_LIST(AS_LIST(args[0]));
                    nc_value_retain(r);
                } else if (strcmp(name, "remove") == 0 && argc == 2 && IS_LIST(args[0])) {
                    NcList *list = AS_LIST(args[0]);
                    for (int i = 0; i < list->count; i++) {
                        bool match = false;
                        if (IS_STRING(args[1]) && IS_STRING(list->items[i]))
                            match = nc_string_equal(AS_STRING(args[1]), AS_STRING(list->items[i]));
                        else if (IS_INT(args[1]) && IS_INT(list->items[i]))
                            match = AS_INT(args[1]) == AS_INT(list->items[i]);
                        if (match) {
                            NcValue removed = list->items[i];
                            for (int j = i; j < list->count - 1; j++)
                                list->items[j] = list->items[j + 1];
                            list->count--;
                            nc_value_release(removed);
                            break;
                        }
                    }
                    r = NC_LIST(list);
                    nc_value_retain(r);
                } else if (strcmp(name, "type") == 0 && argc == 1) {
                    const char *t = "none";
                    if (IS_INT(args[0]) || IS_FLOAT(args[0])) t = "number";
                    else if (IS_BOOL(args[0])) t = "yesno";
                    else if (IS_STRING(args[0])) t = "text";
                    else if (IS_LIST(args[0])) t = "list";
                    else if (IS_MAP(args[0])) t = "record";
                    r = NC_STRING(nc_string_from_cstr(t));
                } else if (strcmp(name, "load_model") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_load_model(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "predict") == 0 && argc == 2 && IS_LIST(args[1])) {
                    r = nc_stdlib_predict(args[0], AS_LIST(args[1]));
                } else if (strcmp(name, "is_text") == 0 && argc == 1) {
                    r = NC_BOOL(IS_STRING(args[0]));
                } else if (strcmp(name, "is_number") == 0 && argc == 1) {
                    r = NC_BOOL(IS_INT(args[0]) || IS_FLOAT(args[0]));
                } else if (strcmp(name, "is_list") == 0 && argc == 1) {
                    r = NC_BOOL(IS_LIST(args[0]));
                } else if (strcmp(name, "is_record") == 0 && argc == 1) {
                    r = NC_BOOL(IS_MAP(args[0]));
                } else if (strcmp(name, "is_none") == 0 && argc == 1) {
                    r = NC_BOOL(IS_NONE(args[0]));
                } else if (strcmp(name, "float") == 0 && argc == 1) {
                    r = NC_FLOAT(as_number(args[0]));
                } else if (strcmp(name, "has_key") == 0 && argc == 2 && IS_MAP(args[0]) && IS_STRING(args[1])) {
                    r = NC_BOOL(nc_map_has(AS_MAP(args[0]), AS_STRING(args[1])));
                } else if (strcmp(name, "starts_with") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_starts_with(AS_STRING(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "ends_with") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_ends_with(AS_STRING(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "pow") == 0 && argc == 2) {
                    r = nc_stdlib_pow(as_number(args[0]), as_number(args[1]));
                } else if (strcmp(name, "min") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *l = AS_LIST(args[0]);
                    if (l->count > 0) {
                        double best = as_number(l->items[0]); int bi = 0;
                        for (int li = 1; li < l->count; li++) {
                            double v = as_number(l->items[li]);
                            if (v < best) { best = v; bi = li; }
                        }
                        r = l->items[bi];
                    }
                } else if (strcmp(name, "min") == 0 && argc == 2) {
                    double a0 = as_number(args[0]), a1 = as_number(args[1]);
                    r = a0 <= a1 ? args[0] : args[1];
                } else if (strcmp(name, "max") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *l = AS_LIST(args[0]);
                    if (l->count > 0) {
                        double best = as_number(l->items[0]); int bi = 0;
                        for (int li = 1; li < l->count; li++) {
                            double v = as_number(l->items[li]);
                            if (v > best) { best = v; bi = li; }
                        }
                        r = l->items[bi];
                    }
                } else if (strcmp(name, "max") == 0 && argc == 2) {
                    double a0 = as_number(args[0]), a1 = as_number(args[1]);
                    r = a0 >= a1 ? args[0] : args[1];
                } else if (strcmp(name, "round") == 0 && argc == 2) {
                    double val = as_number(args[0]);
                    int decimals = (int)as_number(args[1]);
                    double factor = pow(10.0, decimals);
                    r = NC_FLOAT(round(val * factor) / factor);
                } else if (strcmp(name, "round") == 0 && argc == 1) {
                    r = NC_INT((int64_t)round(as_number(args[0])));
                } else if (strcmp(name, "ceil") == 0 && argc == 1) {
                    r = NC_INT((int64_t)ceil(as_number(args[0])));
                } else if (strcmp(name, "floor") == 0 && argc == 1) {
                    r = NC_INT((int64_t)floor(as_number(args[0])));
                } else if (strcmp(name, "first") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *l = AS_LIST(args[0]);
                    r = l->count > 0 ? l->items[0] : NC_NONE();
                } else if (strcmp(name, "last") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *l = AS_LIST(args[0]);
                    r = l->count > 0 ? l->items[l->count - 1] : NC_NONE();
                } else if (strcmp(name, "sum") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *sl = AS_LIST(args[0]); double s = 0;
                    for (int i = 0; i < sl->count; i++) s += as_number(sl->items[i]);
                    r = (s == (int64_t)s) ? NC_INT((int64_t)s) : NC_FLOAT(s);
                } else if (strcmp(name, "average") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *al = AS_LIST(args[0]); double s = 0;
                    for (int i = 0; i < al->count; i++) s += as_number(al->items[i]);
                    double avg = al->count > 0 ? s / al->count : 0;
                    r = NC_FLOAT(avg);
                } else if (strcmp(name, "unique") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *ul = AS_LIST(args[0]); NcList *out = nc_list_new();
                    for (int i = 0; i < ul->count; i++) {
                        bool dup = false;
                        for (int j = 0; j < out->count && !dup; j++) {
                            if (IS_INT(ul->items[i]) && IS_INT(out->items[j])) dup = AS_INT(ul->items[i]) == AS_INT(out->items[j]);
                            else if (IS_STRING(ul->items[i]) && IS_STRING(out->items[j])) dup = nc_string_equal(AS_STRING(ul->items[i]), AS_STRING(out->items[j]));
                        }
                        if (!dup) nc_list_push(out, ul->items[i]);
                    }
                    r = NC_LIST(out);
                } else if (strcmp(name, "flatten") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *fl = AS_LIST(args[0]); NcList *out = nc_list_new();
                    for (int i = 0; i < fl->count; i++) {
                        if (IS_LIST(fl->items[i])) {
                            NcList *sub = AS_LIST(fl->items[i]);
                            for (int j = 0; j < sub->count; j++) nc_list_push(out, sub->items[j]);
                        } else nc_list_push(out, fl->items[i]);
                    }
                    r = NC_LIST(out);
                } else if (strcmp(name, "index_of") == 0 && argc == 2 && IS_LIST(args[0])) {
                    NcList *il = AS_LIST(args[0]); r = NC_INT(-1);
                    for (int i = 0; i < il->count; i++) {
                        if (IS_INT(args[1]) && IS_INT(il->items[i]) && AS_INT(args[1]) == AS_INT(il->items[i])) { r = NC_INT(i); break; }
                        if (IS_STRING(args[1]) && IS_STRING(il->items[i]) && nc_string_equal(AS_STRING(args[1]), AS_STRING(il->items[i]))) { r = NC_INT(i); break; }
                    }
                } else if (strcmp(name, "count") == 0 && argc == 2 && IS_LIST(args[0])) {
                    NcList *cl = AS_LIST(args[0]); int cnt = 0;
                    for (int i = 0; i < cl->count; i++) {
                        if (IS_INT(args[1]) && IS_INT(cl->items[i]) && AS_INT(args[1]) == AS_INT(cl->items[i])) cnt++;
                        if (IS_STRING(args[1]) && IS_STRING(cl->items[i]) && nc_string_equal(AS_STRING(args[1]), AS_STRING(cl->items[i]))) cnt++;
                    }
                    r = NC_INT(cnt);
                } else if (strcmp(name, "slice") == 0 && argc >= 2) {
                    NcValue list_val = args[0];
                    if (IS_STRING(list_val) && AS_STRING(list_val)->length > 0 &&
                        (AS_STRING(list_val)->chars[0] == '[' || AS_STRING(list_val)->chars[0] == '{')) {
                        list_val = nc_json_parse(AS_STRING(list_val)->chars);
                    }
                    if (IS_LIST(list_val)) {
                        NcList *sll = AS_LIST(list_val);
                        int start = (int)as_number(args[1]);
                        int end = argc >= 3 ? (int)as_number(args[2]) : sll->count;
                        if (start < 0) start = sll->count + start;
                        if (end < 0) end = sll->count + end;
                        if (start < 0) start = 0;
                        if (end > sll->count) end = sll->count;
                        if (start > end) start = end;
                        NcList *out = nc_list_new();
                        for (int i = start; i < end; i++) nc_list_push(out, sll->items[i]);
                        r = NC_LIST(out);
                    }
                } else if (strcmp(name, "any") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *al2 = AS_LIST(args[0]); r = NC_BOOL(false);
                    for (int i = 0; i < al2->count; i++) {
                        if (IS_BOOL(al2->items[i]) && AS_BOOL(al2->items[i])) { r = NC_BOOL(true); break; }
                        if (IS_INT(al2->items[i]) && AS_INT(al2->items[i]) != 0) { r = NC_BOOL(true); break; }
                    }
                } else if (strcmp(name, "all") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *al3 = AS_LIST(args[0]); r = NC_BOOL(true);
                    for (int i = 0; i < al3->count; i++) {
                        if (IS_BOOL(al3->items[i]) && !AS_BOOL(al3->items[i])) { r = NC_BOOL(false); break; }
                        if (IS_INT(al3->items[i]) && AS_INT(al3->items[i]) == 0) { r = NC_BOOL(false); break; }
                        if (IS_NONE(al3->items[i])) { r = NC_BOOL(false); break; }
                    }
                } else if (strcmp(name, "cache") == 0 && argc == 2 && IS_STRING(args[0])) {
                    nc_stdlib_cache_set(AS_STRING(args[0])->chars, args[1]);
                    r = args[1];
                } else if (strcmp(name, "cached") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_cache_get(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "is_cached") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = NC_BOOL(nc_stdlib_cache_has(AS_STRING(args[0])->chars));
                } else if (strcmp(name, "memory_new") == 0 && argc <= 1) {
                    r = nc_stdlib_memory_new(argc == 1 && IS_INT(args[0]) ? (int)AS_INT(args[0]) : 50);
                } else if (strcmp(name, "memory_add") == 0 && argc == 3 && IS_STRING(args[1]) && IS_STRING(args[2])) {
                    r = nc_stdlib_memory_add(args[0], AS_STRING(args[1])->chars, AS_STRING(args[2])->chars);
                } else if (strcmp(name, "memory_get") == 0 && argc == 1) {
                    r = nc_stdlib_memory_get(args[0]);
                } else if (strcmp(name, "memory_clear") == 0 && argc == 1) {
                    r = nc_stdlib_memory_clear(args[0]);
                } else if (strcmp(name, "memory_summary") == 0 && argc == 1) {
                    r = nc_stdlib_memory_summary(args[0]);
                } else if (strcmp(name, "memory_save") == 0 && argc == 2 && IS_STRING(args[1])) {
                    r = nc_stdlib_memory_save(args[0], AS_STRING(args[1])->chars);
                } else if (strcmp(name, "memory_load") == 0 && argc >= 1 && argc <= 2 && IS_STRING(args[0])) {
                    r = nc_stdlib_memory_load(AS_STRING(args[0])->chars,
                                              argc == 2 && IS_INT(args[1]) ? (int)AS_INT(args[1]) : 0);
                } else if (strcmp(name, "memory_store") == 0 && argc >= 3 && argc <= 5 && IS_STRING(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2])) {
                    double reward = 0.0;
                    if (argc == 5) {
                        if (IS_FLOAT(args[4])) reward = AS_FLOAT(args[4]);
                        else if (IS_INT(args[4])) reward = (double)AS_INT(args[4]);
                    }
                    r = nc_stdlib_memory_store(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars,
                                               AS_STRING(args[2])->chars,
                                               argc >= 4 ? args[3] : NC_NONE(), reward);
                } else if (strcmp(name, "memory_search") == 0 && argc >= 2 && argc <= 3 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_memory_search(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars,
                                                argc == 3 && IS_INT(args[2]) ? (int)AS_INT(args[2]) : 5);
                } else if (strcmp(name, "memory_context") == 0 && argc >= 2 && argc <= 3 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_memory_context(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars,
                                                 argc == 3 && IS_INT(args[2]) ? (int)AS_INT(args[2]) : 5);
                } else if (strcmp(name, "memory_reflect") == 0 && argc == 6 && IS_STRING(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2]) && IS_STRING(args[3]) && IS_STRING(args[5])) {
                    double conf = IS_FLOAT(args[4]) ? AS_FLOAT(args[4]) : (IS_INT(args[4]) ? (double)AS_INT(args[4]) : 0.0);
                    r = nc_stdlib_memory_reflect(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars,
                                                 AS_STRING(args[2])->chars, AS_STRING(args[3])->chars,
                                                 conf, AS_STRING(args[5])->chars);
                } else if (strcmp(name, "policy_update") == 0 && argc == 3 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    double reward = IS_FLOAT(args[2]) ? AS_FLOAT(args[2]) : (IS_INT(args[2]) ? (double)AS_INT(args[2]) : 0.0);
                    r = nc_stdlib_policy_update(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars, reward);
                } else if (strcmp(name, "policy_choose") == 0 && argc >= 2 && argc <= 3 && IS_STRING(args[0]) && IS_LIST(args[1])) {
                    double epsilon = argc == 3 ? (IS_FLOAT(args[2]) ? AS_FLOAT(args[2]) : (IS_INT(args[2]) ? (double)AS_INT(args[2]) : 0.0)) : 0.0;
                    r = nc_stdlib_policy_choose(AS_STRING(args[0])->chars, AS_LIST(args[1]), epsilon);
                } else if (strcmp(name, "policy_stats") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_policy_stats(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "token_count") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = NC_INT(nc_stdlib_token_count(AS_STRING(args[0])->chars));
                } else if (strcmp(name, "validate") == 0 && argc == 2 && IS_LIST(args[1])) {
                    r = nc_stdlib_validate(args[0], AS_LIST(args[1]));
                } else if (strcmp(name, "input") == 0) {
                    const char *prompt = argc >= 1 && IS_STRING(args[0]) ? AS_STRING(args[0])->chars : "";
                    char buf[1024] = {0};
                    if (prompt[0]) printf("%s", prompt);
                    if (fgets(buf, sizeof(buf), stdin)) {
                        int blen = (int)strlen(buf);
                        if (blen > 0 && buf[blen-1] == '\n') buf[blen-1] = '\0';
                    }
                    r = NC_STRING(nc_string_from_cstr(buf));
                } else if (strcmp(name, "chr") == 0 && argc == 1) {
                    char ch[2] = { (char)(int)as_number(args[0]), 0 };
                    r = NC_STRING(nc_string_from_cstr(ch));
                } else if (strcmp(name, "ord") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = NC_INT((int64_t)(unsigned char)AS_STRING(args[0])->chars[0]);
                } else if (strcmp(name, "substr") == 0 && argc >= 2 && IS_STRING(args[0])) {
                    int start = (int)as_number(args[1]);
                    int slen = argc >= 3 ? (int)as_number(args[2]) : AS_STRING(args[0])->length - start;
                    if (start < 0) start = 0;
                    if (start > AS_STRING(args[0])->length) start = AS_STRING(args[0])->length;
                    if (slen < 0) slen = 0;
                    if (start + slen > AS_STRING(args[0])->length) slen = AS_STRING(args[0])->length - start;
                    char *sub = malloc(slen + 1);
                    memcpy(sub, AS_STRING(args[0])->chars + start, slen);
                    sub[slen] = '\0';
                    r = NC_STRING(nc_string_from_cstr(sub));
                    free(sub);
                } else if (strcmp(name, "time_ms") == 0 && argc == 0) {
                    r = nc_stdlib_time_ms();
                } else if (strcmp(name, "time_format") == 0 && argc == 2 && IS_STRING(args[1])) {
                    double ts = IS_INT(args[0]) ? (double)AS_INT(args[0]) : (IS_FLOAT(args[0]) ? AS_FLOAT(args[0]) : 0);
                    r = nc_stdlib_time_format(ts, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "csv_parse") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_csv_parse(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "yaml_parse") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_yaml_parse(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "xml_parse") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_xml_parse(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "toml_parse") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_toml_parse(AS_STRING(args[0])->chars);
                /* ── Math: trig + log/exp ────────────────── */
                } else if (strcmp(name, "cos") == 0 && argc == 1) {
                    r = NC_FLOAT(cos(as_number(args[0])));
                } else if (strcmp(name, "sin") == 0 && argc == 1) {
                    r = NC_FLOAT(sin(as_number(args[0])));
                } else if (strcmp(name, "tan") == 0 && argc == 1) {
                    r = NC_FLOAT(tan(as_number(args[0])));
                } else if (strcmp(name, "log") == 0 && argc == 1 && IS_NUMBER(args[0])) {
                    double v = as_number(args[0]);
                    r = v > 0 ? NC_FLOAT(log(v)) : NC_FLOAT(0);
                } else if (strcmp(name, "exp") == 0 && argc == 1) {
                    r = NC_FLOAT(exp(as_number(args[0])));

                /* ── Functional: enumerate, zip, filter, map_list ── */
                } else if (strcmp(name, "enumerate") == 0 && argc == 1 && IS_LIST(args[0])) {
                    NcList *src = AS_LIST(args[0]);
                    NcList *out = nc_list_new();
                    for (int i = 0; i < src->count; i++) {
                        NcList *pair = nc_list_new();
                        nc_list_push(pair, NC_INT(i));
                        nc_list_push(pair, src->items[i]);
                        nc_list_push(out, NC_LIST(pair));
                    }
                    r = NC_LIST(out);
                } else if (strcmp(name, "zip") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    NcList *a = AS_LIST(args[0]), *b = AS_LIST(args[1]);
                    NcList *out = nc_list_new();
                    int n = a->count < b->count ? a->count : b->count;
                    for (int i = 0; i < n; i++) {
                        NcList *pair = nc_list_new();
                        nc_list_push(pair, a->items[i]);
                        nc_list_push(pair, b->items[i]);
                        nc_list_push(out, NC_LIST(pair));
                    }
                    r = NC_LIST(out);
                } else if (strcmp(name, "filter") == 0 && argc == 2 && IS_LIST(args[0])) {
                    NcList *src = AS_LIST(args[0]);
                    NcList *out = nc_list_new();
                    for (int i = 0; i < src->count; i++) {
                        if (nc_truthy(src->items[i]))
                            nc_list_push(out, src->items[i]);
                    }
                    r = NC_LIST(out);
                } else if (strcmp(name, "map_list") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    NcList *src = AS_LIST(args[0]);
                    NcString *fn_name_str = AS_STRING(args[1]);
                    NcList *out = nc_list_new();
                    NcValue fn_chunk = nc_map_get(vm->behaviors, fn_name_str);
                    if (IS_INT(fn_chunk) && vm->behavior_chunks) {
                        int ci = (int)AS_INT(fn_chunk);
                        for (int i = 0; i < src->count; i++) {
                            if (ci >= 0 && ci < vm->behavior_chunk_count) {
                                NcChunk *tc = &vm->behavior_chunks[ci];
                                if (tc->var_count > 0)
                                    nc_map_set(vm->globals, tc->var_names[0], src->items[i]);
                                NcValue mr = nc_vm_execute(vm, tc);
                                nc_list_push(out, mr);
                            }
                        }
                    }
                    r = NC_LIST(out);

                /* ── Shell/IO extensions ─────────────────── */
                } else if (strcmp(name, "shell_exec") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_shell_exec(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "write_file_atomic") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_write_file_atomic(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);

                /* ── Hashing / encoding ──────────────────── */
                } else if (strcmp(name, "hash") == 0 && argc == 1 && IS_STRING(args[0])) {
                    uint32_t h = nc_hash_string(AS_STRING(args[0])->chars, AS_STRING(args[0])->length);
                    char hbuf[16]; snprintf(hbuf, sizeof(hbuf), "%08x", h);
                    r = NC_STRING(nc_string_from_cstr(hbuf));
                } else if (strcmp(name, "uuid") == 0 && argc == 0) {
                    char ubuf[40];
                    snprintf(ubuf, sizeof(ubuf), "%08x-%04x-%04x-%04x-%04x%08x",
                        (uint32_t)rand(), (uint16_t)rand() & 0xffff, (uint16_t)(0x4000 | (rand() & 0x0fff)),
                        (uint16_t)(0x8000 | (rand() & 0x3fff)), (uint16_t)rand() & 0xffff, (uint32_t)rand());
                    r = NC_STRING(nc_string_from_cstr(ubuf));
                } else if (strcmp(name, "hex") == 0 && argc == 1) {
                    char hbuf[32]; snprintf(hbuf, sizeof(hbuf), "0x%llx", (long long)AS_INT(args[0]));
                    r = NC_STRING(nc_string_from_cstr(hbuf));
                } else if (strcmp(name, "bin") == 0 && argc == 1) {
                    int64_t v = AS_INT(args[0]);
                    char bbuf[72] = "0b"; int bi = 2;
                    if (v == 0) { bbuf[bi++] = '0'; }
                    else { for (int bit = 63; bit >= 0; bit--) { if (bi > 2 || (v >> bit) & 1) bbuf[bi++] = ((v >> bit) & 1) ? '1' : '0'; } }
                    bbuf[bi] = '\0';
                    r = NC_STRING(nc_string_from_cstr(bbuf));

                /* ── Regex ───────────────────────────────── */
#ifndef NC_WINDOWS
                } else if (strcmp(name, "re_match") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    regex_t re;
                    if (regcomp(&re, AS_STRING(args[1])->chars, REG_EXTENDED | REG_NOSUB) == 0) {
                        r = NC_BOOL(regexec(&re, AS_STRING(args[0])->chars, 0, NULL, 0) == 0);
                        regfree(&re);
                    } else { r = NC_BOOL(false); }
                } else if (strcmp(name, "re_find") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    regex_t re; regmatch_t match[1];
                    if (regcomp(&re, AS_STRING(args[1])->chars, REG_EXTENDED) == 0) {
                        if (regexec(&re, AS_STRING(args[0])->chars, 1, match, 0) == 0) {
                            int mlen = match[0].rm_eo - match[0].rm_so;
                            r = NC_STRING(nc_string_new(AS_STRING(args[0])->chars + match[0].rm_so, mlen));
                        }
                        regfree(&re);
                    }
                } else if (strcmp(name, "re_replace") == 0 && argc == 3 && IS_STRING(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2])) {
                    regex_t re; regmatch_t match[1];
                    if (regcomp(&re, AS_STRING(args[1])->chars, REG_EXTENDED) == 0) {
                        const char *src = AS_STRING(args[0])->chars;
                        const char *repl = AS_STRING(args[2])->chars;
                        int rlen = AS_STRING(args[2])->length;
                        NcDynBuf out; nc_dbuf_init(&out, AS_STRING(args[0])->length * 2);
                        while (regexec(&re, src, 1, match, 0) == 0) {
                            nc_dbuf_append_len(&out, src, match[0].rm_so);
                            nc_dbuf_append_len(&out, repl, rlen);
                            src += match[0].rm_eo;
                            if (match[0].rm_so == match[0].rm_eo) { if (*src) { nc_dbuf_append_len(&out, src, 1); src++; } else break; }
                        }
                        nc_dbuf_append_len(&out, src, (int)strlen(src));
                        r = NC_STRING(nc_string_from_cstr(out.data));
                        nc_dbuf_free(&out);
                        regfree(&re);
                    }
#endif

                /* ── HTTP request ────────────────────────── */
                } else if (strcmp(name, "http_get") == 0 && argc >= 1 && IS_STRING(args[0])) {
                    NcMap *hdrs = argc >= 2 && IS_MAP(args[1]) ? AS_MAP(args[1]) : NULL;
                    r = nc_stdlib_http_request("GET", AS_STRING(args[0])->chars, hdrs, NC_NONE());
                } else if (strcmp(name, "http_post") == 0 && argc >= 2 && IS_STRING(args[0])) {
                    NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                    r = nc_stdlib_http_request("POST", AS_STRING(args[0])->chars, hdrs, args[1]);
                } else if (strcmp(name, "http_request") == 0 && argc >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                    NcValue body = argc >= 4 ? args[3] : NC_NONE();
                    r = nc_stdlib_http_request(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars, hdrs, body);

                } else if (strcmp(name, "jwt_generate") == 0 && argc >= 2) {
                    const char *uid = IS_STRING(args[0]) ? AS_STRING(args[0])->chars : "anonymous";
                    const char *rl = IS_STRING(args[1]) ? AS_STRING(args[1])->chars : "user";
                    int exp_s = argc >= 3 && IS_INT(args[2]) ? (int)AS_INT(args[2]) : 3600;
                    NcMap *extra = argc >= 4 && IS_MAP(args[3]) ? AS_MAP(args[3]) : NULL;
                    r = nc_jwt_generate(uid, rl, exp_s, extra);

                } else if (strcmp(name, "time_iso") == 0) {
                    double ts = argc >= 1 ? as_number(args[0]) : (double)time(NULL);
                    r = nc_stdlib_time_iso(ts);

                } else if (strcmp(name, "find_similar") == 0 && argc >= 3 && IS_LIST(args[0]) && IS_LIST(args[1]) && IS_LIST(args[2])) {
                    int k = argc >= 4 && IS_INT(args[3]) ? (int)AS_INT(args[3]) : 5;
                    r = nc_stdlib_find_similar(AS_LIST(args[0]), AS_LIST(args[1]), AS_LIST(args[2]), k);

                } else if (strcmp(name, "ini_parse") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_ini_parse(AS_STRING(args[0])->chars);

                } else if (strcmp(name, "get") == 0 && argc >= 2 && argc <= 3) {
                    NcValue fallback = argc == 3 ? args[2] : NC_NONE();
                    if (IS_MAP(args[0])) {
                        NcString *map_key = IS_STRING(args[1])
                            ? nc_string_ref(AS_STRING(args[1]))
                            : nc_value_to_string(args[1]);
                        r = nc_map_get(AS_MAP(args[0]), map_key);
                        nc_string_free(map_key);
                        if (IS_NONE(r) && argc == 3) r = fallback;
                    } else if (IS_LIST(args[0]) && IS_INT(args[1])) {
                        NcList *list = AS_LIST(args[0]);
                        int64_t idx = AS_INT(args[1]);
                        if (idx >= 0 && idx < list->count) r = list->items[idx];
                        else r = argc == 3 ? fallback : NC_NONE();
                    } else {
                        r = argc == 3 ? fallback : NC_NONE();
                    }

                } else if (strcmp(name, "range") == 0) {
                    int64_t start = 0, end = 0;
                    if (argc >= 1) end = IS_INT(args[0]) ? AS_INT(args[0]) : (int64_t)as_number(args[0]);
                    if (argc >= 2) { start = end; end = IS_INT(args[1]) ? AS_INT(args[1]) : (int64_t)as_number(args[1]); }
                    NcList *l = nc_list_new();
                    for (int64_t i = start; i < end; i++) nc_list_push(l, NC_INT(i));
                    r = NC_LIST(l);

                /* ── Enterprise: Cryptography ──────────────────── */
                } else if (strcmp(name, "hash_sha256") == 0 && argc == 1) {
                    NcString *s = nc_value_to_string(args[0]);
                    r = nc_stdlib_hash_sha256(s->chars);
                    nc_string_free(s);
                } else if (strcmp(name, "hash_password") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_hash_password(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "verify_password") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_verify_password(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "hash_hmac") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_hash_hmac(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);

                /* ── Enterprise: JWT verify ─────────────────────── */
                } else if (strcmp(name, "jwt_verify") == 0 && argc >= 1 && IS_STRING(args[0])) {
                    const char *token = AS_STRING(args[0])->chars;
                    if (argc >= 2 && IS_STRING(args[1]))
                        nc_setenv("NC_JWT_SECRET", AS_STRING(args[1])->chars, 1);
                    char auth_hdr[8192];
                    snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", token);
                    NcAuthContext ctx = nc_mw_auth_check(auth_hdr);
                    if (!ctx.authenticated) { r = NC_BOOL(false); }
                    else {
                        NcMap *claims = nc_map_new();
                        nc_map_set(claims, nc_string_from_cstr("sub"), NC_STRING(nc_string_from_cstr(ctx.user_id)));
                        nc_map_set(claims, nc_string_from_cstr("role"), NC_STRING(nc_string_from_cstr(ctx.role)));
                        nc_map_set(claims, nc_string_from_cstr("tenant_id"), NC_STRING(nc_string_from_cstr(ctx.tenant_id)));
                        nc_map_set(claims, nc_string_from_cstr("authenticated"), NC_BOOL(true));
                        r = NC_MAP(claims);
                    }

                /* ── Enterprise: Sessions ──────────────────────── */
                } else if (strcmp(name, "session_create") == 0 && argc == 0) {
                    r = nc_stdlib_session_create();
                } else if (strcmp(name, "session_set") == 0 && argc == 3 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_session_set(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars, args[2]);
                } else if (strcmp(name, "session_get") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_session_get(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                } else if (strcmp(name, "session_destroy") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_session_destroy(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "session_exists") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_session_exists(AS_STRING(args[0])->chars);

                /* ── Enterprise: Request Context ───────────────── */
                } else if (strcmp(name, "request_header") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_stdlib_request_header(AS_STRING(args[0])->chars);
                } else if (strcmp(name, "request_ip") == 0 && argc == 0) {
                    NcRequestContext *ctx = nc_request_ctx_get();
                    r = ctx ? NC_STRING(nc_string_from_cstr(ctx->client_ip)) : NC_NONE();
                } else if (strcmp(name, "request_method") == 0 && argc == 0) {
                    NcRequestContext *ctx = nc_request_ctx_get();
                    r = ctx ? NC_STRING(nc_string_from_cstr(ctx->method)) : NC_NONE();
                } else if (strcmp(name, "request_path") == 0 && argc == 0) {
                    NcRequestContext *ctx = nc_request_ctx_get();
                    r = ctx ? NC_STRING(nc_string_from_cstr(ctx->path)) : NC_NONE();
                } else if (strcmp(name, "request_headers") == 0 && argc == 0) {
                    NcRequestContext *ctx = nc_request_ctx_get();
                    if (!ctx) { r = NC_NONE(); }
                    else {
                        NcMap *hdrs = nc_map_new();
                        for (int hi = 0; hi < ctx->header_count; hi++)
                            nc_map_set(hdrs, nc_string_from_cstr(ctx->headers[hi][0]),
                                       NC_STRING(nc_string_from_cstr(ctx->headers[hi][1])));
                        r = NC_MAP(hdrs);
                    }

                /* ── Enterprise: Feature Flags ─────────────────── */
                } else if (strcmp(name, "feature") == 0 && argc >= 1 && IS_STRING(args[0])) {
                    const char *tenant = (argc >= 2 && IS_STRING(args[1])) ? AS_STRING(args[1])->chars : NULL;
                    r = NC_BOOL(nc_ff_is_enabled(AS_STRING(args[0])->chars, tenant));

                /* ── Enterprise: Circuit Breaker ───────────────── */
                } else if (strcmp(name, "circuit_open") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = NC_BOOL(!nc_cb_allow(AS_STRING(args[0])->chars));

                /* ── Enterprise: Higher-order list ops ──────────── */
                } else if (strcmp(name, "sort_by") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_sort_by(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "max_by") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_max_by(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "min_by") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_min_by(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "sum_by") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_sum_by(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "map_field") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_stdlib_list_map_field(AS_LIST(args[0]), AS_STRING(args[1]));
                } else if (strcmp(name, "filter_by") == 0 && argc == 4 && IS_LIST(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2])) {
                    if (IS_STRING(args[3]))
                        r = nc_stdlib_list_filter_by_str(AS_LIST(args[0]), AS_STRING(args[1]),
                            AS_STRING(args[2])->chars, AS_STRING(args[3]));
                    else
                        r = nc_stdlib_list_filter_by(AS_LIST(args[0]), AS_STRING(args[1]),
                            AS_STRING(args[2])->chars, as_number(args[3]));

                /* ── Tensor operations for NC AI ─────────────── */
                } else if (strcmp(name, "tensor_create") == 0 && argc == 2) {
                    r = nc_ncfn_tensor_create((int)as_number(args[0]), (int)as_number(args[1]));
                } else if (strcmp(name, "tensor_ones") == 0 && argc == 2) {
                    r = nc_ncfn_tensor_ones((int)as_number(args[0]), (int)as_number(args[1]));
                } else if (strcmp(name, "tensor_random") == 0 && argc == 2) {
                    r = nc_ncfn_tensor_random((int)as_number(args[0]), (int)as_number(args[1]));
                } else if (strcmp(name, "tensor_matmul") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_matmul(args[0], args[1]);
                } else if (strcmp(name, "tensor_add") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_add(args[0], args[1]);
                } else if (strcmp(name, "tensor_sub") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_sub(args[0], args[1]);
                } else if (strcmp(name, "tensor_mul") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_mul(args[0], args[1]);
                } else if (strcmp(name, "tensor_scale") == 0 && argc == 2 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_scale(args[0], as_number(args[1]));
                } else if (strcmp(name, "tensor_transpose") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_transpose(args[0]);
                } else if (strcmp(name, "tensor_softmax") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_softmax(args[0]);
                } else if (strcmp(name, "tensor_gelu") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_gelu(args[0]);
                } else if (strcmp(name, "tensor_relu") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_relu(args[0]);
                } else if (strcmp(name, "tensor_tanh") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_tanh(args[0]);
                } else if (strcmp(name, "tensor_layer_norm") == 0 && argc == 3 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_layer_norm(args[0], args[1], args[2]);
                } else if (strcmp(name, "tensor_add_bias") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_add_bias(args[0], args[1]);
                } else if (strcmp(name, "tensor_causal_mask") == 0 && argc == 1) {
                    r = nc_ncfn_tensor_causal_mask((int)as_number(args[0]));
                } else if (strcmp(name, "tensor_embedding") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_embedding(args[0], args[1]);
                } else if (strcmp(name, "tensor_shape") == 0 && argc == 1 && IS_LIST(args[0])) {
                    r = nc_ncfn_tensor_shape(args[0]);
                } else if (strcmp(name, "tensor_cross_entropy") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                    r = nc_ncfn_tensor_cross_entropy(args[0], args[1]);
                } else if (strcmp(name, "tensor_save") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                    r = nc_ncfn_tensor_save(args[0], AS_STRING(args[1])->chars);
                } else if (strcmp(name, "tensor_load") == 0 && argc == 1 && IS_STRING(args[0])) {
                    r = nc_ncfn_tensor_load(AS_STRING(args[0])->chars);
                }

                /* If no built-in matched, try user-defined behaviors */
                if (r.type == VAL_NONE && vm->behaviors) {
                    NcString *bname = nc_string_from_cstr(name);
                    NcValue chunk_val = nc_map_get(vm->behaviors, bname);
                    nc_string_free(bname);
                    if (IS_INT(chunk_val)) {
                        int chunk_idx = (int)AS_INT(chunk_val);
                        if (chunk_idx >= 0 && chunk_idx < vm->behavior_chunk_count &&
                            vm->behavior_chunks) {
                            NcChunk *target = &vm->behavior_chunks[chunk_idx];
                            for (int a = 0; a < argc && a < target->var_count; a++) {
                                nc_map_set(vm->globals, target->var_names[a], args[a]);
                            }
                            r = nc_vm_execute(vm, target);
                            nc_value_retain(r);
                        }
                    }
                }

                vm->stack_top -= argc + 1;
                vm_push(vm, r);
            }
            break;
        }

        case OP_CALL: {
            uint8_t argc = read_byte(frame);
            if (argc + 1 > vm->stack_top) {
                vm_error(vm, "Stack underflow in call (need %d, have %d).", argc + 1, vm->stack_top);
                return result;
            }
            NcValue fn_name = vm->stack[vm->stack_top - argc - 1];
            if (IS_STRING(fn_name)) {
                NcString *sname = AS_STRING(fn_name);
                NcValue chunk_val = nc_map_get(vm->behaviors, sname);
                if (IS_INT(chunk_val)) {
                    int chunk_idx = (int)AS_INT(chunk_val);
                    if (chunk_idx >= 0 && chunk_idx < vm->behavior_chunk_count &&
                        vm->behavior_chunks) {
                        NcChunk *target = &vm->behavior_chunks[chunk_idx];

                        /* Pass args as variables matching the behavior's parameter names */
                        for (int a = 0; a < argc && a < target->var_count; a++) {
                            NcValue arg = vm->stack[vm->stack_top - argc + a];
                            nc_map_set(vm->globals, target->var_names[a], arg);
                        }
                        vm->stack_top -= (argc + 1);

                        NcValue call_result = nc_vm_execute(vm, target);
                        nc_value_retain(call_result);
                        vm_push(vm, call_result);
                        break;
                    }
                }
                /* Behavior not found — fall through to native function dispatch.
                 * This allows fn(args) syntax to call built-in tensor_*, len(), etc. */
                {
                    const char *name = sname->chars;
                    NcValue *args = &vm->stack[vm->stack_top - argc];
                    NcValue r = NC_NONE();

                    /* Reuse the OP_CALL_NATIVE dispatch logic for string-named builtins */
                    if (strcmp(name, "len") == 0 && argc == 1) {
                        if (IS_LIST(args[0])) r = NC_INT(AS_LIST(args[0])->count);
                        else if (IS_STRING(args[0])) r = NC_INT(AS_STRING(args[0])->length);
                        else if (IS_MAP(args[0])) r = NC_INT(AS_MAP(args[0])->count);
                    } else if (strcmp(name, "string") == 0 && argc == 1) {
                        r = NC_STRING(nc_value_to_string(args[0]));
                    } else if (strcmp(name, "type") == 0 && argc == 1) {
                        const char *tn = "nothing";
                        switch (args[0].type) {
                            case VAL_INT: tn = "number"; break;
                            case VAL_FLOAT: tn = "number"; break;
                            case VAL_BOOL: tn = "boolean"; break;
                            case VAL_STRING: tn = "string"; break;
                            case VAL_LIST: tn = "list"; break;
                            case VAL_MAP: tn = "map"; break;
                            default: break;
                        }
                        r = NC_STRING(nc_string_from_cstr(tn));
                    /* ── Tensor operations (same as OP_CALL_NATIVE) ── */
                    } else if (strcmp(name, "tensor_create") == 0 && argc == 2) {
                        r = nc_ncfn_tensor_create((int)as_number(args[0]), (int)as_number(args[1]));
                    } else if (strcmp(name, "tensor_ones") == 0 && argc == 2) {
                        r = nc_ncfn_tensor_ones((int)as_number(args[0]), (int)as_number(args[1]));
                    } else if (strcmp(name, "tensor_random") == 0 && argc == 2) {
                        r = nc_ncfn_tensor_random((int)as_number(args[0]), (int)as_number(args[1]));
                    } else if (strcmp(name, "tensor_matmul") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_matmul(args[0], args[1]);
                    } else if (strcmp(name, "tensor_add") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_add(args[0], args[1]);
                    } else if (strcmp(name, "tensor_sub") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_sub(args[0], args[1]);
                    } else if (strcmp(name, "tensor_mul") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_mul(args[0], args[1]);
                    } else if (strcmp(name, "tensor_scale") == 0 && argc == 2 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_scale(args[0], as_number(args[1]));
                    } else if (strcmp(name, "tensor_transpose") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_transpose(args[0]);
                    } else if (strcmp(name, "tensor_softmax") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_softmax(args[0]);
                    } else if (strcmp(name, "tensor_gelu") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_gelu(args[0]);
                    } else if (strcmp(name, "tensor_relu") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_relu(args[0]);
                    } else if (strcmp(name, "tensor_tanh") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_tanh(args[0]);
                    } else if (strcmp(name, "tensor_layer_norm") == 0 && argc == 3 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_layer_norm(args[0], args[1], args[2]);
                    } else if (strcmp(name, "tensor_add_bias") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_add_bias(args[0], args[1]);
                    } else if (strcmp(name, "tensor_causal_mask") == 0 && argc == 1) {
                        r = nc_ncfn_tensor_causal_mask((int)as_number(args[0]));
                    } else if (strcmp(name, "tensor_embedding") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_embedding(args[0], args[1]);
                    } else if (strcmp(name, "tensor_shape") == 0 && argc == 1 && IS_LIST(args[0])) {
                        r = nc_ncfn_tensor_shape(args[0]);
                    } else if (strcmp(name, "tensor_cross_entropy") == 0 && argc == 2 && IS_LIST(args[0]) && IS_LIST(args[1])) {
                        r = nc_ncfn_tensor_cross_entropy(args[0], args[1]);
                    } else if (strcmp(name, "tensor_save") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                        r = nc_ncfn_tensor_save(args[0], AS_STRING(args[1])->chars);
                    } else if (strcmp(name, "tensor_load") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_ncfn_tensor_load(AS_STRING(args[0])->chars);

                    /* ── File I/O ── */
                    } else if (strcmp(name, "read_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_read_file(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "write_file") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_write_file(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                    } else if (strcmp(name, "file_exists") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_file_exists(AS_STRING(args[0])->chars);

                    /* ── String functions ── */
                    } else if (strcmp(name, "upper") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_upper(AS_STRING(args[0]));
                    } else if (strcmp(name, "lower") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_lower(AS_STRING(args[0]));
                    } else if (strcmp(name, "trim") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_trim(AS_STRING(args[0]));
                    } else if (strcmp(name, "split") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_split(AS_STRING(args[0]), AS_STRING(args[1]));
                    } else if (strcmp(name, "join") == 0 && argc == 2 && IS_LIST(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_join(AS_LIST(args[0]), AS_STRING(args[1]));
                    } else if (strcmp(name, "replace") == 0 && argc == 3 && IS_STRING(args[0]) && IS_STRING(args[1]) && IS_STRING(args[2])) {
                        r = nc_stdlib_replace(AS_STRING(args[0]), AS_STRING(args[1]), AS_STRING(args[2]));
                    } else if (strcmp(name, "contains") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_contains(AS_STRING(args[0]), AS_STRING(args[1]));

                    /* ── JSON ── */
                    } else if (strcmp(name, "json_encode") == 0 && argc == 1) {
                        char *j = nc_json_serialize(args[0], false);
                        r = NC_STRING(nc_string_from_cstr(j)); free(j);
                    } else if (strcmp(name, "json_decode") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_json_parse(AS_STRING(args[0])->chars);

                    /* ── Math ── */
                    } else if (strcmp(name, "abs") == 0 && argc == 1) {
                        double val = as_number(args[0]);
                        r = (IS_INT(args[0]) && fabs(val) == (int64_t)fabs(val)) ? NC_INT((int64_t)fabs(val)) : NC_FLOAT(fabs(val));
                    } else if (strcmp(name, "sqrt") == 0 && argc == 1) {
                        r = nc_stdlib_sqrt(as_number(args[0]));
                    } else if (strcmp(name, "random") == 0 && argc == 0) {
                        r = nc_stdlib_random();
                    } else if (strcmp(name, "time_now") == 0 && argc == 0) {
                        r = nc_stdlib_time_now();

                    /* ── Env ── */
                    } else if (strcmp(name, "env") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_env_get(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "platform_system") == 0 && argc == 0) {
                        r = nc_stdlib_platform_system();

                    /* ── CSV ── */
                    } else if (strcmp(name, "csv_parse") == 0 && argc >= 1 && IS_STRING(args[0])) {
                        if (argc >= 2 && IS_STRING(args[1]) && AS_STRING(args[1])->length > 0)
                            r = nc_stdlib_csv_parse_delim(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars[0]);
                        else
                            r = nc_stdlib_csv_parse(AS_STRING(args[0])->chars);

                    /* ── HTTP ── */
                    } else if (strcmp(name, "http_get") == 0 && argc >= 1 && IS_STRING(args[0])) {
                        NcMap *hdrs = argc >= 2 && IS_MAP(args[1]) ? AS_MAP(args[1]) : NULL;
                        r = nc_stdlib_http_request("GET", AS_STRING(args[0])->chars, hdrs, NC_NONE());
                    } else if (strcmp(name, "http_post") == 0 && argc >= 2 && IS_STRING(args[0])) {
                        NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                        r = nc_stdlib_http_request("POST", AS_STRING(args[0])->chars, hdrs, args[1]);
                    } else if (strcmp(name, "http_put") == 0 && argc >= 2 && IS_STRING(args[0])) {
                        NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                        r = nc_stdlib_http_request("PUT", AS_STRING(args[0])->chars, hdrs, args[1]);
                    } else if (strcmp(name, "http_delete") == 0 && argc >= 1 && IS_STRING(args[0])) {
                        NcMap *hdrs = argc >= 2 && IS_MAP(args[1]) ? AS_MAP(args[1]) : NULL;
                        r = nc_stdlib_http_request("DELETE", AS_STRING(args[0])->chars, hdrs, NC_NONE());
                    } else if (strcmp(name, "http_patch") == 0 && argc >= 2 && IS_STRING(args[0])) {
                        NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                        r = nc_stdlib_http_request("PATCH", AS_STRING(args[0])->chars, hdrs, args[1]);
                    } else if (strcmp(name, "http_request") == 0 && argc >= 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        NcMap *hdrs = argc >= 3 && IS_MAP(args[2]) ? AS_MAP(args[2]) : NULL;
                        NcValue body = argc >= 4 ? args[3] : NC_NONE();
                        r = nc_stdlib_http_request(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars, hdrs, body);

                    /* ── OS module ── */
                    } else if (strcmp(name, "os_env") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_env(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_cwd") == 0 && argc == 0) {
                        r = nc_stdlib_os_cwd();
                    } else if (strcmp(name, "os_listdir") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_listdir(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_glob") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_os_glob(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                    } else if (strcmp(name, "os_walk") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_walk(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_exists") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_exists(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_is_dir") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_is_dir(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_is_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_is_file(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_file_size") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_file_size(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_mkdir") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_mkdir_p(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_remove") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_remove(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_read_file") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_read_file(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_write_file") == 0 && argc == 2 && IS_STRING(args[0]) && IS_STRING(args[1])) {
                        r = nc_stdlib_os_write_file(AS_STRING(args[0])->chars, AS_STRING(args[1])->chars);
                    } else if (strcmp(name, "os_exec") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_exec(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_path_join") == 0 && argc >= 1) {
                        NcList *parts = nc_list_new();
                        for (int pi = 0; pi < argc; pi++) nc_list_push(parts, args[pi]);
                        r = nc_stdlib_os_path_join(parts);
                    } else if (strcmp(name, "os_basename") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_basename(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_dirname") == 0 && argc == 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_dirname(AS_STRING(args[0])->chars);
                    } else if (strcmp(name, "os_setenv") == 0 && argc >= 1 && IS_STRING(args[0])) {
                        r = nc_stdlib_os_setenv(AS_STRING(args[0])->chars,
                            argc >= 2 && IS_STRING(args[1]) ? AS_STRING(args[1])->chars : NULL);

                    /* ── Plugin/FFI ── */
                    } else if (strcmp(name, "plugin_load") == 0 && argc == 1 && IS_STRING(args[0])) {
                        int rc = nc_plugin_load(AS_STRING(args[0])->chars);
                        r = NC_BOOL(rc == 0);
                    } else if (strcmp(name, "plugin_call") == 0 && argc >= 1 && IS_STRING(args[0])) {
                        r = nc_plugin_call(AS_STRING(args[0])->chars, argc - 1, &args[1]);
                    } else if (nc_plugin_has(name)) {
                        r = nc_plugin_call(name, argc, args);
                    }

                    vm->stack_top -= (argc + 1);
                    vm_push(vm, r);
                    break;
                }
            }
            vm->stack_top -= (argc + 1);
            vm_push(vm, NC_NONE());
            break;
        }

        case OP_RETURN:
        case OP_HALT:
            vm->frame_count--;
            return result;

        default:
            vm_error(vm, "Unknown opcode: %d", instruction);
            return NC_NONE();
        }
    }

    return result;
}

void nc_vm_free(NcVM *vm) {
    if (!vm) return;
    nc_gc_unregister_vm(vm);
    /* Release remaining stack values (creation refs not consumed by operations) */
    for (int i = 0; i < vm->stack_top; i++)
        nc_value_release(vm->stack[i]);
    vm->stack_top = 0;
    /* Release frame locals */
    for (int f = 0; f < vm->frame_count; f++) {
        for (int i = 0; i < vm->frames[f].local_count; i++)
            nc_value_release(vm->frames[f].locals[i]);
        vm->frames[f].local_count = 0;
    }
    nc_map_free(vm->globals);
    nc_map_free(vm->behaviors);
    for (int i = 0; i < vm->output_count; i++) free(vm->output[i]);
    free(vm->output);
    free(vm);
}
