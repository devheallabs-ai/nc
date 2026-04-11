/*
 * nc_debug.c — Debugger for NC programs.
 *
 * Provides step-through debugging, breakpoints, and variable inspection.
 *
 * Usage:
 *   nc debug service.nc           Start debugger
 *   nc debug service.nc -b greet  Debug specific behavior
 *
 * Debugger commands:
 *   s / step      Execute next statement
 *   n / next      Step over (don't enter called behaviors)
 *   c / continue  Run until next breakpoint
 *   b <line>      Set breakpoint at line
 *   p <var>       Print variable value
 *   vars          Show all variables
 *   bt            Show call stack
 *   q / quit      Exit debugger
 */

#include "../include/nc.h"

#define MAX_BREAKPOINTS 64

typedef struct {
    int   breakpoints[MAX_BREAKPOINTS];
    int   bp_count;
    bool  stepping;
    bool  running;
    int   current_line;
    const char *current_file;
    NcMap *watch_vars;
} NcDebugger;

static NcDebugger *dbg_instance = NULL;

/* ── Debugger lifecycle ────────────────────────────────────── */

NcDebugger *nc_debug_new(void) {
    NcDebugger *dbg = calloc(1, sizeof(NcDebugger));
    dbg->stepping = true;
    dbg->running = true;
    dbg->watch_vars = nc_map_new();
    dbg_instance = dbg;
    return dbg;
}

void nc_debug_free(NcDebugger *dbg) {
    if (dbg) {
        nc_map_free(dbg->watch_vars);
        free(dbg);
    }
    if (dbg_instance == dbg) dbg_instance = NULL;
}

/* ── Breakpoints ───────────────────────────────────────────── */

void nc_debug_set_breakpoint(NcDebugger *dbg, int line) {
    if (dbg->bp_count >= MAX_BREAKPOINTS) {
        printf("  Max breakpoints reached\n");
        return;
    }
    dbg->breakpoints[dbg->bp_count++] = line;
    printf("  Breakpoint set at line %d\n", line);
}

void nc_debug_remove_breakpoint(NcDebugger *dbg, int line) {
    for (int i = 0; i < dbg->bp_count; i++) {
        if (dbg->breakpoints[i] == line) {
            memmove(&dbg->breakpoints[i], &dbg->breakpoints[i + 1],
                    (dbg->bp_count - i - 1) * sizeof(int));
            dbg->bp_count--;
            printf("  Breakpoint removed at line %d\n", line);
            return;
        }
    }
    printf("  No breakpoint at line %d\n", line);
}

static bool is_breakpoint(NcDebugger *dbg, int line) {
    for (int i = 0; i < dbg->bp_count; i++)
        if (dbg->breakpoints[i] == line) return true;
    return false;
}

/* ── Source display ─────────────────────────────────────────── */

static void show_source_line(const char *source, int target_line) {
    const char *p = source;
    int line = 1;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        if (line >= target_line - 2 && line <= target_line + 2) {
            char marker = (line == target_line) ? '>' : ' ';
            printf("  %c %3d | %.*s\n", marker, line, (int)(eol - p), p);
        }
        line++;
        p = (*eol == '\n') ? eol + 1 : eol;
        if (line > target_line + 3) break;
    }
}

/* ── Print variables ───────────────────────────────────────── */

static void print_var(const char *name, NcValue val) {
    printf("  %s = ", name);
    nc_value_print(val, stdout);
    printf("\n");
}

static void print_all_vars(NcMap *vars) {
    printf("\n  Variables:\n");
    printf("  %s\n", "────────────────────────────────");
    for (int i = 0; i < vars->count; i++) {
        printf("  ");
        print_var(vars->keys[i]->chars, vars->values[i]);
    }
    printf("\n");
}

/* ── Debug prompt (interactive) ────────────────────────────── */

