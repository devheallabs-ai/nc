/*
 * nc_ui_compiler.c — NC UI Language Compiler
 *
 * Compiles .ncui source → NC bytecode (OP_UI_* opcodes).
 *
 * This is NOT a template engine (like old compiler.js was).
 * This is a REAL language compiler — like how tsc compiles TypeScript,
 * or how vue-compiler compiles .vue SFCs.
 *
 * Pipeline:
 *   .ncui source text
 *       ↓  nc_ui_lex()         — tokenize into NcUIToken stream
 *       ↓  nc_ui_parse()       — build AST (NcUINode tree)
 *       ↓  nc_ui_compile()     — walk AST → emit NC bytecode
 *   NcChunk (bytecode)
 *       ↓  nc_vm_execute_fast() — runs in NC VM
 *   VNode tree
 *       ↓  nc_ui_diff/patch    — updates DOM
 *
 * NC UI Grammar (what this compiler understands):
 *
 *   program         → declaration* EOF
 *   declaration     → app_decl | state_decl | theme_decl | routes_decl
 *                    | page_decl | component_decl | action_decl
 *                    | auth_decl | guard_decl | footer_decl
 *
 *   app_decl        → "app" STRING
 *   state_decl      → "state" ":" state_field*
 *   state_field     → IDENT "is" expr ("with" "type" STRING)?
 *
 *   page_decl       → "page" STRING ("title" STRING)? ":" page_body
 *   page_body       → guard_stmt? lifecycle_stmt* element*
 *
 *   element         → section_elem | card_elem | grid_elem | button_elem
 *                    | input_elem | text_elem | heading_elem | image_elem
 *                    | form_elem | if_elem | repeat_elem | component_use
 *
 *   section_elem    → "section" modifier* ":" element*
 *   card_elem       → "card" modifier* ":" element*
 *   button_elem     → "button" STRING modifier* event_handler?
 *   input_elem      → "input" STRING "type" expr? "bind" IDENT modifier*
 *   text_elem       → "text" expr
 *   heading_elem    → "heading" expr
 *
 *   if_elem         → "if" expr ":" element* ("otherwise" ":" element*)?
 *   repeat_elem     → "repeat" "for" "each" IDENT "in" expr ":" element*
 *
 *   event_handler   → "on" EVENT_NAME HANDLER
 *   HANDLER         → "navigates" "to" STRING | "runs" IDENT
 *
 *   guard_stmt      → "guard" STRING ":" guard_rule
 *   guard_rule      → "require" ("role" | "permission") STRING
 *                     "redirect" "to" STRING "when" "unauthorized"
 *
 *   action_decl     → "action" IDENT ("with" IDENT ("," IDENT)*)? ":" stmt*
 *   stmt            → "set" IDENT "to" expr
 *                    | "fetch" STRING ("with" "auth")? ("save" "as" IDENT)?
 *                    | "navigate" "to" STRING
 *                    | "try" ":" stmt* "on" "failure" ("as" IDENT)? ":" stmt*
 *                      "always" ":" stmt*
 *
 *   modifier        → "class" STRING | "style" STRING | "full-width"
 *                    | "loading" expr | "disabled" expr | "centered"
 *                    | "icon" STRING | "placeholder" STRING
 */

#include "../include/nc.h"
#include "../include/nc_chunk.h"

/* Portable strndup for Windows/MinGW */
#ifdef NC_WINDOWS
#ifndef strndup
static char *nc_strndup_(const char *s, size_t n) {
    size_t len = strlen(s);
    if (n < len) len = n;
    char *r = (char *)malloc(len + 1);
    if (r) { memcpy(r, s, len); r[len] = '\0'; }
    return r;
}
#define strndup nc_strndup_
#endif
#endif

