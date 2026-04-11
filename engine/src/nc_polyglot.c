/*
 * nc_polyglot.c — Cross-language digestion engine.
 *
 * Reads Python, JavaScript, YAML, and JSON source code and
 * converts it into NC (.nc) format. This is what makes NC
 * a "universal language" — it can absorb code from other languages.
 *
 * Usage:
 *   nc digest app.py           → generates app.nc
 *   nc digest server.js        → generates server.nc
 *   nc digest config.yaml      → generates config.nc
 *   nc digest api.json          → generates api.nc
 *
 * The digester doesn't need a full parser for each language —
 * it uses pattern recognition to extract structure and intent.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"

typedef struct {
    char *output;
    int   len;
    int   cap;
} NCWriter;

static void w_init(NCWriter *w) {
    w->cap = 4096;
    w->output = malloc(w->cap);
    w->output[0] = '\0';
    w->len = 0;
}

static void w_write(NCWriter *w, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    while (w->len + n + 1 >= w->cap) {
        w->cap *= 2;
        w->output = realloc(w->output, w->cap);
    }
    memcpy(w->output + w->len, buf, n);
    w->len += n;
    w->output[w->len] = '\0';
}

/* ═══════════════════════════════════════════════════════════
 *  Python → NC converter
 * ═══════════════════════════════════════════════════════════ */

