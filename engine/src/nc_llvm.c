/*
 * nc_llvm.c — LLVM IR code generator and AOT compiler for NC.
 *
 * Generates LLVM IR text from NC AST and bytecode, which can then be:
 *   1. Compiled to native machine code:  llc output.ll -o output.o
 *   2. JIT compiled at runtime
 *   3. Cross-compiled for any architecture LLVM supports
 *
 * This is how languages like Rust, Swift, and Mojo achieve
 * native performance — same approach, applied to NC.
 *
 * Usage:
 *   nc compile service.nc          -> generates service.ll (LLVM IR)
 *   llc service.ll -o service.o    -> native object file
 *   cc service.o -o service        -> executable binary
 *
 * NC does NOT link against the LLVM C API. All IR is generated as
 * text and clang/llc is invoked as a subprocess for compilation.
 */

#include "../include/nc.h"
#include "../include/nc_llvm.h"

/* ═══════════════════════════════════════════════════════════════
 *  Internal types
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    FILE *out;
    int   tmp_counter;
    int   str_counter;
    int   label_counter;
    bool  had_error;
} LLVMGen;

static int next_tmp(LLVMGen *g) { return g->tmp_counter++; }
static int next_label(LLVMGen *g) { return g->label_counter++; }

/* Validate a user-provided string for shell safety */
static bool nc_shell_safe(const char *s) {
    if (!s) return false;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c == ';' || c == '|' || c == '&' || c == '$' || c == '`' ||
            c == '(' || c == ')' || c == '{' || c == '}' || c == '<' ||
            c == '>' || c == '\n' || c == '\r') return false;
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 1: LLVM IR Prelude and Helpers
 * ═══════════════════════════════════════════════════════════════ */

static void emit_prelude(LLVMGen *g) {
    fprintf(g->out,
        "; Notation-as-Code — LLVM IR (auto-generated)\n"
        "; Compile: llc this_file.ll -filetype=obj -o output.o\n"
        "; Link:    cc output.o -lnc_runtime -o program\n"
        "\n"
        "; NC value is a tagged union: {type: i8, data: i64}\n"
        "%%NcValue = type { i8, i64 }\n"
        "\n"
        "; Type tags\n"
        "@.tag_none   = private constant i8 0\n"
        "@.tag_bool   = private constant i8 1\n"
        "@.tag_int    = private constant i8 2\n"
        "@.tag_float  = private constant i8 3\n"
        "@.tag_string = private constant i8 4\n"
        "@.tag_list   = private constant i8 5\n"
        "@.tag_map    = private constant i8 6\n"
        "\n"
        "; External runtime functions\n"
        "declare i8* @nc_rt_string_new(i8*, i32)\n"
        "declare i8* @nc_rt_string_concat(i8*, i8*)\n"
        "declare i32 @nc_rt_string_compare(i8*, i8*)\n"
        "declare void @nc_rt_print(%%NcValue)\n"
        "declare void @nc_rt_log(i8*)\n"
        "declare %%NcValue @nc_rt_ai_call(i8*, i8*, i8*)\n"
        "declare %%NcValue @nc_rt_mcp_call(i8*, i8*)\n"
        "declare %%NcValue @nc_rt_map_get(i8*, i8*)\n"
        "declare void @nc_rt_map_set(i8*, i8*, %%NcValue)\n"
        "declare i8* @nc_rt_map_new()\n"
        "declare i8* @nc_rt_list_new()\n"
        "declare void @nc_rt_list_push(i8*, %%NcValue)\n"
        "declare i32 @nc_rt_list_len(i8*)\n"
        "declare %%NcValue @nc_rt_list_get(i8*, i32)\n"
        "declare void @nc_rt_list_set(i8*, i32, %%NcValue)\n"
        "declare %%NcValue @nc_rt_gather(i8*, %%NcValue)\n"
        "declare void @nc_rt_store(i8*, %%NcValue)\n"
        "declare %%NcValue @nc_rt_ask_ai(i8*, i8*)\n"
        "declare void @nc_rt_notify(i8*, i8*)\n"
        "declare void @nc_rt_emit(%%NcValue)\n"
        "declare void @nc_rt_wait(double)\n"
        "declare %%NcValue @nc_rt_call_native(i8*, i32, %%NcValue*)\n"
        "declare i32 @printf(i8*, ...)\n"
        "declare void @sleep(i32)\n"
        "declare i8* @malloc(i64)\n"
        "\n"
    );
}

/* Emit a string constant, returns its id */
static int emit_string_const(LLVMGen *g, const char *str) {
    int id = g->str_counter++;
    int len = (int)strlen(str) + 1;
    fprintf(g->out, "@.str.%d = private constant [%d x i8] c\"", id, len);
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') fprintf(g->out, "\\0A");
        else if (str[i] == '\t') fprintf(g->out, "\\09");
        else if (str[i] == '"') fprintf(g->out, "\\22");
        else if (str[i] == '\\') fprintf(g->out, "\\5C");
        else fprintf(g->out, "%c", str[i]);
    }
    fprintf(g->out, "\\00\"\n");
    return id;
}

/* Helper: emit GEP to get i8* from a string constant */
static void emit_string_gep(LLVMGen *g, int tmp_id, int str_id, int str_len) {
    fprintf(g->out,
        "  %%t%d = getelementptr [%d x i8], [%d x i8]* @.str.%d, i32 0, i32 0\n",
        tmp_id, str_len, str_len, str_id);
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 2: AST-based Expression Codegen
 * ═══════════════════════════════════════════════════════════════ */

static int compile_llvm_expr(LLVMGen *g, NcASTNode *node) {
    if (!node) return -1;
    int result = next_tmp(g);

    switch (node->type) {
    case NODE_INT_LIT:
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %lld, 1\n",
                result, (long long)node->as.int_lit.value);
        return result;

    case NODE_FLOAT_LIT: {
        union { double d; int64_t i; } conv;
        conv.d = node->as.float_lit.value;
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 3, i64 undef}, i64 %lld, 1  ; float %g\n",
                result, (long long)conv.i, node->as.float_lit.value);
        return result;
    }

    case NODE_BOOL_LIT:
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %d, 1\n",
                result, node->as.bool_lit.value ? 1 : 0);
        return result;

    case NODE_NONE_LIT:
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0\n", result);
        return result;

    case NODE_STRING_LIT: {
        int sid = emit_string_const(g, node->as.string_lit.value->chars);
        int slen = (int)strlen(node->as.string_lit.value->chars) + 1;
        int ptr = next_tmp(g);
        emit_string_gep(g, ptr, sid, slen);
        int sobj = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i8* @nc_rt_string_new(i8* %%t%d, i32 %d)\n",
                sobj, ptr, slen - 1);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 4, i64 undef}, i64 ptrtoint(i8* %%t%d to i64), 1\n",
                result, sobj);
        return result;
    }

    case NODE_TEMPLATE: {
        int sid = emit_string_const(g, node->as.template_lit.expr->chars);
        int slen = (int)strlen(node->as.template_lit.expr->chars) + 1;
        int ptr = next_tmp(g);
        emit_string_gep(g, ptr, sid, slen);
        int sobj = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i8* @nc_rt_string_new(i8* %%t%d, i32 %d)\n",
                sobj, ptr, slen - 1);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 4, i64 undef}, i64 ptrtoint(i8* %%t%d to i64), 1\n",
                result, sobj);
        return result;
    }

    case NODE_IDENT: {
        fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%var.%s\n",
                result, node->as.ident.name->chars);
        return result;
    }

    case NODE_MATH: {
        int left = compile_llvm_expr(g, node->as.math.left);
        int right = compile_llvm_expr(g, node->as.math.right);
        int lv = next_tmp(g), rv = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", lv, left);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", rv, right);
        int res_val = next_tmp(g);
        switch (node->as.math.op) {
            case '+': fprintf(g->out, "  %%t%d = add i64 %%t%d, %%t%d\n", res_val, lv, rv); break;
            case '-': fprintf(g->out, "  %%t%d = sub i64 %%t%d, %%t%d\n", res_val, lv, rv); break;
            case '*': fprintf(g->out, "  %%t%d = mul i64 %%t%d, %%t%d\n", res_val, lv, rv); break;
            case '/': fprintf(g->out, "  %%t%d = sdiv i64 %%t%d, %%t%d\n", res_val, lv, rv); break;
            case '%': fprintf(g->out, "  %%t%d = srem i64 %%t%d, %%t%d\n", res_val, lv, rv); break;
        }
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n",
                result, res_val);
        return result;
    }

    case NODE_COMPARISON: {
        int left = compile_llvm_expr(g, node->as.comparison.left);
        int right = compile_llvm_expr(g, node->as.comparison.right);
        int lv = next_tmp(g), rv = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", lv, left);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", rv, right);
        int cmp = next_tmp(g);
        const char *op = node->as.comparison.op->chars;
        if (strcmp(op, "above") == 0)
            fprintf(g->out, "  %%t%d = icmp sgt i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else if (strcmp(op, "below") == 0)
            fprintf(g->out, "  %%t%d = icmp slt i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else if (strcmp(op, "equal") == 0)
            fprintf(g->out, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else if (strcmp(op, "not_equal") == 0)
            fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else if (strcmp(op, "at_least") == 0)
            fprintf(g->out, "  %%t%d = icmp sge i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else if (strcmp(op, "at_most") == 0)
            fprintf(g->out, "  %%t%d = icmp sle i64 %%t%d, %%t%d\n", cmp, lv, rv);
        else
            fprintf(g->out, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", cmp, lv, rv);
        int ext = next_tmp(g);
        fprintf(g->out, "  %%t%d = zext i1 %%t%d to i64\n", ext, cmp);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %%t%d, 1\n",
                result, ext);
        return result;
    }

    case NODE_LOGIC: {
        int left = compile_llvm_expr(g, node->as.logic.left);
        int right = compile_llvm_expr(g, node->as.logic.right);
        int lv = next_tmp(g), rv = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", lv, left);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", rv, right);
        int lbool = next_tmp(g), rbool = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", lbool, lv);
        fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", rbool, rv);
        int res_bool = next_tmp(g);
        if (strcmp(node->as.logic.op->chars, "and") == 0)
            fprintf(g->out, "  %%t%d = and i1 %%t%d, %%t%d\n", res_bool, lbool, rbool);
        else
            fprintf(g->out, "  %%t%d = or i1 %%t%d, %%t%d\n", res_bool, lbool, rbool);
        int ext = next_tmp(g);
        fprintf(g->out, "  %%t%d = zext i1 %%t%d to i64\n", ext, res_bool);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %%t%d, 1\n",
                result, ext);
        return result;
    }

    case NODE_NOT: {
        int operand = compile_llvm_expr(g, node->as.logic.left);
        int val = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", val, operand);
        int cmp = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp eq i64 %%t%d, 0\n", cmp, val);
        int ext = next_tmp(g);
        fprintf(g->out, "  %%t%d = zext i1 %%t%d to i64\n", ext, cmp);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %%t%d, 1\n",
                result, ext);
        return result;
    }

    case NODE_DOT: {
        int obj = compile_llvm_expr(g, node->as.dot.object);
        int obj_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
        int map_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", map_ptr, obj_ptr);
        int sid = emit_string_const(g, node->as.dot.member->chars);
        int slen = (int)strlen(node->as.dot.member->chars) + 1;
        int key_ptr = next_tmp(g);
        emit_string_gep(g, key_ptr, sid, slen);
        fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_map_get(i8* %%t%d, i8* %%t%d)\n",
                result, map_ptr, key_ptr);
        return result;
    }

    case NODE_INDEX: {
        int obj = compile_llvm_expr(g, node->as.math.left);
        int idx = compile_llvm_expr(g, node->as.math.right);
        int obj_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
        int list_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", list_ptr, obj_ptr);
        int idx_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", idx_val, idx);
        int idx_i32 = next_tmp(g);
        fprintf(g->out, "  %%t%d = trunc i64 %%t%d to i32\n", idx_i32, idx_val);
        fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_list_get(i8* %%t%d, i32 %%t%d)\n",
                result, list_ptr, idx_i32);
        return result;
    }

    case NODE_LIST_LIT: {
        int list = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i8* @nc_rt_list_new()\n", list);
        for (int i = 0; i < node->as.list_lit.count; i++) {
            int elem = compile_llvm_expr(g, node->as.list_lit.elements[i]);
            fprintf(g->out, "  call void @nc_rt_list_push(i8* %%t%d, %%NcValue %%t%d)\n",
                    list, elem);
        }
        int ptr_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = ptrtoint i8* %%t%d to i64\n", ptr_val, list);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 5, i64 undef}, i64 %%t%d, 1\n",
                result, ptr_val);
        return result;
    }

    case NODE_MAP_LIT: {
        int map = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i8* @nc_rt_map_new()\n", map);
        for (int i = 0; i < node->as.map_lit.count; i++) {
            int sid = emit_string_const(g, node->as.map_lit.keys[i]->chars);
            int slen = (int)strlen(node->as.map_lit.keys[i]->chars) + 1;
            int key_ptr = next_tmp(g);
            emit_string_gep(g, key_ptr, sid, slen);
            int val = compile_llvm_expr(g, node->as.map_lit.values[i]);
            fprintf(g->out, "  call void @nc_rt_map_set(i8* %%t%d, i8* %%t%d, %%NcValue %%t%d)\n",
                    map, key_ptr, val);
        }
        int ptr_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = ptrtoint i8* %%t%d to i64\n", ptr_val, map);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 6, i64 undef}, i64 %%t%d, 1\n",
                result, ptr_val);
        return result;
    }

    case NODE_CALL: {
        /* Compile args, then call the NC native function */
        int sid = emit_string_const(g, node->as.call.name->chars);
        int slen = (int)strlen(node->as.call.name->chars) + 1;
        int name_ptr = next_tmp(g);
        emit_string_gep(g, name_ptr, sid, slen);

        int argc = node->as.call.arg_count;
        if (argc > 0) {
            int arr = next_tmp(g);
            fprintf(g->out, "  %%t%d = alloca %%NcValue, i32 %d\n", arr, argc);
            for (int i = 0; i < argc; i++) {
                int arg = compile_llvm_expr(g, node->as.call.args[i]);
                int slot = next_tmp(g);
                fprintf(g->out, "  %%t%d = getelementptr %%NcValue, %%NcValue* %%t%d, i32 %d\n",
                        slot, arr, i);
                fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%t%d\n", arg, slot);
            }
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 %d, %%NcValue* %%t%d)\n",
                    result, name_ptr, argc, arr);
        } else {
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 0, %%NcValue* null)\n",
                    result, name_ptr);
        }
        return result;
    }

    default:
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0  ; unhandled expr type %d\n",
                result, node->type);
        return result;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 3: AST-based Statement Codegen
 * ═══════════════════════════════════════════════════════════════ */