/* ═══════════════════════════════════════════════════════════
 *  Token Types
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    /* Literals */
    NCUI_TOK_STRING,    /* "hello"                      */
    NCUI_TOK_NUMBER,    /* 42, 3.14                     */
    NCUI_TOK_BOOL,      /* true, false                  */
    NCUI_TOK_IDENT,     /* identifier                   */
    NCUI_TOK_COMMENT,   /* // ...                       */

    /* Keywords — App Structure */
    NCUI_TOK_APP,       /* app                          */
    NCUI_TOK_VERSION,   /* version                      */
    NCUI_TOK_DESCRIPTION,

    /* Keywords — State & Data */
    NCUI_TOK_STATE,     /* state                        */
    NCUI_TOK_IS,        /* is                           */
    NCUI_TOK_WITH,      /* with                         */
    NCUI_TOK_TYPE,      /* type                         */
    NCUI_TOK_COMPUTED,  /* computed                     */

    /* Keywords — Routing */
    NCUI_TOK_ROUTES,    /* routes                       */
    NCUI_TOK_SHOWS,     /* shows                        */
    NCUI_TOK_PAGE_KW,   /* page (keyword in routes)     */
    NCUI_TOK_PUBLICLY,  /* publicly                     */

    /* Keywords — Auth */
    NCUI_TOK_AUTH,      /* auth                         */
    NCUI_TOK_GUARD,     /* guard                        */
    NCUI_TOK_REQUIRE,   /* require                      */
    NCUI_TOK_ROLE,      /* role                         */
    NCUI_TOK_PERMISSION,/* permission                   */
    NCUI_TOK_REDIRECT,  /* redirect                     */
    NCUI_TOK_WHEN,      /* when                         */
    NCUI_TOK_UNAUTHORIZED,
    NCUI_TOK_AUTHENTICATED,

    /* Keywords — Theme */
    NCUI_TOK_THEME,     /* theme                        */

    /* Keywords — Pages & Components */
    NCUI_TOK_PAGE,      /* page "name":                 */
    NCUI_TOK_COMPONENT, /* component "name":            */
    NCUI_TOK_ACTION,    /* action name:                 */
    NCUI_TOK_FOOTER,    /* footer:                      */
    NCUI_TOK_HEADER,    /* header:                      */

    /* Keywords — Lifecycle */
    NCUI_TOK_ON,        /* on                           */
    NCUI_TOK_MOUNT,     /* mount                        */
    NCUI_TOK_UNMOUNT,   /* unmount                      */
    NCUI_TOK_BEFORE,    /* before                       */
    NCUI_TOK_AFTER,     /* after                        */

    /* Keywords — UI Elements */
    NCUI_TOK_SECTION,   /* section                      */
    NCUI_TOK_CARD,      /* card                         */
    NCUI_TOK_GRID,      /* grid                         */
    NCUI_TOK_ROW,       /* row                          */
    NCUI_TOK_BUTTON,    /* button                       */
    NCUI_TOK_INPUT,     /* input                        */
    NCUI_TOK_CHECKBOX,  /* checkbox                     */
    NCUI_TOK_SELECT,    /* select                       */
    NCUI_TOK_TEXTAREA,  /* textarea                     */
    NCUI_TOK_TEXT,      /* text                         */
    NCUI_TOK_HEADING,   /* heading                      */
    NCUI_TOK_SUBHEADING,/* subheading                   */
    NCUI_TOK_IMAGE,     /* image                        */
    NCUI_TOK_LINK,      /* link                         */
    NCUI_TOK_ICON,      /* icon                         */
    NCUI_TOK_DIVIDER,   /* divider                      */
    NCUI_TOK_BADGE,     /* badge                        */
    NCUI_TOK_TABLE,     /* table                        */
    NCUI_TOK_CHART,     /* chart                        */
    NCUI_TOK_FORM,      /* form                         */
    NCUI_TOK_ALERT,     /* alert                        */
    NCUI_TOK_MODAL,     /* modal                        */
    NCUI_TOK_NAV,       /* nav                          */
    NCUI_TOK_SIDEBAR,   /* sidebar                      */
    NCUI_TOK_LOADING,   /* loading (spinner)            */
    NCUI_TOK_SPACER,    /* spacer                       */

    /* Keywords — Control Flow (UI context) */
    NCUI_TOK_IF,        /* if                           */
    NCUI_TOK_OTHERWISE, /* otherwise                    */
    NCUI_TOK_REPEAT,    /* repeat                       */
    NCUI_TOK_FOR,       /* for                          */
    NCUI_TOK_EACH,      /* each                         */
    NCUI_TOK_IN,        /* in                           */
    NCUI_TOK_SHOW,      /* show                         */
    NCUI_TOK_HIDE,      /* hide                         */

    /* Keywords — Element Modifiers */
    NCUI_TOK_CLASS,     /* class                        */
    NCUI_TOK_STYLE,     /* style                        */
    NCUI_TOK_TITLE,     /* title                        */
    NCUI_TOK_BIND,      /* bind                         */
    NCUI_TOK_VALIDATE,  /* validate                     */
    NCUI_TOK_PLACEHOLDER,
    NCUI_TOK_AUTOCOMPLETE,
    NCUI_TOK_FULL_WIDTH,/* full-width                   */
    NCUI_TOK_LOADING_TEXT,
    NCUI_TOK_DISABLED,  /* disabled                     */
    NCUI_TOK_DISMISSIBLE,
    NCUI_TOK_CENTERED,  /* centered                     */
    NCUI_TOK_BETWEEN,   /* between                      */
    NCUI_TOK_COLUMNS,   /* columns                      */
    NCUI_TOK_ROWS,      /* rows                         */

    /* Keywords — Actions / Events */
    NCUI_TOK_RUNS,      /* runs                         */
    NCUI_TOK_NAVIGATES, /* navigates                    */
    NCUI_TOK_TO,        /* to                           */
    NCUI_TOK_SET,       /* set                          */
    NCUI_TOK_FETCH,     /* fetch                        */
    NCUI_TOK_NAVIGATE,  /* navigate                     */
    NCUI_TOK_TRY,       /* try                          */
    NCUI_TOK_FAILURE,   /* failure                      */
    NCUI_TOK_ALWAYS,    /* always                       */
    NCUI_TOK_AS,        /* as                           */
    NCUI_TOK_SAVE,      /* save                         */
    NCUI_TOK_EMIT,      /* emit                         */
    NCUI_TOK_CLICK,     /* click                        */
    NCUI_TOK_SUBMIT,    /* submit                       */
    NCUI_TOK_CHANGE,    /* change                       */
    NCUI_TOK_INPUT_EVENT,/* input                       */

    /* Keywords — Expressions */
    NCUI_TOK_THEN,      /* then                         */
    NCUI_TOK_NOT,       /* not                          */
    NCUI_TOK_AND,       /* and                          */
    NCUI_TOK_OR,        /* or                           */

    /* Punctuation */
    NCUI_TOK_COLON,     /* :                            */
    NCUI_TOK_COMMA,     /* ,                            */
    NCUI_TOK_DOT,       /* .                            */
    NCUI_TOK_PLUS,      /* +                            */
    NCUI_TOK_MINUS,     /* -                            */
    NCUI_TOK_SLASH,     /* /                            */
    NCUI_TOK_NEWLINE,   /* \n (significant for indent)  */
    NCUI_TOK_INDENT,    /* increase in indentation      */
    NCUI_TOK_DEDENT,    /* decrease in indentation      */

    NCUI_TOK_EOF,
    NCUI_TOK_ERROR,
} NcUITokenType;

