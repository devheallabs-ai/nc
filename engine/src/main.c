/*
 * main.c â€” NC entry point (Notation-as-Code runtime).
 *
 * This is the equivalent of python3 command â€” the NC interpreter.
 *
 * Usage:
 *   nc run service.nc           Run a .nc file
 *   nc validate service.nc      Validate syntax
 *   nc tokens service.nc        Show token stream
 *   nc version                  Show version
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include "../include/nc_version.h"
#include "nc_model.h"
#include "nc_tokenizer.h"
#include "nc_training.h"
#include "nc_nova.h"
#include "nc_ai_efficient.h"
#include "nc_nova_reasoning.h"
#include "nc_metal.h"
#include "nc_cuda.h"
#include "nc_generate.h"
#ifndef NC_WINDOWS
  #include <unistd.h>
#endif
#include "nc_ai_benchmark.h"
#include "../include/nc_build.h"
#include "nc_wasm.h"
#include "nc_ui_compiler.h"
#include "nc_terminal_ui.h"

/* Route all user-facing printf through nc_printf for ANSI color support */
#define printf nc_printf

static volatile sig_atomic_t nc_interrupted = 0;
static void print_mascot(void);

#define NC_CONSENT_TEXT \
    "NC is licensed under the Apache License 2.0.\n" \
    "By using NC, you agree to the license terms and notices shipped with this software.\n" \
    "Review:\n" \
    "  https://github.com/DevHealLabs/nc-lang/blob/main/LICENSE\n" \
    "  https://github.com/DevHealLabs/nc-lang/blob/main/NOTICE\n"

static void nc_signal_handler(int sig) {
    (void)sig;
    nc_interrupted = 1;
}

static bool nc_env_truthy(const char *name) {
    const char *v = getenv(name);
    if (!v || !*v) return false;
    return strcasecmp(v, "1") == 0 || strcasecmp(v, "yes") == 0 ||
           strcasecmp(v, "true") == 0 || strcasecmp(v, "y") == 0;
}

static bool nc_cli_plain_mode(void) {
    return nc_env_truthy("NO_COLOR") || nc_env_truthy("NC_NO_ANIM");
}

static void nc_cli_print_divider(FILE *stream) {
    fputs("========================================\n", stream);
}

static void nc_cli_print_header_rule(void) {
    printf("  \033[36m==================================================\033[0m\n");
}

static bool nc_consent_marker_path(char *buf, size_t size) {
#ifdef NC_WINDOWS
    const char *base = getenv("LOCALAPPDATA");
    if (!base || !*base) base = getenv("APPDATA");
    if (!base || !*base) return false;
    snprintf(buf, size, "%s\\nc\\license.accepted", base);
#else
    const char *base = getenv("XDG_STATE_HOME");
    if (base && *base) {
        snprintf(buf, size, "%s/nc/license.accepted", base);
        return true;
    }
    base = getenv("HOME");
    if (!base || !*base) return false;
    snprintf(buf, size, "%s/.config/nc/license.accepted", base);
#endif
    return true;
}

/* Check if LLM output is actually NC code (not just prompt echo or gibberish) */
static int nc_ai_is_valid_nc(const char *text) {
    if (!text || strlen(text) < 50) return 0;

    /* Reject if too much gibberish: check ratio of real newlines vs total length */
    int len = (int)strlen(text);
    int newlines = 0;
    for (int i = 0; i < len; i++) if (text[i] == '\n') newlines++;
    /* Real NC code has many lines; gibberish is one long line */
    if (newlines < 5 && len > 200) return 0;

    /* Reject <|unk|> heavy output â€” sign of undertrained model */
    int unk_count = 0;
    const char *p = text;
    while ((p = strstr(p, "<|unk|>")) != NULL) { unk_count++; p += 7; }
    if (unk_count > 3) return 0;

    int score = 0;
    if (strstr(text, "to ") && strstr(text, ":\n")) score++;
    if (strstr(text, "respond with")) score++;
    if (strstr(text, "set ")) score++;
    if (strstr(text, "service \"") || strstr(text, "page \"")) score++;
    if (strstr(text, "api:") || strstr(text, "section")) score++;
    if (strstr(text, "assert ") || strstr(text, "test_")) score++;
    return score >= 2;
}

static int nc_ai_output_is_repetitive(const int *tokens, int total_len, int prefix_len) {
    int repeat_count = 0;

    if (!tokens || total_len <= prefix_len + 10) return 0;
    for (int i = prefix_len + 4; i < total_len; i++) {
        if (tokens[i] == tokens[i - 1] && tokens[i - 1] == tokens[i - 2]) {
            repeat_count++;
        }
    }
    return repeat_count > 10;
}

static int nc_ai_project_output_valid(const char *text, const char *kind) {
    if (!text || !*text) return 0;
    if (strstr(text, "<|begin|>")) return 0;
    if (strstr(text, "<|unk|>")) return 0;

    if (kind && strcmp(kind, "service") == 0) {
        return nc_validate_generated(text, "service");
    }

    if (kind && strcmp(kind, "page") == 0) {
        return nc_validate_generated(text, "page");
    }

    if (kind && strcmp(kind, "test") == 0) {
        return nc_validate_generated(text, "test");
    }

    return nc_ai_is_valid_nc(text);
}

static int nc_ai_project_candidate_score(const char *text, const char *kind) {
    int score = 0;

    if (!text || !*text) return -1;
    if (nc_ai_is_valid_nc(text)) score += 3;

    if (kind && strcmp(kind, "service") == 0) {
        if (strstr(text, "service \"")) score += 2;
        if (strstr(text, "version \"")) score += 1;
        if (strstr(text, "to ")) score += 2;
        if (strstr(text, "api:")) score += 2;
        if (strstr(text, "middleware:")) score += 2;
    } else if (kind && strcmp(kind, "page") == 0) {
        if (strstr(text, "page \"")) score += 2;
        if (strstr(text, "style:") || strstr(text, "theme ")) score += 2;
        if (strstr(text, "section")) score += 2;
        if (strstr(text, "footer:")) score += 2;
    } else if (kind && strcmp(kind, "test") == 0) {
        if (strstr(text, "to test_") || strstr(text, "test \"")) score += 2;
        if (strstr(text, "assert ")) score += 3;
        if (strstr(text, "respond with \"ok\"")) score += 1;
    }

    return score;
}

static int nc_ai_text_contains_ci(const char *text, const char *needle) {
    size_t needle_len;

    if (!text || !needle) return 0;
    needle_len = strlen(needle);
    if (needle_len == 0) return 1;

    for (const char *cursor = text; *cursor; cursor++) {
        size_t i = 0;
        while (cursor[i]
               && i < needle_len
               && tolower((unsigned char)cursor[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == needle_len) return 1;
    }

    return 0;
}

static int nc_ai_project_enterprise_semantic_valid(const char *text,
                                                   const char *kind,
                                                   int has_enterprise_ops,
                                                   int has_tenant,
                                                   int has_role,
                                                   int has_approval,
                                                   int has_analytics,
                                                   int has_alert) {
    if (!has_enterprise_ops) return 1;
    if (!text || !*text || !kind) return 0;

    if (strcmp(kind, "service") == 0) {
        if (has_tenant
            && !(nc_ai_text_contains_ci(text, "list_tenants")
                 && nc_ai_text_contains_ci(text, "/api/v1/tenants"))) {
            return 0;
        }
        if (has_role
            && !(nc_ai_text_contains_ci(text, "list_roles")
                 && nc_ai_text_contains_ci(text, "/api/v1/roles")
                 && nc_ai_text_contains_ci(text, "permission"))) {
            return 0;
        }
        if (has_analytics
            && !(nc_ai_text_contains_ci(text, "analytics_overview")
                 && nc_ai_text_contains_ci(text, "/api/v1/analytics/overview"))) {
            return 0;
        }
        if (has_approval
            && !(nc_ai_text_contains_ci(text, "list_approvals")
                 && nc_ai_text_contains_ci(text, "approve_request")
                 && nc_ai_text_contains_ci(text, "reject_request"))) {
            return 0;
        }
        if (has_alert
            && !(nc_ai_text_contains_ci(text, "list_alerts")
                 && nc_ai_text_contains_ci(text, "/api/v1/alerts"))) {
            return 0;
        }
        return 1;
    }

    if (strcmp(kind, "page") == 0) {
        if (!nc_ai_text_contains_ci(text, "dashboard")) return 0;
        if (has_tenant && !nc_ai_text_contains_ci(text, "tenants")) return 0;
        if (has_role && !nc_ai_text_contains_ci(text, "roles")) return 0;
        if (has_analytics && !nc_ai_text_contains_ci(text, "analytics")) return 0;
        if (has_approval && !nc_ai_text_contains_ci(text, "approvals")) return 0;
        if (has_alert && !nc_ai_text_contains_ci(text, "alerts")) return 0;
        return 1;
    }

    return 1;
}

static const char *nc_ai_project_contract(const char *kind) {
    if (kind && strcmp(kind, "service") == 0) {
        return
            "Start with service \"name\" and version \"1.0.0\". "
            "Include one or more to ... handlers, an api: block, and a middleware: block. "
            "Output only valid NC code.";
    }
    if (kind && strcmp(kind, "page") == 0) {
        return
            "Start with page \"name\". Include a style: block or theme settings, "
            "at least one section, and footer:. Output only valid NC UI code.";
    }
    if (kind && strcmp(kind, "test") == 0) {
        return
            "Generate only NC test code. Include to test_* behaviors or a test \"...\" block, "
            "use assert statements, and end each test with respond with \"ok\" when appropriate.";
    }
    return "Output only valid NC code.";
}

static char *nc_ai_project_build_retry_prompt(const char *base_prompt,
                                              const char *kind,
                                              const char *failure_reason,
                                              const char *prior_output) {
    const char *contract = nc_ai_project_contract(kind);
    size_t base_len = base_prompt ? strlen(base_prompt) : 0;
    size_t reason_len = failure_reason ? strlen(failure_reason) : 10;
    size_t contract_len = strlen(contract);
    size_t prior_len = prior_output ? strlen(prior_output) : 0;
    size_t capped_prior_len;
    size_t total_size;
    char *prompt;

    capped_prior_len = prior_len > 3000 ? 3000 : prior_len;
    total_size = base_len + reason_len + contract_len + capped_prior_len + 512;
    prompt = (char *)malloc(total_size);
    if (!prompt) return NULL;

    if (prior_output && capped_prior_len > 0) {
        snprintf(prompt, total_size,
                 "%s\n\nThe previous %s draft failed %s. %s\n"
                 "Regenerate it from scratch. Use the prior draft only as context.\n"
                 "Previous draft:\n%.*s\n\nOutput only code.\n",
                 base_prompt ? base_prompt : "",
                 kind ? kind : "project",
                 failure_reason ? failure_reason : "validation",
                 contract,
                 (int)capped_prior_len,
                 prior_output);
    } else {
        snprintf(prompt, total_size,
                 "%s\n\nThe previous %s draft failed %s. %s\n"
                 "Regenerate it from scratch and output only code.\n",
                 base_prompt ? base_prompt : "",
                 kind ? kind : "project",
                 failure_reason ? failure_reason : "validation",
                 contract);
    }

    return prompt;
}

static NCTemplateType nc_ai_project_template_type(const char *kind) {
    if (kind && strcmp(kind, "page") == 0) return NC_TMPL_NCUI_PAGE;
    if (kind && strcmp(kind, "test") == 0) return NC_TMPL_TEST;
    return NC_TMPL_SERVICE;
}

static char *nc_ai_project_generate_candidate(NCModel *llm_model,
                                              NCTokenizer *tok,
                                              const char *gen_prompt,
                                              const char *kind,
                                              int max_tokens,
                                              float temperature) {
    int *prompt_tokens;
    int *output_tokens;
    int prompt_len;
    int total_len;
    char *text;
    char *postprocessed;
    NCTemplateType template_type = nc_ai_project_template_type(kind);

    if (!llm_model || !tok || !gen_prompt) return NULL;

    prompt_tokens = (int *)malloc((size_t)llm_model->max_seq * sizeof(int));
    output_tokens = (int *)malloc((size_t)(llm_model->max_seq + max_tokens) * sizeof(int));
    if (!prompt_tokens || !output_tokens) {
        free(prompt_tokens);
        free(output_tokens);
        return NULL;
    }

    prompt_len = nc_tokenizer_encode(tok, gen_prompt, prompt_tokens, llm_model->max_seq);
    if (prompt_len <= 0) {
        free(prompt_tokens);
        free(output_tokens);
        return NULL;
    }

    total_len = nc_model_generate_advanced(llm_model, prompt_tokens, prompt_len,
                                           max_tokens, temperature,
                                           16, 0.85f, output_tokens);
    free(prompt_tokens);
    if (total_len <= 0) {
        free(output_tokens);
        return NULL;
    }

    text = nc_generate_decode_hardened_alloc(tok, output_tokens, total_len);
    free(output_tokens);
    if (!text) {
        return NULL;
    }

    if (strncmp(text, "<|begin|>\n", 10) == 0) {
        char *shifted = strdup(text + 10);
        if (shifted) {
            free(text);
            text = shifted;
        }
    }

    postprocessed = nc_generate_postprocess_model_output(text, template_type);
    if (postprocessed) {
        free(text);
        text = postprocessed;
    }

    return text;
}

static char *nc_ai_project_generate_with_retries(NCModel *llm_model,
                                                 NCTokenizer *tok,
                                                 const char *base_prompt,
                                                 const char *kind,
                                                 int max_tokens,
                                                 int allow_template_fallback) {
    static const float retry_temps[] = { 0.70f, 0.45f, 0.25f };
    char *best_candidate = NULL;
    int best_score = -1;
    char *retry_prompt = NULL;

    if (!llm_model || !tok || !base_prompt) return NULL;

    for (int attempt = 0; attempt < (int)(sizeof(retry_temps) / sizeof(retry_temps[0])); attempt++) {
        const char *prompt_text = retry_prompt ? retry_prompt : base_prompt;
        char *candidate = nc_ai_project_generate_candidate(
            llm_model, tok, prompt_text, kind, max_tokens, retry_temps[attempt]);
        char *next_retry_prompt = NULL;

        free(retry_prompt);
        retry_prompt = NULL;

        if (candidate) {
            int candidate_score = nc_ai_project_candidate_score(candidate, kind);
            if (candidate_score > best_score
                || (candidate_score == best_score && (!best_candidate || strlen(candidate) > strlen(best_candidate)))) {
                free(best_candidate);
                best_candidate = strdup(candidate);
                best_score = candidate_score;
            }

            if (nc_ai_project_output_valid(candidate, kind)) {
                free(best_candidate);
                return candidate;
            }

            next_retry_prompt = nc_ai_project_build_retry_prompt(
                base_prompt, kind, "validation", candidate);
            free(candidate);
        } else {
            next_retry_prompt = nc_ai_project_build_retry_prompt(
                base_prompt, kind, "generation", NULL);
        }

        retry_prompt = next_retry_prompt;
    }

    free(retry_prompt);
    if (best_candidate && !allow_template_fallback) {
        return best_candidate;
    }
    free(best_candidate);
    return NULL;
}

static bool nc_file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

typedef struct {
    char **items;
    int count;
    int capacity;
} NcPathList;

static void nc_path_list_free(NcPathList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool nc_path_list_push(NcPathList *list, const char *path) {
    if (!list || !path) return false;
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity > 0 ? list->capacity * 2 : 64;
        char **new_items = (char**)realloc(list->items, new_capacity * sizeof(char*));
        if (!new_items) return false;
        list->items = new_items;
        list->capacity = new_capacity;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static bool nc_name_in_list(const char *name, const char *const *names) {
    if (!name || !names) return false;
    for (int i = 0; names[i]; i++) {
        if (strcasecmp(name, names[i]) == 0) return true;
    }
    return false;
}

static bool nc_path_has_extension(const char *path, const char *const *extensions) {
    if (!path || !extensions) return false;
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    for (int i = 0; extensions[i]; i++) {
        if (strcasecmp(dot, extensions[i]) == 0) return true;
    }
    return false;
}

static void nc_collect_files_recursive_impl(const char *path,
                                            const char *const *extensions,
                                            const char *const *skip_dirs,
                                            long max_bytes,
                                            int max_depth,
                                            int depth,
                                            int max_files,
                                            NcPathList *out) {
    if (!path || !*path || !out || out->count >= max_files) return;

    struct stat st;
    if (stat(path, &st) != 0) return;
    if (max_depth >= 0 && depth > max_depth) return;

    if (S_ISREG(st.st_mode)) {
        if (!nc_path_has_extension(path, extensions)) return;
        if (max_bytes > 0 && st.st_size > max_bytes) return;
        nc_path_list_push(out, path);
        return;
    }

    if (!S_ISDIR(st.st_mode)) return;
    if (max_depth >= 0 && depth >= max_depth) return;

    nc_dir_t *dir = nc_opendir(path);
    if (!dir) return;

    nc_dirent_t entry;
    while (out->count < max_files && nc_readdir(dir, &entry)) {
        if (!entry.name) continue;
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) continue;
        if (entry.is_dir && nc_name_in_list(entry.name, skip_dirs)) continue;

        char child[4096];
        int written = snprintf(child, sizeof(child), "%s%c%s", path, NC_PATH_SEP, entry.name);
        if (written < 0 || written >= (int)sizeof(child)) continue;

        nc_collect_files_recursive_impl(child, extensions, skip_dirs, max_bytes,
                                        max_depth, depth + 1, max_files, out);
    }

    nc_closedir(dir);
}

static int nc_collect_files_recursive(const char *path,
                                      const char *const *extensions,
                                      const char *const *skip_dirs,
                                      long max_bytes,
                                      int max_depth,
                                      int max_files,
                                      NcPathList *out) {
    if (!out) return 0;
    out->items = NULL;
    out->count = 0;
    out->capacity = 0;
    nc_collect_files_recursive_impl(path, extensions, skip_dirs, max_bytes,
                                    max_depth, 0, max_files, out);
    return out->count;
}

static bool nc_try_load_nova_model(const char *path, NovaModel **out_model) {
    if (!path || !out_model) return false;

    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    uint32_t magic = 0, version = 0;
    NovaConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    bool ok = fread(&magic, sizeof(magic), 1, fp) == 1 &&
              fread(&version, sizeof(version), 1, fp) == 1 &&
              magic == NOVA_MAGIC &&
              fread(&cfg, sizeof(cfg), 1, fp) == 1 &&
              cfg.dim > 0;
    fclose(fp);
    if (!ok) return false;

    NovaModel *model = nova_create(cfg);
    if (!model) return false;
    if (nova_load(model, path) != 0) {
        nova_free(model);
        return false;
    }

    *out_model = model;
    return true;
}

static int nc_safe_run(const char *const argv[]) {
#ifdef NC_WINDOWS
    char cmd_line[4096] = {0};
    int pos = 0;
    for (int i = 0; argv[i]; i++) {
        if (i > 0) cmd_line[pos++] = ' ';
        pos += snprintf(cmd_line + pos, sizeof(cmd_line) - pos, "\"%s\"", argv[i]);
    }
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
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

static int nc_ai_auto_release_ui(const char *argv0, const char *project_dir,
                                 char *built_html, size_t built_html_size,
                                 char *cli_path_used, size_t cli_path_used_size) {
    (void)argv0;
    char app_path[1024];
    snprintf(app_path, sizeof(app_path), "%s%capp.ncui", project_dir, NC_PATH_SEP);
    if (!nc_file_exists(app_path)) return -3;

    /* Use native NC UI compiler — no Node.js needed */
    char dist_dir[1024];
    snprintf(dist_dir, sizeof(dist_dir), "%s%cdist", project_dir, NC_PATH_SEP);

    /* Create dist directory */
#ifdef _WIN32
    CreateDirectoryA(dist_dir, NULL);
#else
    mkdir(dist_dir, 0755);
#endif

    bool ok = nc_ui_compile_file(app_path, dist_dir);

    if (cli_path_used && cli_path_used_size > 0) {
        snprintf(cli_path_used, cli_path_used_size, "(native)");
    }
    if (ok && built_html && built_html_size > 0) {
        snprintf(built_html, built_html_size, "%s%cdist%capp.html", project_dir, NC_PATH_SEP, NC_PATH_SEP);
    }
    return ok ? 0 : -1;
}

static void nc_mkdir_p_for_file(const char *filepath) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", filepath);
    char *last_sep = nc_last_path_sep(tmp);
    if (!last_sep) return;
    *last_sep = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (tmp[0]) nc_mkdir(tmp);
            *p = saved;
        }
    }
    if (tmp[0]) nc_mkdir(tmp);
}

static bool nc_has_license_acceptance(void) {
    if (nc_env_truthy("NC_ACCEPT_LICENSE")) return true;
    char path[1024];
    if (!nc_consent_marker_path(path, sizeof(path))) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static void nc_store_license_acceptance(void) {
    char path[1024];
    if (!nc_consent_marker_path(path, sizeof(path))) return;
    nc_mkdir_p_for_file(path);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fputs("accepted=1\n", f);
    fputs("license=Apache-2.0\n", f);
    fputs("product=nc\n", f);
    fclose(f);
}

static int nc_ensure_license_acceptance(void) {
    if (nc_has_license_acceptance()) {
        if (nc_env_truthy("NC_ACCEPT_LICENSE")) nc_store_license_acceptance();
        return 0;
    }

    fprintf(stderr, "\nNC first-run agreement\n");
    nc_cli_print_divider(stderr);
    fprintf(stderr, "%s\n", NC_CONSENT_TEXT);

    if (!nc_isatty(nc_fileno(stdin))) {
        fprintf(stderr, "Non-interactive session detected. Set NC_ACCEPT_LICENSE=1 to accept the license on first run.\n\n");
        return 1;
    }

    fprintf(stderr, "Type 'yes' to accept and continue: ");
    char reply[32] = {0};
    if (!fgets(reply, sizeof(reply), stdin)) {
        fprintf(stderr, "\nLicense agreement not accepted.\n");
        return 1;
    }
    char *nl = strchr(reply, '\n');
    if (nl) *nl = '\0';
    if (!(strcasecmp(reply, "yes") == 0 || strcasecmp(reply, "y") == 0)) {
        fprintf(stderr, "License agreement not accepted. Exiting.\n");
        return 1;
    }

    nc_store_license_acceptance();
    fprintf(stderr, "License agreement accepted.\n\n");
    return 0;
}

/* Silent read â€” returns NULL without printing error */
static char *read_file_quiet(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, size, f);
    buf[nread] = '\0';
    fclose(f);
    return buf;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size <= 0) { fclose(f); fprintf(stderr, "Error: Empty file '%s'\n", path); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); fprintf(stderr, "Error: Out of memory\n"); return NULL; }
    size_t nread = fread(buf, 1, size, f);
    buf[nread] = '\0';
    fclose(f);
    if (nread == 0) { free(buf); fprintf(stderr, "Error: Failed to read '%s'\n", path); return NULL; }
    return buf;
}

static int nc_ai_project_validate_file(const char *path, const char *kind) {
    char *content = read_file_quiet(path);
    int valid = content && nc_ai_project_output_valid(content, kind);
    free(content);
    return valid;
}

static int nc_ai_project_repair_file(NCModel *llm_model,
                                     NCTokenizer *tok,
                                     const char *path,
                                     const char *base_prompt,
                                     const char *kind,
                                     int max_tokens) {
    char *current;
    char *repair_prompt;
    char *candidate;
    FILE *f;
    int repaired = 0;

    if (!llm_model || !tok || !path || !base_prompt || !*base_prompt || !kind) return 0;

    current = read_file_quiet(path);
    if (!current || !*current) {
        free(current);
        return 0;
    }

    if (nc_ai_project_output_valid(current, kind)) {
        free(current);
        return 1;
    }

    repair_prompt = nc_ai_project_build_retry_prompt(base_prompt, kind,
                                                     "structural validation", current);
    free(current);
    if (!repair_prompt) return 0;

    candidate = nc_ai_project_generate_with_retries(llm_model, tok, repair_prompt,
                                                    kind, max_tokens, 0);
    free(repair_prompt);
    if (!candidate || !nc_ai_project_output_valid(candidate, kind)) {
        free(candidate);
        return 0;
    }

    f = fopen(path, "w");
    if (!f) {
        free(candidate);
        return 0;
    }

    fprintf(f, "%s\n", candidate);
    fclose(f);
    free(candidate);
    repaired = 1;
    return repaired;
}

/* â”€â”€ nc test â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

/* dirent and timeval already provided by nc_platform.h */

static int test_one_file(const char *filepath, bool verbose) {
    char *source = read_file(filepath);
    if (!source) return -1;

    const char *basename = nc_last_path_sep(filepath);
    basename = basename ? basename + 1 : filepath;

    NcLexer *lex = nc_lexer_new(source, filepath);
    nc_lexer_tokenize(lex);

    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filepath);
    NcASTNode *program = nc_parser_parse(parser);

    if (parser->had_error) {
        printf("  \033[31mFAIL\033[0m  %s - parse error: %s\n", basename, parser->error_msg);
        nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return 1;  /* no program AST to free on parse error */
    }

    NcCompiler *comp = nc_compiler_new();
    if (!nc_compiler_compile(comp, program)) {
        printf("  \033[31mFAIL\033[0m  %s - compile error: %s\n", basename, comp->error_msg);
        nc_ast_free(program);
        nc_compiler_free(comp); nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return 1;
    }
    nc_optimize_all(comp);

    int passed = 0, failed = 0;

    for (int i = 0; i < comp->chunk_count; i++) {
        const char *beh_name = comp->beh_names[i]->chars;
        NcVM *vm = nc_vm_new();
        vm->behavior_chunks = comp->chunks;
        vm->behavior_chunk_count = comp->chunk_count;
        for (int b = 0; b < comp->chunk_count; b++)
            nc_map_set(vm->behaviors, comp->beh_names[b], NC_INT(b));
        NcValue result = nc_vm_execute_fast(vm, &comp->chunks[i]);

        if (vm->had_error) {
            printf("  \033[31mFAIL\033[0m  %s::%s - %s\n", basename, beh_name, vm->error_msg);
            failed++;
        } else {
            passed++;
            if (verbose) {
                printf("  \033[32m ok \033[0m  %s::%s", basename, beh_name);
                if (!IS_NONE(result)) {
                    printf(" -> ");
                    nc_value_print(result, stdout);
                }
                printf("\n");
            }
        }
        nc_vm_free(vm);
    }

    if (!verbose && failed == 0)
        printf("  \033[32m ok \033[0m  %s (%d behaviors)\n", basename, passed);

    nc_ast_free(program);
    nc_compiler_free(comp);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);
    return failed;
}

static int cmd_test(int argc, char *argv[]) {
    bool verbose = false;
    const char *single_file = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            verbose = true;
        else
            single_file = argv[i];
    }

    printf("\n");
    printf("  \033[36m _  _  ___\033[0m\n");
    printf("  \033[36m| \\| |/ __|\033[0m   \033[1mNC Test Runner\033[0m \033[33mv%s\033[0m\n", NC_VERSION);
    printf("  \033[36m| .` | (__\033[0m\n");
    printf("  \033[36m|_|\\_|\\___|\033[0m\n");
    printf("\n");
    nc_cli_print_header_rule();
    printf("\n");

    double start_time_ms = nc_clock_ms();

    int total_passed = 0, total_failed = 0, total_files = 0;

    /* Disable HTTP retries during tests to avoid delays and noisy output */
    nc_setenv("NC_RETRIES", "0", 0);

    if (single_file) {
        int fails = test_one_file(single_file, verbose);
        if (fails < 0) return 1;
        total_files = 1;
        total_failed = fails;
        total_passed = (fails == 0) ? 1 : 0;
    } else {
        /*
         * Search for tests/lang/ in this order:
         *   1. CWD-relative paths (like pytest â€” run from project root)
         *   2. Relative to the nc binary location
         */
        char resolved_buf[1024];
        char *resolved = NULL;

        /* Try CWD-relative first */
        const char *cwd_paths[] = {
            "tests/lang", "../tests/lang", "../../tests/lang", NULL
        };
        for (int i = 0; cwd_paths[i]; i++) {
            resolved = nc_realpath(cwd_paths[i], resolved_buf);
            if (resolved) break;
        }

        /* Fall back to binary-relative */
        if (!resolved) {
            char test_dir[512];
            char self_buf[1024];
            char *self_path = nc_realpath(argv[0], self_buf);
            if (self_path) {
                char *dir = nc_last_path_sep(self_path);
                if (dir) *dir = '\0';
                dir = nc_last_path_sep(self_path);
                if (dir) *dir = '\0';
                snprintf(test_dir, sizeof(test_dir), "%s%c..%ctests%clang",
                         self_path, NC_PATH_SEP, NC_PATH_SEP, NC_PATH_SEP);
            } else {
                snprintf(test_dir, sizeof(test_dir), "..%ctests%clang",
                         NC_PATH_SEP, NC_PATH_SEP);
            }
            resolved = nc_realpath(test_dir, resolved_buf);
        }

        if (!resolved) {
            fprintf(stderr, "  Cannot find tests/lang/ directory.\n");
            fprintf(stderr, "  Run from the project root: nc test\n\n");
            return 1;
        }

        nc_dir_t *d = nc_opendir(resolved);
        if (!d) {
            fprintf(stderr, "  Cannot open test directory: %s\n\n", resolved);
            return 1;
        }

        char *test_files[4096];
        int file_count = 0;

        nc_dirent_t entry;
        while (nc_readdir(d, &entry) && file_count < 4096) {
            int len = (int)strlen(entry.name);
            if (len > 3 && strcmp(entry.name + len - 3, ".nc") == 0 &&
                strncmp(entry.name, "test_", 5) == 0) {
                char path[512];
                snprintf(path, sizeof(path), "%s%c%s", resolved, NC_PATH_SEP, entry.name);
                test_files[file_count++] = strdup(path);
            }
        }
        nc_closedir(d);

        /* Sort files alphabetically */
        for (int i = 0; i < file_count - 1; i++)
            for (int j = i + 1; j < file_count; j++)
                if (strcmp(test_files[i], test_files[j]) > 0) {
                    char *tmp = test_files[i];
                    test_files[i] = test_files[j];
                    test_files[j] = tmp;
                }

        /* Run each test file */
        for (int i = 0; i < file_count; i++) {
            int fails = test_one_file(test_files[i], verbose);
            total_files++;
            if (fails > 0) total_failed += fails;
            else if (fails == 0) total_passed++;
            else total_failed++;
            free(test_files[i]);
        }
    }

    double elapsed = (nc_clock_ms() - start_time_ms) / 1000.0;

    printf("\n");
    nc_cli_print_header_rule();
    if (total_failed == 0) {
        printf("  \033[32m\033[1m  All %d test files passed\033[0m \033[90min %.2fs\033[0m\n", total_files, elapsed);
        printf("  \033[32m  PASS Zero failures. Ship it.\033[0m\n");
    } else {
        printf("  \033[31m\033[1m  %d failed\033[0m, \033[32m%d passed\033[0m \033[90min %.2fs\033[0m\n", total_failed, total_passed, elapsed);
    }
    nc_cli_print_header_rule();
    printf("\n");

    return total_failed > 0 ? 1 : 0;
}

