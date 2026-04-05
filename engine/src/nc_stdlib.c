

#include "../include/nc_value.h"
#include "../include/nc_platform.h"
// Platform/system info helpers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NC_WINDOWS
#include <windows.h>
#else
#include <unistd.h> // gethostname
#endif

// Extra platform/system info (must come after includes)
NcValue nc_stdlib_platform_hostname(void) {
#ifdef NC_WINDOWS
    char buf[256] = {0};
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz))
        return NC_STRING(nc_string_from_cstr(buf));
    return NC_STRING(nc_string_from_cstr("Unknown"));
#else
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf)) == 0)
        return NC_STRING(nc_string_from_cstr(buf));
    return NC_STRING(nc_string_from_cstr("Unknown"));
#endif
}

NcValue nc_stdlib_platform_user(void) {
#ifdef NC_WINDOWS
    char buf[256] = {0};
    DWORD sz = sizeof(buf);
    if (GetUserNameA(buf, &sz))
        return NC_STRING(nc_string_from_cstr(buf));
    return NC_STRING(nc_string_from_cstr("Unknown"));
#else
    const char *user = getenv("USER");
    if (user) return NC_STRING(nc_string_from_cstr(user));
    user = getenv("LOGNAME");
    if (user) return NC_STRING(nc_string_from_cstr(user));
    return NC_STRING(nc_string_from_cstr("Unknown"));
#endif
}

NcValue nc_stdlib_platform_home_dir(void) {
#ifdef NC_WINDOWS
    const char *home = getenv("USERPROFILE");
    if (home) return NC_STRING(nc_string_from_cstr(home));
    const char *drive = getenv("HOMEDRIVE");
    const char *path = getenv("HOMEPATH");
    if (drive && path) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s%s", drive, path);
        return NC_STRING(nc_string_from_cstr(buf));
    }
    return NC_STRING(nc_string_from_cstr("C:\\"));
#else
    const char *home = getenv("HOME");
    if (home) return NC_STRING(nc_string_from_cstr(home));
    return NC_STRING(nc_string_from_cstr("/"));
#endif
}

NcValue nc_stdlib_platform_temp_dir(void) {
#ifdef NC_WINDOWS
    char buf[256] = {0};
    DWORD sz = GetTempPathA(sizeof(buf), buf);
    if (sz > 0 && sz < sizeof(buf))
        return NC_STRING(nc_string_from_cstr(buf));
    return NC_STRING(nc_string_from_cstr("C:\\Temp\\"));
#else
    const char *tmp = getenv("TMPDIR");
    if (tmp) return NC_STRING(nc_string_from_cstr(tmp));
    return NC_STRING(nc_string_from_cstr("/tmp/"));
#endif
}

NcValue nc_stdlib_platform_system(void) {
#ifdef NC_WINDOWS
    return NC_STRING(nc_string_from_cstr("Windows"));
#elif defined(__APPLE__)
    return NC_STRING(nc_string_from_cstr("macOS"));
#elif defined(__linux__)
    return NC_STRING(nc_string_from_cstr("Linux"));
#else
    return NC_STRING(nc_string_from_cstr("Unknown"));
#endif
}

NcValue nc_stdlib_platform_architecture(void) {
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return NC_STRING(nc_string_from_cstr("x86_64"));
#elif defined(_M_ARM64) || defined(__aarch64__)
    return NC_STRING(nc_string_from_cstr("arm64"));
#elif defined(_M_IX86) || defined(__i386__)
    return NC_STRING(nc_string_from_cstr("x86"));
#else
    return NC_STRING(nc_string_from_cstr("Unknown"));
#endif
}

NcValue nc_stdlib_platform_info(void) {
    const char *sys = "Unknown";
    const char *arch = "Unknown";
#ifdef NC_WINDOWS
    sys = "Windows";
#elif defined(__APPLE__)
    sys = "macOS";
#elif defined(__linux__)
    sys = "Linux";
#endif
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    arch = "x86_64";
#elif defined(_M_ARM64) || defined(__aarch64__)
    arch = "arm64";
#elif defined(_M_IX86) || defined(__i386__)
    arch = "x86";
#endif
    char buf[128];
    snprintf(buf, sizeof(buf), "%s/%s", sys, arch);
    return NC_STRING(nc_string_from_cstr(buf));
}
/*
 * nc_stdlib.c — Standard library for NC (built-in functions).
 *
 * Provides: file I/O, time, math, string ops, HTTP server.
 * These are registered as native functions in the VM/interpreter.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include <time.h>
#ifndef NC_WINDOWS
#include <limits.h>
#endif

/* ═══════════════════════════════════════════════════════════
 *  File I/O — Path safety
 * ═══════════════════════════════════════════════════════════ */

/* Normalize path separators: backslash → forward slash (Windows compat) */
static void nc_normalize_path(char *buf, const char *path, int buf_size) {
    int i = 0;
    for (; path[i] && i < buf_size - 1; i++) {
        buf[i] = (path[i] == '\\') ? '/' : path[i];
    }
    buf[i] = '\0';
}

static bool nc_path_is_safe(const char *path) {
    if (!path || !path[0]) {
        fprintf(stderr, "[NC] ERROR: Path is empty or null.\n");
        return false;
    }

    /* Normalize path separators for consistent comparison */
    char normalized[4096];
    nc_normalize_path(normalized, path, sizeof(normalized));
    path = normalized;

    /* Reject UNC paths on Windows */
#ifdef NC_WINDOWS
    if (path[0] == '/' && path[1] == '/') {
        fprintf(stderr, "[NC] ERROR: UNC paths (e.g., //server/share) are not supported on Windows.\n");
        return false;
    }
#endif
    /* Reject absolute paths outside the working directory */
    if (path[0] == '/' || (path[0] != '\0' && path[1] == ':')) {
#ifdef NC_WINDOWS
        /* Windows: allow system temp directory and common temp paths.
         * All paths are normalized to forward slashes before comparison. */
        char win_temp_raw[512] = {0};
        GetTempPathA(sizeof(win_temp_raw), win_temp_raw);
        char win_temp[512] = {0};
        nc_normalize_path(win_temp, win_temp_raw, sizeof(win_temp));
        const char *allowed_prefixes[] = {win_temp, "C:/Temp/", NULL};
#else
        /* macOS uses /var/folders/.../T/ for TMPDIR, /private/var/folders for realpath */
        const char *allowed_prefixes[] = {"/tmp/", "/var/folders/", "/private/tmp/", "/private/var/folders/", NULL};
#endif
        const char *env_prefix = getenv("NC_ALLOWED_PATH");
        bool allowed = false;
        for (int i = 0; allowed_prefixes[i]; i++) {
            /* On Windows, compare case-insensitively (paths already normalized to /) */
#ifdef NC_WINDOWS
            if (_strnicmp(path, allowed_prefixes[i], strlen(allowed_prefixes[i])) == 0) {
#else
            if (strncmp(path, allowed_prefixes[i], strlen(allowed_prefixes[i])) == 0) {
#endif
                allowed = true;
                break;
            }
        }
        if (env_prefix && strncmp(path, env_prefix, strlen(env_prefix)) == 0)
            allowed = true;
        if (!allowed) {
            fprintf(stderr, "[NC] ERROR: Absolute path '%s' is not allowed. Use allowed temp or working directory paths only.\n", path);
            return false;
        }
    }

    /* Reject path traversal sequences */
    if (strstr(path, "..") != NULL) {
        fprintf(stderr, "[NC] ERROR: Path traversal ('..') is not allowed in path '%s'.\n", path);
        return false;
    }

    /* Embedded null bytes in C strings are harmless (strlen stops at first \0).
     * Real protection is realpath() resolution below. */

    /* Resolve and re-check with realpath when available */
    /* Symlink traversal protection: resolve and re-check with realpath */
#ifndef NC_WINDOWS
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) {
        if (strstr(resolved, "..") != NULL) {
            fprintf(stderr, "[NC] ERROR: Symlink traversal detected in resolved path '%s'.\n", resolved);
            return false;
        }
        /* Block sensitive system paths */
        const char *blocked[] = {"/etc/shadow", "/etc/passwd", "/etc/sudoers",
                                  "/proc/", "/sys/", "/dev/", NULL};
        for (int i = 0; blocked[i]; i++) {
            if (strncmp(resolved, blocked[i], strlen(blocked[i])) == 0) {
                fprintf(stderr, "[NC] ERROR: Access to sensitive system path '%s' is blocked.\n", resolved);
                return false;
            }
        }
    }
#endif

    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  File I/O
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_read_file(const char *path) {
    if (!nc_sandbox_check("file_read")) return NC_NONE();
    if (!nc_path_is_safe(path)) return NC_NONE();

    FILE *f = fopen(path, "rb");
    if (!f) return NC_NONE();
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NC_NONE(); }
    size_t nread = fread(buf, 1, size, f);
    buf[nread] = '\0';
    fclose(f);
    NcString *s = nc_string_new(buf, (int)nread);
    free(buf);
    return NC_STRING(s);
}

NcValue nc_stdlib_write_file(const char *path, const char *content) {
    if (!nc_sandbox_check("file_write")) return NC_BOOL(false);
    if (!nc_path_is_safe(path)) return NC_BOOL(false);

    FILE *f = fopen(path, "w");
    if (!f) {
        /* Auto-create parent directories if they don't exist */
        char tmp[512];
        strncpy(tmp, path, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        char *last_sep = strrchr(tmp, '/');
#ifdef NC_WINDOWS
        char *last_bsep = strrchr(tmp, '\\');
        if (last_bsep && (!last_sep || last_bsep > last_sep))
            last_sep = last_bsep;
#endif
        if (last_sep) {
            *last_sep = '\0';
            for (int i = 1; tmp[i]; i++) {
                if (tmp[i] == '/' || tmp[i] == '\\') {
                    char saved = tmp[i];
                    tmp[i] = '\0';
                    nc_mkdir(tmp);
                    tmp[i] = saved;
                }
            }
            nc_mkdir(tmp);
        }
        f = fopen(path, "w");
        if (!f) return NC_BOOL(false);
    }
    fputs(content, f);
    fflush(f);
    fclose(f);
    return NC_BOOL(true);
}

NcValue nc_stdlib_file_exists(const char *path) {
    if (!nc_path_is_safe(path)) return NC_BOOL(false);

    struct stat st;
    return NC_BOOL(stat(path, &st) == 0);
}

NcValue nc_stdlib_delete_file(const char *path) {
    if (!nc_sandbox_check("file_write")) return NC_BOOL(false);
    if (!nc_path_is_safe(path)) return NC_BOOL(false);

    return NC_BOOL(remove(path) == 0);
}

NcValue nc_stdlib_mkdir(const char *path) {
    if (!nc_sandbox_check("file_write")) return NC_BOOL(false);
    if (!nc_path_is_safe(path)) return NC_BOOL(false);

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    int len = (int)strlen(tmp);

    /* Recursively create parent directories (like mkdir -p) */
    for (int i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];
            tmp[i] = '\0';
            nc_mkdir(tmp);
            tmp[i] = saved;
        }
    }
    int result = nc_mkdir(tmp);
    return NC_BOOL(result == 0 || errno == EEXIST);
}

/* ═══════════════════════════════════════════════════════════
 *  Time
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_time_now(void) {
    return NC_FLOAT((double)time(NULL));
}

NcValue nc_stdlib_time_ms(void) {
    return NC_FLOAT(nc_realtime_ms());
}

NcValue nc_stdlib_time_format(double timestamp, const char *fmt) {
    time_t t = (time_t)timestamp;
    struct tm tm_buf;
#ifdef NC_WINDOWS
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[128];
    strftime(buf, sizeof(buf), fmt ? fmt : "%Y-%m-%d %H:%M:%S", &tm_buf);
    return NC_STRING(nc_string_from_cstr(buf));
}

NcValue nc_stdlib_time_iso(double timestamp) {
    time_t t = (time_t)timestamp;
    struct tm tm_buf;
#ifdef NC_WINDOWS
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return NC_STRING(nc_string_from_cstr(buf));
}

/* ═══════════════════════════════════════════════════════════
 *  Math
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_abs(double x) { return NC_FLOAT(fabs(x)); }
NcValue nc_stdlib_ceil(double x) { return NC_INT((int64_t)ceil(x)); }
NcValue nc_stdlib_floor(double x) { return NC_INT((int64_t)floor(x)); }
NcValue nc_stdlib_round(double x) { return NC_INT((int64_t)round(x)); }
NcValue nc_stdlib_sqrt(double x) { return NC_FLOAT(sqrt(x)); }
NcValue nc_stdlib_pow(double base, double exp) { return NC_FLOAT(pow(base, exp)); }
NcValue nc_stdlib_min(double a, double b) { return NC_FLOAT(a < b ? a : b); }
NcValue nc_stdlib_max(double a, double b) { return NC_FLOAT(a > b ? a : b); }
NcValue nc_stdlib_random(void) { return NC_FLOAT((double)rand() / RAND_MAX); }

/* ═══════════════════════════════════════════════════════════
 *  String operations
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_upper(NcString *s) {
    char *buf = malloc(s->length + 1);
    for (int i = 0; i < s->length; i++) buf[i] = toupper(s->chars[i]);
    buf[s->length] = '\0';
    NcString *result = nc_string_new(buf, s->length);
    free(buf);
    return NC_STRING(result);
}

NcValue nc_stdlib_lower(NcString *s) {
    char *buf = malloc(s->length + 1);
    for (int i = 0; i < s->length; i++) buf[i] = tolower(s->chars[i]);
    buf[s->length] = '\0';
    NcString *result = nc_string_new(buf, s->length);
    free(buf);
    return NC_STRING(result);
}

NcValue nc_stdlib_trim(NcString *s) {
    int start = 0, end = s->length - 1;
    while (start < s->length && isspace(s->chars[start])) start++;
    while (end > start && isspace(s->chars[end])) end--;
    NcString *result = nc_string_new(s->chars + start, end - start + 1);
    return NC_STRING(result);
}

NcValue nc_stdlib_contains(NcString *haystack, NcString *needle) {
    return NC_BOOL(strstr(haystack->chars, needle->chars) != NULL);
}

NcValue nc_stdlib_starts_with(NcString *s, NcString *prefix) {
    if (prefix->length > s->length) return NC_BOOL(false);
    return NC_BOOL(strncmp(s->chars, prefix->chars, prefix->length) == 0);
}

NcValue nc_stdlib_ends_with(NcString *s, NcString *suffix) {
    if (suffix->length > s->length) return NC_BOOL(false);
    return NC_BOOL(strcmp(s->chars + s->length - suffix->length, suffix->chars) == 0);
}

NcValue nc_stdlib_replace(NcString *s, NcString *old, NcString *new_str) {
    size_t result_cap = s->length * 2 + 1;
    char *result = malloc(result_cap);
    result[0] = '\0';
    size_t result_len = 0;
    const char *p = s->chars;
    while (*p) {
        if (strncmp(p, old->chars, old->length) == 0) {
            size_t add_len = new_str->length;
            if (result_len + add_len >= result_cap) {
                result_cap = (result_len + add_len) * 2 + 1;
                result = realloc(result, result_cap);
            }
            memcpy(result + result_len, new_str->chars, add_len);
            result_len += add_len;
            result[result_len] = '\0';
            p += old->length;
        } else {
            result[result_len++] = *p;
            result[result_len] = '\0';
            p++;
        }
    }
    NcString *res = nc_string_from_cstr(result);
    free(result);
    return NC_STRING(res);
}

NcValue nc_stdlib_split(NcString *s, NcString *delimiter) {
    NcList *list = nc_list_new();
    if (!s || s->length == 0) return NC_LIST(list);

    /* Manual split instead of strtok to handle edge cases properly.
     * strtok merges adjacent delimiters and can't handle empty fields. */
    int dlen = delimiter->length;
    if (dlen == 0) {
        nc_list_push(list, NC_STRING(nc_string_ref(s)));
        return NC_LIST(list);
    }

    const char *p = s->chars;
    const char *end = s->chars + s->length;
    while (p <= end) {
        const char *found = NULL;
        if (dlen == 1) {
            found = strchr(p, delimiter->chars[0]);
        } else {
            found = strstr(p, delimiter->chars);
        }
        if (!found || found >= end) {
            if (p < end)
                nc_list_push(list, NC_STRING(nc_string_new(p, (int)(end - p))));
            break;
        }
        int seg_len = (int)(found - p);
        if (seg_len > 0)
            nc_list_push(list, NC_STRING(nc_string_new(p, seg_len)));
        p = found + dlen;
    }
    return NC_LIST(list);
}

NcValue nc_stdlib_join(NcList *list, NcString *separator) {
    if (list->count == 0) return NC_STRING(nc_string_from_cstr(""));
    int total = 0;
    for (int i = 0; i < list->count; i++) {
        NcString *s = nc_value_to_string(list->items[i]);
        total += s->length;
        nc_string_free(s);
    }
    total += separator->length * (list->count - 1);

    char *buf = malloc(total + 1);
    buf[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < list->count; i++) {
        if (i > 0) {
            memcpy(buf + pos, separator->chars, separator->length);
            pos += separator->length;
        }
        NcString *s = nc_value_to_string(list->items[i]);
        memcpy(buf + pos, s->chars, s->length);
        pos += s->length;
        nc_string_free(s);
    }
    buf[pos] = '\0';
    NcString *result = nc_string_from_cstr(buf);
    free(buf);
    return NC_STRING(result);
}

