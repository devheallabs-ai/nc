/*
 * nc_embed.c — Embeddable NC Runtime implementation
 *
 * Provides a clean C API for running NC code from any host language.
 * Wraps the existing nc_run_source() and nc_call_behavior() functions
 * with proper output capture and error handling.
 */

#include "../include/nc.h"
#include "../include/nc_embed.h"
#include "../include/nc_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct nc_runtime {
    char **env_keys;
    char **env_vals;
    int    env_count;
    int    env_cap;
    char  *providers_path;
};

nc_runtime_t *nc_runtime_new(void) {
    nc_runtime_t *rt = calloc(1, sizeof(nc_runtime_t));
    rt->env_cap = 16;
    rt->env_keys = malloc(sizeof(char *) * rt->env_cap);
    rt->env_vals = malloc(sizeof(char *) * rt->env_cap);
    rt->env_count = 0;
    rt->providers_path = NULL;
    return rt;
}

void nc_runtime_free(nc_runtime_t *rt) {
    if (!rt) return;
    for (int i = 0; i < rt->env_count; i++) {
        free(rt->env_keys[i]);
        free(rt->env_vals[i]);
    }
    free(rt->env_keys);
    free(rt->env_vals);
    free(rt->providers_path);
    free(rt);
}

void nc_runtime_set_env(nc_runtime_t *rt, const char *key, const char *value) {
    if (!rt || !key) return;

    for (int i = 0; i < rt->env_count; i++) {
        if (strcmp(rt->env_keys[i], key) == 0) {
            free(rt->env_vals[i]);
            rt->env_vals[i] = value ? strdup(value) : strdup("");
            return;
        }
    }

    if (rt->env_count >= rt->env_cap) {
        int new_cap = rt->env_cap * 2;
        char **tmp_keys = realloc(rt->env_keys, sizeof(char *) * new_cap);
        if (!tmp_keys) return;
        char **tmp_vals = realloc(rt->env_vals, sizeof(char *) * new_cap);
        if (!tmp_vals) {
            rt->env_keys = realloc(tmp_keys, sizeof(char *) * rt->env_cap);
            return;
        }
        rt->env_cap = new_cap;
        rt->env_keys = tmp_keys;
        rt->env_vals = tmp_vals;
    }
    rt->env_keys[rt->env_count] = strdup(key);
    rt->env_vals[rt->env_count] = value ? strdup(value) : strdup("");
    rt->env_count++;
}

void nc_runtime_load_providers(nc_runtime_t *rt, const char *json_path) {
    if (!rt) return;
    free(rt->providers_path);
    rt->providers_path = json_path ? strdup(json_path) : NULL;
}

static void apply_env(nc_runtime_t *rt) {
    for (int i = 0; i < rt->env_count; i++) {
        nc_setenv(rt->env_keys[i], rt->env_vals[i], 1);
    }
    if (rt->providers_path) {
        nc_setenv("NC_AI_CONFIG_FILE", rt->providers_path, 1);
    }
}

static nc_result_t capture_eval(nc_runtime_t *rt, const char *source, const char *filename) {
    nc_result_t res = {0};
    apply_env(rt);

    /* Capture stdout/stderr using portable tmpfile + freopen approach */
    char tmpname[512], tmpname_err[512];
    snprintf(tmpname, sizeof(tmpname), "%s%cnc_embed_%d.tmp",
             nc_tempdir(), NC_PATH_SEP, nc_getpid());
    snprintf(tmpname_err, sizeof(tmpname_err), "%s%cnc_embed_err_%d.tmp",
             nc_tempdir(), NC_PATH_SEP, nc_getpid());

    fflush(stdout);
    fflush(stderr);

#ifndef NC_WINDOWS
    int stdout_fd = dup(nc_fileno(stdout));
    int stderr_fd = dup(nc_fileno(stderr));
    FILE *tmp_out = fopen(tmpname, "w+");
    FILE *tmp_err = fopen(tmpname_err, "w+");
    if (tmp_out) dup2(nc_fileno(tmp_out), nc_fileno(stdout));
    if (tmp_err) dup2(nc_fileno(tmp_err), nc_fileno(stderr));
#else
    FILE *tmp_out = freopen(tmpname, "w+", stdout);
    FILE *tmp_err = freopen(tmpname_err, "w+", stderr);
#endif

    int exit_code = nc_run_source(source, filename ? filename : "<embed>");

    fflush(stdout);
    fflush(stderr);

#ifndef NC_WINDOWS
    dup2(stdout_fd, nc_fileno(stdout));
    dup2(stderr_fd, nc_fileno(stderr));
    close(stdout_fd);
    close(stderr_fd);
    if (tmp_out) fclose(tmp_out);
    if (tmp_err) fclose(tmp_err);
#else
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#endif

    /* Read captured stdout */
    FILE *f = fopen(tmpname, "r");
    char *out_buf = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        out_buf = malloc(sz + 1);
        if (out_buf) {
            size_t n = fread(out_buf, 1, sz, f);
            out_buf[n] = '\0';
        }
        fclose(f);
    }
    remove(tmpname);

    /* Read captured stderr */
    char *err_buf = NULL;
    f = fopen(tmpname_err, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        err_buf = malloc(sz + 1);
        if (err_buf) {
            size_t n = fread(err_buf, 1, sz, f);
            err_buf[n] = '\0';
        }
        fclose(f);
    }
    remove(tmpname_err);

    res.ok = (exit_code == 0);
    res.output = out_buf ? out_buf : strdup("");
    res.error = err_buf;
    res.exit_code = exit_code;
    return res;
}

