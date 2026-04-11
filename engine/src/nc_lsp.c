/*
 * nc_lsp.c — Language Server Protocol implementation for NC.
 *
 * Provides IDE integration: autocomplete, diagnostics, hover info,
 * go-to-definition.  Speaks JSON-RPC over stdio (standard LSP transport).
 *
 * Usage:
 *   nc lsp          Start the LSP server (IDEs connect via stdio)
 *
 * Supports:
 *   - textDocument/didOpen, didChange, didClose
 *   - textDocument/completion (autocomplete)
 *   - textDocument/diagnostic (error checking)
 *   - textDocument/hover (type info on hover)
 *   - textDocument/definition (go to definition)
 */

#include "../include/nc.h"

/* ═══════════════════════════════════════════════════════════
 *  JSON-RPC message reading/writing (stdio transport)
 * ═══════════════════════════════════════════════════════════ */

static char *lsp_read_message(void) {
    char header[256];
    int content_length = 0;

    while (fgets(header, sizeof(header), stdin)) {
        if (strncmp(header, "Content-Length:", 15) == 0) {
            content_length = atoi(header + 15);
        }
        if (header[0] == '\r' || header[0] == '\n') break;
    }
    if (content_length <= 0 || content_length > 1048576) return NULL;

    char *body = malloc(content_length + 1);
    int nread = (int)fread(body, 1, content_length, stdin);
    body[nread] = '\0';
    return body;
}

static void lsp_send_message(const char *json) {
    int len = (int)strlen(json);
    fprintf(stdout, "Content-Length: %d\r\n\r\n%s", len, json);
    fflush(stdout);
}

static void lsp_send_response(int id, const char *result_json) {
    char buf[8192];
    snprintf(buf, sizeof(buf),
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}", id, result_json);
    lsp_send_message(buf);
}

/* ═══════════════════════════════════════════════════════════
 *  Diagnostics — validate .nc files and report errors
 * ═══════════════════════════════════════════════════════════ */

static void lsp_publish_diagnostics(const char *uri, const char *source) {
    NcLexer *lex = nc_lexer_new(source, uri);
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, uri);
    NcASTNode *program = nc_parser_parse(parser);

    char diagnostics[4096] = "[";

    if (parser->had_error) {
        char diag[1024];
        snprintf(diag, sizeof(diag),
            "{\"range\":{\"start\":{\"line\":0,\"character\":0},"
            "\"end\":{\"line\":0,\"character\":100}},"
            "\"severity\":1,\"source\":\"nc\","
            "\"message\":\"%s\"}", parser->error_msg);
        strncat(diagnostics, diag, sizeof(diagnostics) - strlen(diagnostics) - 1);
    } else {
        /* Warnings */
        if (!program->as.program.service_name) {
            strncat(diagnostics,
                "{\"range\":{\"start\":{\"line\":0,\"character\":0},"
                "\"end\":{\"line\":0,\"character\":1}},"
                "\"severity\":2,\"source\":\"nc\","
                "\"message\":\"No service name declared. Add: service \\\"my-service\\\"\"}",
                sizeof(diagnostics) - strlen(diagnostics) - 1);
        }
    }

    strncat(diagnostics, "]", sizeof(diagnostics) - strlen(diagnostics) - 1);

    char msg[8192];
    snprintf(msg, sizeof(msg),
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\","
        "\"params\":{\"uri\":\"%s\",\"diagnostics\":%s}}", uri, diagnostics);
    lsp_send_message(msg);

    nc_parser_free(parser);
    nc_lexer_free(lex);
    (void)program;
}

/* ═══════════════════════════════════════════════════════════
 *  Completions — autocomplete suggestions
 * ═══════════════════════════════════════════════════════════ */