/* ═══════════════════════════════════════════════════════════
 *  List operations
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_list_append(NcList *list, NcValue val) {
    nc_list_push(list, val);
    return NC_LIST(list);
}

NcValue nc_stdlib_list_length(NcList *list) {
    return NC_INT(list->count);
}

NcValue nc_stdlib_list_reverse(NcList *list) {
    NcList *result = nc_list_new();
    for (int i = list->count - 1; i >= 0; i--)
        nc_list_push(result, list->items[i]);
    return NC_LIST(result);
}

static double nc_sort_key(NcValue v, NcString *field) {
    if (field && IS_MAP(v)) {
        NcValue fv = nc_map_get(AS_MAP(v), field);
        if (IS_INT(fv)) return (double)AS_INT(fv);
        if (IS_FLOAT(fv)) return AS_FLOAT(fv);
        if (IS_STRING(fv)) return atof(AS_STRING(fv)->chars);
        return 0;
    }
    if (IS_INT(v)) return (double)AS_INT(v);
    if (IS_FLOAT(v)) return AS_FLOAT(v);
    if (IS_STRING(v)) return atof(AS_STRING(v)->chars);
    return 0;
}

static void nc_quicksort(NcValue *items, int lo, int hi, NcString *field) {
    if (lo >= hi) return;
    double pivot = nc_sort_key(items[hi], field);
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (nc_sort_key(items[j], field) <= pivot) {
            i++;
            NcValue tmp = items[i]; items[i] = items[j]; items[j] = tmp;
        }
    }
    NcValue tmp = items[i + 1]; items[i + 1] = items[hi]; items[hi] = tmp;
    int pi = i + 1;
    nc_quicksort(items, lo, pi - 1, field);
    nc_quicksort(items, pi + 1, hi, field);
}

NcValue nc_stdlib_list_sort(NcList *list) {
    NcList *result = nc_list_new();
    for (int i = 0; i < list->count; i++)
        nc_list_push(result, list->items[i]);
    if (result->count > 1)
        nc_quicksort(result->items, 0, result->count - 1, NULL);
    return NC_LIST(result);
}

NcValue nc_stdlib_list_sort_by(NcList *list, NcString *field) {
    NcList *result = nc_list_new();
    for (int i = 0; i < list->count; i++)
        nc_list_push(result, list->items[i]);
    if (result->count > 1)
        nc_quicksort(result->items, 0, result->count - 1, field);
    return NC_LIST(result);
}

NcValue nc_stdlib_list_max_by(NcList *list, NcString *field) {
    if (!list || list->count == 0) return NC_NONE();
    int bi = 0;
    double best = nc_sort_key(list->items[0], field);
    for (int i = 1; i < list->count; i++) {
        double v = nc_sort_key(list->items[i], field);
        if (v > best) { best = v; bi = i; }
    }
    return list->items[bi];
}

NcValue nc_stdlib_list_min_by(NcList *list, NcString *field) {
    if (!list || list->count == 0) return NC_NONE();
    int bi = 0;
    double best = nc_sort_key(list->items[0], field);
    for (int i = 1; i < list->count; i++) {
        double v = nc_sort_key(list->items[i], field);
        if (v < best) { best = v; bi = i; }
    }
    return list->items[bi];
}

NcValue nc_stdlib_list_sum_by(NcList *list, NcString *field) {
    if (!list || list->count == 0) return NC_FLOAT(0);
    double sum = 0;
    for (int i = 0; i < list->count; i++)
        sum += nc_sort_key(list->items[i], field);
    return NC_FLOAT(sum);
}

NcValue nc_stdlib_list_map_field(NcList *list, NcString *field) {
    NcList *result = nc_list_new();
    if (!list) return NC_LIST(result);
    for (int i = 0; i < list->count; i++) {
        if (IS_MAP(list->items[i])) {
            NcValue fv = nc_map_get(AS_MAP(list->items[i]), field);
            nc_list_push(result, fv);
        } else {
            nc_list_push(result, NC_NONE());
        }
    }
    return NC_LIST(result);
}

NcValue nc_stdlib_list_filter_by(NcList *list, NcString *field,
                                  const char *op, double threshold) {
    NcList *result = nc_list_new();
    if (!list) return NC_LIST(result);
    for (int i = 0; i < list->count; i++) {
        double v = nc_sort_key(list->items[i], field);
        bool pass = false;
        if (strcmp(op, "above") == 0 || strcmp(op, ">") == 0) pass = v > threshold;
        else if (strcmp(op, "below") == 0 || strcmp(op, "<") == 0) pass = v < threshold;
        else if (strcmp(op, "equal") == 0 || strcmp(op, "==") == 0) pass = fabs(v - threshold) < 1e-9;
        else if (strcmp(op, "at_least") == 0 || strcmp(op, ">=") == 0) pass = v >= threshold;
        else if (strcmp(op, "at_most") == 0 || strcmp(op, "<=") == 0) pass = v <= threshold;
        else pass = v > threshold;
        if (pass) nc_list_push(result, list->items[i]);
    }
    return NC_LIST(result);
}

NcValue nc_stdlib_list_filter_by_str(NcList *list, NcString *field,
                                      const char *op, NcString *threshold) {
    NcList *result = nc_list_new();
    if (!list || !threshold) return NC_LIST(result);
    for (int i = 0; i < list->count; i++) {
        if (!IS_MAP(list->items[i])) continue;
        NcValue fv = nc_map_get(AS_MAP(list->items[i]), field);
        bool pass = false;
        if (strcmp(op, "equal") == 0 || strcmp(op, "==") == 0) {
            if (IS_STRING(fv))
                pass = nc_string_equal(AS_STRING(fv), threshold);
        } else if (strcmp(op, "not_equal") == 0 || strcmp(op, "!=") == 0) {
            if (IS_STRING(fv))
                pass = !nc_string_equal(AS_STRING(fv), threshold);
            else
                pass = true;
        } else {
            double v = nc_sort_key(list->items[i], field);
            double t = strtod(threshold->chars, NULL);
            if (strcmp(op, "above") == 0 || strcmp(op, ">") == 0) pass = v > t;
            else if (strcmp(op, "below") == 0 || strcmp(op, "<") == 0) pass = v < t;
            else if (strcmp(op, "at_least") == 0 || strcmp(op, ">=") == 0) pass = v >= t;
            else if (strcmp(op, "at_most") == 0 || strcmp(op, "<=") == 0) pass = v <= t;
        }
        if (pass) nc_list_push(result, list->items[i]);
    }
    return NC_LIST(result);
}

/* ═══════════════════════════════════════════════════════════
 *  Environment
 * ═══════════════════════════════════════════════════════════ */

/* Check if an env var name looks like a secret */
static bool is_secret_name(const char *name) {
    const char *patterns[] = {"KEY", "SECRET", "TOKEN", "PASSWORD", "PASSWD",
                               "CREDENTIAL", "AUTH", "_PWD", NULL};
    char upper[256] = {0};
    for (int i = 0; name[i] && i < 255; i++)
        upper[i] = (name[i] >= 'a' && name[i] <= 'z') ? name[i] - 32 : name[i];
    for (int i = 0; patterns[i]; i++)
        if (strstr(upper, patterns[i])) return true;
    return false;
}

NcValue nc_stdlib_env_get(const char *name) {
    const char *val = getenv(name);
    if (!val) return NC_NONE();

    /* Return the real value for internal use (AI calls etc.).
     * The value is accessible — but log/show will redact it
     * via nc_redact_for_display below. */
    return NC_STRING(nc_string_from_cstr(val));
}

/* Redact sensitive values before display in show/log output.
 * Uses a generic pattern-based approach: any env var whose name contains
 * KEY, SECRET, TOKEN, PASSWORD, CREDENTIAL, or WEBHOOK is treated as
 * sensitive. Users can add custom vars via NC_REDACT_VARS (comma-separated).
 * If a match is found, replaces the secret with a masked version. */
/* ── Cached redaction values ──────────────────────────────
 * Resolves env var names → values ONCE on first call, then
 * reuses the cache. Eliminates O(E * T) getenv scanning per
 * log message. Cache is invalidated if NC_REDACT_VARS changes. */
typedef struct {
    const char *value;
    int         length;
} RedactEntry;

static RedactEntry redact_cache[64];
static int redact_cache_count = 0;
static bool redact_cache_ready = false;

static void redact_cache_init(void) {
    redact_cache_count = 0;

    /* Well-known NC env vars */
    const char *nc_vars[] = { "NC_AI_KEY", "NC_JWT_SECRET", "NC_API_KEYS", NULL };
    for (int i = 0; nc_vars[i] && redact_cache_count < 60; i++) {
        const char *val = getenv(nc_vars[i]);
        if (val && strlen(val) >= 5) {
            redact_cache[redact_cache_count].value = val;
            redact_cache[redact_cache_count].length = (int)strlen(val);
            redact_cache_count++;
        }
    }

    /* User-supplied NC_REDACT_VARS */
    const char *custom = getenv("NC_REDACT_VARS");
    if (custom && custom[0]) {
        static char rbuf[1024]; /* static to keep tok pointers valid */
        strncpy(rbuf, custom, sizeof(rbuf) - 1);
        rbuf[sizeof(rbuf) - 1] = '\0';
        char *tok = strtok(rbuf, ",");
        while (tok && redact_cache_count < 58) {
            while (*tok == ' ') tok++;
            const char *val = getenv(tok);
            if (val && strlen(val) >= 5) {
                redact_cache[redact_cache_count].value = val;
                redact_cache[redact_cache_count].length = (int)strlen(val);
                redact_cache_count++;
            }
            tok = strtok(NULL, ",");
        }
    }

    /* NC_SECRET_VARS */
    const char *extra = getenv("NC_SECRET_VARS");
    if (extra) {
        static char sbuf[1024];
        strncpy(sbuf, extra, sizeof(sbuf) - 1);
        sbuf[sizeof(sbuf) - 1] = '\0';
        char *stok = strtok(sbuf, ",");
        while (stok && redact_cache_count < 58) {
            while (*stok == ' ') stok++;
            const char *val = getenv(stok);
            if (val && strlen(val) >= 5) {
                redact_cache[redact_cache_count].value = val;
                redact_cache[redact_cache_count].length = (int)strlen(val);
                redact_cache_count++;
            }
            stok = strtok(NULL, ",");
        }
    }

    /* Standard secret patterns */
    const char *patterns[] = {
        "API_KEY", "API_SECRET", "SECRET_KEY", "ACCESS_KEY",
        "AUTH_TOKEN", "PRIVATE_KEY", "CLIENT_SECRET",
        "DATABASE_PASSWORD", "DB_PASSWORD", "SMTP_PASSWORD", NULL
    };
    for (int i = 0; patterns[i] && redact_cache_count < 63; i++) {
        const char *val = getenv(patterns[i]);
        if (val && strlen(val) >= 5) {
            redact_cache[redact_cache_count].value = val;
            redact_cache[redact_cache_count].length = (int)strlen(val);
            redact_cache_count++;
        }
    }

    redact_cache_ready = true;
}

char *nc_redact_for_display(const char *text) {
    if (!text || !text[0]) return NULL;

    /* Initialize cache on first call */
    if (!redact_cache_ready) redact_cache_init();

    for (int i = 0; i < redact_cache_count; i++) {
        const char *val = redact_cache[i].value;
        if (!val) continue;

        const char *found = strstr(text, val);
        if (!found) continue;

        int vlen = (int)strlen(val);
        int tlen = (int)strlen(text);
        int show_chars = vlen > 8 ? 4 : 2;
        char *out = (char *)malloc(tlen + 16);
        if (!out) return NULL;

        int before = (int)(found - text);
        memcpy(out, text, before);
        memcpy(out + before, val, show_chars);
        memcpy(out + before + show_chars, "****", 4);
        int after_start = before + vlen;
        int after_len = tlen - after_start;
        if (after_len > 0) memcpy(out + before + show_chars + 4, text + after_start, after_len);
        out[before + show_chars + 4 + (after_len > 0 ? after_len : 0)] = '\0';
        return out;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  Data Format Parsers
 * ═══════════════════════════════════════════════════════════ */

/* ── YAML parser — supports flat, nested objects, and lists ── */

/* Count leading whitespace (spaces only, 2-space indent convention) */
static int yaml_indent(const char *line, const char *eol) {
    int n = 0;
    while (line + n < eol && line[n] == ' ') n++;
    return n;
}

/* Parse a YAML value string into a typed NcValue */
static NcValue yaml_parse_scalar(const char *val, int vlen) {
    if (vlen == 0) return NC_NONE();
    /* Booleans */
    if ((vlen == 4 && strncmp(val, "true", 4) == 0) ||
        (vlen == 3 && strncmp(val, "yes", 3) == 0))
        return NC_BOOL(true);
    if ((vlen == 5 && strncmp(val, "false", 5) == 0) ||
        (vlen == 2 && strncmp(val, "no", 2) == 0))
        return NC_BOOL(false);
    if (vlen == 4 && strncmp(val, "null", 4) == 0) return NC_NONE();
    /* Numbers */
    char tmp[64];
    if (vlen < 63) {
        memcpy(tmp, val, vlen); tmp[vlen] = '\0';
        char *end = NULL;
        long lv = strtol(tmp, &end, 10);
        if (end && end == tmp + vlen) return NC_INT(lv);
        double dv = strtod(tmp, &end);
        if (end && end == tmp + vlen) return NC_FLOAT(dv);
    }
    /* Quoted strings — strip quotes */
    if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                       (val[0] == '\'' && val[vlen-1] == '\'')))
        return NC_STRING(nc_string_new(val + 1, vlen - 2));
    return NC_STRING(nc_string_new(val, vlen));
}

/* Helper: get all lines as array of (start, end, indent) */
typedef struct { const char *start; const char *end; int indent; } YamlLine;

NcValue nc_stdlib_yaml_parse(const char *s) {
    /* Collect lines */
    YamlLine lines[4096];
    int line_count = 0;
    const char *p = s;
    while (*p && line_count < 4096) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        int ind = yaml_indent(p, eol);
        const char *content = p + ind;
        /* Skip empty lines and comments */
        if (content >= eol || *content == '#') {
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }
        lines[line_count].start = p;
        lines[line_count].end = eol;
        lines[line_count].indent = ind;
        line_count++;
        p = (*eol == '\n') ? eol + 1 : eol;
    }

    /* Recursive descent would be complex, so use iterative approach
     * with a parent stack for nested structures */
    NcMap *root = nc_map_new();
    NcMap *stack[32];      /* parent maps */
    int stack_indent[32];  /* indent level of each parent */
    int depth = 0;
    stack[0] = root;
    stack_indent[0] = -1;

    for (int i = 0; i < line_count; i++) {
        const char *c = lines[i].start + lines[i].indent;
        const char *eol = lines[i].end;
        int ind = lines[i].indent;

        /* Pop stack to correct parent for this indent level */
        while (depth > 0 && ind <= stack_indent[depth])
            depth--;

        NcMap *parent = stack[depth];

        /* Check if this is a list item: "- value" or "- key: value" */
        if (*c == '-' && (c + 1 >= eol || c[1] == ' ')) {
            const char *item = c + 1;
            while (item < eol && *item == ' ') item++;
            int ilen = (int)(eol - item);
            while (ilen > 0 && (item[ilen-1] == '\r' || item[ilen-1] == ' ')) ilen--;

            /* Check if item itself is "key: value" */
            const char *colon = NULL;
            for (const char *s2 = item; s2 < item + ilen; s2++)
                if (*s2 == ':') { colon = s2; break; }

            if (colon && colon > item && colon + 1 < item + ilen) {
                int klen = (int)(colon - item);
                while (klen > 0 && item[klen-1] == ' ') klen--;
                NcString *key = nc_string_new(item, klen);
                const char *val = colon + 1;
                while (*val == ' ') val++;
                int vlen = (int)((item + ilen) - val);
                nc_map_set(parent, key, yaml_parse_scalar(val, vlen));
            } else if (ilen > 0) {
                /* Simple list item — store with numeric index key */
                char idx_buf[16];
                snprintf(idx_buf, sizeof(idx_buf), "%d", parent->count);
                nc_map_set(parent, nc_string_from_cstr(idx_buf),
                           yaml_parse_scalar(item, ilen));
            }
            continue;
        }

        /* Regular key: value */
        const char *colon = NULL;
        for (const char *s2 = c; s2 < eol; s2++)
            if (*s2 == ':') { colon = s2; break; }

        if (colon && colon > c) {
            int klen = (int)(colon - c);
            while (klen > 0 && c[klen-1] == ' ') klen--;
            NcString *key = nc_string_new(c, klen);

            const char *val = colon + 1;
            while (val < eol && *val == ' ') val++;
            int vlen = (int)(eol - val);
            while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == ' ')) vlen--;

            if (vlen > 0) {
                /* Scalar value */
                nc_map_set(parent, key, yaml_parse_scalar(val, vlen));
            } else {
                /* Empty value — next lines at deeper indent are children */
                NcMap *child = nc_map_new();
                nc_map_set(parent, key, NC_MAP(child));
                if (depth < 30) {
                    depth++;
                    stack[depth] = child;
                    stack_indent[depth] = ind;
                }
            }
        }
    }

    return NC_MAP(root);
}

char *nc_stdlib_yaml_serialize(NcValue v, int indent) {
    (void)indent;
    return nc_json_serialize(v, true);
}

NcValue nc_stdlib_xml_parse(const char *s) {
    NcMap *m = nc_map_new();
    const char *p = s;
    while (*p) {
        const char *lt = strchr(p, '<');
        if (!lt) break;
        lt++;
        if (*lt == '?' || *lt == '!' || *lt == '/') { p = strchr(lt, '>'); if (p) p++; else break; continue; }
        char tag[256] = {0};
        int ti = 0;
        while (*lt && *lt != '>' && *lt != ' ' && *lt != '/' && ti < 254) tag[ti++] = *lt++;
        const char *gt = strchr(lt, '>');
        if (!gt) break;
        if (*(gt - 1) == '/') { p = gt + 1; continue; }
        char closing[264];
        snprintf(closing, sizeof(closing), "</%s>", tag);
        const char *end = strstr(gt + 1, closing);
        if (!end) { p = gt + 1; continue; }
        int clen = (int)(end - (gt + 1));
        char *content = malloc(clen + 1);
        if (!content) { p = end + strlen(closing); continue; }
        memcpy(content, gt + 1, clen);
        content[clen] = '\0';
        char *trimmed = content;
        while (*trimmed == ' ' || *trimmed == '\n' || *trimmed == '\r') trimmed++;
        int tlen = (int)strlen(trimmed);
        while (tlen > 0 && (trimmed[tlen-1] == ' ' || trimmed[tlen-1] == '\n')) tlen--;
        trimmed[tlen] = '\0';
        nc_map_set(m, nc_string_from_cstr(tag), NC_STRING(nc_string_from_cstr(trimmed)));
        free(content);
        p = end + strlen(closing);
    }
    return NC_MAP(m);
}

char *nc_stdlib_xml_serialize(NcValue v, const char *root, int indent) {
    (void)root; (void)indent;
    return nc_json_serialize(v, false);
}

