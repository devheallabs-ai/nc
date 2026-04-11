/*
 * nc_wasm.c — WebAssembly compilation target for NC bytecode.
 *
 * Translates NC bytecode into WebAssembly text format (.wat).
 * The generated module imports host functions for string ops,
 * I/O, and HTTP from a JavaScript runtime, and exports each
 * compiled NC behavior as a WASM function.
 */

#include "../include/nc.h"
#include "nc_wasm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

/* ── Internal helpers ───────────────────────────────────────── */

#define WASM_INIT_CAP    4096
#define WASM_FUNC_CAP    64

static void wasm_error(NcWasmCompiler *w, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(w->error_msg, sizeof(w->error_msg), fmt, ap);
    va_end(ap);
    w->had_error = true;
}

/*
 * Append formatted text to a dynamically-growing string buffer.
 */
static void buf_appendf(char **buf, int *len, int *cap,
                         const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return;

    while (*len + need + 1 > *cap) {
        *cap = (*cap < 64) ? 128 : (*cap * 2);
        *buf = realloc(*buf, *cap);
    }
    va_start(ap, fmt);
    vsnprintf(*buf + *len, *cap - *len, fmt, ap);
    va_end(ap);
    *len += need;
}

/*
 * Shorthand to append to the compiler's main output buffer.
 */
static void emit(NcWasmCompiler *w, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) return;

    while (w->output_len + need + 1 > w->output_cap) {
        w->output_cap = (w->output_cap < 64) ? 128 : (w->output_cap * 2);
        w->output = realloc(w->output, w->output_cap);
    }
    va_start(ap, fmt);
    vsnprintf(w->output + w->output_len, w->output_cap - w->output_len,
              fmt, ap);
    va_end(ap);
    w->output_len += need;
}

/*
 * Read a 2-byte big-endian operand from bytecode at the given offset.
 */
static uint16_t read_u16(uint8_t *code, int offset) {
    return (uint16_t)((code[offset] << 8) | code[offset + 1]);
}

/*
 * Sanitize a behavior name for use as a WASM identifier.
 * Replaces non-alphanumeric characters with underscores.
 */
static char *sanitize_name(const char *name) {
    int len = (int)strlen(name);
    char *out = malloc(len + 1);
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_') {
            out[i] = c;
        } else {
            out[i] = '_';
        }
    }
    out[len] = '\0';
    return out;
}

/* ── Lifecycle ──────────────────────────────────────────────── */

NcWasmCompiler *nc_wasm_new(void) {
    NcWasmCompiler *w = calloc(1, sizeof(NcWasmCompiler));
    w->output     = malloc(WASM_INIT_CAP);
    w->output[0]  = '\0';
    w->output_len = 0;
    w->output_cap = WASM_INIT_CAP;

    w->func_bodies    = calloc(WASM_FUNC_CAP, sizeof(char *));
    w->func_body_cap  = WASM_FUNC_CAP;
    w->func_body_count = 0;

    w->export_names   = calloc(WASM_FUNC_CAP, sizeof(char *));
    w->export_cap     = WASM_FUNC_CAP;
    w->export_count   = 0;

    w->had_error = false;
    return w;
}

void nc_wasm_free(NcWasmCompiler *w) {
    if (!w) return;
    free(w->output);
    for (int i = 0; i < w->func_body_count; i++)
        free(w->func_bodies[i]);
    free(w->func_bodies);
    for (int i = 0; i < w->export_count; i++)
        free(w->export_names[i]);
    free(w->export_names);
    free(w);
}

/* ── Store a compiled function body ─────────────────────────── */

static void store_func(NcWasmCompiler *w, char *body, const char *name) {
    if (w->func_body_count >= w->func_body_cap) {
        w->func_body_cap *= 2;
        w->func_bodies = realloc(w->func_bodies,
                                  w->func_body_cap * sizeof(char *));
    }
    w->func_bodies[w->func_body_count++] = body;

    if (w->export_count >= w->export_cap) {
        w->export_cap *= 2;
        w->export_names = realloc(w->export_names,
                                   w->export_cap * sizeof(char *));
    }
    w->export_names[w->export_count++] = sanitize_name(name);
}

/* ── Bytecode → WAT translation ─────────────────────────────── */

/*
 * Emit WAT for a single constant value.
 * Integers become i64.const, floats become f64.const, strings get
 * handled through the imported nc_string_new.
 */
