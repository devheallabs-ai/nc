/*
 * nc_migrate.c — AI-powered code migration engine with hybrid support.
 *
 * Converts code from ANY language to NC using the configured AI engine.
 * Unlike the pattern-matching digester (nc_polyglot.c), this understands
 * semantics: it knows when to convert code to NC and when to wrap it
 * as a hybrid call (e.g., ML models stay in Python, NC orchestrates).
 *
 * Usage:
 *   nc migrate app.py              → AI-powered conversion to app.nc
 *   nc migrate src/                → Convert all files in directory
 *   nc migrate model.py --hybrid   → Wrap as hybrid (don't convert logic)
 *   nc migrate app.java --dry-run  → Preview without writing
 *   nc migrate code.txt --lang py  → Override language detection
 *
 * The migration engine uses NC's own AI bridge (nc_ai_call), so it works
 * with whatever AI provider is configured — no hardcoded APIs.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"

/* ═══════════════════════════════════════════════════════════
 *  Migration system prompt — the core intelligence
 * ═══════════════════════════════════════════════════════════ */

static const char *NC_MIGRATE_SYSTEM_PROMPT =
    "You are an expert code migration assistant for NC (Notation-as-Code). "
    "NC is a plain-English programming language. Convert the given source code to valid NC.\n\n"
    "NC SYNTAX RULES:\n"
    "- Variables: set name to value\n"
    "- Functions: to behavior_name with param1 and param2:\n"
    "- Return: respond with value\n"
    "- Print: show value\n"
    "- If: if condition:\\n    ...\\notherwise:\\n    ...\n"
    "- Loops: repeat for each item in list:\\n    ...\n"
    "- Loop N: repeat 10 times:\\n    ...\n"
    "- While: while condition:\\n    ...\n"
    "- Lists: set items to [1, 2, 3]\n"
    "- Records: set user to {name: \"Alice\", age: 30}\n"
    "- Try: try:\\n    ...\\non error e:\\n    ...\n"
    "- Match: match value:\\n    when \"a\":\\n        ...\n"
    "- AI: ask AI to \"prompt\" using context:\\n        save as: result\n"
    "- HTTP: gather from \"url\":\\n    save as: data\n"
    "- Import: import \"module\"\n"
    "- Service: service \"name\"\\nversion \"1.0.0\"\n"
    "- Comments: // comment\n"
    "- String templates: \"Hello {{name}}\"\n"
    "- Dot access: user.name\n"
    "- Append: append item to list\n"
    "- Remove: remove item from list\n"
    "- Natural checks: is empty, is not empty, contains, starts_with, ends_with\n"
    "- Comparisons: is above, is below, is at least, is at most\n"
    "- Built-ins: len, upper, lower, trim, split, join, replace, sort, reverse, "
    "range, sum, average, unique, flatten, type, str, int, float, abs, sqrt, "
    "round, min, max, random, read_file, write_file, json_encode, json_decode, "
    "csv_parse, env, exec, shell, time_now, time_ms, input, print\n\n"
    "RULES:\n"
    "1. Output ONLY valid NC code. No markdown, no explanations, no code fences.\n"
    "2. Add a service header and version.\n"
    "3. Convert classes to define blocks or behaviors.\n"
    "4. Convert methods/functions to 'to' behaviors.\n"
    "5. Use NC idioms — 'show' not 'print', 'respond with' not 'return'.\n"
    "6. Convert language-specific patterns to NC equivalents.\n"
    "7. Add a '// Migrated from: <filename>' comment at the top.\n"
    "8. If something cannot be directly expressed in NC, add a TODO comment.\n";