static const char *COMPLETIONS_JSON =
    "{\"isIncomplete\":false,\"items\":["
    "{\"label\":\"service\",\"kind\":14,\"detail\":\"Declare service name\",\"insertText\":\"service \\\"${1:name}\\\"\"},"
    "{\"label\":\"version\",\"kind\":14,\"detail\":\"Declare version\",\"insertText\":\"version \\\"${1:1.0.0}\\\"\"},"
    "{\"label\":\"model\",\"kind\":14,\"detail\":\"Set AI model\",\"insertText\":\"model \\\"${1:your-model}\\\"\"},"
    "{\"label\":\"to\",\"kind\":3,\"detail\":\"Define a behavior\",\"insertText\":\"to ${1:name}:\\n    ${2:purpose: \\\"${3:description}\\\"}\\n    ${0}\"},"
    "{\"label\":\"define\",\"kind\":22,\"detail\":\"Define a type\",\"insertText\":\"define ${1:Name} as:\\n    ${2:field} is ${3:text}\\n    ${0}\"},"
    "{\"label\":\"gather\",\"kind\":3,\"detail\":\"Gather data from source\",\"insertText\":\"gather ${1:data} from ${2:source}\"},"
    "{\"label\":\"ask AI to\",\"kind\":3,\"detail\":\"Ask AI for analysis\",\"insertText\":\"ask AI to \\\"${1:prompt}\\\" using ${2:data}:\\n    save as: ${3:result}\"},"
    "{\"label\":\"if\",\"kind\":14,\"detail\":\"Conditional\",\"insertText\":\"if ${1:condition}:\\n    ${0}\"},"
    "{\"label\":\"otherwise\",\"kind\":14,\"detail\":\"Else clause\",\"insertText\":\"otherwise:\\n    ${0}\"},"
    "{\"label\":\"repeat for each\",\"kind\":14,\"detail\":\"Loop\",\"insertText\":\"repeat for each ${1:item} in ${2:items}:\\n    ${0}\"},"
    "{\"label\":\"respond with\",\"kind\":3,\"detail\":\"Return value\",\"insertText\":\"respond with ${1:result}\"},"
    "{\"label\":\"set\",\"kind\":3,\"detail\":\"Set variable\",\"insertText\":\"set ${1:name} to ${2:value}\"},"
    "{\"label\":\"log\",\"kind\":3,\"detail\":\"Log message\",\"insertText\":\"log \\\"${1:message}\\\"\"},"
    "{\"label\":\"wait\",\"kind\":3,\"detail\":\"Wait duration\",\"insertText\":\"wait ${1:30} seconds\"},"
    "{\"label\":\"notify\",\"kind\":3,\"detail\":\"Send notification\",\"insertText\":\"notify ${1:channel} \\\"${2:message}\\\"\"},"
    "{\"label\":\"run\",\"kind\":3,\"detail\":\"Run another behavior\",\"insertText\":\"run ${1:behavior} with ${2:args}\"},"
    "{\"label\":\"configure\",\"kind\":14,\"detail\":\"Configuration block\",\"insertText\":\"configure:\\n    ${1:key}: ${2:value}\\n    ${0}\"},"
    "{\"label\":\"api\",\"kind\":14,\"detail\":\"API routes\",\"insertText\":\"api:\\n    ${1:POST} ${2:/path} runs ${3:handler}\\n    ${0}\"},"
    "{\"label\":\"match\",\"kind\":14,\"detail\":\"Pattern match\",\"insertText\":\"match ${1:value}:\\n    when \\\"${2:pattern}\\\":\\n        ${0}\"},"
    "{\"label\":\"try\",\"kind\":14,\"detail\":\"Error handling\",\"insertText\":\"try:\\n    ${1}\\non error:\\n    ${0}\"},"
    "{\"label\":\"store\",\"kind\":3,\"detail\":\"Store data\",\"insertText\":\"store ${1:data} into \\\"${2:target}\\\"\"},"
    "{\"label\":\"emit\",\"kind\":3,\"detail\":\"Emit event\",\"insertText\":\"emit \\\"${1:event_name}\\\"\"}"
    "]}";

/* ═══════════════════════════════════════════════════════════
 *  Main LSP loop
 * ═══════════════════════════════════════════════════════════ */

