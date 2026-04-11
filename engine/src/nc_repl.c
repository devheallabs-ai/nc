/*
 * nc_repl.c — Interactive Read-Eval-Print Loop for NC.
 *
 * Works like Python's interactive mode with full line editing:
 *   - Arrow keys (left/right) for cursor movement
 *   - Arrow keys (up/down) for command history
 *   - Tab completion, Ctrl-A/E, Ctrl-K, etc.
 *
 *   $ nc
 *   NC 1.0.0 (Mar 2026) [C11] on Darwin
 *   Type "help", "vars", "clear", or "quit"
 *   >>> show "hello world"
 *   hello world
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"

#define printf nc_printf

#ifndef NC_WINDOWS
#include <sys/utsname.h>
#endif

#if !defined(NC_NO_REPL) && !defined(NC_WINDOWS)
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* Minimal readline fallback for Windows and systems without readline.
 * Provides basic line editing via fgets — no history or arrow keys,
 * but allows the REPL to function on all platforms. */
#if defined(NC_NO_REPL) || defined(NC_WINDOWS)
static char *nc_basic_readline(const char *prompt) {
    static char line_buf[4096];
    fprintf(stdout, "%s", prompt);
    fflush(stdout);
    if (!fgets(line_buf, sizeof(line_buf), stdin)) return NULL;
    size_t len = strlen(line_buf);
    if (len > 0 && line_buf[len - 1] == '\n') line_buf[len - 1] = '\0';
    char *copy = malloc(strlen(line_buf) + 1);
    if (copy) strcpy(copy, line_buf);
    return copy;
}
#define readline nc_basic_readline
#define add_history(x) ((void)(x))
#endif

#include "../include/nc_version.h"

#define REPL_LINE_MAX 4096