typedef struct {
    NcUITokenType type;
    const char   *start;
    int           length;
    int           line;
    int           col;
    double        number;   /* for NCUI_TOK_NUMBER */
} NcUIToken;

/* ═══════════════════════════════════════════════════════════
 *  Lexer
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *source;
    const char *current;
    int         line;
    int         col;
    int         indent_stack[64]; /* track indentation levels   */
    int         indent_depth;
    NcUIToken   lookahead[3];     /* lookahead buffer           */
    int         la_count;
} NcUILexer;

static void nc_ui_lexer_init(NcUILexer *lex, const char *source) {
    lex->source       = source;
    lex->current      = source;
    lex->line         = 1;
    lex->col          = 0;
    lex->indent_depth = 0;
    lex->indent_stack[0] = 0;
    lex->la_count     = 0;
}

static bool nc_ui_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
}
static bool nc_ui_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool nc_ui_is_alnum(char c) { return nc_ui_is_alpha(c) || nc_ui_is_digit(c); }

/* Map identifier string to keyword token type */
static NcUITokenType nc_ui_keyword(const char *s, int len) {
    /* Simple keyword table — use a hash in production */
    static const struct { const char *kw; NcUITokenType t; } kws[] = {
        {"app",           NCUI_TOK_APP},
        {"version",       NCUI_TOK_VERSION},
        {"state",         NCUI_TOK_STATE},
        {"is",            NCUI_TOK_IS},
        {"with",          NCUI_TOK_WITH},
        {"type",          NCUI_TOK_TYPE},
        {"computed",      NCUI_TOK_COMPUTED},
        {"routes",        NCUI_TOK_ROUTES},
        {"shows",         NCUI_TOK_SHOWS},
        {"publicly",      NCUI_TOK_PUBLICLY},
        {"auth",          NCUI_TOK_AUTH},
        {"guard",         NCUI_TOK_GUARD},
        {"require",       NCUI_TOK_REQUIRE},
        {"role",          NCUI_TOK_ROLE},
        {"permission",    NCUI_TOK_PERMISSION},
        {"redirect",      NCUI_TOK_REDIRECT},
        {"when",          NCUI_TOK_WHEN},
        {"unauthorized",  NCUI_TOK_UNAUTHORIZED},
        {"authenticated", NCUI_TOK_AUTHENTICATED},
        {"theme",         NCUI_TOK_THEME},
        {"page",          NCUI_TOK_PAGE},
        {"component",     NCUI_TOK_COMPONENT},
        {"action",        NCUI_TOK_ACTION},
        {"footer",        NCUI_TOK_FOOTER},
        {"header",        NCUI_TOK_HEADER},
        {"on",            NCUI_TOK_ON},
        {"mount",         NCUI_TOK_MOUNT},
        {"unmount",       NCUI_TOK_UNMOUNT},
        {"section",       NCUI_TOK_SECTION},
        {"card",          NCUI_TOK_CARD},
        {"grid",          NCUI_TOK_GRID},
        {"row",           NCUI_TOK_ROW},
        {"button",        NCUI_TOK_BUTTON},
        {"input",         NCUI_TOK_INPUT},
        {"checkbox",      NCUI_TOK_CHECKBOX},
        {"text",          NCUI_TOK_TEXT},
        {"heading",       NCUI_TOK_HEADING},
        {"subheading",    NCUI_TOK_SUBHEADING},
        {"image",         NCUI_TOK_IMAGE},
        {"link",          NCUI_TOK_LINK},
        {"divider",       NCUI_TOK_DIVIDER},
        {"badge",         NCUI_TOK_BADGE},
        {"table",         NCUI_TOK_TABLE},
        {"chart",         NCUI_TOK_CHART},
        {"form",          NCUI_TOK_FORM},
        {"alert",         NCUI_TOK_ALERT},
        {"modal",         NCUI_TOK_MODAL},
        {"nav",           NCUI_TOK_NAV},
        {"sidebar",       NCUI_TOK_SIDEBAR},
        {"if",            NCUI_TOK_IF},
        {"otherwise",     NCUI_TOK_OTHERWISE},
        {"repeat",        NCUI_TOK_REPEAT},
        {"for",           NCUI_TOK_FOR},
        {"each",          NCUI_TOK_EACH},
        {"in",            NCUI_TOK_IN},
        {"show",          NCUI_TOK_SHOW},
        {"hide",          NCUI_TOK_HIDE},
        {"class",         NCUI_TOK_CLASS},
        {"style",         NCUI_TOK_STYLE},
        {"title",         NCUI_TOK_TITLE},
        {"bind",          NCUI_TOK_BIND},
        {"validate",      NCUI_TOK_VALIDATE},
        {"placeholder",   NCUI_TOK_PLACEHOLDER},
        {"full-width",    NCUI_TOK_FULL_WIDTH},
        {"disabled",      NCUI_TOK_DISABLED},
        {"centered",      NCUI_TOK_CENTERED},
        {"between",       NCUI_TOK_BETWEEN},
        {"columns",       NCUI_TOK_COLUMNS},
        {"rows",          NCUI_TOK_ROWS},
        {"runs",          NCUI_TOK_RUNS},
        {"navigates",     NCUI_TOK_NAVIGATES},
        {"to",            NCUI_TOK_TO},
        {"set",           NCUI_TOK_SET},
        {"fetch",         NCUI_TOK_FETCH},
        {"navigate",      NCUI_TOK_NAVIGATE},
        {"try",           NCUI_TOK_TRY},
        {"failure",       NCUI_TOK_FAILURE},
        {"always",        NCUI_TOK_ALWAYS},
        {"as",            NCUI_TOK_AS},
        {"save",          NCUI_TOK_SAVE},
        {"emit",          NCUI_TOK_EMIT},
        {"click",         NCUI_TOK_CLICK},
        {"submit",        NCUI_TOK_SUBMIT},
        {"change",        NCUI_TOK_CHANGE},
        {"then",          NCUI_TOK_THEN},
        {"not",           NCUI_TOK_NOT},
        {"and",           NCUI_TOK_AND},
        {"or",            NCUI_TOK_OR},
        {"true",          NCUI_TOK_BOOL},
        {"false",         NCUI_TOK_BOOL},
        {NULL,            NCUI_TOK_IDENT},
    };

    for (int i = 0; kws[i].kw; i++) {
        int kl = (int)strlen(kws[i].kw);
        if (kl == len && strncmp(kws[i].kw, s, len) == 0) {
            return kws[i].t;
        }
    }
    return NCUI_TOK_IDENT;
}