static const char *NC_HYBRID_SYSTEM_PROMPT =
    "You are an expert code migration assistant for NC (Notation-as-Code). "
    "NC is a plain-English programming language with hybrid execution support.\n\n"
    "The user wants to WRAP this code for hybrid use — NC will orchestrate it, "
    "but the heavy computation stays in the original language.\n\n"
    "Generate NC code that:\n"
    "1. Uses 'shell' or 'exec' to call the original script/binary\n"
    "2. Passes inputs via command-line args, stdin, or temp files\n"
    "3. Parses the output (JSON preferred) back into NC values\n"
    "4. Wraps each public function/class as an NC behavior\n"
    "5. Handles errors with try/on error\n"
    "6. For ML models: use load_model/predict/unload_model when possible, "
    "otherwise shell out to python/java/node\n\n"
    "NC HYBRID PATTERNS:\n"
    "- Run Python:  set result to exec(\"python3 script.py --input {{data}}\")\n"
    "- Run Java:    set result to exec(\"java -jar app.jar {{args}}\")\n"
    "- Run Node:    set result to exec(\"node script.js {{args}}\")\n"
    "- Parse JSON:  set parsed to json_decode(result)\n"
    "- Temp files:  write_file(\"/tmp/input.json\", json_encode(data))\n"
    "               set result to exec(\"python3 process.py /tmp/input.json\")\n"
    "- ML predict:  load_model(\"model.onnx\") as model\\n"
    "               set prediction to predict(model, input_data)\n\n"
    "Output ONLY valid NC code. No markdown, no explanations, no code fences.\n"
    "Add '// Hybrid wrapper for: <filename>' at the top.\n"
    "Add '// Original runtime: <language>' below it.\n"
    "Add '// Requires: <runtime> installed' as a dependency note.\n";

/* ═══════════════════════════════════════════════════════════
 *  Language detection
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *ext;
    const char *name;
    const char *runtime;
} LangInfo;

static const LangInfo known_languages[] = {
    {".py",    "Python",      "python3"},
    {".java",  "Java",        "java"},
    {".js",    "JavaScript",  "node"},
    {".ts",    "TypeScript",  "npx ts-node"},
    {".go",    "Go",          "go run"},
    {".rs",    "Rust",        "cargo run"},
    {".rb",    "Ruby",        "ruby"},
    {".php",   "PHP",         "php"},
    {".cs",    "C#",          "dotnet run"},
    {".swift", "Swift",       "swift"},
    {".kt",    "Kotlin",      "kotlin"},
    {".scala", "Scala",       "scala"},
    {".r",     "R",           "Rscript"},
    {".R",     "R",           "Rscript"},
    {".lua",   "Lua",         "lua"},
    {".pl",    "Perl",        "perl"},
    {".sh",    "Shell",       "bash"},
    {".bash",  "Shell",       "bash"},
    {".c",     "C",           "gcc"},
    {".cpp",   "C++",         "g++"},
    {".h",     "C/C++",       "gcc"},
    {".hpp",   "C++",         "g++"},
    {".yaml",  "YAML",        NULL},
    {".yml",   "YAML",        NULL},
    {".json",  "JSON",        NULL},
    {".toml",  "TOML",        NULL},
    {".xml",   "XML",         NULL},
    {".sql",   "SQL",         NULL},
    {NULL, NULL, NULL}
};

static const LangInfo *detect_language(const char *filename, const char *lang_override) {
    if (lang_override && lang_override[0]) {
        for (const LangInfo *l = known_languages; l->ext; l++) {
            if (strcasecmp(lang_override, l->name) == 0) return l;
            if (strcasecmp(lang_override, l->ext + 1) == 0) return l;
        }
    }

    const char *ext = strrchr(filename, '.');
    if (!ext) return NULL;

    for (const LangInfo *l = known_languages; l->ext; l++) {
        if (strcmp(ext, l->ext) == 0) return l;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  Hybrid detection — should this code be wrapped, not converted?
 * ═══════════════════════════════════════════════════════════ */

static bool looks_like_ml_code(const char *source) {
    const char *ml_markers[] = {
        "import torch", "import tensorflow", "import keras",
        "import sklearn", "import xgboost", "import lightgbm",
        "import pandas", "import numpy", "from torch",
        "from tensorflow", "from keras", "from sklearn",
        "model.fit(", "model.train(", "model.predict(",
        "nn.Module", "tf.keras", "Sequential(",
        "DataLoader", "Dataset", "optimizer.step(",
        "loss.backward(", "torch.tensor", "np.array(",
        "pd.DataFrame", "pd.read_csv",
        NULL
    };
    for (const char **m = ml_markers; *m; m++) {
        if (strstr(source, *m)) return true;
    }
    return false;
}