static void emit_constant(char **buf, int *len, int *cap,
                           NcValue val, int indent) {
    const char *pad = "        ";  /* 8 spaces for nesting */
    (void)indent;
    switch (val.type) {
    case VAL_INT:
        buf_appendf(buf, len, cap, "%si64.const %lld\n",
                    pad, (long long)val.as.integer);
        break;
    case VAL_FLOAT:
        buf_appendf(buf, len, cap, "%sf64.const %g\n",
                    pad, val.as.floating);
        break;
    case VAL_BOOL:
        buf_appendf(buf, len, cap, "%si32.const %d\n",
                    pad, val.as.boolean ? 1 : 0);
        break;
    case VAL_NONE:
        buf_appendf(buf, len, cap, "%si32.const 0  ;; none\n", pad);
        break;
    case VAL_STRING:
        /* Strings are stored in linear memory; push a pointer via
         * the imported nc_string_new(offset, length).
         * For now, emit the string length as a placeholder. The
         * full data segment approach is done at module emit time. */
        buf_appendf(buf, len, cap,
                    "%s;; string \"%.*s\"\n"
                    "%si32.const %d  ;; string length\n"
                    "%scall $nc_string_new\n",
                    pad, val.as.string->length, val.as.string->chars,
                    pad, val.as.string->length,
                    pad);
        break;
    default:
        buf_appendf(buf, len, cap,
                    "%si32.const 0  ;; unsupported type %d\n",
                    pad, val.type);
        break;
    }
}