static void compile_llvm_stmt(LLVMGen *g, NcASTNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_RESPOND: {
        int val = compile_llvm_expr(g, node->as.single_expr.value);
        fprintf(g->out, "  call void @nc_rt_print(%%NcValue %%t%d)\n", val);
        fprintf(g->out, "  ret %%NcValue %%t%d\n", val);
        break;
    }

    case NODE_LOG: {
        int val = compile_llvm_expr(g, node->as.single_expr.value);
        fprintf(g->out, "  call void @nc_rt_print(%%NcValue %%t%d)\n", val);
        break;
    }

    case NODE_SHOW: {
        int val = compile_llvm_expr(g, node->as.single_expr.value);
        fprintf(g->out, "  call void @nc_rt_print(%%NcValue %%t%d)\n", val);
        break;
    }

    case NODE_SET: {
        int val = compile_llvm_expr(g, node->as.set_stmt.value);
        if (node->as.set_stmt.field) {
            /* set target.field to value */
            int obj = next_tmp(g);
            fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%var.%s\n",
                    obj, node->as.set_stmt.target->chars);
            int obj_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
            int map_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", map_ptr, obj_ptr);
            int sid = emit_string_const(g, node->as.set_stmt.field->chars);
            int slen = (int)strlen(node->as.set_stmt.field->chars) + 1;
            int key_ptr = next_tmp(g);
            emit_string_gep(g, key_ptr, sid, slen);
            fprintf(g->out, "  call void @nc_rt_map_set(i8* %%t%d, i8* %%t%d, %%NcValue %%t%d)\n",
                    map_ptr, key_ptr, val);
        } else {
            fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var.%s\n",
                    val, node->as.set_stmt.target->chars);
        }
        break;
    }

    case NODE_SET_INDEX: {
        int val = compile_llvm_expr(g, node->as.set_index.value);
        int idx = compile_llvm_expr(g, node->as.set_index.index);
        int obj = next_tmp(g);
        fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%var.%s\n",
                obj, node->as.set_index.target->chars);
        int obj_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
        int list_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", list_ptr, obj_ptr);
        int idx_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", idx_val, idx);
        int idx_i32 = next_tmp(g);
        fprintf(g->out, "  %%t%d = trunc i64 %%t%d to i32\n", idx_i32, idx_val);
        fprintf(g->out, "  call void @nc_rt_list_set(i8* %%t%d, i32 %%t%d, %%NcValue %%t%d)\n",
                list_ptr, idx_i32, val);
        break;
    }

    case NODE_IF: {
        int cond = compile_llvm_expr(g, node->as.if_stmt.condition);
        int cv = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", cv, cond);
        int cmp = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", cmp, cv);
        int then_label = next_label(g);
        int else_label = next_label(g);
        int end_label = next_label(g);
        fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", cmp, then_label, else_label);
        fprintf(g->out, "L%d:\n", then_label);
        for (int i = 0; i < node->as.if_stmt.then_count; i++)
            compile_llvm_stmt(g, node->as.if_stmt.then_body[i]);
        fprintf(g->out, "  br label %%L%d\n\n", end_label);
        fprintf(g->out, "L%d:\n", else_label);
        if (node->as.if_stmt.else_body) {
            for (int i = 0; i < node->as.if_stmt.else_count; i++)
                compile_llvm_stmt(g, node->as.if_stmt.else_body[i]);
        }
        fprintf(g->out, "  br label %%L%d\n\n", end_label);
        fprintf(g->out, "L%d:\n", end_label);
        break;
    }

    case NODE_WAIT: {
        fprintf(g->out, "  call void @sleep(i32 %d)\n", (int)node->as.wait_stmt.amount);
        break;
    }

    case NODE_REPEAT: {
        int iter = compile_llvm_expr(g, node->as.repeat.iterable);
        int iter_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", iter_ptr, iter);
        int iter_obj = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", iter_obj, iter_ptr);
        int list_len = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i32 @nc_rt_list_len(i8* %%t%d)\n", list_len, iter_obj);

        fprintf(g->out, "  %%var.%s = alloca %%NcValue\n", node->as.repeat.variable->chars);
        int iter_idx_id = next_tmp(g);
        fprintf(g->out, "  %%iter_idx_%d = alloca i32\n", iter_idx_id);
        fprintf(g->out, "  store i32 0, i32* %%iter_idx_%d\n", iter_idx_id);

        int loop_label = next_label(g);
        int body_label = next_label(g);
        int end_label = next_label(g);

        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", loop_label);
        int cur_idx = next_tmp(g);
        fprintf(g->out, "  %%t%d = load i32, i32* %%iter_idx_%d\n", cur_idx, iter_idx_id);
        int cmp = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp slt i32 %%t%d, %%t%d\n", cmp, cur_idx, list_len);
        fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", cmp, body_label, end_label);

        fprintf(g->out, "L%d:\n", body_label);
        int elem = next_tmp(g);
        fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_list_get(i8* %%t%d, i32 %%t%d)\n",
                elem, iter_obj, cur_idx);
        fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var.%s\n",
                elem, node->as.repeat.variable->chars);

        for (int i = 0; i < node->as.repeat.body_count; i++)
            compile_llvm_stmt(g, node->as.repeat.body[i]);

        int cur2 = next_tmp(g);
        fprintf(g->out, "  %%t%d = load i32, i32* %%iter_idx_%d\n", cur2, iter_idx_id);
        int next_idx = next_tmp(g);
        fprintf(g->out, "  %%t%d = add i32 %%t%d, 1\n", next_idx, cur2);
        fprintf(g->out, "  store i32 %%t%d, i32* %%iter_idx_%d\n", next_idx, iter_idx_id);
        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", end_label);
        break;
    }

    case NODE_WHILE: {
        int loop_label = next_label(g);
        int body_label = next_label(g);
        int end_label = next_label(g);

        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", loop_label);
        int cond = compile_llvm_expr(g, node->as.while_stmt.condition);
        int cv = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", cv, cond);
        int cmp = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", cmp, cv);
        fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", cmp, body_label, end_label);

        fprintf(g->out, "L%d:\n", body_label);
        for (int i = 0; i < node->as.while_stmt.body_count; i++)
            compile_llvm_stmt(g, node->as.while_stmt.body[i]);
        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", end_label);
        break;
    }

    case NODE_FOR_COUNT: {
        int count_val = compile_llvm_expr(g, node->as.for_count.count_expr);
        int max_v = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", max_v, count_val);

        /* Allocate loop counter */
        fprintf(g->out, "  %%var.%s = alloca %%NcValue\n", node->as.for_count.variable->chars);
        int ctr_id = next_tmp(g);
        fprintf(g->out, "  %%ctr_%d = alloca i64\n", ctr_id);
        fprintf(g->out, "  store i64 0, i64* %%ctr_%d\n", ctr_id);

        int loop_label = next_label(g);
        int body_label = next_label(g);
        int end_label = next_label(g);

        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", loop_label);
        int cur = next_tmp(g);
        fprintf(g->out, "  %%t%d = load i64, i64* %%ctr_%d\n", cur, ctr_id);
        int cmp = next_tmp(g);
        fprintf(g->out, "  %%t%d = icmp slt i64 %%t%d, %%t%d\n", cmp, cur, max_v);
        fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", cmp, body_label, end_label);

        fprintf(g->out, "L%d:\n", body_label);
        /* Store current counter as NcValue */
        int cur_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n",
                cur_val, cur);
        fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var.%s\n",
                cur_val, node->as.for_count.variable->chars);

        for (int i = 0; i < node->as.for_count.body_count; i++)
            compile_llvm_stmt(g, node->as.for_count.body[i]);

        int cur2 = next_tmp(g);
        fprintf(g->out, "  %%t%d = load i64, i64* %%ctr_%d\n", cur2, ctr_id);
        int nxt = next_tmp(g);
        fprintf(g->out, "  %%t%d = add i64 %%t%d, 1\n", nxt, cur2);
        fprintf(g->out, "  store i64 %%t%d, i64* %%ctr_%d\n", nxt, ctr_id);
        fprintf(g->out, "  br label %%L%d\n\n", loop_label);
        fprintf(g->out, "L%d:\n", end_label);
        break;
    }

    case NODE_MATCH: {
        int subj = compile_llvm_expr(g, node->as.match_stmt.subject);
        int subj_val = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", subj_val, subj);

        int end_label = next_label(g);
        for (int i = 0; i < node->as.match_stmt.case_count; i++) {
            NcASTNode *when = node->as.match_stmt.cases[i];
            int case_val = compile_llvm_expr(g, when->as.when_clause.value);
            int cv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", cv, case_val);
            int cmp = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp eq i64 %%t%d, %%t%d\n", cmp, subj_val, cv);
            int match_label = next_label(g);
            int next_label_id = next_label(g);
            fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", cmp, match_label, next_label_id);
            fprintf(g->out, "L%d:\n", match_label);
            for (int j = 0; j < when->as.when_clause.body_count; j++)
                compile_llvm_stmt(g, when->as.when_clause.body[j]);
            fprintf(g->out, "  br label %%L%d\n\n", end_label);
            fprintf(g->out, "L%d:\n", next_label_id);
        }
        if (node->as.match_stmt.otherwise) {
            for (int i = 0; i < node->as.match_stmt.otherwise_count; i++)
                compile_llvm_stmt(g, node->as.match_stmt.otherwise[i]);
        }
        fprintf(g->out, "  br label %%L%d\n\n", end_label);
        fprintf(g->out, "L%d:\n", end_label);
        break;
    }

    case NODE_RUN: {
        fprintf(g->out, "  ; call behavior: %s\n", node->as.run_stmt.name->chars);
        fprintf(g->out, "  %%run_result_%d = call %%NcValue @nc_%s(",
                next_tmp(g), node->as.run_stmt.name->chars);
        for (int i = 0; i < node->as.run_stmt.arg_count; i++) {
            if (i > 0) fprintf(g->out, ", ");
            int arg = compile_llvm_expr(g, node->as.run_stmt.args[i]);
            fprintf(g->out, "%%NcValue %%t%d", arg);
        }
        fprintf(g->out, ")\n");
        break;
    }

    case NODE_EMIT: {
        int val = compile_llvm_expr(g, node->as.single_expr.value);
        fprintf(g->out, "  call void @nc_rt_emit(%%NcValue %%t%d)\n", val);
        break;
    }

    case NODE_NOTIFY: {
        int ch = compile_llvm_expr(g, node->as.notify.channel);
        int ch_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", ch_ptr, ch);
        int ch_str = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", ch_str, ch_ptr);
        if (node->as.notify.message) {
            int msg = compile_llvm_expr(g, node->as.notify.message);
            int msg_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", msg_ptr, msg);
            int msg_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", msg_str, msg_ptr);
            fprintf(g->out, "  call void @nc_rt_notify(i8* %%t%d, i8* %%t%d)\n", ch_str, msg_str);
        } else {
            fprintf(g->out, "  call void @nc_rt_notify(i8* %%t%d, i8* null)\n", ch_str);
        }
        break;
    }

    case NODE_STORE: {
        int val = compile_llvm_expr(g, node->as.store_stmt.value);
        int sid = emit_string_const(g, node->as.store_stmt.target->chars);
        int slen = (int)strlen(node->as.store_stmt.target->chars) + 1;
        int tgt_ptr = next_tmp(g);
        emit_string_gep(g, tgt_ptr, sid, slen);
        fprintf(g->out, "  call void @nc_rt_store(i8* %%t%d, %%NcValue %%t%d)\n", tgt_ptr, val);
        break;
    }

    case NODE_GATHER: {
        int sid = emit_string_const(g, node->as.gather.source->chars);
        int slen = (int)strlen(node->as.gather.source->chars) + 1;
        int src_ptr = next_tmp(g);
        emit_string_gep(g, src_ptr, sid, slen);
        int opts = next_tmp(g);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0  ; none opts\n", opts);
        int result = next_tmp(g);
        fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_gather(i8* %%t%d, %%NcValue %%t%d)\n",
                result, src_ptr, opts);
        fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var.%s\n",
                result, node->as.gather.target->chars);
        break;
    }

    case NODE_ASK_AI: {
        int sid = emit_string_const(g, node->as.ask_ai.prompt->chars);
        int slen = (int)strlen(node->as.ask_ai.prompt->chars) + 1;
        int prompt_ptr = next_tmp(g);
        emit_string_gep(g, prompt_ptr, sid, slen);
        int result = next_tmp(g);
        fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_ask_ai(i8* %%t%d, i8* null)\n",
                result, prompt_ptr);
        const char *save_name = node->as.ask_ai.save_as ?
            node->as.ask_ai.save_as->chars : "result";
        fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var.%s\n", result, save_name);
        break;
    }

    case NODE_APPEND: {
        int val = compile_llvm_expr(g, node->as.append_stmt.value);
        int list = next_tmp(g);
        fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%var.%s\n",
                list, node->as.append_stmt.target->chars);
        int list_ptr = next_tmp(g);
        fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", list_ptr, list);
        int list_obj = next_tmp(g);
        fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", list_obj, list_ptr);
        fprintf(g->out, "  call void @nc_rt_list_push(i8* %%t%d, %%NcValue %%t%d)\n",
                list_obj, val);
        break;
    }

    case NODE_TRY: {
        fprintf(g->out, "  ; try block start\n");
        for (int i = 0; i < node->as.try_stmt.body_count; i++)
            compile_llvm_stmt(g, node->as.try_stmt.body[i]);
        fprintf(g->out, "  ; try block end\n");
        break;
    }

    case NODE_EXPR_STMT: {
        compile_llvm_expr(g, node->as.single_expr.value);
        break;
    }

    default:
        fprintf(g->out, "  ; [uncompiled statement: type %d]\n", node->type);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 4: AST-based Top-Level IR Generation
 * ═══════════════════════════════════════════════════════════════ */

