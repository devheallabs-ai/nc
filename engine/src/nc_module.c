/*
 * nc_module.c — Module system for NC.
 *
 * Implements: import "file", from "file" import behavior
 * Provides: module loading, caching, namespace isolation,
 *           circular import detection, path resolution.
 *
 * This is what makes NC a real platform — code can be
 * organized into files, packages, and libraries.
 *
 * Search order:
 *   1. Current directory
 *   2. .nc_packages/<name>/main.nc
 *   3. NC_PATH environment variable directories
 *   4. Built-in stdlib modules
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"

#define MAX_MODULES 256
#define MAX_IMPORT_DEPTH 32

/* ═══════════════════════════════════════════════════════════
 *  Module cache — loaded modules stored here
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcString  *name;         /* module name (e.g. "helpers") */
    NcString  *path;         /* resolved file path */
    NcASTNode *ast;          /* parsed AST */
    NcMap     *exports;      /* exported behaviors + types */
    bool       loaded;
    bool       loading;      /* for circular import detection */
} NcModule;

static NcModule modules[MAX_MODULES];
static int module_count = 0;

/* Import stack for detecting circular imports */
static const char *import_stack[MAX_IMPORT_DEPTH];
static int import_depth = 0;

/* ── Path resolution ───────────────────────────────────────── */

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static char *resolve_module_path(const char *name, const char *from_file) {
    char path[512];

    /* Reject module names containing path traversal or separators */
    if (!name || !name[0]) return NULL;
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == '\\') return NULL;
        if (*p == '.' && *(p + 1) == '.') return NULL;
        if ((unsigned char)*p < 0x20) return NULL;
    }

    /* 1. Try relative to importing file */
    if (from_file) {
        const char *last_slash = nc_last_path_sep(from_file);
        if (last_slash) {
            int dir_len = (int)(last_slash - from_file);
            snprintf(path, sizeof(path), "%.*s" NC_PATH_SEP_STR "%s.nc", dir_len, from_file, name);
            if (file_exists(path)) return strdup(path);
        }
    }

    /* 2. Try current directory */
    snprintf(path, sizeof(path), "%s.nc", name);
    if (file_exists(path)) return strdup(path);

    /* 3. Try .nc_packages */
    snprintf(path, sizeof(path), ".nc_packages" NC_PATH_SEP_STR "%s" NC_PATH_SEP_STR "main.nc", name);
    if (file_exists(path)) return strdup(path);

    /* 4. Try NC_PATH */
    const char *nc_path = getenv("NC_PATH");
    if (nc_path) {
        char nc_path_copy[1024];
        strncpy(nc_path_copy, nc_path, sizeof(nc_path_copy) - 1); nc_path_copy[sizeof(nc_path_copy) - 1] = '\0';
        char sep[2] = { NC_PATH_LIST_SEP, '\0' };
        char *saveptr = NULL;
        char *dir = strtok_r(nc_path_copy, sep, &saveptr);
        while (dir) {
            snprintf(path, sizeof(path), "%s%c%s.nc", dir, NC_PATH_SEP, name);
            if (file_exists(path)) return strdup(path);
            snprintf(path, sizeof(path), "%s%c%s%cmain.nc", dir, NC_PATH_SEP, name, NC_PATH_SEP);
            if (file_exists(path)) return strdup(path);
            dir = strtok_r(NULL, sep, &saveptr);
        }
    }

    return NULL;
}

/* ── Module cache lookup ───────────────────────────────────── */

static NcModule *find_cached_module(const char *name) {
    for (int i = 0; i < module_count; i++)
        if (strcmp(modules[i].name->chars, name) == 0)
            return &modules[i];
    return NULL;
}

/* ── Circular import detection ─────────────────────────────── */

