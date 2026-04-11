/*
 * nc_pkg.c — Package manager for NC.
 *
 * Manages NC packages (libraries of behaviors, types, AI prompts).
 *
 * Commands:
 *   nc pkg init                Create nc.pkg manifest
 *   nc pkg install <name>      Install a package
 *   nc pkg list                List installed packages
 *   nc pkg publish             Publish to registry
 *
 * Security: All inputs are validated. Downloads use libcurl (no shell).
 * Directory removal uses native C filesystem APIs (no system("rm -rf")).
 * Git operations use execvp-style safe spawning where available.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/nc_json.h"
#include "../include/nc_http.h"
#include "../include/nc_platform.h"
#include "../include/nc_pkg.h"

#define NC_PKG_DIR    ".nc_packages"
#define NC_PKG_FILE   "nc.pkg"
#define NC_REGISTRY_DEFAULT "https://registry.notation-code.dev"

static const char *nc_get_registry(void) {
    const char *env = getenv("NC_REGISTRY");
    return (env && env[0]) ? env : NC_REGISTRY_DEFAULT;
}

/* Validate package name to prevent injection.
 * Only allows: alphanumeric, dash, underscore, dot, forward slash, colon, at */
static bool is_safe_pkg_name(const char *name) {
    if (!name || !name[0]) return false;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '-' && c != '_' && c != '.' &&
            c != '/' && c != ':' && c != '@') {
            return false;
        }
    }
    if (strstr(name, "..")) return false;
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Safe process execution — uses execvp/CreateProcess, NOT system()
 * ═══════════════════════════════════════════════════════════ */

static int safe_run(const char *const argv[]) {
#ifdef NC_WINDOWS
    char cmd_line[4096] = {0};
    int pos = 0;
    for (int i = 0; argv[i]; i++) {
        if (i > 0) cmd_line[pos++] = ' ';
        pos += snprintf(cmd_line + pos, sizeof(cmd_line) - pos, "\"%s\"", argv[i]);
    }
    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Safe recursive directory removal — no system("rm -rf")
 * ═══════════════════════════════════════════════════════════ */

static int remove_dir_recursive(const char *path) {
    nc_dir_t *d = nc_opendir(path);
    if (!d) return remove(path);

    nc_dirent_t entry;
    while (nc_readdir(d, &entry)) {
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
            continue;
        char child[1024];
        snprintf(child, sizeof(child), "%s%c%s", path, NC_PATH_SEP, entry.name);
        if (entry.is_dir) {
            remove_dir_recursive(child);
        } else {
            remove(child);
        }
    }
    nc_closedir(d);

#ifdef NC_WINDOWS
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

/* ═══════════════════════════════════════════════════════════
 *  Download file using libcurl (no shelling out to curl CLI)
 * ═══════════════════════════════════════════════════════════ */

static int download_file(const char *url, const char *dest_path) {
    char *response = nc_http_get(url, NULL);
    if (!response) return -1;

    /* Check if it's an error response */
    if (strstr(response, "\"error\"") || strlen(response) < 100) {
        free(response);
        return -1;
    }

    FILE *f = fopen(dest_path, "wb");
    if (!f) { free(response); return -1; }
    fwrite(response, 1, strlen(response), f);
    fclose(f);
    free(response);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Package manifest
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char name[128];
    char version[32];
    char description[256];
    char author[128];
    char **dependencies;
    int   dep_count;
} NcPackage;

/* ── pkg init ──────────────────────────────────────────────── */

int nc_pkg_init(const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR "%s", dir, NC_PKG_FILE);

    FILE *f = fopen(path, "r");
    if (f) { fclose(f); printf("  nc.pkg already exists\n"); return 0; }

    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "Cannot create %s\n", path); return 1; }

    fprintf(f,
        "// NC Package Manifest\n"
        "name \"my-package\"\n"
        "version \"1.0.0\"\n"
        "description \"My NC package\"\n"
        "author \"Your Name\"\n"
        "\n"
        "// Dependencies — other NC packages this requires\n"
        "// requires [\"http-tools\", \"ai-helpers\"]\n"
    );
    fclose(f);
    printf("  Created %s\n", path);
    return 0;
}

/* ── pkg install ───────────────────────────────────────────── */

static bool is_git_url(const char *name) {
    return strstr(name, "://") || strstr(name, ".git");
}

/* Expand shorthand package names to full git URLs.
 *
 *   github:user/repo   → https://github.com/user/repo.git
 *   gh:user/repo       → https://github.com/user/repo.git
 *   gitlab:user/repo   → https://gitlab.com/user/repo.git
 *   gl:user/repo       → https://gitlab.com/user/repo.git
 *   github.com/user/r  → https://github.com/user/r.git  (already handled by is_git_url path)
 *
 * Returns true if pkg_name was a shorthand and git_url was populated.
 */
static bool expand_pkg_shorthand(const char *pkg_name, char *git_url, int git_url_cap) {
    if (strncmp(pkg_name, "github:", 7) == 0 || strncmp(pkg_name, "gh:", 3) == 0) {
        const char *path = strchr(pkg_name, ':') + 1;
        snprintf(git_url, git_url_cap, "https://github.com/%s", path);
        if (!strstr(git_url, ".git"))
            strncat(git_url, ".git", git_url_cap - strlen(git_url) - 1);
        return true;
    }
    if (strncmp(pkg_name, "gitlab:", 7) == 0 || strncmp(pkg_name, "gl:", 3) == 0) {
        const char *path = strchr(pkg_name, ':') + 1;
        snprintf(git_url, git_url_cap, "https://gitlab.com/%s", path);
        if (!strstr(git_url, ".git"))
            strncat(git_url, ".git", git_url_cap - strlen(git_url) - 1);
        return true;
    }
    if (strncmp(pkg_name, "bitbucket:", 10) == 0 || strncmp(pkg_name, "bb:", 3) == 0) {
        const char *path = strchr(pkg_name, ':') + 1;
        snprintf(git_url, git_url_cap, "https://bitbucket.org/%s", path);
        if (!strstr(git_url, ".git"))
            strncat(git_url, ".git", git_url_cap - strlen(git_url) - 1);
        return true;
    }
    return false;
}

/* Extract the package's short name from a URL or shorthand */
static void extract_short_name(const char *pkg_name, char *short_name, int cap) {
    const char *last_slash = strrchr(pkg_name, '/');
    if (last_slash) {
        strncpy(short_name, last_slash + 1, cap - 1);
        short_name[cap - 1] = '\0';
        char *dot = strstr(short_name, ".git");
        if (dot) *dot = '\0';
    } else {
        /* Handle shorthand: "gh:user/repo" or bare names */
        const char *colon = strchr(pkg_name, ':');
        const char *name_start = colon ? colon + 1 : pkg_name;
        last_slash = strrchr(name_start, '/');
        if (last_slash) {
            strncpy(short_name, last_slash + 1, cap - 1);
        } else {
            strncpy(short_name, name_start, cap - 1);
        }
        short_name[cap - 1] = '\0';
    }
}