nc_result_t nc_runtime_eval(nc_runtime_t *rt, const char *source) {
    if (!rt || !source) {
        nc_result_t err = {false, NULL, strdup("NULL runtime or source"), 1};
        return err;
    }
    return capture_eval(rt, source, NULL);
}

nc_result_t nc_runtime_eval_with(nc_runtime_t *rt, const char *source,
                                  nc_var_t *vars, int var_count) {
    if (!rt || !source) {
        nc_result_t err = {false, NULL, strdup("NULL runtime or source"), 1};
        return err;
    }

    /* Build NC source that sets variables, then runs the user source.
     * Account for escaping doubling value length in worst case. */
    size_t total = strlen(source) + 256;
    for (int i = 0; i < var_count; i++) {
        total += strlen(vars[i].name) + (strlen(vars[i].value) * 2) + 64;
    }

    char *full = malloc(total);
    full[0] = '\0';

    for (int i = 0; i < var_count; i++) {
        /* Validate variable name: only alphanumeric and underscore */
        const char *n = vars[i].name;
        bool name_valid = (n && n[0]);
        for (const char *c = n; c && *c; c++) {
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') ||
                  (*c >= '0' && *c <= '9') || *c == '_')) {
                name_valid = false;
                break;
            }
        }
        if (!name_valid) continue;

        /* Escape value to prevent NC code injection */
        const char *raw_val = vars[i].value ? vars[i].value : "";
        size_t raw_len = strlen(raw_val);
        size_t esc_cap = raw_len * 2 + 1;
        char *escaped = malloc(esc_cap);
        if (!escaped) continue;
        size_t ei = 0;
        for (size_t vi = 0; vi < raw_len && ei < esc_cap - 2; vi++) {
            char ch = raw_val[vi];
            if (ch == '"' || ch == '\\') {
                escaped[ei++] = '\\';
                escaped[ei++] = ch;
            } else if (ch == '\n') {
                escaped[ei++] = '\\';
                escaped[ei++] = 'n';
            } else if (ch == '\r') {
                escaped[ei++] = '\\';
                escaped[ei++] = 'r';
            } else if (ch == '\t') {
                escaped[ei++] = '\\';
                escaped[ei++] = 't';
            } else {
                escaped[ei++] = ch;
            }
        }
        escaped[ei] = '\0';

        char line[2048];
        snprintf(line, sizeof(line), "set %s to \"%s\"\n", n, escaped);
        strcat(full, line);
        free(escaped);
    }
    strcat(full, source);

    nc_result_t res = capture_eval(rt, full, NULL);
    free(full);
    return res;
}

nc_result_t nc_runtime_call(nc_runtime_t *rt, const char *source,
                             const char *behavior_name,
                             nc_var_t *args, int arg_count) {
    if (!rt || !source || !behavior_name) {
        nc_result_t err = {false, NULL, strdup("NULL runtime, source, or behavior"), 1};
        return err;
    }

    apply_env(rt);

    NcMap *arg_map = NULL;
    if (arg_count > 0) {
        arg_map = nc_map_new();
        for (int i = 0; i < arg_count; i++) {
            NcString *key = nc_string_from_cstr(args[i].name);
            NcString *val = nc_string_from_cstr(args[i].value);
            nc_map_set(arg_map, key, NC_STRING(val));
            nc_string_free(key);
        }
    }

    NcValue result = nc_call_behavior(source, "<embed>", behavior_name, arg_map);

    nc_result_t res = {0};
    if (IS_STRING(result)) {
        res.ok = true;
        res.output = strdup(AS_STRING(result)->chars);
        res.exit_code = 0;
    } else if (IS_NONE(result)) {
        res.ok = true;
        res.output = strdup("");
        res.exit_code = 0;
    } else {
        res.ok = true;
        char *json = nc_json_serialize(result, false);
        res.output = json ? json : strdup("");
        res.exit_code = 0;
    }

    if (arg_map) nc_map_free(arg_map);
    return res;
}

nc_result_t nc_runtime_eval_file(nc_runtime_t *rt, const char *filename) {
    if (!rt || !filename) {
        nc_result_t err = {false, NULL, strdup("NULL runtime or filename"), 1};
        return err;
    }

    apply_env(rt);

    FILE *f = fopen(filename, "r");
    if (!f) {
        nc_result_t err = {false, NULL, strdup("Cannot open file"), 1};
        return err;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    nc_result_t res = capture_eval(rt, buf, filename);
    free(buf);
    return res;
}

void nc_result_free(nc_result_t *res) {
    if (!res) return;
    free(res->output);
    free(res->error);
    res->output = NULL;
    res->error = NULL;
}

const char *nc_version(void) {
    return "1.0.0";
}