static bool debug_prompt(NcDebugger *dbg, const char *source, NcMap *vars) {
    while (true) {
        printf("\n(ncdb) ");
        fflush(stdout);

        char cmd[256];
        if (!fgets(cmd, sizeof(cmd), stdin)) return false;
        cmd[strcspn(cmd, "\n")] = '\0';

        if (cmd[0] == '\0' || strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            dbg->stepping = true;
            return true;
        }
        if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            dbg->stepping = true;
            return true;
        }
        if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            dbg->stepping = false;
            return true;
        }
        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            dbg->running = false;
            return false;
        }
        if (cmd[0] == 'b' && cmd[1] == ' ') {
            int line = atoi(cmd + 2);
            if (line > 0) nc_debug_set_breakpoint(dbg, line);
            continue;
        }
        if (cmd[0] == 'p' && cmd[1] == ' ') {
            NcString *name = nc_string_from_cstr(cmd + 2);
            if (nc_map_has(vars, name)) {
                print_var(cmd + 2, nc_map_get(vars, name));
            } else {
                printf("  Variable '%s' not found\n", cmd + 2);
            }
            nc_string_free(name);
            continue;
        }
        if (strcmp(cmd, "vars") == 0) {
            print_all_vars(vars);
            continue;
        }
        if (strcmp(cmd, "bt") == 0 || strcmp(cmd, "backtrace") == 0) {
            printf("  #0  line %d in %s\n", dbg->current_line, dbg->current_file);
            continue;
        }
        if (strcmp(cmd, "src") == 0 || strcmp(cmd, "list") == 0) {
            show_source_line(source, dbg->current_line);
            continue;
        }
        if (strcmp(cmd, "help") == 0) {
            printf("  Commands:\n");
            printf("    s/step       Step to next statement\n");
            printf("    n/next       Step over\n");
            printf("    c/continue   Run to next breakpoint\n");
            printf("    b <line>     Set breakpoint\n");
            printf("    p <var>      Print variable\n");
            printf("    vars         Show all variables\n");
            printf("    bt           Show call stack\n");
            printf("    src/list     Show source code\n");
            printf("    q/quit       Exit debugger\n");
            continue;
        }
        printf("  Unknown command: '%s' (type 'help' for commands)\n", cmd);
    }
}

/* ── Debug hook — called before each statement ─────────────── */

void nc_debug_hook(int line, const char *source, NcMap *scope_vars) {
    if (!dbg_instance || !dbg_instance->running) return;
    NcDebugger *dbg = dbg_instance;
    dbg->current_line = line;

    bool should_stop = dbg->stepping || is_breakpoint(dbg, line);
    if (!should_stop) return;

    printf("\n  Stopped at line %d", line);
    if (is_breakpoint(dbg, line)) printf(" [breakpoint]");
    printf("\n");
    show_source_line(source, line);

    debug_prompt(dbg, source, scope_vars);
}

/* ═══════════════════════════════════════════════════════════
 *  DAP (Debug Adapter Protocol) — IDE debug integration
 *
 *  Implements a subset of the Debug Adapter Protocol over stdio.
 *  This allows VS Code, Cursor, and other editors to:
 *    - Set breakpoints
 *    - Step through code
 *    - Inspect variables
 *    - View call stack
 *
 *  Protocol: JSON messages with Content-Length headers on stdio.
 * ═══════════════════════════════════════════════════════════ */

static void dap_send(const char *json) {
    int len = (int)strlen(json);
    printf("Content-Length: %d\r\n\r\n%s", len, json);
    fflush(stdout);
}

static void dap_send_event(const char *event, const char *body) {
    static int seq = 1;
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{\"seq\":%d,\"type\":\"event\",\"event\":\"%s\",\"body\":{%s}}",
        seq++, event, body ? body : "");
    dap_send(buf);
}

static void dap_send_response(int request_seq, const char *command,
                              bool success, const char *body) {
    static int seq = 1000;
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{\"seq\":%d,\"type\":\"response\",\"request_seq\":%d,"
        "\"success\":%s,\"command\":\"%s\",\"body\":{%s}}",
        seq++, request_seq, success ? "true" : "false",
        command, body ? body : "");
    dap_send(buf);
}

static char *dap_read_message(void) {
    char header[256];
    int content_length = 0;

    while (fgets(header, sizeof(header), stdin)) {
        if (header[0] == '\r' || header[0] == '\n') break;
        if (strncmp(header, "Content-Length:", 14) == 0)
            content_length = atoi(header + 15);
    }

    if (content_length <= 0 || content_length > 65536) return NULL;
    char *body = malloc(content_length + 1);
    if ((int)fread(body, 1, content_length, stdin) != content_length) {
        free(body);
        return NULL;
    }
    body[content_length] = '\0';
    return body;
}