/* Scan the next token from source */
static NcUIToken nc_ui_scan(NcUILexer *lex) {
    /* Skip whitespace (but not newlines — they're significant) */
    while (*lex->current == ' ' || *lex->current == '\t' || *lex->current == '\r') {
        lex->current++;
        lex->col++;
    }

    const char *start = lex->current;
    NcUIToken tok = { .start = start, .line = lex->line, .col = lex->col };

    if (*lex->current == '\0') { tok.type = NCUI_TOK_EOF; return tok; }

    /* Comment */
    if (lex->current[0] == '/' && lex->current[1] == '/') {
        while (*lex->current && *lex->current != '\n') lex->current++;
        tok.type   = NCUI_TOK_COMMENT;
        tok.length = (int)(lex->current - start);
        return tok;
    }

    /* Newline */
    if (*lex->current == '\n') {
        lex->current++;
        lex->line++;
        lex->col = 0;
        tok.type   = NCUI_TOK_NEWLINE;
        tok.length = 1;
        return tok;
    }

    /* String literal */
    if (*lex->current == '"') {
        lex->current++; /* skip opening " */
        const char *str_start = lex->current;
        while (*lex->current && *lex->current != '"' && *lex->current != '\n') {
            lex->current++;
        }
        tok.type   = NCUI_TOK_STRING;
        tok.start  = str_start;
        tok.length = (int)(lex->current - str_start);
        if (*lex->current == '"') lex->current++; /* skip closing " */
        return tok;
    }

    /* Number */
    if (nc_ui_is_digit(*lex->current) ||
        (*lex->current == '-' && nc_ui_is_digit(lex->current[1]))) {
        while (nc_ui_is_digit(*lex->current) || *lex->current == '.') {
            lex->current++;
        }
        tok.type   = NCUI_TOK_NUMBER;
        tok.length = (int)(lex->current - start);
        tok.number = strtod(start, NULL);
        return tok;
    }

    /* Identifier / keyword */
    if (nc_ui_is_alpha(*lex->current)) {
        while (nc_ui_is_alnum(*lex->current)) lex->current++;
        tok.length = (int)(lex->current - start);
        tok.type   = nc_ui_keyword(start, tok.length);
        return tok;
    }

    /* Punctuation */
    tok.length = 1;
    switch (*lex->current++) {
        case ':': tok.type = NCUI_TOK_COLON;  return tok;
        case ',': tok.type = NCUI_TOK_COMMA;  return tok;
        case '.': tok.type = NCUI_TOK_DOT;    return tok;
        case '+': tok.type = NCUI_TOK_PLUS;   return tok;
        case '-': tok.type = NCUI_TOK_MINUS;  return tok;
    }

    tok.type = NCUI_TOK_ERROR;
    return tok;
}

/* ═══════════════════════════════════════════════════════════
 *  Code Generator — emits NC bytecode for .ncui constructs
 *
 *  Each emit_* function corresponds to a grammar production.
 *  This is a single-pass compiler (no AST needed for simple cases).
 *  For complex expressions, a small AST is built inline.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcUILexer  *lex;
    NcChunk    *chunk;    /* bytecode output                     */
    NcUIToken   current;  /* current token                       */
    NcUIToken   previous; /* previous token                      */
    bool        had_error;
    char        error_msg[512];
    int         indent;   /* current expected indent level       */
} NcUICompiler;

static void nc_ui_compiler_advance(NcUICompiler *c) {
    c->previous = c->current;
    do {
        c->current = nc_ui_scan(c->lex);
    } while (c->current.type == NCUI_TOK_COMMENT ||
             c->current.type == NCUI_TOK_NEWLINE);
}

static void nc_ui_compiler_error(NcUICompiler *c, const char *msg) {
    if (c->had_error) return;
    snprintf(c->error_msg, sizeof(c->error_msg),
             "Line %d: %s", c->current.line, msg);
    c->had_error = true;
}

static bool nc_ui_check(NcUICompiler *c, NcUITokenType t) {
    return c->current.type == t;
}

static bool nc_ui_match(NcUICompiler *c, NcUITokenType t) {
    if (!nc_ui_check(c, t)) return false;
    nc_ui_compiler_advance(c);
    return true;
}

static void nc_ui_expect(NcUICompiler *c, NcUITokenType t, const char *msg) {
    if (nc_ui_check(c, t)) { nc_ui_compiler_advance(c); return; }
    nc_ui_compiler_error(c, msg);
}