NcValue nc_stdlib_csv_parse(const char *s) {
    NcList *rows = nc_list_new();
    const char *p = s;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r') eol++;
        NcList *fields = nc_list_new();
        const char *fp = p;
        while (fp < eol) {
            char buf[4096] = {0};
            int bi = 0;
            while (fp < eol && *fp != ',' && bi < 4090) buf[bi++] = *fp++;
            while (bi > 0 && buf[bi-1] == ' ') bi--;
            buf[bi] = '\0';
            nc_list_push(fields, NC_STRING(nc_string_from_cstr(buf)));
            if (*fp == ',') fp++;
        }
        if (fields->count > 0) nc_list_push(rows, NC_LIST(fields));
        p = eol;
        while (*p == '\n' || *p == '\r') p++;
    }
    return NC_LIST(rows);
}

char *nc_stdlib_csv_serialize(NcValue v) { return nc_json_serialize(v, false); }

NcValue nc_stdlib_toml_parse(const char *s) {
    NcMap *m = nc_map_new();
    const char *p = s;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        const char *c = p;
        while (c < eol && (*c == ' ' || *c == '\t')) c++;
        if (c >= eol || *c == '#') { p = (*eol == '\n') ? eol + 1 : eol; continue; }
        const char *eq = strchr(c, '=');
        if (eq && eq < eol) {
            int klen = (int)(eq - c);
            while (klen > 0 && c[klen-1] == ' ') klen--;
            NcString *key = nc_string_new(c, klen);
            const char *val = eq + 1;
            while (val < eol && *val == ' ') val++;
            int vlen = (int)(eol - val);
            while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == ' ')) vlen--;
            if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"')
                nc_map_set(m, key, NC_STRING(nc_string_new(val+1, vlen-2)));
            else
                nc_map_set(m, key, NC_STRING(nc_string_new(val, vlen)));
        }
        p = (*eol == '\n') ? eol + 1 : eol;
    }
    return NC_MAP(m);
}

NcValue nc_stdlib_ini_parse(const char *s) { return nc_stdlib_toml_parse(s); }

/* ═══════════════════════════════════════════════════════════
 *  Cache — thread-safe with mutex protection
 * ═══════════════════════════════════════════════════════════ */

#define NC_CACHE_SIZE 4096
typedef struct { char *key; NcValue value; bool used; } NcCacheEntry;
static NcCacheEntry nc_cache[NC_CACHE_SIZE];
static bool nc_cache_init = false;
static nc_mutex_t nc_cache_mutex = NC_MUTEX_INITIALIZER;

static uint32_t cache_hash(const char *key) {
    uint32_t h = 5381;
    while (*key) h = ((h << 5) + h) + (unsigned char)*key++;
    return h % NC_CACHE_SIZE;
}

NcValue nc_stdlib_cache_set(const char *key, NcValue value) {
    nc_mutex_lock(&nc_cache_mutex);
    if (!nc_cache_init) { memset(nc_cache, 0, sizeof(nc_cache)); nc_cache_init = true; }
    uint32_t idx = cache_hash(key);
    for (int i = 0; i < NC_CACHE_SIZE; i++) {
        uint32_t pos = (idx + i) % NC_CACHE_SIZE;
        if (!nc_cache[pos].used || (nc_cache[pos].key && strcmp(nc_cache[pos].key, key) == 0)) {
            free(nc_cache[pos].key);
            nc_cache[pos].key = strdup(key);
            nc_cache[pos].value = value;
            nc_cache[pos].used = true;
            nc_mutex_unlock(&nc_cache_mutex);
            return value;
        }
    }
    nc_mutex_unlock(&nc_cache_mutex);
    return value;
}

NcValue nc_stdlib_cache_get(const char *key) {
    nc_mutex_lock(&nc_cache_mutex);
    if (!nc_cache_init) { nc_mutex_unlock(&nc_cache_mutex); return NC_NONE(); }
    uint32_t idx = cache_hash(key);
    for (int i = 0; i < NC_CACHE_SIZE; i++) {
        uint32_t pos = (idx + i) % NC_CACHE_SIZE;
        if (!nc_cache[pos].used) { nc_mutex_unlock(&nc_cache_mutex); return NC_NONE(); }
        if (nc_cache[pos].key && strcmp(nc_cache[pos].key, key) == 0) {
            NcValue val = nc_cache[pos].value;
            nc_mutex_unlock(&nc_cache_mutex);
            return val;
        }
    }
    nc_mutex_unlock(&nc_cache_mutex);
    return NC_NONE();
}

bool nc_stdlib_cache_has(const char *key) {
    nc_mutex_lock(&nc_cache_mutex);
    if (!nc_cache_init) { nc_mutex_unlock(&nc_cache_mutex); return false; }
    uint32_t idx = cache_hash(key);
    for (int i = 0; i < NC_CACHE_SIZE; i++) {
        uint32_t pos = (idx + i) % NC_CACHE_SIZE;
        if (!nc_cache[pos].used) { nc_mutex_unlock(&nc_cache_mutex); return false; }
        if (nc_cache[pos].key && strcmp(nc_cache[pos].key, key) == 0) {
            nc_mutex_unlock(&nc_cache_mutex);
            return true;
        }
    }
    nc_mutex_unlock(&nc_cache_mutex);
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  RAG / Text helpers
 * ═══════════════════════════════════════════════════════════ */

int nc_stdlib_token_count(const char *text) {
    if (!text || !text[0]) return 0;
    int chars = (int)strlen(text);
    int words = 1;
    for (int i = 0; text[i]; i++)
        if (text[i] == ' ' || text[i] == '\n') words++;
    return (chars / 4 + words * 4 / 3) / 2;
}

NcValue nc_stdlib_chunk(NcString *text, int chunk_size, int overlap) {
    NcList *chunks = nc_list_new();
    if (!text || text->length == 0 || chunk_size <= 0) return NC_LIST(chunks);
    if (overlap < 0) overlap = 0;
    int pos = 0;
    while (pos < text->length) {
        int end = pos + chunk_size;
        if (end > text->length) end = text->length;
        nc_list_push(chunks, NC_STRING(nc_string_new(text->chars + pos, end - pos)));
        pos = end - overlap;
        if (pos <= (end - chunk_size)) pos = end;
    }
    return NC_LIST(chunks);
}

NcValue nc_stdlib_top_k(NcList *items, int k) {
    if (!items || items->count == 0 || k <= 0) return NC_LIST(nc_list_new());
    if (k > items->count) k = items->count;
    NcList *result = nc_list_new();
    for (int i = 0; i < k; i++) nc_list_push(result, items->items[i]);
    return NC_LIST(result);
}

NcValue nc_stdlib_find_similar(NcList *query, NcList *vecs, NcList *docs, int k) {
    if (!query || !vecs || !docs || k <= 0 || vecs->count == 0)
        return NC_LIST(nc_list_new());

    int qdim = query->count;
    int n = vecs->count;
    if (k > n) k = n;

    /* Compute cosine similarity between query and each vector */
    double *scores = calloc(n, sizeof(double));
    int *indices = calloc(n, sizeof(int));
    if (!scores || !indices) { free(scores); free(indices); return NC_LIST(nc_list_new()); }

    double q_mag = 0;
    for (int i = 0; i < qdim; i++) {
        double qv = IS_FLOAT(query->items[i]) ? AS_FLOAT(query->items[i]) :
                    IS_INT(query->items[i]) ? (double)AS_INT(query->items[i]) : 0;
        q_mag += qv * qv;
    }
    q_mag = sqrt(q_mag);
    if (q_mag == 0) q_mag = 1;

    for (int vi = 0; vi < n; vi++) {
        indices[vi] = vi;
        scores[vi] = 0;
        if (!IS_LIST(vecs->items[vi])) continue;
        NcList *vec = AS_LIST(vecs->items[vi]);
        double dot = 0, v_mag = 0;
        int dim = vec->count < qdim ? vec->count : qdim;
        for (int d = 0; d < dim; d++) {
            double qv = IS_FLOAT(query->items[d]) ? AS_FLOAT(query->items[d]) :
                        IS_INT(query->items[d]) ? (double)AS_INT(query->items[d]) : 0;
            double vv = IS_FLOAT(vec->items[d]) ? AS_FLOAT(vec->items[d]) :
                        IS_INT(vec->items[d]) ? (double)AS_INT(vec->items[d]) : 0;
            dot += qv * vv;
            v_mag += vv * vv;
        }
        v_mag = sqrt(v_mag);
        if (v_mag == 0) v_mag = 1;
        scores[vi] = dot / (q_mag * v_mag);
    }

    /* Partial sort: find top-k by score (selection sort on k elements) */
    for (int i = 0; i < k; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            double ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
            int ti = indices[i]; indices[i] = indices[best]; indices[best] = ti;
        }
    }

    NcList *result = nc_list_new();
    for (int i = 0; i < k; i++) {
        int idx = indices[i];
        NcMap *entry = nc_map_new();
        if (idx < docs->count) {
            nc_map_set(entry, nc_string_from_cstr("document"), docs->items[idx]);
        }
        nc_map_set(entry, nc_string_from_cstr("score"), NC_FLOAT(scores[i]));
        nc_map_set(entry, nc_string_from_cstr("index"), NC_INT(idx));
        nc_list_push(result, NC_MAP(entry));
    }

    free(scores);
    free(indices);
    return NC_LIST(result);
}

/* ═══════════════════════════════════════════════════════════
 *  Python Model Integration
 *
 *  load_model("model.pkl")  → loads a Python model, returns handle
 *  predict(model, features) → runs prediction, returns result
 *
 *  Supports: sklearn (.pkl), PyTorch (.pt/.pth), TensorFlow (.h5),
 *            ONNX (.onnx), joblib (.joblib), any pickle-based model.
 *
 *  Under the hood: spawns a persistent Python process that loads
 *  the model once and accepts prediction requests via stdin/stdout.
 * ═══════════════════════════════════════════════════════════ */

#define NC_MAX_MODELS 16

typedef struct {
    char   path[512];
    FILE  *proc_in;
    FILE  *proc_out;
    int    pid;
    bool   alive;
} NcModelHandle;

static NcModelHandle model_handles[NC_MAX_MODELS];
static int model_handle_count = 0;

NcValue nc_stdlib_load_model(const char *model_path) {
    if (!nc_path_is_safe(model_path)) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("This file path is not allowed for security reasons. Use a path within your project directory.")));
        return NC_MAP(err);
    }
    if (model_handle_count >= NC_MAX_MODELS) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Too many models loaded (max 16)")));
        return NC_MAP(err);
    }

    /* Detect model type by extension */
    const char *ext = strrchr(model_path, '.');
    if (!ext) ext = ".pkl";

    /* Build the Python loader script based on model type */
    const char *load_code;
    if (strcmp(ext, ".pt") == 0 || strcmp(ext, ".pth") == 0) {
        load_code =
            "import torch, json, sys\n"
            "model = torch.load('%s', map_location='cpu')\n"
            "model.eval()\n"
            "sys.stdout.write(json.dumps({'status':'loaded','type':'pytorch'})+'\\n')\n"
            "sys.stdout.flush()\n"
            "for line in sys.stdin:\n"
            "    data = json.loads(line)\n"
            "    inp = torch.tensor([data['features']], dtype=torch.float32)\n"
            "    with torch.no_grad(): out = model(inp)\n"
            "    result = out.tolist()[0]\n"
            "    if isinstance(result, list): pred = result\n"
            "    else: pred = [result]\n"
            "    sys.stdout.write(json.dumps({'prediction':pred})+'\\n')\n"
            "    sys.stdout.flush()\n";
    } else if (strcmp(ext, ".h5") == 0 || strcmp(ext, ".keras") == 0) {
        load_code =
            "import tensorflow as tf, json, sys\n"
            "model = tf.keras.models.load_model('%s')\n"
            "sys.stdout.write(json.dumps({'status':'loaded','type':'tensorflow'})+'\\n')\n"
            "sys.stdout.flush()\n"
            "for line in sys.stdin:\n"
            "    data = json.loads(line)\n"
            "    import numpy as np\n"
            "    inp = np.array([data['features']])\n"
            "    pred = model.predict(inp, verbose=0).tolist()[0]\n"
            "    sys.stdout.write(json.dumps({'prediction':pred})+'\\n')\n"
            "    sys.stdout.flush()\n";
    } else if (strcmp(ext, ".onnx") == 0) {
        load_code =
            "import onnxruntime as ort, json, sys, numpy as np\n"
            "sess = ort.InferenceSession('%s')\n"
            "inp_name = sess.get_inputs()[0].name\n"
            "sys.stdout.write(json.dumps({'status':'loaded','type':'onnx'})+'\\n')\n"
            "sys.stdout.flush()\n"
            "for line in sys.stdin:\n"
            "    data = json.loads(line)\n"
            "    inp = np.array([data['features']], dtype=np.float32)\n"
            "    pred = sess.run(None, {inp_name: inp})[0].tolist()[0]\n"
            "    sys.stdout.write(json.dumps({'prediction':pred})+'\\n')\n"
            "    sys.stdout.flush()\n";
    } else {
        load_code =
            "import json, sys, os\n"
            "model_path = '%s'\n"
            "if os.environ.get('NC_ALLOW_PICKLE', '0') != '1':\n"
            "    sys.stderr.write('[NC] ERROR: pickle loading is disabled by default (arbitrary code risk).\\n')\n"
            "    sys.stderr.write('[NC] Set NC_ALLOW_PICKLE=1 to enable, or use .onnx / .h5 format.\\n')\n"
            "    sys.stdout.write(json.dumps({'status':'error','error':'pickle disabled — set NC_ALLOW_PICKLE=1'})+'\\n')\n"
            "    sys.stdout.flush()\n"
            "    sys.exit(1)\n"
            "import pickle\n"
            "model = pickle.load(open(model_path,'rb'))\n"
            "sys.stdout.write(json.dumps({'status':'loaded','type':'sklearn'})+'\\n')\n"
            "sys.stdout.flush()\n"
            "for line in sys.stdin:\n"
            "    data = json.loads(line)\n"
            "    features = [data['features']]\n"
            "    pred = model.predict(features)\n"
            "    result = {'prediction': int(pred[0]) if hasattr(pred[0],'item') else pred[0]}\n"
            "    if hasattr(model, 'predict_proba'):\n"
            "        proba = model.predict_proba(features)[0]\n"
            "        result['confidence'] = round(float(max(proba)), 4)\n"
            "        result['probabilities'] = [round(float(p),4) for p in proba]\n"
            "    sys.stdout.write(json.dumps(result)+'\\n')\n"
            "    sys.stdout.flush()\n";
    }

    /* Write the loader script to a temp file */
    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s%c_nc_model_%d.py",
             nc_tempdir(), NC_PATH_SEP, model_handle_count);
    FILE *sf = fopen(script_path, "w");
    if (!sf) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Cannot create model loader script")));
        return NC_MAP(err);
    }
    /* Sanitize model_path to prevent format string and code injection.
     * Convert Windows backslashes to forward slashes to remain safe. */
    char safe_path[512];
    strncpy(safe_path, model_path, sizeof(safe_path) - 1);
    safe_path[sizeof(safe_path) - 1] = '\0';
    for (char *c = safe_path; *c; c++) {
        if (*c == '\\') *c = '/';
    }

    for (const char *c = safe_path; *c; c++) {
        if (*c == '\'' || *c == '"' || *c == '%') {
            fclose(sf);
            NcMap *err = nc_map_new();
            nc_map_set(err, nc_string_from_cstr("error"),
                NC_STRING(nc_string_from_cstr("Model path contains unsafe characters")));
            return NC_MAP(err);
        }
    }
    char script_buf[8192];
    snprintf(script_buf, sizeof(script_buf), load_code, safe_path);
    fprintf(sf, "%s", script_buf);
    fclose(sf);

    /* Spawn persistent Python process */
    const char *python_bin = getenv("NC_PYTHON");
    if (!python_bin || !python_bin[0]) python_bin = "python3";

    /* Launch Python as a bidirectional subprocess.
     * On all platforms, use popen for the read side and write via stdin. */
    char spawn_cmd[1024];
    snprintf(spawn_cmd, sizeof(spawn_cmd), "%s \"%s\"", python_bin, script_path);

#ifdef NC_WINDOWS
    /* Windows: use CreateProcess with redirected stdin/stdout pipes
     * for true bidirectional I/O with the child process. */
    HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;
    HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0);
    SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
    CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0);
    SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.hStdOutput = hChildStdoutWr;
    si.hStdInput = hChildStdinRd;
    si.dwFlags |= STARTF_USESTDHANDLES;

    char win_spawn[1024];
    snprintf(win_spawn, sizeof(win_spawn), "%s \"%s\"", python_bin, script_path);

    BOOL proc_ok = CreateProcessA(NULL, win_spawn, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);

    CloseHandle(hChildStdoutWr);
    CloseHandle(hChildStdinRd);

    FILE *child_out = NULL;
    FILE *child_in = NULL;
    int model_pid = 0;

    if (proc_ok) {
        int out_fd = _open_osfhandle((intptr_t)hChildStdoutRd, 0);
        int in_fd = _open_osfhandle((intptr_t)hChildStdinWr, 0);
        if (out_fd >= 0) child_out = _fdopen(out_fd, "r");
        if (in_fd >= 0) child_in = _fdopen(in_fd, "w");
        model_pid = (int)pi.dwProcessId;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        CloseHandle(hChildStdoutRd);
        CloseHandle(hChildStdinWr);
    }

    if (!child_out) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Could not start the model process on Windows.")));
        return NC_MAP(err);
    }
#else
    int to_child[2], from_child[2];
    if (pipe(to_child) < 0 || pipe(from_child) < 0) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Could not start the model process. Check that Python is installed and working.")));
        return NC_MAP(err);
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]);
        close(from_child[1]);
        execlp(python_bin, python_bin, script_path, NULL);
        _exit(1);
    }

    close(to_child[0]);
    close(from_child[1]);

    FILE *child_out = fdopen(from_child[0], "r");
    FILE *child_in = fdopen(to_child[1], "w");
    int model_pid = pid;