static bool looks_like_heavy_compute(const char *source) {
    const char *compute_markers[] = {
        "cuda()", ".to(device)", "gpu", "GPU",
        "multiprocessing", "threading.Thread",
        "ProcessPoolExecutor", "ThreadPoolExecutor",
        "numba", "cython", "cffi", "ctypes",
        "opencv", "cv2.", "PIL.", "Image.",
        "scipy.", "sympy.",
        NULL
    };
    for (const char **m = compute_markers; *m; m++) {
        if (strstr(source, *m)) return true;
    }
    return false;
}

static bool looks_like_web_framework(const char *source) {
    /* Detects Express/Flask/FastAPI/Django/Rails/Spring Boot/Laravel.
     * These frameworks have their own routing and middleware layers —
     * stripping them out during direct conversion loses critical behaviour.
     * Hybrid mode is better: NC orchestrates, the framework stays intact. */
    const char *web_markers[] = {
        /* Express / Node */
        "express()", "app.get(", "app.post(", "app.put(", "app.delete(",
        "app.use(", "router.get(", "router.post(", "req, res",
        "res.json(", "res.send(", "res.render(",
        /* Flask */
        "@app.route(", "from flask import", "import flask",
        "Flask(__name__)", "jsonify(", "render_template(",
        /* FastAPI */
        "from fastapi import", "import fastapi",
        "FastAPI()", "@app.get(", "@app.post(", "@router.get(", "@router.post(",
        /* Django */
        "from django", "import django", "urlpatterns", "HttpResponse(",
        "render(request,", "get_object_or_404(",
        /* Rails (Ruby) */
        "ActionController", "ActionDispatch", "before_action",
        "render json:", "respond_to do",
        /* Spring Boot (Java) */
        "@RestController", "@GetMapping", "@PostMapping", "@RequestMapping",
        "@Autowired", "ResponseEntity",
        /* Laravel (PHP) */
        "use Illuminate", "Route::get(", "Route::post(",
        "Artisan::", "Eloquent",
        NULL
    };
    for (const char **m = web_markers; *m; m++) {
        if (strstr(source, *m)) return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  Build the migration prompt
 * ═══════════════════════════════════════════════════════════ */

static char *build_migration_prompt(const char *source, const char *filename,
                                    const LangInfo *lang, bool hybrid) {
    NcDynBuf buf;
    nc_dbuf_init(&buf, 4096);

    if (hybrid) {
        nc_dbuf_append(&buf, NC_HYBRID_SYSTEM_PROMPT);
    } else {
        nc_dbuf_append(&buf, NC_MIGRATE_SYSTEM_PROMPT);
    }

    nc_dbuf_append(&buf, "\n--- SOURCE CODE ---\n");
    nc_dbuf_append(&buf, "File: ");
    nc_dbuf_append(&buf, filename);
    nc_dbuf_append(&buf, "\nLanguage: ");
    nc_dbuf_append(&buf, lang ? lang->name : "Unknown");
    if (hybrid && lang && lang->runtime) {
        nc_dbuf_append(&buf, "\nRuntime: ");
        nc_dbuf_append(&buf, lang->runtime);
    }
    nc_dbuf_append(&buf, "\n\n");
    nc_dbuf_append(&buf, source);
    nc_dbuf_append(&buf, "\n--- END SOURCE CODE ---\n");

    char *result = strdup(buf.data);
    nc_dbuf_free(&buf);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Output path generation
 * ═══════════════════════════════════════════════════════════ */

static void make_output_path(char *outpath, int outpath_size,
                             const char *filename, const char *out_dir) {
    if (out_dir && out_dir[0]) {
        const char *base = nc_last_path_sep(filename);
        base = base ? base + 1 : filename;
        snprintf(outpath, outpath_size, "%s" NC_PATH_SEP_STR "%s", out_dir, base);
    } else {
        strncpy(outpath, filename, outpath_size - 1);
        outpath[outpath_size - 1] = '\0';
    }

    char *dot = strrchr(outpath, '.');
    if (dot && (dot - outpath) + 4 < outpath_size)
        memcpy(dot, ".nc", 4);
    else
        strncat(outpath, ".nc", outpath_size - strlen(outpath) - 1);
}

/* ═══════════════════════════════════════════════════════════
 *  Strip markdown fences from AI response
 * ═══════════════════════════════════════════════════════════ */

static char *strip_code_fences(const char *code) {
    if (!code) return NULL;

    const char *start = code;
    const char *end = code + strlen(code);

    while (*start == ' ' || *start == '\n' || *start == '\r') start++;
    if (strncmp(start, "```", 3) == 0) {
        start += 3;
        while (*start && *start != '\n') start++;
        if (*start == '\n') start++;
    }

    while (end > start) {
        char c = *(end - 1);
        if (c == ' ' || c == '\n' || c == '\r') { end--; continue; }
        break;
    }
    if (end - start >= 3 && strncmp(end - 3, "```", 3) == 0) {
        end -= 3;
        while (end > start && (*(end-1) == '\n' || *(end-1) == '\r')) end--;
    }

    int len = (int)(end - start);
    char *result = malloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Core: migrate a single file
 * ═══════════════════════════════════════════════════════════ */

static int migrate_single_file(const char *filename, bool dry_run, bool hybrid,
                               const char *lang_override, const char *out_dir) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "  [error] Cannot open: %s\n", filename);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    size_t nread = fread(source, 1, size, f);
    source[nread] = '\0';
    fclose(f);

    const LangInfo *lang = detect_language(filename, lang_override);
    if (!lang) {
        fprintf(stderr, "  [error] Unknown language for: %s\n", filename);
        fprintf(stderr, "          Use --lang to specify (e.g., --lang python)\n");
        free(source);
        return 1;
    }

    bool auto_hybrid = !hybrid && (looks_like_ml_code(source) ||
                                     looks_like_heavy_compute(source) ||
                                     looks_like_web_framework(source));
    if (auto_hybrid) {
        const char *reason = looks_like_ml_code(source)       ? "ML framework"  :
                             looks_like_heavy_compute(source)  ? "heavy compute" :
                                                                 "web framework";
        printf("  [hybrid] %s detected in %s — wrapping instead of converting\n",
               reason, filename);
        hybrid = true;
    }

    printf("  [migrate] %s (%s)%s\n", filename, lang->name,
           hybrid ? " [hybrid]" : "");

    char *prompt = build_migration_prompt(source, filename, lang, hybrid);

    nc_http_init();
    NcValue result = nc_ai_call(prompt, NULL, NULL);
    nc_http_cleanup();
    free(prompt);

    if (!IS_MAP(result)) {
        fprintf(stderr, "  [error] AI call failed for: %s\n", filename);
        free(source);
        return 1;
    }

    NcMap *rmap = AS_MAP(result);
    NcValue ok_val = nc_map_get(rmap, nc_string_from_cstr("ok"));
    if (IS_BOOL(ok_val) && !AS_BOOL(ok_val)) {
        NcValue raw = nc_map_get(rmap, nc_string_from_cstr("raw"));
        fprintf(stderr, "  [error] AI error for %s: %s\n", filename,
                IS_STRING(raw) ? AS_STRING(raw)->chars : "unknown error");
        free(source);
        return 1;
    }

    NcValue response = nc_map_get(rmap, nc_string_from_cstr("response"));
    if (!IS_STRING(response)) {
        fprintf(stderr, "  [error] Empty AI response for: %s\n", filename);
        free(source);
        return 1;
    }

    char *nc_code = strip_code_fences(AS_STRING(response)->chars);

    if (dry_run) {
        printf("\n── %s → (dry run) ──\n", filename);
        printf("%s\n", nc_code);
        printf("── end ──\n\n");
    } else {
        char outpath[512];
        make_output_path(outpath, sizeof(outpath), filename, out_dir);

        FILE *out = fopen(outpath, "w");
        if (!out) {
            fprintf(stderr, "  [error] Cannot write: %s\n", outpath);
            free(nc_code);
            free(source);
            return 1;
        }
        fputs(nc_code, out);
        fputs("\n", out);
        fclose(out);

        printf("  [done] %s → %s\n", filename, outpath);
    }

    free(nc_code);
    free(source);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Directory migration — walk and convert all files
 * ═══════════════════════════════════════════════════════════ */

static int migrate_directory(const char *dirpath, bool dry_run, bool hybrid,
                             const char *lang_override, const char *out_dir) {
    nc_dir_t *dir = nc_opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "  [error] Cannot open directory: %s\n", dirpath);
        return 1;
    }

    int total = 0, success = 0, skipped = 0, failed = 0;
    nc_dirent_t entry;

    printf("\n  Scanning %s for source files...\n\n", dirpath);

    while (nc_readdir(dir, &entry)) {
        if (entry.name[0] == '.') continue;

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s" NC_PATH_SEP_STR "%s", dirpath, entry.name);

        struct stat st;
        if (stat(filepath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            int sub = migrate_directory(filepath, dry_run, hybrid, lang_override, out_dir);
            if (sub != 0) failed++;
            continue;
        }

        if (!S_ISREG(st.st_mode)) continue;

        const char *ext = strrchr(entry.name, '.');
        if (!ext) { skipped++; continue; }

        if (strcmp(ext, ".nc") == 0) { skipped++; continue; }

        const LangInfo *lang = detect_language(entry.name, lang_override);
        if (!lang) { skipped++; continue; }

        total++;
        int r = migrate_single_file(filepath, dry_run, hybrid, lang_override, out_dir);
        if (r == 0) success++;
        else failed++;
    }

    nc_closedir(dir);

    printf("\n  Migration summary:\n");
    printf("    Total files:  %d\n", total);
    printf("    Converted:    %d\n", success);
    printf("    Failed:       %d\n", failed);
    printf("    Skipped:      %d (unrecognized or .nc)\n\n", skipped);

    return failed > 0 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Public API — nc migrate <file-or-dir> [flags]
 * ═══════════════════════════════════════════════════════════ */

int nc_migrate(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: nc migrate <file-or-dir> [options]\n\n");
        printf("  Options:\n");
        printf("    --dry-run       Preview conversion without writing files\n");
        printf("    --hybrid        Wrap code for hybrid execution (don't convert logic)\n");
        printf("    --lang <name>   Override language detection (python, java, go, etc.)\n");
        printf("    --out <dir>     Output directory (default: alongside source)\n");
        printf("    --plan          Analyze project and show migration plan only\n\n");
        printf("  Examples:\n");
        printf("    nc migrate app.py                 Convert Python to NC\n");
        printf("    nc migrate model.py --hybrid      Wrap ML model for NC orchestration\n");
        printf("    nc migrate src/ --dry-run         Preview all conversions\n");
        printf("    nc migrate code.txt --lang java   Force Java detection\n");
        printf("    nc migrate project/ --out nc_out/ Write all to nc_out/\n\n");
        printf("  Hybrid mode:\n");
        printf("    When --hybrid is set (or auto-detected for ML/compute code),\n");
        printf("    NC generates wrapper behaviors that shell out to the original\n");
        printf("    runtime. The heavy logic stays in Python/Java/Go — NC orchestrates.\n\n");
        printf("  Auto-hybrid detection:\n");
        printf("    Files with ML frameworks (torch, tensorflow, sklearn, etc.)\n");
        printf("    or heavy compute (CUDA, multiprocessing, OpenCV, etc.)\n");
        printf("    are automatically wrapped instead of converted.\n\n");
        printf("  Note: Requires an AI provider configured via NC_AI_URL/NC_AI_KEY\n");
        printf("  or nc_ai_providers.json. For offline conversion, use 'nc digest'.\n");
        return 1;
    }

    const char *target = argv[2];
    bool dry_run = false;
    bool hybrid = false;
    bool plan_only = false;
    const char *lang_override = NULL;
    const char *out_dir = NULL;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0) dry_run = true;
        else if (strcmp(argv[i], "--hybrid") == 0) hybrid = true;
        else if (strcmp(argv[i], "--plan") == 0) plan_only = true;
        else if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc) lang_override = argv[++i];
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_dir = argv[++i];
    }

    printf("\n  NC Migration Engine v1.0\n");
    printf("  ========================\n\n");

    struct stat st;
    if (stat(target, &st) != 0) {
        fprintf(stderr, "  [error] Cannot access: %s\n", target);
        return 1;
    }

    if (plan_only) {
        printf("  Migration plan for: %s\n\n", target);

        if (S_ISDIR(st.st_mode)) {
            nc_dir_t *dir = nc_opendir(target);
            if (!dir) { fprintf(stderr, "  [error] Cannot open: %s\n", target); return 1; }
            nc_dirent_t entry;
            int convert_count = 0, hybrid_count = 0, skip_count = 0;

            while (nc_readdir(dir, &entry)) {
                if (entry.name[0] == '.') continue;
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s" NC_PATH_SEP_STR "%s", target, entry.name);
                struct stat fst;
                if (stat(filepath, &fst) != 0 || !S_ISREG(fst.st_mode)) continue;

                const LangInfo *lang = detect_language(entry.name, lang_override);
                if (!lang) { skip_count++; continue; }

                FILE *f = fopen(filepath, "rb");
                if (!f) { skip_count++; continue; }
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                fseek(f, 0, SEEK_SET);
                char *src = malloc(sz + 1);
                size_t n = fread(src, 1, sz, f);
                src[n] = '\0';
                fclose(f);

                bool is_hybrid_candidate = looks_like_ml_code(src) ||
                                           looks_like_heavy_compute(src) ||
                                           looks_like_web_framework(src);
                free(src);

                if (is_hybrid_candidate) {
                    printf("    [hybrid]  %s (%s) — ML/compute/web framework detected, will wrap\n",
                           entry.name, lang->name);
                    hybrid_count++;
                } else {
                    printf("    [convert] %s (%s) — will convert to NC\n",
                           entry.name, lang->name);
                    convert_count++;
                }
            }
            nc_closedir(dir);

            printf("\n  Plan summary:\n");
            printf("    Convert to NC:  %d files\n", convert_count);
            printf("    Hybrid wrap:    %d files\n", hybrid_count);
            printf("    Skip:           %d files\n", skip_count);
            printf("    Total:          %d files\n\n", convert_count + hybrid_count + skip_count);
        } else {
            const LangInfo *lang = detect_language(target, lang_override);
            if (!lang) {
                printf("    [skip] %s — unrecognized language\n", target);
            } else {
                FILE *f = fopen(target, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long sz = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    char *src = malloc(sz + 1);
                    size_t n = fread(src, 1, sz, f);
                    src[n] = '\0';
                    fclose(f);
                    bool is_hybrid_candidate = looks_like_ml_code(src) ||
                                               looks_like_heavy_compute(src) ||
                                               looks_like_web_framework(src);
                    free(src);
                    if (is_hybrid_candidate)
                        printf("    [hybrid]  %s (%s) — ML/compute/web framework detected\n", target, lang->name);
                    else
                        printf("    [convert] %s (%s)\n", target, lang->name);
                }
            }
        }
        return 0;
    }

    if (out_dir) {
        struct stat ost;
        if (stat(out_dir, &ost) != 0) {
            if (nc_mkdir(out_dir) != 0) {
                fprintf(stderr, "  [error] Cannot create output directory: %s\n", out_dir);
                return 1;
            }
            printf("  Created output directory: %s\n\n", out_dir);
        }
    }

    if (S_ISDIR(st.st_mode)) {
        return migrate_directory(target, dry_run, hybrid, lang_override, out_dir);
    } else {
        return migrate_single_file(target, dry_run, hybrid, lang_override, out_dir);
    }
}