/* Emit a single opcode byte */
static void emit_op(NcUICompiler *c, NcOpCode op) {
    nc_chunk_write(c->chunk, (uint8_t)op, c->current.line);
}

/* Add a string constant and emit OP_CONSTANT + index */
static int emit_string_constant(NcUICompiler *c, const char *s, int len) {
    /* Build NC string value and add to chunk constants */
    char *buf = strndup(s, len);
    NcValue sv = NC_STRING_CONST(buf); /* creates interned NcString */
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    emit_op(c, OP_CONSTANT);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
    return idx;
}

/* Emit OP_UI_ELEMENT "tag" */
static void emit_element_open(NcUICompiler *c, const char *tag) {
    emit_op(c, OP_UI_ELEMENT);
    char *buf = strdup(tag);
    NcValue sv = NC_STRING_CONST(buf);
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
}

/* Emit OP_UI_PROP "name" after pushing value */
static void emit_prop(NcUICompiler *c, const char *prop_name, const char *value) {
    /* push value */
    emit_string_constant(c, value, strlen(value));
    /* emit prop instruction */
    emit_op(c, OP_UI_PROP);
    char *buf = strdup(prop_name);
    NcValue sv = NC_STRING_CONST(buf);
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
}

/* Emit OP_UI_END_ELEMENT */
static void emit_element_close(NcUICompiler *c) {
    emit_op(c, OP_UI_END_ELEMENT);
}

/* Emit OP_STATE_GET "slot_name" */
static void emit_state_get(NcUICompiler *c, const char *slot) {
    emit_op(c, OP_STATE_GET);
    char *buf = strdup(slot);
    NcValue sv = NC_STRING_CONST(buf);
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
}

/* Emit OP_STATE_SET "slot_name" */
static void emit_state_set(NcUICompiler *c, const char *slot) {
    emit_op(c, OP_STATE_SET);
    char *buf = strdup(slot);
    NcValue sv = NC_STRING_CONST(buf);
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
}

/* ── Parse a string token and push it onto stack ─────────── */
static void emit_expr_string(NcUICompiler *c) {
    if (!nc_ui_check(c, NCUI_TOK_STRING)) {
        nc_ui_compiler_error(c, "Expected string");
        return;
    }
    emit_string_constant(c, c->current.start, c->current.length);
    nc_ui_compiler_advance(c);
}

/* ── Parse state.field expression ───────────────────────── */
/* Handles: state.message, state.user.name, result.html */
static void emit_expr_field_access(NcUICompiler *c, const char *first_ident) {
    /* We already consumed the first ident — push it as var */
    emit_op(c, OP_GET_VAR);
    char *buf = strdup(first_ident);
    NcValue sv = NC_STRING_CONST(buf);
    int idx = nc_chunk_add_constant(c->chunk, sv);
    free(buf);
    nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);

    /* Consume .field chains */
    while (nc_ui_match(c, NCUI_TOK_DOT)) {
        if (!nc_ui_check(c, NCUI_TOK_IDENT)) {
            nc_ui_compiler_error(c, "Expected field name after '.'");
            return;
        }
        emit_op(c, OP_GET_FIELD);
        buf = strndup(c->current.start, c->current.length);
        sv  = NC_STRING_CONST(buf);
        idx = nc_chunk_add_constant(c->chunk, sv);
        free(buf);
        nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
        nc_ui_compiler_advance(c);
    }
}

/* ── Compile a button element ────────────────────────────── */
/*
 * Syntax:
 *   button "Sign In" style "primary" full-width
 *       loading state.loading
 *       on click runs handleLogin
 *
 * Emits:
 *   OP_UI_ELEMENT "button"
 *   OP_CONSTANT "Sign In"
 *   OP_UI_CHILD              ← text child
 *   OP_CONSTANT "primary"
 *   OP_UI_PROP "class"
 *   OP_STATE_GET "loading"
 *   OP_UI_BIND "data-loading" "loading"
 *   OP_UI_ON_EVENT "click" <handler_chunk_idx>
 *   OP_UI_END_ELEMENT
 */