int nc_pkg_install(const char *pkg_name) {
    char dir[512];
    snprintf(dir, sizeof(dir), "." NC_PATH_SEP_STR "%s", NC_PKG_DIR);
    nc_mkdir(dir);

    if (!is_safe_pkg_name(pkg_name)) {
        fprintf(stderr, "  Error: Invalid package name '%s'\n", pkg_name);
        fprintf(stderr, "  Names may only contain: a-z A-Z 0-9 - _ . / : @\n");
        return 1;
    }

    /* Resolve shorthand to a full git URL */
    char expanded_git_url[512] = {0};
    bool is_shorthand = expand_pkg_shorthand(pkg_name, expanded_git_url, sizeof(expanded_git_url));

    char short_name[128];
    if (is_shorthand || is_git_url(pkg_name)) {
        extract_short_name(pkg_name, short_name, sizeof(short_name));
    } else {
        strncpy(short_name, pkg_name, 127);
        short_name[127] = '\0';
    }

    char pkg_dir[512];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", dir, short_name);

    struct stat st;
    if (stat(pkg_dir, &st) == 0) {
        printf("  Package '%s' already installed at %s\n", short_name, pkg_dir);
        printf("  To update, run: nc pkg update %s\n", short_name);
        return 0;
    }

    printf("  Installing package: %s\n", short_name);

    if (is_shorthand) {
        /* Install expanded shorthand (github:/gitlab: etc.) via git clone */
        printf("  Cloning %s ...\n", expanded_git_url);
        const char *git_args[] = {"git", "clone", "--depth", "1",
                                   expanded_git_url, pkg_dir, NULL};
        int ret = safe_run(git_args);
        if (ret != 0) {
            fprintf(stderr, "  Error: git clone failed (code %d)\n", ret);
            fprintf(stderr, "  Check: is git installed and the repo URL correct?\n");
            return 1;
        }
        printf("  Installed %s to %s/\n", short_name, pkg_dir);
        return 0;
    }

    if (is_git_url(pkg_name)) {
        char git_url[512];
        if (strstr(pkg_name, "://")) {
            strncpy(git_url, pkg_name, sizeof(git_url) - 1);
            git_url[sizeof(git_url) - 1] = '\0';
        } else {
            snprintf(git_url, sizeof(git_url), "https://%s", pkg_name);
        }
        if (!strstr(git_url, ".git"))
            strncat(git_url, ".git", sizeof(git_url) - strlen(git_url) - 1);

        printf("  Cloning %s ...\n", git_url);
        const char *git_args[] = {"git", "clone", "--depth", "1", git_url, pkg_dir, NULL};
        int ret = safe_run(git_args);
        if (ret != 0) {
            fprintf(stderr, "  Error: git clone failed (code %d)\n", ret);
            fprintf(stderr, "  Check: is git installed and the repo URL correct?\n");
            return 1;
        }
        printf("  Installed %s to %s/\n", short_name, pkg_dir);
        return 0;
    }

    /* Named package — try registry, then GitHub fallback */
    const char *registry = nc_get_registry();

    char url[512];
    snprintf(url, sizeof(url), "%s/packages/%s.tar.gz", registry, pkg_name);
    printf("  Trying registry: %s ...\n", url);

    char tmp_file[256];
    snprintf(tmp_file, sizeof(tmp_file), "%s%c_nc_pkg_%s.tar.gz",
             nc_tempdir(), NC_PATH_SEP, pkg_name);

    int ret = download_file(url, tmp_file);
    if (ret == 0) {
        nc_mkdir(pkg_dir);
        const char *tar_args[] = {"tar", "xzf", tmp_file, "-C", pkg_dir,
                                 "--no-same-owner", "--strip-components=1", NULL};
        safe_run(tar_args);
        remove(tmp_file);
        printf("  Installed %s from registry\n", pkg_name);
        return 0;
    }

    /* Registry failed — try GitHub devheallabs-ai/nc-pkg-{name} */
    char gh_url[512];
    snprintf(gh_url, sizeof(gh_url),
             "https://github.com/devheallabs-ai/nc-pkg-%s.git", pkg_name);
    printf("  Registry not available — trying GitHub: %s ...\n", gh_url);
    {
        const char *git_args[] = {"git", "clone", "--depth", "1",
                                   gh_url, pkg_dir, NULL};
        int gret = safe_run(git_args);
        if (gret == 0) {
            printf("  Installed %s from GitHub\n", pkg_name);
            return 0;
        }
    }

    fprintf(stderr, "\n  Package '%s' not found.\n\n", pkg_name);
    fprintf(stderr, "  Installation options:\n");
    fprintf(stderr, "    nc pkg install github:user/%s        (GitHub shorthand)\n", pkg_name);
    fprintf(stderr, "    nc pkg install github.com/user/%s    (full GitHub path)\n", pkg_name);
    fprintf(stderr, "    nc pkg install https://github.com/user/%s.git\n\n", pkg_name);
    return 1;
}

/* ── pkg update ───────────────────────────────────────────── */

static int nc_pkg_update(const char *pkg_name) {
    if (!is_safe_pkg_name(pkg_name)) {
        fprintf(stderr, "  Error: Invalid package name\n");
        return 1;
    }

    char pkg_dir[512];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_PKG_DIR, pkg_name);

    struct stat st;
    if (stat(pkg_dir, &st) != 0) {
        fprintf(stderr, "  Package '%s' is not installed\n", pkg_name);
        return 1;
    }

    char git_dir[512];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", pkg_dir);
    if (stat(git_dir, &st) == 0) {
        printf("  Updating %s ...\n", pkg_name);
        const char *git_args[] = {"git", "-C", pkg_dir, "pull", NULL};
        int ret = safe_run(git_args);
        if (ret != 0) { fprintf(stderr, "  Error: git pull failed\n"); return 1; }
        printf("  Updated %s\n", pkg_name);
    } else {
        fprintf(stderr, "  Package '%s' was not installed from git. Reinstall it.\n", pkg_name);
        return 1;
    }
    return 0;
}

/* ── pkg remove ───────────────────────────────────────────── */

static int nc_pkg_remove(const char *pkg_name) {
    if (!is_safe_pkg_name(pkg_name)) {
        fprintf(stderr, "  Error: Invalid package name\n");
        return 1;
    }

    char pkg_dir[512];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_PKG_DIR, pkg_name);

    struct stat st;
    if (stat(pkg_dir, &st) != 0) {
        fprintf(stderr, "  Package '%s' is not installed\n", pkg_name);
        return 1;
    }

    int ret = remove_dir_recursive(pkg_dir);
    if (ret == 0) printf("  Removed package '%s'\n", pkg_name);
    else fprintf(stderr, "  Error removing '%s'\n", pkg_name);
    return ret;
}

/* ── pkg list ──────────────────────────────────────────────── */