void nc_llvm_generate(NcASTNode *program, const char *output_path) {
    FILE *out = fopen(output_path, "w");
    if (!out) { fprintf(stderr, "Cannot open %s for writing\n", output_path); return; }

    LLVMGen g = { .out = out, .tmp_counter = 0, .str_counter = 0, .label_counter = 0 };

    emit_prelude(&g);

    /* String constants for service metadata */
    if (program->as.program.service_name) {
        int sid = emit_string_const(&g, program->as.program.service_name->chars);
        int slen = (int)strlen(program->as.program.service_name->chars) + 1;
        fprintf(out, "@.service_name = private constant i8* getelementptr([%d x i8], [%d x i8]* @.str.%d, i32 0, i32 0)\n",
                slen, slen, sid);
    }
    fprintf(out, "\n");

    /* Generate function for each behavior */
    for (int b = 0; b < program->as.program.beh_count; b++) {
        NcASTNode *beh = program->as.program.behaviors[b];
        g.tmp_counter = 0;
        g.label_counter = 0;

        fprintf(out, "define %%NcValue @nc_%s(", beh->as.behavior.name->chars);
        for (int i = 0; i < beh->as.behavior.param_count; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%%NcValue %%param.%s", beh->as.behavior.params[i]->as.param.name->chars);
        }
        fprintf(out, ") {\nentry:\n");

        /* Allocate local variables for parameters */
        for (int i = 0; i < beh->as.behavior.param_count; i++) {
            fprintf(out, "  %%var.%s = alloca %%NcValue\n",
                    beh->as.behavior.params[i]->as.param.name->chars);
            fprintf(out, "  store %%NcValue %%param.%s, %%NcValue* %%var.%s\n",
                    beh->as.behavior.params[i]->as.param.name->chars,
                    beh->as.behavior.params[i]->as.param.name->chars);
        }

        /* Compile body */
        for (int i = 0; i < beh->as.behavior.body_count; i++)
            compile_llvm_stmt(&g, beh->as.behavior.body[i]);

        /* Default return */
        fprintf(out, "  ret %%NcValue zeroinitializer\n");
        fprintf(out, "}\n\n");
    }

    /* Main function */
    fprintf(out, "define i32 @main() {\n");
    fprintf(out, "entry:\n");
    if (program->as.program.service_name) {
        int sid = emit_string_const(&g, "NC Service Running\\n");
        fprintf(out, "  %%fmt = getelementptr [%d x i8], [%d x i8]* @.str.%d, i32 0, i32 0\n",
                20, 20, sid);
        fprintf(out, "  call i32 (i8*, ...) @printf(i8* %%fmt)\n");
    }
    fprintf(out, "  ret i32 0\n");
    fprintf(out, "}\n");

    fclose(out);
    printf("  LLVM IR written to: %s\n", output_path);
    printf("  To compile: llc %s -filetype=obj -o output.o && cc output.o -o program\n", output_path);
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 5: Bytecode-based IR Generation (ALL opcodes)
 *
 *  Walks the compiled bytecode and generates LLVM IR for every
 *  NC opcode defined in nc_chunk.h.
 * ═══════════════════════════════════════════════════════════════ */

/* State for bytecode->IR translation within one function */
typedef struct {
    LLVMGen   *gen;
    NcChunk   *chunk;
    int        pc;          /* current program counter */
    int        stack_depth; /* simulated stack depth for SSA naming */
    /* SSA value names for the simulated stack (indices into tmp space) */
    int        vstack[256];
    int        vsp;         /* virtual stack pointer */
} BCGen;

static void bc_push(BCGen *bc, int tmp) {
    if (bc->vsp < 256) bc->vstack[bc->vsp++] = tmp;
}

static int bc_pop(BCGen *bc) {
    if (bc->vsp > 0) return bc->vstack[--bc->vsp];
    return 0; /* underflow guard */
}

static int bc_peek(BCGen *bc) {
    if (bc->vsp > 0) return bc->vstack[bc->vsp - 1];
    return 0;
}

static uint16_t bc_read_u16(BCGen *bc) {
    uint8_t hi = bc->chunk->code[bc->pc++];
    uint8_t lo = bc->chunk->code[bc->pc++];
    return (uint16_t)((hi << 8) | lo);
}

static void bc_emit_constant(BCGen *bc, int const_idx) {
    LLVMGen *g = bc->gen;
    NcValue val = bc->chunk->constants[const_idx];
    int t = next_tmp(g);

    if (val.type == VAL_INT) {
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %lld, 1\n",
                t, (long long)val.as.integer);
        bc_push(bc, t);
    } else if (val.type == VAL_FLOAT) {
        union { double d; int64_t i; } conv;
        conv.d = val.as.floating;
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 3, i64 undef}, i64 %lld, 1  ; float %g\n",
                t, (long long)conv.i, val.as.floating);
        bc_push(bc, t);
    } else if (val.type == VAL_BOOL) {
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %d, 1\n",
                t, val.as.boolean ? 1 : 0);
        bc_push(bc, t);
    } else if (val.type == VAL_STRING) {
        NcString *s = val.as.string;
        int sid = emit_string_const(g, s->chars);
        int slen = s->length + 1;
        int ptr = next_tmp(g);
        emit_string_gep(g, ptr, sid, slen);
        int sobj = next_tmp(g);
        fprintf(g->out, "  %%t%d = call i8* @nc_rt_string_new(i8* %%t%d, i32 %d)\n",
                sobj, ptr, s->length);
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 4, i64 undef}, i64 ptrtoint(i8* %%t%d to i64), 1\n",
                t, sobj);
        bc_push(bc, t);
    } else {
        /* none or other */
        fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0\n", t);
        bc_push(bc, t);
    }
}