static void compile_button(NcUICompiler *c) {
    emit_element_open(c, "button");

    /* Button label text */
    if (nc_ui_check(c, NCUI_TOK_STRING)) {
        NcUIToken label = c->current;
        nc_ui_compiler_advance(c);
        /* Create text VNode child */
        emit_string_constant(c, label.start, label.length);
        emit_op(c, OP_UI_TEXT);
        emit_op(c, OP_UI_CHILD);
    }

    /* Modifiers */
    while (true) {
        if (nc_ui_match(c, NCUI_TOK_STYLE)) {
            /* style "primary" → class="ncui-btn-primary" */
            if (nc_ui_check(c, NCUI_TOK_STRING)) {
                char class_val[128];
                snprintf(class_val, sizeof(class_val),
                         "ncui-btn-%.*s", c->current.length, c->current.start);
                emit_prop(c, "class", class_val);
                nc_ui_compiler_advance(c);
            }
        } else if (nc_ui_match(c, NCUI_TOK_CLASS)) {
            if (nc_ui_check(c, NCUI_TOK_STRING)) {
                emit_prop(c, "class",
                          strndup(c->current.start, c->current.length));
                nc_ui_compiler_advance(c);
            }
        } else if (nc_ui_match(c, NCUI_TOK_FULL_WIDTH)) {
            emit_prop(c, "data-full-width", "true");
        } else if (nc_ui_match(c, NCUI_TOK_DISABLED)) {
            /* disabled expr → OP_STATE_GET + OP_UI_BIND "disabled" */
            if (nc_ui_check(c, NCUI_TOK_IDENT)) {
                char slot[128];
                snprintf(slot, sizeof(slot), "%.*s",
                         c->current.length, c->current.start);
                nc_ui_compiler_advance(c);
                emit_op(c, OP_UI_BIND);
                /* operands: prop_name idx, state_key idx */
                NcValue pn = NC_STRING_CONST(strdup("disabled"));
                int pi = nc_chunk_add_constant(c->chunk, pn);
                nc_chunk_write(c->chunk, (uint8_t)pi, c->current.line);
                NcValue sk = NC_STRING_CONST(strdup(slot));
                int si = nc_chunk_add_constant(c->chunk, sk);
                nc_chunk_write(c->chunk, (uint8_t)si, c->current.line);
            }
        } else if (nc_ui_match(c, NCUI_TOK_ON)) {
            /* on click runs handler / on click navigates to "/path" */
            NcUITokenType event_type = c->current.type;
            nc_ui_compiler_advance(c); /* consume event name */
            (void)event_type;

            if (nc_ui_match(c, NCUI_TOK_RUNS)) {
                /* runs handler_name → OP_UI_ON_EVENT "click" handler_idx */
                if (nc_ui_check(c, NCUI_TOK_IDENT)) {
                    emit_op(c, OP_UI_ON_EVENT);
                    /* event name */
                    NcValue en = NC_STRING_CONST(strdup("click"));
                    int ei = nc_chunk_add_constant(c->chunk, en);
                    nc_chunk_write(c->chunk, (uint8_t)ei, c->current.line);
                    /* handler name */
                    NcValue hn = NC_STRING_CONST(
                        strndup(c->current.start, c->current.length));
                    int hi = nc_chunk_add_constant(c->chunk, hn);
                    nc_chunk_write(c->chunk, (uint8_t)hi, c->current.line);
                    nc_ui_compiler_advance(c);
                }
            } else if (nc_ui_match(c, NCUI_TOK_NAVIGATES)) {
                nc_ui_expect(c, NCUI_TOK_TO, "Expected 'to' after 'navigates'");
                if (nc_ui_check(c, NCUI_TOK_STRING)) {
                    /* OP_UI_ROUTE_PUSH "path" */
                    emit_op(c, OP_UI_ROUTE_PUSH);
                    NcValue pv = NC_STRING_CONST(
                        strndup(c->current.start, c->current.length));
                    int pi2 = nc_chunk_add_constant(c->chunk, pv);
                    nc_chunk_write(c->chunk, (uint8_t)pi2, c->current.line);
                    nc_ui_compiler_advance(c);
                }
            }
        } else {
            break;
        }
    }

    emit_element_close(c);
}

/* ── Compile an input element ────────────────────────────── */
/*
 * Syntax:
 *   input "Email address" type "email" bind email
 *       validate email required
 *       placeholder "you@company.com"
 *
 * Emits:
 *   OP_UI_ELEMENT "input"
 *   OP_UI_PROP "type" "email"
 *   OP_UI_PROP "placeholder" "you@company.com"
 *   OP_UI_BIND_INPUT "email"       ← two-way bind to state.email
 *   OP_UI_VALIDATE "email"
 *   OP_UI_VALIDATE "required"
 *   OP_UI_END_ELEMENT
 */
static void compile_input(NcUICompiler *c) {
    emit_element_open(c, "input");

    /* Label → aria-label */
    if (nc_ui_check(c, NCUI_TOK_STRING)) {
        emit_prop(c, "aria-label",
                  strndup(c->current.start, c->current.length));
        nc_ui_compiler_advance(c);
    }

    /* type "email" etc. */
    if (nc_ui_match(c, NCUI_TOK_TYPE)) {
        if (nc_ui_check(c, NCUI_TOK_STRING)) {
            emit_prop(c, "type",
                      strndup(c->current.start, c->current.length));
            nc_ui_compiler_advance(c);
        }
    }

    /* bind state_slot → two-way binding */
    if (nc_ui_match(c, NCUI_TOK_BIND)) {
        if (nc_ui_check(c, NCUI_TOK_IDENT)) {
            emit_op(c, OP_UI_BIND_INPUT);
            NcValue sk = NC_STRING_CONST(
                strndup(c->current.start, c->current.length));
            int si = nc_chunk_add_constant(c->chunk, sk);
            nc_chunk_write(c->chunk, (uint8_t)si, c->current.line);
            nc_ui_compiler_advance(c);
        }
    }

    /* validate / placeholder / autocomplete modifiers */
    while (true) {
        if (nc_ui_match(c, NCUI_TOK_VALIDATE)) {
            /* emit validate rules */
            while (nc_ui_check(c, NCUI_TOK_IDENT) ||
                   nc_ui_check(c, NCUI_TOK_STRING)) {
                emit_op(c, OP_UI_VALIDATE);
                if (nc_ui_check(c, NCUI_TOK_IDENT)) {
                    NcValue rv = NC_STRING_CONST(
                        strndup(c->current.start, c->current.length));
                    int ri = nc_chunk_add_constant(c->chunk, rv);
                    nc_chunk_write(c->chunk, (uint8_t)ri, c->current.line);
                }
                nc_ui_compiler_advance(c);
            }
        } else if (nc_ui_match(c, NCUI_TOK_PLACEHOLDER)) {
            if (nc_ui_check(c, NCUI_TOK_STRING)) {
                emit_prop(c, "placeholder",
                          strndup(c->current.start, c->current.length));
                nc_ui_compiler_advance(c);
            }
        } else {
            break;
        }
    }

    emit_element_close(c);
}