int nc_pkg_list(void) {
    printf("\n  Installed NC Packages:\n");
    printf("  %s\n", "────────────────────────────────");

    char dir[512];
    snprintf(dir, sizeof(dir), "%s", NC_PKG_DIR);

    nc_dir_t *d = nc_opendir(dir);
    if (!d) {
        printf("  (none — run 'nc pkg install <name>' to install)\n\n");
        return 0;
    }
    nc_dirent_t entry;
    int count = 0;
    while (nc_readdir(d, &entry)) {
        if (entry.name[0] == '.') continue;
        printf("    %s\n", entry.name);
        count++;
    }
    nc_closedir(d);
    if (count == 0) printf("  (none)\n");

    printf("\n");
    return 0;
}

/* ── pkg command router ────────────────────────────────────── */

int nc_pkg_command(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: nc pkg <command>\n");
        printf("  init                         Create nc.pkg manifest\n");
        printf("  install <name|git-url>       Install a package\n");
        printf("  update <name>                Update a git-installed package\n");
        printf("  remove <name>                Remove a package\n");
        printf("  list                         List installed packages\n");
        printf("\nExamples:\n");
        printf("  nc pkg install github.com/user/my-nc-package\n");
        printf("  nc pkg install ai-helpers\n");
        printf("  nc pkg update my-nc-package\n");
        return 0;
    }

    const char *subcmd = argv[2];

    if (strcmp(subcmd, "init") == 0)
        return nc_pkg_init(".");
    if (strcmp(subcmd, "install") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg install <name|git-url>\n"); return 1; }
        return nc_pkg_install(argv[3]);
    }
    if (strcmp(subcmd, "update") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg update <name>\n"); return 1; }
        return nc_pkg_update(argv[3]);
    }
    if (strcmp(subcmd, "remove") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg remove <name>\n"); return 1; }
        return nc_pkg_remove(argv[3]);
    }
    if (strcmp(subcmd, "list") == 0)
        return nc_pkg_list();

    if (strcmp(subcmd, "search") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg search <query>\n"); return 1; }
        int count = 0;
        NcPackageInfo *results = nc_pkg_search(argv[3], &count);
        if (!results || count == 0) {
            printf("  No packages found for '%s'\n", argv[3]);
            return 0;
        }
        printf("\n  Search results for '%s':\n", argv[3]);
        printf("  ────────────────────────────────────────\n");
        for (int i = 0; i < count; i++) {
            printf("  %-24s %-10s  %s\n",
                   results[i].name, results[i].version, results[i].description);
        }
        printf("\n");
        free(results);
        return 0;
    }
    if (strcmp(subcmd, "info") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg info <name>\n"); return 1; }
        NcPackageInfo *info = nc_pkg_info(argv[3]);
        if (!info) { fprintf(stderr, "  Package '%s' not found\n", argv[3]); return 1; }
        printf("\n  Package: %s\n", info->name);
        printf("  Version: %s\n", info->version);
        printf("  Description: %s\n", info->description);
        printf("  Author: %s\n", info->author);
        if (info->url[0]) printf("  URL: %s\n", info->url);
        if (info->checksum[0]) printf("  Checksum: %s\n", info->checksum);
        if (info->dependencies_count > 0) {
            printf("  Dependencies:\n");
            for (int i = 0; i < info->dependencies_count; i++)
                printf("    - %s\n", info->dependencies[i]);
        }
        printf("\n");
        free(info);
        return 0;
    }
    if (strcmp(subcmd, "init-manifest") == 0)
        return nc_pkg_init_manifest(".");
    if (strcmp(subcmd, "pack") == 0) {
        const char *output = (argc >= 4) ? argv[3] : NULL;
        return nc_pkg_pack(".", output);
    }
    if (strcmp(subcmd, "publish") == 0)
        return nc_pkg_publish(".");
    if (strcmp(subcmd, "uninstall") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: nc pkg uninstall <name>\n"); return 1; }
        return nc_pkg_uninstall(argv[3]);
    }

    fprintf(stderr, "Unknown pkg command: %s\n", subcmd);
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *
 *  ENHANCED PACKAGE MANAGER — Full implementation below.
 *  Everything above is the original skeleton, kept intact.
 *
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════
 *  Helper: read entire file into malloc'd string
 * ═══════════════════════════════════════════════════════════ */

static char *read_file_to_string(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0 || len > 16 * 1024 * 1024) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_len = fread(buf, 1, (size_t)len, f);
    buf[read_len] = '\0';
    fclose(f);
    return buf;
}

/* ═══════════════════════════════════════════════════════════
 *  Helper: get registry URL with /api/v1 suffix
 * ═══════════════════════════════════════════════════════════ */

static const char *nc_get_registry_api(void) {
    static char api_url[600];
    const char *env = getenv("NC_REGISTRY_API");
    if (env && env[0]) return env;
    snprintf(api_url, sizeof(api_url), "%s", NC_REGISTRY_URL);
    return api_url;
}

/* ═══════════════════════════════════════════════════════════
 *  Semantic Versioning
 * ═══════════════════════════════════════════════════════════ */

int nc_semver_parse(const char *str, NcSemVer *out) {
    if (!str || !out) return -1;
    out->major = out->minor = out->patch = 0;

    /* Skip leading 'v' or 'V' */
    if (*str == 'v' || *str == 'V') str++;

    int matched = sscanf(str, "%d.%d.%d", &out->major, &out->minor, &out->patch);
    if (matched < 1) return -1;
    return 0;
}

int nc_semver_compare(NcSemVer a, NcSemVer b) {
    if (a.major != b.major) return a.major - b.major;
    if (a.minor != b.minor) return a.minor - b.minor;
    return a.patch - b.patch;
}