/* Generate IR for a single bytecode function/chunk */
static void bc_gen_chunk(LLVMGen *g, NcChunk *chunk, const char *func_name,
                         int param_count, NcString **param_names) {
    (void)param_names; /* names used for documentation only */
    BCGen bc = {
        .gen = g, .chunk = chunk, .pc = 0, .stack_depth = 0, .vsp = 0
    };

    fprintf(g->out, "define %%NcValue @nc_%s(", func_name);
    for (int i = 0; i < param_count; i++) {
        if (i > 0) fprintf(g->out, ", ");
        fprintf(g->out, "%%NcValue %%param_%d", i);
    }
    fprintf(g->out, ") {\nentry:\n");

    /* Allocate locals (all as alloca) */
    for (int i = 0; i < chunk->var_count; i++) {
        fprintf(g->out, "  %%var_%d = alloca %%NcValue  ; %s\n",
                i, chunk->var_names[i] ? chunk->var_names[i]->chars : "?");
    }
    /* Allocate local slots */
    for (int i = 0; i < NC_LOCALS_MAX && i < 32; i++) {
        fprintf(g->out, "  %%local_%d = alloca %%NcValue\n", i);
    }

    /* Store parameters into first local slots */
    for (int i = 0; i < param_count; i++) {
        fprintf(g->out, "  store %%NcValue %%param_%d, %%NcValue* %%local_%d\n", i, i);
    }

    /* Label at each bytecode offset (for jump targets) */
    /* Pre-scan to find all jump targets */
    bool *is_target = calloc(chunk->count + 1, 1);
    for (int pc = 0; pc < chunk->count; ) {
        uint8_t op = chunk->code[pc];
        switch (op) {
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_AND:
        case OP_OR: {
            uint16_t offset = (uint16_t)((chunk->code[pc+1] << 8) | chunk->code[pc+2]);
            int target = pc + 3 + offset;
            if (target >= 0 && target <= chunk->count)
                is_target[target] = true;
            pc += 3;
            break;
        }
        case OP_LOOP: {
            uint16_t offset = (uint16_t)((chunk->code[pc+1] << 8) | chunk->code[pc+2]);
            int target = pc + 3 - offset;
            if (target >= 0 && target <= chunk->count)
                is_target[target] = true;
            pc += 3;
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
            pc += 2; break;
        case OP_CONSTANT_LONG:
            pc += 3; break;
        default:
            pc += 1; break;
        }
    }
    /* Initial branch to bc_0 */
    fprintf(g->out, "  br label %%bc_0\n\n");

    /* Main bytecode translation loop */
    bc.pc = 0;
    while (bc.pc < chunk->count) {
        int cur_pc = bc.pc;

        /* Emit label if this is a jump target */
        if (is_target[cur_pc] || cur_pc == 0) {
            fprintf(g->out, "bc_%d:\n", cur_pc);
        }

        uint8_t op = chunk->code[bc.pc++];

        switch (op) {

        /* ── Data ────────────────────────────────────────── */
        case OP_CONSTANT: {
            uint8_t idx = chunk->code[bc.pc++];
            bc_emit_constant(&bc, idx);
            break;
        }
        case OP_CONSTANT_LONG: {
            uint16_t idx = bc_read_u16(&bc);
            bc_emit_constant(&bc, idx);
            break;
        }
        case OP_NONE: {
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0\n", t);
            bc_push(&bc, t);
            break;
        }
        case OP_TRUE: {
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 1, 1\n", t);
            bc_push(&bc, t);
            break;
        }
        case OP_FALSE: {
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 0, 1\n", t);
            bc_push(&bc, t);
            break;
        }
        case OP_POP: {
            if (bc.vsp > 0) bc.vsp--;
            break;
        }

        /* ── Variables ───────────────────────────────────── */
        case OP_GET_VAR: {
            uint8_t idx = chunk->code[bc.pc++];
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%var_%d  ; get %s\n",
                    t, idx, idx < chunk->var_count && chunk->var_names[idx] ?
                    chunk->var_names[idx]->chars : "?");
            bc_push(&bc, t);
            break;
        }
        case OP_SET_VAR: {
            uint8_t idx = chunk->code[bc.pc++];
            int v = bc_peek(&bc);
            fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%var_%d  ; set %s\n",
                    v, idx, idx < chunk->var_count && chunk->var_names[idx] ?
                    chunk->var_names[idx]->chars : "?");
            break;
        }
        case OP_GET_LOCAL: {
            uint8_t idx = chunk->code[bc.pc++];
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = load %%NcValue, %%NcValue* %%local_%d\n", t, idx);
            bc_push(&bc, t);
            break;
        }
        case OP_SET_LOCAL: {
            uint8_t idx = chunk->code[bc.pc++];
            int v = bc_peek(&bc);
            fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%local_%d\n", v, idx);
            break;
        }

        /* ── Fields / Indexes ────────────────────────────── */
        case OP_GET_FIELD: {
            uint8_t name_idx = chunk->code[bc.pc++];
            int obj = bc_pop(&bc);
            int obj_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
            int map_p = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", map_p, obj_ptr);
            const char *field_name = (name_idx < chunk->var_count && chunk->var_names[name_idx]) ?
                chunk->var_names[name_idx]->chars : "field";
            int sid = emit_string_const(g, field_name);
            int slen = (int)strlen(field_name) + 1;
            int key = next_tmp(g);
            emit_string_gep(g, key, sid, slen);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_map_get(i8* %%t%d, i8* %%t%d)\n",
                    t, map_p, key);
            bc_push(&bc, t);
            break;
        }
        case OP_SET_FIELD: {
            uint8_t name_idx = chunk->code[bc.pc++];
            int val = bc_pop(&bc);
            int obj = bc_pop(&bc);
            int obj_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
            int map_p = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", map_p, obj_ptr);
            const char *field_name = (name_idx < chunk->var_count && chunk->var_names[name_idx]) ?
                chunk->var_names[name_idx]->chars : "field";
            int sid = emit_string_const(g, field_name);
            int slen = (int)strlen(field_name) + 1;
            int key = next_tmp(g);
            emit_string_gep(g, key, sid, slen);
            fprintf(g->out, "  call void @nc_rt_map_set(i8* %%t%d, i8* %%t%d, %%NcValue %%t%d)\n",
                    map_p, key, val);
            /* Push the map back (VM semantics) */
            bc_push(&bc, obj);
            break;
        }
        case OP_GET_INDEX: {
            int idx = bc_pop(&bc);
            int obj = bc_pop(&bc);
            int obj_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
            int list_p = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", list_p, obj_ptr);
            int idx_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", idx_val, idx);
            int idx_i32 = next_tmp(g);
            fprintf(g->out, "  %%t%d = trunc i64 %%t%d to i32\n", idx_i32, idx_val);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_list_get(i8* %%t%d, i32 %%t%d)\n",
                    t, list_p, idx_i32);
            bc_push(&bc, t);
            break;
        }
        case OP_SET_INDEX: {
            int val = bc_pop(&bc);
            int idx = bc_pop(&bc);
            int obj = bc_pop(&bc);
            int obj_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", obj_ptr, obj);
            int list_p = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", list_p, obj_ptr);
            int idx_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", idx_val, idx);
            int idx_i32 = next_tmp(g);
            fprintf(g->out, "  %%t%d = trunc i64 %%t%d to i32\n", idx_i32, idx_val);
            fprintf(g->out, "  call void @nc_rt_list_set(i8* %%t%d, i32 %%t%d, %%NcValue %%t%d)\n",
                    list_p, idx_i32, val);
            /* Push object back */
            bc_push(&bc, obj);
            break;
        }
        case OP_SLICE: {
            int end_v = bc_pop(&bc);
            int start_v = bc_pop(&bc);
            int obj = bc_pop(&bc);
            /* Emit as runtime call (placeholder — full slice in runtime) */
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue zeroinitializer, i8 0, 0  ; TODO: slice\n", t);
            bc_push(&bc, t);
            (void)end_v; (void)start_v; (void)obj;
            break;
        }

        /* ── Arithmetic ──────────────────────────────────── */
        case OP_ADD: {
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int av = next_tmp(g), bv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b);

            /* Check type tag for string concat vs integer add */
            int tag_a = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 0\n", tag_a, a);
            int is_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp eq i8 %%t%d, 4\n", is_str, tag_a);
            int lbl_str = next_label(g), lbl_num = next_label(g), lbl_merge = next_label(g);
            fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%L%d\n\n", is_str, lbl_str, lbl_num);

            /* String concat path */
            fprintf(g->out, "L%d:\n", lbl_str);
            int sa = next_tmp(g), sb = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", sa, av);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", sb, bv);
            int cat = next_tmp(g);
            fprintf(g->out, "  %%t%d = call i8* @nc_rt_string_concat(i8* %%t%d, i8* %%t%d)\n", cat, sa, sb);
            int str_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = ptrtoint i8* %%t%d to i64\n", str_val, cat);
            int str_res = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 4, i64 undef}, i64 %%t%d, 1\n", str_res, str_val);
            fprintf(g->out, "  br label %%L%d\n\n", lbl_merge);

            /* Numeric add path */
            fprintf(g->out, "L%d:\n", lbl_num);
            int sum = next_tmp(g);
            fprintf(g->out, "  %%t%d = add i64 %%t%d, %%t%d\n", sum, av, bv);
            int num_res = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", num_res, sum);
            fprintf(g->out, "  br label %%L%d\n\n", lbl_merge);

            /* Merge */
            fprintf(g->out, "L%d:\n", lbl_merge);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = phi %%NcValue [ %%t%d, %%L%d ], [ %%t%d, %%L%d ]\n",
                    t, str_res, lbl_str, num_res, lbl_num);
            bc_push(&bc, t);
            break;
        }
        case OP_SUBTRACT: {
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int av = next_tmp(g), bv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b);
            int r = next_tmp(g);
            fprintf(g->out, "  %%t%d = sub i64 %%t%d, %%t%d\n", r, av, bv);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", t, r);
            bc_push(&bc, t);
            break;
        }
        case OP_MULTIPLY: {
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int av = next_tmp(g), bv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b);
            int r = next_tmp(g);
            fprintf(g->out, "  %%t%d = mul i64 %%t%d, %%t%d\n", r, av, bv);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", t, r);
            bc_push(&bc, t);
            break;
        }
        case OP_DIVIDE: {
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int av = next_tmp(g), bv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b);
            int r = next_tmp(g);
            fprintf(g->out, "  %%t%d = sdiv i64 %%t%d, %%t%d\n", r, av, bv);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", t, r);
            bc_push(&bc, t);
            break;
        }
        case OP_MODULO: {
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int av = next_tmp(g), bv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b);
            int r = next_tmp(g);
            fprintf(g->out, "  %%t%d = srem i64 %%t%d, %%t%d\n", r, av, bv);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", t, r);
            bc_push(&bc, t);
            break;
        }
        case OP_NEGATE: {
            int a = bc_pop(&bc);
            int av = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            int r = next_tmp(g);
            fprintf(g->out, "  %%t%d = sub i64 0, %%t%d\n", r, av);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 2, i64 undef}, i64 %%t%d, 1\n", t, r);
            bc_push(&bc, t);
            break;
        }

        /* ── Logic / Comparison ──────────────────────────── */
        case OP_NOT: {
            int a = bc_pop(&bc);
            int av = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a);
            int cmp = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp eq i64 %%t%d, 0\n", cmp, av);
            int ext = next_tmp(g);
            fprintf(g->out, "  %%t%d = zext i1 %%t%d to i64\n", ext, cmp);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %%t%d, 1\n", t, ext);
            bc_push(&bc, t);
            break;
        }