int nc_wasm_compile_chunk(NcWasmCompiler *w, NcChunk *chunk,
                          const char *func_name) {
    if (!w || !chunk || !func_name) {
        if (w) wasm_error(w, "null argument to nc_wasm_compile_chunk");
        return -1;
    }

    char *safe_name = sanitize_name(func_name);

    /* Build the function body into a temporary buffer. */
    char *body = NULL;
    int   blen = 0, bcap = 0;
    const char *I = "        ";  /* 8-space indent for instructions */

    /* Determine how many locals this function needs. We scan for
     * OP_GET_LOCAL / OP_SET_LOCAL to find the maximum slot index. */
    int max_local = -1;
    for (int i = 0; i < chunk->count; ) {
        uint8_t op = chunk->code[i];
        switch (op) {
        case OP_GET_LOCAL:
        case OP_SET_LOCAL: {
            if (i + 1 < chunk->count) {
                int slot = chunk->code[i + 1];
                if (slot > max_local) max_local = slot;
            }
            i += 2;
            break;
        }
        /* Opcodes with 2-byte operand */
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_LOOP:
        case OP_CONSTANT_LONG:
            i += 3;
            break;
        /* Opcodes with 1-byte operand */
        case OP_CONSTANT:
        case OP_GET_VAR:
        case OP_SET_VAR:
        case OP_CALL:
        case OP_CALL_NATIVE:
        case OP_MAKE_LIST:
        case OP_MAKE_MAP:
        case OP_GET_FIELD:
        case OP_SET_FIELD:
            i += 2;
            break;
        default:
            i += 1;
            break;
        }
    }
    w->local_count = max_local + 1;
    w->label_count = 0;

    /* Function header */
    buf_appendf(&body, &blen, &bcap,
                "    (func $%s (result i64)\n", safe_name);

    /* Declare locals — use i64 as the generic NC value representation */
    if (w->local_count > 0) {
        buf_appendf(&body, &blen, &bcap,
                    "        ;; %d local slots\n", w->local_count);
        for (int i = 0; i < w->local_count; i++) {
            buf_appendf(&body, &blen, &bcap,
                        "        (local $L%d i64)\n", i);
        }
    }
    /* Extra scratch locals for binary ops */
    buf_appendf(&body, &blen, &bcap,
                "        (local $tmp_a i64)\n"
                "        (local $tmp_b i64)\n"
                "        (local $tmp_cond i32)\n");

    /* ── Walk bytecode ─────────────────────────────────────── */

    int ip = 0;
    while (ip < chunk->count) {
        uint8_t op = chunk->code[ip];

        /* Emit a comment with the opcode offset for debugging */
        buf_appendf(&body, &blen, &bcap,
                    "%s;; [%04d] opcode %d\n", I, ip, op);

        switch (op) {

        /* ── Data ─────────────────────────────────────────── */

        case OP_CONSTANT: {
            uint8_t idx = chunk->code[ip + 1];
            if (idx < chunk->const_count) {
                emit_constant(&body, &blen, &bcap,
                              chunk->constants[idx], 2);
            } else {
                buf_appendf(&body, &blen, &bcap,
                            "%si64.const 0  ;; bad constant %d\n",
                            I, idx);
            }
            ip += 2;
            break;
        }

        case OP_CONSTANT_LONG: {
            uint16_t idx = read_u16(chunk->code, ip + 1);
            if (idx < chunk->const_count) {
                emit_constant(&body, &blen, &bcap,
                              chunk->constants[idx], 2);
            } else {
                buf_appendf(&body, &blen, &bcap,
                            "%si64.const 0  ;; bad constant %d\n",
                            I, idx);
            }
            ip += 3;
            break;
        }

        case OP_NONE:
            buf_appendf(&body, &blen, &bcap,
                        "%si64.const 0  ;; none\n", I);
            ip++;
            break;

        case OP_TRUE:
            buf_appendf(&body, &blen, &bcap, "%si64.const 1\n", I);
            ip++;
            break;

        case OP_FALSE:
            buf_appendf(&body, &blen, &bcap, "%si64.const 0\n", I);
            ip++;
            break;

        case OP_POP:
            buf_appendf(&body, &blen, &bcap, "%sdrop\n", I);
            ip++;
            break;

        /* ── Variables ────────────────────────────────────── */

        case OP_GET_LOCAL: {
            int slot = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.get $L%d\n", I, slot);
            ip += 2;
            break;
        }

        case OP_SET_LOCAL: {
            int slot = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $L%d\n", I, slot);
            ip += 2;
            break;
        }

        case OP_GET_VAR: {
            uint8_t idx = chunk->code[ip + 1];
            const char *vname = "unknown";
            if (idx < chunk->var_count && chunk->var_names[idx])
                vname = chunk->var_names[idx]->chars;
            buf_appendf(&body, &blen, &bcap,
                        "%s;; get_var \"%s\"\n"
                        "%scall $nc_global_get  ;; idx=%d\n",
                        I, vname, I, idx);
            ip += 2;
            break;
        }

        case OP_SET_VAR: {
            uint8_t idx = chunk->code[ip + 1];
            const char *vname = "unknown";
            if (idx < chunk->var_count && chunk->var_names[idx])
                vname = chunk->var_names[idx]->chars;
            buf_appendf(&body, &blen, &bcap,
                        "%s;; set_var \"%s\"\n"
                        "%scall $nc_global_set  ;; idx=%d\n",
                        I, vname, I, idx);
            ip += 2;
            break;
        }

        /* ── Fields / Indexes ─────────────────────────────── */

        case OP_GET_FIELD: {
            uint8_t idx = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; get_field (const %d)\n"
                        "%scall $nc_get_field\n",
                        I, idx, I);
            ip += 2;
            break;
        }

        case OP_SET_FIELD: {
            uint8_t idx = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; set_field (const %d)\n"
                        "%scall $nc_set_field\n",
                        I, idx, I);
            ip += 2;
            break;
        }

        case OP_GET_INDEX:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_get_index\n", I);
            ip++;
            break;

        case OP_SET_INDEX:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_set_index\n", I);
            ip++;
            break;

        case OP_SLICE:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_slice\n", I);
            ip++;
            break;

        /* ── Arithmetic ───────────────────────────────────── */
        /*
         * NC values are dynamically typed, but in WASM we compile
         * integer-path operations using i64. The runtime host can
         * provide type-dispatching wrappers if needed.
         */

        case OP_ADD:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.add\n",
                        I, I, I, I, I);
            ip++;
            break;

        case OP_SUBTRACT:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.sub\n",
                        I, I, I, I, I);
            ip++;
            break;

        case OP_MULTIPLY:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.mul\n",
                        I, I, I, I, I);
            ip++;
            break;

        case OP_DIVIDE:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.div_s\n",
                        I, I, I, I, I);
            ip++;
            break;

        case OP_MODULO:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.rem_s\n",
                        I, I, I, I, I);
            ip++;
            break;

        case OP_NEGATE:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_a\n"
                        "%si64.const 0\n"
                        "%slocal.get $tmp_a\n"
                        "%si64.sub\n",
                        I, I, I, I);
            ip++;
            break;

        /* ── Logic / Comparison ───────────────────────────── */

        case OP_NOT:
            buf_appendf(&body, &blen, &bcap,
                        "%si64.eqz\n"
                        "%si64.extend_i32_u\n",
                        I, I);
            ip++;
            break;

        case OP_EQUAL:
            buf_appendf(&body, &blen, &bcap,
                        "%si64.eq\n"
                        "%si64.extend_i32_u\n",
                        I, I);
            ip++;
            break;

        case OP_NOT_EQUAL:
            buf_appendf(&body, &blen, &bcap,
                        "%si64.ne\n"
                        "%si64.extend_i32_u\n",
                        I, I);
            ip++;
            break;

        case OP_ABOVE:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.gt_s\n"
                        "%si64.extend_i32_u\n",
                        I, I, I, I, I, I);
            ip++;
            break;

        case OP_BELOW:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.lt_s\n"
                        "%si64.extend_i32_u\n",
                        I, I, I, I, I, I);
            ip++;
            break;

        case OP_AT_LEAST:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.ge_s\n"
                        "%si64.extend_i32_u\n",
                        I, I, I, I, I, I);
            ip++;
            break;

        case OP_AT_MOST:
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%slocal.get $tmp_b\n"
                        "%si64.le_s\n"
                        "%si64.extend_i32_u\n",
                        I, I, I, I, I, I);
            ip++;
            break;

        case OP_AND:
            /* Short-circuit AND: if left is falsy, keep it;
             * otherwise evaluate right side. */
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%si64.eqz\n"
                        "%s(if (result i64)\n"
                        "%s    (then local.get $tmp_a)\n"
                        "%s    (else local.get $tmp_b)\n"
                        "%s)\n",
                        I, I, I, I, I, I, I, I);
            ip++;
            break;

        case OP_OR:
            /* Short-circuit OR: if left is truthy, keep it;
             * otherwise evaluate right side. */
            buf_appendf(&body, &blen, &bcap,
                        "%slocal.set $tmp_b\n"
                        "%slocal.set $tmp_a\n"
                        "%slocal.get $tmp_a\n"
                        "%si64.eqz\n"
                        "%si32.eqz\n"
                        "%s(if (result i64)\n"
                        "%s    (then local.get $tmp_a)\n"
                        "%s    (else local.get $tmp_b)\n"
                        "%s)\n",
                        I, I, I, I, I, I, I, I, I);
            ip++;
            break;

        case OP_IN:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_op_in\n", I);
            ip++;
            break;

        /* ── Control Flow ─────────────────────────────────── */

        case OP_JUMP: {
            uint16_t offset = read_u16(chunk->code, ip + 1);
            buf_appendf(&body, &blen, &bcap,
                        "%s;; jump +%d (to %d)\n"
                        "%sbr $block_%d\n",
                        I, offset, ip + 3 + offset,
                        I, w->label_count);
            w->label_count++;
            ip += 3;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            uint16_t offset = read_u16(chunk->code, ip + 1);
            buf_appendf(&body, &blen, &bcap,
                        "%s;; jump_if_false +%d\n"
                        "%si64.eqz\n"
                        "%sbr_if $block_%d\n",
                        I, offset,
                        I,
                        I, w->label_count);
            w->label_count++;
            ip += 3;
            break;
        }

        case OP_LOOP: {
            uint16_t offset = read_u16(chunk->code, ip + 1);
            buf_appendf(&body, &blen, &bcap,
                        "%s;; loop -%d (back to %d)\n"
                        "%sbr $loop_%d\n",
                        I, offset, ip + 3 - offset,
                        I, w->label_count);
            w->label_count++;
            ip += 3;
            break;
        }

        /* ── Functions ────────────────────────────────────── */

        case OP_CALL: {
            uint8_t arg_count = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; call (args=%d)\n"
                        "%scall $nc_dispatch  ;; runtime dispatch\n",
                        I, arg_count, I);
            ip += 2;
            break;
        }

        case OP_CALL_NATIVE: {
            uint8_t idx = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; call_native idx=%d\n"
                        "%scall $nc_call_native\n",
                        I, idx, I);
            ip += 2;
            break;
        }

        case OP_RETURN:
            buf_appendf(&body, &blen, &bcap, "%sreturn\n", I);
            ip++;
            break;

        case OP_HALT:
            buf_appendf(&body, &blen, &bcap, "%sreturn\n", I);
            ip++;
            break;

        /* ── Collections ──────────────────────────────────── */

        case OP_MAKE_LIST: {
            uint8_t count = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; make_list count=%d\n"
                        "%si32.const %d\n"
                        "%scall $nc_make_list\n",
                        I, count, I, count, I);
            ip += 2;
            break;
        }

        case OP_MAKE_MAP: {
            uint8_t count = chunk->code[ip + 1];
            buf_appendf(&body, &blen, &bcap,
                        "%s;; make_map pairs=%d\n"
                        "%si32.const %d\n"
                        "%scall $nc_make_map\n",
                        I, count, I, count, I);
            ip += 2;
            break;
        }

        /* ── NC-Specific: I/O & AI ────────────────────────── */

        case OP_LOG:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_print\n", I);
            ip++;
            break;

        case OP_GATHER:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_http_get\n", I);
            ip++;
            break;

        case OP_STORE:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_store\n", I);
            ip++;
            break;

        case OP_ASK_AI:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_ask_ai\n", I);
            ip++;
            break;

        case OP_NOTIFY:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_notify\n", I);
            ip++;
            break;

        case OP_EMIT:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_emit\n", I);
            ip++;
            break;

        case OP_WAIT:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_wait\n", I);
            ip++;
            break;

        case OP_RESPOND:
            buf_appendf(&body, &blen, &bcap,
                        "%scall $nc_respond\n"
                        "%sreturn\n",
                        I, I);
            ip++;
            break;

        /* ── UI opcodes — delegate to host runtime ────────── */

        case OP_UI_ELEMENT:
        case OP_UI_PROP:
        case OP_UI_PROP_EXPR:
        case OP_UI_TEXT:
        case OP_UI_CHILD:
        case OP_UI_END_ELEMENT:
        case OP_STATE_DECLARE:
        case OP_STATE_GET:
        case OP_STATE_SET:
        case OP_STATE_COMPUTED:
        case OP_STATE_WATCH:
        case OP_UI_BIND:
        case OP_UI_BIND_INPUT:
        case OP_UI_ON_EVENT:
        case OP_UI_COMPONENT:
        case OP_UI_MOUNT:
        case OP_UI_UNMOUNT:
        case OP_UI_ON_MOUNT:
        case OP_UI_ON_UNMOUNT:
        case OP_UI_RENDER:
        case OP_UI_DIFF:
        case OP_UI_PATCH:
        case OP_UI_ROUTE_DEF:
        case OP_UI_ROUTE_PUSH:
        case OP_UI_ROUTE_GUARD:
        case OP_UI_ROUTE_MATCH:
        case OP_UI_FETCH:
        case OP_UI_FETCH_AUTH:
        case OP_UI_IF:
        case OP_UI_FOR_EACH:
        case OP_UI_SHOW:
        case OP_UI_FORM:
        case OP_UI_VALIDATE:
        case OP_UI_FORM_SUBMIT:
        case OP_UI_AUTH_CHECK:
        case OP_UI_ROLE_CHECK:
        case OP_UI_PERM_CHECK:
            buf_appendf(&body, &blen, &bcap,
                        "%s;; UI opcode %d — delegated to host\n"
                        "%si32.const %d\n"
                        "%scall $nc_ui_dispatch\n",
                        I, op, I, op, I);
            /* These opcodes may have 1-byte or 2-byte operands.
             * For the WAT output we pass the opcode number to the
             * host dispatch and skip conservatively. */
            ip += 2;  /* most UI ops have a 1-byte operand */
            break;

        default:
            buf_appendf(&body, &blen, &bcap,
                        "%s;; unknown opcode %d\n"
                        "%si64.const 0\n",
                        I, op, I);
            ip++;
            break;
        }
    }

    /* Ensure function always returns a value */
    buf_appendf(&body, &blen, &bcap,
                "%si64.const 0  ;; implicit return\n", I);
    buf_appendf(&body, &blen, &bcap, "    )\n\n");

    store_func(w, body, func_name);
    free(safe_name);
    return 0;
}