static char *digest_python(const char *source, const char *filename) {
    NCWriter w;
    w_init(&w);

    w_write(&w, "// Auto-converted from Python: %s\n", filename);
    w_write(&w, "// Review and adjust as needed\n\n");

    /* Extract module name from filename */
    const char *base = nc_last_path_sep(filename);
    base = base ? base + 1 : filename;
    char module[128] = {0};
    strncpy(module, base, sizeof(module) - 1);
    char *dot = strrchr(module, '.');
    if (dot) *dot = '\0';

    w_write(&w, "service \"%s\"\n", module);
    w_write(&w, "version \"1.0.0\"\n\n");

    /* Scan for patterns */
    const char *p = source;
    int line_num = 0;
    bool in_class = false;
    char current_class[128] = {0};

    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        int line_len = (int)(eol - p);
        line_num++;

        /* Skip blank lines and comments */
        const char *content = p;
        while (content < eol && (*content == ' ' || *content == '\t')) content++;

        /* class ClassName: → define ClassName as: */
        if (strncmp(content, "class ", 6) == 0) {
            char name[128] = {0};
            const char *n = content + 6;
            int ni = 0;
            while (n < eol && *n != '(' && *n != ':' && ni < 126) name[ni++] = *n++;
            while (ni > 0 && name[ni-1] == ' ') ni--;
            name[ni] = '\0';
            w_write(&w, "define %s as:\n", name);
            in_class = true;
            strncpy(current_class, name, sizeof(current_class) - 1); current_class[sizeof(current_class) - 1] = '\0';
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* async def / def → async to / to */
        if (strncmp(content, "def ", 4) == 0 ||
            strncmp(content, "async def ", 10) == 0) {
            bool fn_async = (strncmp(content, "async ", 6) == 0);
            char fname[128] = {0};
            const char *n = fn_async ? content + 10 : content + 4;
            int ni = 0;
            while (n < eol && *n != '(' && ni < 126) fname[ni++] = *n++;
            fname[ni] = '\0';

            /* Extract parameters */
            char params[256] = {0};
            if (*n == '(') {
                n++;
                int pi = 0;
                while (n < eol && *n != ')' && pi < 254) {
                    if (*n == ':') { /* skip type annotations */
                        while (n < eol && *n != ',' && *n != ')') n++;
                        continue;
                    }
                    if (strncmp(n, "self", 4) == 0) { n += 4; if (*n == ',') n++; while (*n == ' ') n++; continue; }
                    params[pi++] = *n++;
                }
            }

            /* Skip __init__, __str__, etc. */
            if (fname[0] == '_' && fname[1] == '_') {
                if (in_class && strcmp(fname, "__init__") == 0) {
                    /* Convert __init__ params to type fields */
                    /* Scan body for self.x = x patterns */
                    p = (*eol == '\n') ? eol + 1 : eol;
                    while (*p) {
                        const char *bl = p;
                        while (bl < p + 200 && (*bl == ' ' || *bl == '\t')) bl++;
                        if (strncmp(bl, "self.", 5) == 0) {
                            char field[64] = {0};
                            const char *fn = bl + 5;
                            int fi = 0;
                            while (*fn && *fn != ' ' && *fn != '=' && fi < 62) field[fi++] = *fn++;
                            field[fi] = '\0';
                            w_write(&w, "    %s is text\n", field);
                        }
                        while (*p && *p != '\n') p++;
                        if (*p == '\n') p++;
                        /* Check if next line is still indented */
                        if (*p && *p != ' ' && *p != '\t') break;
                    }
                    w_write(&w, "\n");
                    continue;
                }
                p = (*eol == '\n') ? eol + 1 : eol;
                continue;
            }

            if (params[0]) {
                /* Clean up params: remove defaults, type hints */
                char clean_params[256] = {0};
                int ci = 0;
                for (int i = 0; params[i]; i++) {
                    if (params[i] == '=') { while (params[i] && params[i] != ',') i++; if (!params[i]) break; }
                    if (params[i] == ' ' && ci > 0 && clean_params[ci-1] == ' ') continue;
                    clean_params[ci++] = params[i];
                }
                /* Replace , with " and " */
                w_write(&w, fn_async ? "async to %s with " : "to %s with ", fname);
                for (int i = 0; clean_params[i]; i++) {
                    if (clean_params[i] == ',') { w_write(&w, " and "); while (clean_params[i+1] == ' ') i++; }
                    else w_write(&w, "%c", clean_params[i]);
                }
                w_write(&w, ":\n");
            } else {
                w_write(&w, fn_async ? "async to %s:\n" : "to %s:\n", fname);
            }
            in_class = false;
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* return x → respond with x */
        if (strncmp(content, "return ", 7) == 0) {
            w_write(&w, "    respond with %.*s\n", (int)(eol - content - 7), content + 7);
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* await expr → gather expr */
        if (strncmp(content, "await ", 6) == 0) {
            w_write(&w, "    gather %.*s\n", (int)(eol - content - 6), content + 6);
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* yield expr → yield expr (NC has native yield) */
        if (strncmp(content, "yield ", 6) == 0) {
            w_write(&w, "    yield %.*s\n", (int)(eol - content - 6), content + 6);
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* print(...) → show ... */
        if (strncmp(content, "print(", 6) == 0) {
            const char *arg = content + 6;
            int alen = (int)(eol - arg);
            if (alen > 0 && arg[alen - 1] == ')') alen--;
            w_write(&w, "    show %.*s\n", alen, arg);
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }

        /* x = value → set x to value */
        if (content < eol) {
            const char *eq = NULL;
            for (const char *s = content; s < eol; s++) {
                if (*s == '=' && s > content && *(s-1) != '!' && *(s-1) != '<' &&
                    *(s-1) != '>' && (s+1 < eol && *(s+1) != '=')) {
                    eq = s; break;
                }
            }
            if (eq && eq > content) {
                int vname_len = (int)(eq - content);
                while (vname_len > 0 && content[vname_len-1] == ' ') vname_len--;
                const char *val = eq + 1;
                while (val < eol && *val == ' ') val++;
                w_write(&w, "    set %.*s to %.*s\n", vname_len, content, (int)(eol - val), val);
                p = (*eol == '\n') ? eol + 1 : eol;
                continue;
            }
        }

        /* if/for/import — copy with NC adjustments */
        if (strncmp(content, "if ", 3) == 0) {
            w_write(&w, "    if %.*s\n", line_len - (int)(content - p), content);
        } else if (strncmp(content, "for ", 4) == 0) {
            w_write(&w, "    // TODO: convert loop: %.*s\n", (int)(eol - content), content);
        } else if (strncmp(content, "import ", 7) == 0) {
            w_write(&w, "// import %.*s\n", (int)(eol - content - 7), content + 7);
        } else if (content < eol && *content != '#') {
            w_write(&w, "    // %.*s\n", (int)(eol - content), content);
        }

        p = (*eol == '\n') ? eol + 1 : eol;
    }

    return w.output;
}

/* ═══════════════════════════════════════════════════════════
 *  JavaScript → NC converter
 * ═══════════════════════════════════════════════════════════ */

static char *digest_javascript(const char *source, const char *filename) {
    NCWriter w;
    w_init(&w);

    const char *base = nc_last_path_sep(filename);
    base = base ? base + 1 : filename;
    char module[128] = {0};
    strncpy(module, base, sizeof(module) - 1);
    char *dot = strrchr(module, '.');
    if (dot) *dot = '\0';

    w_write(&w, "// Auto-converted from JavaScript: %s\n\n", filename);
    w_write(&w, "service \"%s\"\n", module);
    w_write(&w, "version \"1.0.0\"\n\n");

    const char *p = source;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        const char *content = p;
        while (content < eol && (*content == ' ' || *content == '\t')) content++;

        /* function name(args) { → to name:
         * async function name(args) { → async to name: */
        if (strncmp(content, "function ", 9) == 0 ||
            strncmp(content, "async function ", 15) == 0) {
            bool js_async = (strncmp(content, "async ", 6) == 0);
            const char *n = strstr(content, "function ") + 9;
            char fname[128] = {0};
            int fi = 0;
            while (n < eol && *n != '(' && fi < 126) fname[fi++] = *n++;
            fname[fi] = '\0';
            w_write(&w, js_async ? "async to %s:\n" : "to %s:\n", fname);
        }
        /* await expr → gather expr */
        else if (strncmp(content, "await ", 6) == 0) {
            w_write(&w, "    gather %.*s\n", (int)(eol - content - 6), content + 6);
        }
        /* yield expr → yield expr (NC native) */
        else if (strncmp(content, "yield ", 6) == 0) {
            w_write(&w, "    yield %.*s\n", (int)(eol - content - 6), content + 6);
        }
        /* Promise.then(fn) → continuation comment — no direct NC equivalent */
        else if (strstr(content, ".then(") != NULL) {
            w_write(&w, "    // TODO: convert Promise chain to: gather <promise>\n");
        }
        /* const/let/var name = → set name to */
        else if (strncmp(content, "const ", 6) == 0 ||
                 strncmp(content, "let ", 4) == 0 ||
                 strncmp(content, "var ", 4) == 0) {
            const char *n = content;
            while (n < eol && *n != ' ') n++;
            while (n < eol && *n == ' ') n++;
            const char *eq = strchr(n, '=');
            if (eq && eq < eol) {
                int nlen = (int)(eq - n);
                while (nlen > 0 && n[nlen-1] == ' ') nlen--;
                const char *val = eq + 1;
                while (val < eol && *val == ' ') val++;
                int vlen = (int)(eol - val);
                if (vlen > 0 && val[vlen-1] == ';') vlen--;
                w_write(&w, "    set %.*s to %.*s\n", nlen, n, vlen, val);
            }
        }
        /* console.log → show */
        else if (strstr(content, "console.log") != NULL) {
            const char *arg = strchr(content, '(');
            if (arg) {
                arg++;
                const char *end = strrchr(arg, ')');
                if (end) w_write(&w, "    show %.*s\n", (int)(end - arg), arg);
            }
        }
        /* return → respond with */
        else if (strncmp(content, "return ", 7) == 0) {
            int vlen = (int)(eol - content - 7);
            if (vlen > 0 && content[6 + vlen] == ';') vlen--;
            w_write(&w, "    respond with %.*s\n", vlen, content + 7);
        }
        else if (content < eol && *content != '/' && *content != '{' && *content != '}' && *content != '*') {
            w_write(&w, "    // %.*s\n", (int)(eol - content), content);
        }

        p = (*eol == '\n') ? eol + 1 : eol;
    }

    return w.output;
}

/* ═══════════════════════════════════════════════════════════
 *  YAML → NC converter
 * ═══════════════════════════════════════════════════════════ */

static char *digest_yaml(const char *source, const char *filename) {
    NCWriter w;
    w_init(&w);

    w_write(&w, "// Auto-converted from YAML: %s\n\n", filename);
    w_write(&w, "service \"yaml-config\"\n");
    w_write(&w, "version \"1.0.0\"\n\n");
    w_write(&w, "configure:\n");

    const char *p = source;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        const char *content = p;
        while (content < eol && (*content == ' ' || *content == '\t')) content++;
        int indent = (int)(content - p);

        if (content < eol && *content != '#' && *content != '-') {
            const char *colon = strchr(content, ':');
            if (colon && colon < eol) {
                int klen = (int)(colon - content);
                const char *val = colon + 1;
                while (val < eol && *val == ' ') val++;
                int vlen = (int)(eol - val);

                int nc_indent = (indent / 2) + 1;
                for (int i = 0; i < nc_indent; i++) w_write(&w, "    ");

                if (vlen > 0)
                    w_write(&w, "%.*s: %.*s\n", klen, content, vlen, val);
                else
                    w_write(&w, "%.*s:\n", klen, content);
            }
        } else if (content < eol && *content == '-') {
            /* List item */
            content++;
            while (content < eol && *content == ' ') content++;
            int nc_indent = (indent / 2) + 1;
            for (int i = 0; i < nc_indent; i++) w_write(&w, "    ");
            w_write(&w, "// - %.*s\n", (int)(eol - content), content);
        }

        p = (*eol == '\n') ? eol + 1 : eol;
    }

    return w.output;
}

/* ═══════════════════════════════════════════════════════════
 *  JSON → NC converter
 * ═══════════════════════════════════════════════════════════ */

static char *digest_json(const char *source, const char *filename) {
    NCWriter w;
    w_init(&w);

    w_write(&w, "// Auto-converted from JSON: %s\n\n", filename);

    NcValue parsed = nc_json_parse(source);
    if (IS_MAP(parsed)) {
        w_write(&w, "configure:\n");
        NcMap *m = AS_MAP(parsed);
        for (int i = 0; i < m->count; i++) {
            w_write(&w, "    %s: ", m->keys[i]->chars);
            NcValue v = m->values[i];
            if (IS_STRING(v)) w_write(&w, "\"%s\"", AS_STRING(v)->chars);
            else if (IS_INT(v)) w_write(&w, "%lld", (long long)AS_INT(v));
            else if (IS_FLOAT(v)) w_write(&w, "%g", AS_FLOAT(v));
            else if (IS_BOOL(v)) w_write(&w, "%s", AS_BOOL(v) ? "yes" : "no");
            else w_write(&w, "nothing");
            w_write(&w, "\n");
        }
    }

    return w.output;
}

/* ═══════════════════════════════════════════════════════════
 *  Public API — nc digest <file>
 * ═══════════════════════════════════════════════════════════ */

int nc_digest_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    size_t nread = fread(source, 1, size, f);
    source[nread] = '\0';
    fclose(f);

    /* Detect language from extension */
    const char *ext = strrchr(filename, '.');
    char *result = NULL;

    if (ext && (strcmp(ext, ".py") == 0))
        result = digest_python(source, filename);
    else if (ext && (strcmp(ext, ".js") == 0 || strcmp(ext, ".ts") == 0))
        result = digest_javascript(source, filename);
    else if (ext && (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0))
        result = digest_yaml(source, filename);
    else if (ext && strcmp(ext, ".json") == 0)
        result = digest_json(source, filename);
    else {
        fprintf(stderr, "  Unsupported file type: %s\n", ext ? ext : "(none)");
        fprintf(stderr, "  Supported: .py, .js, .ts, .yaml, .yml, .json\n");
        free(source);
        return 1;
    }

    /* Write output file */
    char outpath[512];
    strncpy(outpath, filename, sizeof(outpath) - 1);
    outpath[sizeof(outpath) - 1] = '\0';
    char *dot = strrchr(outpath, '.');
    if (dot) memcpy(dot, ".nc", 4);
    else strncat(outpath, ".nc", sizeof(outpath) - strlen(outpath) - 1);

    f = fopen(outpath, "w");
    if (!f) { fprintf(stderr, "Cannot write %s\n", outpath); free(source); free(result); return 1; }
    fputs(result, f);
    fclose(f);

    printf("  Digested: %s → %s\n", filename, outpath);
    free(source);
    free(result);
    return 0;
}