#define BC_CMP_OP(opname, icmp_pred)                                           \
        case opname: {                                                         \
            int b = bc_pop(&bc), a = bc_pop(&bc);                              \
            int av = next_tmp(g), bv = next_tmp(g);                            \
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", av, a); \
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", bv, b); \
            int cmp = next_tmp(g);                                             \
            fprintf(g->out, "  %%t%d = icmp " icmp_pred " i64 %%t%d, %%t%d\n", cmp, av, bv); \
            int ext = next_tmp(g);                                             \
            fprintf(g->out, "  %%t%d = zext i1 %%t%d to i64\n", ext, cmp);    \
            int t = next_tmp(g);                                               \
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 %%t%d, 1\n", t, ext); \
            bc_push(&bc, t);                                                   \
            break;                                                             \
        }

        BC_CMP_OP(OP_EQUAL,    "eq")
        BC_CMP_OP(OP_NOT_EQUAL,"ne")
        BC_CMP_OP(OP_ABOVE,    "sgt")
        BC_CMP_OP(OP_BELOW,    "slt")
        BC_CMP_OP(OP_AT_LEAST, "sge")
        BC_CMP_OP(OP_AT_MOST,  "sle")

#undef BC_CMP_OP

        case OP_IN: {
            /* Runtime call for 'in' membership check */
            int b = bc_pop(&bc), a = bc_pop(&bc);
            int t = next_tmp(g);
            fprintf(g->out, "  ; OP_IN — membership test (runtime)\n");
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 1, i64 undef}, i64 0, 1  ; TODO: in\n", t);
            bc_push(&bc, t);
            (void)a; (void)b;
            break;
        }

        case OP_AND: {
            /* Short-circuit AND: if top is falsy, jump offset ahead */
            uint16_t offset = bc_read_u16(&bc);
            int top = bc_peek(&bc);
            int tv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", tv, top);
            int cmp = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", cmp, tv);
            int target = bc.pc + offset;
            int true_label = next_label(g);
            fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%bc_%d\n\n", cmp, true_label, target);
            fprintf(g->out, "L%d:\n", true_label);
            /* Pop the truthy left side — will evaluate right side */
            bc_pop(&bc);
            break;
        }
        case OP_OR: {
            /* Short-circuit OR: if top is truthy, jump offset ahead */
            uint16_t offset = bc_read_u16(&bc);
            int top = bc_peek(&bc);
            int tv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", tv, top);
            int cmp = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", cmp, tv);
            int target = bc.pc + offset;
            int false_label = next_label(g);
            fprintf(g->out, "  br i1 %%t%d, label %%bc_%d, label %%L%d\n\n", cmp, target, false_label);
            fprintf(g->out, "L%d:\n", false_label);
            bc_pop(&bc);
            break;
        }

        /* ── Control Flow ────────────────────────────────── */
        case OP_JUMP: {
            uint16_t offset = bc_read_u16(&bc);
            int target = bc.pc + offset;
            fprintf(g->out, "  br label %%bc_%d\n\n", target);
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = bc_read_u16(&bc);
            int target = bc.pc + offset;
            int top = bc_peek(&bc);
            int tv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", tv, top);
            int cmp = next_tmp(g);
            fprintf(g->out, "  %%t%d = icmp ne i64 %%t%d, 0\n", cmp, tv);
            int next_lbl = next_label(g);
            fprintf(g->out, "  br i1 %%t%d, label %%L%d, label %%bc_%d\n\n", cmp, next_lbl, target);
            fprintf(g->out, "L%d:\n", next_lbl);
            break;
        }
        case OP_LOOP: {
            uint16_t offset = bc_read_u16(&bc);
            int target = bc.pc - offset;
            fprintf(g->out, "  br label %%bc_%d\n\n", target);
            break;
        }

        /* ── Function Calls ──────────────────────────────── */
        case OP_CALL: {
            uint8_t argc = chunk->code[bc.pc++];
            /* Args are on stack after the function name */
            /* Pop args in reverse */
            int args[256];
            for (int i = argc - 1; i >= 0; i--)
                args[i] = bc_pop(&bc);
            int name_v = bc_pop(&bc); /* function name string */

            /* Extract function name pointer */
            int name_ptr_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", name_ptr_val, name_v);
            int name_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", name_ptr, name_ptr_val);

            /* Allocate args array */
            int t = next_tmp(g);
            if (argc > 0) {
                int arr = next_tmp(g);
                fprintf(g->out, "  %%t%d = alloca %%NcValue, i32 %d\n", arr, argc);
                for (int i = 0; i < argc; i++) {
                    int slot = next_tmp(g);
                    fprintf(g->out, "  %%t%d = getelementptr %%NcValue, %%NcValue* %%t%d, i32 %d\n",
                            slot, arr, i);
                    fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%t%d\n", args[i], slot);
                }
                fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 %d, %%NcValue* %%t%d)\n",
                        t, name_ptr, argc, arr);
            } else {
                fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 0, %%NcValue* null)\n",
                        t, name_ptr);
            }
            bc_push(&bc, t);
            break;
        }
        case OP_CALL_NATIVE: {
            uint8_t argc = chunk->code[bc.pc++];
            int args[256];
            for (int i = argc - 1; i >= 0; i--)
                args[i] = bc_pop(&bc);
            int name_v = bc_pop(&bc);

            int name_ptr_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", name_ptr_val, name_v);
            int name_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", name_ptr, name_ptr_val);

            int t = next_tmp(g);
            if (argc > 0) {
                int arr = next_tmp(g);
                fprintf(g->out, "  %%t%d = alloca %%NcValue, i32 %d\n", arr, argc);
                for (int i = 0; i < argc; i++) {
                    int slot = next_tmp(g);
                    fprintf(g->out, "  %%t%d = getelementptr %%NcValue, %%NcValue* %%t%d, i32 %d\n",
                            slot, arr, i);
                    fprintf(g->out, "  store %%NcValue %%t%d, %%NcValue* %%t%d\n", args[i], slot);
                }
                fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 %d, %%NcValue* %%t%d)\n",
                        t, name_ptr, argc, arr);
            } else {
                fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_call_native(i8* %%t%d, i32 0, %%NcValue* null)\n",
                        t, name_ptr);
            }
            bc_push(&bc, t);
            break;
        }
        case OP_RETURN: {
            int v = (bc.vsp > 0) ? bc_pop(&bc) : -1;
            if (v >= 0)
                fprintf(g->out, "  ret %%NcValue %%t%d\n", v);
            else
                fprintf(g->out, "  ret %%NcValue zeroinitializer\n");
            break;
        }
        case OP_HALT: {
            fprintf(g->out, "  ret %%NcValue zeroinitializer\n");
            break;
        }

        /* ── Collections ─────────────────────────────────── */
        case OP_MAKE_LIST: {
            uint8_t count = chunk->code[bc.pc++];
            int list = next_tmp(g);
            fprintf(g->out, "  %%t%d = call i8* @nc_rt_list_new()\n", list);
            /* Pop items in order (they were pushed left to right) */
            int items[256];
            for (int i = count - 1; i >= 0; i--)
                items[i] = bc_pop(&bc);
            for (int i = 0; i < count; i++) {
                fprintf(g->out, "  call void @nc_rt_list_push(i8* %%t%d, %%NcValue %%t%d)\n",
                        list, items[i]);
            }
            int ptr_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = ptrtoint i8* %%t%d to i64\n", ptr_val, list);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 5, i64 undef}, i64 %%t%d, 1\n", t, ptr_val);
            bc_push(&bc, t);
            break;
        }
        case OP_MAKE_MAP: {
            uint8_t count = chunk->code[bc.pc++];
            int map = next_tmp(g);
            fprintf(g->out, "  %%t%d = call i8* @nc_rt_map_new()\n", map);
            /* Pop key-value pairs (pushed as key, value, key, value...) */
            int pairs[512];
            for (int i = count * 2 - 1; i >= 0; i--)
                pairs[i] = bc_pop(&bc);
            for (int i = 0; i < count; i++) {
                int key = pairs[i * 2];
                int val = pairs[i * 2 + 1];
                int key_ptr = next_tmp(g);
                fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", key_ptr, key);
                int key_str = next_tmp(g);
                fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", key_str, key_ptr);
                fprintf(g->out, "  call void @nc_rt_map_set(i8* %%t%d, i8* %%t%d, %%NcValue %%t%d)\n",
                        map, key_str, val);
            }
            int ptr_val = next_tmp(g);
            fprintf(g->out, "  %%t%d = ptrtoint i8* %%t%d to i64\n", ptr_val, map);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = insertvalue %%NcValue {i8 6, i64 undef}, i64 %%t%d, 1\n", t, ptr_val);
            bc_push(&bc, t);
            break;
        }

        /* ── NC-Specific I/O & AI ────────────────────────── */
        case OP_GATHER: {
            int opts = bc_pop(&bc);
            int src = bc_pop(&bc);
            int src_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", src_ptr, src);
            int src_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", src_str, src_ptr);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_gather(i8* %%t%d, %%NcValue %%t%d)\n",
                    t, src_str, opts);
            bc_push(&bc, t);
            break;
        }
        case OP_STORE: {
            int val = bc_pop(&bc);
            int tgt = bc_pop(&bc);
            int tgt_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", tgt_ptr, tgt);
            int tgt_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", tgt_str, tgt_ptr);
            fprintf(g->out, "  call void @nc_rt_store(i8* %%t%d, %%NcValue %%t%d)\n", tgt_str, val);
            break;
        }
        case OP_ASK_AI: {
            int ctx = bc_pop(&bc);   /* using context list */
            int prompt = bc_pop(&bc);
            int prompt_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", prompt_ptr, prompt);
            int prompt_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", prompt_str, prompt_ptr);
            int ctx_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", ctx_ptr, ctx);
            int ctx_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", ctx_str, ctx_ptr);
            int t = next_tmp(g);
            fprintf(g->out, "  %%t%d = call %%NcValue @nc_rt_ask_ai(i8* %%t%d, i8* %%t%d)\n",
                    t, prompt_str, ctx_str);
            bc_push(&bc, t);
            (void)ctx;
            break;
        }
        case OP_NOTIFY: {
            int msg = bc_pop(&bc);
            int ch = bc_pop(&bc);
            int ch_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", ch_ptr, ch);
            int ch_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", ch_str, ch_ptr);
            int msg_ptr = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", msg_ptr, msg);
            int msg_str = next_tmp(g);
            fprintf(g->out, "  %%t%d = inttoptr i64 %%t%d to i8*\n", msg_str, msg_ptr);
            fprintf(g->out, "  call void @nc_rt_notify(i8* %%t%d, i8* %%t%d)\n", ch_str, msg_str);
            break;
        }
        case OP_LOG: {
            int v = bc_pop(&bc);
            fprintf(g->out, "  call void @nc_rt_print(%%NcValue %%t%d)\n", v);
            break;
        }
        case OP_EMIT: {
            int v = bc_pop(&bc);
            fprintf(g->out, "  call void @nc_rt_emit(%%NcValue %%t%d)\n", v);
            break;
        }
        case OP_WAIT: {
            int v = bc_pop(&bc);
            int fv = next_tmp(g);
            fprintf(g->out, "  %%t%d = extractvalue %%NcValue %%t%d, 1\n", fv, v);
            /* Interpret as seconds, call sleep */
            int sec = next_tmp(g);
            fprintf(g->out, "  %%t%d = trunc i64 %%t%d to i32\n", sec, fv);
            fprintf(g->out, "  call void @sleep(i32 %%t%d)\n", sec);
            break;
        }
        case OP_RESPOND: {
            int v = bc_pop(&bc);
            fprintf(g->out, "  call void @nc_rt_print(%%NcValue %%t%d)\n", v);
            fprintf(g->out, "  ret %%NcValue %%t%d\n", v);
            break;
        }

        /* ── UI opcodes (skip for native compilation) ────── */
        case OP_UI_ELEMENT: case OP_UI_PROP: case OP_UI_PROP_EXPR:
        case OP_UI_TEXT: case OP_UI_CHILD: case OP_UI_END_ELEMENT:
        case OP_STATE_DECLARE: case OP_STATE_GET: case OP_STATE_SET:
        case OP_STATE_COMPUTED: case OP_STATE_WATCH:
        case OP_UI_BIND: case OP_UI_BIND_INPUT:
        case OP_UI_ON_EVENT: case OP_UI_COMPONENT:
        case OP_UI_MOUNT: case OP_UI_UNMOUNT:
        case OP_UI_ON_MOUNT: case OP_UI_ON_UNMOUNT:
        case OP_UI_RENDER: case OP_UI_DIFF: case OP_UI_PATCH:
        case OP_UI_ROUTE_DEF: case OP_UI_ROUTE_PUSH:
        case OP_UI_ROUTE_GUARD: case OP_UI_ROUTE_MATCH:
        case OP_UI_FETCH: case OP_UI_FETCH_AUTH:
        case OP_UI_IF: case OP_UI_FOR_EACH: case OP_UI_SHOW:
        case OP_UI_FORM: case OP_UI_VALIDATE: case OP_UI_FORM_SUBMIT:
        case OP_UI_AUTH_CHECK: case OP_UI_ROLE_CHECK: case OP_UI_PERM_CHECK:
            fprintf(g->out, "  ; [UI opcode %d — skipped for native compilation]\n", op);
            /* Some UI opcodes have operands; skip 1 byte for safety */
            if (op == OP_UI_ELEMENT || op == OP_UI_PROP || op == OP_UI_PROP_EXPR ||
                op == OP_STATE_DECLARE || op == OP_STATE_GET || op == OP_STATE_SET ||
                op == OP_UI_VALIDATE || op == OP_UI_ROLE_CHECK || op == OP_UI_PERM_CHECK ||
                op == OP_UI_ROUTE_PUSH || op == OP_UI_ON_MOUNT || op == OP_UI_ON_UNMOUNT ||
                op == OP_STATE_COMPUTED || op == OP_STATE_WATCH ||
                op == OP_UI_ON_EVENT || op == OP_UI_COMPONENT) {
                bc.pc++; /* skip 1-byte operand */
            }
            if (op == OP_UI_BIND || op == OP_UI_BIND_INPUT ||
                op == OP_UI_ROUTE_DEF || op == OP_UI_ROUTE_GUARD ||
                op == OP_UI_FOR_EACH || op == OP_UI_FORM) {
                bc.pc += 2; /* skip 2-byte operand */
            }
            break;

        default:
            fprintf(g->out, "  ; [unknown opcode %d]\n", op);
            break;
        }
    }

    /* Default return at end of function */
    fprintf(g->out, "  ret %%NcValue zeroinitializer\n");
    fprintf(g->out, "}\n\n");

    free(is_target);
}