/* ── Compile a text element ──────────────────────────────── */
/*
 * Syntax:
 *   text "Hello"
 *   text state.message
 *
 * Emits:
 *   OP_UI_ELEMENT "p"
 *   (push text value onto stack — string literal or state get)
 *   OP_UI_TEXT
 *   OP_UI_CHILD
 *   OP_UI_END_ELEMENT
 */
static void compile_text(NcUICompiler *c) {
    emit_element_open(c, "p");
    if (nc_ui_check(c, NCUI_TOK_STRING)) {
        emit_string_constant(c, c->current.start, c->current.length);
        nc_ui_compiler_advance(c);
    } else if (nc_ui_check(c, NCUI_TOK_IDENT)) {
        char ident[128];
        snprintf(ident, sizeof(ident), "%.*s",
                 c->current.length, c->current.start);
        nc_ui_compiler_advance(c);
        emit_expr_field_access(c, ident);
    }
    emit_op(c, OP_UI_TEXT);
    emit_op(c, OP_UI_CHILD);
    emit_element_close(c);
}

/* ── Compile a heading element ───────────────────────────── */
static void compile_heading(NcUICompiler *c) {
    emit_element_open(c, "h1");
    if (nc_ui_check(c, NCUI_TOK_STRING)) {
        emit_string_constant(c, c->current.start, c->current.length);
        nc_ui_compiler_advance(c);
        emit_op(c, OP_UI_TEXT);
        emit_op(c, OP_UI_CHILD);
    }
    emit_element_close(c);
}

/* ── Compile an if/otherwise block ──────────────────────── */
/*
 * Syntax:
 *   if state.error:
 *       alert state.error type "error"
 *   otherwise:
 *       text "All good"
 *
 * Emits:
 *   (condition expr)
 *   OP_UI_IF         ← pops condition
 *   <true-branch VNode>
 *   <false-branch VNode or OP_NONE>
 */
static void compile_if(NcUICompiler *c);  /* forward decl */
static void compile_element(NcUICompiler *c); /* forward decl */

static void compile_if(NcUICompiler *c) {
    /* condition expression */
    if (nc_ui_check(c, NCUI_TOK_IDENT)) {
        char ident[128];
        snprintf(ident, sizeof(ident), "%.*s",
                 c->current.length, c->current.start);
        nc_ui_compiler_advance(c);
        emit_expr_field_access(c, ident);
    }

    nc_ui_expect(c, NCUI_TOK_COLON, "Expected ':' after if condition");

    /* true branch — compile single element */
    emit_element_open(c, "div");
    compile_element(c);
    emit_element_close(c);

    /* false branch */
    if (nc_ui_match(c, NCUI_TOK_OTHERWISE)) {
        nc_ui_match(c, NCUI_TOK_COLON);
        emit_element_open(c, "div");
        compile_element(c);
        emit_element_close(c);
    } else {
        emit_op(c, OP_NONE); /* no false branch */
    }

    emit_op(c, OP_UI_IF);
}

/* ── Compile a single element ────────────────────────────── */
static void compile_element(NcUICompiler *c) {
    switch (c->current.type) {
        case NCUI_TOK_BUTTON:
            nc_ui_compiler_advance(c);
            compile_button(c);
            break;
        case NCUI_TOK_INPUT:
            nc_ui_compiler_advance(c);
            compile_input(c);
            break;
        case NCUI_TOK_TEXT:
            nc_ui_compiler_advance(c);
            compile_text(c);
            break;
        case NCUI_TOK_HEADING:
            nc_ui_compiler_advance(c);
            compile_heading(c);
            break;
        case NCUI_TOK_IF:
            nc_ui_compiler_advance(c);
            compile_if(c);
            break;
        case NCUI_TOK_SECTION:
        case NCUI_TOK_CARD:
        case NCUI_TOK_GRID:
        case NCUI_TOK_ROW: {
            /* Generic container: section/card/grid/row */
            const char *tag_map[] = {"section","div","div","div"};
            int tidx = (c->current.type == NCUI_TOK_SECTION) ? 0 :
                       (c->current.type == NCUI_TOK_CARD)    ? 1 :
                       (c->current.type == NCUI_TOK_GRID)    ? 2 : 3;
            nc_ui_compiler_advance(c);
            emit_element_open(c, tag_map[tidx]);
            /* class modifier */
            if (nc_ui_match(c, NCUI_TOK_CLASS)) {
                if (nc_ui_check(c, NCUI_TOK_STRING)) {
                    emit_prop(c, "class",
                              strndup(c->current.start, c->current.length));
                    nc_ui_compiler_advance(c);
                }
            }
            if (nc_ui_match(c, NCUI_TOK_CENTERED))
                emit_prop(c, "data-centered", "true");
            nc_ui_expect(c, NCUI_TOK_COLON, "Expected ':' after container");
            /* children — read one element for now */
            /* In full compiler: read until dedent */
            compile_element(c);
            emit_element_close(c);
            break;
        }
        default:
            /* skip unknown token */
            nc_ui_compiler_advance(c);
            break;
    }
}

/* ── Compile state declarations ──────────────────────────── */
/*
 * Syntax:
 *   state:
 *       email is "" with type "string"
 *       loading is false with type "boolean"
 *
 * Emits per slot:
 *   OP_CONSTANT ""          ← initial value
 *   OP_STATE_DECLARE "email"
 */