int nc_repl_run(void) {
    const char *os_name = "unknown";
#ifdef NC_WINDOWS
    os_name = "Windows";
#else
    struct utsname uname_data;
    if (uname(&uname_data) == 0) os_name = uname_data.sysname;
#endif

    printf("\n");
    printf("  \033[36m _  _  ___\033[0m\n");
    printf("  \033[36m| \\| |/ __|\033[0m   \033[1m\033[36mNC REPL\033[0m \033[33mv%s\033[0m \033[90m[C11] on %s\033[0m\n", NC_VERSION, os_name);
    printf("  \033[36m| .` | (__\033[0m    \033[90mInteractive Notation-as-Code\033[0m\n");
    printf("  \033[36m|_|\\_|\\___|\033[0m\n");
    printf("\n");
    printf("  \033[90mType \033[0m\033[33mhelp\033[0m\033[90m for commands, \033[0m\033[33mquit\033[0m\033[90m to exit.\033[0m\n");
    printf("  \033[90mWrite plain English: \033[0mset name to \"world\"\033[90m then \033[0mshow \"Hello, \" + name\n\n");

    NcMap *globals = nc_map_new();

    char buffer[REPL_LINE_MAX * 10];
    buffer[0] = '\0';
    bool in_block = false;

    while (true) {
        char *input = readline(in_block ? "... " : ">>> ");

        if (!input) {
            printf("\n");
            break;
        }

        /* Add non-empty lines to readline history */
        if (input[0] != '\0') add_history(input);

        /* Skip empty lines outside blocks */
        if (!in_block && input[0] == '\0') {
            free(input);
            continue;
        }

        /* Built-in commands */
        if (!in_block) {
            if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0 ||
                strcmp(input, "quit()") == 0 || strcmp(input, "exit()") == 0) {
                free(input);
                break;
            }
            if (strcmp(input, "help") == 0 || strcmp(input, "help()") == 0) {
                printf("NC — Plain English programming language for AI\n\n");
                printf("Statements:\n");
                printf("  set x to 42                   Assign a variable\n");
                printf("  show x + 8                    Print a value\n");
                printf("  show \"Hello, \" + name         String concatenation\n");
                printf("  set items to [1, 2, 3]        Create a list\n");
                printf("  show len(items)               Call a function\n\n");
                printf("Control flow:\n");
                printf("  if x is above 10:             Conditional (end block with empty line)\n");
                printf("      show \"big\"\n");
                printf("  repeat 5 times:               Loop\n");
                printf("      show \"hello\"\n\n");
                printf("Commands:\n");
                printf("  help          Show this help\n");
                printf("  vars          Show all variables\n");
                printf("  clear         Clear all variables\n");
                printf("  quit          Exit\n");
                free(input);
                continue;
            }
            if (strcmp(input, "vars") == 0 || strcmp(input, "vars()") == 0) {
                if (globals->count == 0) {
                    printf("(no variables)\n");
                } else {
                    for (int i = 0; i < globals->count; i++) {
                        printf("%s = ", globals->keys[i]->chars);
                        nc_value_print(globals->values[i], stdout);
                        printf("\n");
                    }
                }
                free(input);
                continue;
            }
            if (strcmp(input, "clear") == 0 || strcmp(input, "clear()") == 0) {
                nc_map_free(globals);
                globals = nc_map_new();
                printf("Variables cleared.\n");
                free(input);
                continue;
            }
        }

        /* Empty line ends a block */
        if (in_block && input[0] == '\0') {
            in_block = false;
            free(input);
            goto execute;
        }

        /* Detect block start (line ends with :) */
        int line_len = (int)strlen(input);
        if (line_len > 0 && input[line_len - 1] == ':') {
            strncat(buffer, input, sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            in_block = true;
            free(input);
            continue;
        }

        /* Accumulate indented lines in block */
        if (in_block) {
            strncat(buffer, input, sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            free(input);
            continue;
        }

        /* Single-line execution */
        strncpy(buffer, input, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        free(input);

execute:
        if (buffer[0] == '\0') continue;

        /* ── Input validation ─────────────────────────────────
         * NC statements must start with a known keyword.
         * Bare words like "ghgh" or "hello" are not valid NC.
         * We check the first word against all valid statement
         * starters before sending to the interpreter.
         */
        {
            char trimmed[REPL_LINE_MAX];
            strncpy(trimmed, buffer, sizeof(trimmed) - 1);
            trimmed[sizeof(trimmed) - 1] = '\0';
            char *t = trimmed;
            while (*t == ' ' || *t == '\t') t++;
            int tl = (int)strlen(t);
            while (tl > 0 && (t[tl-1] == ' ' || t[tl-1] == '\n' || t[tl-1] == '\t')) t[--tl] = '\0';

            /* Extract the first word */
            char first_word[64] = {0};
            int wi = 0;
            const char *p = t;
            while (*p && *p != ' ' && *p != '\t' && *p != '(' && *p != '"' && wi < 62) {
                first_word[wi++] = *p++;
            }
            first_word[wi] = '\0';

            /* All valid NC statement keywords (including synonyms) */
            static const char *valid_starters[] = {
                /* Core NC keywords */
                "set", "show", "ask", "gather", "if", "otherwise",
                "repeat", "while", "match", "respond", "log", "try",
                "run", "store", "emit", "notify", "send", "wait",
                "stop", "skip", "append", "remove", "apply", "check",
                "import", "define", "to", "service", "module",
                "configure", "api", "on_error", "finally", "forward",
                /* Synonyms from other languages */
                "print", "puts", "echo", "display", "output", "write",
                "var", "let", "const",
                "return", "give", "yield",
                "def", "function", "func", "fn", "method", "sub",
                "else", "elif",
                "loop", "foreach", "for",
                "switch", "case", "break", "continue",
                "catch", "except", "rescue",
                "require", "include", "use",
                "class", "struct", "type",
                NULL
            };

            bool is_valid = false;

            /* Function call: identifier followed by ( */
            if (strchr(t, '(') != NULL) is_valid = true;

            /* String literal at start */
            if (t[0] == '"' || t[0] == '\'') is_valid = true;

            /* Number at start (expression) */
            if (t[0] >= '0' && t[0] <= '9') is_valid = true;

            /* List/record literal */
            if (t[0] == '[' || t[0] == '{') is_valid = true;

            /* Check against known keywords */
            if (!is_valid) {
                for (int i = 0; valid_starters[i] != NULL; i++) {
                    if (strcasecmp(first_word, valid_starters[i]) == 0) {
                        is_valid = true;
                        break;
                    }
                }
            }

            /* Check if it's a known variable in scope (allow "show myvar") */
            if (!is_valid && globals->count > 0) {
                NcString *fw = nc_string_from_cstr(first_word);
                if (nc_map_has(globals, fw)) is_valid = true;
                nc_string_free(fw);
            }

            if (!is_valid) {
                /* Try to suggest a close match */
                const char *suggestion = nc_suggest_builtin(first_word);
                if (suggestion) {
                    printf("\033[33mError:\033[0m I don't recognize '\033[1m%s\033[0m'. ", first_word);
                    printf("Did you mean → '\033[32m%s\033[0m'?\n", suggestion);
                } else {
                    printf("\033[33mError:\033[0m '\033[1m%s\033[0m' is not a valid NC command. ", first_word);
                    printf("Type '\033[33mhelp\033[0m' for examples.\n");
                }
                buffer[0] = '\0';
                continue;
            }

            /* Specific incomplete statement checks */
            if ((strcasecmp(t, "ask ai") == 0 || strcasecmp(t, "ask AI") == 0)) {
                printf("\033[33mError:\033[0m 'ask AI' needs a prompt. Example:\n");
                printf("  ask AI to \"summarize this\" using data\n");
                buffer[0] = '\0';
                continue;
            }
            if (strcasecmp(t, "ask") == 0) {
                printf("\033[33mError:\033[0m Incomplete. Example:\n");
                printf("  ask AI to \"classify this\" using data\n");
                buffer[0] = '\0';
                continue;
            }
            if (strcasecmp(t, "gather") == 0) {
                printf("\033[33mError:\033[0m 'gather' needs a URL. Example:\n");
                printf("  gather from \"https://api.example.com/data\"\n");
                buffer[0] = '\0';
                continue;
            }
            if (strcasecmp(t, "api") == 0) {
                printf("\033[33mError:\033[0m 'api' defines routes. Use with a colon:\n");
                printf("  api:\n");
                printf("      POST /analyze runs analyze\n");
                buffer[0] = '\0';
                continue;
            }
        }

        char wrapped[REPL_LINE_MAX * 10 + 256];
        snprintf(wrapped, sizeof(wrapped),
            "to __repl__:\n"
            "    %s\n",
            buffer);

        NcLexer *lex = nc_lexer_new(wrapped, "<repl>");
        nc_lexer_tokenize(lex);
        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, "<repl>");
        NcASTNode *program = nc_parser_parse(parser);

        if (parser->had_error) {
            printf("\033[31mError:\033[0m %s\n", parser->error_msg);
        } else {
            NcValue result = nc_call_behavior(wrapped, "<repl>", "__repl__", globals);
            if (IS_MAP(result)) {
                NcValue err = nc_map_get(AS_MAP(result), nc_string_from_cstr("error"));
                if (IS_STRING(err)) {
                    printf("\033[31mError:\033[0m %s\n", AS_STRING(err)->chars);
                } else {
                    nc_value_print(result, stdout);
                    printf("\n");
                }
            } else if (!IS_NONE(result)) {
                nc_value_print(result, stdout);
                printf("\n");
            }
        }

        nc_parser_free(parser);
        nc_lexer_free(lex);
        buffer[0] = '\0';
    }

    nc_map_free(globals);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  REPL Enhancements — Tab Completion, Syntax Highlighting,
 *  Multi-line Input, History Search, Dot-Commands
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── Completion word lists ─────────────────────────────────────────── */

static const char *nc_completion_builtins[] = {
    "len", "str", "int", "print", "show", "log",
    "keys", "values", "upper", "lower", "trim",
    "split", "join", "contains", "replace",
    "abs", "sqrt", "sort", "reverse", "range",
    "type", "starts_with", "ends_with",
    "read_file", "write_file", "file_exists",
    "json_encode", "json_decode", "env",
    "time_now", "random", "exec", "shell",
    "memory_new", "memory_add", "memory_get",
    "validate", "remove", "append",
    NULL
};

static const char *nc_completion_keywords[] = {
    "if", "otherwise", "repeat", "while", "match", "when",
    "set", "to", "show", "ask", "gather", "store", "emit",
    "notify", "check", "respond", "run", "log", "try",
    "stop", "skip", "import", "define", "service", "module",
    "configure", "api", "on_error", "finally", "forward",
    "wait", "send", "append", "remove", "apply",
    "is", "above", "below", "equal", "not", "and", "or",
    "at", "least", "most", "between", "greater", "less", "than",
    "using", "save", "called", "from", "in", "as", "with",
    "yes", "no", "true", "false", "none", "nothing",
    NULL
};

/* ═══════════════════════════════════════════════════════════
 *  1. Tab Completion
 *
 *  nc_repl_complete() — Given a partial word, returns all
 *  matching completions from builtins, keywords, and the
 *  current variable scope.
 *
 *  The caller must free the returned array and its strings.
 * ═══════════════════════════════════════════════════════════ */

void nc_repl_complete(const char *partial, char ***completions, int *count) {
    if (!partial || !completions || !count) return;

    int partial_len = (int)strlen(partial);
    if (partial_len == 0) { *completions = NULL; *count = 0; return; }

    /* Allocate space for up to 256 completions */
    int capacity = 256;
    char **results = (char **)malloc(capacity * sizeof(char *));
    int n = 0;

    /* Search builtin function names */
    for (int i = 0; nc_completion_builtins[i] != NULL; i++) {
        if (strncmp(nc_completion_builtins[i], partial, partial_len) == 0) {
            if (n < capacity) {
                results[n] = (char *)malloc(strlen(nc_completion_builtins[i]) + 1);
                strcpy(results[n], nc_completion_builtins[i]);
                n++;
            }
        }
    }

    /* Search keywords */
    for (int i = 0; nc_completion_keywords[i] != NULL; i++) {
        if (strncmp(nc_completion_keywords[i], partial, partial_len) == 0) {
            /* Avoid duplicates (some words appear in both lists) */
            bool dup = false;
            for (int j = 0; j < n; j++) {
                if (strcmp(results[j], nc_completion_keywords[i]) == 0) { dup = true; break; }
            }
            if (!dup && n < capacity) {
                results[n] = (char *)malloc(strlen(nc_completion_keywords[i]) + 1);
                strcpy(results[n], nc_completion_keywords[i]);
                n++;
            }
        }
    }

    *completions = results;
    *count = n;
}

/* Overload that also searches variables in a given scope */
void nc_repl_complete_with_scope(const char *partial, NcMap *scope,
                                  char ***completions, int *count) {
    /* Start with builtins + keywords */
    nc_repl_complete(partial, completions, count);

    if (!scope || scope->count == 0 || !completions || !count) return;

    int partial_len = (int)strlen(partial);
    int n = *count;
    char **results = *completions;

    /* Extend the array with scope variables */
    for (int i = 0; i < scope->count; i++) {
        if (!scope->keys[i]) continue;
        const char *varname = scope->keys[i]->chars;
        if (strncmp(varname, partial, partial_len) == 0) {
            bool dup = false;
            for (int j = 0; j < n; j++) {
                if (strcmp(results[j], varname) == 0) { dup = true; break; }
            }
            if (!dup && n < 256) {
                results = (char **)realloc(results, (n + 1) * sizeof(char *));
                results[n] = (char *)malloc(strlen(varname) + 1);
                strcpy(results[n], varname);
                n++;
            }
        }
    }

    *completions = results;
    *count = n;
}

/* Free completions array */
void nc_repl_complete_free(char **completions, int count) {
    if (!completions) return;
    for (int i = 0; i < count; i++) free(completions[i]);
    free(completions);
}

#if !defined(NC_NO_REPL) && !defined(NC_WINDOWS)
/* Readline TAB completion generator — integrates with GNU readline */
static NcMap *nc_rl_scope = NULL;  /* Set before starting REPL loop */

static char *nc_rl_generator(const char *text, int state) {
    static char **matches;
    static int match_count;
    static int match_index;

    if (state == 0) {
        if (matches) nc_repl_complete_free(matches, match_count);
        nc_repl_complete_with_scope(text, nc_rl_scope, &matches, &match_count);
        match_index = 0;
    }

    if (match_index < match_count) {
        char *m = (char *)malloc(strlen(matches[match_index]) + 1);
        strcpy(m, matches[match_index]);
        match_index++;
        return m;
    }

    return NULL;
}

static char **nc_rl_completion(const char *text, int start, int end) {
    (void)start; (void)end;
    rl_attempted_completion_over = 1;  /* Don't fall back to filename completion */
    return rl_completion_matches(text, nc_rl_generator);
}
#endif /* !NC_NO_REPL && !NC_WINDOWS */

/* ═══════════════════════════════════════════════════════════
 *  2. Syntax Highlighting
 *
 *  nc_repl_highlight() — Returns a newly allocated string with
 *  ANSI color codes inserted for NC syntax elements.
 *
 *  Color scheme:
 *    Keywords  → cyan    (\033[36m)
 *    Strings   → green   (\033[32m)
 *    Numbers   → yellow  (\033[33m)
 *    Comments  → gray    (\033[90m)
 *    Errors    → red     (\033[31m)
 *
 *  The caller must free() the returned string.
 * ═══════════════════════════════════════════════════════════ */

static bool nc_hl_is_keyword(const char *word, int len) {
    static const char *kws[] = {
        "if", "otherwise", "repeat", "while", "match", "when",
        "set", "to", "show", "ask", "gather", "store", "emit",
        "notify", "check", "respond", "run", "log", "try",
        "stop", "skip", "import", "define", "service", "module",
        "configure", "api", "on_error", "finally", "forward",
        "wait", "send", "append", "remove", "apply",
        "is", "above", "below", "equal", "not", "and", "or",
        "at", "least", "most", "between", "greater", "less", "than",
        "using", "save", "called", "from", "in", "as", "with",
        "yes", "no", "true", "false", "none", "nothing",
        "len", "str", "int", "print", "upper", "lower", "trim",
        "split", "join", "contains", "replace", "abs", "sqrt",
        "sort", "reverse", "range", "type", "starts_with", "ends_with",
        NULL
    };
    char buf[64];
    if (len <= 0 || len >= 63) return false;
    memcpy(buf, word, len);
    buf[len] = '\0';
    for (int i = 0; kws[i]; i++) {
        if (strcasecmp(buf, kws[i]) == 0) return true;
    }
    return false;
}

char *nc_repl_highlight(const char *input) {
    if (!input) return NULL;

    int input_len = (int)strlen(input);
    /* Worst case: every char gets ~10 bytes of ANSI codes */
    int cap = input_len * 12 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    int pos = 0;

    #define HL_APPEND(s) do { \
        const char *_s = (s); \
        while (*_s && pos < cap - 1) out[pos++] = *_s++; \
    } while(0)
    #define HL_APPENDN(s, n) do { \
        for (int _i = 0; _i < (n) && pos < cap - 1; _i++) out[pos++] = (s)[_i]; \
    } while(0)

    const char *p = input;
    while (*p) {
        /* Comment: # to end of line → gray */
        if (*p == '#') {
            HL_APPEND("\033[90m");
            while (*p && *p != '\n') out[pos++] = *p++;
            HL_APPEND("\033[0m");
            continue;
        }

        /* String literal: "..." or '...' → green */
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            HL_APPEND("\033[32m");
            out[pos++] = *p++;
            while (*p && *p != quote) {
                if (*p == '\\' && *(p + 1)) { out[pos++] = *p++; }
                out[pos++] = *p++;
            }
            if (*p == quote) out[pos++] = *p++;
            HL_APPEND("\033[0m");
            continue;
        }

        /* Number → yellow */
        if ((*p >= '0' && *p <= '9') ||
            (*p == '-' && *(p+1) >= '0' && *(p+1) <= '9' &&
             (p == input || *(p-1) == ' ' || *(p-1) == '(' || *(p-1) == ','))) {
            HL_APPEND("\033[33m");
            if (*p == '-') out[pos++] = *p++;
            while (*p && ((*p >= '0' && *p <= '9') || *p == '.')) out[pos++] = *p++;
            HL_APPEND("\033[0m");
            continue;
        }

        /* Identifier / keyword → check if keyword for cyan */
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
            const char *start = p;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                          (*p >= '0' && *p <= '9') || *p == '_'))
                p++;
            int wlen = (int)(p - start);
            if (nc_hl_is_keyword(start, wlen)) {
                HL_APPEND("\033[36m");
                HL_APPENDN(start, wlen);
                HL_APPEND("\033[0m");
            } else {
                HL_APPENDN(start, wlen);
            }
            continue;
        }

        /* Everything else: pass through */
        out[pos++] = *p++;
    }

    out[pos] = '\0';

    #undef HL_APPEND
    #undef HL_APPENDN

    return out;
}