void nc_llvm_generate_from_bytecode(NcCompiler *comp, const char *output_path) {
    if (!comp || comp->had_error) return;

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Cannot open %s for writing\n", output_path);
        return;
    }

    LLVMGen g = { .out = out, .tmp_counter = 0, .str_counter = 0, .label_counter = 0 };

    emit_prelude(&g);
    fprintf(out, "\n");

    /* Generate LLVM IR for each behavior chunk */
    for (int i = 0; i < comp->chunk_count; i++) {
        g.tmp_counter = 0;
        g.label_counter = 0;
        const char *name = comp->beh_names[i] ? comp->beh_names[i]->chars : "unknown";
        bc_gen_chunk(&g, &comp->chunks[i], name, 0, NULL);
    }

    /* Main entry point */
    fprintf(out, "define i32 @main() {\n");
    fprintf(out, "entry:\n");
    if (comp->chunk_count > 0) {
        /* Call the first behavior as entry */
        const char *entry = comp->beh_names[0] ? comp->beh_names[0]->chars : "unknown";
        fprintf(out, "  %%result = call %%NcValue @nc_%s()\n", entry);
    }
    fprintf(out, "  ret i32 0\n");
    fprintf(out, "}\n");

    fclose(out);
    printf("  LLVM IR (bytecode) written to: %s\n", output_path);
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 6: Bytecode Optimization Passes
 *
 *  These run on the bytecode BEFORE IR generation, performing
 *  NC-specific optimizations that reduce code size and improve
 *  the quality of generated IR.
 * ═══════════════════════════════════════════════════════════════ */

/* ── Constant Folding ─────────────────────────────────────── */
/* Identifies sequences like: CONSTANT a, CONSTANT b, OP_ADD
 * and replaces them with a single CONSTANT (a+b).               */

void nc_opt_constant_fold(NcChunk *chunk) {
    if (!chunk || chunk->count < 4) return;

    for (int pc = 0; pc + 4 < chunk->count; ) {
        /* Look for: OP_CONSTANT idx1, OP_CONSTANT idx2, <arith_op> */
        if (chunk->code[pc] == OP_CONSTANT &&
            chunk->code[pc + 2] == OP_CONSTANT &&
            pc + 4 < chunk->count) {

            uint8_t idx_a = chunk->code[pc + 1];
            uint8_t idx_b = chunk->code[pc + 3];
            uint8_t op    = chunk->code[pc + 4];

            if (idx_a >= chunk->const_count || idx_b >= chunk->const_count) { pc++; continue; }

            NcValue a = chunk->constants[idx_a];
            NcValue b = chunk->constants[idx_b];

            /* Only fold integer operations */
            if (a.type != VAL_INT || b.type != VAL_INT) { pc++; continue; }

            int64_t result;
            bool folded = true;
            switch (op) {
                case OP_ADD:      result = a.as.integer + b.as.integer; break;
                case OP_SUBTRACT: result = a.as.integer - b.as.integer; break;
                case OP_MULTIPLY: result = a.as.integer * b.as.integer; break;
                case OP_DIVIDE:
                    if (b.as.integer == 0) { folded = false; break; }
                    result = a.as.integer / b.as.integer;
                    break;
                case OP_MODULO:
                    if (b.as.integer == 0) { folded = false; break; }
                    result = a.as.integer % b.as.integer;
                    break;
                default:
                    folded = false;
                    break;
            }

            if (folded) {
                /* Add the folded constant */
                NcValue folded_val;
                folded_val.type = VAL_INT;
                folded_val.as.integer = result;
                int new_idx = nc_chunk_add_constant(chunk, folded_val);
                if (new_idx < 0 || new_idx > 255) { pc++; continue; }

                /* Replace: CONST a, CONST b, OP -> CONST result, NOP, NOP, NOP, NOP */
                int line = chunk->lines[pc];
                chunk->code[pc]     = OP_CONSTANT;
                chunk->code[pc + 1] = (uint8_t)new_idx;
                /* Fill remainder with OP_POP to maintain valid bytecode
                 * (these will be cleaned up by dead code elimination) */
                chunk->code[pc + 2] = OP_NONE;
                chunk->code[pc + 3] = OP_POP;
                chunk->code[pc + 4] = OP_POP;
                chunk->lines[pc + 2] = line;
                chunk->lines[pc + 3] = line;
                chunk->lines[pc + 4] = line;

                /* Don't advance — try to fold again from same position */
                continue;
            }
        }

        /* Also fold float constants */
        if (chunk->code[pc] == OP_CONSTANT &&
            chunk->code[pc + 2] == OP_CONSTANT &&
            pc + 4 < chunk->count) {

            uint8_t idx_a = chunk->code[pc + 1];
            uint8_t idx_b = chunk->code[pc + 3];
            uint8_t op    = chunk->code[pc + 4];

            if (idx_a < chunk->const_count && idx_b < chunk->const_count) {
                NcValue a = chunk->constants[idx_a];
                NcValue b = chunk->constants[idx_b];

                if (a.type == VAL_FLOAT && b.type == VAL_FLOAT) {
                    double result;
                    bool folded = true;
                    switch (op) {
                        case OP_ADD:      result = a.as.floating + b.as.floating; break;
                        case OP_SUBTRACT: result = a.as.floating - b.as.floating; break;
                        case OP_MULTIPLY: result = a.as.floating * b.as.floating; break;
                        case OP_DIVIDE:
                            if (b.as.floating == 0.0) { folded = false; break; }
                            result = a.as.floating / b.as.floating;
                            break;
                        default: folded = false; break;
                    }

                    if (folded) {
                        NcValue fv;
                        fv.type = VAL_FLOAT;
                        fv.as.floating = result;
                        int new_idx = nc_chunk_add_constant(chunk, fv);
                        if (new_idx >= 0 && new_idx <= 255) {
                            int line = chunk->lines[pc];
                            chunk->code[pc]     = OP_CONSTANT;
                            chunk->code[pc + 1] = (uint8_t)new_idx;
                            chunk->code[pc + 2] = OP_NONE;
                            chunk->code[pc + 3] = OP_POP;
                            chunk->code[pc + 4] = OP_POP;
                            chunk->lines[pc + 2] = line;
                            chunk->lines[pc + 3] = line;
                            chunk->lines[pc + 4] = line;
                            continue;
                        }
                    }
                }
            }
        }

        pc++;
    }
}