int nc_lsp_run(void) {
    /* Redirect stderr so debug output doesn't corrupt LSP */
#ifdef NC_WINDOWS
    freopen("NUL", "w", stderr);
#else
    freopen("/dev/null", "w", stderr);
#endif

    bool initialized = false;
    char *open_source = NULL;
    char open_uri[512] = "";

    while (true) {
        char *msg = lsp_read_message();
        if (!msg) break;

        /* Minimal JSON parsing for method extraction */
        char *method_ptr = strstr(msg, "\"method\"");
        char *id_ptr = strstr(msg, "\"id\"");
        int id = 0;
        if (id_ptr) {
            char *colon = strchr(id_ptr, ':');
            if (colon) id = atoi(colon + 1);
        }

        if (!method_ptr) { free(msg); continue; }

        if (strstr(method_ptr, "\"initialize\"")) {
            lsp_send_response(id,
                "{\"capabilities\":{"
                "\"textDocumentSync\":1,"
                "\"completionProvider\":{\"triggerCharacters\":[\"\\\"\",\" \"]},"
                "\"hoverProvider\":true,"
                "\"definitionProvider\":true"
                "},\"serverInfo\":{\"name\":\"nc-lsp\",\"version\":\"1.0.0\"}}");
            initialized = true;
        }
        else if (strstr(method_ptr, "\"initialized\"")) {
            /* Client acknowledged */
        }
        else if (strstr(method_ptr, "\"shutdown\"")) {
            lsp_send_response(id, "null");
        }
        else if (strstr(method_ptr, "\"exit\"")) {
            free(msg);
            break;
        }
        else if (strstr(method_ptr, "\"textDocument/didOpen\"") ||
                 strstr(method_ptr, "\"textDocument/didChange\"")) {
            /* Extract URI and text for diagnostics */
            char *uri_ptr = strstr(msg, "\"uri\"");
            if (uri_ptr) {
                char *start = strchr(uri_ptr + 4, '"') + 1;
                char *end = strchr(start, '"');
                if (start && end) {
                    int len = (int)(end - start);
                    if (len > 511) len = 511; strncpy(open_uri, start, len);
                    open_uri[len] = '\0';
                }
            }
            char *text_ptr = strstr(msg, "\"text\"");
            if (text_ptr) {
                char *start = strchr(text_ptr + 5, '"') + 1;
                /* Simple extraction — in production use proper JSON parser */
                if (start) {
                    if (open_source) free(open_source);
                    /* Find end of text value (simplified) */
                    int len = 0;
                    while (start[len] && !(start[len] == '"' && start[len - 1] != '\\')) len++;
                    open_source = malloc(len + 1);
                    memcpy(open_source, start, len);
                    open_source[len] = '\0';
                    lsp_publish_diagnostics(open_uri, open_source);
                }
            }
        }
        else if (strstr(method_ptr, "\"textDocument/completion\"")) {
            lsp_send_response(id, COMPLETIONS_JSON);
        }
        else if (strstr(method_ptr, "\"textDocument/definition\"")) {
            /* Go-to-definition: find behavior/type at cursor position */
            int line_num = 0, col = 0;
            char *pos_ptr = strstr(msg, "\"position\"");
            if (pos_ptr) {
                char *ln = strstr(pos_ptr, "\"line\"");
                if (ln) { char *c = strchr(ln, ':'); if (c) line_num = atoi(c + 1); }
                char *ch = strstr(pos_ptr, "\"character\"");
                if (ch) { char *c = strchr(ch, ':'); if (c) col = atoi(c + 1); }
            }

            /* Find the word at cursor position */
            char word[128] = "";
            if (open_source) {
                const char *p = open_source;
                int cur_line = 0;
                while (*p && cur_line < line_num) {
                    if (*p == '\n' || (*p == '\\' && *(p+1) == 'n')) {
                        cur_line++;
                        if (*p == '\\') p++;
                    }
                    p++;
                }
                int line_start = (int)(p - open_source);
                int line_end = line_start;
                while (open_source[line_end] && open_source[line_end] != '\n' &&
                       !(open_source[line_end] == '\\' && open_source[line_end+1] == 'n'))
                    line_end++;

                /* Find word under col */
                int pos = line_start + col;
                if (pos < line_end) {
                    int ws = pos, we = pos;
                    while (ws > line_start && open_source[ws-1] != ' ' &&
                           open_source[ws-1] != '"' && open_source[ws-1] != '\t') ws--;
                    while (we < line_end && open_source[we] != ' ' &&
                           open_source[we] != '"' && open_source[we] != '\t' &&
                           open_source[we] != '(' && open_source[we] != ')') we++;
                    int wl = we - ws;
                    if (wl > 0 && wl < 127) { memcpy(word, open_source + ws, wl); word[wl] = '\0'; }
                }
            }

            /* Search for behavior or type definition in source */
            bool found = false;
            if (open_source && word[0]) {
                NcLexer *dl = nc_lexer_new(open_source, open_uri);
                nc_lexer_tokenize(dl);
                NcParser *dp = nc_parser_new(dl->tokens, dl->token_count, open_uri);
                NcASTNode *dprog = nc_parser_parse(dp);
                if (!dp->had_error) {
                    /* Search behaviors */
                    for (int b = 0; b < dprog->as.program.beh_count; b++) {
                        NcASTNode *beh = dprog->as.program.behaviors[b];
                        if (strcmp(beh->as.behavior.name->chars, word) == 0) {
                            char res[1024];
                            snprintf(res, sizeof(res),
                                "{\"uri\":\"%s\",\"range\":{"
                                "\"start\":{\"line\":%d,\"character\":0},"
                                "\"end\":{\"line\":%d,\"character\":0}}}",
                                open_uri, beh->line - 1, beh->line - 1);
                            lsp_send_response(id, res);
                            found = true;
                            break;
                        }
                    }
                    /* Search type definitions */
                    if (!found) {
                        for (int d = 0; d < dprog->as.program.def_count; d++) {
                            NcASTNode *def = dprog->as.program.definitions[d];
                            if (strcmp(def->as.definition.name->chars, word) == 0) {
                                char res[1024];
                                snprintf(res, sizeof(res),
                                    "{\"uri\":\"%s\",\"range\":{"
                                    "\"start\":{\"line\":%d,\"character\":0},"
                                    "\"end\":{\"line\":%d,\"character\":0}}}",
                                    open_uri, def->line - 1, def->line - 1);
                                lsp_send_response(id, res);
                                found = true;
                                break;
                            }
                        }
                    }
                }
                nc_parser_free(dp); nc_lexer_free(dl);
            }
            if (!found) lsp_send_response(id, "null");
        }
        else if (strstr(method_ptr, "\"textDocument/hover\"")) {
            /* Context-aware hover — show behavior purpose or type fields */
            int line_num = 0;
            char *pos_ptr = strstr(msg, "\"position\"");
            if (pos_ptr) {
                char *ln = strstr(pos_ptr, "\"line\"");
                if (ln) { char *c = strchr(ln, ':'); if (c) line_num = atoi(c + 1); }
            }

            char hover_text[1024] = "**Notation-as-Code**\\n\\nPlain English programming language.";
            if (open_source) {
                NcLexer *hl = nc_lexer_new(open_source, open_uri);
                nc_lexer_tokenize(hl);
                NcParser *hp = nc_parser_new(hl->tokens, hl->token_count, open_uri);
                NcASTNode *hprog = nc_parser_parse(hp);
                if (!hp->had_error) {
                    for (int b = 0; b < hprog->as.program.beh_count; b++) {
                        NcASTNode *beh = hprog->as.program.behaviors[b];
                        if (beh->line - 1 == line_num || beh->line == line_num) {
                            if (beh->as.behavior.purpose) {
                                snprintf(hover_text, sizeof(hover_text),
                                    "**%s**\\n\\n%s\\n\\nParameters: %d",
                                    beh->as.behavior.name->chars,
                                    beh->as.behavior.purpose->chars,
                                    beh->as.behavior.param_count);
                            } else {
                                snprintf(hover_text, sizeof(hover_text),
                                    "**%s**\\n\\nBehavior with %d parameters",
                                    beh->as.behavior.name->chars,
                                    beh->as.behavior.param_count);
                            }
                            break;
                        }
                    }
                }
                nc_parser_free(hp); nc_lexer_free(hl);
            }

            char resp[2048];
            snprintf(resp, sizeof(resp),
                "{\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"}}", hover_text);
            lsp_send_response(id, resp);
        }

        free(msg);
        (void)initialized;
    }

    if (open_source) free(open_source);
    return 0;
}