/* ═══════════════════════════════════════════════════════════
 *  3. Multi-line Input Detection
 *
 *  nc_repl_is_incomplete() — Returns true if the input looks
 *  like an incomplete block that needs more lines:
 *    - Line ends with ':'  (block opener)
 *    - Unterminated string literal
 *    - Unmatched brackets/parens
 * ═══════════════════════════════════════════════════════════ */

bool nc_repl_is_incomplete(const char *input) {
    if (!input || !*input) return false;

    int len = (int)strlen(input);

    /* Trim trailing whitespace to find the real last char */
    int end = len - 1;
    while (end >= 0 && (input[end] == ' ' || input[end] == '\t' || input[end] == '\n'))
        end--;
    if (end < 0) return false;

    /* Line ends with colon → block opener, needs body */
    if (input[end] == ':') return true;

    /* Check for unterminated string literals */
    bool in_string = false;
    char string_char = 0;
    for (int i = 0; i <= end; i++) {
        if (!in_string && (input[i] == '"' || input[i] == '\'')) {
            in_string = true;
            string_char = input[i];
        } else if (in_string && input[i] == string_char && (i == 0 || input[i-1] != '\\')) {
            in_string = false;
        }
    }
    if (in_string) return true;

    /* Check for unmatched brackets/parens */
    int parens = 0, brackets = 0, braces = 0;
    in_string = false;
    string_char = 0;
    for (int i = 0; i <= end; i++) {
        if (!in_string && (input[i] == '"' || input[i] == '\'')) {
            in_string = true; string_char = input[i];
        } else if (in_string && input[i] == string_char && (i == 0 || input[i-1] != '\\')) {
            in_string = false;
        } else if (!in_string) {
            switch (input[i]) {
                case '(': parens++; break;
                case ')': parens--; break;
                case '[': brackets++; break;
                case ']': brackets--; break;
                case '{': braces++; break;
                case '}': braces--; break;
            }
        }
    }
    if (parens > 0 || brackets > 0 || braces > 0) return true;

    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  4. History Search (Ctrl+R)
 *
 *  nc_repl_history_search() — Searches backward through history
 *  for lines containing the search term.  Returns a newly
 *  allocated string with the match, or NULL if not found.
 *
 *  On platforms with GNU readline, Ctrl+R is already built in.
 *  This function provides the same for the basic fallback.
 * ═══════════════════════════════════════════════════════════ */

#define NC_HISTORY_MAX 1000

static char *nc_repl_history_buf[NC_HISTORY_MAX];
static int   nc_repl_history_count = 0;

void nc_repl_history_add(const char *line) {
    if (!line || !*line) return;
    if (nc_repl_history_count >= NC_HISTORY_MAX) {
        /* Evict oldest entry */
        free(nc_repl_history_buf[0]);
        memmove(nc_repl_history_buf, nc_repl_history_buf + 1,
                (NC_HISTORY_MAX - 1) * sizeof(char *));
        nc_repl_history_count--;
    }
    nc_repl_history_buf[nc_repl_history_count] = (char *)malloc(strlen(line) + 1);
    if (nc_repl_history_buf[nc_repl_history_count]) {
        strcpy(nc_repl_history_buf[nc_repl_history_count], line);
        nc_repl_history_count++;
    }
}

char *nc_repl_history_search(const char *term) {
    if (!term || !*term) return NULL;
    /* Search backward (most recent first) */
    for (int i = nc_repl_history_count - 1; i >= 0; i--) {
        if (nc_repl_history_buf[i] && strstr(nc_repl_history_buf[i], term)) {
            char *result = (char *)malloc(strlen(nc_repl_history_buf[i]) + 1);
            if (result) strcpy(result, nc_repl_history_buf[i]);
            return result;
        }
    }
    return NULL;
}

void nc_repl_history_free(void) {
    for (int i = 0; i < nc_repl_history_count; i++)
        free(nc_repl_history_buf[i]);
    nc_repl_history_count = 0;
}

/* ═══════════════════════════════════════════════════════════
 *  5. Dot-Commands
 *
 *  REPL meta-commands prefixed with '.' for inspecting
 *  state and controlling the environment.
 *
 *  .help           — show available dot-commands
 *  .clear          — clear screen
 *  .vars           — show current variables with types
 *  .time <expr>    — time an expression's execution
 *  .type <expr>    — show the type of an expression result
 * ═══════════════════════════════════════════════════════════ */

static const char *nc_value_type_name(NcValue v) {
    if (IS_NONE(v))   return "nothing";
    if (IS_BOOL(v))   return "yesno";
    if (IS_INT(v))    return "number (int)";
    if (IS_FLOAT(v))  return "number (float)";
    if (IS_STRING(v)) return "text";
    if (IS_LIST(v))   return "list";
    if (IS_MAP(v))    return "record";
    return "unknown";
}

/* Returns true if the line was handled as a dot-command */
bool nc_repl_dot_command(const char *input, NcMap *globals) {
    if (!input || input[0] != '.') return false;

    /* .help */
    if (strcmp(input, ".help") == 0) {
        printf("\033[36mREPL Dot-Commands:\033[0m\n\n");
        printf("  \033[33m.help\033[0m            Show this list of commands\n");
        printf("  \033[33m.clear\033[0m           Clear the terminal screen\n");
        printf("  \033[33m.vars\033[0m            Show all variables with their types\n");
        printf("  \033[33m.time\033[0m \033[90m<expr>\033[0m    Time the execution of an expression\n");
        printf("  \033[33m.type\033[0m \033[90m<expr>\033[0m    Show the type of an expression's result\n");
        return true;
    }

    /* .clear */
    if (strcmp(input, ".clear") == 0) {
        printf("\033[2J\033[H");  /* ANSI: clear screen + move cursor home */
        fflush(stdout);
        return true;
    }

    /* .vars — enhanced version showing types */
    if (strcmp(input, ".vars") == 0) {
        if (!globals || globals->count == 0) {
            printf("\033[90m(no variables defined)\033[0m\n");
        } else {
            printf("\033[36m%-20s  %-16s  %s\033[0m\n", "Name", "Type", "Value");
            printf("\033[90m%-20s  %-16s  %s\033[0m\n", "----", "----", "-----");
            for (int i = 0; i < globals->count; i++) {
                const char *name = globals->keys[i]->chars;
                const char *tname = nc_value_type_name(globals->values[i]);
                printf("  \033[33m%-18s\033[0m  \033[90m%-16s\033[0m  ", name, tname);
                nc_value_print(globals->values[i], stdout);
                printf("\n");
            }
            printf("\033[90m(%d variable%s)\033[0m\n", globals->count,
                   globals->count == 1 ? "" : "s");
        }
        return true;
    }

    /* .time <expr> — benchmark an expression */
    if (strncmp(input, ".time ", 6) == 0) {
        const char *expr = input + 6;
        while (*expr == ' ') expr++;
        if (!*expr) {
            printf("\033[31mUsage:\033[0m .time <expression>\n");
            return true;
        }

        char wrapped[REPL_LINE_MAX + 256];
        snprintf(wrapped, sizeof(wrapped),
            "to __repl__:\n    %s\n", expr);

        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);

        NcValue result = nc_call_behavior(wrapped, "<repl>", "__repl__", globals);

        clock_gettime(CLOCK_MONOTONIC, &t_end);

        double elapsed_ms = (t_end.tv_sec - t_start.tv_sec) * 1000.0 +
                            (t_end.tv_nsec - t_start.tv_nsec) / 1e6;

        if (!IS_NONE(result)) {
            nc_value_print(result, stdout);
            printf("\n");
        }
        printf("\033[90mTime: %.3f ms\033[0m\n", elapsed_ms);
        return true;
    }

    /* .type <expr> — show the type of a result */
    if (strncmp(input, ".type ", 6) == 0) {
        const char *expr = input + 6;
        while (*expr == ' ') expr++;
        if (!*expr) {
            printf("\033[31mUsage:\033[0m .type <expression>\n");
            return true;
        }

        char wrapped[REPL_LINE_MAX + 256];
        snprintf(wrapped, sizeof(wrapped),
            "to __repl__:\n    %s\n", expr);

        NcValue result = nc_call_behavior(wrapped, "<repl>", "__repl__", globals);
        const char *tname = nc_value_type_name(result);
        printf("\033[36m%s\033[0m\n", tname);
        return true;
    }

    /* Unknown dot-command */
    printf("\033[33mUnknown command:\033[0m '%s'. Type \033[33m.help\033[0m for available commands.\n", input);
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Enhanced REPL entry point
 *
 *  nc_repl_run_enhanced() — Full-featured REPL with:
 *    - Tab completion via readline integration
 *    - Multi-line input detection
 *    - Dot-commands
 *    - History search
 *    - Syntax highlighting on echo
 * ═══════════════════════════════════════════════════════════ */

int nc_repl_run_enhanced(void) {
    const char *os_name = "unknown";
#ifdef NC_WINDOWS
    os_name = "Windows";
#else
    struct utsname uname_data;
    if (uname(&uname_data) == 0) os_name = uname_data.sysname;
#endif

    printf("\n");
    printf("  \033[36m _  _  ___\033[0m\n");
    printf("  \033[36m| \\| |/ __|\033[0m   \033[1m\033[36mNC REPL\033[0m \033[33mv%s\033[0m \033[90m[C11] on %s\033[0m\n", NC_VERSION, os_name);
    printf("  \033[36m| .` | (__\033[0m    \033[90mInteractive Notation-as-Code\033[0m\n");
    printf("  \033[36m|_|\\_|\\___|\033[0m   \033[90mTab completion · Syntax highlighting · .help\033[0m\n");
    printf("\n");
    printf("  \033[90mType \033[0m\033[33mhelp\033[0m\033[90m for commands, \033[0m\033[33m.help\033[0m\033[90m for REPL controls, \033[0m\033[33mquit\033[0m\033[90m to exit.\033[0m\n\n");

    NcMap *globals = nc_map_new();

#if !defined(NC_NO_REPL) && !defined(NC_WINDOWS)
    /* Register tab completion with readline */
    nc_rl_scope = globals;
    rl_attempted_completion_function = nc_rl_completion;
#endif

    char buffer[REPL_LINE_MAX * 10];
    buffer[0] = '\0';
    bool in_block = false;

    while (true) {
        char *input = readline(in_block ? "... " : ">>> ");

        if (!input) {
            printf("\n");
            break;
        }

        /* Add non-empty lines to history */
        if (input[0] != '\0') {
            add_history(input);
            nc_repl_history_add(input);
        }

        /* Skip empty lines outside blocks */
        if (!in_block && input[0] == '\0') {
            free(input);
            continue;
        }

        /* Dot-commands (only at top level, not inside blocks) */
        if (!in_block && input[0] == '.') {
            nc_repl_dot_command(input, globals);
            free(input);
            continue;
        }

        /* Standard built-in commands (quit, help, vars, clear) */
        if (!in_block) {
            if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0 ||
                strcmp(input, "quit()") == 0 || strcmp(input, "exit()") == 0) {
                free(input);
                break;
            }
            if (strcmp(input, "help") == 0 || strcmp(input, "help()") == 0) {
                printf("NC — Plain English programming language for AI\n\n");
                printf("Statements:\n");
                printf("  set x to 42                   Assign a variable\n");
                printf("  show x + 8                    Print a value\n");
                printf("  show \"Hello, \" + name         String concatenation\n");
                printf("  set items to [1, 2, 3]        Create a list\n");
                printf("  show len(items)               Call a function\n\n");
                printf("Control flow:\n");
                printf("  if x is above 10:             Conditional (end block with empty line)\n");
                printf("      show \"big\"\n");
                printf("  repeat 5 times:               Loop\n");
                printf("      show \"hello\"\n\n");
                printf("Commands:\n");
                printf("  help          Show this help\n");
                printf("  vars          Show all variables\n");
                printf("  clear         Clear all variables\n");
                printf("  quit          Exit\n");
                printf("  .help         REPL dot-commands\n");
                free(input);
                continue;
            }
            if (strcmp(input, "vars") == 0 || strcmp(input, "vars()") == 0) {
                nc_repl_dot_command(".vars", globals);
                free(input);
                continue;
            }
            if (strcmp(input, "clear") == 0 || strcmp(input, "clear()") == 0) {
                nc_map_free(globals);
                globals = nc_map_new();
#if !defined(NC_NO_REPL) && !defined(NC_WINDOWS)
                nc_rl_scope = globals;
#endif
                printf("Variables cleared.\n");
                free(input);
                continue;
            }
        }

        /* Multi-line detection: check if the accumulated input is incomplete */
        if (!in_block && nc_repl_is_incomplete(input)) {
            strncat(buffer, input, sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            in_block = true;
            free(input);
            continue;
        }

        /* Empty line ends a block */
        if (in_block && input[0] == '\0') {
            in_block = false;
            free(input);
            goto enhanced_execute;
        }

        /* Accumulate indented lines in block */
        if (in_block) {
            strncat(buffer, input, sizeof(buffer) - strlen(buffer) - 1);
            strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);
            /* Check if the new line itself opens a sub-block */
            free(input);
            continue;
        }

        /* Single-line execution */
        strncpy(buffer, input, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        free(input);

enhanced_execute:
        if (buffer[0] == '\0') continue;

        /* Echo with syntax highlighting */
        {
            char *highlighted = nc_repl_highlight(buffer);
            if (highlighted) {
                /* Only echo if it was a multi-line block (so user can see assembled input) */
                if (strchr(buffer, '\n')) {
                    printf("\033[90m[block]:\033[0m\n");
                    /* Print each line with highlighting */
                    char *line_start = highlighted;
                    while (*line_start) {
                        char *nl = strchr(line_start, '\n');
                        if (nl) {
                            *nl = '\0';
                            printf("  %s\n", line_start);
                            line_start = nl + 1;
                        } else {
                            if (*line_start) printf("  %s\n", line_start);
                            break;
                        }
                    }
                }
                free(highlighted);
            }
        }

        /* Execute */
        char wrapped[REPL_LINE_MAX * 10 + 256];
        snprintf(wrapped, sizeof(wrapped),
            "to __repl__:\n"
            "    %s\n",
            buffer);

        NcLexer *lex = nc_lexer_new(wrapped, "<repl>");
        nc_lexer_tokenize(lex);
        NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, "<repl>");
        NcASTNode *program = nc_parser_parse(parser);
        (void)program;

        if (parser->had_error) {
            printf("\033[31mError:\033[0m %s\n", parser->error_msg);
        } else {
            NcValue result = nc_call_behavior(wrapped, "<repl>", "__repl__", globals);
            if (IS_MAP(result)) {
                NcValue err = nc_map_get(AS_MAP(result), nc_string_from_cstr("error"));
                if (IS_STRING(err)) {
                    printf("\033[31mError:\033[0m %s\n", AS_STRING(err)->chars);
                } else {
                    nc_value_print(result, stdout);
                    printf("\n");
                }
            } else if (!IS_NONE(result)) {
                nc_value_print(result, stdout);
                printf("\n");
            }
        }

        nc_parser_free(parser);
        nc_lexer_free(lex);
        buffer[0] = '\0';
    }

    nc_repl_history_free();
    nc_map_free(globals);
    return 0;
}