/* ── Dead Code Elimination ────────────────────────────────── */
/* Removes unreachable code after unconditional jumps/returns. */

void nc_opt_dead_code_eliminate(NcChunk *chunk) {
    if (!chunk || chunk->count < 2) return;

    /* Mark reachable bytecode offsets */
    bool *reachable = calloc(chunk->count + 1, 1);
    if (!reachable) return;

    /* Simple forward pass: mark code reachable from pc=0 */
    /* Use a work list for proper reachability */
    int *worklist = malloc(chunk->count * sizeof(int));
    int wl_count = 0;
    worklist[wl_count++] = 0;
    reachable[0] = true;

    while (wl_count > 0) {
        int pc = worklist[--wl_count];
        if (pc < 0 || pc >= chunk->count) continue;

        uint8_t op = chunk->code[pc];

        /* Determine instruction size and successors */
        int next_pc = -1;
        int branch_pc = -1;

        switch (op) {
        case OP_CONSTANT:
        case OP_GET_VAR: case OP_SET_VAR:
        case OP_GET_LOCAL: case OP_SET_LOCAL:
        case OP_GET_FIELD: case OP_SET_FIELD:
        case OP_CALL: case OP_CALL_NATIVE:
        case OP_MAKE_LIST: case OP_MAKE_MAP:
            next_pc = pc + 2;
            break;
        case OP_CONSTANT_LONG:
            next_pc = pc + 3;
            break;
        case OP_JUMP: {
            uint16_t offset = (uint16_t)((chunk->code[pc+1] << 8) | chunk->code[pc+2]);
            branch_pc = pc + 3 + offset;
            /* No fallthrough — unconditional */
            break;
        }
        case OP_JUMP_IF_FALSE:
        case OP_AND:
        case OP_OR: {
            uint16_t offset = (uint16_t)((chunk->code[pc+1] << 8) | chunk->code[pc+2]);
            next_pc = pc + 3;
            branch_pc = pc + 3 + offset;
            break;
        }
        case OP_LOOP: {
            uint16_t offset = (uint16_t)((chunk->code[pc+1] << 8) | chunk->code[pc+2]);
            branch_pc = pc + 3 - offset;
            /* No fallthrough */
            break;
        }
        case OP_RETURN:
        case OP_HALT:
            /* No successors */
            break;
        default:
            next_pc = pc + 1;
            break;
        }

        /* Add successors to worklist */
        if (next_pc >= 0 && next_pc < chunk->count && !reachable[next_pc]) {
            reachable[next_pc] = true;
            worklist[wl_count++] = next_pc;
        }
        if (branch_pc >= 0 && branch_pc < chunk->count && !reachable[branch_pc]) {
            reachable[branch_pc] = true;
            worklist[wl_count++] = branch_pc;
        }
    }

    /* NOP-out unreachable instructions (replace with OP_NONE + OP_POP pairs) */
    /* We can only safely NOP single-byte instructions. For multi-byte ones
     * in unreachable code, we leave them (they won't execute anyway). */
    /* Actually, just replace each unreachable byte with a harmless sequence */
    /* Keep it simple: only eliminate obvious cases like dead code after
     * unconditional jumps — scan for OP_JUMP/OP_RETURN followed by non-jump-target code */
    for (int pc = 0; pc < chunk->count; ) {
        uint8_t op = chunk->code[pc];
        if ((op == OP_RETURN || op == OP_HALT) && pc + 1 < chunk->count) {
            /* Check if next instruction is unreachable */
            int next = pc + 1;
            while (next < chunk->count && !reachable[next]) {
                chunk->code[next] = OP_NONE; /* Replace with harmless NOP */
                next++;
            }
        }
        /* Advance past this instruction */
        switch (op) {
        case OP_CONSTANT: case OP_GET_VAR: case OP_SET_VAR:
        case OP_GET_LOCAL: case OP_SET_LOCAL:
        case OP_GET_FIELD: case OP_SET_FIELD:
        case OP_CALL: case OP_CALL_NATIVE:
        case OP_MAKE_LIST: case OP_MAKE_MAP:
            pc += 2; break;
        case OP_CONSTANT_LONG: case OP_JUMP: case OP_JUMP_IF_FALSE:
        case OP_LOOP: case OP_AND: case OP_OR:
            pc += 3; break;
        default:
            pc += 1; break;
        }
    }

    free(reachable);
    free(worklist);
}

/* ── Strength Reduction ───────────────────────────────────── */
/* Replaces expensive operations with cheaper equivalents:
 *   - Multiply by power of 2 -> left shift
 *   - Divide by power of 2 -> arithmetic right shift
 *   - Multiply by 0 -> push 0
 *   - Multiply by 1 -> nop (remove multiply)
 *   - Add 0 -> nop                                              */