#endif

    char buf[4096] = {0};
    if (!fgets(buf, sizeof(buf), child_out)) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Model failed to load. Check the file path and Python dependencies.")));
        nc_map_set(err, nc_string_from_cstr("path"), NC_STRING(nc_string_from_cstr(model_path)));
        fclose(child_in);
        fclose(child_out);
        return NC_MAP(err);
    }

    /* Store handle */
    int handle_id = model_handle_count++;
    NcModelHandle *h = &model_handles[handle_id];
    strncpy(h->path, model_path, 511); h->path[511] = '\0';
    h->proc_in = child_in;
    h->proc_out = child_out;
    h->pid = model_pid;
    h->alive = true;

    /* Parse the loaded response and add handle info */
    NcValue loaded = nc_json_parse(buf);
    if (IS_MAP(loaded)) {
        nc_map_set(AS_MAP(loaded), nc_string_from_cstr("_handle"),
            NC_INT(handle_id));
        nc_map_set(AS_MAP(loaded), nc_string_from_cstr("path"),
            NC_STRING(nc_string_from_cstr(model_path)));
    }

    return loaded;
}

NcValue nc_stdlib_predict(NcValue model_handle, NcList *features) {
    /* Get the handle ID from the model map */
    int handle_id = -1;
    if (IS_MAP(model_handle)) {
        NcString *hk = nc_string_from_cstr("_handle");
        NcValue hv = nc_map_get(AS_MAP(model_handle), hk);
        nc_string_free(hk);
        if (IS_INT(hv)) handle_id = (int)AS_INT(hv);
    } else if (IS_INT(model_handle)) {
        handle_id = (int)AS_INT(model_handle);
    }

    if (handle_id < 0 || handle_id >= model_handle_count ||
        !model_handles[handle_id].alive) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("No model is loaded. Call load_model(\"your-model.pkl\") before calling predict().")));
        return NC_MAP(err);
    }

    NcModelHandle *h = &model_handles[handle_id];

    /* Build features JSON */
    NcMap *req = nc_map_new();
    nc_map_set(req, nc_string_from_cstr("features"), NC_LIST(features));
    char *req_json = nc_json_serialize(NC_MAP(req), false);

    /* Send to Python process */
    fprintf(h->proc_in, "%s\n", req_json);
    fflush(h->proc_in);
    free(req_json);

    /* Read result */
    char buf[4096] = {0};
    if (!fgets(buf, sizeof(buf), h->proc_out)) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Prediction failed. The model process stopped unexpectedly. Check your model file and Python setup.")));
        h->alive = false;
        return NC_MAP(err);
    }

    return nc_json_parse(buf);
}

void nc_stdlib_unload_model(int handle_id) {
    if (handle_id < 0 || handle_id >= model_handle_count) return;
    NcModelHandle *h = &model_handles[handle_id];
    if (h->alive) {
        if (h->proc_in) fclose(h->proc_in);
        if (h->proc_out) fclose(h->proc_out);
#ifdef NC_WINDOWS
        /* On Windows, closing pipes is sufficient to terminate child */
#else
        if (h->pid > 0) kill(h->pid, SIGTERM);
#endif
        h->alive = false;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Conversation Memory — for chatbots and multi-turn AI
 *
 *  memory_new()                    → create a conversation
 *  memory_add(mem, role, content)  → add a message
 *  memory_get(mem)                 → get conversation as list
 *  memory_clear(mem)               → reset conversation
 *  memory_summary(mem)             → get as single string for context
 * ═══════════════════════════════════════════════════════════ */

#define NC_MAX_MEMORIES 64

typedef struct {
    NcList *messages;
    int     max_turns;
    bool    active;
} NcMemory;

static NcMemory memories[NC_MAX_MEMORIES];
static int memory_count = 0;

NcValue nc_stdlib_memory_new(int max_turns) {
    if (memory_count >= NC_MAX_MEMORIES) return NC_INT(-1);
    int id = memory_count++;
    memories[id].messages = nc_list_new();
    memories[id].max_turns = max_turns > 0 ? max_turns : 50;
    memories[id].active = true;

    NcMap *handle = nc_map_new();
    nc_map_set(handle, nc_string_from_cstr("_memory_id"), NC_INT(id));
    nc_map_set(handle, nc_string_from_cstr("max_turns"), NC_INT(memories[id].max_turns));
    return NC_MAP(handle);
}

static int get_memory_id(NcValue handle) {
    if (IS_INT(handle)) return (int)AS_INT(handle);
    if (IS_MAP(handle)) {
        NcString *k = nc_string_from_cstr("_memory_id");
        NcValue v = nc_map_get(AS_MAP(handle), k);
        nc_string_free(k);
        if (IS_INT(v)) return (int)AS_INT(v);
    }
    return -1;
}

NcValue nc_stdlib_memory_add(NcValue handle, const char *role, const char *content) {
    int id = get_memory_id(handle);
    if (id < 0 || id >= memory_count || !memories[id].active) return NC_BOOL(false);

    NcMap *msg = nc_map_new();
    nc_map_set(msg, nc_string_from_cstr("role"), NC_STRING(nc_string_from_cstr(role)));
    nc_map_set(msg, nc_string_from_cstr("content"), NC_STRING(nc_string_from_cstr(content)));
    nc_list_push(memories[id].messages, NC_MAP(msg));

    /* Trim old messages if over limit (keep system + last N) */
    while (memories[id].messages->count > memories[id].max_turns) {
        nc_value_release(memories[id].messages->items[0]);
        for (int i = 0; i < memories[id].messages->count - 1; i++)
            memories[id].messages->items[i] = memories[id].messages->items[i + 1];
        memories[id].messages->count--;
    }

    return NC_BOOL(true);
}

NcValue nc_stdlib_memory_get(NcValue handle) {
    int id = get_memory_id(handle);
    if (id < 0 || id >= memory_count || !memories[id].active) return NC_LIST(nc_list_new());
    return NC_LIST(memories[id].messages);
}

NcValue nc_stdlib_memory_clear(NcValue handle) {
    int id = get_memory_id(handle);
    if (id < 0 || id >= memory_count) return NC_BOOL(false);
    nc_list_free(memories[id].messages);
    memories[id].messages = nc_list_new();
    return NC_BOOL(true);
}

NcValue nc_stdlib_memory_summary(NcValue handle) {
    int id = get_memory_id(handle);
    if (id < 0 || id >= memory_count || !memories[id].active)
        return NC_STRING(nc_string_from_cstr(""));

    char buf[8192] = {0};
    int pos = 0;
    NcList *msgs = memories[id].messages;
    for (int i = 0; i < msgs->count && pos < 8000; i++) {
        if (IS_MAP(msgs->items[i])) {
            NcMap *m = AS_MAP(msgs->items[i]);
            NcString *rk = nc_string_from_cstr("role");
            NcString *ck = nc_string_from_cstr("content");
            NcValue rv = nc_map_get(m, rk);
            NcValue cv = nc_map_get(m, ck);
            nc_string_free(rk); nc_string_free(ck);
            if (IS_STRING(rv) && IS_STRING(cv)) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: %s\n",
                    AS_STRING(rv)->chars, AS_STRING(cv)->chars);
            }
        }
    }
    return NC_STRING(nc_string_from_cstr(buf));
}

/* ═══════════════════════════════════════════════════════════
 *  Persistent Long-Term Memory + Policy Memory
 *
 *  These primitives keep memory entirely in the NC/C core:
 *    - Episodic / procedural / user / project memory entries
 *    - Reflection records with worked/failed/confidence/next action
 *    - Lightweight semantic retrieval via hashed text embeddings
 *    - Reward-scored policy memory via UCB / epsilon-greedy selection
 *
 *  File format is plain JSON so NC code can inspect and manipulate it.
 * ═══════════════════════════════════════════════════════════ */

#define NC_MEMORY_VEC_DIM 32

static NcValue nc_stdlib_map_get_cstr(NcMap *map, const char *key) {
    NcString *k;
    NcValue value;

    if (!map || !key) return NC_NONE();
    k = nc_string_from_cstr(key);
    value = nc_map_get(map, k);
    nc_string_free(k);
    return value;
}

static double nc_stdlib_value_as_number(NcValue value, double fallback) {
    if (IS_FLOAT(value)) return AS_FLOAT(value);
    if (IS_INT(value)) return (double)AS_INT(value);
    if (IS_BOOL(value)) return AS_BOOL(value) ? 1.0 : 0.0;
    return fallback;
}

static void nc_stdlib_memory_embed_text(const char *text, float *out, int dim) {
    uint32_t hash = 2166136261u;
    char token[128];
    int token_len = 0;

    for (int i = 0; i < dim; i++) out[i] = 0.0f;
    if (!text || !text[0]) return;

    for (const unsigned char *p = (const unsigned char *)text;; p++) {
        unsigned char ch = *p;
        if (isalnum(ch) || ch == '_') {
            if (token_len < (int)sizeof(token) - 1) {
                token[token_len++] = (char)tolower(ch);
            }
        } else {
            if (token_len > 0) {
                token[token_len] = '\0';
                hash = 2166136261u;
                for (int i = 0; i < token_len; i++) {
                    hash ^= (uint32_t)(unsigned char)token[i];
                    hash *= 16777619u;
                }
                out[hash % dim] += 1.0f;
                out[(hash >> 7) % dim] += 0.5f;
                out[(hash >> 13) % dim] += 0.25f;
                token_len = 0;
            }
            if (ch == '\0') break;
        }
    }

    {
        double norm = 0.0;
        for (int i = 0; i < dim; i++) norm += (double)out[i] * (double)out[i];
        norm = sqrt(norm);
        if (norm > 0.0) {
            for (int i = 0; i < dim; i++) out[i] = (float)(out[i] / norm);
        }
    }
}

static NcList *nc_stdlib_memory_vector_list(const char *text) {
    float vec[NC_MEMORY_VEC_DIM];
    NcList *list = nc_list_new();

    nc_stdlib_memory_embed_text(text, vec, NC_MEMORY_VEC_DIM);
    for (int i = 0; i < NC_MEMORY_VEC_DIM; i++) {
        nc_list_push(list, NC_FLOAT(vec[i]));
    }
    return list;
}

static NcMap *nc_stdlib_memory_root_new(const char *type) {
    NcMap *root = nc_map_new();
    nc_map_set(root, nc_string_from_cstr("version"), NC_INT(1));
    nc_map_set(root, nc_string_from_cstr("type"), NC_STRING(nc_string_from_cstr(type)));
    nc_map_set(root, nc_string_from_cstr("entries"), NC_LIST(nc_list_new()));
    return root;
}

static NcMap *nc_stdlib_memory_root_load(const char *path, const char *type) {
    NcValue raw;
    NcValue parsed;

    if (!path || !path[0]) return nc_stdlib_memory_root_new(type);

    raw = nc_stdlib_read_file(path);
    if (!IS_STRING(raw)) return nc_stdlib_memory_root_new(type);

    parsed = nc_json_parse(AS_STRING(raw)->chars);
    if (IS_MAP(parsed)) {
        NcValue entries = nc_stdlib_map_get_cstr(AS_MAP(parsed), "entries");
        if (IS_LIST(entries)) return AS_MAP(parsed);
    }
    if (IS_LIST(parsed)) {
        NcMap *root = nc_stdlib_memory_root_new(type);
        nc_map_set(root, nc_string_from_cstr("entries"), parsed);
        return root;
    }
    return nc_stdlib_memory_root_new(type);
}

static bool nc_stdlib_memory_root_save(const char *path, NcMap *root) {
    char *json;
    NcValue ok;

    if (!path || !path[0] || !root) return false;
    json = nc_json_serialize(NC_MAP(root), false);
    if (!json) return false;
    ok = nc_stdlib_write_file_atomic(path, json);
    free(json);
    return IS_BOOL(ok) && AS_BOOL(ok);
}

static NcList *nc_stdlib_memory_entries(NcMap *root) {
    NcValue entries = nc_stdlib_map_get_cstr(root, "entries");
    if (IS_LIST(entries)) return AS_LIST(entries);
    {
        NcList *list = nc_list_new();
        nc_map_set(root, nc_string_from_cstr("entries"), NC_LIST(list));
        return list;
    }
}

static NcMap *nc_stdlib_policy_root_new(void) {
    NcMap *root = nc_map_new();
    nc_map_set(root, nc_string_from_cstr("version"), NC_INT(1));
    nc_map_set(root, nc_string_from_cstr("type"), NC_STRING(nc_string_from_cstr("nc_policy_memory")));
    nc_map_set(root, nc_string_from_cstr("actions"), NC_MAP(nc_map_new()));
    return root;
}

static NcMap *nc_stdlib_policy_root_load(const char *path) {
    NcValue raw;
    NcValue parsed;

    if (!path || !path[0]) return nc_stdlib_policy_root_new();

    raw = nc_stdlib_read_file(path);
    if (!IS_STRING(raw)) return nc_stdlib_policy_root_new();

    parsed = nc_json_parse(AS_STRING(raw)->chars);
    if (IS_MAP(parsed)) {
        NcValue actions = nc_stdlib_map_get_cstr(AS_MAP(parsed), "actions");
        if (IS_MAP(actions)) return AS_MAP(parsed);
    }
    return nc_stdlib_policy_root_new();
}

static NcMap *nc_stdlib_policy_actions(NcMap *root) {
    NcValue actions = nc_stdlib_map_get_cstr(root, "actions");
    if (IS_MAP(actions)) return AS_MAP(actions);
    {
        NcMap *map = nc_map_new();
        nc_map_set(root, nc_string_from_cstr("actions"), NC_MAP(map));
        return map;
    }
}

static bool nc_stdlib_policy_root_save(const char *path, NcMap *root) {
    char *json;
    NcValue ok;

    if (!path || !path[0] || !root) return false;
    json = nc_json_serialize(NC_MAP(root), false);
    if (!json) return false;
    ok = nc_stdlib_write_file_atomic(path, json);
    free(json);
    return IS_BOOL(ok) && AS_BOOL(ok);
}

NcValue nc_stdlib_memory_save(NcValue handle, const char *path) {
    int id = get_memory_id(handle);
    NcMap *root;

    if (id < 0 || id >= memory_count || !memories[id].active) return NC_BOOL(false);

    root = nc_stdlib_memory_root_new("nc_conversation_memory");
    nc_map_set(root, nc_string_from_cstr("max_turns"), NC_INT(memories[id].max_turns));
    nc_map_set(root, nc_string_from_cstr("entries"), NC_LIST(memories[id].messages));
    return NC_BOOL(nc_stdlib_memory_root_save(path, root));
}

NcValue nc_stdlib_memory_load(const char *path, int max_turns) {
    NcMap *root = nc_stdlib_memory_root_load(path, "nc_conversation_memory");
    NcList *entries = nc_stdlib_memory_entries(root);
    NcValue handle = nc_stdlib_memory_new(max_turns);
    int id = get_memory_id(handle);
    NcValue stored_max;

    if (id < 0 || !IS_MAP(handle)) return NC_BOOL(false);

    stored_max = nc_stdlib_map_get_cstr(root, "max_turns");
    if (max_turns <= 0 && IS_INT(stored_max)) {
        memories[id].max_turns = (int)AS_INT(stored_max);
        nc_map_set(AS_MAP(handle), nc_string_from_cstr("max_turns"), NC_INT(memories[id].max_turns));
    }
    nc_list_free(memories[id].messages);
    memories[id].messages = entries;
    return handle;
}

NcValue nc_stdlib_memory_store(const char *path, const char *kind,
                               const char *content, NcValue metadata,
                               double reward) {
    NcMap *root;
    NcList *entries;
    NcMap *entry;

    if (!path || !kind || !content) return NC_BOOL(false);

    root = nc_stdlib_memory_root_load(path, "nc_long_term_memory");
    entries = nc_stdlib_memory_entries(root);
    entry = nc_map_new();

    nc_map_set(entry, nc_string_from_cstr("timestamp"), NC_FLOAT((double)time(NULL)));
    nc_map_set(entry, nc_string_from_cstr("kind"), NC_STRING(nc_string_from_cstr(kind)));
    nc_map_set(entry, nc_string_from_cstr("content"), NC_STRING(nc_string_from_cstr(content)));
    nc_map_set(entry, nc_string_from_cstr("metadata"), metadata);
    nc_map_set(entry, nc_string_from_cstr("reward"), NC_FLOAT(reward));
    nc_map_set(entry, nc_string_from_cstr("vector"), NC_LIST(nc_stdlib_memory_vector_list(content)));
    nc_list_push(entries, NC_MAP(entry));

    return NC_BOOL(nc_stdlib_memory_root_save(path, root));
}

NcValue nc_stdlib_memory_search(const char *path, const char *query, int top_k) {
    NcMap *root;
    NcList *entries;
    NcList *docs;
    NcList *vecs;
    NcList *query_vec;

    if (!path || !query || top_k <= 0) return NC_LIST(nc_list_new());

    root = nc_stdlib_memory_root_load(path, "nc_long_term_memory");
    entries = nc_stdlib_memory_entries(root);
    docs = nc_list_new();
    vecs = nc_list_new();
    query_vec = nc_stdlib_memory_vector_list(query);

    for (int i = 0; i < entries->count; i++) {
        NcValue item = entries->items[i];
        NcValue vec;
        NcValue content;
        if (!IS_MAP(item)) continue;
        vec = nc_stdlib_map_get_cstr(AS_MAP(item), "vector");
        if (!IS_LIST(vec)) {
            content = nc_stdlib_map_get_cstr(AS_MAP(item), "content");
            if (IS_STRING(content)) {
                vec = NC_LIST(nc_stdlib_memory_vector_list(AS_STRING(content)->chars));
                nc_map_set(AS_MAP(item), nc_string_from_cstr("vector"), vec);
            } else {
                vec = NC_LIST(nc_stdlib_memory_vector_list(""));
            }
        }
        nc_list_push(docs, item);
        nc_list_push(vecs, vec);
    }

    return nc_stdlib_find_similar(query_vec, vecs, docs, top_k);
}