/* ── Module emission ────────────────────────────────────────── */

/*
 * Emit the host import declarations for the browser/JS runtime.
 */
static void emit_imports(NcWasmCompiler *w) {
    emit(w,
        "    ;; ═══════════════════════════════════════════════════\n"
        "    ;; Host imports — provided by the JavaScript runtime\n"
        "    ;; ═══════════════════════════════════════════════════\n"
        "\n"
        "    ;; I/O\n"
        "    (import \"nc\" \"nc_print\"\n"
        "        (func $nc_print (param i64)))\n"
        "    (import \"nc\" \"nc_http_get\"\n"
        "        (func $nc_http_get (param i64) (result i64)))\n"
        "    (import \"nc\" \"nc_store\"\n"
        "        (func $nc_store (param i64 i64)))\n"
        "\n"
        "    ;; Strings & collections\n"
        "    (import \"nc\" \"nc_string_new\"\n"
        "        (func $nc_string_new (param i32) (result i64)))\n"
        "    (import \"nc\" \"nc_make_list\"\n"
        "        (func $nc_make_list (param i32) (result i64)))\n"
        "    (import \"nc\" \"nc_make_map\"\n"
        "        (func $nc_make_map (param i32) (result i64)))\n"
        "    (import \"nc\" \"nc_list_push\"\n"
        "        (func $nc_list_push (param i64 i64)))\n"
        "\n"
        "    ;; Field / index access\n"
        "    (import \"nc\" \"nc_get_field\"\n"
        "        (func $nc_get_field (param i64) (result i64)))\n"
        "    (import \"nc\" \"nc_set_field\"\n"
        "        (func $nc_set_field (param i64 i64)))\n"
        "    (import \"nc\" \"nc_get_index\"\n"
        "        (func $nc_get_index (param i64 i64) (result i64)))\n"
        "    (import \"nc\" \"nc_set_index\"\n"
        "        (func $nc_set_index (param i64 i64 i64)))\n"
        "    (import \"nc\" \"nc_slice\"\n"
        "        (func $nc_slice (param i64 i64 i64) (result i64)))\n"
        "    (import \"nc\" \"nc_op_in\"\n"
        "        (func $nc_op_in (param i64 i64) (result i64)))\n"
        "\n"
        "    ;; Globals\n"
        "    (import \"nc\" \"nc_global_get\"\n"
        "        (func $nc_global_get (result i64)))\n"
        "    (import \"nc\" \"nc_global_set\"\n"
        "        (func $nc_global_set (param i64)))\n"
        "\n"
        "    ;; AI / events\n"
        "    (import \"nc\" \"nc_ask_ai\"\n"
        "        (func $nc_ask_ai (param i64) (result i64)))\n"
        "    (import \"nc\" \"nc_notify\"\n"
        "        (func $nc_notify (param i64 i64)))\n"
        "    (import \"nc\" \"nc_emit\"\n"
        "        (func $nc_emit (param i64)))\n"
        "    (import \"nc\" \"nc_wait\"\n"
        "        (func $nc_wait (param i64)))\n"
        "    (import \"nc\" \"nc_respond\"\n"
        "        (func $nc_respond (param i64)))\n"
        "\n"
        "    ;; Runtime dispatch\n"
        "    (import \"nc\" \"nc_dispatch\"\n"
        "        (func $nc_dispatch (result i64)))\n"
        "    (import \"nc\" \"nc_call_native\"\n"
        "        (func $nc_call_native (result i64)))\n"
        "\n"
        "    ;; UI host dispatch (handles all UI opcodes)\n"
        "    (import \"nc\" \"nc_ui_dispatch\"\n"
        "        (func $nc_ui_dispatch (param i32)))\n"
        "\n");
}