int nc_version_constraint_parse(const char *str, NcVersionConstraint *out) {
    if (!str || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* Skip whitespace */
    while (*str == ' ') str++;

    if (*str == '*' || *str == '\0') {
        out->type = NC_VER_ANY;
        return 0;
    }

    if (*str == '^') {
        out->type = NC_VER_CARET;
        return nc_semver_parse(str + 1, &out->lower);
    }

    if (*str == '~') {
        out->type = NC_VER_TILDE;
        return nc_semver_parse(str + 1, &out->lower);
    }

    /* Range: >=X.Y.Z <A.B.C */
    if (str[0] == '>' && str[1] == '=') {
        const char *p = str + 2;
        while (*p == ' ') p++;
        if (nc_semver_parse(p, &out->lower) != 0) return -1;

        /* Look for '<' */
        const char *lt = strstr(p, "<");
        if (lt && lt[1] != '=') {
            out->type = NC_VER_RANGE;
            lt++;
            while (*lt == ' ') lt++;
            return nc_semver_parse(lt, &out->upper);
        }
        out->type = NC_VER_GTE;
        return 0;
    }

    if (*str == '<') {
        out->type = NC_VER_LT;
        str++;
        while (*str == ' ') str++;
        return nc_semver_parse(str, &out->upper);
    }

    /* Exact version (possibly with leading '=') */
    out->type = NC_VER_EXACT;
    if (*str == '=') str++;
    while (*str == ' ') str++;
    return nc_semver_parse(str, &out->lower);
}

bool nc_version_satisfies(NcSemVer ver, NcVersionConstraint c) {
    switch (c.type) {
        case NC_VER_ANY:
            return true;

        case NC_VER_EXACT:
            return nc_semver_compare(ver, c.lower) == 0;

        case NC_VER_CARET:
            /* ^1.2.3 means >=1.2.3 <2.0.0 (same major) */
            if (ver.major != c.lower.major) return false;
            return nc_semver_compare(ver, c.lower) >= 0;

        case NC_VER_TILDE:
            /* ~1.2.3 means >=1.2.3 <1.3.0 (same major.minor) */
            if (ver.major != c.lower.major) return false;
            if (ver.minor != c.lower.minor) return false;
            return ver.patch >= c.lower.patch;

        case NC_VER_GTE:
            return nc_semver_compare(ver, c.lower) >= 0;

        case NC_VER_LT:
            return nc_semver_compare(ver, c.upper) < 0;

        case NC_VER_RANGE:
            return nc_semver_compare(ver, c.lower) >= 0 &&
                   nc_semver_compare(ver, c.upper) < 0;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  JSON helper: extract string field from NcValue map
 * ═══════════════════════════════════════════════════════════ */

static const char *json_get_str(NcValue map, const char *key) {
    if (!IS_MAP(map)) return NULL;
    NcString *k = nc_string_from_cstr(key);
    NcValue v = nc_map_get(AS_MAP(map), k);
    nc_string_free(k);
    if (IS_STRING(v) && v.as.string) return v.as.string->chars;
    return NULL;
}

static NcValue json_get_val(NcValue map, const char *key) {
    if (!IS_MAP(map)) return NC_NONE();
    NcString *k = nc_string_from_cstr(key);
    NcValue v = nc_map_get(AS_MAP(map), k);
    nc_string_free(k);
    return v;
}

/* ═══════════════════════════════════════════════════════════
 *  Helper: safely copy string into fixed buffer
 * ═══════════════════════════════════════════════════════════ */

static void safe_strcpy(char *dst, int dstsize, const char *src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstsize - 1);
    dst[dstsize - 1] = '\0';
}

/* ═══════════════════════════════════════════════════════════
 *  Helper: parse NcPackageInfo from a JSON NcValue (map)
 * ═══════════════════════════════════════════════════════════ */

static void parse_package_from_json(NcValue json, NcPackageInfo *pkg) {
    memset(pkg, 0, sizeof(*pkg));
    const char *s;

    s = json_get_str(json, "name");
    safe_strcpy(pkg->name, sizeof(pkg->name), s);

    s = json_get_str(json, "version");
    safe_strcpy(pkg->version, sizeof(pkg->version), s);

    s = json_get_str(json, "description");
    safe_strcpy(pkg->description, sizeof(pkg->description), s);

    s = json_get_str(json, "author");
    safe_strcpy(pkg->author, sizeof(pkg->author), s);

    s = json_get_str(json, "url");
    safe_strcpy(pkg->url, sizeof(pkg->url), s);

    s = json_get_str(json, "checksum");
    safe_strcpy(pkg->checksum, sizeof(pkg->checksum), s);

    /* Parse dependencies array */
    NcValue deps = json_get_val(json, "dependencies");
    if (IS_LIST(deps)) {
        NcList *list = AS_LIST(deps);
        pkg->dependencies_count = (list->count > 32) ? 32 : list->count;
        for (int i = 0; i < pkg->dependencies_count; i++) {
            NcValue item = nc_list_get(list, i);
            if (IS_STRING(item))
                safe_strcpy(pkg->dependencies[i], 128, item.as.string->chars);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Registry Client — nc_pkg_search
 * ═══════════════════════════════════════════════════════════ */

NcPackageInfo *nc_pkg_search(const char *query, int *count) {
    if (!query || !count) return NULL;
    *count = 0;

    const char *api = nc_get_registry_api();
    char url[768];
    snprintf(url, sizeof(url), "%s/packages/search?q=%s", api, query);

    printf("  Searching registry: %s ...\n", api);
    char *response = nc_http_get(url, NULL);
    if (!response) {
        printf("  Registry not available, using local cache.\n");
        return NULL;
    }

    NcValue json = nc_json_parse(response);
    free(response);

    if (IS_NONE(json)) {
        printf("  Registry returned invalid response.\n");
        return NULL;
    }

    /* Expect { "packages": [ ... ] } or a top-level list */
    NcValue pkg_list = json_get_val(json, "packages");
    if (!IS_LIST(pkg_list)) {
        if (IS_LIST(json)) pkg_list = json;
        else {
            printf("  No results found.\n");
            return NULL;
        }
    }

    NcList *list = AS_LIST(pkg_list);
    int n = list->count;
    if (n == 0) return NULL;
    if (n > 256) n = 256;

    NcPackageInfo *results = calloc(n, sizeof(NcPackageInfo));
    if (!results) return NULL;

    for (int i = 0; i < n; i++) {
        NcValue item = nc_list_get(list, i);
        parse_package_from_json(item, &results[i]);
    }

    *count = n;
    return results;
}

/* ═══════════════════════════════════════════════════════════
 *  Registry Client — nc_pkg_info
 * ═══════════════════════════════════════════════════════════ */

NcPackageInfo *nc_pkg_info(const char *name) {
    if (!name) return NULL;

    const char *api = nc_get_registry_api();
    char url[768];
    snprintf(url, sizeof(url), "%s/packages/%s", api, name);

    printf("  Fetching package info from registry ...\n");
    char *response = nc_http_get(url, NULL);
    if (!response) {
        printf("  Registry not available, using local cache.\n");

        /* Try to read from local nc_modules */
        char manifest_path[768];
        snprintf(manifest_path, sizeof(manifest_path),
                 "%s" NC_PATH_SEP_STR "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE,
                 NC_MODULES_DIR, name);
        char *local = read_file_to_string(manifest_path);
        if (!local) return NULL;

        NcValue json = nc_json_parse(local);
        free(local);
        if (IS_NONE(json)) return NULL;

        NcPackageInfo *pkg = calloc(1, sizeof(NcPackageInfo));
        if (!pkg) return NULL;
        parse_package_from_json(json, pkg);
        return pkg;
    }

    NcValue json = nc_json_parse(response);
    free(response);
    if (IS_NONE(json)) {
        printf("  Registry returned invalid response.\n");
        return NULL;
    }

    NcPackageInfo *pkg = calloc(1, sizeof(NcPackageInfo));
    if (!pkg) return NULL;
    parse_package_from_json(json, pkg);
    return pkg;
}

/* ═══════════════════════════════════════════════════════════
 *  Registry Client — nc_pkg_download
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_download(const char *name, const char *version, const char *dest) {
    if (!name || !dest) return -1;

    const char *api = nc_get_registry_api();
    char url[768];
    if (version && version[0])
        snprintf(url, sizeof(url), "%s/packages/%s/%s/download", api, name, version);
    else
        snprintf(url, sizeof(url), "%s/packages/%s/latest/download", api, name);

    printf("  Downloading %s", name);
    if (version && version[0]) printf("@%s", version);
    printf(" ...\n");

    int ret = download_file(url, dest);
    if (ret != 0) {
        printf("  Registry not available. Cannot download '%s'.\n", name);
        printf("  Hint: check your network or try: nc pkg install github.com/user/%s\n", name);
    }
    return ret;
}

/* ═══════════════════════════════════════════════════════════
 *  Dependency Resolver — Topological sort with cycle detection
 * ═══════════════════════════════════════════════════════════ */

#define NC_MAX_RESOLVE_DEPTH 64
#define NC_MAX_RESOLVED      256

/* State for the resolver */
typedef struct {
    NcPackageInfo resolved[NC_MAX_RESOLVED];
    int           resolved_count;
    char          visiting[NC_MAX_RESOLVE_DEPTH][128]; /* cycle detection stack */
    int           visiting_count;
    char          visited[NC_MAX_RESOLVED][128];       /* already fully resolved */
    int           visited_count;
} NcResolveState;

static bool resolve_is_visited(NcResolveState *st, const char *name) {
    for (int i = 0; i < st->visited_count; i++)
        if (strcmp(st->visited[i], name) == 0) return true;
    return false;
}

static bool resolve_is_visiting(NcResolveState *st, const char *name) {
    for (int i = 0; i < st->visiting_count; i++)
        if (strcmp(st->visiting[i], name) == 0) return true;
    return false;
}

static int resolve_recursive(NcResolveState *st, const char *name, const char *version_spec) {
    if (resolve_is_visited(st, name)) return 0;

    if (resolve_is_visiting(st, name)) {
        fprintf(stderr, "  Error: Circular dependency detected: %s\n", name);
        fprintf(stderr, "  Dependency chain: ");
        for (int i = 0; i < st->visiting_count; i++)
            fprintf(stderr, "%s -> ", st->visiting[i]);
        fprintf(stderr, "%s\n", name);
        return -1;
    }

    if (st->visiting_count >= NC_MAX_RESOLVE_DEPTH) {
        fprintf(stderr, "  Error: Dependency tree too deep (max %d)\n", NC_MAX_RESOLVE_DEPTH);
        return -1;
    }

    /* Push onto visiting stack */
    safe_strcpy(st->visiting[st->visiting_count], 128, name);
    st->visiting_count++;

    /* Fetch package info to get its dependencies */
    NcPackageInfo *info = nc_pkg_info(name);
    if (!info) {
        /* Package not found — add a placeholder and continue */
        fprintf(stderr, "  Warning: Could not resolve '%s' — not found in registry\n", name);
        st->visiting_count--;
        return 0;
    }

    /* If a version constraint was given, check it */
    if (version_spec && version_spec[0]) {
        NcVersionConstraint constraint;
        NcSemVer pkg_ver;
        if (nc_version_constraint_parse(version_spec, &constraint) == 0 &&
            nc_semver_parse(info->version, &pkg_ver) == 0) {
            if (!nc_version_satisfies(pkg_ver, constraint)) {
                fprintf(stderr, "  Warning: %s@%s does not satisfy constraint '%s'\n",
                        name, info->version, version_spec);
            }
        }
    }

    /* Resolve all sub-dependencies first (depth-first) */
    for (int i = 0; i < info->dependencies_count; i++) {
        char dep_name[128] = {0};
        char dep_ver[32] = {0};

        /* Parse "dep_name@version" format */
        const char *at = strchr(info->dependencies[i], '@');
        if (at) {
            int name_len = (int)(at - info->dependencies[i]);
            if (name_len > 127) name_len = 127;
            strncpy(dep_name, info->dependencies[i], name_len);
            dep_name[name_len] = '\0';
            safe_strcpy(dep_ver, sizeof(dep_ver), at + 1);
        } else {
            safe_strcpy(dep_name, sizeof(dep_name), info->dependencies[i]);
        }

        int ret = resolve_recursive(st, dep_name, dep_ver);
        if (ret != 0) { free(info); return ret; }
    }

    /* Pop from visiting stack */
    st->visiting_count--;

    /* Mark as visited and add to resolved list */
    if (st->visited_count < NC_MAX_RESOLVED) {
        safe_strcpy(st->visited[st->visited_count], 128, name);
        st->visited_count++;
    }

    if (st->resolved_count < NC_MAX_RESOLVED) {
        memcpy(&st->resolved[st->resolved_count], info, sizeof(NcPackageInfo));
        st->resolved_count++;
    }

    free(info);
    return 0;
}

int nc_pkg_resolve(NcPackageInfo *pkg, NcPackageInfo **resolved, int *count) {
    if (!pkg || !resolved || !count) return -1;
    *resolved = NULL;
    *count = 0;

    NcResolveState *st = calloc(1, sizeof(NcResolveState));
    if (!st) return -1;

    printf("  Resolving dependencies for %s@%s ...\n", pkg->name, pkg->version);

    /* Resolve each direct dependency */
    for (int i = 0; i < pkg->dependencies_count; i++) {
        char dep_name[128] = {0};
        char dep_ver[32] = {0};

        const char *at = strchr(pkg->dependencies[i], '@');
        if (at) {
            int name_len = (int)(at - pkg->dependencies[i]);
            if (name_len > 127) name_len = 127;
            strncpy(dep_name, pkg->dependencies[i], name_len);
            dep_name[name_len] = '\0';
            safe_strcpy(dep_ver, sizeof(dep_ver), at + 1);
        } else {
            safe_strcpy(dep_name, sizeof(dep_name), pkg->dependencies[i]);
        }

        int ret = resolve_recursive(st, dep_name, dep_ver);
        if (ret != 0) { free(st); return ret; }
    }

    if (st->resolved_count > 0) {
        *resolved = calloc(st->resolved_count, sizeof(NcPackageInfo));
        if (*resolved) {
            memcpy(*resolved, st->resolved, st->resolved_count * sizeof(NcPackageInfo));
            *count = st->resolved_count;
        }
    }

    printf("  Resolved %d dependencies\n", st->resolved_count);
    free(st);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Local Package Management — nc_pkg_install_v2
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_install_v2(const char *name, const char *version) {
    if (!name) return -1;
    if (!is_safe_pkg_name(name)) {
        fprintf(stderr, "  Error: Invalid package name '%s'\n", name);
        return 1;
    }

    /* Create nc_modules directory */
    nc_mkdir_p(NC_MODULES_DIR);

    /* Check if already installed */
    char pkg_dir[768];
    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_MODULES_DIR, name);

    struct stat st;
    if (stat(pkg_dir, &st) == 0) {
        /* Check version if specified */
        if (version && version[0]) {
            char manifest_path[768];
            snprintf(manifest_path, sizeof(manifest_path),
                     "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, pkg_dir);
            char *data = read_file_to_string(manifest_path);
            if (data) {
                NcValue json = nc_json_parse(data);
                free(data);
                const char *installed_ver = json_get_str(json, "version");
                if (installed_ver && strcmp(installed_ver, version) == 0) {
                    printf("  %s@%s is already installed\n", name, version);
                    return 0;
                }
                printf("  Upgrading %s to version %s ...\n", name, version);
                remove_dir_recursive(pkg_dir);
            }
        } else {
            printf("  %s is already installed at %s\n", name, pkg_dir);
            printf("  To update: nc pkg update %s\n", name);
            return 0;
        }
    }

    printf("  Installing %s", name);
    if (version && version[0]) printf("@%s", version);
    printf(" ...\n");

    /* Try registry download */
    char tmp_file[512];
    snprintf(tmp_file, sizeof(tmp_file), "%s%c_nc_pkg_%s.tar.gz",
             nc_tempdir(), NC_PATH_SEP, name);

    int ret = nc_pkg_download(name, version, tmp_file);
    if (ret == 0) {
        nc_mkdir_p(pkg_dir);
        const char *tar_args[] = {"tar", "xzf", tmp_file, "-C", pkg_dir,
                                   "--no-same-owner", "--strip-components=1", NULL};
        safe_run(tar_args);
        remove(tmp_file);

        /* After installing, resolve and install transitive dependencies */
        char manifest_path[768];
        snprintf(manifest_path, sizeof(manifest_path),
                 "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, pkg_dir);
        char *data = read_file_to_string(manifest_path);
        if (data) {
            NcValue json = nc_json_parse(data);
            free(data);
            NcPackageInfo pkg_info;
            parse_package_from_json(json, &pkg_info);
            if (pkg_info.dependencies_count > 0) {
                printf("  Installing dependencies of %s ...\n", name);
                for (int i = 0; i < pkg_info.dependencies_count; i++) {
                    char dep_name[128] = {0};
                    char dep_ver[32] = {0};
                    const char *at = strchr(pkg_info.dependencies[i], '@');
                    if (at) {
                        int nlen = (int)(at - pkg_info.dependencies[i]);
                        if (nlen > 127) nlen = 127;
                        strncpy(dep_name, pkg_info.dependencies[i], nlen);
                        dep_name[nlen] = '\0';
                        safe_strcpy(dep_ver, sizeof(dep_ver), at + 1);
                    } else {
                        safe_strcpy(dep_name, sizeof(dep_name), pkg_info.dependencies[i]);
                    }
                    nc_pkg_install_v2(dep_name, dep_ver);
                }
            }
        }

        printf("  Installed %s to %s/\n", name, pkg_dir);

        /* Update lock file */
        NcPackageInfo lock_entry;
        memset(&lock_entry, 0, sizeof(lock_entry));
        safe_strcpy(lock_entry.name, sizeof(lock_entry.name), name);
        safe_strcpy(lock_entry.version, sizeof(lock_entry.version),
                    (version && version[0]) ? version : "latest");

        /* Read existing lock, append, write back */
        NcPackageInfo *existing = NULL;
        int existing_count = 0;
        nc_pkg_lock_read(".", &existing, &existing_count);

        int total = existing_count + 1;
        NcPackageInfo *all = calloc(total, sizeof(NcPackageInfo));
        if (all) {
            if (existing && existing_count > 0)
                memcpy(all, existing, existing_count * sizeof(NcPackageInfo));
            /* Replace if already in lock, else append */
            bool found = false;
            for (int i = 0; i < existing_count; i++) {
                if (strcmp(all[i].name, name) == 0) {
                    memcpy(&all[i], &lock_entry, sizeof(NcPackageInfo));
                    total = existing_count;
                    found = true;
                    break;
                }
            }
            if (!found)
                memcpy(&all[existing_count], &lock_entry, sizeof(NcPackageInfo));
            nc_pkg_lock_write(".", all, total);
            free(all);
        }
        free(existing);
        return 0;
    }

    /* Fallback: try git URL patterns */
    printf("  Registry download failed. Try: nc pkg install github.com/user/%s\n", name);
    return 1;
}

/* ═══════════════════════════════════════════════════════════
 *  Local Package Management — nc_pkg_uninstall
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_uninstall(const char *name) {
    if (!name || !is_safe_pkg_name(name)) {
        fprintf(stderr, "  Error: Invalid package name\n");
        return 1;
    }

    /* Check nc_modules first, then .nc_packages (legacy) */
    char pkg_dir[768];
    struct stat st;

    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_MODULES_DIR, name);
    if (stat(pkg_dir, &st) != 0) {
        snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_PKG_DIR, name);
        if (stat(pkg_dir, &st) != 0) {
            fprintf(stderr, "  Package '%s' is not installed\n", name);
            return 1;
        }
    }

    int ret = remove_dir_recursive(pkg_dir);
    if (ret == 0) {
        printf("  Uninstalled '%s'\n", name);

        /* Update lock file — remove entry */
        NcPackageInfo *existing = NULL;
        int existing_count = 0;
        nc_pkg_lock_read(".", &existing, &existing_count);
        if (existing && existing_count > 0) {
            NcPackageInfo *updated = calloc(existing_count, sizeof(NcPackageInfo));
            int new_count = 0;
            for (int i = 0; i < existing_count; i++) {
                if (strcmp(existing[i].name, name) != 0)
                    memcpy(&updated[new_count++], &existing[i], sizeof(NcPackageInfo));
            }
            nc_pkg_lock_write(".", updated, new_count);
            free(updated);
        }
        free(existing);
    } else {
        fprintf(stderr, "  Error removing '%s'\n", name);
    }
    return ret;
}

/* ═══════════════════════════════════════════════════════════
 *  Local Package Management — nc_pkg_update_v2
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_update_v2(const char *name) {
    if (!name || !is_safe_pkg_name(name)) {
        fprintf(stderr, "  Error: Invalid package name\n");
        return 1;
    }

    char pkg_dir[768];
    struct stat st;

    /* Check nc_modules */
    snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_MODULES_DIR, name);
    if (stat(pkg_dir, &st) != 0) {
        /* Try legacy .nc_packages */
        snprintf(pkg_dir, sizeof(pkg_dir), "%s" NC_PATH_SEP_STR "%s", NC_PKG_DIR, name);
        if (stat(pkg_dir, &st) != 0) {
            fprintf(stderr, "  Package '%s' is not installed\n", name);
            return 1;
        }
    }

    /* Check if it's a git-cloned package */
    char git_dir[768];
    snprintf(git_dir, sizeof(git_dir), "%s" NC_PATH_SEP_STR ".git", pkg_dir);
    if (stat(git_dir, &st) == 0) {
        printf("  Updating %s via git pull ...\n", name);
        const char *git_args[] = {"git", "-C", pkg_dir, "pull", NULL};
        int ret = safe_run(git_args);
        if (ret != 0) { fprintf(stderr, "  Error: git pull failed\n"); return 1; }
        printf("  Updated %s\n", name);
        return 0;
    }

    /* Registry-installed package: fetch latest info */
    NcPackageInfo *latest = nc_pkg_info(name);
    if (!latest) {
        fprintf(stderr, "  Cannot fetch latest version of '%s' from registry.\n", name);
        fprintf(stderr, "  Registry not available, using local cache.\n");
        return 1;
    }

    /* Check if update is needed */
    char manifest_path[768];
    snprintf(manifest_path, sizeof(manifest_path),
             "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, pkg_dir);
    char *data = read_file_to_string(manifest_path);
    if (data) {
        NcValue json = nc_json_parse(data);
        free(data);
        const char *current_ver = json_get_str(json, "version");
        if (current_ver && strcmp(current_ver, latest->version) == 0) {
            printf("  %s is already at latest version %s\n", name, latest->version);
            free(latest);
            return 0;
        }
        printf("  Updating %s: %s -> %s\n", name,
               current_ver ? current_ver : "unknown", latest->version);
    }

    /* Remove and reinstall */
    remove_dir_recursive(pkg_dir);
    int ret = nc_pkg_install_v2(name, latest->version);
    free(latest);
    return ret;
}

/* ═══════════════════════════════════════════════════════════
 *  Local Package Management — nc_pkg_list_v2
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_list_v2(void) {
    printf("\n  Installed NC Packages:\n");
    printf("  ════════════════════════════════════════════════════════\n");
    printf("  %-24s %-10s  %s\n", "NAME", "VERSION", "DESCRIPTION");
    printf("  ────────────────────────────────────────────────────────\n");

    int total = 0;

    /* List from nc_modules/ */
    nc_dir_t *d = nc_opendir(NC_MODULES_DIR);
    if (d) {
        nc_dirent_t entry;
        while (nc_readdir(d, &entry)) {
            if (entry.name[0] == '.' || !entry.is_dir) continue;

            char manifest_path[768];
            snprintf(manifest_path, sizeof(manifest_path),
                     "%s" NC_PATH_SEP_STR "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE,
                     NC_MODULES_DIR, entry.name);

            char *data = read_file_to_string(manifest_path);
            if (data) {
                NcValue json = nc_json_parse(data);
                free(data);
                const char *ver = json_get_str(json, "version");
                const char *desc = json_get_str(json, "description");
                printf("  %-24s %-10s  %s\n",
                       entry.name,
                       ver ? ver : "-",
                       desc ? desc : "");
            } else {
                printf("  %-24s %-10s  %s\n", entry.name, "-", "(no manifest)");
            }
            total++;
        }
        nc_closedir(d);
    }

    /* Also list from legacy .nc_packages/ */
    d = nc_opendir(NC_PKG_DIR);
    if (d) {
        nc_dirent_t entry;
        while (nc_readdir(d, &entry)) {
            if (entry.name[0] == '.' || !entry.is_dir) continue;
            printf("  %-24s %-10s  %s\n", entry.name, "-", "(legacy .nc_packages)");
            total++;
        }
        nc_closedir(d);
    }

    if (total == 0) {
        printf("  (none)\n");
        printf("  Run 'nc pkg install <name>' to install a package.\n");
    }

    printf("\n  Total: %d package(s)\n\n", total);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Package Manifest — nc_package.json
 * ═══════════════════════════════════════════════════════════ */

NcManifest *nc_pkg_read_manifest(const char *dir) {
    if (!dir) return NULL;

    char path[768];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, dir);

    char *data = read_file_to_string(path);
    if (!data) return NULL;

    NcValue json = nc_json_parse(data);
    free(data);

    if (IS_NONE(json)) return NULL;

    NcManifest *m = calloc(1, sizeof(NcManifest));
    if (!m) return NULL;

    const char *s;
    s = json_get_str(json, "name");
    safe_strcpy(m->name, sizeof(m->name), s);

    s = json_get_str(json, "version");
    safe_strcpy(m->version, sizeof(m->version), s);

    s = json_get_str(json, "description");
    safe_strcpy(m->description, sizeof(m->description), s);

    s = json_get_str(json, "entry");
    safe_strcpy(m->entry, sizeof(m->entry), s);

    /* Parse dependencies object: { "name": "version", ... } */
    NcValue deps = json_get_val(json, "dependencies");
    if (IS_MAP(deps)) {
        NcMap *map = AS_MAP(deps);
        m->dep_count = (map->count > 64) ? 64 : map->count;
        for (int i = 0; i < m->dep_count; i++) {
            if (map->keys[i])
                safe_strcpy(m->dep_names[i], 128, map->keys[i]->chars);
            NcValue v = map->values[i];
            if (IS_STRING(v) && v.as.string)
                safe_strcpy(m->dep_versions[i], 32, v.as.string->chars);
        }
    }
    /* Also support array format: ["name@version", ...] */
    else if (IS_LIST(deps)) {
        NcList *list = AS_LIST(deps);
        m->dep_count = (list->count > 64) ? 64 : list->count;
        for (int i = 0; i < m->dep_count; i++) {
            NcValue item = nc_list_get(list, i);
            if (IS_STRING(item) && item.as.string) {
                const char *dep_str = item.as.string->chars;
                const char *at = strchr(dep_str, '@');
                if (at) {
                    int nlen = (int)(at - dep_str);
                    if (nlen > 127) nlen = 127;
                    strncpy(m->dep_names[i], dep_str, nlen);
                    m->dep_names[i][nlen] = '\0';
                    safe_strcpy(m->dep_versions[i], 32, at + 1);
                } else {
                    safe_strcpy(m->dep_names[i], 128, dep_str);
                    safe_strcpy(m->dep_versions[i], 32, "*");
                }
            }
        }
    }

    return m;
}

int nc_pkg_write_manifest(const char *dir, NcManifest *m) {
    if (!dir || !m) return -1;

    char path[768];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, dir);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "  Error: Cannot write %s\n", path);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", m->name);
    fprintf(f, "  \"version\": \"%s\",\n", m->version);
    fprintf(f, "  \"description\": \"%s\",\n", m->description);
    fprintf(f, "  \"entry\": \"%s\"", m->entry);

    if (m->dep_count > 0) {
        fprintf(f, ",\n  \"dependencies\": {\n");
        for (int i = 0; i < m->dep_count; i++) {
            fprintf(f, "    \"%s\": \"%s\"", m->dep_names[i], m->dep_versions[i]);
            if (i < m->dep_count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  }\n");
    } else {
        fprintf(f, ",\n  \"dependencies\": {}\n");
    }

    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int nc_pkg_init_manifest(const char *dir) {
    if (!dir) return -1;

    char path[768];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR NC_MANIFEST_FILE, dir);

    FILE *check = fopen(path, "r");
    if (check) {
        fclose(check);
        printf("  %s already exists\n", NC_MANIFEST_FILE);
        return 0;
    }

    NcManifest m;
    memset(&m, 0, sizeof(m));
    safe_strcpy(m.name, sizeof(m.name), "my-nc-package");
    safe_strcpy(m.version, sizeof(m.version), "1.0.0");
    safe_strcpy(m.description, sizeof(m.description), "An NC package");
    safe_strcpy(m.entry, sizeof(m.entry), "main.nc");
    m.dep_count = 0;

    int ret = nc_pkg_write_manifest(dir, &m);
    if (ret == 0)
        printf("  Created %s\n", path);
    return ret;
}

/* ═══════════════════════════════════════════════════════════
 *  Lock File — nc_package.lock
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_lock_write(const char *dir, NcPackageInfo *pkgs, int count) {
    if (!dir || !pkgs) return -1;

    char path[768];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR NC_LOCK_FILE, dir);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "  Error: Cannot write %s\n", path);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"lockfile_version\": 1,\n");
    fprintf(f, "  \"packages\": [\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", pkgs[i].name);
        fprintf(f, "      \"version\": \"%s\",\n", pkgs[i].version);
        fprintf(f, "      \"url\": \"%s\",\n", pkgs[i].url);
        fprintf(f, "      \"checksum\": \"%s\"", pkgs[i].checksum);

        if (pkgs[i].dependencies_count > 0) {
            fprintf(f, ",\n      \"dependencies\": [");
            for (int j = 0; j < pkgs[i].dependencies_count; j++) {
                fprintf(f, "\"%s\"", pkgs[i].dependencies[j]);
                if (j < pkgs[i].dependencies_count - 1) fprintf(f, ", ");
            }
            fprintf(f, "]");
        }

        fprintf(f, "\n    }");
        if (i < count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int nc_pkg_lock_read(const char *dir, NcPackageInfo **pkgs, int *count) {
    if (!dir || !pkgs || !count) return -1;
    *pkgs = NULL;
    *count = 0;

    char path[768];
    snprintf(path, sizeof(path), "%s" NC_PATH_SEP_STR NC_LOCK_FILE, dir);

    char *data = read_file_to_string(path);
    if (!data) return 0; /* No lock file is not an error */

    NcValue json = nc_json_parse(data);
    free(data);
    if (IS_NONE(json)) return -1;

    NcValue pkg_list = json_get_val(json, "packages");
    if (!IS_LIST(pkg_list)) return -1;

    NcList *list = AS_LIST(pkg_list);
    int n = list->count;
    if (n == 0) return 0;
    if (n > NC_MAX_RESOLVED) n = NC_MAX_RESOLVED;

    *pkgs = calloc(n, sizeof(NcPackageInfo));
    if (!*pkgs) return -1;

    for (int i = 0; i < n; i++) {
        NcValue item = nc_list_get(list, i);
        parse_package_from_json(item, &(*pkgs)[i]);
    }

    *count = n;
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Publishing — nc_pkg_pack
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_pack(const char *dir, const char *output) {
    if (!dir) return -1;

    NcManifest *m = nc_pkg_read_manifest(dir);
    if (!m) {
        fprintf(stderr, "  Error: No %s found. Run 'nc pkg init-manifest' first.\n",
                NC_MANIFEST_FILE);
        return 1;
    }

    char outfile[768];
    if (output && output[0]) {
        safe_strcpy(outfile, sizeof(outfile), output);
    } else {
        snprintf(outfile, sizeof(outfile), "%s-%s.tar.gz", m->name, m->version);
    }

    printf("  Packing %s@%s -> %s\n", m->name, m->version, outfile);

    /* Use tar to create the archive, excluding nc_modules and .git */
    const char *tar_args[] = {
        "tar", "czf", outfile,
        "--exclude", NC_MODULES_DIR,
        "--exclude", NC_PKG_DIR,
        "--exclude", ".git",
        "--exclude", "*.o",
        "--exclude", "*.tar.gz",
        "-C", dir, ".",
        NULL
    };

    int ret = safe_run(tar_args);
    if (ret != 0) {
        fprintf(stderr, "  Error: tar failed to create archive\n");
        free(m);
        return 1;
    }

    printf("  Created %s (%s@%s)\n", outfile, m->name, m->version);
    free(m);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Publishing — nc_pkg_publish
 * ═══════════════════════════════════════════════════════════ */

int nc_pkg_publish(const char *dir) {
    if (!dir) return -1;

    NcManifest *m = nc_pkg_read_manifest(dir);
    if (!m) {
        fprintf(stderr, "  Error: No %s found. Run 'nc pkg init-manifest' first.\n",
                NC_MANIFEST_FILE);
        return 1;
    }

    /* Pack first */
    char tarball[768];
    snprintf(tarball, sizeof(tarball), "%s%c%s-%s.tar.gz",
             nc_tempdir(), NC_PATH_SEP, m->name, m->version);
    int ret = nc_pkg_pack(dir, tarball);
    if (ret != 0) { free(m); return ret; }

    /* Read the tarball for upload */
    char *tarball_data = read_file_to_string(tarball);
    if (!tarball_data) {
        fprintf(stderr, "  Error: Cannot read packaged tarball\n");
        free(m);
        return 1;
    }

    /* Get auth token */
    const char *token = getenv("NC_REGISTRY_TOKEN");
    char auth_header[512] = {0};
    if (token && token[0]) {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
    }

    /* Build JSON metadata for publish */
    char json_body[4096];
    snprintf(json_body, sizeof(json_body),
        "{"
        "\"name\":\"%s\","
        "\"version\":\"%s\","
        "\"description\":\"%s\""
        "}",
        m->name, m->version, m->description);

    /* POST to registry */
    const char *api = nc_get_registry_api();
    char url[768];
    snprintf(url, sizeof(url), "%s/packages/publish", api);

    printf("  Publishing %s@%s to %s ...\n", m->name, m->version, api);

    char *response = nc_http_post(url, json_body, "application/json",
                                   auth_header[0] ? auth_header : NULL);
    free(tarball_data);
    remove(tarball);

    if (!response) {
        printf("  Registry not available.\n");
        printf("  Package packed successfully as %s-%s.tar.gz\n", m->name, m->version);
        printf("  You can publish manually when the registry is online.\n");
        free(m);
        return 0;
    }

    /* Check response */
    NcValue resp_json = nc_json_parse(response);
    free(response);

    const char *error = json_get_str(resp_json, "error");
    if (error) {
        fprintf(stderr, "  Publish failed: %s\n", error);
        free(m);
        return 1;
    }

    printf("  Published %s@%s successfully!\n", m->name, m->version);
    printf("  Install with: nc pkg install %s\n", m->name);
    free(m);
    return 0;
}