int nc_dap_run(const char *filename) {
    NcDebugger *dbg = nc_debug_new();
    dbg->current_file = filename;
    dbg->stepping = true;

    char *source = NULL;
    {
        FILE *f = fopen(filename, "rb");
        if (!f) return 1;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        source = malloc(sz + 1);
        fread(source, 1, sz, f);
        source[sz] = '\0';
        fclose(f);
    }

    bool initialized = false;
    bool running = true;

    while (running) {
        char *msg = dap_read_message();
        if (!msg) break;

        int req_seq = 0;
        char command[64] = "";

        char *seq_p = strstr(msg, "\"seq\":");
        if (seq_p) req_seq = atoi(seq_p + 6);
        char *cmd_p = strstr(msg, "\"command\":\"");
        if (cmd_p) {
            cmd_p += 11;
            int ci = 0;
            while (*cmd_p && *cmd_p != '"' && ci < 63) command[ci++] = *cmd_p++;
            command[ci] = '\0';
        }

        if (strcmp(command, "initialize") == 0) {
            dap_send_response(req_seq, "initialize", true,
                "\"supportsConfigurationDoneRequest\":true,"
                "\"supportsFunctionBreakpoints\":false,"
                "\"supportsConditionalBreakpoints\":false,"
                "\"supportsStepBack\":false,"
                "\"supportsSetVariable\":false");
            dap_send_event("initialized", NULL);
            initialized = true;
        }
        else if (strcmp(command, "configurationDone") == 0) {
            dap_send_response(req_seq, "configurationDone", true, NULL);
        }
        else if (strcmp(command, "launch") == 0) {
            dap_send_response(req_seq, "launch", true, NULL);
            dap_send_event("stopped", "\"reason\":\"entry\",\"threadId\":1");
        }
        else if (strcmp(command, "setBreakpoints") == 0) {
            char *lines_p = strstr(msg, "\"lines\":[");
            dbg->bp_count = 0;
            if (lines_p) {
                lines_p += 9;
                while (*lines_p && *lines_p != ']') {
                    if (*lines_p >= '0' && *lines_p <= '9') {
                        int line = atoi(lines_p);
                        if (dbg->bp_count < MAX_BREAKPOINTS)
                            dbg->breakpoints[dbg->bp_count++] = line;
                        while (*lines_p >= '0' && *lines_p <= '9') lines_p++;
                    } else lines_p++;
                }
            }
            char body[1024] = "\"breakpoints\":[";
            int pos = (int)strlen(body);
            for (int i = 0; i < dbg->bp_count; i++) {
                pos += snprintf(body + pos, sizeof(body) - pos,
                    "%s{\"verified\":true,\"line\":%d}",
                    i > 0 ? "," : "", dbg->breakpoints[i]);
            }
            snprintf(body + pos, sizeof(body) - pos, "]");
            dap_send_response(req_seq, "setBreakpoints", true, body);
        }
        else if (strcmp(command, "threads") == 0) {
            dap_send_response(req_seq, "threads", true,
                "\"threads\":[{\"id\":1,\"name\":\"NC Main\"}]");
        }
        else if (strcmp(command, "stackTrace") == 0) {
            char body[1024];
            snprintf(body, sizeof(body),
                "\"stackFrames\":[{\"id\":0,\"name\":\"main\","
                "\"source\":{\"path\":\"%s\"},\"line\":%d,\"column\":1}],"
                "\"totalFrames\":1",
                filename, dbg->current_line > 0 ? dbg->current_line : 1);
            dap_send_response(req_seq, "stackTrace", true, body);
        }
        else if (strcmp(command, "scopes") == 0) {
            dap_send_response(req_seq, "scopes", true,
                "\"scopes\":[{\"name\":\"Locals\",\"variablesReference\":1,"
                "\"expensive\":false}]");
        }
        else if (strcmp(command, "variables") == 0) {
            dap_send_response(req_seq, "variables", true,
                "\"variables\":[]");
        }
        else if (strcmp(command, "continue") == 0) {
            dbg->stepping = false;
            dap_send_response(req_seq, "continue", true,
                "\"allThreadsContinued\":true");
        }
        else if (strcmp(command, "next") == 0 || strcmp(command, "stepIn") == 0) {
            dbg->stepping = true;
            dap_send_response(req_seq, command, true, NULL);
            dap_send_event("stopped", "\"reason\":\"step\",\"threadId\":1");
        }
        else if (strcmp(command, "disconnect") == 0) {
            dap_send_response(req_seq, "disconnect", true, NULL);
            running = false;
        }
        else {
            dap_send_response(req_seq, command, true, NULL);
        }

        free(msg);
        (void)initialized;
    }

    nc_debug_free(dbg);
    free(source);
    return 0;
}

/* ── Top-level debug command ───────────────────────────────── */

int nc_debug_file(const char *filename, const char *behavior) {
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    printf("\n  NC Debugger — %s\n", filename);
    printf("  Type 'help' for commands, 's' to step, 'c' to continue\n");
    printf("  ────────────────────────────────────────\n\n");

    NcDebugger *dbg = nc_debug_new();
    dbg->current_file = filename;

    /* Parse */
    NcLexer *lex = nc_lexer_new(source, filename);
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, filename);
    NcASTNode *program = nc_parser_parse(parser);

    if (parser->had_error) {
        fprintf(stderr, "  Parse error: %s\n", parser->error_msg);
        nc_parser_free(parser);
        nc_lexer_free(lex);
        nc_debug_free(dbg);
        free(source);
        return 1;
    }

    printf("  Loaded %d behaviors\n", program->as.program.beh_count);
    if (behavior) {
        printf("  Debugging behavior: %s\n", behavior);
    }
    printf("  Press Enter to begin stepping...\n");

    /* Interactive — wait for user */
    NcMap *empty_vars = nc_map_new();
    debug_prompt(dbg, source, empty_vars);
    nc_map_free(empty_vars);

    nc_debug_free(dbg);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    free(source);
    return 0;
}