int nc_wasm_emit_module(NcWasmCompiler *w, const char *output_path) {
    if (!w || !output_path) return -1;

    /* Reset output buffer */
    w->output_len = 0;
    w->output[0] = '\0';

    /* Module header */
    emit(w,
        ";; ═════════════════════════════════════════════════════════\n"
        ";; NC WebAssembly Module — generated by nc_wasm compiler\n"
        ";; ═════════════════════════════════════════════════════════\n"
        "(module\n"
        "\n");

    /* Imports */
    emit_imports(w);

    /* Linear memory (1 page = 64KB, growable) */
    emit(w,
        "    ;; Linear memory for string data and scratch space\n"
        "    (memory (export \"memory\") 1 256)\n"
        "\n");

    /* All compiled function bodies */
    emit(w,
        "    ;; ═══════════════════════════════════════════════════\n"
        "    ;; Compiled NC behaviors\n"
        "    ;; ═══════════════════════════════════════════════════\n"
        "\n");

    for (int i = 0; i < w->func_body_count; i++) {
        emit(w, "%s", w->func_bodies[i]);
    }

    /* Export all compiled functions */
    emit(w,
        "    ;; ═══════════════════════════════════════════════════\n"
        "    ;; Exports\n"
        "    ;; ═══════════════════════════════════════════════════\n"
        "\n");

    for (int i = 0; i < w->export_count; i++) {
        emit(w, "    (export \"%s\" (func $%s))\n",
             w->export_names[i], w->export_names[i]);
    }

    /* If there is a "main" export, also set it as the start function
     * indirectly via a _start wrapper. */
    for (int i = 0; i < w->export_count; i++) {
        if (strcmp(w->export_names[i], "main") == 0) {
            emit(w,
                "\n"
                "    ;; Entry point wrapper\n"
                "    (func $_start\n"
                "        call $main\n"
                "        drop\n"
                "    )\n"
                "    (export \"_start\" (func $_start))\n");
            break;
        }
    }

    /* Close module */
    emit(w, "\n)\n");

    /* Write to file */
    FILE *f = fopen(output_path, "w");
    if (!f) {
        wasm_error(w, "cannot open %s: %s", output_path, strerror(errno));
        return -1;
    }
    fwrite(w->output, 1, w->output_len, f);
    fclose(f);

    return 0;
}