NcValue nc_stdlib_memory_context(const char *path, const char *query, int top_k) {
    NcValue results = nc_stdlib_memory_search(path, query, top_k);
    char buf[16384] = {0};
    int pos = 0;

    if (!IS_LIST(results)) return NC_STRING(nc_string_from_cstr(""));

    for (int i = 0; i < AS_LIST(results)->count && pos < (int)sizeof(buf) - 256; i++) {
        NcValue item = AS_LIST(results)->items[i];
        NcValue entry;
        NcValue kind;
        NcValue content;
        NcValue reward;
        NcValue score;

        if (!IS_MAP(item)) continue;
        score = nc_stdlib_map_get_cstr(AS_MAP(item), "score");
        entry = nc_stdlib_map_get_cstr(AS_MAP(item), "document");
        if (!IS_MAP(entry)) continue;
        kind = nc_stdlib_map_get_cstr(AS_MAP(entry), "kind");
        content = nc_stdlib_map_get_cstr(AS_MAP(entry), "content");
        reward = nc_stdlib_map_get_cstr(AS_MAP(entry), "reward");
        if (!IS_STRING(content)) continue;

        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                        "[%s score=%.3f reward=%.2f] %s\n",
                        IS_STRING(kind) ? AS_STRING(kind)->chars : "memory",
                        nc_stdlib_value_as_number(score, 0.0),
                        nc_stdlib_value_as_number(reward, 0.0),
                        AS_STRING(content)->chars);
    }

    return NC_STRING(nc_string_from_cstr(buf));
}

NcValue nc_stdlib_memory_reflect(const char *path, const char *task,
                                 const char *worked, const char *failed,
                                 double confidence, const char *next_action) {
    NcMap *metadata;
    char summary[4096];

    if (!path || !task || !worked || !failed || !next_action) return NC_BOOL(false);

    metadata = nc_map_new();
    nc_map_set(metadata, nc_string_from_cstr("task"), NC_STRING(nc_string_from_cstr(task)));
    nc_map_set(metadata, nc_string_from_cstr("worked"), NC_STRING(nc_string_from_cstr(worked)));
    nc_map_set(metadata, nc_string_from_cstr("failed"), NC_STRING(nc_string_from_cstr(failed)));
    nc_map_set(metadata, nc_string_from_cstr("confidence"), NC_FLOAT(confidence));
    nc_map_set(metadata, nc_string_from_cstr("next_action"), NC_STRING(nc_string_from_cstr(next_action)));

    snprintf(summary, sizeof(summary),
             "task=%s | worked=%s | failed=%s | confidence=%.2f | next=%s",
             task, worked, failed, confidence, next_action);
    return nc_stdlib_memory_store(path, "reflection", summary, NC_MAP(metadata), confidence);
}

NcValue nc_stdlib_policy_update(const char *path, const char *action, double reward) {
    NcMap *root;
    NcMap *actions;
    NcString *action_key;
    NcValue existing;
    NcMap *stats;
    double count;
    double reward_sum;
    double avg_reward;
    double wins;
    double losses;

    if (!path || !action) return NC_BOOL(false);

    root = nc_stdlib_policy_root_load(path);
    actions = nc_stdlib_policy_actions(root);
    action_key = nc_string_from_cstr(action);
    existing = nc_map_get(actions, action_key);
    nc_string_free(action_key);

    if (IS_MAP(existing)) stats = AS_MAP(existing);
    else {
        stats = nc_map_new();
        nc_map_set(actions, nc_string_from_cstr(action), NC_MAP(stats));
    }

    count = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "count"), 0.0) + 1.0;
    reward_sum = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "reward_sum"), 0.0) + reward;
    avg_reward = reward_sum / (count > 0.0 ? count : 1.0);
    wins = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "wins"), 0.0);
    losses = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "losses"), 0.0);
    if (reward > 0.0) wins += 1.0;
    else if (reward < 0.0) losses += 1.0;

    nc_map_set(stats, nc_string_from_cstr("count"), NC_FLOAT(count));
    nc_map_set(stats, nc_string_from_cstr("reward_sum"), NC_FLOAT(reward_sum));
    nc_map_set(stats, nc_string_from_cstr("avg_reward"), NC_FLOAT(avg_reward));
    nc_map_set(stats, nc_string_from_cstr("last_reward"), NC_FLOAT(reward));
    nc_map_set(stats, nc_string_from_cstr("last_timestamp"), NC_FLOAT((double)time(NULL)));
    nc_map_set(stats, nc_string_from_cstr("wins"), NC_FLOAT(wins));
    nc_map_set(stats, nc_string_from_cstr("losses"), NC_FLOAT(losses));

    if (!nc_stdlib_policy_root_save(path, root)) return NC_BOOL(false);
    return NC_MAP(stats);
}

NcValue nc_stdlib_policy_choose(const char *path, NcList *actions, double epsilon) {
    static int seeded = 0;
    NcMap *root;
    NcMap *policy_actions;
    NcMap *result;
    double total_count = 0.0;
    int best_idx = -1;
    double best_score = -1e30;
    const char *strategy = "ucb";

    if (!actions || actions->count == 0 || !path) return NC_NONE();
    if (!seeded) {
        srand((unsigned)time(NULL) ^ (unsigned)clock());
        seeded = 1;
    }

    root = nc_stdlib_policy_root_load(path);
    policy_actions = nc_stdlib_policy_actions(root);
    for (int i = 0; i < policy_actions->count; i++) {
        if (!IS_MAP(policy_actions->values[i])) continue;
        total_count += nc_stdlib_value_as_number(
            nc_stdlib_map_get_cstr(AS_MAP(policy_actions->values[i]), "count"), 0.0);
    }

    if (epsilon > 0.0 && ((double)rand() / (double)RAND_MAX) < epsilon) {
        best_idx = rand() % actions->count;
        strategy = "epsilon";
    } else {
        for (int i = 0; i < actions->count; i++) {
            NcValue action_val = actions->items[i];
            NcString *action_key;
            NcValue stat_val;
            double score;

            if (!IS_STRING(action_val)) continue;
            action_key = nc_string_from_cstr(AS_STRING(action_val)->chars);
            stat_val = nc_map_get(policy_actions, action_key);
            nc_string_free(action_key);

            if (!IS_MAP(stat_val)) {
                best_idx = i;
                best_score = 1e30;
                break;
            }

            {
                NcMap *stats = AS_MAP(stat_val);
                double count = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "count"), 0.0);
                double avg = nc_stdlib_value_as_number(nc_stdlib_map_get_cstr(stats, "avg_reward"), 0.0);
                double bonus = sqrt(2.0 * log(total_count + 1.0) / (count + 1e-9));
                score = avg + bonus;
            }

            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
    }

    if (best_idx < 0 || best_idx >= actions->count || !IS_STRING(actions->items[best_idx])) {
        return NC_NONE();
    }

    result = nc_map_new();
    nc_map_set(result, nc_string_from_cstr("action"), actions->items[best_idx]);
    nc_map_set(result, nc_string_from_cstr("strategy"), NC_STRING(nc_string_from_cstr(strategy)));
    nc_map_set(result, nc_string_from_cstr("score"), NC_FLOAT(best_score));
    return NC_MAP(result);
}

NcValue nc_stdlib_policy_stats(const char *path) {
    NcMap *root;
    if (!path || !path[0]) return NC_MAP(nc_map_new());
    root = nc_stdlib_policy_root_load(path);
    return NC_MAP(root);
}

/* ═══════════════════════════════════════════════════════════
 *  Response Validation — validate AI responses match a schema
 *
 *  validate(response, ["field1", "field2"])  → check required fields
 *  validate_type(value, "text|number|list")  → check value type
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_validate(NcValue response, NcList *required_fields) {
    if (!IS_MAP(response)) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("valid"), NC_BOOL(false));
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Response is not a record/map")));
        return NC_MAP(err);
    }

    NcMap *rmap = AS_MAP(response);
    NcList *missing = nc_list_new();

    for (int i = 0; i < required_fields->count; i++) {
        if (IS_STRING(required_fields->items[i])) {
            NcString *field = AS_STRING(required_fields->items[i]);
            if (!nc_map_has(rmap, field))
                nc_list_push(missing, NC_STRING(nc_string_ref(field)));
        }
    }

    NcMap *result = nc_map_new();
    nc_map_set(result, nc_string_from_cstr("valid"), NC_BOOL(missing->count == 0));
    nc_map_set(result, nc_string_from_cstr("missing"), NC_LIST(missing));
    nc_map_set(result, nc_string_from_cstr("field_count"), NC_INT(rmap->count));
    return NC_MAP(result);
}

/* ═══════════════════════════════════════════════════════════
 *  AI Retry with Fallback Models
 *
 *  ai_with_fallback(prompt, context, models_list)
 *  Tries each model in order until one succeeds.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_ai_with_fallback(const char *prompt, NcMap *context, NcList *models) {
    for (int i = 0; i < models->count; i++) {
        if (!IS_STRING(models->items[i])) continue;
        const char *model = AS_STRING(models->items[i])->chars;

        NC_INFO("Trying model: %s", model);
        NcValue result = nc_ai_call(prompt, context, model);

        /* Check if it's an error */
        if (IS_MAP(result)) {
            NcString *ek = nc_string_from_cstr("error");
            NcValue ev = nc_map_get(AS_MAP(result), ek);
            nc_string_free(ek);
            if (IS_STRING(ev)) {
                NC_WARN("Model %s failed: %s — trying next", model, AS_STRING(ev)->chars);
                continue;
            }
        }

        /* Success */
        if (!IS_NONE(result)) {
            if (IS_MAP(result))
                nc_map_set(AS_MAP(result), nc_string_from_cstr("_model_used"),
                    NC_STRING(nc_string_from_cstr(model)));
            return result;
        }
    }

    NcMap *err = nc_map_new();
    nc_map_set(err, nc_string_from_cstr("error"),
        NC_STRING(nc_string_from_cstr("All models failed")));
    return NC_MAP(err);
}

/* ═══════════════════════════════════════════════════════════
 *  Cross-language execution
 *
 *  NC can call any language — Python, Node, Go, Rust, Java,
 *  Ruby, Bash, or any executable. This is a core language
 *  feature, not a vendor integration.
 *
 *  NC code:
 *    set result to exec("python3", "predict.py", data)
 *    set output to shell("ls -la /app")
 *
 *  The exec() function runs an external process, captures
 *  stdout, and returns it as a string. If the output is
 *  valid JSON, it's automatically parsed into an NC value.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_exec(NcList *args) {
    if (!args || args->count < 1) return NC_NONE();

    if (!nc_sandbox_check("exec")) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Running external programs is not allowed. Set NC_ALLOW_EXEC=1 to enable.")));
        return NC_MAP(err);
    }

    /* Calculate total length needed for safe allocation */
    size_t total_len = 0;
    for (int i = 0; i < args->count; i++) {
        NcString *arg = nc_value_to_string(args->items[i]);
        total_len += (size_t)arg->length * 4 + 3;
        nc_string_free(arg);
    }
    total_len += args->count + 1;

    size_t cmd_cap = total_len < 8192 ? 8192 : total_len;
    char *cmd = calloc(cmd_cap, 1);
    if (!cmd) return NC_NONE();

    int pos = 0;
    for (int i = 0; i < args->count && pos < (int)cmd_cap - 4; i++) {
        if (i > 0) cmd[pos++] = ' ';
        NcString *arg = nc_value_to_string(args->items[i]);

        /* Shell-escape each argument: wrap in single quotes,
         * escaping embedded single quotes as '\'' */
        cmd[pos++] = '\'';
        for (int j = 0; j < arg->length && pos < (int)cmd_cap - 6; j++) {
            if (arg->chars[j] == '\'') {
                cmd[pos++] = '\'';
                cmd[pos++] = '\\';
                cmd[pos++] = '\'';
                cmd[pos++] = '\'';
            } else {
                cmd[pos++] = arg->chars[j];
            }
        }
        cmd[pos++] = '\'';
        nc_string_free(arg);
    }
    cmd[pos] = '\0';

    NcValue result = nc_stdlib_shell(cmd);
    free(cmd);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Generic HTTP request — for direct HTTP calls from NC code
 *
 *  NC code:
 *    set result to http_request("POST", "https://api.example.com/data",
 *                               {"Authorization": "Bearer " + token},
 *                               {"key": "value"})
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_http_request(const char *method, const char *url,
                                NcMap *headers, NcValue body) {
    char header_str[2048] = {0};
    int hpos = 0;
    bool has_user_agent = false;
    if (headers) {
        for (int i = 0; i < headers->count && hpos < (int)sizeof(header_str) - 256; i++) {
            NcString *vs = nc_value_to_string(headers->values[i]);
            const char *raw_val = vs->chars;
            if (strncmp(raw_val, "env:", 4) == 0) {
                const char *env_val = getenv(raw_val + 4);
                if (env_val) raw_val = env_val;
            }
            if (strcasecmp(headers->keys[i]->chars, "User-Agent") == 0)
                has_user_agent = true;
            hpos += snprintf(header_str + hpos, sizeof(header_str) - hpos,
                             "%s%s: %s", hpos > 0 ? "\n" : "",
                             headers->keys[i]->chars, raw_val);
            nc_string_free(vs);
        }
    }
    /* Set a default User-Agent if none provided to avoid rate limiting */
    if (!has_user_agent && hpos < (int)sizeof(header_str) - 128) {
        hpos += snprintf(header_str + hpos, sizeof(header_str) - hpos,
                         "%sUser-Agent: %s", hpos > 0 ? "\n" : "", nc_get_user_agent());
    }
    /* Add Accept header if not provided */
    bool has_accept = false;
    if (headers) {
        for (int i = 0; i < headers->count; i++)
            if (strcasecmp(headers->keys[i]->chars, "Accept") == 0)
                has_accept = true;
    }
    if (!has_accept && hpos < (int)sizeof(header_str) - 128) {
        hpos += snprintf(header_str + hpos, sizeof(header_str) - hpos,
                         "%sAccept: application/json, text/plain, */*", hpos > 0 ? "\n" : "");
    }

    const char *body_str = NULL;
    char *body_alloc = NULL;
    if (IS_STRING(body)) {
        body_str = AS_STRING(body)->chars;
    } else if (IS_MAP(body) || IS_LIST(body)) {
        body_alloc = nc_json_serialize(body, false);
        body_str = body_alloc;
    }

    char *response = NULL;
    if (strcasecmp(method, "GET") == 0) {
        response = nc_http_get(url, header_str[0] ? header_str : NULL);
    } else if (strcasecmp(method, "POST") == 0) {
        response = nc_http_post(url, body_str ? body_str : "",
                                "application/json",
                                header_str[0] ? header_str : NULL);
    } else {
        /* PUT, DELETE, PATCH, etc. — use generic nc_http_request */
        extern char *nc_http_request(const char *, const char *, const char *,
                                     const char *, const char *);
        response = nc_http_request(method, url, body_str ? body_str : "",
                                   "application/json",
                                   header_str[0] ? header_str : NULL);
    }
    free(body_alloc);

    if (response) {
        int rlen = (int)strlen(response);
        /* Strip trailing whitespace */
        while (rlen > 0 && (response[rlen-1] == '\r' || response[rlen-1] == '\n' ||
               response[rlen-1] == ' ' || response[rlen-1] == '\t'))
            response[--rlen] = '\0';
    }
    /* Skip leading whitespace/BOM for JSON parsing */
    const char *json_start = response;
    if (json_start) {
        /* Skip UTF-8 BOM if present */
        if ((unsigned char)json_start[0] == 0xEF &&
            (unsigned char)json_start[1] == 0xBB &&
            (unsigned char)json_start[2] == 0xBF)
            json_start += 3;
        while (*json_start == ' ' || *json_start == '\t' ||
               *json_start == '\r' || *json_start == '\n')
            json_start++;
    }
    NcValue result = nc_json_parse(json_start);
    if (IS_NONE(result) && response) result = NC_STRING(nc_string_from_cstr(response));
    free(response);
    return result;
}

/* Internal helper: run a command and capture output + exit code */
static NcValue shell_run_internal(const char *command, char **out_buf, size_t *out_len, int *out_exit_code) {
    if (!command || !command[0]) return NC_NONE();

    if (!nc_sandbox_check("exec")) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("shell() blocked by sandbox (allow_exec is false)")));
        return NC_MAP(err);
    }

    char full_cmd[8320];
    snprintf(full_cmd, sizeof(full_cmd), "%s 2>&1", command);

    FILE *proc = nc_popen(full_cmd, "r");
    if (!proc) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Failed to start process")));
        nc_map_set(err, nc_string_from_cstr("command"),
            NC_STRING(nc_string_from_cstr(command)));
        return NC_MAP(err);
    }

    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        nc_pclose(proc);
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("Out of memory")));
        return NC_MAP(err);
    }
    while (1) {
        size_t n = fread(buf + len, 1, cap - len - 1, proc);
        if (n == 0) break;
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); nc_pclose(proc); return NC_NONE(); }
            buf = tmp;
        }
    }
    buf[len] = '\0';

    int exit_code = nc_pclose(proc);

    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';

    *out_buf = buf;
    *out_len = len;
    *out_exit_code = exit_code;
    return NC_NONE(); /* sentinel: no error */
}

NcValue nc_stdlib_shell(const char *command) {
    char *buf = NULL;
    size_t len = 0;
    int exit_code = 0;

    NcValue err = shell_run_internal(command, &buf, &len, &exit_code);
    if (IS_MAP(err)) return err; /* sandbox/popen error */

    /* Try to parse as JSON for structured results */
    if (len > 0 && (buf[0] == '{' || buf[0] == '[')) {
        NcValue parsed = nc_json_parse(buf);
        if (!IS_NONE(parsed)) {
            if (IS_MAP(parsed))
                nc_map_set(AS_MAP(parsed), nc_string_from_cstr("_exit_code"), NC_INT(exit_code));
            free(buf);
            return parsed;
        }
    }

    if (exit_code != 0) {
        NcMap *result = nc_map_new();
        nc_map_set(result, nc_string_from_cstr("output"),
            NC_STRING(nc_string_from_cstr(buf)));
        nc_map_set(result, nc_string_from_cstr("exit_code"),
            NC_INT(exit_code));
        free(buf);
        return NC_MAP(result);
    }

    /* Always return a string, even if empty — never return a number.
     * This ensures split(shell("ls"), "\n") always gets a string input. */
    NcValue result = NC_STRING(nc_string_from_cstr(buf));
    free(buf);
    return result;
}

/*
 * shell_exec() — always returns {"output": "...", "exit_code": N, "ok": bool}
 * Unlike shell(), this never returns a bare string. Callers can always check
 * result.exit_code and result.ok to determine success/failure.
 */