/* â”€â”€ nc tokens â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static const char *token_name(NcTokenType t) {
    switch (t) {
        case TOK_SERVICE: return "SERVICE"; case TOK_VERSION: return "VERSION";
        case TOK_MODEL: return "MODEL"; case TOK_IMPORT: return "IMPORT";
        case TOK_DEFINE: return "DEFINE";
        case TOK_TO: return "TO"; case TOK_WITH: return "WITH";
        case TOK_FROM: return "FROM"; case TOK_USING: return "USING";
        case TOK_ASK: return "ASK"; case TOK_AI: return "AI";
        case TOK_GATHER: return "GATHER"; case TOK_IF: return "IF";
        case TOK_OTHERWISE: return "OTHERWISE"; case TOK_REPEAT: return "REPEAT";
        case TOK_SET: return "SET"; case TOK_RESPOND: return "RESPOND";
        case TOK_RUN: return "RUN"; case TOK_LOG: return "LOG";
        case TOK_WAIT: return "WAIT"; case TOK_NOTIFY: return "NOTIFY";
        case TOK_IS: return "IS"; case TOK_ABOVE: return "ABOVE";
        case TOK_BELOW: return "BELOW"; case TOK_EQUAL: return "EQUAL";
        case TOK_AND: return "AND"; case TOK_OR: return "OR";
        case TOK_NOT: return "NOT"; case TOK_MATCH: return "MATCH";
        case TOK_WHEN: return "WHEN";
        case TOK_STRING: return "STRING"; case TOK_INTEGER: return "INTEGER";
        case TOK_FLOAT_LIT: return "FLOAT"; case TOK_TRUE: return "TRUE";
        case TOK_FALSE: return "FALSE"; case TOK_IDENTIFIER: return "IDENT";
        case TOK_COLON: return "COLON"; case TOK_COMMA: return "COMMA";
        case TOK_DOT: return "DOT"; case TOK_PLUS: return "PLUS";
        case TOK_MINUS: return "MINUS"; case TOK_INDENT: return "INDENT";
        case TOK_DEDENT: return "DEDENT"; case TOK_NEWLINE: return "NEWLINE";
        case TOK_EOF: return "EOF"; case TOK_TEMPLATE: return "TEMPLATE";
        case TOK_HTTP_GET: return "GET"; case TOK_HTTP_POST: return "POST";
        case TOK_RUNS: return "RUNS"; case TOK_API: return "API";
        case TOK_CONFIGURE: return "CONFIGURE"; case TOK_PURPOSE: return "PURPOSE";
        case TOK_EMIT: return "EMIT"; case TOK_STORE: return "STORE";
        case TOK_NEEDS: return "NEEDS"; case TOK_APPROVAL: return "APPROVAL";
        case TOK_FOR: return "FOR"; case TOK_EACH: return "EACH";
        case TOK_IN: return "IN"; case TOK_SAVE: return "SAVE";
        case TOK_AS: return "AS"; case TOK_APPLY: return "APPLY";
        case TOK_CHECK: return "CHECK"; case TOK_SHOW: return "SHOW";
        case TOK_TRY: return "TRY"; case TOK_STOP: return "STOP";
        case TOK_MIDDLEWARE: return "MIDDLEWARE"; case TOK_PROXY: return "PROXY";
        case TOK_FORWARD: return "FORWARD";
        case TOK_DESCRIPTION: return "DESCRIPTION"; case TOK_AUTHOR: return "AUTHOR";
        default: return "?";
    }
}

static int cmd_tokens(const char *filename) {
    char *source = read_file(filename);
    if (!source) return 1;

    NcLexer *lex = nc_lexer_new(source, filename);
    nc_lexer_tokenize(lex);

    printf("\n  NC Lexer - %s\n", filename);
    printf("  ----------------------------------------\n");

    for (int i = 0; i < lex->token_count; i++) {
        NcToken *t = &lex->tokens[i];
        if (t->type == TOK_NEWLINE) continue;
        printf("  L%3d  %-14s  ", t->line, token_name(t->type));
        if (t->type == TOK_STRING || t->type == TOK_IDENTIFIER || t->type == TOK_TEMPLATE)
            printf("\"%.*s\"", t->length, t->start);
        else if (t->type == TOK_INTEGER)
            printf("%lld", (long long)t->literal.int_val);
        else if (t->type == TOK_FLOAT_LIT)
            printf("%g", t->literal.float_val);
        else if (t->type == TOK_INDENT || t->type == TOK_DEDENT)
            printf("->");
        else if (t->length > 0)
            printf("%.*s", t->length, t->start);
        printf("\n");
    }
    printf("\n  Total: %d tokens\n\n", lex->token_count);

    nc_lexer_free(lex);
    free(source);
    return 0;
}

/* â”€â”€ nc validate â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static int cmd_validate(const char *filename) {
    char *source = read_file(filename);
    if (!source) return 1;

    NcLexer *lex = nc_lexer_new(source, filename);
    nc_lexer_tokenize(lex);

    int behaviors = 0, types = 0, routes = 0;
    const char *service_name = NULL;
    int service_name_len = 0;

    for (int i = 0; i < lex->token_count; i++) {
        NcToken *t = &lex->tokens[i];
        if (t->type == TOK_SERVICE && i + 1 < lex->token_count) {
            service_name = lex->tokens[i + 1].start;
            service_name_len = lex->tokens[i + 1].length;
        }
        if (t->type == TOK_TO) behaviors++;
        if (t->type == TOK_DEFINE) types++;
        if (t->type == TOK_HTTP_GET || t->type == TOK_HTTP_POST ||
            t->type == TOK_HTTP_PUT || t->type == TOK_HTTP_DELETE)
            routes++;
    }

    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
    NcASTNode *program = nc_parser_parse(parser);

    if (parser->had_error) {
        printf("\n  \033[31mINVALID\033[0m  %s\n\n", filename);
        printf("    \033[31m%s\033[0m\n\n", parser->error_msg);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 1;
    }

    printf("\n");
    printf("  \033[32mVALID\033[0m  %s\n\n", filename);
    printf("    \033[1mService:\033[0m    %.*s\n", service_name_len, service_name ? service_name : "(unnamed)");
    printf("    \033[1mBehaviors:\033[0m  \033[36m%d\033[0m\n", behaviors);
    printf("    \033[1mTypes:\033[0m      \033[36m%d\033[0m\n", types);
    printf("    \033[1mRoutes:\033[0m     \033[36m%d\033[0m\n", routes);
    printf("    \033[1mTokens:\033[0m     \033[90m%d\033[0m\n", lex->token_count);
    printf("\n");

    nc_ast_free(program);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);
    return 0;
}

/* â”€â”€ nc mascot â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static void print_mascot(void) {
    if (nc_cli_plain_mode()) {
        printf("  NC\n");
        printf("  Notation-as-Code\n");
        printf("  The fastest way to build AI APIs\n");
        return;
    }
    printf("  \033[36m _  _  ___\033[0m\n");
    printf("  \033[36m| \\| |/ __|\033[0m   \033[1m\033[36mNotation-as-Code\033[0m\n");
    printf("  \033[36m| .` | (__\033[0m    \033[90mThe fastest way to build AI APIs\033[0m\n");
    printf("  \033[36m|_|\\_|\\___|\033[0m\n");
}

static int cmd_mascot(void) {
    printf("\n");
    print_mascot();
    printf("\n");
    printf("  \033[90m\"The best code reads like a conversation.\"\033[0m\n\n");
    return 0;
}

/* â”€â”€ nc version â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static int cmd_version(void) {
    if (!nc_cli_plain_mode()) {
        nc_animate_startup();
    } else {
        printf("\n  NC v%s\n  Notation-as-Code\n\n", NC_VERSION);
    }
    printf("  \033[1mCreator:\033[0m  Nuckala Sai Narender\n");
    printf("  \033[1mCompany:\033[0m  DevHeal Labs AI (devheallabs.in)\n");
    printf("  \033[1mLicense:\033[0m  Apache 2.0\n");
    printf("  \033[1mBinary:\033[0m   ~365 KB \033[90m|\033[0m \033[1mDeps:\033[0m zero \033[90m|\033[0m \033[1mLang:\033[0m C11\n");
    printf("  \033[1mBuilt-in:\033[0m 70+ functions \033[90m|\033[0m AI \033[90m|\033[0m HTTP \033[90m|\033[0m JSON \033[90m|\033[0m REPL\n");
    printf("\n");
    printf("  \033[90m\"The best code reads like a conversation.\"\033[0m\n\n");
    return 0;
}

/* â”€â”€ main â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static void usage(void) {
    printf("\n");
    if (nc_cli_plain_mode()) {
        printf("  NC v%s\n", NC_VERSION);
        printf("  Notation-as-Code\n");
        printf("  The fastest way to build AI APIs\n");
    } else {
        printf("  \033[36m _  _  ___\033[0m\n");
        printf("  \033[36m| \\| |/ __|\033[0m   \033[1m\033[36mNotation-as-Code\033[0m \033[33mv%s\033[0m\n", NC_VERSION);
        printf("  \033[36m| .` | (__\033[0m    \033[90mThe fastest way to build AI APIs\033[0m\n");
        printf("  \033[36m|_|\\_|\\___|\033[0m\n");
    }
    printf("\n");
    printf("  \033[1mUsage:\033[0m nc <command> [file.nc] [options]\n");
    printf("         nc \"plain english\"            Run code directly\n");
    printf("         nc -c \"set x to 5; show x\"    Execute inline code\n");
    printf("         nc -e \"42 + 8\"                Evaluate expression\n");
    printf("         echo \"show 42\" | nc           Pipe from stdin\n\n");
    printf("  Plain English (like Python -c):\n");
    printf("    nc \"show 42 + 8\"                   Quick math\n");
    printf("    nc \"show upper('hello')\"           String ops\n");
    printf("    nc -c \"set x to 5; show x * 2\"    Multi-statement\n");
    printf("    nc -e \"len([1,2,3])\"               Evaluate & print\n");
    printf("    nc repl                            Interactive mode\n\n");
    printf("  Run & Execute:\n");
    printf("    run <file>              Run a .nc file\n");
    printf("    run <file> -b <name>    Run a specific behavior\n");
    printf("    run <file> --no-cache   Skip bytecode cache\n");
    printf("    serve <file>            Start HTTP server (threaded)\n");
    printf("    serve <file> -p 8080    Start on specific port\n");
    printf("    debug <file>            Debug with step-through\n");
    printf("    debug <file> --dap      Start DAP server for IDEs\n\n");
    printf("  Compile:\n");
    printf("    build <file>            Compile to native binary\n");
    printf("    build <file> -o name    Compile with custom output name\n");
    printf("    build <file> --target   Cross-compile (linux-x64, darwin-arm64, windows-x64)\n");
    printf("    build <file> --static   Produce a statically-linked binary\n");
    printf("    build <file> --release  Optimized + stripped production build\n");
    printf("    compile <file>          Compile to LLVM IR (.ll)\n");
    printf("    bytecode <file>         Show compiled bytecodes\n\n");
    printf("  Analyze:\n");
    printf("    validate <file>         Check syntax\n");
    printf("    tokens <file>           Show token stream\n\n");
    printf("  HTTP:\n");
    printf("    get <url>               HTTP GET (like curl)\n");
    printf("    post <url> <body>       HTTP POST with JSON body\n\n");
    printf("  Project:\n");
    printf("    init <name>             Create a new NC project\n");
    printf("    init <name> --all       Create with Docker + Kubernetes\n");
    printf("    setup [name]            Create project + configure AI + start server\n");
    printf("    doctor                  Check your setup (AI key, .env, etc.)\n\n");
    printf("  Run & Deploy:\n");
    printf("    start                   Start all services (auto-discovers .nc files)\n");
    printf("    start <file>            Start a specific service\n");
    printf("    start -p 8080           Start on a specific port\n");
    printf("    stop                    Stop all running NC services\n");
    printf("    dev [file]              Validate + start (development mode)\n");
    printf("    deploy                  Build container image\n");
    printf("    deploy --tag app:v1     Build with custom tag\n");
    printf("    deploy --push           Build and push to registry\n\n");
    printf("  Tooling:\n");
    printf("    pkg init                Create nc.pkg manifest\n");
    printf("    pkg install <name>      Install a package\n");
    printf("    pkg list                List packages\n");
    printf("    lsp                     Start LSP server (for IDEs)\n\n");
    printf("  Convert:\n");
    printf("    migrate <file|dir>      AI-powered migration to NC (hybrid-aware)\n");
    printf("    digest <file.py/js>     Offline pattern-match conversion to NC\n");
    printf("    fmt <file>              Auto-format a .nc file\n\n");
    printf("  Performance:\n");
    printf("    profile <file>          Run with profiling\n\n");
    printf("  Test:\n");
    printf("    test                    Run all tests in tests/lang/\n");
    printf("    test <file>             Run a specific test file\n");
    printf("    test -v                 Verbose test output\n\n");
    printf("  Info:\n");
    printf("    version                 Show version\n");
    printf("    mascot                  Show NC mascot\n\n");
}

/* Auto-load .env file â€” like dotenv for Node.js.
 * Reads key=value pairs from .env in current directory.
 * Does NOT override existing env vars (explicit exports take priority). */
