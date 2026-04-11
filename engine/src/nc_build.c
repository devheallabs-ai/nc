/*
 * nc_build.c — Native binary build system for NC.
 *
 * Pipeline:  NC source -> bytecode -> embed in C -> compile with system cc
 *
 * This produces standalone native binaries from NC source files
 * without requiring LLVM libraries at build time. The system C
 * compiler (cc/gcc/clang) does all the heavy lifting.
 */

#include "../include/nc.h"
#include "../include/nc_build.h"

/* ═══════════════════════════════════════════════════════════
 *  Source files required for the standalone runtime
 * ═══════════════════════════════════════════════════════════ */

static const char *runtime_src_files[] = {
    "nc_value.c", "nc_vm.c", "nc_jit.c", "nc_gc.c",
    "nc_json.c", "nc_http.c", "nc_stdlib.c", "nc_lexer.c",
    "nc_parser.c", "nc_compiler.c", "nc_interp.c", "nc_llvm.c",
    "nc_semantic.c", "nc_repl.c", "nc_module.c", "nc_optimizer.c",
    "nc_polyglot.c", "nc_async.c", "nc_distributed.c",
    "nc_enterprise.c", "nc_server.c", "nc_database.c",
    "nc_websocket.c", "nc_middleware.c", "nc_pkg.c",
    "nc_debug.c", "nc_lsp.c", "nc_tensor.c", "nc_autograd.c",
    "nc_suggestions.c", "nc_migrate.c", "nc_plugin.c",
    "nc_embed.c", "nc_model.c", "nc_training.c", "nc_tokenizer.c",
    "nc_cortex.c", "nc_nova.c", "nc_crypto.c", "nc_generate.c",
    "nc_ai_router.c", "nc_ai_benchmark.c", "nc_ai_enterprise.c",
    "nc_nova_reasoning.c", "nc_ai_efficient.c", "nc_wasm.c",
    "nc_dataset.c", "nc_metal_noop.c",
    NULL
};

/* ═══════════════════════════════════════════════════════════
 *  Helpers
 * ═══════════════════════════════════════════════════════════ */

/* Validate a user-provided string for shell safety */
static bool is_shell_safe(const char *s) {
    if (!s) return false;
    for (const char *c = s; *c; c++) {
        if (*c == '"' || *c == ';' || *c == '|' ||
            *c == '&' || *c == '$' || *c == '`' ||
            *c == '(' || *c == ')' || *c == '{' ||
            *c == '}' || *c == '<' || *c == '>' ||
            *c == '\n' || *c == '\r') {
            return false;
        }
    }
    return true;
}