NcValue nc_stdlib_shell_exec(const char *command) {
    char *buf = NULL;
    size_t len = 0;
    int exit_code = 0;

    NcValue err = shell_run_internal(command, &buf, &len, &exit_code);
    if (IS_MAP(err)) return err; /* sandbox/popen error */

    NcMap *result = nc_map_new();
    nc_map_set(result, nc_string_from_cstr("output"),
        NC_STRING(nc_string_from_cstr(buf)));
    nc_map_set(result, nc_string_from_cstr("exit_code"),
        NC_INT(exit_code));
    nc_map_set(result, nc_string_from_cstr("ok"),
        NC_BOOL(exit_code == 0));
    free(buf);
    return NC_MAP(result);
}

/*
 * write_file_atomic() — write to a temp file, then rename into place.
 * Prevents partial/corrupt files from concurrent read-modify-write cycles.
 * Works cross-platform: POSIX rename() is atomic on the same filesystem;
 * Windows MoveFileEx with MOVEFILE_REPLACE_EXISTING achieves the same.
 */
NcValue nc_stdlib_write_file_atomic(const char *path, const char *content) {
    if (!nc_sandbox_check("file_write")) return NC_BOOL(false);
    if (!path || !path[0]) return NC_BOOL(false);

    /* Build temp path: original path + ".tmp.<pid>.<ms>" */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%d.%lld",
             path, nc_getpid(), (long long)(nc_realtime_ms()));

    /* Write content to temp file */
    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        /* Try creating parent directories first */
        char dir[1024];
        strncpy(dir, path, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *last_sep = strrchr(dir, '/');
#ifdef NC_WINDOWS
        char *last_bsep = strrchr(dir, '\\');
        if (last_bsep && (!last_sep || last_bsep > last_sep))
            last_sep = last_bsep;
#endif
        if (last_sep) {
            *last_sep = '\0';
            for (int i = 1; dir[i]; i++) {
                if (dir[i] == '/' || dir[i] == '\\') {
                    char saved = dir[i];
                    dir[i] = '\0';
                    nc_mkdir(dir);
                    dir[i] = saved;
                }
            }
            nc_mkdir(dir);
        }
        f = fopen(tmp_path, "w");
        if (!f) return NC_BOOL(false);
    }
    fputs(content, f);
    fflush(f);
    fclose(f);

    /* Atomic rename: temp → target */
#ifdef NC_WINDOWS
    /* MoveFileExA with MOVEFILE_REPLACE_EXISTING is atomic on NTFS */
    BOOL ok = MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING);
    if (!ok) {
        /* Fallback: delete target then rename (not atomic but better than nothing) */
        DeleteFileA(path);
        ok = MoveFileA(tmp_path, path);
    }
    return NC_BOOL(ok != 0);
#else
    int rc = rename(tmp_path, path);
    if (rc != 0) {
        remove(tmp_path);
        return NC_BOOL(false);
    }
    return NC_BOOL(true);
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Cryptographic Hashing — pure C, no external dependencies
 *
 *  NC code:
 *    set digest to hash_sha256("hello world")
 *    set stored to hash_password("my_secret")
 *    if verify_password("my_secret", stored):
 *        log "password valid"
 *    set mac to hash_hmac("data", "secret_key")
 * ═══════════════════════════════════════════════════════════ */

static void nc_sha256_raw(const uint8_t *data, size_t len, uint8_t out[32]) {
    /* SHA-256 — minimal implementation (same algorithm as nc_middleware.c) */
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    #define SHA_RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
    #define SHA_CH(x,y,z) (((x)&(y))^((~(x))&(z)))
    #define SHA_MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
    #define SHA_EP0(x) (SHA_RR(x,2)^SHA_RR(x,13)^SHA_RR(x,22))
    #define SHA_EP1(x) (SHA_RR(x,6)^SHA_RR(x,11)^SHA_RR(x,25))
    #define SHA_SIG0(x) (SHA_RR(x,7)^SHA_RR(x,18)^((x)>>3))
    #define SHA_SIG1(x) (SHA_RR(x,17)^SHA_RR(x,19)^((x)>>10))

    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    /* Pad message */
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = calloc(padded_len, 1);
    if (!msg) { memset(out, 0, 32); return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[padded_len - 1 - i] = (uint8_t)(bit_len >> (i * 8));

    for (size_t offset = 0; offset < padded_len; offset += 64) {
        uint32_t W[64];
        for (int i = 0; i < 16; i++)
            W[i] = ((uint32_t)msg[offset+i*4]<<24)|((uint32_t)msg[offset+i*4+1]<<16)|
                    ((uint32_t)msg[offset+i*4+2]<<8)|msg[offset+i*4+3];
        for (int i = 16; i < 64; i++)
            W[i] = SHA_SIG1(W[i-2]) + W[i-7] + SHA_SIG0(W[i-15]) + W[i-16];

        uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
        uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
        for (int i = 0; i < 64; i++) {
            uint32_t t1 = h + SHA_EP1(e) + SHA_CH(e,f,g) + K[i] + W[i];
            uint32_t t2 = SHA_EP0(a) + SHA_MAJ(a,b,c);
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    free(msg);

    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(state[i]>>24);
        out[i*4+1] = (uint8_t)(state[i]>>16);
        out[i*4+2] = (uint8_t)(state[i]>>8);
        out[i*4+3] = (uint8_t)(state[i]);
    }
    #undef SHA_RR
    #undef SHA_CH
    #undef SHA_MAJ
    #undef SHA_EP0
    #undef SHA_EP1
    #undef SHA_SIG0
    #undef SHA_SIG1
}

NcValue nc_stdlib_hash_sha256(const char *data) {
    if (!data) return NC_NONE();
    uint8_t hash[32];
    nc_sha256_raw((const uint8_t *)data, strlen(data), hash);
    char hex[65];
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return NC_STRING(nc_string_from_cstr(hex));
}

NcValue nc_stdlib_hash_hmac(const char *data, const char *key) {
    if (!data || !key) return NC_NONE();

    uint8_t k_ipad[64], k_opad[64];
    uint8_t key_hash[32];
    const uint8_t *k = (const uint8_t *)key;
    int klen = (int)strlen(key);

    if (klen > 64) {
        nc_sha256_raw(k, klen, key_hash);
        k = key_hash;
        klen = 32;
    }
    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5c, 64);
    for (int i = 0; i < klen; i++) {
        k_ipad[i] ^= k[i];
        k_opad[i] ^= k[i];
    }

    /* inner = SHA256(k_ipad || data) */
    size_t dlen = strlen(data);
    uint8_t *inner_msg = malloc(64 + dlen);
    if (!inner_msg) return NC_NONE();
    memcpy(inner_msg, k_ipad, 64);
    memcpy(inner_msg + 64, data, dlen);
    uint8_t inner[32];
    nc_sha256_raw(inner_msg, 64 + dlen, inner);
    free(inner_msg);

    /* outer = SHA256(k_opad || inner) */
    uint8_t outer_msg[96];
    memcpy(outer_msg, k_opad, 64);
    memcpy(outer_msg + 64, inner, 32);
    uint8_t result[32];
    nc_sha256_raw(outer_msg, 96, result);

    char hex[65];
    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", result[i]);
    return NC_STRING(nc_string_from_cstr(hex));
}

/*
 * Password hashing — uses SHA-256 with a random salt.
 * Format: $nc$<salt_hex>$<hash_hex>
 * Salt is 16 bytes of randomness. Hash is SHA-256(salt + password).
 * Constant-time comparison to prevent timing attacks.
 */
NcValue nc_stdlib_hash_password(const char *password) {
    if (!password) return NC_NONE();

    /* Generate 16-byte random salt */
    uint8_t salt[16];
    srand((unsigned)time(NULL) ^ (unsigned)clock());
    for (int i = 0; i < 16; i++)
        salt[i] = (uint8_t)(rand() & 0xFF);

    /* Hash = SHA-256(salt || password) iterated 10000 times */
    size_t plen = strlen(password);
    uint8_t *msg = malloc(16 + plen);
    if (!msg) return NC_NONE();
    memcpy(msg, salt, 16);
    memcpy(msg + 16, password, plen);
    uint8_t hash[32];
    nc_sha256_raw(msg, 16 + plen, hash);
    for (int round = 1; round < 10000; round++) {
        uint8_t tmp[64];
        memcpy(tmp, hash, 32);
        memcpy(tmp + 32, salt, 16);
        nc_sha256_raw(tmp, 48, hash);
    }
    free(msg);

    /* Format: $nc$<salt_hex>$<hash_hex> */
    char result[128];
    int pos = snprintf(result, sizeof(result), "$nc$");
    for (int i = 0; i < 16; i++)
        pos += snprintf(result + pos, sizeof(result) - pos, "%02x", salt[i]);
    result[pos++] = '$';
    for (int i = 0; i < 32; i++)
        pos += snprintf(result + pos, sizeof(result) - pos, "%02x", hash[i]);

    return NC_STRING(nc_string_from_cstr(result));
}

NcValue nc_stdlib_verify_password(const char *password, const char *stored_hash) {
    if (!password || !stored_hash) return NC_BOOL(false);
    if (strncmp(stored_hash, "$nc$", 4) != 0) return NC_BOOL(false);

    /* Parse salt from stored hash */
    const char *salt_hex = stored_hash + 4;
    const char *dollar = strchr(salt_hex, '$');
    if (!dollar || dollar - salt_hex != 32) return NC_BOOL(false);
    const char *hash_hex = dollar + 1;
    if (strlen(hash_hex) != 64) return NC_BOOL(false);

    uint8_t salt[16];
    for (int i = 0; i < 16; i++) {
        unsigned int b;
        char byte_str[3] = { salt_hex[i*2], salt_hex[i*2+1], '\0' };
        sscanf(byte_str, "%02x", &b);
        salt[i] = (uint8_t)b;
    }

    /* Recompute hash with same salt and iterations */
    size_t plen = strlen(password);
    uint8_t *msg = malloc(16 + plen);
    if (!msg) return NC_BOOL(false);
    memcpy(msg, salt, 16);
    memcpy(msg + 16, password, plen);
    uint8_t hash[32];
    nc_sha256_raw(msg, 16 + plen, hash);
    for (int round = 1; round < 10000; round++) {
        uint8_t tmp[64];
        memcpy(tmp, hash, 32);
        memcpy(tmp + 32, salt, 16);
        nc_sha256_raw(tmp, 48, hash);
    }
    free(msg);

    /* Constant-time comparison */
    volatile uint8_t diff = 0;
    for (int i = 0; i < 32; i++) {
        unsigned int expected;
        char byte_str[3] = { hash_hex[i*2], hash_hex[i*2+1], '\0' };
        sscanf(byte_str, "%02x", &expected);
        diff |= hash[i] ^ (uint8_t)expected;
    }
    return NC_BOOL(diff == 0);
}

/* ═══════════════════════════════════════════════════════════
 *  Request Header Access — read headers from current request
 *
 *  NC code:
 *    set token to request_header("Authorization")
 *    set agent to request_header("User-Agent")
 *    set ip to request_ip()
 * ═══════════════════════════════════════════════════════════ */

#ifndef NC_WINDOWS
#include <pthread.h>
static pthread_key_t nc_req_ctx_key;
static int nc_req_ctx_key_initialized = 0;
#else
static nc_thread_local void *nc_req_ctx_tls = NULL;
#endif

void nc_request_ctx_set(NcRequestContext *ctx) {
#ifndef NC_WINDOWS
    if (!nc_req_ctx_key_initialized) {
        pthread_key_create(&nc_req_ctx_key, NULL);
        nc_req_ctx_key_initialized = 1;
    }
    pthread_setspecific(nc_req_ctx_key, ctx);
#else
    nc_req_ctx_tls = ctx;
#endif
}

NcRequestContext *nc_request_ctx_get(void) {
#ifndef NC_WINDOWS
    if (!nc_req_ctx_key_initialized) return NULL;
    return (NcRequestContext *)pthread_getspecific(nc_req_ctx_key);
#else
    return (NcRequestContext *)nc_req_ctx_tls;
#endif
}

NcValue nc_stdlib_request_header(const char *header_name) {
    if (!header_name) return NC_NONE();
    NcRequestContext *ctx = nc_request_ctx_get();
    if (!ctx) return NC_NONE();

    for (int i = 0; i < ctx->header_count; i++) {
        if (strcasecmp(ctx->headers[i][0], header_name) == 0)
            return NC_STRING(nc_string_from_cstr(ctx->headers[i][1]));
    }
    return NC_NONE();
}

/* ═══════════════════════════════════════════════════════════
 *  Session Management — in-memory server-side sessions
 *
 *  NC code:
 *    set sid to session_create()
 *    session_set(sid, "username", "alice")
 *    set user to session_get(sid, "username")
 *    session_destroy(sid)
 *
 *  Sessions auto-expire after NC_SESSION_TTL seconds (default 3600).
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char    id[64];
    NcMap  *data;
    time_t  created_at;
    time_t  last_accessed;
    bool    active;
} NcSession;

#define NC_MAX_SESSIONS 4096
static NcSession sessions[NC_MAX_SESSIONS];
static int session_count = 0;
static nc_mutex_t session_mutex = NC_MUTEX_INITIALIZER;

static int nc_session_ttl(void) {
    const char *v = getenv("NC_SESSION_TTL");
    return (v && atoi(v) > 0) ? atoi(v) : 3600;
}

static void nc_session_gc(void) {
    time_t now = time(NULL);
    int ttl = nc_session_ttl();
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].active && now - sessions[i].last_accessed > ttl) {
            if (sessions[i].data) nc_map_free(sessions[i].data);
            sessions[i].active = false;
        }
    }
}

NcValue nc_stdlib_session_create(void) {
    nc_mutex_lock(&session_mutex);
    nc_session_gc();

    /* Find a free slot */
    int idx = -1;
    for (int i = 0; i < session_count; i++) {
        if (!sessions[i].active) { idx = i; break; }
    }
    if (idx < 0) {
        if (session_count >= NC_MAX_SESSIONS) {
            nc_mutex_unlock(&session_mutex);
            return NC_NONE();
        }
        idx = session_count++;
    }

    NcSession *s = &sessions[idx];
    s->active = true;
    s->data = nc_map_new();
    s->created_at = time(NULL);
    s->last_accessed = s->created_at;

    /* Generate random session ID */
    unsigned long r1 = (unsigned long)time(NULL) ^ (unsigned long)clock() ^ (unsigned long)rand();
    unsigned long r2 = r1 * 6364136223846793005ULL + 1442695040888963407ULL;
    snprintf(s->id, sizeof(s->id), "nc_%016lx%016lx", r1, r2);

    nc_mutex_unlock(&session_mutex);
    return NC_STRING(nc_string_from_cstr(s->id));
}

static NcSession *find_session(const char *session_id) {
    if (!session_id) return NULL;
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].active && strcmp(sessions[i].id, session_id) == 0) {
            sessions[i].last_accessed = time(NULL);
            return &sessions[i];
        }
    }
    return NULL;
}

NcValue nc_stdlib_session_set(const char *session_id, const char *key, NcValue value) {
    nc_mutex_lock(&session_mutex);
    NcSession *s = find_session(session_id);
    if (!s) { nc_mutex_unlock(&session_mutex); return NC_BOOL(false); }
    nc_map_set(s->data, nc_string_from_cstr(key), value);
    nc_mutex_unlock(&session_mutex);
    return NC_BOOL(true);
}

NcValue nc_stdlib_session_get(const char *session_id, const char *key) {
    nc_mutex_lock(&session_mutex);
    NcSession *s = find_session(session_id);
    if (!s) { nc_mutex_unlock(&session_mutex); return NC_NONE(); }
    NcString *k = nc_string_from_cstr(key);
    NcValue result = nc_map_get(s->data, k);
    nc_string_free(k);
    nc_mutex_unlock(&session_mutex);
    return result;
}

NcValue nc_stdlib_session_destroy(const char *session_id) {
    nc_mutex_lock(&session_mutex);
    NcSession *s = find_session(session_id);
    if (!s) { nc_mutex_unlock(&session_mutex); return NC_BOOL(false); }
    if (s->data) nc_map_free(s->data);
    s->data = NULL;
    s->active = false;
    nc_mutex_unlock(&session_mutex);
    return NC_BOOL(true);
}

NcValue nc_stdlib_session_exists(const char *session_id) {
    nc_mutex_lock(&session_mutex);
    NcSession *s = find_session(session_id);
    nc_mutex_unlock(&session_mutex);
    return NC_BOOL(s != NULL);
}

/* ═══════════════════════════════════════════════════════════
 *  Connection Pool — reusable HTTP connections
 *
 *  Transparent to NC code — automatically reuses connections
 *  when the same host is contacted multiple times.
 *  Configurable via:
 *    NC_CONNPOOL_SIZE=32   (max pooled connections)
 *    NC_CONNPOOL_TTL=60    (max idle seconds)
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char    host[256];
    int     port;
    void   *handle;
    time_t  last_used;
    bool    in_use;
} NcPooledConn;

#define NC_CONNPOOL_MAX 64
static NcPooledConn conn_pool[NC_CONNPOOL_MAX];
static int connpool_count = 0;
static nc_mutex_t connpool_mutex = NC_MUTEX_INITIALIZER;
static bool connpool_initialized = false;

void nc_connpool_init(void) {
    nc_mutex_lock(&connpool_mutex);
    if (!connpool_initialized) {
        memset(conn_pool, 0, sizeof(conn_pool));
        connpool_count = 0;
        connpool_initialized = true;
    }
    nc_mutex_unlock(&connpool_mutex);
}

void nc_connpool_cleanup(void) {
    nc_mutex_lock(&connpool_mutex);
    for (int i = 0; i < connpool_count; i++) {
        conn_pool[i].in_use = false;
        conn_pool[i].handle = NULL;
    }
    connpool_count = 0;
    connpool_initialized = false;
    nc_mutex_unlock(&connpool_mutex);
}

/* ═══════════════════════════════════════════════════════════
 *  Module: Regex (re) — custom engine, no PCRE dependency
 * ═══════════════════════════════════════════════════════════ */

/* ── Simple regex engine ───────────────────────────────────
 * Supports: . * + ? ^ $ \d \w \s [...] | () groups
 * Returns match length or -1 for no match.
 */