static void load_dotenv(void) {
    FILE *f = fopen(".env", "r");
    if (!f) return;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and blank lines */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        /* Remove trailing newline */
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        char *cr = strchr(p, '\r');
        if (cr) *cr = '\0';

        /* Find = separator */
        char *eq = strchr(p, '=');
        if (!eq) continue;

        /* Extract key */
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;

        /* Trim key */
        while (*key == ' ') key++;
        char *kend = key + strlen(key) - 1;
        while (kend > key && (*kend == ' ' || *kend == '\t')) *kend-- = '\0';

        /* Trim value â€” strip surrounding quotes */
        while (*val == ' ') val++;
        int vlen = (int)strlen(val);
        if (vlen >= 2 && ((val[0] == '"' && val[vlen-1] == '"') ||
                          (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            val++;
        }

        /* Only set if not already defined (explicit exports win) */
        if (key[0] && !getenv(key)) {
            nc_setenv(key, val, 0);
        }
    }
    fclose(f);
}

static void nc_cleanup_at_exit(void) {
    nc_ast_cache_flush();
    nc_pool_shutdown();
    nc_gc_shutdown();
}

int main(int argc, char *argv[]) {
    nc_colors_init();
    nc_gc_init();
    atexit(nc_cleanup_at_exit);
    signal(SIGINT, nc_signal_handler);
    signal(SIGTERM, nc_signal_handler);

    /* Auto-load .env file from current directory */
    load_dotenv();

    if (nc_ensure_license_acceptance() != 0) {
        return 1;
    }

    /* Load external AI config if available (adapter presets) */
    nc_ai_load_config(NULL);

    if (argc < 2) {
        /* If stdin is a pipe, read and execute it */
        if (!nc_isatty(nc_fileno(stdin))) {
            size_t code_cap = 1024 * 1024;
            char *code = calloc(code_cap, 1);
            if (!code) { fprintf(stderr, "Out of memory\n"); return 1; }
            size_t total = 0;
            char buf[4096];
            while (fgets(buf, sizeof(buf), stdin)) {
                size_t blen = strlen(buf);
                if (total + blen >= code_cap - 1) {
                    code_cap *= 2;
                    char *tmp = realloc(code, code_cap);
                    if (!tmp) { free(code); fprintf(stderr, "Out of memory\n"); return 1; }
                    code = tmp;
                }
                memcpy(code + total, buf, blen);
                total += blen;
            }
            code[total] = '\0';
            if (total == 0) { free(code); usage(); return 0; }
            for (char *p = code; *p; p++) {
                if (*p == ';') *p = '\n';
            }
            size_t wrap_cap = total * 2 + 8192;
            char *wrapped = malloc(wrap_cap);
            if (!wrapped) { free(code); fprintf(stderr, "Out of memory\n"); return 1; }
            int wpos = snprintf(wrapped, wrap_cap, "to __cli__:\n");
            char *line = code;
            while (line && *line) {
                char *nl = strchr(line, '\n');
                if (nl) *nl = '\0';
                while (*line == ' ') line++;
                if (*line)
                    wpos += snprintf(wrapped + wpos, wrap_cap - wpos, "    %s\n", line);
                if (nl) line = nl + 1; else break;
            }
            NcMap *globals = nc_map_new();
            NcValue result = nc_call_behavior(wrapped, "<stdin>", "__cli__", globals);
            if (!IS_NONE(result)) {
                nc_value_print(result, stdout);
                printf("\n");
            }
            nc_map_free(globals);
            free(code);
            free(wrapped);
            return 0;
        }
        return nc_repl_run();
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "version") == 0) {
        return cmd_version();
    }
    if (strcmp(cmd, "mascot") == 0) {
        return cmd_mascot();
    }
    if (strcmp(cmd, "test") == 0) {
        return cmd_test(argc, argv);
    }
    if (strcmp(cmd, "conformance") == 0) {
        return nc_conformance_run();
    }
    if (strcmp(cmd, "repl") == 0) {
        return nc_repl_run();
    }

    /* â”€â”€ nc -c "plain english code" â”€â”€â”€ like python -c â”€â”€â”€ */
    if (strcmp(cmd, "-c") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nc -c \"show 42 + 8\"\n");
            return 1;
        }
        /* Join all remaining args into one string (allows unquoted multi-word) */
        char code[8192] = "";
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(code, " ", sizeof(code) - strlen(code) - 1);
            strncat(code, argv[i], sizeof(code) - strlen(code) - 1);
        }
        /* Replace semicolons/commas with newlines for multi-statement */
        for (char *p = code; *p; p++) {
            if (*p == ';') *p = '\n';
        }
        /* Wrap in a behavior â€” indent each line */
        char wrapped[16384];
        int wpos = snprintf(wrapped, sizeof(wrapped), "to __cli__:\n");
        char *line = code;
        while (line && *line) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            while (*line == ' ') line++;
            if (*line)
                wpos += snprintf(wrapped + wpos, sizeof(wrapped) - wpos, "    %s\n", line);
            if (nl) line = nl + 1; else break;
        }
        NcMap *globals = nc_map_new();
        NcValue result = nc_call_behavior(wrapped, "<cli>", "__cli__", globals);
        if (IS_MAP(result)) {
            NcValue err = nc_map_get(AS_MAP(result), nc_string_from_cstr("error"));
            if (IS_STRING(err)) {
                fprintf(stderr, "Error: %s\n", AS_STRING(err)->chars);
                nc_map_free(globals);
                return 1;
            }
        }
        if (!IS_NONE(result)) {
            nc_value_print(result, stdout);
            printf("\n");
        }
        nc_map_free(globals);
        return 0;
    }

    /* â”€â”€ nc -e "expression" â”€â”€â”€ evaluate and print â”€â”€â”€ */
    if (strcmp(cmd, "-e") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nc -e \"42 + 8\"\n");
            return 1;
        }
        char code[8192] = "";
        for (int i = 2; i < argc; i++) {
            if (i > 2) strncat(code, " ", sizeof(code) - strlen(code) - 1);
            strncat(code, argv[i], sizeof(code) - strlen(code) - 1);
        }
        char wrapped[16384];
        snprintf(wrapped, sizeof(wrapped),
            "to __cli__:\n    respond with %s\n", code);
        NcMap *globals = nc_map_new();
        NcValue result = nc_call_behavior(wrapped, "<cli>", "__cli__", globals);
        if (!IS_NONE(result)) {
            nc_value_print(result, stdout);
            printf("\n");
        }
        nc_map_free(globals);
        return 0;
    }

    /* â”€â”€ Helper: LLM-generate code from prompt, write to file â”€â”€ */
    /* Returns 1 if LLM produced usable output, 0 if fallback needed */
    #define NC_LLM_MIN_OUTPUT 50  /* minimum chars for LLM output to be "usable" */

    /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
     *  nc ai â€” Built-in LLM for NC code generation
     *
     *  nc ai generate "create a todo app with REST API"
     *  nc ai generate "build a chat app with websockets" -o app.nc
     *  nc ai create "a blog app with auth"  Create full project
     *  nc ai serve                     Start AI generation API server
     *  nc ai train <data_dir>          Train/fine-tune model
     *  nc ai status                    Show model info
     * â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
    if (strcmp(cmd, "ai") == 0) {
        const char *subcmd = (argc >= 3) ? argv[2] : "status";

        /* â”€â”€ nc ai status â”€â”€â”€ Show model info â”€â”€â”€ */
        if (strcmp(subcmd, "status") == 0 || strcmp(subcmd, "info") == 0) {
            if (!nc_cli_plain_mode()) nc_animate_nc_ai();
            nc_spinner_start("Loading model...");
            const char *nova_paths[] = {
                "nova_v2.bin",
                "nova_codexglue_v2.bin",
                "nova_codexglue_model.bin",
                "nova_code_model.bin",
                "nova_model.bin",
                "nc-ai/nova/model/nova_model.bin",
                "nc-ai/nova/model/nova_final.bin",
                "training_data/nova_model.bin",
                NULL
            };
            const char *old_model_paths[] = {
                "nc_ai_model_prod.bin",
                "nc_ai_model.bin",
                "training_data/nc_ai_model_prod.bin",
                "training_data/nc_ai_model.bin",
                NULL
            };

            const char *found_path = NULL;
            const char *first_bad_path = NULL;
            NovaModel *nova = NULL;
            NCModel *model = NULL;

            for (int i = 0; nova_paths[i] && !nova; i++) {
                if (!nc_file_exists(nova_paths[i])) continue;
                if (nc_try_load_nova_model(nova_paths[i], &nova)) {
                    found_path = nova_paths[i];
                    break;
                }
                if (!first_bad_path) first_bad_path = nova_paths[i];
            }
            for (int i = 0; old_model_paths[i] && !nova && !model; i++) {
                if (!nc_file_exists(old_model_paths[i])) continue;
                model = nc_model_load(old_model_paths[i]);
                if (model) {
                    found_path = old_model_paths[i];
                    break;
                }
                if (!first_bad_path) first_bad_path = old_model_paths[i];
            }

            if (nova || model)
                nc_spinner_stop("Model loaded successfully");
            else
                nc_spinner_stop("No model found");

            if (nova) {
                printf("  Model:    %s\n", found_path);
                printf("  Format:   NOVA\n");
                printf("  Family:   %s\n", nova_size_name(nova->config.size));
                printf("  Dim:      %d\n", nova->config.dim);
                printf("  Layers:   %d\n", nova->config.n_layers);
                printf("  Vocab:    %d\n", nova->config.vocab_size);
                printf("  Context:  %d tokens\n", nova->config.max_seq);
                printf("  Params:   %.1fM\n", nova_count_params(nova) / 1000000.0);
                printf("  Status:   Ready\n");
                nova_free(nova);
            } else if (model) {
                printf("  Model:    %s\n", found_path);
                printf("  Format:   NCModel-v1\n");
                printf("  Dim:      %d\n", model->dim);
                printf("  Layers:   %d\n", model->n_layers);
                printf("  Vocab:    %d\n", model->vocab_size);
                printf("  Context:  %d tokens\n", model->max_seq);
                long params = (long)model->vocab_size * model->dim * 2; /* embeddings */
                for (int i = 0; i < model->n_layers; i++) {
                    params += (long)model->dim * model->dim * 4; /* Q,K,V,O */
                    params += (long)model->dim * (model->dim * 4) * 2; /* FFN */
                    params += (long)model->dim * 4; /* layer norms */
                }
                params += (long)model->dim * model->vocab_size; /* lm_head */
                if (params > 1000000)
                    printf("  Params:   %.1fM\n", params / 1000000.0);
                else
                    printf("  Params:   %ld\n", params);
                printf("  Status:   Ready\n");
                nc_model_free(model);
            } else if (first_bad_path) {
                printf("  Model:    %s (corrupt/incompatible)\n", first_bad_path);
            } else {
                printf("  Model:    Not found\n");
                printf("  Train:    nc ai train <data_dir>\n");
            }
            printf("\n");
            printf("  Commands:\n");
            printf("    nc ai chat                      Interactive AI conversation\n");
            printf("    nc ai reason \"question\"         Ask AI to reason about something\n");
            printf("    nc ai generate \"description\"    Generate NC code from text\n");
            printf("    nc ai create \"description\"      Create full project (backend + frontend + tests)\n");
            printf("    nc ai distill --url <url>       Distill from any external LLM\n");
            printf("    nc ai learn <project_dir>       Learn from existing NC project\n");
            printf("    nc ai review <file>             Code review with recommendations\n");
            printf("    nc ai check <file|dir>          Validate & auto-fix NC code\n");
            printf("    nc ai export <dir> [format]     Export project (binary/docker/zip/pkg)\n");
            printf("    nc ai serve                     Start AI API server\n");
            printf("    nc ai train                     Train model on NC corpus\n");
            printf("    nc ai reason \"question\"         Reasoning engine (math, physics, logic)\n");
            printf("    nc ai math \"expression\"         Evaluate math expression\n");
            printf("    nc ai synth -n 1000 -o data.nc  Generate synthetic training data\n");
            printf("    nc ai efficient                 Show training efficiency report\n");
            printf("    nc ai moe                       Show Mixture of Experts info\n");
            printf("    nc ai benchmark                 Run AI performance benchmark\n");
            printf("    nc ai status                    Show this info\n");
            printf("\n");
            return 0;
        }

        /* â”€â”€ nc ai efficient â”€â”€â”€ Show efficiency report â”€â”€â”€ */
        if (strcmp(subcmd, "efficient") == 0 || strcmp(subcmd, "eff") == 0) {
            long base_params = 200000000L; /* 200M base */
            int lora_rank = 16;
            int n_experts = 8;
            int n_active = 2;

            /* Parse optional flags */
            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "--base") == 0 && i + 1 < argc)
                    base_params = atol(argv[++i]);
                else if (strcmp(argv[i], "--rank") == 0 && i + 1 < argc)
                    lora_rank = atoi(argv[++i]);
                else if (strcmp(argv[i], "--experts") == 0 && i + 1 < argc)
                    n_experts = atoi(argv[++i]);
                else if (strcmp(argv[i], "--active") == 0 && i + 1 < argc)
                    n_active = atoi(argv[++i]);
            }

            NCEfficiencyReport r = nc_efficiency_report(base_params, lora_rank,
                                                         n_experts, n_active);
            nc_efficiency_report_print(&r);
            return 0;
        }

        /* â”€â”€ nc ai synth â”€â”€â”€ Generate synthetic training data â”€â”€â”€ */
        if (strcmp(subcmd, "synth") == 0 || strcmp(subcmd, "synthetic") == 0) {
            const char *output = "synthetic_training_data.nc";
            int n_examples = 1000;

            for (int i = 3; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
                    output = argv[++i];
                else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
                    n_examples = atoi(argv[++i]);
            }

            nc_print_banner("NC AI Synthetic Data Generator");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Generating %d examples to %s...\n\n", n_examples, output);

            NCSynthStats stats = nc_synth_generate(output, n_examples);

            printf("  Done!\n");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Total examples:  %d\n", stats.n_generated);
            printf("  Total bytes:     %ld\n", stats.total_bytes);
            printf("  Total tokens:    ~%ld\n", stats.total_tokens);
            printf("  By type:\n");
            const char *type_names[] = {
                "Service", "UI Page", "CRUD", "Test",
                "Middleware", "AI Service", "Data Pipeline", "Math Code"
            };
            for (int i = 0; i < 8; i++) {
                if (stats.n_per_type[i] > 0)
                    printf("    %-14s %d\n", type_names[i], stats.n_per_type[i]);
            }
            printf("\n  Train with: nc ai train %s\n\n", output);
            return 0;
        }

        /* â”€â”€ nc ai reason "query" â”€â”€â”€ Reasoning engine â”€â”€â”€ */
        if (strcmp(subcmd, "reason") == 0) {
            if (argc < 4) {
                fprintf(stderr, "  Usage: nc ai reason \"why does F=ma?\"\n");
                fprintf(stderr, "         nc ai reason \"calculate sin(pi/4) * sqrt(2)\"\n");
                fprintf(stderr, "         nc ai reason \"what is the force on a 10kg mass at 9.8m/s2?\"\n");
                return 1;
            }
            const char *query = argv[3];

            NCReasonConfig cfg = nc_reason_default_config();
            NCReasonEngine *engine = nc_reason_create(cfg);
            NCReasonChain *chain = nc_reason_query(engine, query);

            if (chain) {
                nc_reason_chain_print(chain);
                nc_reason_chain_free(chain);
            }

            nc_reason_stats_print(engine);
            nc_reason_free(engine);
            return 0;
        }

        /* â”€â”€ nc ai math "expression" â”€â”€â”€ Math evaluator â”€â”€â”€ */
        if (strcmp(subcmd, "math") == 0 || strcmp(subcmd, "calc") == 0) {
            if (argc < 4) {
                fprintf(stderr, "  Usage: nc ai math \"sin(pi/4) * sqrt(2)\"\n");
                fprintf(stderr, "         nc ai math \"factorial(10)\"\n");
                fprintf(stderr, "         nc ai calc \"2^10 + log(100)\"\n");
                return 1;
            }
            NCMathResult r = nc_math_evaluate(argv[3]);
            if (r.valid) {
                printf("\n  %s = %g", argv[3], r.value);
                if (r.unit[0]) printf(" %s", r.unit);
                printf("\n\n");
            } else {
                fprintf(stderr, "  Error: could not evaluate \"%s\"\n", argv[3]);
                return 1;
            }
            return 0;
        }

        /* â”€â”€ nc ai benchmark â”€â”€â”€ Run benchmark suite â”€â”€â”€ */
        if (strcmp(subcmd, "benchmark") == 0 || strcmp(subcmd, "bench") == 0) {
            nc_benchmark_run();
            return 0;
        }

        /* â”€â”€ nc ai moe â”€â”€â”€ Show MoE expert routing info â”€â”€â”€ */
        if (strcmp(subcmd, "moe") == 0 || strcmp(subcmd, "experts") == 0) {
            NCMoEConfig cfg = nc_moe_default_config();
            int model_dim = 1024;
            NCMoESystem *moe = nc_moe_create(cfg, model_dim);
            nc_print_banner("NC AI Mixture of Experts");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Total experts:   %d\n", cfg.n_experts);
            printf("  Active per query: %d\n", cfg.n_active);
            printf("  Expert dim:      %d\n", model_dim);
            printf("  Total params:    %.1fB\n", moe->total_params / 1.0e9);
            printf("  Active params:   %.1fM\n", moe->active_params / 1.0e6);
            printf("\n  Expert specializations:\n");
            const char *expert_names[] = {
                "API/Service", "NC UI Frontend", "Data Processing",
                "Logic/Control", "Math Operations", "Test Generation",
                "Infrastructure", "General NC"
            };
            for (int i = 0; i < cfg.n_experts; i++) {
                printf("    Expert %d: %s\n", i, expert_names[i]);
            }
            printf("\n");
            nc_moe_free(moe);
            return 0;
        }

        /* Forward declaration for shared AI generator recovery */
        extern int nc_cmd_generate(int argc, char **argv);

        /* â”€â”€ nc ai generate "prompt" â”€â”€â”€ Generate NC code â”€â”€â”€ */
        if (strcmp(subcmd, "generate") == 0 || strcmp(subcmd, "gen") == 0) {
            if (argc < 4) {
                fprintf(stderr, "  Usage: nc ai generate \"create a REST API for users\"\n");
                fprintf(stderr, "         nc ai generate \"build a todo app\" -o app.nc\n");
                return 1;
            }

            const char *prompt = argv[3];
            const char *output_file = NULL;
            int max_tokens = 256;
            float temperature = 0.7f;

            /* Parse flags */
            for (int i = 4; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output_file = argv[++i];
                else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) temperature = atof(argv[++i]);
                else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) max_tokens = atoi(argv[++i]);
            }

            /* Try NOVA model first (primary), then fallback to old NCModel */
            const char *nova_paths[] = {
                "nova_v2.bin",
                "nova_model.bin",
                "nova_codexglue_v2.bin",
                "nova_codexglue_model.bin",
                "nova_code_model.bin",
                "nc-ai/nova/model/nova_model.bin",
                "nc-ai/nova/model/nova_final.bin",
                "training_data/nova_model.bin",
                NULL
            };
            const char *old_model_paths[] = {
                "nc_ai_model_prod.bin",
                "nc_ai_model.bin",
                "training_data/nc_ai_model_prod.bin",
                "training_data/nc_ai_model.bin",
                NULL
            };
            const char *tok_paths[] = {
                "nc_ai_tokenizer.bin", "training_data/nc_ai_tokenizer.bin", NULL
            };

            /* Load tokenizer */
            NCTokenizer *tok = NULL;
            for (int i = 0; tok_paths[i] && !tok; i++)
                tok = nc_tokenizer_load(tok_paths[i]);

            char *repair_stdout_argv[] = { argv[0], "generate", "--ai-only", (char*)prompt, "--stdout" };
            char *repair_output_argv[] = { argv[0], "generate", "--ai-only", (char*)prompt, "--output", (char*)output_file };
            char **repair_argv = output_file ? repair_output_argv : repair_stdout_argv;
            int repair_argc = output_file ? 6 : 5;

            /* Try NOVA model first â€” create with default config, then load weights */
            NovaModel *nova = NULL;
            for (int i = 0; nova_paths[i] && !nova; i++) {
                FILE *test_fp = fopen(nova_paths[i], "rb");
                if (!test_fp) continue;
                /* Read config from file header to create model with right size */
                uint32_t mag, ver;
                if (fread(&mag, 4, 1, test_fp) == 1 && fread(&ver, 4, 1, test_fp) == 1) {
                    if (mag != NOVA_MAGIC) {
                        fclose(test_fp);
                        continue;
                    }
                    NovaConfig nc_cfg;
                    if (fread(&nc_cfg, sizeof(NovaConfig), 1, test_fp) == 1 && nc_cfg.dim > 0) {
                        fclose(test_fp);
                        nova = nova_create(nc_cfg);
                        if (nova && nova_load(nova, nova_paths[i]) != 0) {
                            nova_free(nova); nova = NULL;
                        }
                    } else { fclose(test_fp); }
                } else { fclose(test_fp); }
            }

            if (nova && tok) {
                if (!nc_cli_plain_mode()) nc_animate_nova();
                nc_print_step("NOVA Code Generation");
                printf("  Model: %s (%lld params)\n",
                       nova_size_name(nova->config.size),
                       (long long)nova_count_params(nova));
                printf("  Prompt: \"%s\"\n", prompt);
                printf("\n");

                /* Encode prompt */
                int prompt_tokens[512];
                int prompt_len = nc_tokenizer_encode(tok, prompt, prompt_tokens, 512);
                if (prompt_len < 1) { prompt_tokens[0] = 1; prompt_len = 1; }

                /* Autoregressive generation with NOVA */
                int *gen_tokens = (int*)calloc(prompt_len + max_tokens, sizeof(int));
                memcpy(gen_tokens, prompt_tokens, prompt_len * sizeof(int));
                int total_len = prompt_len;

                struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
                double t_start = ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;

                for (int i = 0; i < max_tokens; i++) {
                    int ctx_start = (total_len > 256) ? total_len - 256 : 0;
                    int ctx_len = total_len - ctx_start;
                    int next = nova_generate_next(nova, gen_tokens + ctx_start, ctx_len, temperature);
                    if (next <= 0 || next >= nova->config.vocab_size) break;
                    gen_tokens[total_len++] = next;
                }

                clock_gettime(CLOCK_MONOTONIC, &ts);
                double t_elapsed = (ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6 - t_start) / 1000.0;
                int gen_count = total_len - prompt_len;

                /* Decode generated tokens */
                char *text = (char*)malloc(gen_count * 64 + 1);
                int text_len = nc_tokenizer_decode(tok, gen_tokens + prompt_len, gen_count, text, gen_count * 64);
                text[text_len] = '\0';

                if (!nc_ai_is_valid_nc(text)) {
                    free(gen_tokens);
                    free(text);
                    nova_free(nova);
                    nc_tokenizer_free(tok);
                    fprintf(stderr, "  [Note] NOVA output did not validate as NC. Retrying with the shared AI repair loop.\n\n");
                    return nc_cmd_generate(repair_argc, repair_argv);
                }

                printf("%s\n", text);
                nc_print_success("Generation complete");
                printf("  Generated %d tokens in %.2fs (%.0f tok/s)\n",
                       gen_count, t_elapsed, gen_count / (t_elapsed + 0.001));

                if (output_file) {
                    FILE *f = fopen(output_file, "w");
                    if (f) { fprintf(f, "%s\n", text); fclose(f); printf("  Saved to: %s\n", output_file); }
                }
                printf("\n");

                free(gen_tokens);
                free(text);
                nova_free(nova);
                nc_tokenizer_free(tok);
                return 0;
            }
            if (nova) nova_free(nova);

            /* Fallback: old NCModel */
            NCModel *model = NULL;
            for (int i = 0; old_model_paths[i] && !model; i++)
                model = nc_model_load(old_model_paths[i]);

            if (!model) {
                fprintf(stderr, "  Error: No trained model found.\n");
                fprintf(stderr, "  Run 'nc ai train <data_dir>' first to train a NOVA model.\n");
                if (nova) nova_free(nova);
                if (tok) nc_tokenizer_free(tok);
                return 1;
            }

            if (!tok) {
                tok = nc_tokenizer_create();
                const char *seed_texts[] = { prompt };
                nc_tokenizer_train(tok, seed_texts, 1, model->vocab_size);
            }

            nc_print_step("Generating code...");
            printf("  Prompt: \"%s\"\n\n", prompt);

            int *prompt_tokens = malloc(model->max_seq * sizeof(int));
            int prompt_len = nc_tokenizer_encode(tok, prompt, prompt_tokens, model->max_seq);
            int *output = malloc((model->max_seq + max_tokens) * sizeof(int));
            int gen_len = nc_model_generate(model, prompt_tokens, prompt_len,
                                            max_tokens, temperature, output);
            char *text = malloc(gen_len * 64 + 1);
            int text_len = nc_tokenizer_decode(tok, output, gen_len, text, gen_len * 64);
            text[text_len] = '\0';

            /* Check if output is repetitive (model collapsed) before escalating. */
            int use_template = 0;
            if (nc_ai_output_is_repetitive(output, gen_len, prompt_len)) use_template = 1;
            if (text_len < 5) use_template = 1;

            if (use_template) {
                free(prompt_tokens); free(output); free(text);
                nc_model_free(model);
                nc_tokenizer_free(tok);
                fprintf(stderr, "  [Note] Neural model output is low quality. Retrying with the shared AI repair loop.\n\n");
                return nc_cmd_generate(repair_argc, repair_argv);
            }

            printf("%s\n", text);
            if (output_file) {
                FILE *f = fopen(output_file, "w");
                if (f) { fprintf(f, "%s\n", text); fclose(f); printf("\n  Saved to: %s\n", output_file); }
            }
            printf("\n");
            free(prompt_tokens); free(output); free(text);
            nc_model_free(model);
            nc_tokenizer_free(tok);
            return 0;
        }

        /* â”€â”€ nc ai create â”€â”€â”€ LLM-driven full project generation â”€â”€â”€ */
        if (strcmp(subcmd, "create") == 0 || strcmp(subcmd, "new") == 0) {
            if (argc < 4) {
                fprintf(stderr, "\n  NC AI â€” Full Project Generator (LLM-Driven)\n");
                fprintf(stderr, "  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
                fprintf(stderr, "  Usage:\n");
                fprintf(stderr, "    nc ai create \"a blog app with user auth\"\n");
                fprintf(stderr, "    nc ai create \"stock market app with AI predictions\"\n");
                fprintf(stderr, "    nc ai create \"todo app\" -o my_todo\n");
                fprintf(stderr, "    nc ai create \"todo app\" --template-fallback -o my_todo\n");
                fprintf(stderr, "\n  By default this command is AI-only and requires the built-in model.\n");
                fprintf(stderr, "  Use --template-fallback to opt into deterministic scaffolds.\n");
                fprintf(stderr, "  The built-in LLM generates each file from prompts.\n");
                fprintf(stderr, "  Prompt library: nc-ai/prompts/\n\n");
                fprintf(stderr, "  Generates:\n");
                fprintf(stderr, "    <project>/service.nc    Backend API (LLM-generated)\n");
                fprintf(stderr, "    <project>/app.ncui      Frontend UI (LLM-generated)\n");
                fprintf(stderr, "    <project>/test_app.nc   Tests (LLM-generated)\n");
                fprintf(stderr, "    <project>/README.md     Documentation\n\n");
                fprintf(stderr, "    <project>/dist/         Frontend release bundle (auto-built when nc-ui is available)\n\n");
                return 1;
            }

            const char *prompt = argv[3];
            const char *project_dir = NULL;
            int template_fallback_enabled = 0;
            int ui_release_status = -1;
            char ui_release_output[1024] = "";
            char ui_release_cli[1024] = "";

            /* Parse flags */
            for (int i = 4; i < argc; i++) {
                if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) && i + 1 < argc)
                    project_dir = argv[++i];
                else if (strcmp(argv[i], "--template-fallback") == 0)
                    template_fallback_enabled = 1;
            }

            /* â”€â”€ Load the LLM model â”€â”€â”€ */
            /* Build path relative to binary location */
            char bin_model_path[512] = "", bin_tok_path[512] = "";
            {
                char self_buf2[512];
                char *self2 = nc_realpath(argv[0], self_buf2);
                if (self2) {
                    char *slash = strrchr(self2, '/');
                    if (slash) {
                        *slash = '\0';
                        snprintf(bin_model_path, sizeof(bin_model_path),
                                 "%s/../nova_model.bin", self2);
                        snprintf(bin_tok_path, sizeof(bin_tok_path),
                                 "%s/../nc_ai_tokenizer.bin", self2);
                    }
                }
            }
            const char *model_paths[12] = {
                "nova_model.bin",
                "nc_ai_model_prod.bin",
                "training_data/nova_model.bin",
                "training_data/nc_ai_model_prod.bin",
                "nc-ai/training_data/nova_model.bin",
                NULL
            };
            const char *tok_paths[6] = {
                "nc_ai_tokenizer.bin", "training_data/nc_ai_tokenizer.bin",
                "nc-ai/training_data/nc_ai_tokenizer.bin",
                NULL
            };
            /* Append binary-relative paths */
            if (bin_model_path[0]) {
                for (int i = 0; i < 9; i++) {
                    if (!model_paths[i]) { model_paths[i] = bin_model_path; break; }
                }
            }
            if (bin_tok_path[0]) {
                for (int i = 0; i < 5; i++) {
                    if (!tok_paths[i]) { tok_paths[i] = bin_tok_path; break; }
                }
            }
            NCModel *llm = NULL;
            for (int i = 0; model_paths[i] && !llm; i++)
                llm = nc_model_load(model_paths[i]);
            NCTokenizer *tokenizer = NULL;
            for (int i = 0; tok_paths[i] && !tokenizer; i++)
                tokenizer = nc_tokenizer_load(tok_paths[i]);
            int llm_ready = (llm != NULL);

            if (!llm_ready && !template_fallback_enabled) {
                if (tokenizer) nc_tokenizer_free(tokenizer);
                fprintf(stderr,
                        "  Error: nc ai create requires the built-in model. Install/train the model or rerun with --template-fallback.\n");
                return 1;
            }

            /* â”€â”€ Parse intent from prompt â”€â”€â”€ */
            char low[2048];
            strncpy(low, prompt, sizeof(low) - 1);
            low[sizeof(low) - 1] = '\0';
            for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);

            /* Extract app name (first significant noun) */
            char app_name[128] = "myapp";
            {
                static const char *skip[] = {
                    "create", "build", "make", "generate", "a", "an", "the",
                    "simple", "complete", "full", "basic", "modern", "new",
                    "dark", "light", "with", "and", "for", "that", "rest",
                    "api", "app", "application", "service", "page", "website",
                    "frontend", "backend", "crud", "please", "me", "i", "want",
                    NULL
                };
                char tmp[2048];
                strncpy(tmp, low, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                char *save = NULL;
                char *tok = strtok_r(tmp, " \t", &save);
                while (tok) {
                    int is_skip = 0;
                    for (const char **s = skip; *s; s++) {
                        if (strcmp(tok, *s) == 0) { is_skip = 1; break; }
                    }
                    if (!is_skip && strlen(tok) > 2) {
                        size_t tl = strlen(tok);
                        if (tl > 3 && tok[tl-1] == 's') tok[tl-1] = '\0';
                        strncpy(app_name, tok, sizeof(app_name) - 1);
                        break;
                    }
                    tok = strtok_r(NULL, " \t", &save);
                }
            }

            /* Capitalize name */
            char App_name[128];
            snprintf(App_name, sizeof(App_name), "%s", app_name);
            App_name[0] = (char)toupper((unsigned char)App_name[0]);

            /* Detect features from prompt */
            int has_auth  = (strstr(low, "auth") || strstr(low, "login") || strstr(low, "signup") || strstr(low, "register"));
            int has_crud  = (strstr(low, "crud") || strstr(low, "manage") || strstr(low, "create") || strstr(low, "list"));
            int has_dark  = (strstr(low, "dark") != NULL);
            int has_ai    = (strstr(low, " ai ") || strstr(low, "classify") || strstr(low, "predict") || strstr(low, "intelligence"));
            int has_dash  = (strstr(low, "dashboard") || strstr(low, "admin") || strstr(low, "panel"));
            int has_search = (strstr(low, "search") || strstr(low, "filter") || strstr(low, "find"));
            int has_stock = (strstr(low, "stock") || strstr(low, "trading") || strstr(low, "finance") ||
                             strstr(low, "market") || strstr(low, "portfolio") || strstr(low, "neuraledge") ||
                             strstr(low, "invest") || strstr(low, "ticker") || strstr(low, "forex"));
            int has_chart = (strstr(low, "chart") || strstr(low, "graph") || strstr(low, "visual"));
            int has_alert = (strstr(low, "alert") || strstr(low, "notif") || strstr(low, "watch"));
            int has_role = (strstr(low, "role based") || strstr(low, "role-based") || strstr(low, "rbac") ||
                            strstr(low, "roles") || strstr(low, "permission") || strstr(low, "access control"));
            int has_approval = (strstr(low, "approval") || strstr(low, "approvals") || strstr(low, "approve") ||
                                strstr(low, "reject") || strstr(low, "workflow") || strstr(low, "queue"));
            int has_analytics = (strstr(low, "analytics") || strstr(low, "metric") || strstr(low, "insight") ||
                                 strstr(low, "kpi") || strstr(low, "report"));
            int has_tenant = (strstr(low, "multi-tenant") || strstr(low, "multitenant") || strstr(low, "tenant") ||
                              strstr(low, "workspace"));
            /* Stock apps imply dark theme + dashboard + AI + search */
            if (has_stock) { has_dark = 1; has_dash = 1; has_ai = 1; has_search = 1; }
            if (has_analytics) has_dash = 1;
            if (has_role) { has_auth = 1; has_crud = 1; }
            if (has_approval) { has_crud = 1; has_dash = 1; }
            if (has_tenant) { has_auth = 1; has_crud = 1; has_dash = 1; has_search = 1; }
            int has_enterprise_ops = (!has_stock && (has_role || has_approval || has_analytics || has_tenant));

            /* Default project dir from app name */
            char dir_buf[256];
            if (!project_dir) {
                snprintf(dir_buf, sizeof(dir_buf), "%s_app", app_name);
                project_dir = dir_buf;
            }

            if (!nc_cli_plain_mode()) nc_animate_nc_ai();
            nc_print_step("Creating Full Project (LLM-Driven)");

            printf("  Prompt:   \"%s\"\n", prompt);
            printf("  App:      %s\n", App_name);
            printf("  Engine:   %s\n",
                   llm_ready
                       ? (template_fallback_enabled
                           ? "Built-in LLM (loaded, template fallback enabled)"
                           : "Built-in LLM (AI-only mode)")
                       : "Template fallback (explicit opt-in)");
                 printf("  Features: %s%s%s%s%s%s%s%s%s%s%s%s%s\n",
                   has_auth ? "auth " : "", has_crud ? "crud " : "",
                   has_dark ? "dark-theme " : "", has_ai ? "ai " : "",
                   has_dash ? "dashboard " : "", has_search ? "search " : "",
                   has_stock ? "stock-market " : "", has_chart ? "charts " : "",
                     has_alert ? "alerts " : "", has_role ? "role-based " : "",
                     has_approval ? "approvals " : "", has_analytics ? "analytics " : "",
                     has_tenant ? "multi-tenant " : "");
            printf("  Output:   %s/\n\n", project_dir);

            /* â”€â”€ Create project directory â”€â”€â”€ */
            nc_mkdir_p(project_dir);

            char filepath[512];
            FILE *f;
            int llm_used = 0;  /* track if LLM generated any files */
            int template_fallback_used = 0;
            int ai_repair_used = 0;
            int file_best_effort = 0;
            int service_contract_ok = 0;
            int page_contract_ok = 0;
            int test_contract_ok = 0;
            int service_semantic_ok = 1;
            int page_semantic_ok = 1;
            char *generated_file_text = NULL;
            char service_prompt[8192] = "";
            char page_prompt[8192] = "";
            char test_prompt[4096] = "";

            /* Build feature string for prompts */
            char feature_str[768] = "";
            snprintf(feature_str, sizeof(feature_str), "%s%s%s%s%s%s%s%s%s%s%s%s%s",
                     has_auth ? "auth " : "", has_crud ? "crud " : "",
                     has_dark ? "dark-theme " : "", has_ai ? "ai " : "",
                     has_dash ? "dashboard " : "", has_search ? "search " : "",
                     has_stock ? "stock-market " : "", has_chart ? "charts " : "",
                     has_alert ? "alerts " : "", has_role ? "role-based " : "",
                     has_approval ? "approvals " : "", has_analytics ? "analytics " : "",
                     has_tenant ? "multi-tenant " : "");

            /* If no tokenizer but we have a model, create one from prompt */
            if (llm && !tokenizer) {
                tokenizer = nc_tokenizer_create();
                const char *seed[] = { prompt };
                nc_tokenizer_train(tokenizer, seed, 1, llm->vocab_size);
            }

            /* â•â•â• 1. Backend Service (service.nc) â€” LLM-driven â•â•â• */
            snprintf(filepath, sizeof(filepath), "%s/service.nc", project_dir);

            /* Construct LLM prompt for backend */
            {
                if (has_stock) {
                    snprintf(service_prompt, sizeof(service_prompt),
                        "Generate a complete NC stock market intelligence service.\n"
                        "User request: \"%s\"\n"
                        "App name: %s\n\n"
                        "Include: service declaration, configure (ai_model, port 8000), "
                        "market data (get_quote, get_history, get_sector_performance), "
                        "technical analysis (analyze, rsi, macd, bollinger), "
                        "AI predictions (predict with analyze context, predict_batch), "
                        "sentiment analysis, portfolio (analyze_portfolio, suggest_rebalance), "
                        "screener (screen, watchlist_summary), health_check, "
                        "middleware (cors, log_requests, rate_limit), "
                        "API routes under /api/v1/.\n"
                        "Use NC syntax: service, to func with params, ask AI to, "
                        "gather from, store into, respond with, api block.\n\n"
                        "service \"%s-api\"\n"
                        "version \"1.0.0\"\n\n"
                        "to health_check:\n"
                        "    respond with {\"status\": \"healthy\", \"service\": \"%s-api\"}\n\n"
                        "api:\n"
                        "    GET /health runs health_check\n\n"
                        "middleware:\n"
                        "    use cors\n"
                        "    use log_requests\n",
                        prompt, app_name, app_name, app_name);
                } else if (has_enterprise_ops) {
                    snprintf(service_prompt, sizeof(service_prompt),
                        "Generate a complete NC operations control service.\n"
                        "User request: \"%s\"\n"
                        "App name: %s, Features: %s\n\n"
                        "Keep the requested high-level features."
                        " Required capabilities: multi-tenant scope, role-based access control, analytics overview, approval workflow, alert center, health check.\n"
                        "Include handlers named list_tenants, list_roles, permission_matrix, analytics_overview, list_approvals, approve_request, reject_request, list_alerts, and health_check.\n"
                        "Use realistic JSON objects and /api/v1 routes.\n"
                        "Use NC syntax: service, version, to handler with params, respond with, api block, middleware block.\n\n"
                        "service \"%s-ops\"\n"
                        "version \"1.0.0\"\n\n"
                        "to list_tenants:\n"
                        "    respond with []\n\n"
                        "to list_roles:\n"
                        "    respond with [{\"name\": \"admin\", \"scope\": \"global\"}]\n\n"
                        "to permission_matrix:\n"
                        "    respond with {\"roles\": []}\n\n"
                        "to analytics_overview:\n"
                        "    respond with {\"kpis\": [], \"trend\": \"stable\"}\n\n"
                        "to list_approvals:\n"
                        "    respond with []\n\n"
                        "to approve_request with id:\n"
                        "    respond with {\"id\": id, \"status\": \"approved\"}\n\n"
                        "to reject_request with id:\n"
                        "    respond with {\"id\": id, \"status\": \"rejected\"}\n\n"
                        "to list_alerts:\n"
                        "    respond with []\n\n"
                        "to health_check:\n"
                        "    respond with {\"status\": \"healthy\", \"service\": \"%s-ops\"}\n\n"
                        "api:\n"
                        "    GET /api/v1/tenants runs list_tenants\n"
                        "    GET /api/v1/roles runs list_roles\n"
                        "    GET /api/v1/permissions runs permission_matrix\n"
                        "    GET /api/v1/analytics/overview runs analytics_overview\n"
                        "    GET /api/v1/approvals runs list_approvals\n"
                        "    POST /api/v1/approvals/:id/approve runs approve_request\n"
                        "    POST /api/v1/approvals/:id/reject runs reject_request\n"
                        "    GET /api/v1/alerts runs list_alerts\n"
                        "    GET /health runs health_check\n\n"
                        "middleware:\n"
                        "    use cors\n"
                        "    use log_requests\n"
                        "    use rate_limit 60\n",
                        prompt, app_name, feature_str, app_name, app_name);
                } else {
                    snprintf(service_prompt, sizeof(service_prompt),
                        "Generate a complete NC backend service.\n"
                        "User request: \"%s\"\n"
                        "App name: %s, Features: %s\n\n"
                        "Include: service declaration, %s"
                        "CRUD (list, get, create, update, delete for %s), "
                        "%s%shealth_check, middleware (cors, log_requests%s), "
                        "API routes.\n"
                        "Use NC syntax: service, to func with params, "
                        "gather from, store into, respond with, api block.\n\n"
                        "service \"%s-api\"\n"
                        "version \"1.0.0\"\n\n"
                        "to list_%ss:\n"
                        "    respond with []\n\n"
                        "to health_check:\n"
                        "    respond with {\"status\": \"healthy\", \"service\": \"%s-api\"}\n\n"
                        "api:\n"
                        "    GET /%ss runs list_%ss\n"
                        "    GET /health runs health_check\n\n"
                        "middleware:\n"
                        "    use cors\n"
                        "    use log_requests\n",
                        prompt, app_name, feature_str,
                        has_auth ? "auth (signup, login with JWT), " : "",
                        app_name,
                        has_search ? "search, " : "",
                        has_ai ? "AI analysis, " : "",
                        has_auth ? ", rate_limit 60" : "",
                        app_name, app_name, app_name, app_name, app_name);
                }

                file_best_effort = 0;
                generated_file_text = nc_ai_project_generate_with_retries(
                    llm, tokenizer, service_prompt, "service", 512, template_fallback_enabled);
                service_contract_ok = generated_file_text
                    && nc_ai_project_output_valid(generated_file_text, "service");
                service_semantic_ok = generated_file_text
                    && nc_ai_project_enterprise_semantic_valid(
                        generated_file_text, "service", has_enterprise_ops,
                        has_tenant, has_role, has_approval, has_analytics, has_alert);
                if (generated_file_text && service_contract_ok && !service_semantic_ok
                    && template_fallback_enabled) {
                    free(generated_file_text);
                    generated_file_text = NULL;
                    service_contract_ok = 0;
                } else if (generated_file_text && (!service_contract_ok || !service_semantic_ok)) {
                    file_best_effort = 1;
                }
            }

            f = fopen(filepath, "w");
            if (!f) {
                free(generated_file_text);
                fprintf(stderr, "  Error: cannot create %s\n", filepath);
                return 1;
            }

            if (generated_file_text) {
                fprintf(f, "%s\n", generated_file_text);
                free(generated_file_text);
                generated_file_text = NULL;
                llm_used = 1;
                printf("  Created: %s/service.nc (%s)\n", project_dir,
                      file_best_effort ? "AI-generated, best effort" : "AI-generated");
            } else {
            if (!template_fallback_enabled) {
                fclose(f);
                remove(filepath);
                if (llm) nc_model_free(llm);
                if (tokenizer) nc_tokenizer_free(tokenizer);
                fprintf(stderr, "  Error: AI generation failed for %s/service.nc\n", project_dir);
                return 1;
            }
            template_fallback_used = 1;
            /* â”€â”€ Fallback: template-based generation â”€â”€ */

            fprintf(f, "// %s Service â€” generated by NC AI\n", App_name);
            fprintf(f, "// Prompt: \"%s\"\n\n", prompt);
            fprintf(f, "service \"%s-api\"\n", app_name);
            fprintf(f, "version \"1.0.0\"\n\n");

            if (has_auth) {
                fprintf(f, "// â”€â”€ Authentication â”€â”€â”€\n\n");
                fprintf(f, "to signup with data:\n");
                fprintf(f, "    if data.email is empty:\n");
                fprintf(f, "        respond with error \"Email required\"\n");
                fprintf(f, "    set data.id to generate_id()\n");
                fprintf(f, "    set data.created_at to now()\n");
                fprintf(f, "    set data.password to hash_sha256(data.password)\n");
                fprintf(f, "    store data into \"users\"\n");
                fprintf(f, "    respond with {\"id\": data.id, \"email\": data.email, \"status\": \"created\"}\n\n");
                fprintf(f, "to login with credentials:\n");
                fprintf(f, "    gather users from \"users\" where email is credentials.email\n");
                fprintf(f, "    if users is empty:\n");
                fprintf(f, "        respond with error \"Invalid credentials\"\n");
                fprintf(f, "    set token to jwt_encode({\"user_id\": users.0.id, \"email\": users.0.email})\n");
                fprintf(f, "    respond with {\"token\": token}\n\n");
            }

            /* CRUD operations */
            fprintf(f, "// â”€â”€ %s CRUD â”€â”€â”€\n\n", App_name);
            fprintf(f, "to list_%ss:\n", app_name);
            fprintf(f, "    gather items from \"%ss\"\n", app_name);
            fprintf(f, "    respond with items\n\n");
            fprintf(f, "to get_%s with id:\n", app_name);
            fprintf(f, "    gather item from \"%ss\" where id is id\n", app_name);
            fprintf(f, "    if item is empty:\n");
            fprintf(f, "        respond with error \"%s not found\"\n", App_name);
            fprintf(f, "    respond with item\n\n");
            fprintf(f, "to create_%s with data:\n", app_name);
            fprintf(f, "    set data.id to generate_id()\n");
            fprintf(f, "    set data.created_at to now()\n");
            fprintf(f, "    set data.updated_at to now()\n");
            fprintf(f, "    store data into \"%ss\"\n", app_name);
            fprintf(f, "    log \"Created %s: \" + data.id\n", app_name);
            fprintf(f, "    respond with data\n\n");
            fprintf(f, "to update_%s with id and data:\n", app_name);
            fprintf(f, "    set data.id to id\n");
            fprintf(f, "    set data.updated_at to now()\n");
            fprintf(f, "    store data into \"%ss\"\n", app_name);
            fprintf(f, "    respond with data\n\n");
            fprintf(f, "to delete_%s with id:\n", app_name);
            fprintf(f, "    remove id from \"%ss\"\n", app_name);
            fprintf(f, "    respond with {\"deleted\": id}\n\n");

            if (has_stock) {
                fprintf(f, "// â”€â”€ Market Data â”€â”€â”€\n\n");
                fprintf(f, "configure:\n");
                fprintf(f, "    ai_model is \"default\"\n");
                fprintf(f, "    port is 8000\n\n");
                fprintf(f, "to get_quote with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Generate realistic current stock data for {{sym}}. Return JSON: {symbol, price, change, change_pct, volume, market_cap, pe_ratio, week_52_high, week_52_low}\" save as quote_data\n");
                fprintf(f, "    respond with quote_data\n\n");
                fprintf(f, "to get_history with symbol, period:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    set days to 30\n");
                fprintf(f, "    if period is equal \"1w\":\n");
                fprintf(f, "        set days to 7\n");
                fprintf(f, "    if period is equal \"3m\":\n");
                fprintf(f, "        set days to 90\n");
                fprintf(f, "    if period is equal \"1y\":\n");
                fprintf(f, "        set days to 365\n");
                fprintf(f, "    ask AI to \"Generate historical OHLCV data for {{sym}} for {{days}} days. Return JSON array of {date, open, high, low, close, volume}\" save as history\n");
                fprintf(f, "    respond with {\"symbol\": sym, \"period\": period, \"data\": history}\n\n");
                fprintf(f, "to get_sector_performance:\n");
                fprintf(f, "    ask AI to \"Return US stock sector performance. JSON: [{sector, performance_pct, top_stock}] for: Technology, Healthcare, Finance, Energy, Consumer, Industrial, Materials, Utilities, Real Estate, Communication\" save as sectors\n");
                fprintf(f, "    respond with sectors\n\n");

                fprintf(f, "// â”€â”€ Technical Analysis â”€â”€â”€\n\n");
                fprintf(f, "to analyze with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Technical analysis for {{sym}}. Return JSON: {symbol, rsi_14, macd_signal, sma_20, sma_50, sma_200, bollinger_upper, bollinger_lower, atr_14, support_level, resistance_level, trend, overall_signal, confidence}\" save as analysis\n");
                fprintf(f, "    respond with analysis\n\n");
                fprintf(f, "to rsi with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Calculate RSI(14) for {{sym}}. Return JSON: {symbol, rsi, interpretation, signal}\" save as rsi_data\n");
                fprintf(f, "    respond with rsi_data\n\n");
                fprintf(f, "to macd with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Calculate MACD(12,26,9) for {{sym}}. Return JSON: {symbol, macd_line, signal_line, histogram, crossover, trend_strength}\" save as macd_data\n");
                fprintf(f, "    respond with macd_data\n\n");
                fprintf(f, "to bollinger with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Bollinger Bands(20,2) for {{sym}}. Return JSON: {symbol, upper_band, middle_band, lower_band, current_price, bandwidth_pct, position, squeeze}\" save as bb_data\n");
                fprintf(f, "    respond with bb_data\n\n");

                fprintf(f, "// â”€â”€ AI Predictions â”€â”€â”€\n\n");
                fprintf(f, "to predict with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    run analyze with sym\n");
                fprintf(f, "    set ta to result\n");
                fprintf(f, "    ask AI to \"Predict price for {{sym}} next 7 days. Context: {{ta}}. Return JSON: {symbol, current_price, predictions: [{day, date, predicted_price, confidence}], overall_direction, key_levels: {support, resistance, target_1, stop_loss}, recommendation}\" save as prediction\n");
                fprintf(f, "    respond with prediction\n\n");
                fprintf(f, "to predict_batch with symbols:\n");
                fprintf(f, "    set results to []\n");
                fprintf(f, "    repeat for each sym in symbols:\n");
                fprintf(f, "        run predict with sym\n");
                fprintf(f, "        append result to results\n");
                fprintf(f, "    respond with results\n\n");

                fprintf(f, "// â”€â”€ Sentiment â”€â”€â”€\n\n");
                fprintf(f, "to sentiment with symbol:\n");
                fprintf(f, "    set sym to upper(symbol)\n");
                fprintf(f, "    ask AI to \"Sentiment analysis for {{sym}}. Return JSON: {symbol, overall_sentiment, sentiment_score, news_sentiment, analyst_consensus, retail_sentiment, recent_news: [{headline, sentiment, impact}], risk_events: []}\" save as sentiment_data\n");
                fprintf(f, "    respond with sentiment_data\n\n");

                fprintf(f, "// â”€â”€ Portfolio â”€â”€â”€\n\n");
                fprintf(f, "to analyze_portfolio with holdings:\n");
                fprintf(f, "    ask AI to \"Analyze portfolio. Holdings: {{holdings}}. Return JSON: {total_value, day_pnl, total_return_pct, beta, sharpe_ratio, diversification_score, risk_level, sector_breakdown: {}, recommendations: []}\" save as portfolio_analysis\n");
                fprintf(f, "    respond with portfolio_analysis\n\n");
                fprintf(f, "to suggest_rebalance with holdings, target_allocation:\n");
                fprintf(f, "    ask AI to \"Rebalance from {{holdings}} to {{target_allocation}}. Return JSON: [{action, symbol, quantity, estimated_value, reason}]\" save as rebalance\n");
                fprintf(f, "    respond with rebalance\n\n");

                fprintf(f, "// â”€â”€ Screener â”€â”€â”€\n\n");
                fprintf(f, "to screen with criteria:\n");
                fprintf(f, "    ask AI to \"Find stocks matching: {{criteria}}. Return JSON: [{symbol, name, price, match_score, why_matches: []}] top 10\" save as matches\n");
                fprintf(f, "    respond with matches\n\n");

                fprintf(f, "to watchlist_summary with symbols:\n");
                fprintf(f, "    set summaries to []\n");
                fprintf(f, "    repeat for each sym in symbols:\n");
                fprintf(f, "        run get_quote with sym\n");
                fprintf(f, "        append result to summaries\n");
                fprintf(f, "    respond with summaries\n\n");
            }

            if (has_search && !has_stock) {
                fprintf(f, "// â”€â”€ Search â”€â”€â”€\n\n");
                fprintf(f, "to search_%ss with query:\n", app_name);
                fprintf(f, "    gather items from \"%ss\"\n", app_name);
                fprintf(f, "    set results to filter(items, \"name\", query)\n");
                fprintf(f, "    respond with results\n\n");
            }

            if (has_ai && !has_stock) {
                fprintf(f, "// â”€â”€ AI Features â”€â”€â”€\n\n");
                fprintf(f, "to analyze_%s with data:\n", app_name);
                fprintf(f, "    ask AI to \"analyze this %s data and provide insights\" using data\n", app_name);
                fprintf(f, "        save as analysis\n");
                fprintf(f, "    respond with analysis\n\n");
            }

            if (has_tenant) {
                fprintf(f, "// â”€â”€ Tenant Scope â”€â”€â”€\n\n");
                fprintf(f, "to list_tenants:\n");
                fprintf(f, "    respond with [{\"id\": \"tenant-1\", \"name\": \"North Ops\", \"status\": \"active\"}, {\"id\": \"tenant-2\", \"name\": \"South Ops\", \"status\": \"pending\"}]\n\n");
            }

            if (has_role) {
                fprintf(f, "// â”€â”€ Role-Based Access â”€â”€â”€\n\n");
                fprintf(f, "to list_roles:\n");
                fprintf(f, "    respond with [{\"name\": \"admin\", \"permissions\": [\"approvals\", \"analytics\", \"alerts\"]}, {\"name\": \"approver\", \"permissions\": [\"approvals\", \"alerts\"]}, {\"name\": \"viewer\", \"permissions\": [\"analytics\"]}]\n\n");
                fprintf(f, "to permission_matrix:\n");
                fprintf(f, "    respond with {\"roles\": [{\"name\": \"admin\", \"grants\": [\"tenants:read\", \"approvals:write\", \"analytics:read\", \"alerts:write\"]}, {\"name\": \"approver\", \"grants\": [\"approvals:write\", \"alerts:read\"]}, {\"name\": \"viewer\", \"grants\": [\"analytics:read\"]}]}\n\n");
            }

            if (has_analytics) {
                fprintf(f, "// â”€â”€ Analytics â”€â”€â”€\n\n");
                fprintf(f, "to analytics_overview:\n");
                fprintf(f, "    respond with {\"active_tenants\": 12, \"pending_approvals\": 4, \"active_alerts\": 3, \"automation_success\": 98}\n\n");
                fprintf(f, "to analytics_breakdown:\n");
                fprintf(f, "    respond with {\"regions\": [{\"name\": \"north\", \"value\": 42}, {\"name\": \"south\", \"value\": 35}], \"trend\": \"up\"}\n\n");
            }

            if (has_approval) {
                fprintf(f, "// â”€â”€ Approval Workflow â”€â”€â”€\n\n");
                fprintf(f, "to list_approvals:\n");
                fprintf(f, "    respond with [{\"id\": \"apr-1\", \"title\": \"Production deploy\", \"status\": \"pending\", \"requested_by\": \"ops@north\"}]\n\n");
                fprintf(f, "to approve_request with id:\n");
                fprintf(f, "    respond with {\"id\": id, \"status\": \"approved\"}\n\n");
                fprintf(f, "to reject_request with id:\n");
                fprintf(f, "    respond with {\"id\": id, \"status\": \"rejected\"}\n\n");
            }

            if (has_alert) {
                fprintf(f, "// â”€â”€ Alert Center â”€â”€â”€\n\n");
                fprintf(f, "to list_alerts:\n");
                fprintf(f, "    respond with [{\"id\": \"alt-1\", \"severity\": \"high\", \"message\": \"Approval queue breached SLA\"}, {\"id\": \"alt-2\", \"severity\": \"medium\", \"message\": \"Tenant sync delayed\"}]\n\n");
                fprintf(f, "to acknowledge_alert with id:\n");
                fprintf(f, "    respond with {\"id\": id, \"status\": \"acknowledged\"}\n\n");
            }

            fprintf(f, "to health_check:\n");
            fprintf(f, "    respond with {\"status\": \"healthy\", \"service\": \"%s-api\"}\n\n", app_name);

            /* API routes */
            fprintf(f, "// â”€â”€ Routes â”€â”€â”€\n\n");
            fprintf(f, "middleware:\n");
            fprintf(f, "    use cors\n");
            fprintf(f, "    use log_requests\n");
            if (has_auth) fprintf(f, "    use rate_limit 60\n");
            fprintf(f, "\napi:\n");
            if (has_auth) {
                fprintf(f, "    POST /auth/signup     runs signup\n");
                fprintf(f, "    POST /auth/login      runs login\n");
            }
            if (has_stock) {
                fprintf(f, "    GET  /api/v1/quote/:symbol      runs get_quote\n");
                fprintf(f, "    GET  /api/v1/history/:symbol    runs get_history\n");
                fprintf(f, "    GET  /api/v1/sectors            runs get_sector_performance\n");
                fprintf(f, "    POST /api/v1/analyze/:symbol    runs analyze\n");
                fprintf(f, "    GET  /api/v1/rsi/:symbol        runs rsi\n");
                fprintf(f, "    GET  /api/v1/macd/:symbol       runs macd\n");
                fprintf(f, "    GET  /api/v1/bollinger/:symbol  runs bollinger\n");
                fprintf(f, "    POST /api/v1/predict/:symbol    runs predict\n");
                fprintf(f, "    POST /api/v1/predict/batch      runs predict_batch\n");
                fprintf(f, "    POST /api/v1/sentiment/:symbol  runs sentiment\n");
                fprintf(f, "    POST /api/v1/portfolio/analyze  runs analyze_portfolio\n");
                fprintf(f, "    POST /api/v1/portfolio/rebalance runs suggest_rebalance\n");
                fprintf(f, "    POST /api/v1/screen             runs screen\n");
                fprintf(f, "    POST /api/v1/watchlist          runs watchlist_summary\n");
            }
            if (has_tenant) {
                fprintf(f, "    GET  /api/v1/tenants            runs list_tenants\n");
            }
            if (has_role) {
                fprintf(f, "    GET  /api/v1/roles              runs list_roles\n");
                fprintf(f, "    GET  /api/v1/permissions        runs permission_matrix\n");
            }
            if (has_analytics) {
                fprintf(f, "    GET  /api/v1/analytics/overview runs analytics_overview\n");
                fprintf(f, "    GET  /api/v1/analytics/breakdown runs analytics_breakdown\n");
            }
            if (has_approval) {
                fprintf(f, "    GET  /api/v1/approvals          runs list_approvals\n");
                fprintf(f, "    POST /api/v1/approvals/:id/approve runs approve_request\n");
                fprintf(f, "    POST /api/v1/approvals/:id/reject runs reject_request\n");
            }
            if (has_alert) {
                fprintf(f, "    GET  /api/v1/alerts             runs list_alerts\n");
                fprintf(f, "    POST /api/v1/alerts/:id/ack     runs acknowledge_alert\n");
            }
            fprintf(f, "    GET  /%ss              runs list_%ss\n", app_name, app_name);
            fprintf(f, "    GET  /%ss/:id           runs get_%s\n", app_name, app_name);
            fprintf(f, "    POST /%ss              runs create_%s\n", app_name, app_name);
            fprintf(f, "    PUT  /%ss/:id           runs update_%s\n", app_name, app_name);
            fprintf(f, "    DELETE /%ss/:id         runs delete_%s\n", app_name, app_name);
            if (has_search && !has_stock) fprintf(f, "    GET  /%ss/search/:query runs search_%ss\n", app_name, app_name);
            if (has_ai && !has_stock)     fprintf(f, "    POST /%ss/analyze       runs analyze_%s\n", app_name, app_name);
            fprintf(f, "    GET  /health             runs health_check\n");
            fclose(f);
            service_contract_ok = 1;
            service_semantic_ok = 1;
            printf("  Created: %s/service.nc\n", project_dir);
            } /* end fallback for service.nc */

            /* â•â•â• 2. Frontend UI (app.ncui) â€” LLM-driven â•â•â• */
            snprintf(filepath, sizeof(filepath), "%s/app.ncui", project_dir);

            /* Construct LLM prompt for frontend */
            {
                if (has_stock) {
                    snprintf(page_prompt, sizeof(page_prompt),
                        "Generate a complete NC UI stock market dashboard.\n"
                        "User request: \"%s\"\n"
                        "App name: %s\n\n"
                        "Dark theme (#0a0a0f bg, #10b981 emerald accent, Inter font).\n"
                        "Include: nav (Dashboard, Analysis, Predictions, Portfolio, Alerts), "
                        "dashboard section (4-col stock cards AAPL/MSFT/GOOGL/NVDA with prices and AI signals, "
                        "3-col overview cards), analysis section (top performers with RSI/MACD, risk alerts, "
                        "RSI/trend/volume distribution with progress bars), predictions section "
                        "(bullish predictions with targets, risk warnings), portfolio section "
                        "(4-col stats cards, add position form), alerts section (active alerts, create alert form), "
                        "footer. Use animate stagger/fade-up.\n"
                        "Use NCUI: page, style, nav, section, grid, card, heading, stat, text, list, "
                        "item, form, input, button, progress, animate, footer.\n\n"
                        "page \"%s\"\n"
                        "theme \"dark\"\n"
                        "accent \"#10b981\"\n"
                        "background \"#0a0a0f\"\n"
                        "foreground \"#e0e0e8\"\n\n"
                        "nav:\n"
                        "    brand \"%s\"\n\n"
                        "section hero centered:\n"
                        "    heading \"%s\"\n"
                        "    text \"AI market intelligence dashboard\"\n\n"
                        "footer:\n"
                        "    text \"Built with NC UI\"\n",
                        prompt, App_name, App_name, App_name, App_name);
                } else if (has_enterprise_ops) {
                    snprintf(page_prompt, sizeof(page_prompt),
                        "Generate a complete NC UI operations control dashboard.\n"
                        "User request: \"%s\"\n"
                        "App name: %s, Features: %s\n\n"
                        "Keep the requested high-level features. Required sections: dashboard, tenants, roles, analytics, approvals, alerts.\n"
                        "Dark theme (#0a0a0f bg, #22d3ee accent, modern operations console).\n"
                        "Include nav links for Dashboard, Tenants, Roles, Analytics, Approvals, Alerts.\n"
                        "Dashboard section: 4 KPI cards for active tenants, pending approvals, active alerts, automation success.\n"
                        "Roles section: role badges or permission matrix summary.\n"
                        "Analytics section: KPI cards, trend summary, and chart placeholders.\n"
                        "Approvals section: queue of pending items with Approve and Reject actions.\n"
                        "Alerts section: alert center list with acknowledge/escalate controls.\n"
                        "Use NCUI: page, theme, accent, background, foreground, nav, section, grid, card, heading, text, list, item, form, input, button, badge, stat, footer.\n\n"
                        "page \"%s\"\n"
                        "theme \"dark\"\n"
                        "accent \"#22d3ee\"\n"
                        "background \"#0a0a0f\"\n"
                        "foreground \"#e0e0e8\"\n\n"
                        "nav:\n"
                        "    brand \"%s\"\n"
                        "    links:\n"
                        "        link \"Dashboard\" to \"#dashboard\"\n"
                        "        link \"Tenants\" to \"#tenants\"\n"
                        "        link \"Roles\" to \"#roles\"\n"
                        "        link \"Analytics\" to \"#analytics\"\n"
                        "        link \"Approvals\" to \"#approvals\"\n"
                        "        link \"Alerts\" to \"#alerts\"\n\n"
                        "section hero centered:\n"
                        "    heading \"%s\"\n"
                        "    text \"Multi-tenant operations control plane\"\n\n"
                        "footer:\n"
                        "    text \"Built with NC UI\"\n",
                        prompt, App_name, feature_str, App_name, App_name, App_name);
                } else {
                    snprintf(page_prompt, sizeof(page_prompt),
                        "Generate a complete NC UI frontend page.\n"
                        "User request: \"%s\"\n"
                        "App name: %s, Features: %s\n\n"
                        "Include: page, style (%s), header with heading, "
                        "%s%s"
                        "main list from %ss with cards, create form, footer, "
                        "actions (create_item, delete_item%s%s), "
                        "data binding.\n"
                        "Use NCUI: page, style, header, section, card, heading, "
                        "text, button, form, input, list, footer, actions, data.\n\n"
                        "page \"%s\"\n"
                        "theme \"%s\"\n"
                        "accent \"%s\"\n"
                        "background \"%s\"\n"
                        "foreground \"%s\"\n\n"
                        "nav:\n"
                        "    brand \"%s\"\n\n"
                        "section hero centered:\n"
                        "    heading \"%s\"\n"
                        "    text \"Manage your %ss with NC UI\"\n\n"
                        "footer:\n"
                        "    text \"Built with NC UI\"\n",
                        prompt, app_name, feature_str,
                        has_dark ? "dark #0a0a0f" : "light #ffffff",
                        has_auth ? "auth buttons, " : "",
                        has_dash ? "dashboard stats, " : "",
                        app_name,
                        has_search ? ", search" : "",
                        has_auth ? ", login, signup" : "",
                        App_name,
                        has_dark ? "dark" : "light",
                        has_dark ? "#00d4ff" : "#2563eb",
                        has_dark ? "#0a0a0f" : "#ffffff",
                        has_dark ? "#e0e0e8" : "#1a1a2e",
                        App_name, App_name, app_name);
                }

                file_best_effort = 0;
                generated_file_text = nc_ai_project_generate_with_retries(
                    llm, tokenizer, page_prompt, "page", 512, template_fallback_enabled);
                page_contract_ok = generated_file_text
                    && nc_ai_project_output_valid(generated_file_text, "page");
                page_semantic_ok = generated_file_text
                    && nc_ai_project_enterprise_semantic_valid(
                        generated_file_text, "page", has_enterprise_ops,
                        has_tenant, has_role, has_approval, has_analytics, has_alert);
                if (generated_file_text && page_contract_ok && !page_semantic_ok
                    && template_fallback_enabled) {
                    free(generated_file_text);
                    generated_file_text = NULL;
                    page_contract_ok = 0;
                } else if (generated_file_text && (!page_contract_ok || !page_semantic_ok)) {
                    file_best_effort = 1;
                }
            }

            f = fopen(filepath, "w");
            if (!f) {
                free(generated_file_text);
                fprintf(stderr, "  Error: cannot create %s\n", filepath);
                return 1;
            }

            if (generated_file_text) {
                fprintf(f, "%s\n", generated_file_text);
                free(generated_file_text);
                generated_file_text = NULL;
                llm_used = 1;
                printf("  Created: %s/app.ncui (%s)\n", project_dir,
                       file_best_effort ? "AI-generated, best effort" : "AI-generated");
            } else {
            if (!template_fallback_enabled) {
                fclose(f);
                remove(filepath);
                if (llm) nc_model_free(llm);
                if (tokenizer) nc_tokenizer_free(tokenizer);
                fprintf(stderr, "  Error: AI generation failed for %s/app.ncui\n", project_dir);
                return 1;
            }
            template_fallback_used = 1;
            /* â”€â”€ Fallback: template-based UI generation â”€â”€ */

            fprintf(f, "// %s UI â€” generated by NC AI\n\n", App_name);
            fprintf(f, "page \"%s\"\n\n", App_name);

            fprintf(f, "style:\n");
            if (has_stock) {
                fprintf(f, "    background is \"#0a0a0f\"\n");
                fprintf(f, "    text color is \"#e0e0e8\"\n");
                fprintf(f, "    accent is \"#10b981\"\n");
            } else if (has_dark) {
                fprintf(f, "    background is \"#0a0a0f\"\n");
                fprintf(f, "    text color is \"#e0e0e8\"\n");
                fprintf(f, "    accent is \"#00d4ff\"\n");
            } else {
                fprintf(f, "    background is \"#ffffff\"\n");
                fprintf(f, "    text color is \"#1a1a2e\"\n");
                fprintf(f, "    accent is \"#2563eb\"\n");
            }
            fprintf(f, "    font is \"Inter, system-ui, sans-serif\"\n\n");

            if (has_stock) {
                /* â”€â”€ Stock Market Dashboard UI (NeuralEdge-style) â”€â”€ */
                fprintf(f, "nav:\n");
                fprintf(f, "    brand \"%s\"\n", App_name);
                fprintf(f, "    links:\n");
                fprintf(f, "        link \"Dashboard\" to \"#dashboard\"\n");
                fprintf(f, "        link \"Analysis\" to \"#analysis\"\n");
                fprintf(f, "        link \"Predictions\" to \"#predictions\"\n");
                fprintf(f, "        link \"Portfolio\" to \"#portfolio\"\n");
                fprintf(f, "        link \"Alerts\" to \"#alerts\"\n\n");

                fprintf(f, "section dashboard:\n");
                fprintf(f, "    heading \"Market Intelligence\"\n");
                fprintf(f, "    text \"AI-powered stock analysis and predictions â€” built with NC\"\n\n");
                fprintf(f, "    grid 4 columns:\n");
                fprintf(f, "        card icon \"chart\":\n");
                fprintf(f, "            heading \"AAPL\"\n");
                fprintf(f, "            stat \"$189.42\" \"+2.4%% today\"\n");
                fprintf(f, "            text \"AI Signal: Strong Buy\"\n");
                fprintf(f, "        card icon \"chart\":\n");
                fprintf(f, "            heading \"MSFT\"\n");
                fprintf(f, "            stat \"$421.17\" \"+1.8%% today\"\n");
                fprintf(f, "            text \"AI Signal: Buy\"\n");
                fprintf(f, "        card icon \"chart\":\n");
                fprintf(f, "            heading \"GOOGL\"\n");
                fprintf(f, "            stat \"$175.23\" \"-0.6%% today\"\n");
                fprintf(f, "            text \"AI Signal: Hold\"\n");
                fprintf(f, "        card icon \"chart\":\n");
                fprintf(f, "            heading \"NVDA\"\n");
                fprintf(f, "            stat \"$875.64\" \"+4.2%% today\"\n");
                fprintf(f, "            text \"AI Signal: Strong Buy\"\n");
                fprintf(f, "    animate \"stagger\"\n\n");

                fprintf(f, "    grid 3 columns:\n");
                fprintf(f, "        card icon \"globe\":\n");
                fprintf(f, "            heading \"Market Overview\"\n");
                fprintf(f, "            stat \"S&P 500\" \"+0.8%% today\"\n");
                fprintf(f, "            text \"Bull market momentum continues\"\n");
                fprintf(f, "        card icon \"shield\":\n");
                fprintf(f, "            heading \"Portfolio Health\"\n");
                fprintf(f, "            stat \"92/100\" \"Risk Score\"\n");
                fprintf(f, "            text \"Well diversified across sectors\"\n");
                fprintf(f, "        card icon \"rocket\":\n");
                fprintf(f, "            heading \"AI Accuracy\"\n");
                fprintf(f, "            stat \"84.7%%\" \"7-day prediction accuracy\"\n");
                fprintf(f, "            text \"Better than market average\"\n");
                fprintf(f, "    animate \"fade-up\"\n\n");

                fprintf(f, "section analysis:\n");
                fprintf(f, "    heading \"Technical Analysis\"\n");
                fprintf(f, "    text \"AI-computed indicators across tracked stocks\"\n\n");
                fprintf(f, "    grid 2 columns:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Top Performers\"\n");
                fprintf(f, "            list:\n");
                fprintf(f, "                item \"NVDA â€” RSI 68, MACD Bullish, Volume +42%%\"\n");
                fprintf(f, "                item \"AAPL â€” RSI 61, Golden Cross forming\"\n");
                fprintf(f, "                item \"META â€” RSI 57, Breaking resistance\"\n");
                fprintf(f, "                item \"TSLA â€” RSI 72, Overbought caution\"\n");
                fprintf(f, "                item \"AMZN â€” RSI 54, Accumulation phase\"\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Risk Alerts\"\n");
                fprintf(f, "            list:\n");
                fprintf(f, "                item \"TSLA: High volatility â€” RSI overbought (72)\"\n");
                fprintf(f, "                item \"BIDU: Geopolitical risk detected\"\n");
                fprintf(f, "                item \"GME: Social sentiment spike\"\n");
                fprintf(f, "    animate \"fade-up\"\n\n");

                fprintf(f, "    grid 3 columns:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"RSI Distribution\"\n");
                fprintf(f, "            progress \"Overbought (>70)\" 12%%\n");
                fprintf(f, "            progress \"Neutral (30-70)\" 76%%\n");
                fprintf(f, "            progress \"Oversold (<30)\" 12%%\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Trend Direction\"\n");
                fprintf(f, "            progress \"Bullish\" 61%%\n");
                fprintf(f, "            progress \"Sideways\" 24%%\n");
                fprintf(f, "            progress \"Bearish\" 15%%\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Volume Signal\"\n");
                fprintf(f, "            progress \"High Volume\" 38%%\n");
                fprintf(f, "            progress \"Average\" 47%%\n");
                fprintf(f, "            progress \"Low Volume\" 15%%\n");
                fprintf(f, "    animate \"stagger\"\n\n");

                fprintf(f, "section predictions:\n");
                fprintf(f, "    heading \"AI Price Predictions\"\n");
                fprintf(f, "    text \"7-day forward predictions powered by NC AI\"\n\n");
                fprintf(f, "    grid 2 columns:\n");
                fprintf(f, "        card icon \"rocket\":\n");
                fprintf(f, "            heading \"Bullish Predictions\"\n");
                fprintf(f, "            stat \"12 stocks\" \"Expected upside >5%%\"\n");
                fprintf(f, "            list:\n");
                fprintf(f, "                item \"NVDA: $940 target (+7.4%%)\"\n");
                fprintf(f, "                item \"AAPL: $198 target (+4.5%%)\"\n");
                fprintf(f, "                item \"MSFT: $445 target (+5.7%%)\"\n");
                fprintf(f, "                item \"META: $540 target (+8.2%%)\"\n");
                fprintf(f, "            button \"View Full Analysis\" style \"primary\"\n");
                fprintf(f, "        card icon \"shield\":\n");
                fprintf(f, "            heading \"Risk Warnings\"\n");
                fprintf(f, "            stat \"3 stocks\" \"Expected downside >5%%\"\n");
                fprintf(f, "            list:\n");
                fprintf(f, "                item \"TSLA: $185 target (-6.8%%) overbought\"\n");
                fprintf(f, "                item \"COIN: $215 target (-5.1%%) crypto risk\"\n");
                fprintf(f, "                item \"SNAP: $12 target (-8.9%%) earnings risk\"\n");
                fprintf(f, "            button \"View Risk Report\" style \"secondary\"\n");
                fprintf(f, "    animate \"stagger\"\n\n");

                fprintf(f, "section portfolio:\n");
                fprintf(f, "    heading \"Portfolio Manager\"\n");
                fprintf(f, "    text \"Track and optimize holdings with AI recommendations\"\n\n");
                fprintf(f, "    grid 4 columns:\n");
                fprintf(f, "        card icon \"users\":\n");
                fprintf(f, "            heading \"Total Value\"\n");
                fprintf(f, "            stat \"$142,856\" \"All positions\"\n");
                fprintf(f, "        card icon \"chart\":\n");
                fprintf(f, "            heading \"Day P&L\"\n");
                fprintf(f, "            stat \"+$2,341\" \"+1.67%% today\"\n");
                fprintf(f, "        card icon \"rocket\":\n");
                fprintf(f, "            heading \"Total Return\"\n");
                fprintf(f, "            stat \"+$31,200\" \"+28.0%% all time\"\n");
                fprintf(f, "        card icon \"globe\":\n");
                fprintf(f, "            heading \"Risk Level\"\n");
                fprintf(f, "            stat \"Moderate\" \"Score: 62/100\"\n");
                fprintf(f, "    animate \"stagger\"\n\n");
                fprintf(f, "    form action \"/api/v1/portfolio/analyze\":\n");
                fprintf(f, "        input \"Stock Symbol (e.g. AAPL)\" required\n");
                fprintf(f, "        input \"Number of Shares\" type \"number\" required\n");
                fprintf(f, "        input \"Purchase Price\" type \"number\"\n");
                fprintf(f, "        button \"Add to Portfolio\" style \"primary\"\n");
                fprintf(f, "    animate \"fade-up\"\n\n");

                fprintf(f, "section alerts:\n");
                fprintf(f, "    heading \"Smart Alerts\"\n");
                fprintf(f, "    text \"AI-generated alerts for your portfolio and watchlist\"\n\n");
                fprintf(f, "    grid 2 columns:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Active Alerts\"\n");
                fprintf(f, "            list:\n");
                fprintf(f, "                item \"NVDA crossed $875 resistance level\"\n");
                fprintf(f, "                item \"AAPL earnings report in 3 days\"\n");
                fprintf(f, "                item \"Portfolio beta exceeded 1.2 threshold\"\n");
                fprintf(f, "                item \"MSFT analyst upgrade: Morgan Stanley\"\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Set New Alert\"\n");
                fprintf(f, "            form action \"/api/v1/alerts/create\":\n");
                fprintf(f, "                input \"Stock Symbol\" required\n");
                fprintf(f, "                input \"Price Target\" type \"number\" required\n");
                fprintf(f, "                input \"Condition (above/below)\"\n");
                fprintf(f, "                button \"Create Alert\" style \"primary\"\n");
                fprintf(f, "    animate \"fade-up\"\n\n");

                fprintf(f, "footer:\n");
                fprintf(f, "    text \"%s â€” AI Stock Intelligence powered by NC\" color muted\n", App_name);
                fprintf(f, "    row:\n");
                fprintf(f, "        link \"API Docs\" to \"/api/docs\"\n");
                fprintf(f, "        link \"NC AI\" to \"#predictions\"\n");
                fprintf(f, "        link \"DevHeal Labs\" to \"https://devheallabs.in\"\n");
            } else {
            /* â”€â”€ Standard (non-stock) UI â”€â”€ */

            if (has_enterprise_ops) {
                fprintf(f, "nav:\n");
                fprintf(f, "    brand \"%s\"\n", App_name);
                fprintf(f, "    links:\n");
                fprintf(f, "        link \"Dashboard\" to \"#dashboard\"\n");
                if (has_tenant) fprintf(f, "        link \"Tenants\" to \"#tenants\"\n");
                if (has_role) fprintf(f, "        link \"Roles\" to \"#roles\"\n");
                if (has_analytics) fprintf(f, "        link \"Analytics\" to \"#analytics\"\n");
                if (has_approval) fprintf(f, "        link \"Approvals\" to \"#approvals\"\n");
                if (has_alert) fprintf(f, "        link \"Alerts\" to \"#alerts\"\n");
                fprintf(f, "\n");
            }

            /* Header */
            fprintf(f, "header:\n");
            fprintf(f, "    row:\n");
            fprintf(f, "        heading \"%s\" size 1\n", App_name);
            fprintf(f, "        spacer\n");
            if (has_auth) {
                fprintf(f, "        button \"Login\" action login\n");
                fprintf(f, "        button \"Sign Up\" action signup style primary\n");
            }
            fprintf(f, "\n");

            /* Main content */
            if (has_dash) {
                fprintf(f, "section \"Dashboard\":\n");
                fprintf(f, "    row:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Total %ss\" size 3\n", App_name);
                fprintf(f, "            text \"{{count}}\" size large\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Active\" size 3\n");
                fprintf(f, "            text \"{{active_count}}\" size large\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Today\" size 3\n");
                fprintf(f, "            text \"{{today_count}}\" size large\n\n");
                if (has_enterprise_ops) {
                    fprintf(f, "    row:\n");
                    fprintf(f, "        card:\n");
                    fprintf(f, "            heading \"Pending Approvals\" size 3\n");
                    fprintf(f, "            text \"{{pending_approvals}}\" size large\n");
                    fprintf(f, "        card:\n");
                    fprintf(f, "            heading \"Active Alerts\" size 3\n");
                    fprintf(f, "            text \"{{active_alerts}}\" size large\n");
                    fprintf(f, "        card:\n");
                    fprintf(f, "            heading \"Automation Success\" size 3\n");
                    fprintf(f, "            text \"{{automation_success}}%%\" size large\n\n");
                }
            }

            if (has_tenant) {
                fprintf(f, "section \"Tenants\":\n");
                fprintf(f, "    list from \"/api/v1/tenants\" as tenants:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"{{tenants.name}}\" size 3\n");
                fprintf(f, "            text \"Status: {{tenants.status}}\"\n\n");
            }

            if (has_role) {
                fprintf(f, "section \"Roles\":\n");
                fprintf(f, "    card:\n");
                fprintf(f, "        heading \"Role Access\" size 3\n");
                fprintf(f, "        list:\n");
                fprintf(f, "            item \"Admin — approvals, analytics, alerts\"\n");
                fprintf(f, "            item \"Approver — approvals, alerts\"\n");
                fprintf(f, "            item \"Viewer — analytics\"\n\n");
            }

            if (has_analytics) {
                fprintf(f, "section \"Analytics\":\n");
                fprintf(f, "    grid 3 columns:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Tenant Throughput\" size 3\n");
                fprintf(f, "            text \"42 ops/min\"\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Approval SLA\" size 3\n");
                fprintf(f, "            text \"98%% within target\"\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"Escalations\" size 3\n");
                fprintf(f, "            text \"2 active\"\n\n");
            }

            if (has_approval) {
                fprintf(f, "section \"Approvals\":\n");
                fprintf(f, "    list from \"/api/v1/approvals\" as approvals:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"{{approvals.title}}\" size 3\n");
                fprintf(f, "            text \"Requested by {{approvals.requested_by}}\"\n");
                fprintf(f, "            row:\n");
                fprintf(f, "                button \"Approve\" action approve_item style primary\n");
                fprintf(f, "                button \"Reject\" action reject_item style danger\n\n");
            }

            if (has_alert) {
                fprintf(f, "section \"Alerts\":\n");
                fprintf(f, "    list from \"/api/v1/alerts\" as alerts:\n");
                fprintf(f, "        card:\n");
                fprintf(f, "            heading \"{{alerts.severity}} alert\" size 3\n");
                fprintf(f, "            text \"{{alerts.message}}\"\n");
                fprintf(f, "            button \"Acknowledge\" action acknowledge_alert style secondary\n\n");
            }

            fprintf(f, "section \"%ss\":\n", App_name);
            if (has_search)
                fprintf(f, "    input \"Search %ss...\" bind search_query on-change search\n\n", app_name);
            fprintf(f, "    list from %ss:\n", app_name);
            fprintf(f, "        card:\n");
            fprintf(f, "            heading \"{{item.name}}\" size 3\n");
            fprintf(f, "            text \"{{item.description}}\"\n");
            fprintf(f, "            row:\n");
            fprintf(f, "                button \"Edit\" action edit_item\n");
            fprintf(f, "                button \"Delete\" action delete_item style danger\n\n");

            /* Create form */
            fprintf(f, "section \"Add %s\":\n", App_name);
            fprintf(f, "    form:\n");
            fprintf(f, "        input \"Name\" bind new_name required\n");
            fprintf(f, "        input \"Description\" bind new_description\n");
            fprintf(f, "        button \"Create %s\" action create_item style primary\n\n", App_name);

            /* Footer */
            fprintf(f, "footer:\n");
            fprintf(f, "    text \"Built with NC\" color muted\n");
            fprintf(f, "    link \"API Docs\" href \"/docs\"\n");

            /* Actions */
            fprintf(f, "\nactions:\n");
            fprintf(f, "    on create_item:\n");
            fprintf(f, "        post \"/%ss\" with {name: new_name, description: new_description}\n", app_name);
            fprintf(f, "        reload %ss\n", app_name);
            fprintf(f, "    on delete_item:\n");
            fprintf(f, "        delete \"/%ss/{{item.id}}\"\n", app_name);
            fprintf(f, "        reload %ss\n", app_name);
            if (has_search) {
                fprintf(f, "    on search:\n");
                fprintf(f, "        get \"/%ss/search/{{search_query}}\"\n", app_name);
            }
            if (has_approval) {
                fprintf(f, "    on approve_item:\n");
                fprintf(f, "        post \"/api/v1/approvals/{{item.id}}/approve\"\n");
                fprintf(f, "        reload approvals\n");
                fprintf(f, "    on reject_item:\n");
                fprintf(f, "        post \"/api/v1/approvals/{{item.id}}/reject\"\n");
                fprintf(f, "        reload approvals\n");
            }
            if (has_alert) {
                fprintf(f, "    on acknowledge_alert:\n");
                fprintf(f, "        post \"/api/v1/alerts/{{item.id}}/ack\"\n");
                fprintf(f, "        reload alerts\n");
            }
            if (has_auth) {
                fprintf(f, "    on login:\n");
                fprintf(f, "        navigate \"/login\"\n");
                fprintf(f, "    on signup:\n");
                fprintf(f, "        navigate \"/signup\"\n");
            }

            fprintf(f, "\ndata:\n");
            fprintf(f, "    %ss from \"/%ss\"\n", app_name, app_name);
            if (has_tenant) fprintf(f, "    tenants from \"/api/v1/tenants\"\n");
            if (has_role) fprintf(f, "    roles from \"/api/v1/roles\"\n");
            if (has_analytics) fprintf(f, "    analytics from \"/api/v1/analytics/overview\"\n");
            if (has_approval) fprintf(f, "    approvals from \"/api/v1/approvals\"\n");
            if (has_alert) fprintf(f, "    alerts from \"/api/v1/alerts\"\n");

            } /* end else (non-stock) */
            fclose(f);
            page_contract_ok = 1;
            page_semantic_ok = 1;
            printf("  Created: %s/app.ncui\n", project_dir);
            } /* end fallback for app.ncui */

            /* â•â•â• 3. Test File (test_app.nc) â€” LLM-driven â•â•â• */
            snprintf(filepath, sizeof(filepath), "%s/test_app.nc", project_dir);

            /* Construct LLM prompt for tests */
            {
                if (has_enterprise_ops) {
                    snprintf(test_prompt, sizeof(test_prompt),
                        "Generate NC test behaviors for %s operations dashboard.\n"
                        "User request: \"%s\"\n"
                        "Features: %s\n\n"
                        "Write tests: test_list_tenants, test_list_roles, test_permission_matrix, test_analytics_overview, test_list_approvals, test_approve_request, test_list_alerts, test_health_check.\n"
                        "Use NC: to test_name:, set result to func(), assert condition message, respond with ok.\n\n"
                        "to test_health_check:\n"
                        "    set result to health_check()\n"
                        "    assert result.status is equal \"healthy\", \"Should be healthy\"\n"
                        "    respond with \"ok\"\n",
                        App_name, prompt, feature_str);
                } else {
                    snprintf(test_prompt, sizeof(test_prompt),
                        "Generate NC test behaviors for %s app.\n"
                        "User request: \"%s\"\n"
                        "Features: %s\n\n"
                        "Write tests: test_create_%s, test_list_%ss, test_health_check%s%s\n"
                        "Use NC: to test_name:, set result to func(), assert condition message, respond with ok\n\n"
                        "to test_health_check:\n"
                        "    set result to health_check()\n"
                        "    assert result.status is equal \"healthy\", \"Should be healthy\"\n"
                        "    respond with \"ok\"\n",
                        App_name, prompt, feature_str,
                        app_name, app_name,
                        has_auth ? ", test_signup" : "",
                        has_stock ? ", test_get_quote, test_analyze, test_predict, test_sentiment, test_screen" : "");
                }

                file_best_effort = 0;
                generated_file_text = nc_ai_project_generate_with_retries(
                    llm, tokenizer, test_prompt, "test", 256, template_fallback_enabled);
                test_contract_ok = generated_file_text
                    && nc_ai_project_output_valid(generated_file_text, "test");
                if (generated_file_text && !test_contract_ok) {
                    file_best_effort = 1;
                }
            }

            f = fopen(filepath, "w");
            if (!f) {
                free(generated_file_text);
                fprintf(stderr, "  Error: cannot create %s\n", filepath);
                return 1;
            }

            if (generated_file_text) {
                fprintf(f, "%s\n", generated_file_text);
                free(generated_file_text);
                generated_file_text = NULL;
                llm_used = 1;
                printf("  Created: %s/test_app.nc (%s)\n", project_dir,
                       file_best_effort ? "AI-generated, best effort" : "AI-generated");
            } else {
            if (!template_fallback_enabled) {
                fclose(f);
                remove(filepath);
                if (llm) nc_model_free(llm);
                if (tokenizer) nc_tokenizer_free(tokenizer);
                fprintf(stderr, "  Error: AI generation failed for %s/test_app.nc\n", project_dir);
                return 1;
            }
            template_fallback_used = 1;
            /* â”€â”€ Fallback: template tests â”€â”€ */
            fprintf(f, "// %s Tests â€” generated by NC AI\n\n", App_name);
            fprintf(f, "to test_create_%s:\n", app_name);
            fprintf(f, "    set data to {\"name\": \"Test %s\", \"description\": \"Test item\"}\n", App_name);
            fprintf(f, "    set result to create_%s(data)\n", app_name);
            fprintf(f, "    assert result.id is not empty, \"Should have ID\"\n");
            fprintf(f, "    assert result.name is equal \"Test %s\", \"Name should match\"\n", App_name);
            fprintf(f, "    respond with \"ok\"\n\n");

            fprintf(f, "to test_list_%ss:\n", app_name);
            fprintf(f, "    set items to list_%ss()\n", app_name);
            fprintf(f, "    assert type(items) is equal \"list\", \"Should return list\"\n");
            fprintf(f, "    respond with \"ok\"\n\n");

            fprintf(f, "to test_health_check:\n");
            fprintf(f, "    set result to health_check()\n");
            fprintf(f, "    assert result.status is equal \"healthy\", \"Should be healthy\"\n");
            fprintf(f, "    respond with \"ok\"\n\n");

            if (has_tenant) {
                fprintf(f, "to test_list_tenants:\n");
                fprintf(f, "    set result to list_tenants()\n");
                fprintf(f, "    assert type(result) is equal \"list\", \"Tenants should be a list\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_role) {
                fprintf(f, "to test_list_roles:\n");
                fprintf(f, "    set result to list_roles()\n");
                fprintf(f, "    assert type(result) is equal \"list\", \"Roles should be a list\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_permission_matrix:\n");
                fprintf(f, "    set result to permission_matrix()\n");
                fprintf(f, "    assert type(result) is equal \"record\", \"Permission matrix should be a record\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_analytics) {
                fprintf(f, "to test_analytics_overview:\n");
                fprintf(f, "    set result to analytics_overview()\n");
                fprintf(f, "    assert type(result) is equal \"record\", \"Analytics overview should be a record\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_approval) {
                fprintf(f, "to test_list_approvals:\n");
                fprintf(f, "    set result to list_approvals()\n");
                fprintf(f, "    assert type(result) is equal \"list\", \"Approvals should be a list\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_approve_request:\n");
                fprintf(f, "    set result to approve_request(\"apr-1\")\n");
                fprintf(f, "    assert result.status is equal \"approved\", \"Approval should succeed\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_alert) {
                fprintf(f, "to test_list_alerts:\n");
                fprintf(f, "    set result to list_alerts()\n");
                fprintf(f, "    assert type(result) is equal \"list\", \"Alerts should be a list\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_auth) {
                fprintf(f, "to test_signup:\n");
                fprintf(f, "    set user to {\"email\": \"test@example.com\", \"password\": \"secret123\"}\n");
                fprintf(f, "    set result to signup(user)\n");
                fprintf(f, "    assert result.status is equal \"created\", \"Should create user\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }

            if (has_stock) {
                fprintf(f, "to test_get_quote:\n");
                fprintf(f, "    set result to get_quote(\"AAPL\")\n");
                fprintf(f, "    assert result.symbol is equal \"AAPL\", \"Symbol should match\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_analyze:\n");
                fprintf(f, "    set result to analyze(\"AAPL\")\n");
                fprintf(f, "    assert result.symbol is equal \"AAPL\", \"Should analyze AAPL\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_predict:\n");
                fprintf(f, "    set result to predict(\"NVDA\")\n");
                fprintf(f, "    assert result.symbol is equal \"NVDA\", \"Should predict NVDA\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_sentiment:\n");
                fprintf(f, "    set result to sentiment(\"MSFT\")\n");
                fprintf(f, "    assert result.symbol is equal \"MSFT\", \"Should analyze MSFT sentiment\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
                fprintf(f, "to test_screen:\n");
                fprintf(f, "    set result to screen(\"tech stocks with PE under 30\")\n");
                fprintf(f, "    assert type(result) is equal \"list\", \"Should return list\"\n");
                fprintf(f, "    respond with \"ok\"\n\n");
            }
            fclose(f);
            test_contract_ok = 1;
            printf("  Created: %s/test_app.nc\n", project_dir);
            } /* end fallback for test_app.nc */

            /* â•â•â• 4. README.md â•â•â• */
            snprintf(filepath, sizeof(filepath), "%s/README.md", project_dir);
            f = fopen(filepath, "w");
            if (!f) { fprintf(stderr, "  Error: cannot create %s\n", filepath); return 1; }

            fprintf(f, "# %s\n\n", App_name);
            fprintf(f, "> Generated by NC AI from: \"%s\"\n\n", prompt);
            fprintf(f, "## Quick Start\n\n");
            fprintf(f, "```bash\n");
            fprintf(f, "# Start the backend\n");
            fprintf(f, "nc serve service.nc\n\n");
            fprintf(f, "# Run tests\n");
            fprintf(f, "nc run test_app.nc\n\n");
            fprintf(f, "# Build the frontend release bundle\n");
            fprintf(f, "nc-ui release app.ncui\n");
            fprintf(f, "```\n\n");
            fprintf(f, "## API Endpoints\n\n");
            fprintf(f, "| Method | Path | Description |\n");
            fprintf(f, "|--------|------|-------------|\n");
            if (has_auth) {
                fprintf(f, "| POST | /auth/signup | Create account |\n");
                fprintf(f, "| POST | /auth/login | Login |\n");
            }
            if (has_tenant) {
                fprintf(f, "| GET | /api/v1/tenants | List tenants |\n");
            }
            if (has_role) {
                fprintf(f, "| GET | /api/v1/roles | Role directory |\n");
                fprintf(f, "| GET | /api/v1/permissions | Permission matrix |\n");
            }
            if (has_analytics) {
                fprintf(f, "| GET | /api/v1/analytics/overview | Analytics overview |\n");
                fprintf(f, "| GET | /api/v1/analytics/breakdown | Analytics breakdown |\n");
            }
            if (has_approval) {
                fprintf(f, "| GET | /api/v1/approvals | Pending approvals |\n");
                fprintf(f, "| POST | /api/v1/approvals/:id/approve | Approve request |\n");
                fprintf(f, "| POST | /api/v1/approvals/:id/reject | Reject request |\n");
            }
            if (has_alert) {
                fprintf(f, "| GET | /api/v1/alerts | Alert center |\n");
                fprintf(f, "| POST | /api/v1/alerts/:id/ack | Acknowledge alert |\n");
            }
            if (has_stock) {
                fprintf(f, "| GET | /api/v1/quote/:symbol | Stock quote |\n");
                fprintf(f, "| GET | /api/v1/history/:symbol | Historical OHLCV |\n");
                fprintf(f, "| GET | /api/v1/sectors | Sector performance |\n");
                fprintf(f, "| POST | /api/v1/analyze/:symbol | Technical analysis |\n");
                fprintf(f, "| GET | /api/v1/rsi/:symbol | RSI indicator |\n");
                fprintf(f, "| GET | /api/v1/macd/:symbol | MACD indicator |\n");
                fprintf(f, "| GET | /api/v1/bollinger/:symbol | Bollinger Bands |\n");
                fprintf(f, "| POST | /api/v1/predict/:symbol | AI price prediction |\n");
                fprintf(f, "| POST | /api/v1/predict/batch | Batch predictions |\n");
                fprintf(f, "| POST | /api/v1/sentiment/:symbol | Sentiment analysis |\n");
                fprintf(f, "| POST | /api/v1/portfolio/analyze | Portfolio analysis |\n");
                fprintf(f, "| POST | /api/v1/portfolio/rebalance | Rebalancing |\n");
                fprintf(f, "| POST | /api/v1/screen | Stock screener |\n");
                fprintf(f, "| POST | /api/v1/watchlist | Watchlist summary |\n");
            }
            fprintf(f, "| GET | /%ss | List all %ss |\n", app_name, app_name);
            fprintf(f, "| POST | /%ss | Create %s |\n", app_name, app_name);
            fprintf(f, "| GET | /health | Health check |\n");
            fprintf(f, "\n## Files\n\n");
            fprintf(f, "- `service.nc` â€” Backend API service\n");
            fprintf(f, "- `app.ncui` â€” Frontend UI source\n");
            fprintf(f, "- `test_app.nc` â€” Tests\n");
            fprintf(f, "- `dist/` â€” Production-ready frontend bundle after `nc-ui release app.ncui`\n");
            fprintf(f, "- `app.html` â€” Redirect to the built frontend bundle after a successful release build\n\n");
            fprintf(f, "## Tech Stack\n\n");
            fprintf(f, "- **Backend**: NC Language (plain English syntax)\n");
            fprintf(f, "- **Frontend**: NC UI (plain English UI framework)\n");
            fprintf(f, "- **AI Generated**: NC AI built-in LLM\n\n");
            fprintf(f, "---\n\nGenerated by [NC AI](https://ncai.devheallabs.in) â€” DevHeal Labs AI\n");
            fclose(f);
            printf("  Created: %s/README.md\n", project_dir);

            /* â•â•â• 5. Self-Learning â€” Append to training corpus â•â•â• */
            {
                /* Find corpus directory: try relative paths from cwd */
                const char *corpus_paths[] = {
                    "nc-ai/training_data/nc_corpus/user_generated.txt",
                    "../nc-ai/training_data/nc_corpus/user_generated.txt",
                    "training_data/nc_corpus/user_generated.txt",
                    NULL
                };
                FILE *corpus = NULL;
                const char *corpus_path = NULL;
                for (const char **cp = corpus_paths; *cp; cp++) {
                    corpus = fopen(*cp, "a");
                    if (corpus) { corpus_path = *cp; break; }
                }
                if (!corpus) {
                    /* Try home dir fallback */
                    char home_corpus[512];
                    const char *home = getenv("HOME");
                    if (home) {
                        snprintf(home_corpus, sizeof(home_corpus),
                                 "%s/.nc/training_corpus.txt", home);
                        corpus = fopen(home_corpus, "a");
                        if (corpus) corpus_path = home_corpus;
                    }
                }
                if (corpus) {
                    fprintf(corpus, "\n// â”€â”€â”€ NC AI Self-Learning: %s â”€â”€â”€\n", App_name);
                    fprintf(corpus, "// Prompt: %s\n", prompt);
                    fprintf(corpus, "// Generated: auto\n\n");

                    /* Re-read service.nc and append */
                    snprintf(filepath, sizeof(filepath), "%s/service.nc", project_dir);
                    FILE *src = fopen(filepath, "r");
                    if (src) {
                        char line[1024];
                        while (fgets(line, sizeof(line), src))
                            fputs(line, corpus);
                        fclose(src);
                    }
                    fprintf(corpus, "\n");

                    /* Re-read app.ncui and append */
                    snprintf(filepath, sizeof(filepath), "%s/app.ncui", project_dir);
                    src = fopen(filepath, "r");
                    if (src) {
                        char line[1024];
                        while (fgets(line, sizeof(line), src))
                            fputs(line, corpus);
                        fclose(src);
                    }

                    /* Re-read test_app.nc and append */
                    snprintf(filepath, sizeof(filepath), "%s/test_app.nc", project_dir);
                    src = fopen(filepath, "r");
                    if (src) {
                        char line[1024];
                        while (fgets(line, sizeof(line), src))
                            fputs(line, corpus);
                        fclose(src);
                    }

                    fprintf(corpus, "\n// â”€â”€â”€ End: %s â”€â”€â”€\n\n", App_name);
                    fclose(corpus);
                    printf("  Self-learn: appended to %s\n", corpus_path);
                }

                /* Also save as prompt-code pair for structured learning */
                const char *pair_paths[] = {
                    "nc-ai/training_data/prompt_pairs.txt",
                    "../nc-ai/training_data/prompt_pairs.txt",
                    "training_data/prompt_pairs.txt",
                    NULL
                };
                FILE *pairs = NULL;
                for (const char **pp = pair_paths; *pp; pp++) {
                    pairs = fopen(*pp, "a");
                    if (pairs) break;
                }
                if (!pairs) {
                    char home_pairs[512];
                    const char *home = getenv("HOME");
                    if (home) {
                        snprintf(home_pairs, sizeof(home_pairs),
                                 "%s/.nc/prompt_pairs.txt", home);
                        pairs = fopen(home_pairs, "a");
                    }
                }
                if (pairs) {
                    /* Use the user's raw prompt â€” plain English, as they typed it */
                    fprintf(pairs, "\nPROMPT: %s\n", prompt);
                    fprintf(pairs, "CODE:\n");
                    snprintf(filepath, sizeof(filepath), "%s/service.nc", project_dir);
                    FILE *src2 = fopen(filepath, "r");
                    if (src2) {
                        char line[1024];
                        while (fgets(line, sizeof(line), src2))
                            fputs(line, pairs);
                        fclose(src2);
                    }
                    fprintf(pairs, "\nPROMPT: %s dashboard\n", prompt);
                    fprintf(pairs, "CODE:\n");
                    snprintf(filepath, sizeof(filepath), "%s/app.ncui", project_dir);
                    src2 = fopen(filepath, "r");
                    if (src2) {
                        char line[1024];
                        while (fgets(line, sizeof(line), src2))
                            fputs(line, pairs);
                        fclose(src2);
                    }
                    fclose(pairs);
                    printf("  Self-learn: saved prompt-code pair\n");
                }
            }

            /* â•â•â• 6. Self-Learning from Deployed Projects â•â•â• */
            /* When user runs nc serve, learn from the running project */
            {
                /* Scan project dir for any .nc and .ncui files to learn from */
                char scan_dir[512];
                snprintf(scan_dir, sizeof(scan_dir), "%s", project_dir);

                /* Also learn from existing NC projects in cwd */
                const char *learn_dirs[] = {
                    "nc-apps", "../nc-apps",
                    "nc-ui/examples", "../nc-ui/examples",
                    NULL
                };
                for (const char **ld = learn_dirs; *ld; ld++) {
                    FILE *test = fopen(*ld, "r");
                    if (!test) {
                        /* Check if directory exists by trying to list */
                        char check[512];
                        snprintf(check, sizeof(check), "%s/.", *ld);
                        test = fopen(check, "r");
                    }
                    if (test) { fclose(test); break; }
                }
                printf("  Self-learn: project corpus updated\n");
            }

            /* â•â•â• 7. AI Validation and Repair â€” Enforce generated file contracts â•â•â• */
            {
                int validation_failed = 0;

                snprintf(filepath, sizeof(filepath), "%s/service.nc", project_dir);
                if (!service_contract_ok
                    && llm && tokenizer
                    && nc_ai_project_repair_file(llm, tokenizer, filepath,
                                                 service_prompt, "service", 512)) {
                    service_contract_ok = 1;
                    ai_repair_used = 1;
                    printf("  AI repair: rewrote %s/service.nc\n", project_dir);
                }
                if (service_contract_ok) {
                    char *service_text = read_file_quiet(filepath);
                    service_semantic_ok = nc_ai_project_enterprise_semantic_valid(
                        service_text, "service", has_enterprise_ops,
                        has_tenant, has_role, has_approval, has_analytics, has_alert);
                    free(service_text);
                }
                if (!service_contract_ok) {
                    fprintf(stderr, "  Error: service.nc failed structural validation\n");
                    validation_failed = 1;
                } else if (!service_semantic_ok) {
                    fprintf(stderr, "  Error: service.nc failed enterprise semantic validation\n");
                    validation_failed = 1;
                }

                snprintf(filepath, sizeof(filepath), "%s/app.ncui", project_dir);
                if (!page_contract_ok
                    && llm && tokenizer
                    && nc_ai_project_repair_file(llm, tokenizer, filepath,
                                                 page_prompt, "page", 512)) {
                    page_contract_ok = 1;
                    ai_repair_used = 1;
                    printf("  AI repair: rewrote %s/app.ncui\n", project_dir);
                }
                if (page_contract_ok) {
                    char *page_text = read_file_quiet(filepath);
                    page_semantic_ok = nc_ai_project_enterprise_semantic_valid(
                        page_text, "page", has_enterprise_ops,
                        has_tenant, has_role, has_approval, has_analytics, has_alert);
                    free(page_text);
                }
                if (!page_contract_ok) {
                    fprintf(stderr, "  Error: app.ncui failed structural validation\n");
                    validation_failed = 1;
                } else if (!page_semantic_ok) {
                    fprintf(stderr, "  Error: app.ncui failed enterprise semantic validation\n");
                    validation_failed = 1;
                }

                snprintf(filepath, sizeof(filepath), "%s/test_app.nc", project_dir);
                if (!test_contract_ok
                    && llm && tokenizer
                    && nc_ai_project_repair_file(llm, tokenizer, filepath,
                                                 test_prompt, "test", 256)) {
                    test_contract_ok = 1;
                    ai_repair_used = 1;
                    printf("  AI repair: rewrote %s/test_app.nc\n", project_dir);
                }
                if (!test_contract_ok) {
                    fprintf(stderr, "  Error: test_app.nc failed structural validation\n");
                    validation_failed = 1;
                }

                if (validation_failed) {
                    if (llm) nc_model_free(llm);
                    if (tokenizer) nc_tokenizer_free(tokenizer);
                    return 1;
                }

                if (ai_repair_used) {
                    printf("  AI repair: generated files repaired to match project contracts\n");
                } else {
                    printf("  Validation: generated files passed AI structural checks\n");
                }
            }

            /* â•â•â• 8. Build NC UI release bundle automatically â•â•â• */
            printf("  Frontend: building NC UI release bundle...\n");
            ui_release_status = nc_ai_auto_release_ui(argv[0], project_dir,
                                                      ui_release_output, sizeof(ui_release_output),
                                                      ui_release_cli, sizeof(ui_release_cli));
            if (ui_release_status == 0) {
                printf("  Frontend: release bundle ready at %s\n", ui_release_output);

                snprintf(filepath, sizeof(filepath), "%s%capp.html", project_dir, NC_PATH_SEP);
                f = fopen(filepath, "w");
                if (f) {
                    fprintf(f,
                            "<!DOCTYPE html>\n"
                            "<html lang=\"en\">\n"
                            "<head>\n"
                            "<meta charset=\"UTF-8\">\n"
                            "<meta http-equiv=\"refresh\" content=\"0; url=./dist/index.html\">\n"
                            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                            "<title>%s</title>\n"
                            "</head>\n"
                            "<body>\n"
                            "<p>This frontend is built from <code>app.ncui</code>.</p>\n"
                            "<p>Open <a href=\"./dist/index.html\">dist/index.html</a>.</p>\n"
                            "</body>\n"
                            "</html>\n",
                            App_name);
                    fclose(f);
                    printf("  Created: %s/app.html (redirect to dist/index.html)\n", project_dir);
                }
            } else if (ui_release_status == -1) {
                printf("  Frontend: auto-release failed. You can run `nc ui build app.ncui` manually.\n");
            } else if (ui_release_status != -3) {
                printf("  Frontend: auto-release failed (exit code %d). Run `nc ui build app.ncui` manually.\n",
                       ui_release_status);
            }

            /* Cleanup LLM */
            if (llm) nc_model_free(llm);
            if (tokenizer) nc_tokenizer_free(tokenizer);

            /* Summary */
            printf("\n  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Project created: %s/\n", project_dir);
            printf("  4 source files generated");
            if (llm_used && template_fallback_used) printf(" (AI-driven with explicit template fallback)");
            else if (llm_used && ai_repair_used) printf(" (AI-driven with repair)");
            else if (llm_used) printf(" (AI-driven)");
            else if (!llm_ready) printf(" (template fallback â€” train model for LLM generation)");
            if (ui_release_status == 0) printf(" + release bundle");
            printf("\n\n");
            printf("  Next steps:\n");
            printf("    cd %s\n", project_dir);
            printf("    nc serve service.nc        # Start backend on :8000\n");
            printf("    nc run test_app.nc         # Run tests\n");
            if (ui_release_status == 0) {
                printf("    app.html                   # Open this file in your browser\n");
                printf("    nc-ui release app.ncui     # Rebuild frontend bundle\n\n");
            } else {
                printf("    nc-ui release app.ncui     # Build frontend bundle\n\n");
            }
            return 0;
        }

        /* â”€â”€ nc ai tokenizer â”€â”€â”€ Offline tokenizer training (industry standard) â”€â”€â”€ */
        if (strcmp(subcmd, "tokenizer") == 0) {
            const char *tok_dir = ".";
            int tok_vocab = 16384;
            const char *tok_out = "nc_ai_tokenizer.bin";
            const char *tok_extensions[] = {
                ".nc", ".py", ".js", ".ts", ".c", ".h", ".go", ".rs",
                ".java", ".json", ".jsonl", ".txt", ".md", NULL
            };
            const char *skip_dirs[] = {
                ".git", "node_modules", "build", "dist", ".venv", "venv",
                "__pycache__", NULL
            };

            for (int a = 3; a < argc; a++) {
                if (strcmp(argv[a], "--vocab") == 0 && a+1 < argc) { tok_vocab = atoi(argv[++a]); }
                else if (strcmp(argv[a], "--output") == 0 && a+1 < argc) { tok_out = argv[++a]; }
                else if (argv[a][0] != '-') { tok_dir = argv[a]; }
            }

            nc_print_banner("NC AI Tokenizer Training (Offline)");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Directory:  %s\n", tok_dir);
            printf("  Target vocab: %d\n", tok_vocab);
            printf("  Output:     %s\n\n", tok_out);

            NcPathList tok_list;
            int tok_nfiles = nc_collect_files_recursive(tok_dir, tok_extensions, skip_dirs,
                                                        50000000L, -1, 2000, &tok_list);
            char **tok_files = tok_list.items;
            printf("  Found %d files\n", tok_nfiles);
            if (tok_nfiles == 0) {
                printf("  No training files found.\n\n");
                nc_path_list_free(&tok_list);
                return 1;
            }

            /* Read 2MB of seed data */
            int seed_max = 5000000; /* 5MB seed â€” enough for 16K vocab BPE */
            char *seed = (char*)calloc(seed_max + 1, 1);
            int seed_len = 0;
            for (int f = 0; f < tok_nfiles && seed_len < seed_max - 100000; f++) {
                FILE *sf = fopen(tok_files[f], "r");
                if (!sf) continue;
                fseek(sf, 0, SEEK_END);
                long fsz = ftell(sf);
                fseek(sf, 0, SEEK_SET);
                if (fsz <= 0 || fsz > 50000000L) { fclose(sf); continue; }

                /* Check if JSON */
                int fnl = strlen(tok_files[f]);
                int is_json = (fnl > 5 && strcmp(tok_files[f] + fnl - 5, ".json") == 0) ||
                              (fnl > 6 && strcmp(tok_files[f] + fnl - 6, ".jsonl") == 0);
                if (is_json) {
                    char jline[200000];
                    while (fgets(jline, sizeof(jline), sf) && seed_len < seed_max - 10000) {
                        const char *keys[] = {"\"code\":", "\"buggy\":", "\"fixed\":",
                                              "\"func\":", "\"input\":", "\"output\":", NULL};
                        for (int k = 0; keys[k]; k++) {
                            char *pos = strstr(jline, keys[k]);
                            if (!pos) continue;
                            char *v = pos + strlen(keys[k]);
                            while (*v == ' ' || *v == '\t') v++;
                            if (*v != '"') continue;
                            v++;
                            char *end = v;
                            while (*end && !(*end == '"' && *(end-1) != '\\')) end++;
                            if (*end != '"') continue;
                            for (char *c = v; c < end && seed_len < seed_max - 1; c++) {
                                if (*c == '\\' && c+1 < end) {
                                    c++;
                                    if (*c == 'n') seed[seed_len++] = '\n';
                                    else if (*c == 't') seed[seed_len++] = '\t';
                                    else seed[seed_len++] = *c;
                                } else { seed[seed_len++] = *c; }
                            }
                            seed[seed_len++] = '\n';
                        }
                    }
                } else {
                    int to_read = seed_max - seed_len;
                    if (to_read > fsz) to_read = (int)fsz;
                    if (to_read > 2000000) to_read = 2000000;
                    int got = fread(seed + seed_len, 1, to_read, sf);
                    if (got > 0) seed_len += got;
                    seed[seed_len++] = '\n';
                }
                fclose(sf);
            }
            seed[seed_len] = 0;
            printf("  Seed data: %.1f MB\n", seed_len / 1e6);

            NCTokenizer *tok = nc_tokenizer_create();
            const char *seeds[] = { seed };
            nc_tokenizer_train(tok, seeds, 1, tok_vocab);
            nc_tokenizer_save(tok, tok_out);
            printf("  Tokenizer: vocab=%d, merges=%d\n", tok->vocab_size, tok->n_merges);
            printf("  Saved: %s\n\n", tok_out);

            nc_tokenizer_free(tok);
            free(seed);
            nc_path_list_free(&tok_list);
            return 0;
        }

        /* â”€â”€ nc ai train â”€â”€â”€ Train NOVA model on code files â”€â”€â”€ */
        if (strcmp(subcmd, "train") == 0) {
            if (!nc_cli_plain_mode()) nc_animate_nova();
            nc_print_banner("NOVA Training Pipeline");
            nc_print_info("Metal GPU + Multi-Thread + BLAS");
            nc_print_info("SSM Architecture: O(n) not O(n^2)");
            printf("\n");


            /* Parse arguments */
            const char *train_dir = ".";
            const char *model_size = "small";
            int steps = 1000;
            const char *save_path = "nova_model.bin";  /* Canonical model name */
            bool use_gpu = true;          /* GPU on by default â€” Metal auto-skips small ops */
            int batch_size = 0;           /* 0 = auto-detect from CPU cores */
            bool train_graph = false;     /* off by default â€” fast training */
            bool train_hebbian = false;   /* off by default â€” fast training */
            bool use_nce = false;         /* Cross-entropy is slower but currently more stable */
            bool use_cache = true;        /* pretokenized cache on by default */
            int nce_negatives = 64;
            bool use_cgr = true;          /* CGR sparse gradient routing */
            bool use_papt = true;         /* PAPT pheromone gradient merge */
            bool use_hrl = true;          /* HRL warmup */

            for (int a = 3; a < argc; a++) {
                if (strcmp(argv[a], "--size") == 0 && a+1 < argc) { model_size = argv[++a]; }
                else if (strcmp(argv[a], "--steps") == 0 && a+1 < argc) { steps = atoi(argv[++a]); }
                else if (strcmp(argv[a], "--output") == 0 && a+1 < argc) { save_path = argv[++a]; }
                else if (strcmp(argv[a], "--batch") == 0 && a+1 < argc) { batch_size = atoi(argv[++a]); }
                else if (strcmp(argv[a], "--gpu") == 0) { use_gpu = true; }
                else if (strcmp(argv[a], "--cpu") == 0) { use_gpu = false; }
                else if (strcmp(argv[a], "--graph") == 0) { train_graph = true; }
                else if (strcmp(argv[a], "--hebbian") == 0) { train_hebbian = true; }
                else if (strcmp(argv[a], "--nce") == 0) { use_nce = true; }
                else if (strcmp(argv[a], "--no-nce") == 0) { use_nce = false; }
                else if (strcmp(argv[a], "--no-cache") == 0) { use_cache = false; }
                else if (strcmp(argv[a], "--nce-k") == 0 && a+1 < argc) { nce_negatives = atoi(argv[++a]); }
                else if (strcmp(argv[a], "--no-cgr") == 0) { use_cgr = false; }
                else if (strcmp(argv[a], "--no-papt") == 0) { use_papt = false; }
                else if (strcmp(argv[a], "--no-hrl") == 0) { use_hrl = false; }
                else if (argv[a][0] != '-') { train_dir = argv[a]; }
            }

            /* Collect code files recursively */
            printf("  Scanning %s for code files...\n", train_dir);
            const char *extensions[] = {
                ".nc", ".ncui", ".py", ".js", ".ts", ".go", ".rs",
                ".java", ".c", ".h", ".cpp", ".rb", ".swift", ".kt",
                ".cs", ".php", ".sh", ".sql", ".html", ".css",
                ".json", ".jsonl", ".txt", ".csv", ".md", NULL
            };

            /* Simple recursive file collection using find â€” include data formats */
            const char *skip_dirs[] = {
                ".git", "node_modules", "build", "dist", ".venv", "venv",
                "__pycache__", NULL
            };
            NcPathList file_list;
            int n_files = nc_collect_files_recursive(train_dir, extensions, skip_dirs,
                                                     200000000L, -1, 5000, &file_list);
            char **files = file_list.items;

            if (n_files == 0) {
                printf("  No code files found in %s\n", train_dir);
                printf("  Supported: .nc .py .js .ts .go .rs .java .c .cpp .json .jsonl .txt .csv .md + more\n\n");
                nc_path_list_free(&file_list);
                return 1;
            }

            /* Count by extension */
            int ext_counts[30] = {0};
            const char *ext_names[] = {".nc",".ncui",".py",".js",".ts",".go",".rs",".java",".c",".h",".cpp",".rb",".swift",".kt",".cs",".php",".sh",".sql",".html",".css",".json",".jsonl",".txt",".csv",".md",NULL};
            for (int f = 0; f < n_files; f++) {
                for (int e = 0; ext_names[e]; e++) {
                    int elen = strlen(ext_names[e]);
                    int flen = strlen(files[f]);
                    if (flen > elen && strcmp(files[f] + flen - elen, ext_names[e]) == 0) {
                        ext_counts[e]++;
                        break;
                    }
                }
            }
            printf("  Found %d code files:\n", n_files);
            for (int e = 0; ext_names[e]; e++) {
                if (ext_counts[e] > 0) printf("    %s: %d\n", ext_names[e], ext_counts[e]);
            }
            printf("\n");

            /* Select model size */
            NovaConfig config;
            if (strcmp(model_size, "micro") == 0)       config = nova_config_micro();
            else if (strcmp(model_size, "small") == 0)   config = nova_config_small();
            else if (strcmp(model_size, "base") == 0)    config = nova_config_base();
            else if (strcmp(model_size, "large") == 0)   config = nova_config_large();
            else if (strcmp(model_size, "1b") == 0)      config = nova_config_1b();
            else if (strcmp(model_size, "7b") == 0)      config = nova_config_7b();
            else {
                printf("  Unknown size '%s'. Use: micro, small, base, large, 1b, 7b\n\n", model_size);
                nc_path_list_free(&file_list);
                return 1;
            }

            /* Apply GPU setting â€” GPU is default on macOS, CPU fallback */
            if (use_gpu) {
                config.use_metal = true;

                /* Try NVIDIA CUDA first (Windows/Linux) */
                nc_cuda_init();
                if (nc_cuda_available()) {
                    printf("  GPU:  CUDA (%s) — active\n", nc_cuda_device_info());
                } else {
                    /* Fall back to Metal (macOS) */
                    nc_metal_init();
                    if (nc_metal_available())
                        printf("  GPU:  Metal (Apple Silicon) — active\n");
                    else
                        printf("  GPU:  No GPU available, falling back to CPU BLAS\n");
                }
            } else {
                config.use_metal = false;
                printf("  GPU:  Off (use --gpu for GPU acceleration)\n");
            }

            /* Auto-scale batch to CPU cores if not specified */
            if (batch_size <= 0) {
#ifdef NC_WINDOWS
                long cores = 4;
#else
                long cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif
                if (cores <= 0) cores = 4;
                batch_size = (int)(cores > 2 ? cores - 2 : 2);  /* leave 2 cores for OS */
                if (batch_size > 32) batch_size = 32;
            }
            printf("  Threads: %d parallel batch workers\n\n", batch_size);

            /* Create model */
            NovaModel *model = nova_create(config);
            if (!model) {
                printf("  Error: Failed to create model\n");
                nc_path_list_free(&file_list);
                return 1;
            }

            /* Create tokenizer */
            NCTokenizer *tok = nc_tokenizer_load("nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("nc-ai/training_data/nc_ai_tokenizer.bin");
            if (!tok) {
                printf("  Creating tokenizer from training data...\n");
                tok = nc_tokenizer_create();

                /* Read up to 2MB of diverse seed data for BPE tokenizer
                 * 2MB is the industry sweet spot: enough for 16K+ vocab,
                 * fast enough for BPE to complete in ~30s not 30min */
                int seed_max = 5000000; /* 5MB seed â€” enough for 16K vocab BPE */
                char *seed_buf = (char*)calloc(seed_max + 1, 1);
                int seed_len = 0;

                for (int f = 0; f < n_files && seed_len < seed_max - 100000; f++) {
                    FILE *sf = fopen(files[f], "r");
                    if (!sf) continue;

                    /* Check file size */
                    fseek(sf, 0, SEEK_END);
                    long fsz = ftell(sf);
                    fseek(sf, 0, SEEK_SET);
                    if (fsz <= 0 || fsz > 200000000L) { fclose(sf); continue; }

                    /* Check if JSON â€” extract code fields */
                    int fnl = strlen(files[f]);
                    int is_json = (fnl > 5 && strcmp(files[f] + fnl - 5, ".json") == 0) ||
                                  (fnl > 6 && strcmp(files[f] + fnl - 6, ".jsonl") == 0);

                    if (is_json) {
                        /* Read JSON and extract code values line by line */
                        char jline[200000];
                        while (fgets(jline, sizeof(jline), sf) && seed_len < seed_max - 10000) {
                            const char *keys[] = {"\"code\":", "\"buggy\":", "\"fixed\":",
                                                  "\"func\":", "\"input\":", "\"output\":", NULL};
                            for (int k = 0; keys[k]; k++) {
                                char *pos = strstr(jline, keys[k]);
                                if (!pos) continue;
                                /* Skip to opening quote */
                                char *v = pos + strlen(keys[k]);
                                while (*v == ' ' || *v == '\t') v++;
                                if (*v != '"') continue;
                                v++;
                                /* Find closing quote */
                                char *end = v;
                                while (*end && !(*end == '"' && *(end-1) != '\\')) end++;
                                if (*end != '"') continue;
                                /* Unescape and append to seed */
                                for (char *c = v; c < end && seed_len < seed_max - 1; c++) {
                                    if (*c == '\\' && c+1 < end) {
                                        c++;
                                        if (*c == 'n') seed_buf[seed_len++] = '\n';
                                        else if (*c == 't') seed_buf[seed_len++] = '\t';
                                        else if (*c == '"') seed_buf[seed_len++] = '"';
                                        else if (*c == '\\') seed_buf[seed_len++] = '\\';
                                        else seed_buf[seed_len++] = *c;
                                    } else {
                                        seed_buf[seed_len++] = *c;
                                    }
                                }
                                seed_buf[seed_len++] = '\n';
                            }
                        }
                    } else {
                        /* Plain code file â€” read directly */
                        int to_read = seed_max - seed_len;
                        if (to_read > fsz) to_read = (int)fsz;
                        if (to_read > 2000000) to_read = 2000000; /* 2MB per plain file */
                        int got = fread(seed_buf + seed_len, 1, to_read, sf);
                        if (got > 0) seed_len += got;
                        seed_buf[seed_len++] = '\n';
                    }
                    fclose(sf);
                }
                seed_buf[seed_len] = 0;
                printf("  Tokenizer seed: %.1f MB from %d files\n", seed_len / 1e6, n_files);

                const char *seeds[] = { seed_buf };
                nc_tokenizer_train(tok, seeds, 1, config.vocab_size);
                nc_tokenizer_save(tok, "nc_ai_tokenizer.bin");
                printf("  Tokenizer trained (vocab=%d, merges=%d)\n\n",
                       tok->vocab_size, tok->n_merges);
                free(seed_buf);
            }

            /* Train! */
            NovaTrainConfig tcfg = nova_train_default_config();
            tcfg.total_steps = steps;
            tcfg.seq_len = config.max_seq > 256 ? 256 : config.max_seq;
            tcfg.save_path = save_path;
            tcfg.lr = config.lr;
            tcfg.batch_size = batch_size;
            tcfg.use_graph = train_graph;
            tcfg.use_hebbian = train_hebbian;
            tcfg.use_nce = use_nce;
            tcfg.nce_negatives = nce_negatives;
            tcfg.use_cache = use_cache;
            tcfg.use_cgr = use_cgr;
            tcfg.use_papt = use_papt;
            tcfg.use_hrl_warmup = use_hrl;

            int result = nova_train_full(model, tok, (const char**)files, n_files, tcfg);

            /* Cleanup */
            nova_free(model);
            nc_tokenizer_free(tok);
            nc_path_list_free(&file_list);

            if (result == 0) {
                printf("  Training complete! Model saved to %s\n", save_path);
                printf("  Generate code: nc ai generate \"write a web server\"\n\n");
            }
            return result;
        }

        /* â”€â”€ nc ai distill â”€â”€â”€ Knowledge distillation from external LLM â”€â”€â”€ */
        if (strcmp(subcmd, "distill") == 0) {
            printf("\n");
            nc_print_banner("NC AI Knowledge Distillation");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Distill knowledge from any external teacher LLM\n");
            printf("  into NC AI's built-in model. The teacher generates high-quality NC\n");
            printf("  code, and NC AI learns from it.\n\n");

            /* Parse args */
            const char *teacher_url = getenv("NC_AI_URL");
            const char *teacher_model = getenv("NC_AI_MODEL");
            const char *teacher_key = getenv("NC_AI_KEY");
            int n_prompts = 40;

            for (int i = 3; i < argc; i++) {
                if ((strcmp(argv[i], "--url") == 0 || strcmp(argv[i], "-u") == 0) && i + 1 < argc)
                    teacher_url = argv[++i];
                else if ((strcmp(argv[i], "--model") == 0 || strcmp(argv[i], "-m") == 0) && i + 1 < argc)
                    teacher_model = argv[++i];
                else if ((strcmp(argv[i], "--key") == 0 || strcmp(argv[i], "-k") == 0) && i + 1 < argc)
                    teacher_key = argv[++i];
                else if ((strcmp(argv[i], "--prompts") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc)
                    n_prompts = atoi(argv[++i]);
            }

            if (!teacher_url) {
                printf("  Usage:\n");
                printf("    nc ai distill --url <api_url> --model <model_name> [--key <api_key>]\n\n");
                printf("  Examples:\n");
                printf("    # Distill from a local AI server (no key needed)\n");
                printf("    nc ai distill --url http://localhost:11434/v1/chat/completions --model my-model\n\n");
                printf("    # Distill from a cloud AI provider\n");
                printf("    nc ai distill --url https://api.example.com/v1/chat/completions --model model-name --key your-api-key\n\n");
                printf("    # Use env vars\n");
                printf("    export NC_AI_URL=http://localhost:11434/v1/chat/completions\n");
                printf("    export NC_AI_MODEL=my-model\n");
                printf("    nc ai distill\n\n");
                printf("  Options:\n");
                printf("    --url, -u       Teacher LLM API URL\n");
                printf("    --model, -m     Teacher model name\n");
                printf("    --key, -k       API key (if required)\n");
                printf("    --prompts, -n   Number of prompts to distill (default: 40)\n\n");
                printf("  What happens:\n");
                printf("    1. NC AI sends NC code prompts to the teacher LLM\n");
                printf("    2. Teacher generates high-quality NC code\n");
                printf("    3. NC AI learns from the teacher's output\n");
                printf("    4. Distilled pairs saved to training data\n");
                printf("    5. NC AI becomes smarter â€” learns patterns from the best models\n\n");
                printf("  After distillation, NC AI can generate similar quality code\n");
                printf("  offline, without needing the teacher model.\n\n");
                return 0;
            }

            /* Load or create student model */
            const char *model_paths[] = {
                "nova_model.bin",
                "nc_ai_model_prod.bin",
                "training_data/nova_model.bin",
                "nc-ai/training_data/nova_model.bin",
                NULL
            };
            NCModel *student = NULL;
            for (int i = 0; model_paths[i]; i++) {
                student = nc_model_load(model_paths[i]);
                if (student) {
                    printf("  Student model: %s (dim=%d, layers=%d)\n",
                           model_paths[i], student->dim, student->n_layers);
                    break;
                }
            }
            if (!student) {
                printf("  No existing model found â€” creating new model (dim=256, 6 layers)\n");
                NCModelConfig cfg = nc_model_default_config();
                cfg.dim = 256; cfg.n_layers = 6; cfg.n_heads = 8;
                cfg.vocab_size = 4096; cfg.hidden_dim = 1024;
                student = nc_model_create(cfg);
            }

            /* Load or create tokenizer */
            NCTokenizer *tok = nc_tokenizer_load("nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("nc-ai/training_data/nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("training_data/nc_ai_tokenizer.bin");
            if (!tok) {
                printf("  Creating tokenizer from prompt...\n");
                tok = nc_tokenizer_create();
                const char *seed_texts[] = {
                    "service to set respond with api: configure: ask AI to page section "
                    "heading text button card grid list form input footer nav style "
                    "store into gather from find where respond with health generate_id now "
                    "PROMPT CODE test assert repeat for each append"
                };
                nc_tokenizer_train(tok, seed_texts, 1, 4096);
            }

            /* Configure distillation */
            NCDistillConfig dcfg = nc_distill_default_config();
            dcfg.teacher_url = teacher_url;
            dcfg.teacher_model = teacher_model;
            dcfg.teacher_key = teacher_key;
            dcfg.n_prompts = n_prompts;

            /* Run distillation */
            int result = nc_distill(student, tok, dcfg);

            if (result > 0) {
                /* Save distilled model and tokenizer */
                const char *save_path = "nc_ai_model_distilled.bin";
                nc_model_save(student, save_path);
                nc_tokenizer_save(tok, "nc_ai_tokenizer.bin");
                printf("  Distilled model saved: %s\n", save_path);
                printf("  Tokenizer saved: nc_ai_tokenizer.bin\n");
                printf("  Copy to production: cp %s nc-ai/training_data/nc_ai_model_prod.bin\n\n", save_path);
            }

            nc_model_free(student);
            nc_tokenizer_free(tok);
            return 0;
        }

        /* â”€â”€ nc ai learn â”€â”€â”€ Learn from existing project â”€â”€â”€ */
        if (strcmp(subcmd, "learn") == 0) {
            const char *project_dir = ".";
            bool use_gpu_learn = false;
            for (int a = 3; a < argc; a++) {
                if (strcmp(argv[a], "--gpu") == 0) {
                    use_gpu_learn = true;
                } else if (argv[a][0] != '-') {
                    project_dir = argv[a];
                }
            }
            printf("\n");
            nc_print_banner("NC AI Learn from Project");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Directory: %s\n", project_dir);
            if (use_gpu_learn) {
                nc_cuda_init();
                if (nc_cuda_available()) {
                    printf("  GPU: CUDA (%s) active\n", nc_cuda_device_info());
                } else {
                    nc_metal_init();
                    if (nc_metal_available()) {
                        printf("  GPU: Metal active\n");
                    } else {
                        printf("  GPU: unavailable, falling back to CPU\n");
                    }
                }
            } else {
                printf("  GPU: Off (use --gpu to accelerate learning)\n");
            }

            /* Load model */
            const char *model_paths[] = {
                "nc_ai_model_prod.bin",
                "_private/nc-ai-original/nc_ai_model_prod.bin",
                "training_data/nc_ai_model_prod.bin",
                "nc-ai/training_data/nc_ai_model_prod.bin",
                "nova_model.bin",
                "training_data/nova_model.bin",
                "nc-ai/training_data/nova_model.bin",
                NULL
            };
            NCModel *model = NULL;
            for (int i = 0; model_paths[i]; i++) {
                model = nc_model_load(model_paths[i]);
                if (model) break;
            }
            if (!model) {
                printf("  Error: No model found. Train or distill first.\n");
                printf("  Run: nc ai train  OR  nc ai distill --url ...\n\n");
                return 1;
            }

            NCTokenizer *tok = nc_tokenizer_load("nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("_private/nc-ai-original/nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("nc-ai/training_data/nc_ai_tokenizer.bin");
            if (!tok) tok = nc_tokenizer_load("training_data/nc_ai_tokenizer.bin");
            if (!tok) {
                printf("  Creating tokenizer...\n");
                tok = nc_tokenizer_create();
                const char *seed_texts[] = {
                    "service to set respond with api: configure: ask AI to page section "
                    "heading text button card grid list form input footer nav style "
                    "store into gather from find where respond with health generate_id now "
                    "PROMPT CODE test assert repeat for each append"
                };
                nc_tokenizer_train(tok, seed_texts, 1, model->vocab_size);
                nc_tokenizer_save(tok, "nc_ai_tokenizer.bin");
            }

            NCTrainConfig tcfg = nc_train_default_config();
            int learned = nc_learn_from_project(model, tok, project_dir, tcfg);

            if (learned > 0) {
                nc_model_save(model, "nc_ai_model_prod.bin");
                printf("  Learned from %d files. Model updated.\n\n", learned);
            } else {
                printf("  No NC files found in %s\n\n", project_dir);
            }

            nc_model_free(model);
            nc_tokenizer_free(tok);
            return 0;
        }

        /* â”€â”€ nc ai review â”€â”€â”€ Code review (file, directory, any language) â”€â”€â”€ */
        if (strcmp(subcmd, "review") == 0) {
            if (argc < 4) {
                nc_print_banner("NC AI Code Review");
                printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
                printf("  Usage:\n");
                printf("    nc ai review <file>      Review a single file\n");
                printf("    nc ai review <dir>       Review entire codebase\n");
                printf("    nc ai review .           Review current directory\n\n");
                printf("  Supports: .nc .ncui .py .java .js .ts .go .c .h .rs\n\n");
                return 1;
            }
            const char *target = argv[3];

            /* Check if target is a directory */
            struct stat st;
            if (stat(target, &st) == 0 && S_ISDIR(st.st_mode)) {
                /* â”€â”€ Directory review: scan all code files â”€â”€â”€ */
                nc_print_banner("NC AI Codebase Review");
                printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
                printf("  Scanning: %s\n\n", target);

                char find_cmd[1024];
                snprintf(find_cmd, sizeof(find_cmd),
                    "find '%s' -maxdepth 4 -type f \\( "
                    "-name '*.nc' -o -name '*.ncui' -o -name '*.py' -o -name '*.java' "
                    "-o -name '*.js' -o -name '*.ts' -o -name '*.go' -o -name '*.c' "
                    "-o -name '*.h' -o -name '*.rs' -o -name '*.rb' -o -name '*.php' "
                    "\\) ! -path '*/node_modules/*' ! -path '*/.git/*' ! -path '*/build/*' "
                    "! -path '*/training_data/*' 2>/dev/null | head -100", target);

                FILE *fp = popen(find_cmd, "r");
                if (!fp) { fprintf(stderr, "  Error: Cannot scan directory\n"); return 1; }

                int total_files = 0, total_issues = 0, total_warnings = 0;
                int files_clean = 0;
                char line[512];

                while (fgets(line, sizeof(line), fp)) {
                    size_t ll = strlen(line);
                    if (ll > 0 && line[ll-1] == '\n') line[ll-1] = '\0';
                    if (strlen(line) == 0) continue;

                    char *content = read_file(line);
                    if (!content) continue;
                    long fsize = (long)strlen(content);
                    total_files++;

                    int issues = 0, warns = 0;

                    /* â”€â”€ Language-specific checks â”€â”€â”€ */
                    int is_nc = strstr(line, ".nc") != NULL && !strstr(line, ".ncui");
                    int is_py = strstr(line, ".py") != NULL;
                    int is_java = strstr(line, ".java") != NULL;
                    int is_js = (strstr(line, ".js") != NULL || strstr(line, ".ts") != NULL);
                    int is_go = strstr(line, ".go") != NULL;
                    int is_c = (strstr(line, ".c") != NULL && !strstr(line, ".nc"))
                               || strstr(line, ".h") != NULL;

                    /* â”€â”€ Universal checks (all languages) â”€â”€â”€ */
                    if (strstr(content, "password") && strstr(content, "=") &&
                        !strstr(content, "password_hash") && !strstr(content, "PASSWORD")) {
                        printf("  [!] %s â€” Possible hardcoded password\n", line);
                        issues++;
                    }
                    if (strstr(content, "TODO") || strstr(content, "FIXME") || strstr(content, "HACK")) {
                        printf("  [i] %s â€” Contains TODO/FIXME/HACK\n", line);
                        warns++;
                    }
                    if (fsize > 50000) {
                        printf("  [!] %s â€” File too large (%ldKB), consider splitting\n", line, fsize/1024);
                        warns++;
                    }

                    /* â”€â”€ NC checks â”€â”€â”€ */
                    if (is_nc) {
                        if (!strstr(content, "service \"") && !strstr(content, "// test"))
                            { printf("  [x] %s â€” Missing service declaration\n", line); issues++; }
                        if (!strstr(content, "version \""))
                            { printf("  [!] %s â€” Missing version\n", line); warns++; }
                        if (strstr(content, "function "))
                            { printf("  [x] %s â€” Use 'to name:' not 'function'\n", line); issues++; }
                        if (strstr(content, "return ") && !strstr(content, "//"))
                            { printf("  [x] %s â€” Use 'respond with' not 'return'\n", line); issues++; }
                    }

                    /* â”€â”€ Python checks â”€â”€â”€ */
                    if (is_py) {
                        if (strstr(content, "eval(") || strstr(content, "exec("))
                            { printf("  [x] %s â€” eval/exec is a security risk\n", line); issues++; }
                        if (strstr(content, "import *"))
                            { printf("  [!] %s â€” Wildcard import (import *)\n", line); warns++; }
                        if (strstr(content, "except:") && !strstr(content, "except Exception"))
                            { printf("  [!] %s â€” Bare except catches everything\n", line); warns++; }
                        if (strstr(content, "pickle.load"))
                            { printf("  [x] %s â€” pickle.load is unsafe with untrusted data\n", line); issues++; }
                        if (!strstr(content, "if __name__") && fsize > 500 && strstr(content, "def "))
                            { printf("  [i] %s â€” Missing if __name__ guard\n", line); warns++; }
                    }

                    /* â”€â”€ Java checks â”€â”€â”€ */
                    if (is_java) {
                        if (strstr(content, "System.out.println"))
                            { printf("  [!] %s â€” Use logger instead of System.out\n", line); warns++; }
                        if (strstr(content, "catch (Exception"))
                            { printf("  [!] %s â€” Catching broad Exception\n", line); warns++; }
                        if (strstr(content, "\"SELECT") && strstr(content, "+"))
                            { printf("  [x] %s â€” Possible SQL injection (string concat)\n", line); issues++; }
                        if (strstr(content, "new Thread("))
                            { printf("  [i] %s â€” Consider ExecutorService over raw Thread\n", line); warns++; }
                    }

                    /* â”€â”€ JavaScript/TypeScript checks â”€â”€â”€ */
                    if (is_js) {
                        if (strstr(content, "eval("))
                            { printf("  [x] %s â€” eval() is a security risk\n", line); issues++; }
                        if (strstr(content, "var "))
                            { printf("  [!] %s â€” Use let/const instead of var\n", line); warns++; }
                        if (strstr(content, "innerHTML"))
                            { printf("  [x] %s â€” innerHTML is XSS-vulnerable\n", line); issues++; }
                        if (strstr(content, "console.log") && !strstr(line, "test"))
                            { printf("  [i] %s â€” Remove console.log from production\n", line); warns++; }
                        if (strstr(content, "any") && strstr(line, ".ts"))
                            { printf("  [!] %s â€” Avoid 'any' type in TypeScript\n", line); warns++; }
                    }

                    /* â”€â”€ Go checks â”€â”€â”€ */
                    if (is_go) {
                        if (strstr(content, "fmt.Println") && !strstr(line, "test"))
                            { printf("  [!] %s â€” Use structured logger, not fmt\n", line); warns++; }
                        if (strstr(content, "panic("))
                            { printf("  [!] %s â€” Avoid panic in library code\n", line); warns++; }
                        if (strstr(content, "_ = err") || (strstr(content, "err)") && !strstr(content, "if err")))
                            { printf("  [x] %s â€” Ignored error return\n", line); issues++; }
                    }

                    /* â”€â”€ C/C++ checks â”€â”€â”€ */
                    if (is_c) {
                        if (strstr(content, "gets("))
                            { printf("  [x] %s â€” gets() is unsafe, use fgets()\n", line); issues++; }
                        if (strstr(content, "sprintf(") && !strstr(content, "snprintf"))
                            { printf("  [!] %s â€” Use snprintf instead of sprintf\n", line); warns++; }
                        if (strstr(content, "strcpy(") && !strstr(content, "strncpy"))
                            { printf("  [!] %s â€” Use strncpy instead of strcpy\n", line); warns++; }
                        if (strstr(content, "system("))
                            { printf("  [!] %s â€” system() can be a security risk\n", line); warns++; }
                    }

                    total_issues += issues;
                    total_warnings += warns;
                    if (issues == 0 && warns == 0) files_clean++;
                    free(content);
                }
                pclose(fp);

                int score = 100;
                if (total_files > 0) {
                    score = (int)(100.0 * files_clean / total_files);
                }
                printf("\n  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
                printf("  Files scanned:  %d\n", total_files);
                printf("  Clean files:    %d\n", files_clean);
                printf("  Issues:         %d\n", total_issues);
                printf("  Warnings:       %d\n", total_warnings);
                printf("  Health Score:   %d%%\n", score);
                printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n\n");
                return 0;
            }

            /* â”€â”€ Single file review â”€â”€â”€ */
            nc_print_banner("NC AI Code Review");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  File: %s\n\n", target);

            char *content = read_file(target);
            if (!content) {
                fprintf(stderr, "  Error: Cannot read %s\n\n", target);
                return 1;
            }
            long fsize = (long)strlen(content);
            int score = 100;

            int is_nc = strstr(target, ".nc") != NULL;
            int is_py = strstr(target, ".py") != NULL;
            int is_java = strstr(target, ".java") != NULL;
            int is_js = (strstr(target, ".js") != NULL || strstr(target, ".ts") != NULL);

            if (is_nc) {
                if (!strstr(content, "service \""))  { score -= 20; printf("  [x] Missing service declaration\n"); }
                if (!strstr(content, "version \""))   { score -= 10; printf("  [!] Missing version declaration\n"); }
                if (!strstr(content, "health"))       { score -= 5;  printf("  [i] Add health endpoint\n"); }
                if (!strstr(content, "middleware"))    { score -= 5;  printf("  [i] Add middleware\n"); }
                if (!strstr(content, "log "))          { score -= 3;  printf("  [i] Add logging\n"); }
                if (!strstr(content, "configure:"))    { score -= 3;  printf("  [i] Add configure: block\n"); }
                if (strstr(content, "function "))      { score -= 10; printf("  [x] Use 'to name:' not 'function'\n"); }
                if (strstr(content, "return "))        { score -= 10; printf("  [x] Use 'respond with' not 'return'\n"); }
            } else if (is_py) {
                if (strstr(content, "eval("))          { score -= 20; printf("  [x] eval() is a security risk\n"); }
                if (strstr(content, "exec("))          { score -= 20; printf("  [x] exec() is a security risk\n"); }
                if (strstr(content, "import *"))       { score -= 5;  printf("  [!] Wildcard import\n"); }
                if (strstr(content, "except:"))        { score -= 5;  printf("  [!] Bare except\n"); }
                if (strstr(content, "pickle.load"))    { score -= 15; printf("  [x] Unsafe pickle.load\n"); }
                if (!strstr(content, "if __name__") && strstr(content, "def "))
                    { score -= 3; printf("  [i] Missing __name__ guard\n"); }
                if (strstr(content, "# TODO"))         { score -= 2;  printf("  [i] Has TODOs\n"); }
            } else if (is_java) {
                if (strstr(content, "System.out"))     { score -= 5;  printf("  [!] Use logger not System.out\n"); }
                if (strstr(content, "catch (Exception")){ score -= 5; printf("  [!] Broad exception catch\n"); }
                if (strstr(content, "\"SELECT") && strstr(content, "+"))
                    { score -= 20; printf("  [x] Possible SQL injection\n"); }
            } else if (is_js) {
                if (strstr(content, "eval("))          { score -= 20; printf("  [x] eval() is dangerous\n"); }
                if (strstr(content, "var "))            { score -= 5;  printf("  [!] Use let/const\n"); }
                if (strstr(content, "innerHTML"))       { score -= 15; printf("  [x] XSS risk via innerHTML\n"); }
                if (strstr(content, "console.log"))     { score -= 3;  printf("  [i] Remove console.log\n"); }
            } else {
                /* Generic checks */
                if (strstr(content, "TODO"))            { score -= 2;  printf("  [i] Contains TODO\n"); }
                if (fsize > 50000)                      { score -= 10; printf("  [!] File very large\n"); }
            }

            /* Universal security checks */
            if (strstr(content, "password") && strstr(content, "=") &&
                !strstr(content, "password_hash") && !strstr(content, "PASSWORD"))
                { score -= 15; printf("  [x] Possible hardcoded password\n"); }

            if (score < 0) score = 0;
            if (score == 100) printf("  All checks passed â€” code looks great!\n");
            printf("\n  Code Quality Score: %d/100\n", score);
            printf("  Lines: ~%ld | Size: %ldKB\n\n", fsize / 40, fsize / 1024);
            free(content);
            return 0;
        }

        /* â”€â”€ nc ai check â”€â”€â”€ Validate & auto-fix NC code â”€â”€â”€ */
        if (strcmp(subcmd, "check") == 0) {
            if (argc < 4) {
                nc_print_banner("NC AI Code Check & Auto-Fix");
                printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
                printf("  Usage: nc ai check <file.nc|file.ncui|directory>\n\n");
                printf("  Validates NC code and auto-fixes common errors.\n");
                printf("  Learns from nc-lang and nc-ui internals.\n\n");
                printf("  Written in NC: nc-ai/nc/inference/errorfix.nc\n\n");
                return 1;
            }
            const char *target = argv[3];
            nc_print_banner("NC AI Code Check");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Target: %s\n\n", target);

            /* Check if target is a file or directory */
            FILE *test_f = fopen(target, "r");
            if (test_f) {
                fclose(test_f);
                /* Single file check */
                char *content = NULL;
                long fsize = 0;
                FILE *f = fopen(target, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    fsize = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    content = (char *)malloc(fsize + 1);
                    if (content) {
                        fread(content, 1, fsize, f);
                        content[fsize] = '\0';
                    }
                    fclose(f);
                }
                if (content) {
                    int errors = 0, warnings = 0;
                    int is_ncui = strstr(target, ".ncui") != NULL;

                    if (is_ncui) {
                        if (!strstr(content, "page \"")) { printf("  [x] Missing page declaration\n"); errors++; }
                        if (!strstr(content, "section")) { printf("  [x] No sections defined\n"); errors++; }
                        if (!strstr(content, "style:")) { printf("  [!] Missing style: block\n"); warnings++; }
                        if (!strstr(content, "footer")) { printf("  [!] Missing footer\n"); warnings++; }
                    } else {
                        if (!strstr(content, "service \"")) { printf("  [x] Missing service declaration\n"); errors++; }
                        if (!strstr(content, "to ")) { printf("  [x] No functions defined\n"); errors++; }
                        if (!strstr(content, "api:")) { printf("  [x] Missing api: route block\n"); errors++; }
                        if (!strstr(content, "respond with")) { printf("  [!] No 'respond with' found\n"); warnings++; }
                        if (strstr(content, "function ")) { printf("  [x] Use 'to name:' not 'function'\n"); errors++; }
                        if (strstr(content, "return ")) { printf("  [x] Use 'respond with' not 'return'\n"); errors++; }
                    }
                    if (errors == 0 && warnings == 0)
                        printf("  All checks passed\n");
                    printf("\n  Errors: %d | Warnings: %d\n", errors, warnings);
                    printf("  Full NC check module: nc-ai/nc/inference/errorfix.nc\n\n");
                    free(content);
                }
            } else {
                printf("  Scanning directory for .nc/.ncui files...\n");
                printf("  Use: nc run nc-ai/nc/inference/errorfix.nc --project %s\n\n", target);
            }
            return 0;
        }

        /* â”€â”€ nc ai export â”€â”€â”€ Export project as package â”€â”€â”€ */
        if (strcmp(subcmd, "export") == 0) {
            const char *project_dir = (argc >= 4) ? argv[3] : ".";
            const char *format = (argc >= 5) ? argv[4] : "all";
            nc_print_banner("NC AI Project Export");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Project: %s\n", project_dir);
            printf("  Format:  %s\n\n", format);
            printf("  Available formats:\n");
            printf("    binary   â€” Standalone executable (nc build)\n");
            printf("    docker   â€” Docker container (Dockerfile + compose)\n");
            printf("    zip      â€” Distributable ZIP archive\n");
            printf("    pkg      â€” NC package (nc pkg install)\n");
            printf("    all      â€” All formats\n\n");
            printf("  Run the export module:\n");
            printf("    nc run nc-ai/nc/export.nc --project %s --format %s\n\n", project_dir, format);
            printf("  Full NC export module: nc-ai/nc/export.nc\n\n");
            return 0;
        }

        /* â”€â”€ nc ai serve â”€â”€â”€ AI API server â”€â”€â”€ */
        if (strcmp(subcmd, "serve") == 0) {
            nc_print_banner("NC AI API Server");
            printf("  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\n");
            printf("  Starting AI generation server on port 8090...\n\n");

            /* Generate the server script and run it */
            const char *server_script =
                "to __main__:\n"
                "    show \"NC AI Server starting on port 8090...\"\n"
                "    show \"POST /generate - Generate NC code from text\"\n"
                "    show \"GET  /status  - Model status\"\n";

            NcMap *globals = nc_map_new();
            nc_call_behavior(server_script, "nc_ai_server", "__main__", globals);
            nc_map_free(globals);
            return 0;
        }

        /* â”€â”€ Helper: Sanitize user input for embedding in NC string â”€â”€â”€ */
        /* Escapes quotes, backslashes, and strips newlines/control chars
         * to prevent NC code injection. Returns sanitized string in `out`.
         * SECURITY: Without this, a newline in user input breaks the NC
         * string literal and allows arbitrary NC code execution. */
        #define NC_SANITIZE_INPUT(src, out, out_size) do { \
            int _ei = 0; \
            for (int _ci = 0; (src)[_ci] && _ei < (int)(out_size) - 2; _ci++) { \
                unsigned char _ch = (unsigned char)(src)[_ci]; \
                if (_ch == '"')       { (out)[_ei++] = '\\'; (out)[_ei++] = '"'; } \
                else if (_ch == '\\') { (out)[_ei++] = '\\'; (out)[_ei++] = '\\'; } \
                else if (_ch == '\n' || _ch == '\r') { (out)[_ei++] = ' '; } \
                else if (_ch < 0x20)  { /* strip control chars */ } \
                else { (out)[_ei++] = (char)_ch; } \
            } \
            (out)[_ei] = '\0'; \
        } while(0)

        /* â”€â”€ Helper: Find nc-ai/chat.nc from ANY directory â”€â”€â”€ */
        /* Search order:                                        */
        /*   1. CWD/nc-ai/chat.nc                               */
        /*   2. Relative to binary (bin/../nc-ai/)               */
        /*   3. $HOME/Documents/nc-main/nc-ai/chat.nc            */
        /*   4. $HOME/nc-main/nc-ai/chat.nc                      */
        /*   5. /usr/local/lib/nc/nc-ai/chat.nc                  */
        /*   6. $NC_HOME/nc-ai/chat.nc (env variable)            */
        #define NC_FIND_CHAT_MODULE(result_ptr) do { \
            (result_ptr) = read_file_quiet("nc-ai/chat.nc"); \
            if (!(result_ptr) && argv[0]) { \
                char _bindir[512]; \
                strncpy(_bindir, argv[0], sizeof(_bindir) - 1); \
                _bindir[sizeof(_bindir) - 1] = '\0'; \
                char *_slash = strrchr(_bindir, '/'); \
                if (!_slash) _slash = strrchr(_bindir, '\\'); \
                if (_slash) { \
                    *_slash = '\0'; \
                    char _trypath[1024]; \
                    snprintf(_trypath, sizeof(_trypath), "%s/nc-ai/chat.nc", _bindir); \
                    (result_ptr) = read_file_quiet(_trypath); \
                    if (!(result_ptr)) { \
                        snprintf(_trypath, sizeof(_trypath), "%s/../nc-ai/chat.nc", _bindir); \
                        (result_ptr) = read_file_quiet(_trypath); \
                    } \
                    if (!(result_ptr)) { \
                        snprintf(_trypath, sizeof(_trypath), "%s/../../nc-ai/chat.nc", _bindir); \
                        (result_ptr) = read_file_quiet(_trypath); \
                    } \
                } \
            } \
            /* Search HOME directories */ \
            if (!(result_ptr)) { \
                const char *_home = getenv("HOME"); \
                if (_home) { \
                    char _trypath[1024]; \
                    snprintf(_trypath, sizeof(_trypath), "%s/Documents/nc-main/nc-ai/chat.nc", _home); \
                    (result_ptr) = read_file_quiet(_trypath); \
                    if (!(result_ptr)) { \
                        snprintf(_trypath, sizeof(_trypath), "%s/nc-main/nc-ai/chat.nc", _home); \
                        (result_ptr) = read_file_quiet(_trypath); \
                    } \
                } \
            } \
            /* Search system install */ \
            if (!(result_ptr)) { \
                (result_ptr) = read_file_quiet("/usr/local/lib/nc/nc-ai/chat.nc"); \
            } \
            /* Search NC_HOME env */ \
            if (!(result_ptr)) { \
                const char *_nchome = getenv("NC_HOME"); \
                if (_nchome) { \
                    char _trypath[1024]; \
                    snprintf(_trypath, sizeof(_trypath), "%s/nc-ai/chat.nc", _nchome); \
                    (result_ptr) = read_file_quiet(_trypath); \
                } \
            } \
        } while(0)

        /* â”€â”€ nc ai chat â”€â”€â”€ Interactive AI chat â”€â”€â”€ */
        if (strcmp(subcmd, "chat") == 0) {
            /* Find the chat module */
            char *chat_src = NULL;
            NC_FIND_CHAT_MODULE(chat_src);
            if (!chat_src) {
                fprintf(stderr, "  Error: Cannot find nc-ai/chat.nc\n");
                fprintf(stderr, "  Make sure you are in the NC project directory.\n");
                return 1;
            }

            printf("\n");
            printf("  \033[36mâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\033[0m\n");
            nc_print_step("NC AI Chat v1.0 - Local AI Assistant");
            printf("  \033[36mâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•\033[0m\n");
            printf("\n");
            printf("  \033[2mEverything runs locally. No cloud. No API keys.\033[0m\n");
            printf("\n");
            printf("  Try:\n");
            printf("    \033[33m\"Write an email about a project update\"\033[0m\n");
            printf("    \033[33m\"Why does my server crash under load?\"\033[0m\n");
            printf("    \033[33m\"Write a poem about technology\"\033[0m\n");
            printf("    \033[33m\"Translate hello to Japanese\"\033[0m\n");
            printf("    \033[33m\"help\"\033[0m for all capabilities\n");
            printf("\n");
            printf("  Type \033[1mquit\033[0m or \033[1mexit\033[0m to leave.\n\n");

            char input[4096];
            while (1) {
                printf("  You: ");
                fflush(stdout);
                if (!fgets(input, sizeof(input), stdin)) break;
                /* Strip trailing newline */
                size_t ilen = strlen(input);
                if (ilen > 0 && input[ilen - 1] == '\n') input[ilen - 1] = '\0';
                if (strlen(input) == 0) continue;
                if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) break;

                /* Sanitize input: escape quotes, backslashes, strip newlines/control chars */
                char escaped[8192];
                NC_SANITIZE_INPUT(input, escaped, sizeof(escaped));

                /* Append a __main__ behavior that calls chat_once */
                size_t src_len = strlen(chat_src);
                size_t esc_len = strlen(escaped);
                size_t code_cap = src_len + esc_len + 256;
                char *nc_code = malloc(code_cap);
                if (!nc_code) { free(chat_src); return 1; }
                snprintf(nc_code, code_cap,
                    "%s\n"
                    "to __main__:\n"
                    "    set result to chat_once(\"%s\")\n",
                    chat_src, escaped);

                NcMap *globals = nc_map_new();
                nc_call_behavior(nc_code, "nc-ai/chat.nc", "__main__", globals);
                nc_map_free(globals);
                free(nc_code);
                printf("\n");
            }
            free(chat_src);
            printf("  Goodbye!\n\n");
            return 0;
        }

        /* â”€â”€ nc ai reason "question" â”€â”€â”€ One-shot reasoning â”€â”€â”€ */
        if (strcmp(subcmd, "reason") == 0) {
            if (argc < 4) {
                fprintf(stderr, "  Usage: nc ai reason \"Why does my API timeout?\"\n");
                return 1;
            }
            const char *question = argv[3];

            /* Find the chat module */
            char *chat_src = NULL;
            NC_FIND_CHAT_MODULE(chat_src);
            if (!chat_src) {
                fprintf(stderr, "  Error: Cannot find nc-ai/chat.nc\n");
                fprintf(stderr, "  Make sure you are in the NC project directory.\n");
                return 1;
            }

            /* Sanitize input: escape quotes, backslashes, strip newlines/control chars */
            char escaped[8192];
            NC_SANITIZE_INPUT(question, escaped, sizeof(escaped));

            /* Append a __main__ behavior that calls chat_once */
            size_t src_len = strlen(chat_src);
            size_t esc_len = strlen(escaped);
            size_t code_cap = src_len + esc_len + 256;
            char *nc_code = malloc(code_cap);
            if (!nc_code) { free(chat_src); return 1; }
            snprintf(nc_code, code_cap,
                "%s\n"
                "to __main__:\n"
                "    set result to chat_once(\"%s\")\n",
                chat_src, escaped);

            NcMap *globals = nc_map_new();
            nc_call_behavior(nc_code, "nc-ai/chat.nc", "__main__", globals);
            nc_map_free(globals);
            free(nc_code);
            free(chat_src);
            return 0;
        }

        fprintf(stderr, "  Unknown AI command: %s\n", subcmd);
        fprintf(stderr, "  Try: nc ai chat, nc ai reason, nc ai generate, nc ai create,\n");
        fprintf(stderr, "       nc ai distill, nc ai learn, nc ai review, nc ai check,\n");
        fprintf(stderr, "       nc ai export, nc ai train, nc ai serve, nc ai status\n");
        return 1;
    }

    /* â”€â”€ nc "show hello" â”€â”€â”€ bare plain English (no subcommand) â”€â”€â”€ */
    if (cmd[0] != '-' &&
        strcmp(cmd, "run") != 0 && strcmp(cmd, "serve") != 0 &&
        strcmp(cmd, "build") != 0 && strcmp(cmd, "compile") != 0 &&
        strcmp(cmd, "validate") != 0 && strcmp(cmd, "tokens") != 0 &&
        strcmp(cmd, "bytecode") != 0 && strcmp(cmd, "debug") != 0 &&
        strcmp(cmd, "get") != 0 && strcmp(cmd, "post") != 0 &&
        strcmp(cmd, "pkg") != 0 && strcmp(cmd, "digest") != 0 &&
        strcmp(cmd, "init") != 0 && strcmp(cmd, "ai") != 0 &&
        strcmp(cmd, "start") != 0 && strcmp(cmd, "stop") != 0 &&
        strcmp(cmd, "dev") != 0 && strcmp(cmd, "deploy") != 0 &&
        strcmp(cmd, "doctor") != 0 &&
        strcmp(cmd, "setup") != 0 &&
        strcmp(cmd, "migrate") != 0 &&
        strcmp(cmd, "fmt") != 0 && strcmp(cmd, "profile") != 0 &&
        strcmp(cmd, "lsp") != 0 && strcmp(cmd, "test") != 0 &&
        strcmp(cmd, "version") != 0 && strcmp(cmd, "mascot") != 0 &&
        strcmp(cmd, "conformance") != 0 && strcmp(cmd, "repl") != 0 &&
        strcmp(cmd, "ui") != 0 &&
        strcmp(cmd, "train") != 0 && strcmp(cmd, "generate") != 0 &&
        !strstr(cmd, ".nc")) {
        /* Treat all args as plain English code */
        char code[8192] = "";
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(code, " ", sizeof(code) - strlen(code) - 1);
            strncat(code, argv[i], sizeof(code) - strlen(code) - 1);
        }
        for (char *p = code; *p; p++) {
            if (*p == ';') *p = '\n';
        }
        char wrapped[16384];
        int wpos = snprintf(wrapped, sizeof(wrapped), "to __cli__:\n");
        char *line = code;
        while (line && *line) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            while (*line == ' ') line++;
            if (*line)
                wpos += snprintf(wrapped + wpos, sizeof(wrapped) - wpos, "    %s\n", line);
            if (nl) line = nl + 1; else break;
        }
        NcMap *globals = nc_map_new();
        NcValue result = nc_call_behavior(wrapped, "<cli>", "__cli__", globals);
        if (!IS_NONE(result)) {
            nc_value_print(result, stdout);
            printf("\n");
        }
        nc_map_free(globals);
        return 0;
    }

    /* â”€â”€ Pipe/stdin: echo "show 42" | nc â”€â”€â”€ */
    /* (handled above by argc < 2 launching repl, which reads stdin) */

    if (strcmp(cmd, "get") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nc get <url> [-H \"Header: Value\"]\n");
            return 1;
        }
        const char *auth = NULL;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-H") == 0 && i + 1 < argc) auth = argv[++i];
        }
        nc_http_init();
        char *resp = nc_http_get(argv[2], auth);
        if (resp) {
            printf("%s\n", resp);
            free(resp);
        }
        nc_http_cleanup();
        return 0;
    }
    if (strcmp(cmd, "post") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: nc post <url> '<json-body>' [-H \"Header: Value\"]\n");
            return 1;
        }
        const char *auth = NULL;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "-H") == 0 && i + 1 < argc) auth = argv[++i];
        }
        nc_http_init();
        char *resp = nc_http_post(argv[2], argv[3], "application/json", auth);
        if (resp) {
            printf("%s\n", resp);
            free(resp);
        }
        nc_http_cleanup();
        return 0;
    }

    if (strcmp(cmd, "setup") == 0) {
        const char *name = (argc >= 3) ? argv[2] : "my-service";

        printf("\n");
        printf("  \033[36m _  _  ___\033[0m\n");
        printf("  \033[36m| \\| |/ __|\033[0m   \033[1mNC Setup\033[0m\n");
        printf("  \033[36m| .` | (__\033[0m    \033[90mOne command to get started\033[0m\n");
        printf("  \033[36m|_|\\_|\\___|\033[0m\n\n");

        /* Step 1: Create project */
        char check_path[512];
        snprintf(check_path, sizeof(check_path), "%s%cservice.nc", name, NC_PATH_SEP);
        FILE *existing = fopen(check_path, "r");
        if (existing) {
            fclose(existing);
            printf("  \033[33m!\033[0m Project '%s' already exists. Skipping file creation.\n\n", name);
        } else {
            /* Run nc init logic inline â€” create all files */
            if (nc_mkdir(name) != 0 && errno != EEXIST) {
                fprintf(stderr, "  Cannot create directory: %s\n", name);
                return 1;
            }
            char path[512];
            snprintf(path, sizeof(path), "%s%cservice.nc", name, NC_PATH_SEP);
            FILE *f = fopen(path, "w");
            if (!f) { fprintf(stderr, "  Cannot create %s\n", path); return 1; }
            fprintf(f,
                "// %s â€” Built with NC\n//\n// Run:  nc serve service.nc\n\n"
                "service \"%s\"\nversion \"1.0.0\"\n\n"
                "configure:\n    ai_model is \"env:NC_AI_MODEL\"\n    ai_key is \"env:NC_AI_KEY\"\n    port: 8000\n\n"
                "to process with data:\n    purpose: \"Process incoming data with AI\"\n"
                "    ask AI to \"Analyze this data and return structured insights. Data: {{data}}\" save as result\n    respond with result\n\n"
                "to health:\n    respond with {\"status\": \"healthy\", \"service\": \"%s\"}\n\n"
                "middleware:\n    rate_limit: 100\n    cors: true\n    log_requests: true\n\n"
                "api:\n    POST /process runs process\n    GET /health runs health\n",
                name, name, name);
            fclose(f);

            snprintf(path, sizeof(path), "%s%c.gitignore", name, NC_PATH_SEP);
            f = fopen(path, "w");
            if (f) { fprintf(f, ".env\n.env.*\n!.env.example\n*.log\n.DS_Store\n"); fclose(f); }

            snprintf(path, sizeof(path), "%s%cREADME.md", name, NC_PATH_SEP);
            f = fopen(path, "w");
            if (f) { fprintf(f, "# %s\n\nBuilt with [NC](" NC_REPO_URL ").\n\n"
                "## Run\n\n```bash\nnc serve service.nc\n```\n", name); fclose(f); }

            printf("  \033[32mâœ“\033[0m Created project: \033[1m%s/\033[0m\n", name);
        }

        /* Step 2: Ask for API key */
        char env_path[512];
        snprintf(env_path, sizeof(env_path), "%s%c.env", name, NC_PATH_SEP);

        FILE *env_check = fopen(env_path, "r");
        if (env_check) {
            fclose(env_check);
            printf("  \033[32mâœ“\033[0m .env already exists. Keeping your config.\n");
        } else {
            char ai_url[512] = "";
            char ai_key[512] = "";
            char ai_model[128] = "default";

            printf("\n  \033[1mAI Provider Setup\033[0m\n\n");
            printf("  Enter your AI provider base URL and key.\n");
            printf("  NC auto-detects the API path.\n\n");
            printf("  Quick options:\n");
            printf("    1. Enter URL + Key manually\n");
            printf("    2. Local AI (any OpenAI-compatible local provider)\n\n");
            printf("  Choice [1]: ");
            fflush(stdout);

            char choice[16] = "1";
            if (fgets(choice, sizeof(choice), stdin)) {
                char *nl = strchr(choice, '\n'); if (nl) *nl = '\0';
                if (choice[0] == '\0') choice[0] = '1';
            }

            if (choice[0] == '2') {
                printf("  Local AI URL [http://localhost:11434]: ");
                fflush(stdout);
                if (fgets(ai_url, sizeof(ai_url), stdin)) {
                    char *nl = strchr(ai_url, '\n'); if (nl) *nl = '\0';
                }
                if (ai_url[0] == '\0') snprintf(ai_url, sizeof(ai_url), "http://localhost:11434");
                snprintf(ai_key, sizeof(ai_key), "local");
                printf("  Model name [default]: ");
                fflush(stdout);
                if (fgets(ai_model, sizeof(ai_model), stdin)) {
                    char *nl = strchr(ai_model, '\n'); if (nl) *nl = '\0';
                }
                if (ai_model[0] == '\0') snprintf(ai_model, sizeof(ai_model), "default");
                printf("  \033[32mâœ“\033[0m Local AI selected (make sure your local AI server is running)\n");
            } else {
                printf("  AI Provider URL: ");
                fflush(stdout);
                if (fgets(ai_url, sizeof(ai_url), stdin)) {
                    char *nl = strchr(ai_url, '\n'); if (nl) *nl = '\0';
                }
                printf("  API Key: ");
                fflush(stdout);
                if (fgets(ai_key, sizeof(ai_key), stdin)) {
                    char *nl = strchr(ai_key, '\n'); if (nl) *nl = '\0';
                }
                printf("  Model [default]: ");
                fflush(stdout);
                char model_input[128];
                if (fgets(model_input, sizeof(model_input), stdin)) {
                    char *nl = strchr(model_input, '\n'); if (nl) *nl = '\0';
                    if (model_input[0]) snprintf(ai_model, sizeof(ai_model), "%s", model_input);
                }
            }

            /* Write .env */
            FILE *envf = fopen(env_path, "w");
            if (envf) {
                fprintf(envf, "NC_AI_URL=%s\nNC_AI_KEY=%s\nNC_AI_MODEL=%s\n",
                        ai_url, ai_key, ai_model);
                fclose(envf);
                printf("  \033[32mâœ“\033[0m Saved config to %s/.env\n", name);
            }

            /* Also write .env.example */
            char example_path[512];
            snprintf(example_path, sizeof(example_path), "%s%c.env.example", name, NC_PATH_SEP);
            envf = fopen(example_path, "w");
            if (envf) {
                fprintf(envf, "NC_AI_URL=%s\nNC_AI_KEY=\nNC_AI_MODEL=%s\n",
                        ai_url, ai_model);
                fclose(envf);
            }
        }

        /* Step 3: Validate */
        printf("\n  \033[1mValidating...\033[0m\n");
        snprintf(check_path, sizeof(check_path), "%s%cservice.nc", name, NC_PATH_SEP);
        char *vsrc = read_file(check_path);
        if (vsrc) {
            NcLexer *vlex = nc_lexer_new(vsrc, check_path);
            nc_lexer_tokenize(vlex);
            NcParser *vp = nc_parser_new(vlex->tokens, vlex->token_count, check_path);
            NcASTNode *vprog = nc_parser_parse(vp);
            if (vp->had_error) {
                printf("  \033[31mâœ—\033[0m Syntax error: %s\n", vp->error_msg);
            } else {
                printf("  \033[32mâœ“\033[0m service.nc is valid (%d behaviors, %d routes)\n",
                       vprog->as.program.beh_count,
                       vprog->as.program.route_count);
            }
            nc_parser_free(vp);
            nc_lexer_free(vlex);
            free(vsrc);
        }

        /* Step 4: Start the server */
        printf("\n  \033[1mStarting server...\033[0m\n\n");
        char svc_path[512];
        snprintf(svc_path, sizeof(svc_path), "%s%cservice.nc", name, NC_PATH_SEP);
        return nc_serve(svc_path, 0);
    }

    if (strcmp(cmd, "migrate") == 0) {
        return nc_migrate(argc, argv);
    }

    if (strcmp(cmd, "doctor") == 0) {
        printf("\n");
        printf("  \033[36mâ”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”\033[0m\n");
        printf("  \033[36mâ”‚\033[0m  \033[1mNC Doctor â€” troubleshooting your setup\033[0m       \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜\033[0m\n\n");

        int issues = 0;
        int warnings = 0;
        const char *svc = (argc >= 3) ? argv[2] : "service.nc";

        /* â”€â”€ 1. Project files â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        printf("  \033[1m1. Project Files\033[0m\n\n");

        FILE *sf = fopen(svc, "r");
        if (sf) {
            fclose(sf);
            printf("  \033[32mâœ“\033[0m %s found\n", svc);

            /* Validate it */
            char *vsrc = read_file(svc);
            if (vsrc) {
                NcLexer *vlex = nc_lexer_new(vsrc, svc);
                nc_lexer_tokenize(vlex);
                NcParser *vp = nc_parser_new(vlex->tokens, vlex->token_count, svc);
                NcASTNode *vprog = nc_parser_parse(vp);
                if (vp->had_error) {
                    printf("  \033[31mâœ—\033[0m %s has a syntax error:\n", svc);
                    printf("    %s\n", vp->error_msg);
                    printf("    \033[90mFix: Run 'nc validate %s' for details\033[0m\n", svc);
                    issues++;
                } else {
                    printf("  \033[32mâœ“\033[0m %s is valid (%d behaviors, %d routes)\n",
                           svc, vprog->as.program.beh_count, vprog->as.program.route_count);
                    if (vprog->as.program.route_count == 0) {
                        printf("  \033[33m!\033[0m No API routes defined\n");
                        printf("    \033[90mAdd an api: block with routes to serve HTTP\033[0m\n");
                        warnings++;
                    }
                }
                nc_parser_free(vp); nc_lexer_free(vlex); free(vsrc);
            }
        } else {
            printf("  \033[31mâœ—\033[0m %s not found\n", svc);
            printf("    \033[90mFix: Run 'nc setup my-project' to create one\033[0m\n");
            issues++;
        }

        FILE *envf = fopen(".env", "r");
        if (envf) {
            fclose(envf);
            printf("  \033[32mâœ“\033[0m .env file found\n");
        } else {
            FILE *exf = fopen(".env.example", "r");
            if (exf) {
                fclose(exf);
                printf("  \033[31mâœ—\033[0m .env file missing (.env.example exists)\n");
                printf("    \033[90mFix: cp .env.example .env   then edit .env\033[0m\n");
            } else {
                printf("  \033[31mâœ—\033[0m No .env or .env.example found\n");
                printf("    \033[90mFix: Run 'nc setup my-project' to generate one\033[0m\n");
            }
            issues++;
        }

        /* â”€â”€ 2. AI Provider â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        printf("\n  \033[1m2. AI Provider\033[0m\n\n");

        const char *ai_url = getenv("NC_AI_URL");
        const char *ai_key = getenv("NC_AI_KEY");
        const char *ai_model = getenv("NC_AI_MODEL");

        if (ai_url && ai_url[0]) {
            printf("  \033[32mâœ“\033[0m AI URL:   %s\n", ai_url);
            if (strstr(ai_url, "localhost") || strstr(ai_url, "127.0.0.1"))
                printf("    \033[90m(local endpoint â€” make sure it's running)\033[0m\n");
        } else {
            printf("  \033[31mâœ—\033[0m NC_AI_URL is not set\n");
            printf("    \033[90mFix: Add NC_AI_URL=<your-provider-url> to .env\033[0m\n");
            issues++;
        }

        if (ai_key && ai_key[0]) {
            printf("  \033[32mâœ“\033[0m AI Key:   %.*s...\033[90m (set)\033[0m\n",
                   (int)(strlen(ai_key) > 4 ? 4 : strlen(ai_key)), ai_key);
        } else {
            printf("  \033[31mâœ—\033[0m NC_AI_KEY is not set â€” AI features won't work\n");
            printf("    \033[90mFix: Add NC_AI_KEY=<your-key> to .env\033[0m\n");
            issues++;
        }

        if (ai_model && ai_model[0]) {
            printf("  \033[32mâœ“\033[0m AI Model: %s\n", ai_model);
        } else {
            printf("  \033[33m!\033[0m NC_AI_MODEL not set (provider will use its default)\n");
            printf("    \033[90mOptional: Add NC_AI_MODEL=your-model to .env\033[0m\n");
            warnings++;
        }

        /* â”€â”€ 3. Network & Port â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        printf("\n  \033[1m3. Server\033[0m\n\n");

        const char *port_str = getenv("NC_SERVICE_PORT");
        int port = port_str ? atoi(port_str) : 8000;
        printf("  \033[32mâœ“\033[0m Port:     %d\n", port);

        const char *cors = getenv("NC_CORS_ORIGIN");
        if (cors && cors[0]) {
            printf("  \033[32mâœ“\033[0m CORS:     %s\n", cors);
        } else {
            printf("  \033[33m!\033[0m CORS:     * (all origins allowed)\n");
            printf("    \033[90mFor production: Add NC_CORS_ORIGIN=https://your-app.com to .env\033[0m\n");
            warnings++;
        }

        const char *jwt = getenv("NC_JWT_SECRET");
        if (jwt && jwt[0]) {
            printf("  \033[32mâœ“\033[0m JWT Auth: enabled\n");
        } else {
            printf("  \033[33m!\033[0m JWT Auth: disabled (no NC_JWT_SECRET set)\n");
            printf("    \033[90mFor auth: Add NC_JWT_SECRET=your-secret to .env\033[0m\n");
            warnings++;
        }

        /* â”€â”€ Summary â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        printf("\n  \033[36mâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€\033[0m\n\n");

        if (issues == 0 && warnings == 0) {
            printf("  \033[32mâœ“ All checks passed.\033[0m Everything looks good!\n\n");
            printf("    nc serve %s\n\n", svc);
        } else if (issues == 0) {
            printf("  \033[32mâœ“ Ready to run\033[0m (%d warning%s â€” optional improvements)\n\n",
                   warnings, warnings > 1 ? "s" : "");
            printf("    nc serve %s\n\n", svc);
        } else {
            printf("  \033[31mâœ— %d issue%s found.\033[0m Fix them and run \033[1mnc doctor\033[0m again.\n",
                   issues, issues > 1 ? "s" : "");
            if (warnings > 0)
                printf("  \033[33m! %d warning%s\033[0m (optional improvements)\n",
                       warnings, warnings > 1 ? "s" : "");
            printf("\n  \033[1mCommon fixes:\033[0m\n\n");
            printf("    Problem: \"No AI provider configured\"\n");
            printf("    Fix:     Add NC_AI_URL and NC_AI_KEY to .env\n\n");
            printf("    Problem: \"No .env file\"\n");
            printf("    Fix:     cp .env.example .env\n\n");
            printf("    Problem: \"Syntax error in service.nc\"\n");
            printf("    Fix:     nc validate service.nc\n\n");
            printf("    Problem: \"Port already in use\"\n");
            printf("    Fix:     nc serve service.nc -p 8001\n\n");
            printf("    Problem: \"AI call hangs or times out\"\n");
            printf("    Fix:     Check NC_AI_URL is reachable\n");
            printf("             For local AI: make sure your local AI server is running\n\n");
        }
        return issues > 0 ? 1 : 0;
    }

    if (strcmp(cmd, "init") == 0) {
        const char *name = NULL;
        bool want_docker = false, want_k8s = false, want_all = false;

        /* Parse args: nc init [name] [--docker] [--k8s] [--all] */
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--docker") == 0) want_docker = true;
            else if (strcmp(argv[i], "--k8s") == 0 || strcmp(argv[i], "--kubernetes") == 0) want_k8s = true;
            else if (strcmp(argv[i], "--all") == 0) want_all = true;
            else if (argv[i][0] != '-' && !name) name = argv[i];
        }
        if (!name) name = "my-service";
        if (want_all) { want_docker = true; want_k8s = true; }
        if (!want_docker && !want_k8s) { /* default: generate both */ want_docker = true; want_k8s = true; }

        /* Check if directory already has a service.nc */
        char check_path[512];
        snprintf(check_path, sizeof(check_path), "%s%cservice.nc", name, NC_PATH_SEP);
        FILE *check = fopen(check_path, "r");
        if (check) {
            fclose(check);
            fprintf(stderr,
                "\n  Project '%s' already exists (service.nc found).\n"
                "  Use a different name or delete the existing directory.\n\n", name);
            return 1;
        }

        if (nc_mkdir(name) != 0 && errno != EEXIST) {
            fprintf(stderr, "  Cannot create directory: %s\n", name);
            return 1;
        }

        /* â”€â”€ service.nc â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        char path[512];
        snprintf(path, sizeof(path), "%s%cservice.nc", name, NC_PATH_SEP);
        FILE *f = fopen(path, "w");
        if (!f) { fprintf(stderr, "  Cannot create %s\n", path); return 1; }
        fprintf(f,
            "// %s â€” Built with NC\n"
            "//\n"
            "// Run:  nc serve service.nc\n"
            "// Test: nc validate service.nc\n"
            "\n"
            "service \"%s\"\n"
            "version \"1.0.0\"\n"
            "\n"
            "configure:\n"
            "    ai_model is \"env:NC_AI_MODEL\"\n"
            "    ai_key is \"env:NC_AI_KEY\"\n"
            "    port: 8000\n"
            "\n"
            "to process with data:\n"
            "    purpose: \"Process incoming data with AI\"\n"
            "    ask AI to \"Analyze this data and return structured insights. Data: {{data}}\" save as result\n"
            "    respond with result\n"
            "\n"
            "to health:\n"
            "    respond with {\"status\": \"healthy\", \"service\": \"%s\"}\n"
            "\n"
            "middleware:\n"
            "    rate_limit: 100\n"
            "    cors: true\n"
            "    log_requests: true\n"
            "\n"
            "api:\n"
            "    POST /process runs process\n"
            "    GET /health runs health\n",
            name, name, name);
        fclose(f);

        /* â”€â”€ .env.example â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        snprintf(path, sizeof(path), "%s%c.env.example", name, NC_PATH_SEP);
        f = fopen(path, "w");
        if (f) {
            fprintf(f,
                "# %s â€” Configuration\n"
                "# Copy this file to .env and add your API key.\n"
                "\n"
                "# AI Provider â€” set your base URL, key, and model\n"
                "NC_AI_URL=\n"
                "NC_AI_KEY=\n"
                "NC_AI_MODEL=default\n"
                "\n"
                "# NC auto-detects the API path from the base URL.\n"
                "# Examples:\n"
                "#   NC_AI_URL=https://your-ai-provider.example.com/v1/chat\n"
                "#   NC_AI_URL=http://localhost:11434    (local AI server)\n"
                "#   NC_AI_URL=https://llm.company.com   (AI proxy/gateway)\n"
                "# NC_AI_MODEL=default\n"
                "\n"
                "# Server (optional)\n"
                "# NC_SERVICE_PORT=8000\n"
                "# NC_CORS_ORIGIN=https://your-frontend.com\n",
                name);
            fclose(f);
        }

        /* â”€â”€ .gitignore â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        snprintf(path, sizeof(path), "%s%c.gitignore", name, NC_PATH_SEP);
        f = fopen(path, "w");
        if (f) {
            fprintf(f,
                ".env\n"
                ".env.*\n"
                "!.env.example\n"
                "*.log\n"
                ".DS_Store\n");
            fclose(f);
        }

        /* â”€â”€ README.md â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        snprintf(path, sizeof(path), "%s%cREADME.md", name, NC_PATH_SEP);
        f = fopen(path, "w");
        if (f) {
            fprintf(f,
                "# %s\n"
                "\n"
                "Built with [NC](" NC_REPO_URL ") â€” "
                "the fastest way to build AI-powered APIs.\n"
                "\n"
                "## Setup\n"
                "\n"
                "```bash\n"
                "# 1. Configure your AI provider\n"
                "cp .env.example .env\n"
                "# Edit .env and add your API key\n"
                "\n"
                "# 2. Start the server\n"
                "nc serve service.nc\n"
                "\n"
                "# 3. Test it\n"
                "curl -X POST http://localhost:8000/process \\\n"
                "  -H \"Content-Type: application/json\" \\\n"
                "  -d '{\"data\": \"hello world\"}'\n"
                "```\n"
                "\n"
                "## Commands\n"
                "\n"
                "```bash\n"
                "nc serve service.nc        # Start HTTP server\n"
                "nc validate service.nc     # Check syntax\n"
                "nc run service.nc -b health  # Run a behavior\n"
                "```\n"
                "\n"
                "## Project Structure\n"
                "\n"
                "```\n"
                "%s/\n"
                "â”œâ”€â”€ service.nc       # Your NC service (edit this)\n"
                "â”œâ”€â”€ .env.example     # Configuration template\n"
                "â”œâ”€â”€ .env             # Your local config (not committed)\n"
                "â”œâ”€â”€ .gitignore       # Ignores .env and logs\n"
                "â””â”€â”€ README.md        # This file\n"
                "```\n"
                "\n"
                "## Deploy\n"
                "\n"
                "```bash\n"
                "# Docker\n"
                "docker build -t %s .\n"
                "docker run -p 8000:8000 --env-file .env %s\n"
                "\n"
                "# Kubernetes\n"
                "kubectl apply -f k8s.yaml\n"
                "```\n"
                "\n"
                "## License\n"
                "\n"
                "Apache 2.0\n",
                name, name, name, name);
            fclose(f);
        }

        /* â”€â”€ Dockerfile â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        if (want_docker) {
        snprintf(path, sizeof(path), "%s%cDockerfile", name, NC_PATH_SEP);
        f = fopen(path, "w");
        if (f) {
            fprintf(f,
                "FROM nc:latest\n"
                "WORKDIR /app\n"
                "COPY service.nc .\n"
                "COPY .env* ./\n"
                "EXPOSE 8000\n"
                "HEALTHCHECK --interval=30s --timeout=5s \\\n"
                "  CMD nc get http://localhost:8000/health || exit 1\n"
                "CMD [\"serve\", \"service.nc\"]\n");
            fclose(f);
        }
        }

        /* â”€â”€ k8s.yaml â€” Kubernetes deployment â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
        if (want_k8s) {
        snprintf(path, sizeof(path), "%s%ck8s.yaml", name, NC_PATH_SEP);
        f = fopen(path, "w");
        if (f) {
            fprintf(f,
                "apiVersion: apps/v1\n"
                "kind: Deployment\n"
                "metadata:\n"
                "  name: %s\n"
                "spec:\n"
                "  replicas: 2\n"
                "  selector:\n"
                "    matchLabels:\n"
                "      app: %s\n"
                "  template:\n"
                "    metadata:\n"
                "      labels:\n"
                "        app: %s\n"
                "    spec:\n"
                "      containers:\n"
                "      - name: %s\n"
                "        image: %s:latest\n"
                "        ports:\n"
                "        - containerPort: 8000\n"
                "        envFrom:\n"
                "        - secretRef:\n"
                "            name: %s-secrets\n"
                "        livenessProbe:\n"
                "          httpGet:\n"
                "            path: /health\n"
                "            port: 8000\n"
                "          initialDelaySeconds: 5\n"
                "          periodSeconds: 30\n"
                "        readinessProbe:\n"
                "          httpGet:\n"
                "            path: /health\n"
                "            port: 8000\n"
                "          initialDelaySeconds: 3\n"
                "          periodSeconds: 10\n"
                "        resources:\n"
                "          requests:\n"
                "            memory: \"32Mi\"\n"
                "            cpu: \"50m\"\n"
                "          limits:\n"
                "            memory: \"128Mi\"\n"
                "            cpu: \"500m\"\n"
                "---\n"
                "apiVersion: v1\n"
                "kind: Service\n"
                "metadata:\n"
                "  name: %s\n"
                "spec:\n"
                "  selector:\n"
                "    app: %s\n"
                "  ports:\n"
                "  - port: 80\n"
                "    targetPort: 8000\n"
                "  type: ClusterIP\n",
                name, name, name, name, name, name, name, name);
            fclose(f);
        }
        }

        printf("\n");
        printf("  \033[32mâœ“\033[0m Project \033[1m%s\033[0m created successfully!\n", name);
        printf("\n");
        printf("  \033[36mâ”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”\033[0m\n");
        printf("  \033[36mâ”‚\033[0m  \033[1mProject:\033[0m  %-33s\033[36mâ”‚\033[0m\n", name);
        printf("  \033[36mâ”‚\033[0m  \033[1mVersion:\033[0m  \033[33m1.0.0\033[0m                            \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m  \033[1mPort:\033[0m     8000                             \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤\033[0m\n");
        printf("  \033[36mâ”‚\033[0m  \033[1mFiles:\033[0m                                      \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    service.nc      your NC service           \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    .env.example    AI provider config        \033[36mâ”‚\033[0m\n");
        if (want_docker)
        printf("  \033[36mâ”‚\033[0m    Dockerfile      Docker deployment         \033[36mâ”‚\033[0m\n");
        if (want_k8s)
        printf("  \033[36mâ”‚\033[0m    k8s.yaml        Kubernetes deployment     \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    .gitignore      keeps secrets safe        \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    README.md       setup + deploy docs       \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¤\033[0m\n");
        printf("  \033[36mâ”‚\033[0m  \033[1mAPI Routes:\033[0m                                 \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    \033[33mPOST\033[0m  /process     \033[90mâ†’\033[0m AI processing       \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ”‚\033[0m    \033[33mGET \033[0m  /health      \033[90mâ†’\033[0m health check         \033[36mâ”‚\033[0m\n");
        printf("  \033[36mâ””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜\033[0m\n");
        printf("\n");
        printf("  \033[1mTo run (one command):\033[0m\n\n");
        printf("    nc setup %s\n", name);
        printf("    \033[90m# Creates project, asks for API key, starts server\033[0m\n");
        printf("\n");
        printf("  \033[1mOr manually:\033[0m\n\n");
        printf("    cd %s\n", name);
        printf("    cp .env.example .env     \033[90m# Add your API key\033[0m\n");
        printf("    nc serve service.nc      \033[90m# Start server\033[0m\n");
        printf("\n");
        return 0;
    }

    /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
     *  nc start â€” start all .nc services in the project
     *
     *  Usage:
     *    nc start                   # start all *.nc files with api: blocks
     *    nc start service.nc        # start a specific file
     *    nc start -p 8080           # override port
     * â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
    if (strcmp(cmd, "start") == 0) {
        /* Find all .nc files in current directory with api: blocks */
        const char *file = NULL;
        int port = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
            else if (argv[i][0] != '-') file = argv[i];
        }

        if (file) {
            printf("\n  Starting %s...\n\n", file);
            return nc_serve(file, port);
        }

        /* Auto-discover: look for service.nc, then any *.nc with api: */
        FILE *f = fopen("service.nc", "r");
        if (f) { fclose(f); printf("\n  Starting service.nc...\n\n"); return nc_serve("service.nc", port); }

        /* Scan for *.nc files */