static void compile_state_block(NcUICompiler *c) {
    nc_ui_expect(c, NCUI_TOK_COLON, "Expected ':' after state");

    while (nc_ui_check(c, NCUI_TOK_IDENT)) {
        char slot_name[128];
        snprintf(slot_name, sizeof(slot_name), "%.*s",
                 c->current.length, c->current.start);
        nc_ui_compiler_advance(c);

        nc_ui_expect(c, NCUI_TOK_IS, "Expected 'is' in state declaration");

        /* Initial value */
        if (nc_ui_check(c, NCUI_TOK_STRING)) {
            emit_string_constant(c, c->current.start, c->current.length);
            nc_ui_compiler_advance(c);
        } else if (nc_ui_check(c, NCUI_TOK_NUMBER)) {
            NcValue nv = NC_NUMBER(c->current.number);
            int idx = nc_chunk_add_constant(c->chunk, nv);
            emit_op(c, OP_CONSTANT);
            nc_chunk_write(c->chunk, (uint8_t)idx, c->current.line);
            nc_ui_compiler_advance(c);
        } else if (nc_ui_check(c, NCUI_TOK_BOOL)) {
            bool bval = strncmp(c->current.start, "true", 4) == 0;
            emit_op(c, bval ? OP_TRUE : OP_FALSE);
            nc_ui_compiler_advance(c);
        }

        /* Emit STATE_DECLARE */
        emit_op(c, OP_STATE_DECLARE);
        NcValue sn = NC_STRING_CONST(strdup(slot_name));
        int si = nc_chunk_add_constant(c->chunk, sn);
        nc_chunk_write(c->chunk, (uint8_t)si, c->current.line);

        /* Skip "with type ..." */
        if (nc_ui_match(c, NCUI_TOK_WITH)) {
            nc_ui_match(c, NCUI_TOK_TYPE);
            if (nc_ui_check(c, NCUI_TOK_STRING)) nc_ui_compiler_advance(c);
        }
    }
}

/* ── Compile a page declaration ──────────────────────────── */
/*
 * Syntax:
 *   page "login" title "Sign In":
 *       guard "authenticated": redirect to "/dashboard"
 *       on mount: set error to ""
 *       section centered class "login-page":
 *           ...
 *
 * Emits:
 *   OP_UI_COMPONENT "login"
 *   (lifecycle and element bytecode)
 */
static void compile_page(NcUICompiler *c) {
    /* page name */
    char page_name[128] = "unnamed";
    if (nc_ui_check(c, NCUI_TOK_STRING)) {
        snprintf(page_name, sizeof(page_name), "%.*s",
                 c->current.length, c->current.start);
        nc_ui_compiler_advance(c);
    }

    /* title (optional) */
    if (nc_ui_match(c, NCUI_TOK_TITLE)) {
        if (nc_ui_check(c, NCUI_TOK_STRING)) nc_ui_compiler_advance(c);
    }

    nc_ui_expect(c, NCUI_TOK_COLON, "Expected ':' after page declaration");

    /* Emit component definition start */
    emit_op(c, OP_UI_COMPONENT);
    NcValue pn = NC_STRING_CONST(strdup(page_name));
    int pi = nc_chunk_add_constant(c->chunk, pn);
    nc_chunk_write(c->chunk, (uint8_t)pi, c->current.line);

    /* Page body: guards, lifecycle, elements */
    /* In full compiler: read until EOF or next top-level declaration */
    /* Here we compile the first element as demo */
    if (!nc_ui_check(c, NCUI_TOK_EOF)) {
        compile_element(c);
    }
}

/* ── Top-level compiler entry point ─────────────────────── */
/*
 * Compiles a complete .ncui source file to a NcChunk.
 *
 * Returns the chunk on success, NULL on error.
 * Error message is written to error_out (caller must free).
 */
NcChunk *nc_ui_compile(const char *source, char **error_out) {
    NcUILexer    lex;
    NcUICompiler compiler;

    nc_ui_lexer_init(&lex, source);

    compiler.lex       = &lex;
    compiler.chunk     = nc_chunk_new();
    compiler.had_error = false;
    compiler.indent    = 0;
    memset(compiler.error_msg, 0, sizeof(compiler.error_msg));

    nc_ui_compiler_advance(&compiler); /* prime first token */

    /* Parse top-level declarations */
    while (!nc_ui_check(&compiler, NCUI_TOK_EOF) && !compiler.had_error) {
        if (nc_ui_match(&compiler, NCUI_TOK_APP)) {
            /* app "name" → skip, just consume */
            if (nc_ui_check(&compiler, NCUI_TOK_STRING))
                nc_ui_compiler_advance(&compiler);
        } else if (nc_ui_match(&compiler, NCUI_TOK_STATE)) {
            compile_state_block(&compiler);
        } else if (nc_ui_match(&compiler, NCUI_TOK_PAGE)) {
            compile_page(&compiler);
        } else if (nc_ui_match(&compiler, NCUI_TOK_THEME)) {
            /* skip theme for now */
            while (!nc_ui_check(&compiler, NCUI_TOK_EOF) &&
                   !nc_ui_check(&compiler, NCUI_TOK_PAGE) &&
                   !nc_ui_check(&compiler, NCUI_TOK_STATE)) {
                nc_ui_compiler_advance(&compiler);
            }
        } else {
            nc_ui_compiler_advance(&compiler); /* skip unknown */
        }
    }

    emit_op(&compiler, OP_HALT);

    if (compiler.had_error) {
        if (error_out) *error_out = strdup(compiler.error_msg);
        nc_chunk_free(compiler.chunk);
        return NULL;
    }

    if (error_out) *error_out = NULL;
    return compiler.chunk;
}