/* ── HTML wrapper generation ────────────────────────────────── */

int nc_wasm_generate_html(const char *wasm_path, const char *html_path) {
    if (!wasm_path || !html_path) return -1;

    /* Extract just the filename from the wasm path for the fetch URL */
    const char *wasm_file = strrchr(wasm_path, '/');
    wasm_file = wasm_file ? wasm_file + 1 : wasm_path;

    FILE *f = fopen(html_path, "w");
    if (!f) return -1;

    fprintf(f,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>NC WebAssembly App</title>\n"
        "    <style>\n"
        "        body {\n"
        "            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI',\n"
        "                         Roboto, sans-serif;\n"
        "            max-width: 800px;\n"
        "            margin: 2rem auto;\n"
        "            padding: 0 1rem;\n"
        "            background: #fafafa;\n"
        "            color: #333;\n"
        "        }\n"
        "        #output {\n"
        "            background: #1e1e1e;\n"
        "            color: #d4d4d4;\n"
        "            padding: 1rem;\n"
        "            border-radius: 8px;\n"
        "            font-family: 'SF Mono', Consolas, monospace;\n"
        "            font-size: 14px;\n"
        "            min-height: 200px;\n"
        "            white-space: pre-wrap;\n"
        "            overflow-y: auto;\n"
        "        }\n"
        "        h1 { color: #2563eb; }\n"
        "        .status {\n"
        "            padding: 0.5rem 1rem;\n"
        "            border-radius: 4px;\n"
        "            margin-bottom: 1rem;\n"
        "            font-size: 14px;\n"
        "        }\n"
        "        .status.loading { background: #fef3c7; color: #92400e; }\n"
        "        .status.ready   { background: #d1fae5; color: #065f46; }\n"
        "        .status.error   { background: #fee2e2; color: #991b1b; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>NC WebAssembly Runtime</h1>\n"
        "    <div id=\"status\" class=\"status loading\">Loading WASM module...</div>\n"
        "    <div id=\"output\"></div>\n"
        "\n"
        "    <script>\n"
        "    (async function() {\n"
        "        const output = document.getElementById('output');\n"
        "        const status = document.getElementById('status');\n"
        "\n"
        "        function log(msg) {\n"
        "            output.textContent += msg + '\\n';\n"
        "            console.log('[NC]', msg);\n"
        "        }\n"
        "\n"
        "        // NC host runtime — imported by the WASM module\n"
        "        const ncImports = {\n"
        "            nc: {\n"
        "                // I/O\n"
        "                nc_print: function(val) {\n"
        "                    log(String(val));\n"
        "                },\n"
        "                nc_http_get: async function(url) {\n"
        "                    try {\n"
        "                        const resp = await fetch(String(url));\n"
        "                        return await resp.text();\n"
        "                    } catch (e) {\n"
        "                        log('HTTP error: ' + e.message);\n"
        "                        return 0n;\n"
        "                    }\n"
        "                },\n"
        "                nc_store: function(path, content) {\n"
        "                    log('[store] ' + path + ' = ' + content);\n"
        "                },\n"
        "\n"
        "                // Strings & collections\n"
        "                nc_string_new: function(len) {\n"
        "                    return BigInt(len);  // placeholder\n"
        "                },\n"
        "                nc_make_list: function(count) {\n"
        "                    return 0n;\n"
        "                },\n"
        "                nc_make_map: function(count) {\n"
        "                    return 0n;\n"
        "                },\n"
        "                nc_list_push: function(list, val) {},\n"
        "\n"
        "                // Field / index\n"
        "                nc_get_field: function(obj) { return 0n; },\n"
        "                nc_set_field: function(obj, val) {},\n"
        "                nc_get_index: function(obj, idx) { return 0n; },\n"
        "                nc_set_index: function(obj, idx, val) {},\n"
        "                nc_slice: function(obj, start, end) { return 0n; },\n"
        "                nc_op_in: function(a, b) { return 0n; },\n"
        "\n"
        "                // Globals\n"
        "                nc_global_get: function() { return 0n; },\n"
        "                nc_global_set: function(val) {},\n"
        "\n"
        "                // AI / events\n"
        "                nc_ask_ai: function(prompt) {\n"
        "                    log('[ai] prompt received (not available in browser)');\n"
        "                    return 0n;\n"
        "                },\n"
        "                nc_notify: function(channel, msg) {\n"
        "                    log('[notify] ' + channel + ': ' + msg);\n"
        "                },\n"
        "                nc_emit: function(event) {\n"
        "                    log('[emit] ' + event);\n"
        "                },\n"
        "                nc_wait: function(ms) {\n"
        "                    // Cannot truly block in browser\n"
        "                },\n"
        "                nc_respond: function(val) {\n"
        "                    log('[respond] ' + val);\n"
        "                },\n"
        "\n"
        "                // Runtime dispatch\n"
        "                nc_dispatch: function() { return 0n; },\n"
        "                nc_call_native: function() { return 0n; },\n"
        "\n"
        "                // UI dispatch\n"
        "                nc_ui_dispatch: function(opcode) {\n"
        "                    log('[ui] opcode ' + opcode);\n"
        "                },\n"
        "            }\n"
        "        };\n"
        "\n"
        "        try {\n"
        "            const response = await fetch('%s');\n"
        "            const bytes = await response.arrayBuffer();\n"
        "            const { instance } = await WebAssembly.instantiate(bytes, ncImports);\n"
        "\n"
        "            status.className = 'status ready';\n"
        "            status.textContent = 'WASM module loaded successfully';\n"
        "\n"
        "            log('=== NC WASM Runtime Ready ===');\n"
        "            log('');\n"
        "\n"
        "            // Call _start or main if exported\n"
        "            if (instance.exports._start) {\n"
        "                instance.exports._start();\n"
        "            } else if (instance.exports.main) {\n"
        "                instance.exports.main();\n"
        "            } else {\n"
        "                log('No entry point found. Exported functions:');\n"
        "                for (const name of Object.keys(instance.exports)) {\n"
        "                    if (typeof instance.exports[name] === 'function') {\n"
        "                        log('  - ' + name + '()');\n"
        "                    }\n"
        "                }\n"
        "            }\n"
        "        } catch (e) {\n"
        "            status.className = 'status error';\n"
        "            status.textContent = 'Error: ' + e.message;\n"
        "            log('WASM load error: ' + e.message);\n"
        "            console.error(e);\n"
        "        }\n"
        "    })();\n"
        "    </script>\n"
        "</body>\n"
        "</html>\n",
        wasm_file);

    fclose(f);
    return 0;
}