typedef struct {
    const char *start;
    int length;
} ReGroup;

#define RE_MAX_GROUPS 16

typedef struct {
    ReGroup groups[RE_MAX_GROUPS];
    int group_count;
} ReMatchResult;

/* Forward declarations */
static int re_match_here(const char *pattern, const char *text, ReMatchResult *result, int depth);
static int re_match_star(char c, int is_class, const char *cclass_end, const char *pattern, const char *text, ReMatchResult *result, int depth, int greedy);

/* Check if character matches a character class [...] */
static int re_in_class(const char *classstart, const char *classend, char c) {
    int negate = 0;
    const char *p = classstart;
    if (*p == '^') { negate = 1; p++; }
    while (p < classend) {
        if (p + 2 < classend && *(p+1) == '-') {
            if (c >= *p && c <= *(p+2)) return !negate;
            p += 3;
        } else {
            if (c == *p) return !negate;
            p++;
        }
    }
    return negate;
}

/* Match a single atom: literal, '.', '\d', '\w', '\s', or '[...]' */
static int re_match_one(const char *pattern, char c, const char **pat_end) {
    if (*pattern == '.') {
        *pat_end = pattern + 1;
        return (c != '\0');
    }
    if (*pattern == '\\' && pattern[1]) {
        *pat_end = pattern + 2;
        switch (pattern[1]) {
            case 'd': return isdigit((unsigned char)c);
            case 'w': return isalnum((unsigned char)c) || c == '_';
            case 's': return isspace((unsigned char)c);
            case 'D': return !isdigit((unsigned char)c);
            case 'W': return !(isalnum((unsigned char)c) || c == '_');
            case 'S': return !isspace((unsigned char)c);
            default:  return c == pattern[1];
        }
    }
    if (*pattern == '[') {
        const char *end = pattern + 1;
        while (*end && *end != ']') end++;
        if (*end == ']') {
            *pat_end = end + 1;
            return re_in_class(pattern + 1, end, c);
        }
    }
    *pat_end = pattern + 1;
    return c == *pattern;
}

/* Get the end of an atom (for quantifier lookahead) */
static const char *re_atom_end(const char *pattern) {
    if (*pattern == '\\' && pattern[1]) return pattern + 2;
    if (*pattern == '[') {
        const char *p = pattern + 1;
        while (*p && *p != ']') p++;
        if (*p == ']') return p + 1;
    }
    if (*pattern == '(') {
        int depth = 1;
        const char *p = pattern + 1;
        while (*p && depth > 0) {
            if (*p == '(') depth++;
            else if (*p == ')') depth--;
            p++;
        }
        return p;
    }
    return pattern + 1;
}

/* Find the matching ')' for a '(' */
static const char *re_find_close_paren(const char *p) {
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        if (depth > 0) p++;
    }
    return p;
}

/* Match alternation: tries left | right */
static int re_match_alt(const char *pattern, const char *altend, const char *text, ReMatchResult *result, int depth) {
    /* Find the | at current nesting level */
    const char *p = pattern;
    int nest = 0;
    while (p < altend) {
        if (*p == '\\' && p[1]) { p += 2; continue; }
        if (*p == '(') nest++;
        else if (*p == ')') nest--;
        else if (*p == '|' && nest == 0) {
            /* Try left branch: pattern..p */
            int save_gc = result->group_count;
            /* Temporarily null-terminate by using length */
            char leftbuf[2048];
            int llen = (int)(p - pattern);
            if (llen >= (int)sizeof(leftbuf)) llen = (int)sizeof(leftbuf) - 1;
            memcpy(leftbuf, pattern, llen);
            /* append rest after altend */
            int rlen = (int)strlen(altend);
            if (llen + rlen >= (int)sizeof(leftbuf)) rlen = (int)sizeof(leftbuf) - 1 - llen;
            memcpy(leftbuf + llen, altend, rlen);
            leftbuf[llen + rlen] = '\0';
            int m = re_match_here(leftbuf, text, result, depth + 1);
            if (m >= 0) return m;
            result->group_count = save_gc;

            /* Try right branch: (p+1)..altend then rest */
            char rightbuf[2048];
            int r2len = (int)(altend - (p + 1));
            if (r2len >= (int)sizeof(rightbuf)) r2len = (int)sizeof(rightbuf) - 1;
            memcpy(rightbuf, p + 1, r2len);
            int r3len = (int)strlen(altend);
            if (r2len + r3len >= (int)sizeof(rightbuf)) r3len = (int)sizeof(rightbuf) - 1 - r2len;
            memcpy(rightbuf + r2len, altend, r3len);
            rightbuf[r2len + r3len] = '\0';
            return re_match_here(rightbuf, text, result, depth + 1);
        }
        p++;
    }
    return -1; /* no alternation found */
}

/* Core recursive matching */
static int re_match_here(const char *pattern, const char *text, ReMatchResult *result, int depth) {
    if (depth > 200) return -1; /* recursion limit */

    if (pattern[0] == '\0') return 0;
    if (pattern[0] == '$' && pattern[1] == '\0') return (*text == '\0') ? 0 : -1;

    /* Handle groups: (...) */
    if (pattern[0] == '(') {
        const char *close = re_find_close_paren(pattern + 1);
        if (*close != ')') return -1;
        const char *after = close + 1;
        int gidx = result->group_count;
        if (gidx < RE_MAX_GROUPS) result->group_count++;

        /* Check for alternation inside group */
        int has_alt = 0;
        {
            int nest = 0;
            for (const char *q = pattern + 1; q < close; q++) {
                if (*q == '\\' && q[1]) { q++; continue; }
                if (*q == '(') nest++;
                else if (*q == ')') nest--;
                else if (*q == '|' && nest == 0) { has_alt = 1; break; }
            }
        }

        /* Quantifier on group? */
        if (*after == '*' || *after == '+' || *after == '?') {
            char quant = *after;
            const char *rest = after + 1;
            int min_rep = (quant == '+') ? 1 : 0;
            int max_rep = (quant == '?') ? 1 : 10000;

            /* Try greedy: match as many reps as possible */
            int reps = 0;
            int lengths[10001];
            lengths[0] = 0;
            const char *t = text;
            while (reps < max_rep) {
                int ginner_len = -1;
                if (has_alt) {
                    ginner_len = re_match_alt(pattern + 1, close, t, result, depth + 1);
                } else {
                    char inner[2048];
                    int ilen = (int)(close - (pattern + 1));
                    if (ilen >= (int)sizeof(inner)) ilen = (int)sizeof(inner) - 1;
                    memcpy(inner, pattern + 1, ilen);
                    inner[ilen] = '\0';
                    ginner_len = re_match_here(inner, t, result, depth + 1);
                }
                if (ginner_len <= 0) break;
                t += ginner_len;
                reps++;
                lengths[reps] = (int)(t - text);
            }
            /* Try from max reps down */
            for (int r = reps; r >= min_rep; r--) {
                if (gidx < RE_MAX_GROUPS) {
                    result->groups[gidx].start = text;
                    result->groups[gidx].length = lengths[r];
                }
                int rest_m = re_match_here(rest, text + lengths[r], result, depth + 1);
                if (rest_m >= 0) return lengths[r] + rest_m;
            }
            return -1;
        }

        /* No quantifier — match group once */
        if (has_alt) {
            int m = re_match_alt(pattern + 1, close, text, result, depth + 1);
            if (m < 0) return -1;
            if (gidx < RE_MAX_GROUPS) {
                result->groups[gidx].start = text;
                result->groups[gidx].length = m;
            }
            /* We need to also consume 'after' pattern portion */
            /* The alt handler already matched rest, so just return m for group */
            /* Actually the alt handler built full pattern with after, so m includes rest */
            return m;
        } else {
            /* Build inner pattern string */
            char inner[2048];
            int ilen = (int)(close - (pattern + 1));
            if (ilen >= (int)sizeof(inner)) ilen = (int)sizeof(inner) - 1;
            memcpy(inner, pattern + 1, ilen);
            inner[ilen] = '\0';
            int m = re_match_here(inner, text, result, depth + 1);
            if (m < 0) return -1;
            if (gidx < RE_MAX_GROUPS) {
                result->groups[gidx].start = text;
                result->groups[gidx].length = m;
            }
            int rest_m = re_match_here(after, text + m, result, depth + 1);
            if (rest_m < 0) return -1;
            return m + rest_m;
        }
    }

    /* Handle alternation at top level */
    {
        int nest = 0;
        for (const char *q = pattern; *q; q++) {
            if (*q == '\\' && q[1]) { q++; continue; }
            if (*q == '(') nest++;
            else if (*q == ')') nest--;
            else if (*q == '|' && nest == 0) {
                /* Try left branch */
                char left[2048];
                int llen = (int)(q - pattern);
                if (llen >= (int)sizeof(left)) llen = (int)sizeof(left) - 1;
                memcpy(left, pattern, llen);
                left[llen] = '\0';
                int m = re_match_here(left, text, result, depth + 1);
                if (m >= 0) return m;
                /* Try right branch */
                return re_match_here(q + 1, text, result, depth + 1);
            }
        }
    }

    /* Quantifiers: *, +, ? */
    const char *atom_end = re_atom_end(pattern);
    if (*atom_end == '*') {
        return re_match_star(pattern[0], (*pattern == '[' || *pattern == '\\' || *pattern == '.'), atom_end, atom_end + 1, text, result, depth, 1);
    }
    if (*atom_end == '+') {
        /* + = one then star */
        const char *pat_next;
        if (!re_match_one(pattern, *text, &pat_next) || *text == '\0') return -1;
        int m = re_match_star(pattern[0], (*pattern == '[' || *pattern == '\\' || *pattern == '.'), atom_end, atom_end + 1, text + 1, result, depth, 1);
        if (m >= 0) return 1 + m;
        return -1;
    }
    if (*atom_end == '?') {
        /* Try with and without */
        const char *pat_next;
        if (re_match_one(pattern, *text, &pat_next) && *text != '\0') {
            int m = re_match_here(atom_end + 1, text + 1, result, depth + 1);
            if (m >= 0) return 1 + m;
        }
        return re_match_here(atom_end + 1, text, result, depth + 1);
    }

    /* Match single character */
    const char *pat_next;
    if (re_match_one(pattern, *text, &pat_next) && *text != '\0') {
        int m = re_match_here(pat_next, text + 1, result, depth + 1);
        if (m >= 0) return 1 + m;
    }
    return -1;
}

static int re_match_star(char c, int is_class, const char *cclass_end,
                         const char *pattern, const char *text,
                         ReMatchResult *result, int depth, int greedy) {
    (void)c; (void)greedy;
    /* Greedy: consume as many as possible, then try rest */
    const char *t = text;
    int count = 0;
    const char *pat_base = cclass_end - (int)(cclass_end - (is_class ? cclass_end : cclass_end));
    /* Recalculate: the original atom starts at (cclass_end - atom_length) */
    /* Actually we need the original pattern start for re_match_one */
    /* cclass_end points to the end of the atom (before * or +) */
    /* We need the start of the atom. Let's compute it from the structure. */
    /* For simplicity, cclass_end IS atom_end. The atom starts before it. */
    /* We passed the original pattern[0] as c and is_class as a flag. */
    /* We need the full atom pattern for re_match_one. Let's use a different approach. */

    /* The caller should pass the atom pattern start. For now, reconstruct. */
    /* Actually, let's just find how many chars match the atom from text. */
    /* The atom pattern string is from: pattern - (atom_end - pattern_start) to atom_end */
    /* This is getting complicated. Let's do a simpler approach: */

    /* We know the atom pattern ends at cclass_end. The atom could be:
       - single char: cclass_end - 1
       - \x escape: cclass_end - 2
       - [...]: search back for [
       - .: cclass_end - 1
    */
    const char *atom_start;
    if (*(cclass_end - 1) == ']') {
        atom_start = cclass_end - 1;
        while (atom_start > (cclass_end - 200) && *atom_start != '[') atom_start--;
    } else if (cclass_end >= (const char*)2 && *(cclass_end - 2) == '\\') {
        atom_start = cclass_end - 2;
    } else {
        atom_start = cclass_end - 1;
    }

    /* Count max matching chars */
    while (*t) {
        const char *dummy;
        if (!re_match_one(atom_start, *t, &dummy)) break;
        t++;
        count++;
        if (count > 100000) break;
    }

    /* Try from max down to 0 (greedy) */
    for (int i = count; i >= 0; i--) {
        int m = re_match_here(pattern, text + i, result, depth + 1);
        if (m >= 0) return i + m;
    }
    return -1;

    (void)pat_base;
}

/* Try to match pattern against text starting from each position.
 * Returns the starting position and fills result. */
static int re_search(const char *pattern, const char *text, int *match_start, ReMatchResult *result) {
    result->group_count = 0;
    /* Handle ^ anchor */
    if (pattern[0] == '^') {
        int m = re_match_here(pattern + 1, text, result, 0);
        if (m >= 0) { *match_start = 0; return m; }
        return -1;
    }
    /* Try each position */
    for (int i = 0; text[i] != '\0'; i++) {
        result->group_count = 0;
        int m = re_match_here(pattern, text + i, result, 0);
        if (m >= 0) { *match_start = i; return m; }
    }
    /* Try empty match at end */
    result->group_count = 0;
    int m = re_match_here(pattern, text + strlen(text), result, 0);
    if (m >= 0) { *match_start = (int)strlen(text); return m; }
    return -1;
}

NcValue nc_stdlib_re_match(const char *pattern, const char *string) {
    if (!pattern || !string) return NC_NONE();
    ReMatchResult result;
    int start;
    int len = re_search(pattern, string, &start, &result);
    if (len < 0) return NC_NONE();

    NcMap *m = nc_map_new();
    nc_map_set(m, nc_string_from_cstr("matched"), NC_BOOL(true));
    nc_map_set(m, nc_string_from_cstr("start"), NC_INT(start));
    nc_map_set(m, nc_string_from_cstr("end"), NC_INT(start + len));
    nc_map_set(m, nc_string_from_cstr("text"), NC_STRING(nc_string_new(string + start, len)));

    /* Groups */
    NcList *groups = nc_list_new();
    for (int i = 0; i < result.group_count; i++) {
        nc_list_push(groups, NC_STRING(nc_string_new(result.groups[i].start, result.groups[i].length)));
    }
    nc_map_set(m, nc_string_from_cstr("groups"), NC_LIST(groups));
    return NC_MAP(m);
}

NcValue nc_stdlib_re_search(const char *pattern, const char *string) {
    if (!pattern || !string) return NC_NONE();
    ReMatchResult result;
    int start;
    int len = re_search(pattern, string, &start, &result);
    if (len < 0) return NC_NONE();
    return NC_STRING(nc_string_new(string + start, len));
}

NcValue nc_stdlib_re_findall(const char *pattern, const char *string) {
    if (!pattern || !string) return NC_LIST(nc_list_new());
    NcList *matches = nc_list_new();
    const char *p = string;
    while (*p) {
        ReMatchResult result;
        int start;
        int len = re_search(pattern, p, &start, &result);
        if (len < 0) break;
        nc_list_push(matches, NC_STRING(nc_string_new(p + start, len)));
        p += start + (len > 0 ? len : 1);
        if (start + len == 0 && *p) p++; /* avoid infinite loop on zero-length match */
    }
    return NC_LIST(matches);
}

NcValue nc_stdlib_re_replace(const char *pattern, const char *replacement, const char *string) {
    if (!pattern || !replacement || !string) return NC_STRING(nc_string_from_cstr(string ? string : ""));
    size_t cap = strlen(string) * 2 + 64;
    char *out = malloc(cap);
    int oi = 0;
    const char *p = string;
    int rlen = (int)strlen(replacement);

    while (*p) {
        ReMatchResult result;
        int start;
        int len = re_search(pattern, p, &start, &result);
        if (len < 0) break;
        /* Copy text before match */
        while (oi + start + rlen + 1 >= (int)cap) { cap *= 2; out = realloc(out, cap); }
        memcpy(out + oi, p, start); oi += start;
        memcpy(out + oi, replacement, rlen); oi += rlen;
        p += start + len;
        if (len == 0) {
            if (*p) { out[oi++] = *p++; } else break;
        }
    }
    /* Copy remainder */
    int rem = (int)strlen(p);
    while (oi + rem + 1 >= (int)cap) { cap *= 2; out = realloc(out, cap); }
    memcpy(out + oi, p, rem); oi += rem;
    out[oi] = '\0';
    NcValue r = NC_STRING(nc_string_from_cstr(out));
    free(out);
    return r;
}

NcValue nc_stdlib_re_split(const char *pattern, const char *string) {
    if (!pattern || !string) return NC_LIST(nc_list_new());
    NcList *parts = nc_list_new();
    const char *p = string;

    while (*p) {
        ReMatchResult result;
        int start;
        int len = re_search(pattern, p, &start, &result);
        if (len < 0) break;
        nc_list_push(parts, NC_STRING(nc_string_new(p, start)));
        p += start + len;
        if (len == 0) {
            if (*p) {
                char c[2] = { *p, '\0' };
                nc_list_push(parts, NC_STRING(nc_string_from_cstr(c)));
                p++;
            } else break;
        }
    }
    /* Remaining text */
    nc_list_push(parts, NC_STRING(nc_string_from_cstr(p)));
    return NC_LIST(parts);
}

/* ═══════════════════════════════════════════════════════════
 *  Module: CSV — enhanced with quoted fields
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_csv_parse_delim(const char *text, char delimiter) {
    if (!text) return NC_LIST(nc_list_new());
    NcList *rows = nc_list_new();
    const char *p = text;

    while (*p) {
        NcList *row = nc_list_new();
        while (1) {
            char buf[8192];
            int bi = 0;
            if (*p == '"') {
                /* Quoted field */
                p++; /* skip opening quote */
                while (*p) {
                    if (*p == '"' && *(p+1) == '"') {
                        if (bi < (int)sizeof(buf) - 1) buf[bi++] = '"';
                        p += 2;
                    } else if (*p == '"') {
                        p++; /* skip closing quote */
                        break;
                    } else {
                        if (bi < (int)sizeof(buf) - 1) buf[bi++] = *p;
                        p++;
                    }
                }
            } else {
                /* Unquoted field */
                while (*p && *p != delimiter && *p != '\n' && *p != '\r') {
                    if (bi < (int)sizeof(buf) - 1) buf[bi++] = *p;
                    p++;
                }
                /* Trim trailing spaces */
                while (bi > 0 && buf[bi-1] == ' ') bi--;
            }
            buf[bi] = '\0';
            nc_list_push(row, NC_STRING(nc_string_from_cstr(buf)));

            if (*p == delimiter) { p++; continue; }
            break;
        }
        if (row->count > 0) nc_list_push(rows, NC_LIST(row));
        /* Skip line ending */
        while (*p == '\r' || *p == '\n') p++;
    }
    return NC_LIST(rows);
}