static bool is_circular(const char *path) {
    for (int i = 0; i < import_depth; i++)
        if (strcmp(import_stack[i], path) == 0) return true;
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  Load a module
 * ═══════════════════════════════════════════════════════════ */

NcModule *nc_module_load(const char *name, const char *from_file) {
    /* Check cache first */
    NcModule *cached = find_cached_module(name);
    if (cached && cached->loaded) return cached;

    /* Resolve path */
    char *path = resolve_module_path(name, from_file);
    if (!path) {
        fprintf(stderr, "[NC Import] Cannot find module '%s'\n", name);
        fprintf(stderr, "  Searched:\n");
        fprintf(stderr, "    ./%s.nc\n", name);
        fprintf(stderr, "    .nc_packages/%s/main.nc\n", name);
        fprintf(stderr, "    NC_PATH directories\n");
        return NULL;
    }

    /* Check circular imports */
    if (is_circular(path)) {
        fprintf(stderr, "[NC Import] Circular import detected: '%s'\n", name);
        fprintf(stderr, "  Import chain:\n");
        for (int i = 0; i < import_depth; i++)
            fprintf(stderr, "    %s\n", import_stack[i]);
        fprintf(stderr, "    -> %s (circular!)\n", path);
        free(path);
        return NULL;
    }

    /* Read file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[NC Import] Cannot read '%s'\n", path);
        free(path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    size_t nread = fread(source, 1, size, f);
    source[nread] = '\0';
    fclose(f);

    /* Push onto import stack */
    if (import_depth >= MAX_IMPORT_DEPTH) {
        fprintf(stderr, "[NC Import] Too many nested imports (max %d)\n", MAX_IMPORT_DEPTH);
        free(source); free(path);
        return NULL;
    }
    import_stack[import_depth++] = path;

    /* Parse */
    NcLexer *lex = nc_lexer_new(source, path);
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, path);
    NcASTNode *ast = nc_parser_parse(parser);

    if (parser->had_error) {
        fprintf(stderr, "[NC Import] Parse error in '%s': %s\n", path, parser->error_msg);
        nc_parser_free(parser); nc_lexer_free(lex);
        free(source); import_depth--;
        return NULL;
    }

    /* Register module */
    if (module_count >= MAX_MODULES) {
        fprintf(stderr, "[NC Import] Too many modules (max %d)\n", MAX_MODULES);
        nc_parser_free(parser); nc_lexer_free(lex);
        free(source); import_depth--;
        return NULL;
    }

    NcModule *mod = &modules[module_count++];
    mod->name = nc_string_from_cstr(name);
    mod->path = nc_string_from_cstr(path);
    mod->ast = ast;
    mod->loaded = true;
    mod->loading = false;

    /* Build exports map (behaviors + types) */
    mod->exports = nc_map_new();
    for (int i = 0; i < ast->as.program.beh_count; i++) {
        NcASTNode *beh = ast->as.program.behaviors[i];
        nc_map_set(mod->exports, beh->as.behavior.name,
                   NC_STRING(nc_string_from_cstr("behavior")));
    }
    for (int i = 0; i < ast->as.program.def_count; i++) {
        NcASTNode *def = ast->as.program.definitions[i];
        nc_map_set(mod->exports, def->as.definition.name,
                   NC_STRING(nc_string_from_cstr("type")));
    }

    /* Pop import stack */
    import_depth--;

    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);
    free(path);

    return mod;
}

/* Public API — returns the AST so interpreter can access behaviors */
NcASTNode *nc_module_load_file(const char *name, const char *from_file) {
    NcModule *mod = nc_module_load(name, from_file);
    if (mod && mod->ast) return mod->ast;
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  Module info / listing
 * ═══════════════════════════════════════════════════════════ */

void nc_module_list_loaded(void) {
    printf("\n  Loaded Modules:\n");
    printf("  %s\n", "────────────────────────────────────────");
    if (module_count == 0) {
        printf("  (none)\n");
    }
    for (int i = 0; i < module_count; i++) {
        printf("  %-20s  %s  (%d exports)\n",
               modules[i].name->chars,
               modules[i].path->chars,
               modules[i].exports->count);
    }
    printf("\n");
}

void nc_module_reset(void) {
    for (int i = 0; i < module_count; i++) {
        nc_string_free(modules[i].name);
        nc_string_free(modules[i].path);
        nc_map_free(modules[i].exports);
    }
    module_count = 0;
    import_depth = 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Built-in stdlib modules (virtual modules)
 * ═══════════════════════════════════════════════════════════ */

static NcMap *create_stdlib_math(void) {
    NcMap *m = nc_map_new();
    nc_map_set(m, nc_string_from_cstr("pi"), NC_FLOAT(3.14159265358979323846));
    nc_map_set(m, nc_string_from_cstr("e"), NC_FLOAT(2.71828182845904523536));
    nc_map_set(m, nc_string_from_cstr("tau"), NC_FLOAT(6.28318530717958647692));
    return m;
}

static NcMap *create_stdlib_time(void) {
    NcMap *m = nc_map_new();
    nc_map_set(m, nc_string_from_cstr("now"), NC_FLOAT((double)time(NULL)));
    return m;
}

NcValue nc_module_get_stdlib(const char *name) {
    if (strcmp(name, "math") == 0) return NC_MAP(create_stdlib_math());
    if (strcmp(name, "time") == 0) return NC_MAP(create_stdlib_time());
    return NC_NONE();
}