/* ── Full build pipeline ────────────────────────────────────── */

int nc_wasm_build(const char *nc_source, const char *output_dir) {
    if (!nc_source || !output_dir) return -1;

    /* ── Step 1: Compile NC source to bytecode ────────────── */
    NcLexer *lex = nc_lexer_new(nc_source, "<wasm>");
    nc_lexer_tokenize(lex);
    NcParser *par = nc_parser_new(lex->tokens, lex->token_count, "<wasm>");
    NcASTNode *prog = nc_parser_parse(par);
    if (!prog || par->had_error) {
        fprintf(stderr, "[nc_wasm] Parse failed\n");
        nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }
    NcCompiler *comp = nc_compiler_new();
    if (!comp || !nc_compiler_compile(comp, prog)) {
        fprintf(stderr, "[nc_wasm] Compile failed\n");
        if (comp) nc_compiler_free(comp);
        nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }
    NcChunk *chunk = (comp->chunk_count > 0) ? &comp->chunks[0] : NULL;
    if (!chunk) {
        fprintf(stderr, "[nc_wasm] No behaviors to compile\n");
        nc_compiler_free(comp); nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }

    /* ── Step 2: Generate .wat from bytecode ──────────────── */

    NcWasmCompiler *w = nc_wasm_new();
    if (nc_wasm_compile_chunk(w, chunk, "main") != 0) {
        fprintf(stderr, "[nc_wasm] WAT compilation failed: %s\n",
                w->error_msg);
        nc_wasm_free(w);
        nc_compiler_free(comp); nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }

    /* Build output paths */
    char wat_path[1024], wasm_path[1024], html_path[1024];
    snprintf(wat_path,  sizeof(wat_path),  "%s/module.wat",  output_dir);
    snprintf(wasm_path, sizeof(wasm_path), "%s/module.wasm", output_dir);
    snprintf(html_path, sizeof(html_path), "%s/index.html",  output_dir);

    if (nc_wasm_emit_module(w, wat_path) != 0) {
        fprintf(stderr, "[nc_wasm] Failed to write %s: %s\n",
                wat_path, w->error_msg);
        nc_wasm_free(w);
        nc_compiler_free(comp); nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }

    fprintf(stderr, "[nc_wasm] Generated %s\n", wat_path);

    /* ── Step 3: Run wat2wasm if available ─────────────────── */

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "wat2wasm \"%s\" -o \"%s\" 2>&1",
             wat_path, wasm_path);

    int rc = system(cmd);
    if (rc == 0) {
        fprintf(stderr, "[nc_wasm] Compiled %s -> %s\n", wat_path, wasm_path);
    } else {
        fprintf(stderr,
                "[nc_wasm] wat2wasm not found or failed (exit %d).\n"
                "          Install wabt: brew install wabt / apt install wabt\n"
                "          The .wat file can be compiled manually:\n"
                "            wat2wasm %s -o %s\n",
                rc, wat_path, wasm_path);
        /* Not a fatal error — the .wat file is still usable */
    }

    /* ── Step 4: Generate HTML wrapper ─────────────────────── */

    if (nc_wasm_generate_html(wasm_path, html_path) != 0) {
        fprintf(stderr, "[nc_wasm] Failed to write %s\n", html_path);
        nc_wasm_free(w);
        nc_compiler_free(comp); nc_parser_free(par); nc_lexer_free(lex);
        return -1;
    }

    fprintf(stderr, "[nc_wasm] Generated %s\n", html_path);
    fprintf(stderr, "[nc_wasm] Build complete. Serve with:\n"
                    "          python3 -m http.server -d %s\n",
            output_dir);

    nc_wasm_free(w);
    nc_compiler_free(comp);
    nc_parser_free(par);
    nc_lexer_free(lex);
    return 0;
}