NcValue nc_stdlib_csv_stringify(NcValue data, char delimiter) {
    if (!IS_LIST(data)) return NC_STRING(nc_string_from_cstr(""));
    NcList *rows = AS_LIST(data);
    size_t cap = 4096;
    char *out = malloc(cap);
    int oi = 0;

    for (int r = 0; r < rows->count; r++) {
        if (!IS_LIST(rows->items[r])) continue;
        NcList *row = AS_LIST(rows->items[r]);
        for (int c = 0; c < row->count; c++) {
            NcString *s = nc_value_to_string(row->items[c]);
            int need_quote = 0;
            for (int i = 0; i < s->length; i++) {
                if (s->chars[i] == delimiter || s->chars[i] == '"' ||
                    s->chars[i] == '\n' || s->chars[i] == '\r') {
                    need_quote = 1; break;
                }
            }
            /* Ensure capacity */
            while (oi + s->length * 2 + 10 >= (int)cap) { cap *= 2; out = realloc(out, cap); }
            if (need_quote) {
                out[oi++] = '"';
                for (int i = 0; i < s->length; i++) {
                    if (s->chars[i] == '"') { out[oi++] = '"'; out[oi++] = '"'; }
                    else out[oi++] = s->chars[i];
                }
                out[oi++] = '"';
            } else {
                memcpy(out + oi, s->chars, s->length);
                oi += s->length;
            }
            nc_string_free(s);
            if (c < row->count - 1) out[oi++] = delimiter;
        }
        out[oi++] = '\n';
    }
    out[oi] = '\0';
    NcValue result = NC_STRING(nc_string_from_cstr(out));
    free(out);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Module: OS
 * ═══════════════════════════════════════════════════════════ */

#include <sys/stat.h>
#ifndef NC_WINDOWS
#include <dirent.h>
#include <fnmatch.h>
#endif

NcValue nc_stdlib_os_env(const char *name) {
    if (!name) return NC_NONE();
    const char *val = getenv(name);
    if (val) return NC_STRING(nc_string_from_cstr(val));
    return NC_NONE();
}

NcValue nc_stdlib_os_cwd(void) {
    char buf[4096];
#ifdef NC_WINDOWS
    if (_getcwd(buf, sizeof(buf))) return NC_STRING(nc_string_from_cstr(buf));
#else
    if (getcwd(buf, sizeof(buf))) return NC_STRING(nc_string_from_cstr(buf));
#endif
    return NC_STRING(nc_string_from_cstr("."));
}

NcValue nc_stdlib_os_listdir(const char *path) {
    NcList *list = nc_list_new();
    if (!path) return NC_LIST(list);
#ifdef NC_WINDOWS
    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search_path, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
                nc_list_push(list, NC_STRING(nc_string_from_cstr(fd.cFileName)));
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(path);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
                nc_list_push(list, NC_STRING(nc_string_from_cstr(ent->d_name)));
        }
        closedir(d);
    }
#endif
    return NC_LIST(list);
}

NcValue nc_stdlib_os_exists(const char *path) {
    if (!path) return NC_BOOL(false);
    struct stat st;
    return NC_BOOL(stat(path, &st) == 0);
}

NcValue nc_stdlib_os_mkdir_p(const char *path) {
    if (!path) return NC_BOOL(false);
#ifdef NC_WINDOWS
    return NC_BOOL(_mkdir(path) == 0 || errno == EEXIST);
#else
    return NC_BOOL(mkdir(path, 0755) == 0 || errno == EEXIST);
#endif
}

NcValue nc_stdlib_os_remove(const char *path) {
    if (!path) return NC_BOOL(false);
    return NC_BOOL(remove(path) == 0);
}

/* ── os_glob(dir, pattern) — match files by fnmatch pattern ── */
NcValue nc_stdlib_os_glob(const char *dir, const char *pattern) {
    NcList *list = nc_list_new();
    if (!dir || !pattern) return NC_LIST(list);
#ifdef NC_WINDOWS
    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\%s", dir, pattern);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search_path, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                char full[4096];
                snprintf(full, sizeof(full), "%s" NC_PATH_SEP_STR "%s", dir, fd.cFileName);
                nc_list_push(list, NC_STRING(nc_string_from_cstr(full)));
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            if (fnmatch(pattern, ent->d_name, 0) == 0) {
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
                nc_list_push(list, NC_STRING(nc_string_from_cstr(full)));
            }
        }
        closedir(d);
    }
#endif
    return NC_LIST(list);
}

/* ── os_walk(dir) — recursive directory walk, returns list of all file paths ── */
static void os_walk_recursive(const char *dir, NcList *result, int depth) {
    if (depth > 64) return; /* prevent infinite recursion from symlinks */
#ifdef NC_WINDOWS
    char search_path[4096];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search_path, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            char full[4096];
            snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);
            nc_list_push(result, NC_STRING(nc_string_from_cstr(full)));
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                os_walk_recursive(full, result, depth + 1);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
        nc_list_push(result, NC_STRING(nc_string_from_cstr(full)));
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            os_walk_recursive(full, result, depth + 1);
    }
    closedir(d);
#endif
}

NcValue nc_stdlib_os_walk(const char *dir) {
    NcList *list = nc_list_new();
    if (!dir) return NC_LIST(list);
    os_walk_recursive(dir, list, 0);
    return NC_LIST(list);
}

/* ── os_is_dir(path) — check if path is a directory ── */
NcValue nc_stdlib_os_is_dir(const char *path) {
    if (!path) return NC_BOOL(false);
    struct stat st;
    if (stat(path, &st) != 0) return NC_BOOL(false);
    return NC_BOOL(S_ISDIR(st.st_mode));
}

/* ── os_is_file(path) — check if path is a regular file ── */
NcValue nc_stdlib_os_is_file(const char *path) {
    if (!path) return NC_BOOL(false);
    struct stat st;
    if (stat(path, &st) != 0) return NC_BOOL(false);
    return NC_BOOL(S_ISREG(st.st_mode));
}

/* ── os_file_size(path) — get file size in bytes ── */
NcValue nc_stdlib_os_file_size(const char *path) {
    if (!path) return NC_INT(0);
    struct stat st;
    if (stat(path, &st) != 0) return NC_INT(0);
    return NC_INT((int64_t)st.st_size);
}

/* ── os_path_join(parts...) — join path components ── */
NcValue nc_stdlib_os_path_join(NcList *parts) {
    if (!parts || parts->count == 0) return NC_STRING(nc_string_from_cstr(""));
    char buf[4096] = {0};
    int pos = 0;
    for (int i = 0; i < parts->count && pos < 4000; i++) {
        if (!IS_STRING(parts->items[i])) continue;
        const char *s = AS_STRING(parts->items[i])->chars;
        if (i > 0 && pos > 0 && buf[pos-1] != '/' && buf[pos-1] != '\\')
            buf[pos++] = '/';
        int slen = (int)strlen(s);
        if (pos + slen >= 4096) break;
        memcpy(buf + pos, s, slen);
        pos += slen;
    }
    buf[pos] = '\0';
    return NC_STRING(nc_string_from_cstr(buf));
}

/* ── os_basename(path) — get filename from path ── */
NcValue nc_stdlib_os_basename(const char *path) {
    if (!path) return NC_STRING(nc_string_from_cstr(""));
    const char *last = strrchr(path, '/');
#ifdef NC_WINDOWS
    const char *last_bs = strrchr(path, '\\');
    if (!last || (last_bs && last_bs > last)) last = last_bs;
#endif
    return NC_STRING(nc_string_from_cstr(last ? last + 1 : path));
}

/* ── os_dirname(path) — get directory from path ── */
NcValue nc_stdlib_os_dirname(const char *path) {
    if (!path) return NC_STRING(nc_string_from_cstr(""));
    char buf[4096];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *last = strrchr(buf, '/');
#ifdef NC_WINDOWS
    char *last_bs = strrchr(buf, '\\');
    if (!last || (last_bs && last_bs > last)) last = last_bs;
#endif
    if (last) { *last = '\0'; return NC_STRING(nc_string_from_cstr(buf)); }
    return NC_STRING(nc_string_from_cstr("."));
}

/* ── os_setenv(name, value) — set environment variable ── */
NcValue nc_stdlib_os_setenv(const char *name, const char *value) {
    if (!name) return NC_BOOL(false);
#ifdef NC_WINDOWS
    return NC_BOOL(_putenv_s(name, value ? value : "") == 0);
#else
    if (value) return NC_BOOL(setenv(name, value, 1) == 0);
    else return NC_BOOL(unsetenv(name) == 0);
#endif
}

NcValue nc_stdlib_os_read_file(const char *path) {
    if (!path) return NC_NONE();
    if (!nc_path_is_safe(path)) return NC_NONE();
    FILE *f = fopen(path, "rb");
    if (!f) return NC_NONE();
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NC_NONE(); }
    size_t nread = fread(buf, 1, sz, f);
    buf[nread] = '\0';
    fclose(f);
    NcValue result = NC_STRING(nc_string_new(buf, (int)nread));
    free(buf);
    return result;
}

NcValue nc_stdlib_os_write_file(const char *path, const char *content) {
    if (!path || !content) return NC_BOOL(false);
    if (!nc_path_is_safe(path)) return NC_BOOL(false);
    FILE *f = fopen(path, "wb");
    if (!f) return NC_BOOL(false);
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);
    return NC_BOOL(written == len);
}

NcValue nc_stdlib_os_exec(const char *command) {
    if (!command) return NC_NONE();

    if (!nc_sandbox_check("exec")) {
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
            NC_STRING(nc_string_from_cstr("os.exec() blocked by sandbox (allow_exec is false)")));
        return NC_MAP(err);
    }

    /* Reject shell metacharacters to prevent command injection */
    for (const char *p = command; *p; p++) {
        if (*p == ';' || *p == '|' || *p == '&' || *p == '$' ||
            *p == '`' || *p == '\\' || *p == '(' || *p == ')' ||
            *p == '{' || *p == '}' || *p == '<' || *p == '>') {
            NcMap *err = nc_map_new();
            nc_map_set(err, nc_string_from_cstr("error"),
                NC_STRING(nc_string_from_cstr("os.exec() rejected: command contains disallowed shell metacharacters")));
            return NC_MAP(err);
        }
    }

#ifdef NC_WINDOWS
    FILE *fp = _popen(command, "r");
#else
    FILE *fp = popen(command, "r");
#endif
    if (!fp) return NC_NONE();
    size_t cap = 4096;
    char *buf = malloc(cap);
    int oi = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        int llen = (int)strlen(line);
        while (oi + llen + 1 >= (int)cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + oi, line, llen);
        oi += llen;
    }
    buf[oi] = '\0';
#ifdef NC_WINDOWS
    _pclose(fp);
#else
    pclose(fp);
#endif
    NcValue result = NC_STRING(nc_string_from_cstr(buf));
    free(buf);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Module: DateTime
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_datetime_now(void) {
    time_t t = time(NULL);
    struct tm *tm_info = gmtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return NC_STRING(nc_string_from_cstr(buf));
}

NcValue nc_stdlib_datetime_parse(const char *str, const char *format) {
    if (!str || !format) return NC_NONE();
    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));
#ifdef NC_WINDOWS
    /* Windows does not have strptime — manual parse for ISO 8601 */
    if (sscanf(str, "%d-%d-%dT%d:%d:%d",
        &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday,
        &tm_info.tm_hour, &tm_info.tm_min, &tm_info.tm_sec) >= 3) {
        tm_info.tm_year -= 1900;
        tm_info.tm_mon -= 1;
        time_t t = mktime(&tm_info);
        return NC_FLOAT((double)t);
    }
    return NC_NONE();
#else
    char *res = strptime(str, format, &tm_info);
    if (!res) return NC_NONE();
    time_t t = mktime(&tm_info);
    return NC_FLOAT((double)t);
#endif
}

NcValue nc_stdlib_datetime_format(double timestamp, const char *format) {
    if (!format) return NC_NONE();
    time_t t = (time_t)timestamp;
    struct tm *tm_info = gmtime(&t);
    if (!tm_info) return NC_NONE();
    char buf[256];
    strftime(buf, sizeof(buf), format, tm_info);
    return NC_STRING(nc_string_from_cstr(buf));
}

NcValue nc_stdlib_datetime_diff(double a, double b) {
    return NC_FLOAT(a - b);
}

NcValue nc_stdlib_datetime_add(double timestamp, double seconds) {
    time_t t = (time_t)(timestamp + seconds);
    struct tm *tm_info = gmtime(&t);
    if (!tm_info) return NC_NONE();
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return NC_STRING(nc_string_from_cstr(buf));
}

/* ═══════════════════════════════════════════════════════════
 *  Module: Base64
 * ═══════════════════════════════════════════════════════════ */

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

NcValue nc_stdlib_base64_encode(const char *data) {
    if (!data) return NC_STRING(nc_string_from_cstr(""));
    size_t len = strlen(data);
    size_t out_len = ((len + 2) / 3) * 4;
    char *out = malloc(out_len + 1);
    if (!out) return NC_STRING(nc_string_from_cstr(""));
    int oi = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = ((uint32_t)(unsigned char)data[i]) << 16;
        if (i + 1 < len) n |= ((uint32_t)(unsigned char)data[i+1]) << 8;
        if (i + 2 < len) n |= (uint32_t)(unsigned char)data[i+2];
        out[oi++] = b64_table[(n >> 18) & 0x3F];
        out[oi++] = b64_table[(n >> 12) & 0x3F];
        out[oi++] = (i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=';
        out[oi++] = (i + 2 < len) ? b64_table[n & 0x3F] : '=';
    }
    out[oi] = '\0';
    NcValue result = NC_STRING(nc_string_from_cstr(out));
    free(out);
    return result;
}

static int b64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

NcValue nc_stdlib_base64_decode(const char *data) {
    if (!data) return NC_STRING(nc_string_from_cstr(""));
    size_t len = strlen(data);
    size_t out_len = (len / 4) * 3;
    char *out = malloc(out_len + 1);
    if (!out) return NC_STRING(nc_string_from_cstr(""));
    int oi = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        int a = b64_decode_char(data[i]);
        int b = b64_decode_char(data[i+1]);
        int c = (data[i+2] != '=') ? b64_decode_char(data[i+2]) : 0;
        int d = (data[i+3] != '=') ? b64_decode_char(data[i+3]) : 0;
        if (a < 0 || b < 0) continue;
        uint32_t n = (a << 18) | (b << 12) | (c << 6) | d;
        out[oi++] = (char)((n >> 16) & 0xFF);
        if (data[i+2] != '=') out[oi++] = (char)((n >> 8) & 0xFF);
        if (data[i+3] != '=') out[oi++] = (char)(n & 0xFF);
    }
    out[oi] = '\0';
    NcValue result = NC_STRING(nc_string_new(out, oi));
    free(out);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Module: Hashlib — MD5 and SHA-256 (pure C)
 * ═══════════════════════════════════════════════════════════ */

/* MD5 implementation from RFC 1321 */
static void nc_md5_raw(const uint8_t *data, size_t len, uint8_t out[16]) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89;
    uint32_t c0 = 0x98badcfe, d0 = 0x10325476;

    static const uint32_t T[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const int s[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };

    /* Padding */
    size_t new_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = calloc(new_len, 1);
    if (!msg) return;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        msg[new_len - 8 + i] = (uint8_t)(bit_len >> (i * 8));

    /* Process 64-byte blocks */
    for (size_t offset = 0; offset < new_len; offset += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = (uint32_t)msg[offset+i*4] | ((uint32_t)msg[offset+i*4+1]<<8) |
                   ((uint32_t)msg[offset+i*4+2]<<16) | ((uint32_t)msg[offset+i*4+3]<<24);

        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16)      { F = (B & C) | (~B & D);          g = i; }
            else if (i < 32) { F = (D & B) | (~D & C);          g = (5*i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D;                    g = (3*i + 5) % 16; }
            else              { F = C ^ (B | ~D);                 g = (7*i) % 16; }
            F += A + T[i] + M[g];
            A = D; D = C; C = B;
            B = B + ((F << s[i]) | (F >> (32 - s[i])));
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }
    free(msg);

    /* Output in little-endian */
    uint32_t h[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; i++) {
        out[i*4]   = (uint8_t)(h[i]);
        out[i*4+1] = (uint8_t)(h[i] >> 8);
        out[i*4+2] = (uint8_t)(h[i] >> 16);
        out[i*4+3] = (uint8_t)(h[i] >> 24);
    }
}

NcValue nc_stdlib_hash_md5(const char *data) {
    if (!data) return NC_NONE();
    uint8_t hash[16];
    nc_md5_raw((const uint8_t *)data, strlen(data), hash);
    char hex[33];
    for (int i = 0; i < 16; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    return NC_STRING(nc_string_from_cstr(hex));
}

/* SHA-256 is already implemented above as nc_stdlib_hash_sha256 */

/* ═══════════════════════════════════════════════════════════
 *  Module: UUID — v4 (random)
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_stdlib_uuid_v4(void) {
    /* Use rand() seeded by time — not cryptographically secure but sufficient */
    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL) ^ (unsigned)clock()); seeded = 1; }
    uint8_t bytes[16];
    for (int i = 0; i < 16; i++) bytes[i] = (uint8_t)(rand() & 0xFF);
    /* Set version 4 */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    /* Set variant 1 */
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    char buf[37];
    snprintf(buf, sizeof(buf),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],
        bytes[4],bytes[5],bytes[6],bytes[7],
        bytes[8],bytes[9],bytes[10],bytes[11],
        bytes[12],bytes[13],bytes[14],bytes[15]);
    return NC_STRING(nc_string_from_cstr(buf));
}