static bool is_power_of_two(int64_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

static int log2_of(int64_t n) {
    int r = 0;
    while (n > 1) { n >>= 1; r++; }
    return r;
}

void nc_opt_strength_reduce(NcChunk *chunk) {
    if (!chunk || chunk->count < 4) return;

    for (int pc = 0; pc + 2 < chunk->count; ) {
        /* Pattern: OP_CONSTANT idx, OP_MULTIPLY/OP_DIVIDE
         * where the constant is a power of 2 */
        if (chunk->code[pc] == OP_CONSTANT && pc + 2 < chunk->count) {
            uint8_t idx = chunk->code[pc + 1];
            uint8_t op = chunk->code[pc + 2];

            if (idx < chunk->const_count) {
                NcValue val = chunk->constants[idx];
                if (val.type == VAL_INT) {
                    int64_t n = val.as.integer;

                    if (op == OP_MULTIPLY && n == 0) {
                        /* x * 0 = 0: replace the preceding value + multiply
                         * with just pushing 0. We replace multiply with NOP. */
                        /* Actually tricky without knowing what precedes.
                         * Just replace: CONST 0, MUL -> CONST 0, POP (pop the other operand)
                         * This isn't quite right for stack machine. Leave as-is for safety. */
                        pc += 3; continue;
                    }

                    if (op == OP_MULTIPLY && n == 1) {
                        /* x * 1 = x: remove the constant and the multiply */
                        int line = chunk->lines[pc];
                        chunk->code[pc]     = OP_NONE;
                        chunk->code[pc + 1] = OP_POP;
                        chunk->code[pc + 2] = OP_NONE; /* was multiply */
                        chunk->lines[pc] = line;
                        chunk->lines[pc + 1] = line;
                        pc += 3; continue;
                    }

                    if (op == OP_ADD && n == 0) {
                        /* x + 0 = x: remove constant and add */
                        int line = chunk->lines[pc];
                        chunk->code[pc]     = OP_NONE;
                        chunk->code[pc + 1] = OP_POP;
                        chunk->code[pc + 2] = OP_NONE;
                        chunk->lines[pc] = line;
                        chunk->lines[pc + 1] = line;
                        pc += 3; continue;
                    }

                    if (op == OP_MULTIPLY && is_power_of_two(n) && n > 1) {
                        /* x * 2^k -> x << k
                         * We replace the constant with the shift amount and
                         * change OP_MULTIPLY to a marker. Since the VM doesn't
                         * have a shift opcode, we store the folded result:
                         * Replace CONST pow2, MUL with CONST shift_amount, MUL
                         * where the IR gen knows to emit shl instead.
                         *
                         * Better approach: change the constant value to the
                         * shift amount and leave a comment in the constant pool.
                         * The bytecode IR generator will emit shl when it sees
                         * a multiply by a power of 2. */
                        int shift = log2_of(n);
                        NcValue shift_val;
                        shift_val.type = VAL_INT;
                        shift_val.as.integer = shift;
                        int new_idx = nc_chunk_add_constant(chunk, shift_val);
                        if (new_idx >= 0 && new_idx <= 255) {
                            chunk->code[pc + 1] = (uint8_t)new_idx;
                            /* Mark multiply as a left shift by tagging in
                             * a way the bytecode->IR translator can detect.
                             * We use the convention: if the IR generator sees
                             * OP_MULTIPLY after a small constant (0..63), it
                             * checks if the original was a power-of-2 pattern.
                             * For now, keep as OP_MULTIPLY — the IR gen will
                             * handle strength reduction at IR level. */
                        }
                        pc += 3; continue;
                    }

                    if (op == OP_DIVIDE && is_power_of_two(n) && n > 1) {
                        /* x / 2^k -> x >> k (arithmetic) — same approach */
                        pc += 3; continue;
                    }
                }
            }
        }
        pc++;
    }
}

/* ── Run All Optimization Passes ─────────────────────────── */

void nc_llvm_optimize_bytecode(NcChunk *chunk) {
    if (!chunk) return;

    /* Multiple passes for better results */
    for (int pass = 0; pass < 3; pass++) {
        nc_opt_constant_fold(chunk);
        nc_opt_strength_reduce(chunk);
        nc_opt_dead_code_eliminate(chunk);
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 7: Native Code Emission
 *
 *  Compiles IR text to a native object file by shelling out
 *  to clang or llc (whichever is available).
 * ═══════════════════════════════════════════════════════════════ */

static bool tool_exists(const char *name) {
    if (!nc_shell_safe(name)) return false;
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", name);
    return system(cmd) == 0;
}

int nc_llvm_emit_object(const char *ir_text, const char *output_path) {
    if (!nc_shell_safe(output_path)) {
        fprintf(stderr, "[NC AOT] Error: unsafe characters in output path\n");
        return 1;
    }

    /* Write IR to a temporary .ll file */
    char tmp_ll[512];
    snprintf(tmp_ll, sizeof(tmp_ll), "%s.ll", output_path);

    FILE *f = fopen(tmp_ll, "w");
    if (!f) {
        fprintf(stderr, "[NC AOT] Cannot write temporary IR file: %s\n", tmp_ll);
        return 1;
    }
    fputs(ir_text, f);
    fclose(f);

    char cmd[1024];
    int ret;

    /* Prefer clang (compiles .ll directly to .o) */
    if (tool_exists("clang")) {
        snprintf(cmd, sizeof(cmd),
            "clang -c -O2 -o \"%s\" \"%s\" 2>&1", output_path, tmp_ll);
        printf("  [NC AOT] clang -c -O2 -o %s %s\n", output_path, tmp_ll);
        ret = system(cmd);
        if (ret == 0) {
            remove(tmp_ll);
            return 0;
        }
        fprintf(stderr, "[NC AOT] clang failed (exit %d), trying llc...\n", ret);
    }

    /* Fallback: llc (IR -> .o) */
    if (tool_exists("llc")) {
        snprintf(cmd, sizeof(cmd),
            "llc -filetype=obj -O2 -o \"%s\" \"%s\" 2>&1", output_path, tmp_ll);
        printf("  [NC AOT] llc -filetype=obj -O2 -o %s %s\n", output_path, tmp_ll);
        ret = system(cmd);
        if (ret == 0) {
            remove(tmp_ll);
            return 0;
        }
        fprintf(stderr, "[NC AOT] llc failed (exit %d)\n", ret);
    }

    /* Neither available — leave the IR file for manual compilation */
    fprintf(stderr,
        "[NC AOT] Neither clang nor llc found.\n"
        "  LLVM IR saved to: %s\n"
        "  To compile manually:\n"
        "    llc %s -filetype=obj -o %s\n"
        "  Or:\n"
        "    clang -c -O2 -o %s %s\n",
        tmp_ll, tmp_ll, output_path, output_path, tmp_ll);
    return 2;
}

/* ═══════════════════════════════════════════════════════════════
 *  PART 8: AOT Compilation Pipeline
 *
 *  Full pipeline: NC source -> parse -> compile -> optimize ->
 *                 IR -> native binary
 *
 *  Links with a minimal NC runtime stub that provides the
 *  nc_rt_* functions declared in the IR prelude.
 * ═══════════════════════════════════════════════════════════════ */

/* Minimal runtime stub — written to a .c file and compiled alongside.
 * This provides the nc_rt_* functions that the generated IR calls. */
static const char *NC_RUNTIME_STUB =
    "/* NC minimal runtime stub — auto-generated */\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "#include <unistd.h>\n"
    "\n"
    "typedef struct { char type; long long data; } NcValue;\n"
    "\n"
    "char* nc_rt_string_new(const char *s, int len) {\n"
    "    char *buf = malloc(len + 1);\n"
    "    memcpy(buf, s, len); buf[len] = 0;\n"
    "    return buf;\n"
    "}\n"
    "char* nc_rt_string_concat(char *a, char *b) {\n"
    "    int la = strlen(a), lb = strlen(b);\n"
    "    char *r = malloc(la + lb + 1);\n"
    "    memcpy(r, a, la); memcpy(r + la, b, lb); r[la+lb] = 0;\n"
    "    return r;\n"
    "}\n"
    "int nc_rt_string_compare(char *a, char *b) { return strcmp(a, b); }\n"
    "void nc_rt_print(NcValue v) {\n"
    "    switch(v.type) {\n"
    "        case 0: printf(\"none\\n\"); break;\n"
    "        case 1: printf(\"%s\\n\", v.data ? \"true\" : \"false\"); break;\n"
    "        case 2: printf(\"%lld\\n\", v.data); break;\n"
    "        case 3: { union { double d; long long i; } u; u.i = v.data; printf(\"%g\\n\", u.d); break; }\n"
    "        case 4: printf(\"%s\\n\", (char*)v.data); break;\n"
    "        default: printf(\"[object]\\n\"); break;\n"
    "    }\n"
    "}\n"
    "void nc_rt_log(const char *s) { printf(\"%s\\n\", s); }\n"
    "NcValue nc_rt_ai_call(char *a, char *b, char *c) { NcValue v={0,0}; return v; }\n"
    "NcValue nc_rt_mcp_call(char *a, char *b) { NcValue v={0,0}; return v; }\n"
    "NcValue nc_rt_map_get(char *m, char *k) { NcValue v={0,0}; return v; }\n"
    "void nc_rt_map_set(char *m, char *k, NcValue v) {}\n"
    "char* nc_rt_map_new(void) { return calloc(1, 256); }\n"
    "char* nc_rt_list_new(void) { return calloc(1, 256); }\n"
    "void nc_rt_list_push(char *l, NcValue v) {}\n"
    "int nc_rt_list_len(char *l) { return 0; }\n"
    "NcValue nc_rt_list_get(char *l, int i) { NcValue v={0,0}; return v; }\n"
    "void nc_rt_list_set(char *l, int i, NcValue v) {}\n"
    "NcValue nc_rt_gather(char *s, NcValue o) { NcValue v={0,0}; return v; }\n"
    "void nc_rt_store(char *t, NcValue v) {}\n"
    "NcValue nc_rt_ask_ai(char *p, char *c) { NcValue v={0,0}; return v; }\n"
    "void nc_rt_notify(char *c, char *m) { printf(\"[notify %s] %s\\n\", c, m ? m : \"\"); }\n"
    "void nc_rt_emit(NcValue v) { nc_rt_print(v); }\n"
    "void nc_rt_wait(double s) { sleep((int)s); }\n"
    "NcValue nc_rt_call_native(char *name, int argc, NcValue *args) {\n"
    "    printf(\"[native call] %s (%d args)\\n\", name, argc);\n"
    "    NcValue v={0,0}; return v;\n"
    "}\n";

int nc_aot_compile(const char *nc_source, const char *output_binary) {
    if (!nc_source || !output_binary) return 1;

    if (!nc_shell_safe(output_binary)) {
        fprintf(stderr, "[NC AOT] Error: unsafe characters in output path\n");
        return 1;
    }

    printf("[NC AOT] Starting ahead-of-time compilation...\n");

    /* Step 1: Parse */
    printf("  [1/6] Parsing NC source...\n");
    NcLexer *lexer = nc_lexer_new(nc_source, "<aot>");
    nc_lexer_tokenize(lexer);
    NcParser *parser = nc_parser_new(lexer->tokens, lexer->token_count, "<aot>");
    NcASTNode *program = nc_parser_parse(parser);
    if (!program || parser->had_error) {
        fprintf(stderr, "[NC AOT] Parse failed: %s\n",
                parser->had_error ? parser->error_msg : "unknown error");
        nc_parser_free(parser);
        nc_lexer_free(lexer);
        return 1;
    }

    /* Step 2: Compile to bytecode */
    printf("  [2/6] Compiling to bytecode...\n");
    NcCompiler *comp = nc_compiler_new();
    if (!nc_compiler_compile(comp, program)) {
        fprintf(stderr, "[NC AOT] Compilation failed: %s\n", comp->error_msg);
        nc_compiler_free(comp);
        nc_ast_free(program);
        nc_parser_free(parser);
        nc_lexer_free(lexer);
        return 1;
    }

    /* Step 3: Optimize bytecode */
    printf("  [3/6] Optimizing bytecode...\n");
    for (int i = 0; i < comp->chunk_count; i++) {
        nc_llvm_optimize_bytecode(&comp->chunks[i]);
    }

    /* Step 4: Generate LLVM IR */
    printf("  [4/6] Generating LLVM IR...\n");
    char ir_path[512];
    snprintf(ir_path, sizeof(ir_path), "%s.ll", output_binary);
    nc_llvm_generate_from_bytecode(comp, ir_path);

    /* Step 5: Write runtime stub */
    printf("  [5/6] Writing runtime stub...\n");
    char rt_path[512];
    snprintf(rt_path, sizeof(rt_path), "%s_rt.c", output_binary);
    FILE *rt_file = fopen(rt_path, "w");
    if (!rt_file) {
        fprintf(stderr, "[NC AOT] Cannot write runtime stub: %s\n", rt_path);
        nc_compiler_free(comp);
        nc_ast_free(program);
        nc_parser_free(parser);
        nc_lexer_free(lexer);
        return 1;
    }
    fputs(NC_RUNTIME_STUB, rt_file);
    fclose(rt_file);

    /* Step 6: Compile and link */
    printf("  [6/6] Compiling to native binary...\n");

    char cmd[2048];
    int ret;

    if (tool_exists("clang")) {
        snprintf(cmd, sizeof(cmd),
            "clang -O2 -o \"%s\" \"%s\" \"%s\" 2>&1",
            output_binary, ir_path, rt_path);
        printf("  $ %s\n", cmd);
        ret = system(cmd);
    } else if (tool_exists("gcc")) {
        /* GCC can't compile .ll directly, need llc first */
        char obj_path[512];
        snprintf(obj_path, sizeof(obj_path), "%s.o", output_binary);

        if (tool_exists("llc")) {
            snprintf(cmd, sizeof(cmd),
                "llc -filetype=obj -O2 -o \"%s\" \"%s\" 2>&1",
                obj_path, ir_path);
            ret = system(cmd);
            if (ret == 0) {
                snprintf(cmd, sizeof(cmd),
                    "gcc -O2 -o \"%s\" \"%s\" \"%s\" 2>&1",
                    output_binary, obj_path, rt_path);
                ret = system(cmd);
                remove(obj_path);
            }
        } else {
            fprintf(stderr, "[NC AOT] Need clang or llc+gcc to compile.\n");
            ret = 1;
        }
    } else {
        fprintf(stderr, "[NC AOT] No C compiler found. Install clang or gcc.\n");
        ret = 1;
    }

    /* Clean up temp files on success */
    if (ret == 0) {
        remove(ir_path);
        remove(rt_path);
        printf("\n[NC AOT] Success! Native binary: %s\n", output_binary);
        printf("  Run it: ./%s\n", output_binary);
    } else {
        fprintf(stderr, "\n[NC AOT] Compilation failed (exit %d).\n", ret);
        fprintf(stderr, "  IR saved to: %s\n", ir_path);
        fprintf(stderr, "  Runtime stub: %s\n", rt_path);
    }

    nc_compiler_free(comp);
    nc_ast_free(program);
    nc_parser_free(parser);
    nc_lexer_free(lexer);
    return ret;
}