/* Read a file into a malloc'd buffer. Returns NULL on failure. */
static char *build_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[NC Build] Cannot open file: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* Escape a C string literal (write escaped chars to file) */
static void emit_c_string_escaped(FILE *out, const char *s, int len) {
    for (int i = 0; i < len; i++) {
        char ch = s[i];
        if (ch == '"')       fprintf(out, "\\\"");
        else if (ch == '\\') fprintf(out, "\\\\");
        else if (ch == '\n') fprintf(out, "\\n");
        else if (ch == '\r') fprintf(out, "\\r");
        else if (ch == '\t') fprintf(out, "\\t");
        else if (ch == '\0') fprintf(out, "\\0");
        else                 fputc(ch, out);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Config initialization and CLI parsing
 * ═══════════════════════════════════════════════════════════ */

void nc_build_config_init(NcBuildConfig *cfg, const char *input_file) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->input, input_file, sizeof(cfg->input) - 1);

    /* Derive output name: strip path and .nc extension */
    char tmp[512];
    strncpy(tmp, input_file, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    char *dot = strrchr(tmp, '.');
    if (dot) *dot = '\0';
    char *slash = nc_last_path_sep(tmp);
    if (slash)
        strncpy(cfg->output, slash + 1, sizeof(cfg->output) - 1);
    else
        strncpy(cfg->output, tmp, sizeof(cfg->output) - 1);

    /* Default: current platform */
    cfg->target[0] = '\0';
    cfg->optimize = false;
    cfg->strip = false;
    cfg->static_link = false;
}

int nc_build_config_parse(NcBuildConfig *cfg, int argc, char *argv[],
                          int start_idx) {
    for (int i = start_idx; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strncpy(cfg->output, argv[++i], sizeof(cfg->output) - 1);
        } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            strncpy(cfg->target, argv[++i], sizeof(cfg->target) - 1);
        } else if (strcmp(argv[i], "--static") == 0) {
            cfg->static_link = true;
        } else if (strcmp(argv[i], "--release") == 0) {
            cfg->optimize = true;
            cfg->strip = true;
        } else if (strcmp(argv[i], "--optimize") == 0 || strcmp(argv[i], "-O") == 0) {
            cfg->optimize = true;
        } else if (strcmp(argv[i], "--strip") == 0) {
            cfg->strip = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "[NC Build] Unknown option: %s\n", argv[i]);
            fprintf(stderr, "  Options:\n");
            fprintf(stderr, "    -o <name>          Output binary name\n");
            fprintf(stderr, "    --target <triple>   Cross-compile target (linux-x64, darwin-arm64, windows-x64)\n");
            fprintf(stderr, "    --static            Produce a statically-linked binary\n");
            fprintf(stderr, "    --release           Optimize + strip (production build)\n");
            fprintf(stderr, "    --optimize / -O     Enable -O2 optimization\n");
            fprintf(stderr, "    --strip             Strip debug symbols\n");
            return 1;
        }
    }

    /* Validate output name for shell safety */
    if (!is_shell_safe(cfg->output)) {
        fprintf(stderr, "[NC Build] Output name contains unsafe characters\n");
        return 1;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  1. Bytecode embedding — generate C source with bytecode
 * ═══════════════════════════════════════════════════════════ */

int nc_build_embed_bytecode(NcCompiler *comp, NcASTNode *program,
                            const char *output_c_path) {
    FILE *stub = fopen(output_c_path, "w");
    if (!stub) {
        fprintf(stderr, "[NC Build] Cannot create temp file: %s\n", output_c_path);
        return 1;
    }

    /* ── Header includes ── */
    fprintf(stub, "/* Auto-generated by nc build — do not edit */\n");
    fprintf(stub, "#include \"nc.h\"\n");
    fprintf(stub, "#include <signal.h>\n\n");

    /* ── Bytecode arrays for ALL behaviors ── */
    for (int c = 0; c < comp->chunk_count; c++) {
        NcChunk *ch = &comp->chunks[c];
        fprintf(stub, "static const uint8_t _code_%d[] = {", c);
        for (int j = 0; j < ch->count; j++) {
            if (j % 16 == 0) fprintf(stub, "\n    ");
            fprintf(stub, "0x%02x", ch->code[j]);
            if (j < ch->count - 1) fprintf(stub, ",");
        }
        fprintf(stub, "\n};\n\n");
    }

    /* ── String constants for ALL behaviors ── */
    for (int c = 0; c < comp->chunk_count; c++) {
        NcChunk *ch = &comp->chunks[c];
        for (int j = 0; j < ch->const_count; j++) {
            NcValue v = ch->constants[j];
            if (IS_STRING(v)) {
                fprintf(stub, "static const char _str_%d_%d[] = \"", c, j);
                emit_c_string_escaped(stub, AS_STRING(v)->chars,
                                      AS_STRING(v)->length);
                fprintf(stub, "\";\n");
            }
        }
    }

    /* ── Chunk reconstruction helper ── */
    fprintf(stub, "\nstatic NcChunk *build_chunk(int idx) {\n");
    fprintf(stub, "    NcChunk *chunk = nc_chunk_new();\n");
    fprintf(stub, "    const uint8_t *codes[] = {");
    for (int c = 0; c < comp->chunk_count; c++)
        fprintf(stub, "_code_%d%s", c, c < comp->chunk_count - 1 ? "," : "");
    fprintf(stub, "};\n");
    fprintf(stub, "    const int sizes[] = {");
    for (int c = 0; c < comp->chunk_count; c++)
        fprintf(stub, "%d%s", comp->chunks[c].count,
                c < comp->chunk_count - 1 ? "," : "");
    fprintf(stub, "};\n");
    fprintf(stub, "    for (int i = 0; i < sizes[idx]; i++)\n");
    fprintf(stub, "        nc_chunk_write(chunk, codes[idx][i], 0);\n");
    fprintf(stub, "    switch (idx) {\n");

    for (int c = 0; c < comp->chunk_count; c++) {
        NcChunk *ch = &comp->chunks[c];
        fprintf(stub, "    case %d:\n", c);
        for (int j = 0; j < ch->const_count; j++) {
            NcValue v = ch->constants[j];
            if (IS_STRING(v))
                fprintf(stub,
                    "        nc_chunk_add_constant(chunk, "
                    "NC_STRING(nc_string_from_cstr(_str_%d_%d)));\n", c, j);
            else if (IS_INT(v))
                fprintf(stub,
                    "        nc_chunk_add_constant(chunk, NC_INT(%lld));\n",
                    (long long)AS_INT(v));
            else if (IS_FLOAT(v))
                fprintf(stub,
                    "        nc_chunk_add_constant(chunk, NC_FLOAT(%g));\n",
                    AS_FLOAT(v));
            else if (IS_BOOL(v))
                fprintf(stub,
                    "        nc_chunk_add_constant(chunk, NC_BOOL(%s));\n",
                    AS_BOOL(v) ? "true" : "false");
            else
                fprintf(stub,
                    "        nc_chunk_add_constant(chunk, NC_NONE());\n");
        }
        for (int j = 0; j < ch->var_count; j++)
            fprintf(stub,
                "        nc_chunk_add_var(chunk, nc_string_from_cstr(\"%s\"));\n",
                ch->var_names[j]->chars);
        fprintf(stub, "        break;\n");
    }

    fprintf(stub, "    }\n");
    fprintf(stub, "    return chunk;\n");
    fprintf(stub, "}\n\n");

    /* ── Behavior name table ── */
    fprintf(stub, "static const char *beh_names[] = {");
    for (int c = 0; c < comp->chunk_count; c++)
        fprintf(stub, "\"%s\"%s", comp->beh_names[c]->chars,
                c < comp->chunk_count - 1 ? "," : "");
    fprintf(stub, "};\n");
    fprintf(stub, "static const int beh_count = %d;\n\n", comp->chunk_count);

    /* ── Service metadata ── */
    const char *svc_name = program->as.program.service_name
        ? program->as.program.service_name->chars : "nc-service";
    const char *svc_ver = program->as.program.version
        ? program->as.program.version->chars : "";
    const char *svc_model = program->as.program.model
        ? program->as.program.model->chars : "";
    fprintf(stub, "static const char *service_name = \"%s\";\n", svc_name);
    fprintf(stub, "static const char *service_version = \"%s\";\n", svc_ver);
    fprintf(stub, "static const char *service_model = \"%s\";\n\n", svc_model);

    /* ── main() — full CLI like `nc run` ── */
    fprintf(stub, "int main(int argc, char *argv[]) {\n");
    fprintf(stub, "    nc_gc_init();\n");
    fprintf(stub, "    const char *run_behavior = NULL;\n");
    fprintf(stub, "    int serve_port = 0;\n");
    fprintf(stub, "    int do_serve = 0;\n\n");

    /* Argument parsing */
    fprintf(stub, "    for (int i = 1; i < argc; i++) {\n");
    fprintf(stub, "        if (strcmp(argv[i], \"-b\") == 0 && i+1 < argc) run_behavior = argv[++i];\n");
    fprintf(stub, "        else if (strcmp(argv[i], \"serve\") == 0) do_serve = 1;\n");
    fprintf(stub, "        else if (strcmp(argv[i], \"-p\") == 0 && i+1 < argc) serve_port = atoi(argv[++i]);\n");
    fprintf(stub, "        else if (strcmp(argv[i], \"--help\") == 0 || strcmp(argv[i], \"-h\") == 0) {\n");
    fprintf(stub, "            printf(\"%%s v%%s — compiled NC service\\n\\n\", service_name, service_version);\n");
    fprintf(stub, "            printf(\"Usage:\\n\");\n");
    fprintf(stub, "            printf(\"  %%s                 Show service info\\n\", argv[0]);\n");
    fprintf(stub, "            printf(\"  %%s -b <behavior>   Run a behavior\\n\", argv[0]);\n");
    fprintf(stub, "            printf(\"  %%s serve [-p port]  Start HTTP server\\n\", argv[0]);\n");
    fprintf(stub, "            printf(\"\\nBehaviors:\\n\");\n");
    fprintf(stub, "            for (int b = 0; b < beh_count; b++)\n");
    fprintf(stub, "                printf(\"  %%s\\n\", beh_names[b]);\n");
    fprintf(stub, "            return 0;\n");
    fprintf(stub, "        }\n");
    fprintf(stub, "    }\n\n");

    /* Service info banner */
    fprintf(stub, "    printf(\"\\n\");\n");
    fprintf(stub, "    printf(\"======================================================\\n\");\n");
    fprintf(stub, "    printf(\"  Service: %%s\\n\", service_name);\n");
    fprintf(stub, "    if (service_version[0]) printf(\"  Version: %%s\\n\", service_version);\n");
    fprintf(stub, "    if (service_model[0]) printf(\"  Model:   %%s\\n\", service_model);\n");
    fprintf(stub, "    printf(\"  Engine:  NC Compiled Binary\\n\");\n");
    fprintf(stub, "    printf(\"======================================================\\n\");\n");
    fprintf(stub, "    printf(\"  Behaviors (%%d):\", beh_count);\n");
    fprintf(stub, "    for (int b = 0; b < beh_count; b++)\n");
    fprintf(stub, "        printf(\" %%s%%s\", beh_names[b], b < beh_count-1 ? \",\" : \"\");\n");
    fprintf(stub, "    printf(\"\\n\\n\");\n\n");

    /* Run behavior if specified */
    fprintf(stub, "    if (run_behavior) {\n");
    fprintf(stub, "        int found = -1;\n");
    fprintf(stub, "        for (int b = 0; b < beh_count; b++)\n");
    fprintf(stub, "            if (strcmp(beh_names[b], run_behavior) == 0) { found = b; break; }\n");
    fprintf(stub, "        if (found < 0) {\n");
    fprintf(stub, "            fprintf(stderr, \"  Unknown behavior: %%s\\n\", run_behavior);\n");
    fprintf(stub, "            nc_gc_shutdown();\n");
    fprintf(stub, "            return 1;\n");
    fprintf(stub, "        }\n");
    fprintf(stub, "        NcVM *vm = nc_vm_new();\n");
    fprintf(stub, "        NcChunk *chunk = build_chunk(found);\n");
    fprintf(stub, "        printf(\"  Running '%%s'...\\n\\n\", run_behavior);\n");
    fprintf(stub, "        NcValue result = nc_vm_execute_fast(vm, chunk);\n");
    fprintf(stub, "        if (!IS_NONE(result)) {\n");
    fprintf(stub, "            printf(\"  Result: \");\n");
    fprintf(stub, "            nc_value_print(result, stdout);\n");
    fprintf(stub, "            printf(\"\\n\");\n");
    fprintf(stub, "        }\n");
    fprintf(stub, "        if (vm->output_count > 0)\n");
    fprintf(stub, "            for (int i = 0; i < vm->output_count; i++)\n");
    fprintf(stub, "                printf(\"  %%s\\n\", vm->output[i]);\n");
    fprintf(stub, "        nc_chunk_free(chunk);\n");
    fprintf(stub, "        nc_vm_free(vm);\n");

    /* If no specific behavior, run the first one (default behavior) */
    fprintf(stub, "    } else if (!do_serve && beh_count > 0) {\n");
    fprintf(stub, "        NcVM *vm = nc_vm_new();\n");
    fprintf(stub, "        NcChunk *chunk = build_chunk(0);\n");
    fprintf(stub, "        printf(\"  Running '%%s'...\\n\\n\", beh_names[0]);\n");
    fprintf(stub, "        NcValue result = nc_vm_execute_fast(vm, chunk);\n");
    fprintf(stub, "        if (!IS_NONE(result)) {\n");
    fprintf(stub, "            printf(\"  Result: \");\n");
    fprintf(stub, "            nc_value_print(result, stdout);\n");
    fprintf(stub, "            printf(\"\\n\");\n");
    fprintf(stub, "        }\n");
    fprintf(stub, "        if (vm->output_count > 0)\n");
    fprintf(stub, "            for (int i = 0; i < vm->output_count; i++)\n");
    fprintf(stub, "                printf(\"  %%s\\n\", vm->output[i]);\n");
    fprintf(stub, "        nc_chunk_free(chunk);\n");
    fprintf(stub, "        nc_vm_free(vm);\n");
    fprintf(stub, "    }\n\n");

    fprintf(stub, "    nc_gc_shutdown();\n");
    fprintf(stub, "    return 0;\n");
    fprintf(stub, "}\n");

    fclose(stub);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  2. Runtime stub generation — locate engine source tree
 * ═══════════════════════════════════════════════════════════ */

int nc_build_generate_runtime(const char *self_path, char *nc_dir_out,
                              int nc_dir_size) {
    char resolved_buf[1024];
    char *resolved = nc_realpath(self_path, resolved_buf);
    if (resolved) {
        strncpy(nc_dir_out, resolved, nc_dir_size - 1);
        nc_dir_out[nc_dir_size - 1] = '\0';

        /* Strip binary name: /path/to/build/nc -> /path/to/build */
        char *last_slash = strrchr(nc_dir_out, NC_PATH_SEP);
        if (!last_slash) last_slash = strrchr(nc_dir_out, '/');
        if (last_slash) *last_slash = '\0';

        /* Strip build dir: /path/to/build -> /path/to (engine root) */
        char *build_dir = strrchr(nc_dir_out, NC_PATH_SEP);
        if (!build_dir) build_dir = strrchr(nc_dir_out, '/');
        if (build_dir) *build_dir = '\0';
    } else {
        strncpy(nc_dir_out, ".", nc_dir_size - 1);
        nc_dir_out[nc_dir_size - 1] = '\0';
    }

    /* Verify that the source directory exists */
    char test_path[512];
    snprintf(test_path, sizeof(test_path),
             "%s" NC_PATH_SEP_STR "src" NC_PATH_SEP_STR "nc_vm.c", nc_dir_out);
    struct stat st;
    if (stat(test_path, &st) != 0) {
        fprintf(stderr, "[NC Build] Cannot find engine sources at: %s\n",
                nc_dir_out);
        fprintf(stderr, "  Expected: %s\n", test_path);
        return 1;
    }

    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  3. Native compilation — incremental build + link
 * ═══════════════════════════════════════════════════════════ */

/* Detect which C compiler is available */
static bool build_target_is_windows(const char *target) {
    if (target && target[0]) {
        return strstr(target, "windows") != NULL || strstr(target, "mingw") != NULL;
    }
#ifdef NC_WINDOWS
    return true;
#else
    return false;
#endif
}

static bool build_target_is_linux(const char *target) {
    if (target && target[0]) {
        return strstr(target, "linux") != NULL;
    }
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

static const char *detect_cc(const char *target) {
    const char *env_cc = getenv("NC_CC");
    if (!env_cc || !env_cc[0]) env_cc = getenv("CC");
    if (env_cc && env_cc[0]) return env_cc;

    /* If cross-compiling, try target-specific compilers */
    if (target && target[0]) {
        if (strstr(target, "linux")) {
            /* Try common cross-compiler names */
            if (system("x86_64-linux-gnu-gcc --version >/dev/null 2>&1") == 0)
                return "x86_64-linux-gnu-gcc";
            if (system("aarch64-linux-gnu-gcc --version >/dev/null 2>&1") == 0)
                return "aarch64-linux-gnu-gcc";
        }
        if (strstr(target, "windows") || strstr(target, "mingw")) {
            if (system("x86_64-w64-mingw32-gcc --version >/dev/null 2>&1") == 0)
                return "x86_64-w64-mingw32-gcc";
        }
    }

    /* Try compilers in preference order */
#ifdef NC_WINDOWS
    if (system("gcc --version >/dev/null 2>&1") == 0) return "gcc";
    if (system("clang --version >/dev/null 2>&1") == 0) return "clang";
    if (system("cc --version >/dev/null 2>&1") == 0) return "cc";
#else
    if (system("cc --version >/dev/null 2>&1") == 0) return "cc";
    if (system("clang --version >/dev/null 2>&1") == 0) return "clang";
    if (system("gcc --version >/dev/null 2>&1") == 0) return "gcc";
#endif

    return NULL;
}

static void build_cc_launcher(char *out, int out_size, const char *cc) {
    out[0] = '\0';
    if (!cc || !cc[0]) return;

    if (!strchr(cc, '/') && !strchr(cc, '\\')) {
        snprintf(out, out_size, "%s", cc);
        return;
    }

    char cc_dir[512];
    strncpy(cc_dir, cc, sizeof(cc_dir) - 1);
    cc_dir[sizeof(cc_dir) - 1] = '\0';

    char *slash = strrchr(cc_dir, '\\');
    char *alt_slash = strrchr(cc_dir, '/');
    if (!slash || (alt_slash && alt_slash > slash)) slash = alt_slash;
    if (!slash) {
        snprintf(out, out_size, "\"%s\"", cc);
        return;
    }
    *slash = '\0';

#ifdef NC_WINDOWS
    snprintf(out, out_size, "set \"PATH=%s;%%PATH%%\" && \"%s\"", cc_dir, cc);
#else
    snprintf(out, out_size, "PATH=\"%s:$PATH\" \"%s\"", cc_dir, cc);
#endif
}

/* Build platform-specific compiler flags */
static int build_cc_flags(char *flags, int flags_size,
                          const NcBuildConfig *config) {
    int pos = 0;

    /* Optimization */
    if (config->optimize) {
        pos += snprintf(flags + pos, flags_size - pos, "-O2 -DNDEBUG ");
    } else {
        pos += snprintf(flags + pos, flags_size - pos, "-O0 -g ");
    }

    /* Standard flags */
    pos += snprintf(flags + pos, flags_size - pos,
        "-std=c11 -DNC_NO_REPL "
        "-Wno-unused-function -Wno-unused-variable "
        "-D_GNU_SOURCE ");

    /* Target-specific flags */
    const char *target = config->target;
    bool is_linux = build_target_is_linux(target);
    bool is_windows = build_target_is_windows(target);
    /* bool is_darwin = (!target[0] || strstr(target, "darwin")); */

    if (config->static_link && !is_windows) {
        /* macOS does not support fully static linking via -static;
           for macOS we omit -static and rely on default dynamic linking.
           Linux supports -static just fine. */
        if (is_linux) {
            pos += snprintf(flags + pos, flags_size - pos, "-static ");
        }
    }

    (void)flags_size;
    return pos;
}

/* Build platform-specific linker flags */
static int build_link_flags(char *flags, int flags_size,
                            const NcBuildConfig *config) {
    int pos = 0;
    const char *target = config->target;
    bool is_windows = build_target_is_windows(target);

    if (is_windows) {
        pos += snprintf(flags + pos, flags_size - pos,
            "-lm -lwinhttp -lws2_32 ");
    } else {
        pos += snprintf(flags + pos, flags_size - pos,
            "-lm -lcurl -lpthread -ldl ");
    }

    if (config->strip) {
        pos += snprintf(flags + pos, flags_size - pos, "-s ");
    }

    return pos;
}

int nc_build_compile_native(const char *stub_path, const char *nc_dir,
                            const char *output_binary,
                            const NcBuildConfig *config) {
    const char *cc = detect_cc(config->target);
    if (!cc || !cc[0]) {
        fprintf(stderr, "  [NC Build] No C compiler found.\n");
        fprintf(stderr, "  Set NC_CC or CC, or add gcc/clang/cc to PATH.\n");
        return 1;
    }
    char cc_launcher[1536] = {0};
    char cc_flags[1024] = {0};
    char link_flags[512] = {0};
    build_cc_launcher(cc_launcher, sizeof(cc_launcher), cc);
    build_cc_flags(cc_flags, sizeof(cc_flags), config);
    build_link_flags(link_flags, sizeof(link_flags), config);

    /* Ensure build cache directory exists */
    char build_cache[512];
    snprintf(build_cache, sizeof(build_cache),
             "%s" NC_PATH_SEP_STR "build" NC_PATH_SEP_STR "obj", nc_dir);
    nc_mkdir_p(build_cache);

    int recompiled = 0, skipped = 0;
    int rc = 0;

    /* ── Incremental compile: each runtime .c -> .o ── */
    for (int s = 0; runtime_src_files[s] && rc == 0; s++) {
        char src_path[512], obj_path[512];
        snprintf(src_path, sizeof(src_path),
            "%s" NC_PATH_SEP_STR "src" NC_PATH_SEP_STR "%s",
            nc_dir, runtime_src_files[s]);

        /* Derive .o name from .c name */
        char obj_name[256];
        strncpy(obj_name, runtime_src_files[s], sizeof(obj_name) - 1);
        obj_name[sizeof(obj_name) - 1] = '\0';
        char *dot_c = strrchr(obj_name, '.');
        if (dot_c) memcpy(dot_c, ".o", 3);
        snprintf(obj_path, sizeof(obj_path),
            "%s" NC_PATH_SEP_STR "%s", build_cache, obj_name);

        /* Check if .o is up-to-date */
        struct stat src_stat, obj_stat;
        bool needs_compile = true;
        if (stat(src_path, &src_stat) == 0 && stat(obj_path, &obj_stat) == 0) {
            if (obj_stat.st_mtime >= src_stat.st_mtime) {
                needs_compile = false;
                skipped++;
            }
        }

        if (needs_compile) {
            if (!is_shell_safe(src_path) || !is_shell_safe(obj_path) || !is_shell_safe(nc_dir)) {
                fprintf(stderr, "  [NC Build] Error: unsafe characters in path\n");
                rc = 1;
                break;
            }
            char cc_cmd[2048];
            snprintf(cc_cmd, sizeof(cc_cmd),
                "%s %s -I \"%s/include\" -c \"%s\" -o \"%s\" 2>&1",
                cc_launcher, cc_flags, nc_dir, src_path, obj_path);
            rc = system(cc_cmd);
            if (rc != 0) {
                fprintf(stderr, "  [NC Build] Failed to compile %s\n",
                        runtime_src_files[s]);
                break;
            }
            recompiled++;
        }
    }

    /* ── Compile the stub .c -> .o ── */
    char stub_obj[512];
    snprintf(stub_obj, sizeof(stub_obj),
        "%s" NC_PATH_SEP_STR "_nc_stub.o", build_cache);
    if (rc == 0) {
        if (!is_shell_safe(stub_path) || !is_shell_safe(stub_obj) || !is_shell_safe(nc_dir)) {
            fprintf(stderr, "  [NC Build] Error: unsafe characters in path\n");
            rc = 1;
        } else {
            char cc_cmd[2048];
            snprintf(cc_cmd, sizeof(cc_cmd),
                "%s %s -I \"%s/include\" -c \"%s\" -o \"%s\" 2>&1",
                cc_launcher, cc_flags, nc_dir, stub_path, stub_obj);
            rc = system(cc_cmd);
            if (rc != 0) {
                fprintf(stderr, "  [NC Build] Failed to compile stub\n");
            }
        }
    }

    /* ── Link all .o files together ── */
    if (rc == 0) {
        char link_cmd[8192];
        int pos = snprintf(link_cmd, sizeof(link_cmd),
            "%s \"%s\" ", cc_launcher, stub_obj);

        for (int s = 0; runtime_src_files[s]; s++) {
            char obj_name[256];
            strncpy(obj_name, runtime_src_files[s], sizeof(obj_name) - 1);
            obj_name[sizeof(obj_name) - 1] = '\0';
            char *dot_c = strrchr(obj_name, '.');
            if (dot_c) memcpy(dot_c, ".o", 3);
            pos += snprintf(link_cmd + pos, sizeof(link_cmd) - pos,
                "\"%s" NC_PATH_SEP_STR "%s\" ", build_cache, obj_name);
        }

        /* Append .exe for Windows targets */
        char final_output[520];
        strncpy(final_output, output_binary, sizeof(final_output) - 5);
        final_output[sizeof(final_output) - 5] = '\0';
        if (config->target[0] && strstr(config->target, "windows")) {
            if (!strstr(final_output, ".exe")) {
                strcat(final_output, ".exe");
            }
        }

        pos += snprintf(link_cmd + pos, sizeof(link_cmd) - pos,
            "%s -o \"%s\" 2>&1", link_flags, final_output);

        if (!is_shell_safe(final_output)) {
            fprintf(stderr, "  [NC Build] Error: unsafe characters in output path\n");
            rc = 1;
        } else {
            rc = system(link_cmd);
        }
        if (rc != 0) {
            fprintf(stderr, "  [NC Build] Link failed\n");
        }
    }

    printf("  Compiled: %d files, Cached: %d files\n", recompiled, skipped);
    return rc;
}

/* ═══════════════════════════════════════════════════════════
 *  4. Full pipeline
 * ═══════════════════════════════════════════════════════════ */

int nc_build_run(const char *self_path, const NcBuildConfig *config) {
    /* ── Step 1: Read and compile NC source ── */
    char *source = build_read_file(config->input);
    if (!source) return 1;

    NcLexer *lex = nc_lexer_new(source, config->input);
    nc_lexer_tokenize(lex);

    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count,
                                      config->input);
    NcASTNode *program = nc_parser_parse(parser);
    if (parser->had_error) {
        fprintf(stderr, "[NC Error] %s\n", parser->error_msg);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 1;
    }

    NcCompiler *comp = nc_compiler_new();
    if (!nc_compiler_compile(comp, program)) {
        fprintf(stderr, "[NC Error] %s\n", comp->error_msg);
        nc_compiler_free(comp);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 1;
    }
    nc_optimize_all(comp);

    /* ── Step 2: Generate C stub with embedded bytecode ── */
    char stub_path[512];
    snprintf(stub_path, sizeof(stub_path), "%s%c_nc_build_%d.c",
             nc_tempdir(), NC_PATH_SEP, nc_getpid());

    int rc = nc_build_embed_bytecode(comp, program, stub_path);
    if (rc != 0) {
        nc_compiler_free(comp);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 1;
    }

    /* ── Step 3: Locate engine source directory ── */
    char nc_dir[512];
    rc = nc_build_generate_runtime(self_path, nc_dir, sizeof(nc_dir));
    if (rc != 0) {
        remove(stub_path);
        nc_compiler_free(comp);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 1;
    }

    /* ── Step 4: Print build info ── */
    const char *target_str = config->target[0] ? config->target : "native";
    printf("\n");
    printf("  NC Build Pipeline\n");
    printf("  ─────────────────────────────────\n");
    printf("  Input:    %s\n", config->input);
    printf("  Output:   %s%s\n", config->output,
           (config->target[0] && strstr(config->target, "windows")) ? ".exe" : "");
    printf("  Target:   %s\n", target_str);
    printf("  Optimize: %s\n", config->optimize ? "yes (-O2)" : "no (-O0)");
    printf("  Strip:    %s\n", config->strip ? "yes" : "no");
    printf("  Static:   %s\n", config->static_link ? "yes" : "no");
    printf("  Behaviors: %d\n", comp->chunk_count);
    for (int c = 0; c < comp->chunk_count; c++)
        printf("    - %s (%d bytes bytecode)\n",
               comp->beh_names[c]->chars, comp->chunks[c].count);
    printf("\n");

    /* ── Step 5: Compile native binary ── */
    printf("  Building %s ...\n", config->output);
    rc = nc_build_compile_native(stub_path, nc_dir, config->output, config);

    /* Clean up temp file */
    remove(stub_path);

    if (rc == 0) {
        printf("\n");
        printf("  Built: ./%s\n", config->output);
        printf("  Run:   ./%s\n", config->output);
        printf("  Help:  ./%s --help\n\n", config->output);
    } else {
        fprintf(stderr, "\n  [NC Build] Build failed (cc returned %d)\n", rc);
    }

    nc_compiler_free(comp);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);

    return rc != 0 ? 1 : 0;
}