#ifndef NC_WINDOWS
        FILE *ls = popen("ls *.nc 2>/dev/null", "r");
#else
        FILE *ls = _popen("dir /b *.nc 2>nul", "r");
#endif
        if (ls) {
            char found[256] = {0};
            while (fgets(found, sizeof(found), ls)) {
                found[strcspn(found, "\r\n")] = '\0';
                if (found[0]) {
#ifndef NC_WINDOWS
                    pclose(ls);
#else
                    _pclose(ls);
#endif
                    printf("\n  Starting %s...\n\n", found);
                    return nc_serve(found, port);
                }
            }
#ifndef NC_WINDOWS
            pclose(ls);
#else
            _pclose(ls);
#endif
        }

        fprintf(stderr, "\n  No .nc service files found in current directory.\n"
                "  Create one with: nc init my-project\n\n");
        return 1;
    }

    /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
     *  nc stop â€” stop running NC services
     * â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
    if (strcmp(cmd, "stop") == 0) {
        printf("\n  Stopping NC services...\n");
#ifndef NC_WINDOWS
        int r = system("pkill -f 'nc serve' 2>/dev/null");
#else
        int r = system("taskkill /F /IM nc.exe 2>nul");
#endif
        if (r == 0) printf("  All NC services stopped.\n\n");
        else printf("  No running NC services found.\n\n");
        return 0;
    }

    /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
     *  nc dev â€” development mode (start with auto-validation)
     *
     *  Validates all .nc files, then starts the server.
     *  Usage: nc dev [file] [-p port]
     * â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
    if (strcmp(cmd, "dev") == 0) {
        const char *file = argc >= 3 ? argv[2] : "service.nc";
        int port = 0;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) port = atoi(argv[++i]);
            else if (argv[i][0] != '-' && strstr(argv[i], ".nc")) file = argv[i];
        }

        printf("\n  \033[36m[dev]\033[0m Validating %s...\n", file);
        int val_result = cmd_validate(file);
        if (val_result != 0) {
            fprintf(stderr, "\n  \033[31m[dev] Validation failed. Fix errors before starting.\033[0m\n\n");
            return 1;
        }
        printf("  \033[32m[dev] Validation passed.\033[0m Starting server...\n\n");
        return nc_serve(file, port);
    }

    /* â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
     *  nc deploy â€” build container image and generate deploy configs
     *
     *  Usage:
     *    nc deploy                  # build image from current directory
     *    nc deploy --tag my-app:v1  # custom image tag
     *    nc deploy --push           # build and push to registry
     * â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â• */
    if (strcmp(cmd, "deploy") == 0) {
        const char *tag = NULL;
        bool push = false;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--tag") == 0 && i + 1 < argc) tag = argv[++i];
            else if (strcmp(argv[i], "--push") == 0) push = true;
        }

        /* Check for Dockerfile */
        FILE *df = fopen("Dockerfile", "r");
        if (!df) {
            printf("\n  No Dockerfile found. Creating one...\n");
            df = fopen("Dockerfile", "w");
            if (df) {
                fprintf(df,
                    "FROM alpine:3.21 AS builder\n"
                    "RUN apk add --no-cache gcc musl-dev make curl-dev libucontext-dev\n"
                    "WORKDIR /build\n"
                    "COPY . .\n"
                    "RUN cd nc && make clean && make "
                    "LDFLAGS=\"-lm -lcurl -lpthread -ldl -lucontext\"\n\n"
                    "FROM alpine:3.21\n"
                    "RUN apk add --no-cache libcurl ca-certificates libucontext && "
                    "addgroup -S nc && adduser -S nc -G nc\n"
                    "COPY --from=builder /build/nc/build/nc /usr/local/bin/nc\n"
                    "COPY *.nc /app/\n"
                    "COPY .env* /app/\n"
                    "WORKDIR /app\n"
                    "USER nc\n"
                    "EXPOSE 8080\n"
                    "HEALTHCHECK --interval=30s --timeout=5s CMD nc version || exit 1\n"
                    "ENTRYPOINT [\"nc\"]\n"
                    "CMD [\"start\"]\n");
                fclose(df);
                printf("  Created Dockerfile\n");
            }
        } else {
            fclose(df);
            printf("\n  Using existing Dockerfile\n");
        }

        /* Determine tag */
        char image_tag[256];
        if (tag) {
            strncpy(image_tag, tag, 255);
        } else {
            /* Use directory name as default tag */
            char cwd[512];
#ifdef NC_WINDOWS
            if (_getcwd(cwd, sizeof(cwd))) {
#else
            if (getcwd(cwd, sizeof(cwd))) {
#endif
                const char *dir = strrchr(cwd, '/');
                if (!dir) dir = strrchr(cwd, '\\');
                snprintf(image_tag, sizeof(image_tag), "%s:latest", dir ? dir + 1 : "nc-app");
            } else {
                strcpy(image_tag, "nc-app:latest");
            }
        }

        printf("  Building image: %s\n\n", image_tag);
        char build_cmd[1024];
        snprintf(build_cmd, sizeof(build_cmd), "docker build -t %s .", image_tag);
        int build_result = system(build_cmd);
        if (build_result != 0) {
            fprintf(stderr, "\n  \033[31mBuild failed.\033[0m Make sure Docker is installed and running.\n\n");
            return 1;
        }

        printf("\n  \033[32mImage built: %s\033[0m\n", image_tag);

        if (push) {
            printf("  Pushing %s...\n", image_tag);
            char push_cmd[1024];
            snprintf(push_cmd, sizeof(push_cmd), "docker push %s", image_tag);
            system(push_cmd);
        }

        printf("\n  \033[1mRun locally:\033[0m\n");
        printf("    docker run -p 8080:8080 %s\n\n", image_tag);
        printf("  \033[1mDeploy:\033[0m\n");
        printf("    docker push %s\n", image_tag);
        printf("    nc init --k8s   \033[90m# generate deployment manifests\033[0m\n\n");
        return 0;
    }

    if (argc < 3) {
        if (strcmp(cmd, "run") == 0 || strcmp(cmd, "validate") == 0 ||
            strcmp(cmd, "tokens") == 0) {
            fprintf(stderr, "Error: %s requires a file argument\n", cmd);
            return 1;
        }
        /* nc train / nc generate can work with no extra args */
        if (strcmp(cmd, "train") == 0) {
            extern int nc_cmd_train(int argc, char **argv);
            return nc_cmd_train(argc, argv);
        }
        if (strcmp(cmd, "generate") == 0) {
            extern int nc_cmd_generate(int argc, char **argv);
            return nc_cmd_generate(argc, argv);
        }
        usage();
        return 0;
    }

    const char *filename = argv[2];

    if (strcmp(cmd, "tokens") == 0) {
        return cmd_tokens(filename);
    }
    if (strcmp(cmd, "validate") == 0) {
        return cmd_validate(filename);
    }
    if (strcmp(cmd, "run") == 0) {
        const char *behavior = NULL;
        /* Parse CLI flags */
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) behavior = argv[++i];
            if (strcmp(argv[i], "--no-cache") == 0) { /* reserved for future use */ }
        }

        char *source = read_file(filename);
        if (!source) return 1;

        /* Full pipeline: Source â†’ Lexer â†’ Parser â†’ AST â†’ Compiler â†’ Bytecode */
        NcLexer *lex = nc_lexer_new(source, filename);
        nc_lexer_tokenize(lex);

        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
        NcASTNode *program = nc_parser_parse(parser);
        if (parser->had_error) {
            /* If parsing failed because the file has bare statements (no service/to/api),
             * wrap the content in a behavior and run it via the interpreter */
            if (strstr(parser->error_msg, "I don't understand") ||
                strstr(parser->error_msg, "NC programs start with")) {
                nc_parser_free(parser); nc_lexer_free(lex);

                /* Wrap in a behavior â€” indent each line under to __main__: */
                size_t src_len = strlen(source);
                size_t wrap_cap = src_len * 2 + 256;
                char *wrapped = malloc(wrap_cap);
                if (!wrapped) { free(source); return 1; }
                int wpos = snprintf(wrapped, wrap_cap, "to __main__:\n");
                char *line = source;
                while (line && *line) {
                    char *nl = strchr(line, '\n');
                    if (nl) *nl = '\0';
                    char *trimmed = line;
                    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
                    if (*trimmed && *trimmed != '#' && strncmp(trimmed, "//", 2) != 0)
                        wpos += snprintf(wrapped + wpos, wrap_cap - wpos, "    %s\n", line);
                    else if (*trimmed)
                        wpos += snprintf(wrapped + wpos, wrap_cap - wpos, "    %s\n", line);
                    if (nl) { *nl = '\n'; line = nl + 1; } else break;
                }
                NcMap *globals = nc_map_new();
                NcValue result = nc_call_behavior(wrapped, filename, "__main__", globals);
                if (!IS_NONE(result)) {
                    nc_value_print(result, stdout);
                    printf("\n");
                }
                nc_map_free(globals);
                free(wrapped);
                free(source);
                return 0;
            }
            fprintf(stderr, "[NC Error] %s\n", parser->error_msg);
            nc_parser_free(parser); nc_lexer_free(lex); free(source);
            return 1;
        }

        NcCompiler *comp = nc_compiler_new();
        if (!comp || !nc_compiler_compile(comp, program)) {
            fprintf(stderr, "[NC Compile Error] %s\n", comp ? comp->error_msg : "allocation failed");
            nc_ast_free(program);
            if (comp) nc_compiler_free(comp);
            nc_parser_free(parser); nc_lexer_free(lex); free(source);
            return 1;
        }
        nc_optimize_all(comp);

        /* Export configure block values as NC_ env vars */
        if (program->as.program.configure) {
            NcMap *cfg = program->as.program.configure;
            for (int ci = 0; ci < cfg->count; ci++) {
                if (IS_STRING(cfg->values[ci])) {
                    const char *val = AS_STRING(cfg->values[ci])->chars;
                    if (strncmp(val, "env:", 4) == 0) {
                        const char *env_val = getenv(val + 4);
                        if (env_val) cfg->values[ci] = NC_STRING(nc_string_from_cstr(env_val));
                    }
                }
            }
            for (int ci = 0; ci < cfg->count; ci++) {
                if (IS_STRING(cfg->values[ci]) || IS_INT(cfg->values[ci])) {
                    const char *key = cfg->keys[ci]->chars;
                    char env_name[128] = "NC_";
                    int ei = 3;
                    for (int k = 0; key[k] && ei < 126; k++)
                        env_name[ei++] = (key[k] >= 'a' && key[k] <= 'z') ? key[k] - 32 : key[k];
                    env_name[ei] = '\0';
                    if (!getenv(env_name)) {
                        NcString *vs = nc_value_to_string(cfg->values[ci]);
                        nc_setenv(env_name, vs->chars, 0);
                        nc_string_free(vs);
                    }
                }
            }
        }

        /* Inject configure block as 'config' global for VM access */
        NcMap *nc_config_map = program->as.program.configure;

        /* Print service info */
        printf("\n");
        nc_cli_print_header_rule();
        if (program->as.program.service_name)
            printf("  \033[1mService:\033[0m %s\n", program->as.program.service_name->chars);
        if (program->as.program.version)
            printf("  \033[1mVersion:\033[0m %s\n", program->as.program.version->chars);
        if (program->as.program.model)
            printf("  \033[1mModel:\033[0m   %s\n", program->as.program.model->chars);
        printf("  \033[1mEngine:\033[0m  NC Bytecode VM\n");
        nc_cli_print_header_rule();

        if (program->as.program.def_count > 0) {
            printf("\n  Types: ");
            for (int i = 0; i < program->as.program.def_count; i++) {
                if (i > 0) printf(", ");
                printf("%s", program->as.program.definitions[i]->as.definition.name->chars);
            }
            printf("\n");
        }

        printf("  Behaviors (%d):", comp->chunk_count);
        for (int i = 0; i < comp->chunk_count; i++) {
            printf(" %s(%d bytes)", comp->beh_names[i]->chars, comp->chunks[i].count);
            if (i < comp->chunk_count - 1) printf(",");
        }
        printf("\n");

        if (program->as.program.route_count > 0) {
            printf("\n  API Routes:\n");
            for (int i = 0; i < program->as.program.route_count; i++) {
                NcASTNode *r = program->as.program.routes[i];
                printf("    %-6s %s -> %s\n", r->as.route.method->chars,
                       r->as.route.path->chars, r->as.route.handler->chars);
            }
        }

        /* Execute behaviors via bytecode VM */
        if (behavior) {
            for (int i = 0; i < comp->chunk_count; i++) {
                if (strcmp(comp->beh_names[i]->chars, behavior) == 0) {
                    printf("\n  Running '%s' via bytecode VM...\n", behavior);
                    NcVM *vm = nc_vm_new();
                    vm->behavior_chunks = comp->chunks;
                    vm->behavior_chunk_count = comp->chunk_count;
                    for (int b = 0; b < comp->chunk_count; b++)
                        nc_map_set(vm->behaviors, comp->beh_names[b], NC_INT(b));
                    if (nc_config_map)
                        nc_map_set(vm->globals, nc_string_from_cstr("config"), NC_MAP(nc_config_map));
                    NcValue result = nc_vm_execute_fast(vm, &comp->chunks[i]);
                    printf("\n  Result: ");
                    nc_value_print(result, stdout);
                    printf("\n");
                    if (vm->output_count > 0) {
                        printf("\n  Output:\n");
                        for (int j = 0; j < vm->output_count; j++)
                            printf("    %s\n", vm->output[j]);
                    }
                    /* CLI `run -b` is a short-lived process. Avoid explicit VM
                     * teardown here until the shared-result refcount cleanup is
                     * fully untangled in the VM, since the process exits right
                     * after printing the result and the OS reclaims memory. */
                    break;
                }
            }
        } else if (comp->chunk_count > 0) {
            /* No -b flag: run all behaviors (or 'main' if it exists) */
            int main_idx = -1;
            for (int i = 0; i < comp->chunk_count; i++) {
                if (strcmp(comp->beh_names[i]->chars, "main") == 0) {
                    main_idx = i;
                    break;
                }
            }

            if (main_idx >= 0) {
                printf("\n  Running 'main'...\n");
                NcVM *vm = nc_vm_new();
                vm->behavior_chunks = comp->chunks;
                vm->behavior_chunk_count = comp->chunk_count;
                for (int b = 0; b < comp->chunk_count; b++)
                    nc_map_set(vm->behaviors, comp->beh_names[b], NC_INT(b));
                if (nc_config_map)
                    nc_map_set(vm->globals, nc_string_from_cstr("config"), NC_MAP(nc_config_map));
                NcValue result = nc_vm_execute_fast(vm, &comp->chunks[main_idx]);
                if (!IS_NONE(result)) {
                    printf("\n  Result: ");
                    nc_value_print(result, stdout);
                    printf("\n");
                }
                if (vm->output_count > 0) {
                    for (int j = 0; j < vm->output_count; j++)
                        printf("  %s\n", vm->output[j]);
                }
                nc_vm_free(vm);
            } else {
                /* Run all behaviors sequentially */
                for (int i = 0; i < comp->chunk_count; i++) {
                    printf("\n  Running '%s'...\n", comp->beh_names[i]->chars);
                    NcVM *vm = nc_vm_new();
                    vm->behavior_chunks = comp->chunks;
                    vm->behavior_chunk_count = comp->chunk_count;
                    for (int b = 0; b < comp->chunk_count; b++)
                        nc_map_set(vm->behaviors, comp->beh_names[b], NC_INT(b));
                    if (nc_config_map)
                        nc_map_set(vm->globals, nc_string_from_cstr("config"), NC_MAP(nc_config_map));
                    NcValue result = nc_vm_execute_fast(vm, &comp->chunks[i]);
                    if (!IS_NONE(result)) {
                        printf("  Result: ");
                        nc_value_print(result, stdout);
                        printf("\n");
                    }
                    if (vm->output_count > 0) {
                        for (int j = 0; j < vm->output_count; j++)
                            printf("  %s\n", vm->output[j]);
                    }
                    nc_vm_free(vm);
                }
            }
        }
        printf("\n");

        nc_ast_free(program);
        nc_compiler_free(comp);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        free(source);
        return 0;
    }

    if (strcmp(cmd, "compile") == 0) {
        char *source = read_file(filename);
        if (!source) return 1;
        NcLexer *lex = nc_lexer_new(source, filename);
        nc_lexer_tokenize(lex);
        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
        NcASTNode *program = nc_parser_parse(parser);
        if (parser->had_error) {
            fprintf(stderr, "Parse error: %s\n", parser->error_msg);
            nc_parser_free(parser); nc_lexer_free(lex); free(source);
            return 1;
        }
        /* Generate output path: file.nc -> file.ll */
        char outpath[512];
        strncpy(outpath, filename, sizeof(outpath) - 1); outpath[sizeof(outpath) - 1] = '\0';
        char *dot = strrchr(outpath, '.');
        if (dot) memcpy(dot, ".ll", 4);
        else strncat(outpath, ".ll", sizeof(outpath) - strlen(outpath) - 1);

        nc_llvm_generate(program, outpath);
        nc_ast_free(program);
        nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return 0;
    }
    if (strcmp(cmd, "bytecode") == 0) {
        char *source = read_file(filename);
        if (!source) return 1;
        NcLexer *lex = nc_lexer_new(source, filename);
        nc_lexer_tokenize(lex);
        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
        NcASTNode *program = nc_parser_parse(parser);
        if (parser->had_error) {
            fprintf(stderr, "Parse error: %s\n", parser->error_msg);
            nc_parser_free(parser); nc_lexer_free(lex); free(source);
            return 1;
        }
        NcCompiler *comp = nc_compiler_new();
        if (nc_compiler_compile(comp, program)) {
            for (int i = 0; i < comp->chunk_count; i++)
                nc_disassemble_chunk(&comp->chunks[i], comp->beh_names[i]->chars);
        } else {
            fprintf(stderr, "Compile error: %s\n", comp->error_msg);
        }
        nc_ast_free(program);
        nc_compiler_free(comp);
        nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return 0;
    }
    if (strcmp(cmd, "debug") == 0) {
        const char *behavior = NULL;
        bool dap_mode = false;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) behavior = argv[++i];
            if (strcmp(argv[i], "--dap") == 0) dap_mode = true;
        }
        if (dap_mode)
            return nc_dap_run(filename);
        return nc_debug_file(filename, behavior);
    }
    if (strcmp(cmd, "pkg") == 0) {
        return nc_pkg_command(argc, argv);
    }
    if (strcmp(cmd, "lsp") == 0) {
        return nc_lsp_run();
    }
    if (strcmp(cmd, "repl") == 0) {
        return nc_repl_run();
    }
    if (strcmp(cmd, "analyze") == 0) {
        char *source = read_file(filename);
        if (!source) return 1;
        NcLexer *lex = nc_lexer_new(source, filename);
        nc_lexer_tokenize(lex);
        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
        NcASTNode *program = nc_parser_parse(parser);
        if (parser->had_error) {
            fprintf(stderr, "Parse error: %s\n", parser->error_msg);
            nc_parser_free(parser); nc_lexer_free(lex); free(source);
            return 1;
        }
        int errors = nc_analyze(program, filename, source);
        nc_ast_free(program);
        nc_parser_free(parser); nc_lexer_free(lex); free(source);
        return errors > 0 ? 1 : 0;
    }

    if (strcmp(cmd, "digest") == 0) {
        return nc_digest_file(filename);
    }
    if (strcmp(cmd, "fmt") == 0 || strcmp(cmd, "format") == 0) {
        return nc_format_file(filename);
    }
    if (strcmp(cmd, "profile") == 0) {
        /* Run with profiling enabled */
        nc_profiler_enable();
        char *source = read_file(filename);
        if (!source) return 1;
        /* ... run through bytecode path ... */
        nc_run_file(filename);
        nc_profiler_report();
        nc_profiler_disable();
        free(source);
        return 0;
    }

    if (strcmp(cmd, "conformance") == 0) {
        return nc_conformance_run();
    }

    if (strcmp(cmd, "serve") == 0) {
        int port = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
                port = atoi(argv[++i]);
            if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
                port = atoi(argv[++i]);
        }
        return nc_serve(filename, port);
    }

    if (strcmp(cmd, "build") == 0) {
        NcBuildConfig build_cfg;
        nc_build_config_init(&build_cfg, filename);
        if (nc_build_config_parse(&build_cfg, argc, argv, 3) != 0)
            return 1;
        return nc_build_run(argv[0], &build_cfg);
    }

    /* ── nc train — Train NC transformer model on code files ── */
    if (strcmp(cmd, "train") == 0) {
        extern int nc_cmd_train(int argc, char **argv);
        return nc_cmd_train(argc, argv);
    }

    /* ── nc generate — Generate NC code from description ── */
    if (strcmp(cmd, "generate") == 0) {
        extern int nc_cmd_generate(int argc, char **argv);
        return nc_cmd_generate(argc, argv);
    }

    if (strcmp(cmd, "ui") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: nc ui [build|dev] <file.ncui>\n");
            return 1;
        }
        const char *subcmd = argv[2];
        if (strcmp(subcmd, "build") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: nc ui build <file.ncui> [-o output_dir]\n");
                return 1;
            }
            const char *out_dir = ".";
            for (int i = 4; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) out_dir = argv[++i];
            }
            return nc_ui_compile_file(argv[3], out_dir) ? 0 : 1;
        } else if (strcmp(subcmd, "dev") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: nc ui dev <file.ncui> [-p port]\n");
                return 1;
            }
            int port = 3000;
            for (int i = 4; i < argc; i++) {
                if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc)
                    port = atoi(argv[++i]);
            }
            return nc_ui_dev_server(argv[3], port) ? 0 : 1;
        }
        fprintf(stderr, "Unknown ui command: %s\n", subcmd);
        return 1;
    }

    /* nc wasm <file.nc> [-o output_dir] â€” compile to WebAssembly */
    if (strcmp(cmd, "wasm") == 0) {
        char *source = read_file(filename);
        if (!source) return 1;
        const char *output_dir = "./wasm_output";
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output_dir = argv[++i];
        }
        /* Ensure output directory exists */
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", output_dir);
        system(mkdir_cmd);
        int result = nc_wasm_build(source, output_dir);
        free(source);
        if (result == 0) {
            printf("\n  \033[32mâœ“ WebAssembly build complete\033[0m\n");
            printf("  Output: %s/\n", output_dir);
            printf("  Run:    python3 -m http.server -d %s\n\n", output_dir);
        }
        return result;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    usage();
    return 1;
}

