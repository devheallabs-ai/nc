/*
 * nc_ui_html.c — Native NC UI HTML/CSS/JS Code Generator
 *
 * Compiles .ncui source → deployable HTML/CSS/JS, matching the output of
 * the original compiler.js. This eliminates the Node.js dependency for
 * `nc ui build`.
 *
 * Pipeline:
 *   .ncui source → Lex → Parse (AST) → HTML/CSS/JS code generation
 *
 * Copyright 2024 DevHeal Labs AI — Apache 2.0
 */

#include "nc_ui_compiler.h"
#include "../include/nc_ui_assets.h"
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════
 *  String Buffer — dynamic string for building output
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) { sb->data = NULL; sb->len = 0; sb->cap = 0; }

static void sb_ensure(StrBuf *sb, size_t extra) {
    if (sb->len + extra + 1 > sb->cap) {
        size_t newcap = (sb->cap == 0) ? 4096 : sb->cap;
        while (newcap < sb->len + extra + 1) newcap *= 2;
        sb->data = (char *)realloc(sb->data, newcap);
        sb->cap = newcap;
    }
}

static void sb_append(StrBuf *sb, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    sb_ensure(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_appendn(StrBuf *sb, const char *s, size_t n) {
    if (!s || n == 0) return;
    sb_ensure(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_printf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) { va_end(ap2); return; }
    sb_ensure(sb, (size_t)needed);
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, ap2);
    sb->len += (size_t)needed;
    va_end(ap2);
}

static void sb_free(StrBuf *sb) { free(sb->data); sb->data = NULL; sb->len = sb->cap = 0; }

/* HTML-escape a string into a buffer */
static void sb_append_escaped(StrBuf *sb, const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&':  sb_append(sb, "&amp;");  break;
            case '<':  sb_append(sb, "&lt;");   break;
            case '>':  sb_append(sb, "&gt;");   break;
            case '"':  sb_append(sb, "&quot;"); break;
            default:   sb_ensure(sb, 1); sb->data[sb->len++] = *p; sb->data[sb->len] = '\0';
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Portable strndup
 * ═══════════════════════════════════════════════════════════ */
#ifdef _WIN32
#ifndef strndup
static char *ncui_strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (n < len) len = n;
    char *r = (char *)malloc(len + 1);
    if (r) { memcpy(r, s, len); r[len] = '\0'; }
    return r;
}
#define strndup ncui_strndup
#endif
#endif

/* ═══════════════════════════════════════════════════════════
 *  Mini Lexer — re-implementation for HTML codegen
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    TK_EOF, TK_STRING, TK_NUMBER, TK_IDENT, TK_NEWLINE, TK_COLON,
    TK_COMMA, TK_DOT, TK_COMMENT,
    /* Keywords */
    TK_APP, TK_PAGE, TK_THEME, TK_ACCENT, TK_FONT, TK_VERSION,
    TK_STATE, TK_IS, TK_WITH, TK_TYPE, TK_COMPUTED,
    TK_NAV, TK_BRAND, TK_LINKS, TK_LINK, TK_FOOTER,
    TK_SECTION, TK_CARD, TK_GRID, TK_ROW, TK_COLUMN,
    TK_HEADING, TK_SUBHEADING, TK_TEXT, TK_BADGE,
    TK_BUTTON, TK_INPUT, TK_TEXTAREA, TK_FORM, TK_TABLE,
    TK_IMAGE, TK_VIDEO, TK_DIVIDER, TK_SPACER,
    TK_ICON, TK_STAT, TK_PROGRESS, TK_ALERT, TK_LOADING,
    TK_MODAL, TK_TABS, TK_TAB,
    TK_IF, TK_OTHERWISE, TK_REPEAT, TK_FOR, TK_EACH, TK_IN,
    TK_ON, TK_CLICK, TK_SUBMIT, TK_MOUNT,
    TK_RUNS, TK_NAVIGATES, TK_TO,
    TK_STYLE, TK_CLASS, TK_FULL_WIDTH, TK_CENTERED,
    TK_FULLSCREEN, TK_HERO,
    TK_BIND, TK_VALIDATE, TK_PLACEHOLDER, TK_REQUIRED,
    TK_ACTION, TK_SET, TK_ADD, TK_REMOVE, TK_TOGGLE,
    TK_FETCH, TK_REDIRECT, TK_NAVIGATE,
    TK_ANIMATE, TK_COMPONENT, TK_USE, TK_SLOT,
    TK_ROUTES, TK_SHOWS, TK_PUBLICLY, TK_GUARD, TK_REQUIRE,
    TK_ROLE, TK_AUTH, TK_DISABLED, TK_DISMISSIBLE,
    TK_COLUMNS, TK_ROWS_KW, TK_RESPONSIVE,
    TK_SAVE, TK_AS, TK_RESPONSE, TK_METHOD,
    TK_TRUE, TK_FALSE, TK_SIDEBAR, TK_TOPBAR, TK_SHELL,
    TK_PANEL, TK_BANNER, TK_IMPORT, TK_FROM,
    TK_LIST, TK_ITEM, TK_HEADER, TK_BACKGROUND, TK_FOREGROUND,
} NcHTMLTokType;

typedef struct {
    NcHTMLTokType type;
    const char   *start;
    int           length;
    int           line;
    int           indent;
    double        number;
} NcHTMLToken;

typedef struct {
    const char *source;
    const char *cur;
    int         line;
    int         line_indent; /* indent level at start of current line */
    int        *indents;    /* indent stack */
    int         indent_sp;
    int         indent_cap;
} NcHTMLLexer;

static void lex_init(NcHTMLLexer *L, const char *src) {
    L->source = src; L->cur = src; L->line = 1;
    L->line_indent = 0;
    L->indent_cap = 64;
    L->indents = (int *)calloc((size_t)L->indent_cap, sizeof(int));
    L->indent_sp = 0;
    L->indents[0] = 0;
}
static void lex_free(NcHTMLLexer *L) { free(L->indents); }

static NcHTMLTokType lex_keyword(const char *s, int len) {
    #define KW(str, tok) if (len == (int)sizeof(str)-1 && strncmp(s, str, (size_t)len) == 0) return tok
    KW("app", TK_APP); KW("page", TK_PAGE); KW("theme", TK_THEME);
    KW("accent", TK_ACCENT); KW("font", TK_FONT); KW("version", TK_VERSION);
    KW("state", TK_STATE); KW("is", TK_IS); KW("with", TK_WITH);
    KW("type", TK_TYPE); KW("computed", TK_COMPUTED);
    KW("nav", TK_NAV); KW("brand", TK_BRAND); KW("links", TK_LINKS);
    KW("link", TK_LINK); KW("footer", TK_FOOTER);
    KW("section", TK_SECTION); KW("card", TK_CARD); KW("grid", TK_GRID);
    KW("row", TK_ROW); KW("column", TK_COLUMN);
    KW("heading", TK_HEADING); KW("subheading", TK_SUBHEADING);
    KW("text", TK_TEXT); KW("badge", TK_BADGE);
    KW("button", TK_BUTTON); KW("input", TK_INPUT);
    KW("textarea", TK_TEXTAREA); KW("form", TK_FORM); KW("table", TK_TABLE);
    KW("image", TK_IMAGE); KW("video", TK_VIDEO);
    KW("divider", TK_DIVIDER); KW("spacer", TK_SPACER);
    KW("list", TK_LIST); KW("item", TK_ITEM);
    KW("icon", TK_ICON); KW("stat", TK_STAT); KW("progress", TK_PROGRESS);
    KW("alert", TK_ALERT); KW("loading", TK_LOADING);
    KW("modal", TK_MODAL); KW("tabs", TK_TABS); KW("tab", TK_TAB);
    KW("if", TK_IF); KW("otherwise", TK_OTHERWISE);
    KW("repeat", TK_REPEAT); KW("for", TK_FOR); KW("each", TK_EACH);
    KW("in", TK_IN);
    KW("on", TK_ON); KW("click", TK_CLICK); KW("submit", TK_SUBMIT);
    KW("mount", TK_MOUNT);
    KW("runs", TK_RUNS); KW("navigates", TK_NAVIGATES); KW("to", TK_TO);
    KW("style", TK_STYLE); KW("class", TK_CLASS);
    KW("full-width", TK_FULL_WIDTH); KW("centered", TK_CENTERED);
    KW("fullscreen", TK_FULLSCREEN); KW("hero", TK_HERO);
    KW("bind", TK_BIND); KW("validate", TK_VALIDATE);
    KW("placeholder", TK_PLACEHOLDER); KW("required", TK_REQUIRED);
    KW("action", TK_ACTION); KW("set", TK_SET); KW("add", TK_ADD);
    KW("remove", TK_REMOVE); KW("toggle", TK_TOGGLE);
    KW("fetch", TK_FETCH); KW("redirect", TK_REDIRECT);
    KW("navigate", TK_NAVIGATE);
    KW("animate", TK_ANIMATE); KW("component", TK_COMPONENT);
    KW("use", TK_USE); KW("slot", TK_SLOT);
    KW("routes", TK_ROUTES); KW("shows", TK_SHOWS);
    KW("publicly", TK_PUBLICLY); KW("guard", TK_GUARD);
    KW("require", TK_REQUIRE); KW("role", TK_ROLE); KW("auth", TK_AUTH);
    KW("disabled", TK_DISABLED); KW("dismissible", TK_DISMISSIBLE);
    KW("columns", TK_COLUMNS); KW("rows", TK_ROWS_KW);
    KW("responsive", TK_RESPONSIVE);
    KW("save", TK_SAVE); KW("as", TK_AS); KW("response", TK_RESPONSE);
    KW("method", TK_METHOD);
    KW("true", TK_TRUE); KW("false", TK_FALSE);
    KW("sidebar", TK_SIDEBAR); KW("topbar", TK_TOPBAR);
    KW("shell", TK_SHELL); KW("panel", TK_PANEL); KW("banner", TK_BANNER);
    KW("import", TK_IMPORT); KW("from", TK_FROM);
    KW("header", TK_HEADER); KW("background", TK_BACKGROUND);
    KW("foreground", TK_FOREGROUND);
    #undef KW
    return TK_IDENT;
}

static bool lex_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
}
static bool lex_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool lex_is_alnum(char c) { return lex_is_alpha(c) || lex_is_digit(c); }

static NcHTMLToken lex_next(NcHTMLLexer *L) {
    /* skip spaces/tabs (not newlines), measuring indent at line start */
    int col = 0;
    bool at_line_start = (L->cur == L->source || (L->cur > L->source && *(L->cur - 1) == '\n'));
    while (*L->cur == ' ' || *L->cur == '\t' || *L->cur == '\r') {
        if (*L->cur == '\t') col += 4; else if (*L->cur == ' ') col += 1;
        L->cur++;
    }
    if (at_line_start) L->line_indent = col;
    NcHTMLToken t = { .start = L->cur, .line = L->line, .length = 0, .number = 0, .indent = L->line_indent };
    if (*L->cur == '\0') { t.type = TK_EOF; return t; }
    /* comment */
    if (L->cur[0] == '/' && L->cur[1] == '/') {
        while (*L->cur && *L->cur != '\n') L->cur++;
        t.type = TK_COMMENT; t.length = (int)(L->cur - t.start); return t;
    }
    /* newline */
    if (*L->cur == '\n') { L->cur++; L->line++; t.type = TK_NEWLINE; t.length = 1; return t; }
    /* string */
    if (*L->cur == '"') {
        L->cur++;
        const char *s = L->cur;
        while (*L->cur && *L->cur != '"' && *L->cur != '\n') L->cur++;
        t.type = TK_STRING; t.start = s; t.length = (int)(L->cur - s);
        if (*L->cur == '"') L->cur++;
        return t;
    }
    /* number */
    if (lex_is_digit(*L->cur) || (*L->cur == '-' && lex_is_digit(L->cur[1]))) {
        const char *ns = L->cur;
        if (*L->cur == '-') L->cur++;
        while (lex_is_digit(*L->cur) || *L->cur == '.') L->cur++;
        t.type = TK_NUMBER; t.length = (int)(L->cur - ns); t.start = ns;
        t.number = strtod(ns, NULL);
        return t;
    }
    /* identifier / keyword */
    if (lex_is_alpha(*L->cur)) {
        while (lex_is_alnum(*L->cur)) L->cur++;
        t.length = (int)(L->cur - t.start);
        t.type = lex_keyword(t.start, t.length);
        return t;
    }
    /* punctuation */
    t.length = 1;
    switch (*L->cur++) {
        case ':': t.type = TK_COLON; return t;
        case ',': t.type = TK_COMMA; return t;
        case '.': t.type = TK_DOT;   return t;
    }
    /* skip unknown chars and recurse */
    return lex_next(L);
}

/* ═══════════════════════════════════════════════════════════
 *  AST Node Types
 * ═══════════════════════════════════════════════════════════ */

#define MAX_CHILDREN 128
#define MAX_STATES   64
#define MAX_ACTIONS  32
#define MAX_STMTS    32
#define MAX_ROUTES   32
#define MAX_COMPS    32
#define MAX_STR      512

typedef enum {
    NODE_NAV, NODE_BRAND, NODE_LINKS, NODE_LINK, NODE_FOOTER,
    NODE_SECTION, NODE_CARD, NODE_GRID, NODE_ROW, NODE_COLUMN,
    NODE_HEADING, NODE_SUBHEADING, NODE_TEXT, NODE_BADGE,
    NODE_BUTTON, NODE_INPUT, NODE_TEXTAREA, NODE_FORM, NODE_TABLE,
    NODE_IMAGE, NODE_VIDEO, NODE_DIVIDER, NODE_SPACER,
    NODE_ICON, NODE_STAT, NODE_PROGRESS, NODE_ALERT, NODE_LOADING,
    NODE_MODAL, NODE_TABS, NODE_TAB,
    NODE_IF, NODE_REPEAT,
    NODE_USE, NODE_SLOT, NODE_ANIMATE,
    NODE_SHELL, NODE_SIDEBAR, NODE_TOPBAR, NODE_PANEL, NODE_BANNER,
    NODE_LIST, NODE_ITEM, NODE_HEADER,
} NcNodeType;

typedef struct NcASTNode NcASTNode;
struct NcASTNode {
    NcNodeType  type;
    char        text[MAX_STR];        /* label/content string     */
    char        style_str[64];        /* style "primary" etc      */
    char        class_str[128];       /* explicit class           */
    char        href[MAX_STR];        /* link href / image src    */
    char        action_name[128];     /* on click runs X          */
    char        nav_to[MAX_STR];      /* on click navigates to X  */
    char        bind_field[128];      /* bind fieldname           */
    char        placeholder[MAX_STR];
    char        validate_rules[128];
    char        icon_name[64];
    char        id_str[64];           /* section "hero" id        */
    char        condition[128];       /* if condition             */
    char        collection[128];      /* repeat X in collection   */
    char        item_var[64];         /* repeat item variable     */
    char        component_name[128];  /* use ComponentName        */
    char        animate_type[64];     /* animate "fade-up"        */
    char        variant[32];          /* alert variant            */
    char        trigger_text[64];     /* modal trigger            */
    char        method_str[16];       /* form method              */
    char        action_url[MAX_STR];  /* form action url          */
    int         grid_cols;            /* grid N columns           */
    int         stat_value_i;
    char        stat_value[64];
    char        stat_label[128];
    double      progress_val;
    bool        is_required;
    bool        is_full_width;
    bool        is_centered;
    bool        is_fullscreen;
    bool        is_hero;
    bool        is_dismissible;
    bool        is_disabled;
    bool        with_auth;
    bool        is_responsive;
    NcASTNode  *children[MAX_CHILDREN];
    int         child_count;
    NcASTNode  *else_children[MAX_CHILDREN]; /* if/otherwise */
    int         else_child_count;
};

/* State slot */
typedef struct {
    char name[128];
    char initial[MAX_STR]; /* string repr of initial value */
    char type_hint[32];    /* "string", "boolean", "number", "array" */
} NcStateSlot;

/* Action statement */
typedef struct {
    char stmt_type[16]; /* "set", "add", "remove", "toggle", "fetch", "redirect" */
    char target[128];
    char expr[MAX_STR];
    char method[16];
    char save_as[128];
    bool with_auth;
} NcActionStmt;

/* Action block */
typedef struct {
    char         name[128];
    char         param[64];
    NcActionStmt stmts[MAX_STMTS];
    int          stmt_count;
} NcAction;

/* Route entry */
typedef struct {
    char path[MAX_STR];
    char component[128];
    bool is_public;
    bool require_auth;
    char require_role[64];
    char guard_name[64];
    char redirect_path[MAX_STR];
} NcRoute;

/* Component definition */
typedef struct {
    char        name[128];
    char        params[256];
    NcASTNode  *children[MAX_CHILDREN];
    int         child_count;
} NcComponent;

/* Full AST */
typedef struct {
    char          title[MAX_STR];
    char          theme[16];      /* "dark" or "light" */
    char          accent[16];     /* hex color */
    char          bg[32];         /* background color */
    char          fg[32];         /* text/foreground color */
    char          font[64];

    NcStateSlot   states[MAX_STATES];
    int           state_count;

    NcAction      actions[MAX_ACTIONS];
    int           action_count;

    NcRoute       routes[MAX_ROUTES];
    int           route_count;

    NcComponent   components[MAX_COMPS];
    int           comp_count;

    NcASTNode    *body[MAX_CHILDREN]; /* top-level nodes (nav, sections, footer) */
    int           body_count;

    bool          has_state;
    bool          has_routes;
    bool          has_auth;
    bool          has_actions;
} NcUIAST;

/* ═══════════════════════════════════════════════════════════
 *  Parser — builds AST from token stream
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcHTMLLexer  lex;
    NcHTMLToken  cur;
    NcHTMLToken  prev;
    NcUIAST     *ast;
    bool         had_error;
    char         error_msg[512];
} NcHTMLParser;

static NcASTNode *alloc_node(NcNodeType type) {
    NcASTNode *n = (NcASTNode *)calloc(1, sizeof(NcASTNode));
    n->type = type;
    return n;
}

static void free_node(NcASTNode *n) {
    if (!n) return;
    for (int i = 0; i < n->child_count; i++) free_node(n->children[i]);
    for (int i = 0; i < n->else_child_count; i++) free_node(n->else_children[i]);
    free(n);
}

static void parser_advance(NcHTMLParser *P) {
    P->prev = P->cur;
    do {
        P->cur = lex_next(&P->lex);
    } while (P->cur.type == TK_COMMENT || P->cur.type == TK_NEWLINE);
}

static bool parser_check(NcHTMLParser *P, NcHTMLTokType t) { return P->cur.type == t; }

static bool parser_match(NcHTMLParser *P, NcHTMLTokType t) {
    if (!parser_check(P, t)) return false;
    parser_advance(P);
    return true;
}

static void parser_error(NcHTMLParser *P, const char *msg) {
    if (P->had_error) return;
    snprintf(P->error_msg, sizeof(P->error_msg), "Line %d: %s", P->cur.line, msg);
    P->had_error = true;
}

/* Read current token as string into dest, advance */
static void parser_read_string(NcHTMLParser *P, char *dest, int maxlen) {
    if (P->cur.type == TK_STRING) {
        int n = P->cur.length < maxlen - 1 ? P->cur.length : maxlen - 1;
        memcpy(dest, P->cur.start, (size_t)n);
        dest[n] = '\0';
        parser_advance(P);
    } else {
        dest[0] = '\0';
    }
}

static void parser_read_ident(NcHTMLParser *P, char *dest, int maxlen) {
    if (P->cur.type == TK_IDENT || (P->cur.type >= TK_APP && P->cur.type <= TK_FROM)) {
        int n = P->cur.length < maxlen - 1 ? P->cur.length : maxlen - 1;
        memcpy(dest, P->cur.start, (size_t)n);
        dest[n] = '\0';
        parser_advance(P);
    } else {
        dest[0] = '\0';
    }
}

/* Forward declarations */
static NcASTNode *parse_element(NcHTMLParser *P);
static void parse_children(NcHTMLParser *P, NcASTNode **arr, int *count, int max);

/* Parse a block of children after a colon (indent-based) */
static void parse_children(NcHTMLParser *P, NcASTNode **arr, int *count, int max) {
    if (parser_match(P, TK_COLON)) {
        /* The parent element's colon was at parent_indent.
         * Children must be indented deeper than the parent.
         * When we see a token at parent_indent or less, stop. */
        int parent_indent = P->prev.indent;
        while (!parser_check(P, TK_EOF) && *count < max) {
            /* Check if next token is at same or shallower indent = sibling, not child */
            if (P->cur.indent <= parent_indent && P->cur.type != TK_EOF) {
                break;
            }
            NcASTNode *child = parse_element(P);
            if (child) {
                arr[(*count)++] = child;
            } else {
                /* Unknown token at child indent — skip it and keep trying.
                 * This handles leftover modifiers (e.g. unrecognized idents). */
                if (P->cur.indent > parent_indent && P->cur.type != TK_EOF) {
                    parser_advance(P);
                } else {
                    break;
                }
            }
        }
    }
}

/* Parse button modifiers */
static void parse_button_mods(NcHTMLParser *P, NcASTNode *n) {
    while (true) {
        if (parser_match(P, TK_STYLE)) {
            if (P->cur.type == TK_STRING)
                parser_read_string(P, n->style_str, sizeof(n->style_str));
            else if (P->cur.type == TK_IDENT)
                parser_read_ident(P, n->style_str, sizeof(n->style_str));
        } else if (parser_match(P, TK_CLASS)) {
            parser_read_string(P, n->class_str, sizeof(n->class_str));
        } else if (parser_match(P, TK_ACTION)) {
            /* action actionName (shorthand for on click runs) */
            if (P->cur.type == TK_STRING)
                parser_read_string(P, n->action_name, sizeof(n->action_name));
            else
                parser_read_ident(P, n->action_name, sizeof(n->action_name));
        } else if (parser_match(P, TK_FULL_WIDTH)) {
            n->is_full_width = true;
        } else if (parser_match(P, TK_DISABLED)) {
            n->is_disabled = true;
        } else if (parser_match(P, TK_ON)) {
            if (parser_match(P, TK_CLICK) || parser_match(P, TK_SUBMIT)) {
                if (parser_match(P, TK_RUNS)) {
                    parser_read_ident(P, n->action_name, sizeof(n->action_name));
                } else if (parser_match(P, TK_NAVIGATES)) {
                    parser_match(P, TK_TO);
                    parser_read_string(P, n->nav_to, sizeof(n->nav_to));
                }
            }
        } else {
            break;
        }
    }
}

/* Parse a single UI element */
static NcASTNode *parse_element(NcHTMLParser *P) {
    if (P->had_error || P->cur.type == TK_EOF) return NULL;

    /* heading "text" [size N] [style "gradient"] */
    if (parser_match(P, TK_HEADING)) {
        NcASTNode *n = alloc_node(NODE_HEADING);
        parser_read_string(P, n->text, sizeof(n->text));
        while (true) {
            if (parser_match(P, TK_STYLE)) {
                if (P->cur.type == TK_STRING)
                    parser_read_string(P, n->style_str, sizeof(n->style_str));
                else if (P->cur.type == TK_IDENT) { parser_read_ident(P, n->style_str, sizeof(n->style_str)); }
            } else if (P->cur.type == TK_IDENT && P->cur.length == 4 && strncmp(P->cur.start, "size", 4) == 0) {
                parser_advance(P); /* skip "size" */
                if (P->cur.type == TK_NUMBER) { n->grid_cols = (int)P->cur.number; parser_advance(P); }
            } else break;
        }
        return n;
    }
    if (parser_match(P, TK_SUBHEADING)) {
        NcASTNode *n = alloc_node(NODE_SUBHEADING);
        parser_read_string(P, n->text, sizeof(n->text));
        return n;
    }
    if (parser_match(P, TK_TEXT)) {
        NcASTNode *n = alloc_node(NODE_TEXT);
        if (P->cur.type == TK_STRING) {
            parser_read_string(P, n->text, sizeof(n->text));
        } else {
            /* state.field or ident */
            parser_read_ident(P, n->text, sizeof(n->text));
            while (parser_match(P, TK_DOT)) {
                size_t len = strlen(n->text);
                n->text[len] = '.';
                n->text[len + 1] = '\0';
                char part[128];
                parser_read_ident(P, part, sizeof(part));
                strncat(n->text, part, sizeof(n->text) - strlen(n->text) - 1);
            }
        }
        /* consume optional "size large/small/N" modifier */
        if (P->cur.type == TK_IDENT && P->cur.length == 4 && strncmp(P->cur.start, "size", 4) == 0) {
            parser_advance(P); /* skip "size" */
            if (P->cur.type == TK_IDENT || P->cur.type == TK_STRING || P->cur.type == TK_NUMBER)
                parser_advance(P); /* skip the size value */
        }
        return n;
    }
    if (parser_match(P, TK_BADGE)) {
        NcASTNode *n = alloc_node(NODE_BADGE);
        parser_read_string(P, n->text, sizeof(n->text));
        return n;
    }
    /* button "Label" style "primary" on click runs actionName */
    if (parser_match(P, TK_BUTTON)) {
        NcASTNode *n = alloc_node(NODE_BUTTON);
        parser_read_string(P, n->text, sizeof(n->text));
        parse_button_mods(P, n);
        return n;
    }
    /* link "text" to "url" */
    if (parser_match(P, TK_LINK)) {
        NcASTNode *n = alloc_node(NODE_LINK);
        parser_read_string(P, n->text, sizeof(n->text));
        if (parser_match(P, TK_TO))
            parser_read_string(P, n->href, sizeof(n->href));
        return n;
    }
    /* image "src" [class "rounded"] */
    if (parser_match(P, TK_IMAGE)) {
        NcASTNode *n = alloc_node(NODE_IMAGE);
        parser_read_string(P, n->href, sizeof(n->href));
        if (parser_match(P, TK_CLASS))
            parser_read_string(P, n->class_str, sizeof(n->class_str));
        return n;
    }
    /* video "src" */
    if (parser_match(P, TK_VIDEO)) {
        NcASTNode *n = alloc_node(NODE_VIDEO);
        parser_read_string(P, n->href, sizeof(n->href));
        return n;
    }
    if (parser_match(P, TK_DIVIDER)) { return alloc_node(NODE_DIVIDER); }
    if (parser_match(P, TK_SPACER))  { return alloc_node(NODE_SPACER); }

    /* list: items */
    if (parser_match(P, TK_LIST)) {
        NcASTNode *n = alloc_node(NODE_LIST);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* item "text" */
    if (parser_match(P, TK_ITEM)) {
        NcASTNode *n = alloc_node(NODE_ITEM);
        parser_read_string(P, n->text, sizeof(n->text));
        return n;
    }

    /* icon "name" */
    if (parser_match(P, TK_ICON)) {
        NcASTNode *n = alloc_node(NODE_ICON);
        parser_read_string(P, n->icon_name, sizeof(n->icon_name));
        return n;
    }
    /* stat "value" "label" */
    if (parser_match(P, TK_STAT)) {
        NcASTNode *n = alloc_node(NODE_STAT);
        parser_read_string(P, n->stat_value, sizeof(n->stat_value));
        parser_read_string(P, n->stat_label, sizeof(n->stat_label));
        return n;
    }
    /* progress "label" value N */
    if (parser_match(P, TK_PROGRESS)) {
        NcASTNode *n = alloc_node(NODE_PROGRESS);
        parser_read_string(P, n->stat_label, sizeof(n->stat_label));
        n->progress_val = P->cur.type == TK_NUMBER ? (parser_advance(P), P->prev.number) : 0;
        return n;
    }
    /* alert "text" [type/variant "warning"] [dismissible] */
    if (parser_match(P, TK_ALERT)) {
        NcASTNode *n = alloc_node(NODE_ALERT);
        parser_read_string(P, n->text, sizeof(n->text));
        if (parser_match(P, TK_TYPE) || parser_match(P, TK_STYLE))
            parser_read_string(P, n->variant, sizeof(n->variant));
        if (parser_match(P, TK_DISMISSIBLE))
            n->is_dismissible = true;
        return n;
    }
    /* loading ["text"] [style "spinner"/"skeleton"] */
    if (parser_match(P, TK_LOADING)) {
        NcASTNode *n = alloc_node(NODE_LOADING);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->text, sizeof(n->text));
        if (parser_match(P, TK_STYLE))
            parser_read_string(P, n->style_str, sizeof(n->style_str));
        return n;
    }
    /* input "Label" [type "email"] [bind field] [validate X] [placeholder "..."] [required] */
    if (parser_match(P, TK_INPUT)) {
        NcASTNode *n = alloc_node(NODE_INPUT);
        parser_read_string(P, n->text, sizeof(n->text)); /* label */
        while (true) {
            if (parser_match(P, TK_TYPE)) {
                parser_read_string(P, n->style_str, sizeof(n->style_str)); /* reuse for input type */
            } else if (parser_match(P, TK_BIND)) {
                parser_read_ident(P, n->bind_field, sizeof(n->bind_field));
            } else if (parser_match(P, TK_VALIDATE)) {
                parser_read_ident(P, n->validate_rules, sizeof(n->validate_rules));
                /* additional validate keywords */
                if (parser_match(P, TK_REQUIRED)) n->is_required = true;
            } else if (parser_match(P, TK_PLACEHOLDER)) {
                parser_read_string(P, n->placeholder, sizeof(n->placeholder));
            } else if (parser_match(P, TK_REQUIRED)) {
                n->is_required = true;
            } else {
                break;
            }
        }
        return n;
    }
    /* textarea "Label" [bind field] [placeholder "..."] [rows N] */
    if (parser_match(P, TK_TEXTAREA)) {
        NcASTNode *n = alloc_node(NODE_TEXTAREA);
        parser_read_string(P, n->text, sizeof(n->text));
        while (true) {
            if (parser_match(P, TK_BIND))
                parser_read_ident(P, n->bind_field, sizeof(n->bind_field));
            else if (parser_match(P, TK_PLACEHOLDER))
                parser_read_string(P, n->placeholder, sizeof(n->placeholder));
            else if (parser_match(P, TK_ROWS_KW) && P->cur.type == TK_NUMBER) {
                n->grid_cols = (int)P->cur.number;
                parser_advance(P);
            } else if (parser_match(P, TK_REQUIRED))
                n->is_required = true;
            else break;
        }
        return n;
    }
    /* animate "type" */
    if (parser_match(P, TK_ANIMATE)) {
        NcASTNode *n = alloc_node(NODE_ANIMATE);
        parser_read_string(P, n->animate_type, sizeof(n->animate_type));
        return n;
    }
    /* modal "trigger": children */
    if (parser_match(P, TK_MODAL)) {
        NcASTNode *n = alloc_node(NODE_MODAL);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->trigger_text, sizeof(n->trigger_text));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* tabs: tab "Label": children */
    if (parser_match(P, TK_TABS)) {
        NcASTNode *n = alloc_node(NODE_TABS);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_TAB)) {
        NcASTNode *n = alloc_node(NODE_TAB);
        parser_read_string(P, n->text, sizeof(n->text));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* form [action "url"] [method "POST"] [with auth]: children */
    if (parser_match(P, TK_FORM)) {
        NcASTNode *n = alloc_node(NODE_FORM);
        while (true) {
            if (parser_match(P, TK_ACTION)) {
                if (P->cur.type == TK_STRING)
                    parser_read_string(P, n->action_url, sizeof(n->action_url));
                else
                    parser_read_ident(P, n->action_url, sizeof(n->action_url));
            } else if (parser_match(P, TK_METHOD))
                parser_read_string(P, n->method_str, sizeof(n->method_str));
            else if (parser_match(P, TK_WITH) && parser_match(P, TK_AUTH))
                n->with_auth = true;
            else if (parser_match(P, TK_ON) && parser_match(P, TK_SUBMIT) && parser_match(P, TK_RUNS))
                parser_read_ident(P, n->action_name, sizeof(n->action_name));
            else break;
        }
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* card [icon "name"] [class "..."] [centered]: children */
    if (parser_match(P, TK_CARD)) {
        NcASTNode *n = alloc_node(NODE_CARD);
        while (true) {
            if (parser_match(P, TK_ICON))
                parser_read_string(P, n->icon_name, sizeof(n->icon_name));
            else if (parser_match(P, TK_CLASS))
                parser_read_string(P, n->class_str, sizeof(n->class_str));
            else if (parser_match(P, TK_CENTERED))
                n->is_centered = true;
            else break;
        }
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* grid N columns [responsive]: children */
    if (parser_match(P, TK_GRID)) {
        NcASTNode *n = alloc_node(NODE_GRID);
        if (P->cur.type == TK_NUMBER) {
            n->grid_cols = (int)P->cur.number;
            parser_advance(P);
        }
        parser_match(P, TK_COLUMNS); /* consume optional "columns" */
        if (parser_match(P, TK_RESPONSIVE)) n->is_responsive = true;
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* row [centered] [between]: children */
    if (parser_match(P, TK_ROW)) {
        NcASTNode *n = alloc_node(NODE_ROW);
        if (parser_match(P, TK_CENTERED)) n->is_centered = true;
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* column: children */
    if (parser_match(P, TK_COLUMN)) {
        NcASTNode *n = alloc_node(NODE_COLUMN);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* section [id/"id"] [hero] [centered] [fullscreen]: children */
    if (parser_match(P, TK_SECTION)) {
        NcASTNode *n = alloc_node(NODE_SECTION);
        /* optional section id — can be ident, string, or keyword used as id */
        if (P->cur.type == TK_STRING) {
            parser_read_string(P, n->id_str, sizeof(n->id_str));
        } else if (P->cur.type == TK_IDENT || P->cur.type == TK_HERO) {
            if (P->cur.type == TK_HERO) {
                n->is_hero = true;
                parser_advance(P);
            } else {
                parser_read_ident(P, n->id_str, sizeof(n->id_str));
            }
        }
        while (true) {
            if (parser_match(P, TK_CENTERED)) n->is_centered = true;
            else if (parser_match(P, TK_FULLSCREEN)) n->is_fullscreen = true;
            else if (parser_match(P, TK_HERO)) n->is_hero = true;
            else break;
        }
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* header: children — renders as a header/row container */
    if (parser_match(P, TK_HEADER)) {
        NcASTNode *n = alloc_node(NODE_HEADER);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->text, sizeof(n->text));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* nav: brand, links: children */
    if (parser_match(P, TK_NAV)) {
        NcASTNode *n = alloc_node(NODE_NAV);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_BRAND)) {
        NcASTNode *n = alloc_node(NODE_BRAND);
        parser_read_string(P, n->text, sizeof(n->text));
        return n;
    }
    if (parser_match(P, TK_LINKS)) {
        NcASTNode *n = alloc_node(NODE_LINKS);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* footer: children */
    if (parser_match(P, TK_FOOTER)) {
        NcASTNode *n = alloc_node(NODE_FOOTER);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* if condition: children [otherwise: children] */
    if (parser_match(P, TK_IF)) {
        NcASTNode *n = alloc_node(NODE_IF);
        /* read condition (could be dotted ident) */
        parser_read_ident(P, n->condition, sizeof(n->condition));
        while (parser_match(P, TK_DOT)) {
            size_t len = strlen(n->condition);
            n->condition[len] = '.';
            n->condition[len + 1] = '\0';
            char part[128];
            parser_read_ident(P, part, sizeof(part));
            strncat(n->condition, part, sizeof(n->condition) - strlen(n->condition) - 1);
        }
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        if (parser_match(P, TK_OTHERWISE)) {
            parse_children(P, n->else_children, &n->else_child_count, MAX_CHILDREN);
        }
        return n;
    }
    /* repeat [for each] item in collection: children */
    if (parser_match(P, TK_REPEAT)) {
        NcASTNode *n = alloc_node(NODE_REPEAT);
        parser_match(P, TK_FOR);
        parser_match(P, TK_EACH);
        parser_read_ident(P, n->item_var, sizeof(n->item_var));
        parser_match(P, TK_IN);
        parser_read_ident(P, n->collection, sizeof(n->collection));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* use ComponentName [with prop "val"...] */
    if (parser_match(P, TK_USE)) {
        NcASTNode *n = alloc_node(NODE_USE);
        parser_read_ident(P, n->component_name, sizeof(n->component_name));
        /* skip with ... props for now */
        while (parser_match(P, TK_WITH)) {
            parser_advance(P); /* prop name */
            if (P->cur.type == TK_STRING) parser_advance(P); /* prop value */
        }
        return n;
    }
    /* slot "name": children */
    if (parser_match(P, TK_SLOT)) {
        NcASTNode *n = alloc_node(NODE_SLOT);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->text, sizeof(n->text));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    /* shell: sidebar/topbar/etc children */
    if (parser_match(P, TK_SHELL)) {
        NcASTNode *n = alloc_node(NODE_SHELL);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_SIDEBAR)) {
        NcASTNode *n = alloc_node(NODE_SIDEBAR);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_TOPBAR)) {
        NcASTNode *n = alloc_node(NODE_TOPBAR);
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_PANEL)) {
        NcASTNode *n = alloc_node(NODE_PANEL);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->text, sizeof(n->text));
        parse_children(P, n->children, &n->child_count, MAX_CHILDREN);
        return n;
    }
    if (parser_match(P, TK_BANNER)) {
        NcASTNode *n = alloc_node(NODE_BANNER);
        if (P->cur.type == TK_STRING)
            parser_read_string(P, n->text, sizeof(n->text));
        if (parser_match(P, TK_TYPE) || parser_match(P, TK_STYLE))
            parser_read_string(P, n->variant, sizeof(n->variant));
        return n;
    }
    /* table: ... (simplified — tables usually need columns spec from data) */
    if (parser_match(P, TK_TABLE)) {
        NcASTNode *n = alloc_node(NODE_TABLE);
        parser_read_string(P, n->text, sizeof(n->text));
        return n;
    }

    /* Unknown / end of block — return NULL to signal end */
    return NULL;
}

/* ── Parse top-level declarations ─────────────────────────── */
static void parse_top_level(NcHTMLParser *P) {
    NcUIAST *ast = P->ast;

    while (!parser_check(P, TK_EOF) && !P->had_error) {
        /* app "Title" */
        if (parser_match(P, TK_PAGE) || parser_match(P, TK_APP)) {
            parser_read_string(P, ast->title, sizeof(ast->title));
            continue;
        }
        /* theme "dark" */
        if (parser_match(P, TK_THEME)) {
            char theme[64];
            parser_read_string(P, theme, sizeof(theme));
            strncpy(ast->theme, theme, sizeof(ast->theme) - 1);
            continue;
        }
        /* accent "#hex" */
        if (parser_match(P, TK_ACCENT)) {
            parser_read_string(P, ast->accent, sizeof(ast->accent));
            continue;
        }
        /* font "Name" */
        if (parser_match(P, TK_FONT)) {
            parser_read_string(P, ast->font, sizeof(ast->font));
            continue;
        }
        /* style: block — parse inline style properties */
        if (parser_match(P, TK_STYLE)) {
            if (parser_match(P, TK_COLON)) {
                /* Parse style properties: background is "..", text color is "..", accent is "..", font is ".." */
                int style_indent = P->prev.indent;
                while (!parser_check(P, TK_EOF)) {
                    if (P->cur.indent <= style_indent && P->cur.type != TK_EOF) break;
                    if (parser_match(P, TK_BACKGROUND)) {
                        parser_match(P, TK_IS);
                        if (P->cur.type == TK_STRING) {
                            parser_read_string(P, ast->bg, sizeof(ast->bg));
                        }
                    } else if (parser_match(P, TK_FOREGROUND)) {
                        parser_match(P, TK_IS);
                        if (P->cur.type == TK_STRING) {
                            char fg[64];
                            parser_read_string(P, fg, sizeof(fg));
                            strncpy(ast->fg, fg, sizeof(ast->fg) - 1);
                        }
                    } else if (parser_match(P, TK_TEXT)) {
                        /* "text color is ..." */
                        parser_advance(P); /* skip "color" ident */
                        parser_match(P, TK_IS);
                        if (P->cur.type == TK_STRING) {
                            char fg[64];
                            parser_read_string(P, fg, sizeof(fg));
                            strncpy(ast->fg, fg, sizeof(ast->fg) - 1);
                        }
                    } else if (parser_match(P, TK_ACCENT)) {
                        parser_match(P, TK_IS);
                        if (P->cur.type == TK_STRING) {
                            parser_read_string(P, ast->accent, sizeof(ast->accent));
                        }
                    } else if (parser_match(P, TK_FONT)) {
                        parser_match(P, TK_IS);
                        if (P->cur.type == TK_STRING) {
                            parser_read_string(P, ast->font, sizeof(ast->font));
                        }
                    } else {
                        /* skip unrecognized style property */
                        parser_advance(P);
                    }
                }
            }
            continue;
        }
        /* version "1.0" — skip */
        if (parser_match(P, TK_VERSION)) {
            if (P->cur.type == TK_STRING) parser_advance(P);
            continue;
        }
        /* state: ... */
        if (parser_match(P, TK_STATE)) {
            ast->has_state = true;
            if (parser_match(P, TK_COLON)) {
                /* multi-line state block */
            }
            while (P->cur.type == TK_IDENT) {
                NcStateSlot *s = &ast->states[ast->state_count];
                parser_read_ident(P, s->name, sizeof(s->name));
                if (parser_match(P, TK_IS)) {
                    if (P->cur.type == TK_STRING) {
                        parser_read_string(P, s->initial, sizeof(s->initial));
                        strncpy(s->type_hint, "string", sizeof(s->type_hint));
                    } else if (P->cur.type == TK_NUMBER) {
                        snprintf(s->initial, sizeof(s->initial), "%g", P->cur.number);
                        strncpy(s->type_hint, "number", sizeof(s->type_hint));
                        parser_advance(P);
                    } else if (P->cur.type == TK_TRUE) {
                        strncpy(s->initial, "true", sizeof(s->initial));
                        strncpy(s->type_hint, "boolean", sizeof(s->type_hint));
                        parser_advance(P);
                    } else if (P->cur.type == TK_FALSE) {
                        strncpy(s->initial, "false", sizeof(s->initial));
                        strncpy(s->type_hint, "boolean", sizeof(s->type_hint));
                        parser_advance(P);
                    } else {
                        /* [] or other — treat as array */
                        strncpy(s->initial, "[]", sizeof(s->initial));
                        strncpy(s->type_hint, "array", sizeof(s->type_hint));
                        /* skip tokens until next ident or keyword */
                        while (P->cur.type != TK_IDENT && P->cur.type != TK_EOF &&
                               P->cur.type < TK_APP) parser_advance(P);
                    }
                }
                /* skip "with type ..." */
                if (parser_match(P, TK_WITH)) {
                    parser_match(P, TK_TYPE);
                    if (P->cur.type == TK_STRING) parser_advance(P);
                }
                ast->state_count++;
                if (ast->state_count >= MAX_STATES) break;
            }
            continue;
        }
        /* action name [with param]: stmts */
        if (parser_match(P, TK_ACTION)) {
            NcAction *a = &ast->actions[ast->action_count];
            parser_read_ident(P, a->name, sizeof(a->name));
            if (parser_match(P, TK_WITH))
                parser_read_ident(P, a->param, sizeof(a->param));
            parser_match(P, TK_COLON);
            /* Parse action statements */
            while (a->stmt_count < MAX_STMTS) {
                NcActionStmt *st = &a->stmts[a->stmt_count];
                if (parser_match(P, TK_SET)) {
                    strncpy(st->stmt_type, "set", sizeof(st->stmt_type));
                    parser_read_ident(P, st->target, sizeof(st->target));
                    parser_match(P, TK_TO);
                    if (P->cur.type == TK_STRING)
                        parser_read_string(P, st->expr, sizeof(st->expr));
                    else
                        parser_read_ident(P, st->expr, sizeof(st->expr));
                    a->stmt_count++;
                } else if (parser_match(P, TK_ADD)) {
                    strncpy(st->stmt_type, "add", sizeof(st->stmt_type));
                    parser_read_ident(P, st->expr, sizeof(st->expr));
                    parser_match(P, TK_TO);
                    parser_read_ident(P, st->target, sizeof(st->target));
                    a->stmt_count++;
                } else if (parser_match(P, TK_REMOVE)) {
                    strncpy(st->stmt_type, "remove", sizeof(st->stmt_type));
                    parser_read_ident(P, st->expr, sizeof(st->expr));
                    parser_match(P, TK_FROM);
                    parser_read_ident(P, st->target, sizeof(st->target));
                    a->stmt_count++;
                } else if (parser_match(P, TK_TOGGLE)) {
                    strncpy(st->stmt_type, "toggle", sizeof(st->stmt_type));
                    parser_read_ident(P, st->target, sizeof(st->target));
                    a->stmt_count++;
                } else if (parser_match(P, TK_FETCH)) {
                    strncpy(st->stmt_type, "fetch", sizeof(st->stmt_type));
                    parser_read_string(P, st->expr, sizeof(st->expr));
                    if (parser_match(P, TK_WITH) && parser_match(P, TK_AUTH))
                        st->with_auth = true;
                    if (parser_match(P, TK_SAVE) && parser_match(P, TK_AS))
                        parser_read_ident(P, st->save_as, sizeof(st->save_as));
                    a->stmt_count++;
                } else if (parser_match(P, TK_REDIRECT) || parser_match(P, TK_NAVIGATE)) {
                    strncpy(st->stmt_type, "redirect", sizeof(st->stmt_type));
                    parser_match(P, TK_TO);
                    parser_read_string(P, st->expr, sizeof(st->expr));
                    a->stmt_count++;
                } else {
                    break;
                }
            }
            ast->action_count++;
            ast->has_actions = true;
            if (ast->action_count >= MAX_ACTIONS) break;
            continue;
        }
        /* component name [with params]: children */
        if (parser_match(P, TK_COMPONENT)) {
            NcComponent *comp = &ast->components[ast->comp_count];
            parser_read_ident(P, comp->name, sizeof(comp->name));
            if (parser_match(P, TK_WITH)) {
                /* read param list */
                parser_read_ident(P, comp->params, sizeof(comp->params));
                while (parser_match(P, TK_COMMA)) {
                    strncat(comp->params, ",", sizeof(comp->params) - strlen(comp->params) - 1);
                    char p[64];
                    parser_read_ident(P, p, sizeof(p));
                    strncat(comp->params, p, sizeof(comp->params) - strlen(comp->params) - 1);
                }
            }
            if (parser_match(P, TK_COLON)) {
                while (comp->child_count < MAX_CHILDREN) {
                    NcASTNode *child = parse_element(P);
                    if (!child) break;
                    comp->children[comp->child_count++] = child;
                }
            }
            ast->comp_count++;
            if (ast->comp_count >= MAX_COMPS) break;
            continue;
        }
        /* routes: ... */
        if (parser_match(P, TK_ROUTES)) {
            ast->has_routes = true;
            parser_match(P, TK_COLON);
            /* Parse route entries: "/" shows HomePage [publicly] */
            while (P->cur.type == TK_STRING && ast->route_count < MAX_ROUTES) {
                NcRoute *r = &ast->routes[ast->route_count];
                parser_read_string(P, r->path, sizeof(r->path));
                if (parser_match(P, TK_SHOWS))
                    parser_read_ident(P, r->component, sizeof(r->component));
                if (parser_match(P, TK_PUBLICLY))
                    r->is_public = true;
                if (parser_match(P, TK_REQUIRE) && parser_match(P, TK_AUTH))
                    r->require_auth = true;
                if (parser_match(P, TK_GUARD))
                    parser_read_string(P, r->guard_name, sizeof(r->guard_name));
                ast->route_count++;
            }
            continue;
        }
        /* auth ... — skip for now, record flag */
        if (parser_match(P, TK_AUTH)) {
            ast->has_auth = true;
            /* skip auth block details */
            while (P->cur.type != TK_EOF && P->cur.type != TK_NAV &&
                   P->cur.type != TK_SECTION && P->cur.type != TK_FOOTER &&
                   P->cur.type != TK_ACTION && P->cur.type != TK_COMPONENT &&
                   P->cur.type != TK_ROUTES && P->cur.type != TK_PAGE &&
                   P->cur.type != TK_SHELL)
                parser_advance(P);
            continue;
        }
        /* guard ... — skip for now */
        if (parser_match(P, TK_GUARD)) {
            while (P->cur.type != TK_EOF && P->cur.type != TK_NAV &&
                   P->cur.type != TK_SECTION && P->cur.type != TK_FOOTER &&
                   P->cur.type != TK_ACTION && P->cur.type != TK_COMPONENT &&
                   P->cur.type != TK_ROUTES && P->cur.type != TK_PAGE &&
                   P->cur.type != TK_SHELL)
                parser_advance(P);
            continue;
        }
        /* import ... from ... — skip */
        if (parser_match(P, TK_IMPORT)) {
            while (P->cur.type != TK_EOF && P->cur.type != TK_NAV &&
                   P->cur.type != TK_SECTION && P->cur.type != TK_FOOTER)
                parser_advance(P);
            continue;
        }
        /* UI elements at top level (nav, section, footer, shell, etc.) */
        NcASTNode *node = parse_element(P);
        if (node) {
            if (ast->body_count < MAX_CHILDREN)
                ast->body[ast->body_count++] = node;
        } else {
            /* Skip unrecognized tokens to avoid infinite loop */
            parser_advance(P);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  HTML Code Generator — AST → HTML/CSS/JS
 * ═══════════════════════════════════════════════════════════ */

/* SVG icons matching compiler.js ICONS */
static const char *get_icon_svg(const char *name) {
    if (!name || !name[0]) return "";
    if (strcmp(name, "check") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><polyline points=\"20 6 9 17 4 12\"/></svg>";
    if (strcmp(name, "star") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><polygon points=\"12 2 15 8.5 22 9.3 17 14 18.2 21 12 17.8 5.8 21 7 14 2 9.3 9 8.5 12 2\"/></svg>";
    if (strcmp(name, "users") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2\"/><circle cx=\"9\" cy=\"7\" r=\"4\"/><path d=\"M23 21v-2a4 4 0 0 0-3-3.87\"/><path d=\"M16 3.13a4 4 0 0 1 0 7.75\"/></svg>";
    if (strcmp(name, "chart") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><line x1=\"18\" y1=\"20\" x2=\"18\" y2=\"10\"/><line x1=\"12\" y1=\"20\" x2=\"12\" y2=\"4\"/><line x1=\"6\" y1=\"20\" x2=\"6\" y2=\"14\"/></svg>";
    if (strcmp(name, "clock") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><polyline points=\"12 6 12 12 16 14\"/></svg>";
    if (strcmp(name, "code") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><polyline points=\"16 18 22 12 16 6\"/><polyline points=\"8 6 2 12 8 18\"/></svg>";
    if (strcmp(name, "shield") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z\"/></svg>";
    if (strcmp(name, "zap") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><polygon points=\"13 2 3 14 12 14 11 22 21 10 12 10 13 2\"/></svg>";
    if (strcmp(name, "globe") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><circle cx=\"12\" cy=\"12\" r=\"10\"/><line x1=\"2\" y1=\"12\" x2=\"22\" y2=\"12\"/><path d=\"M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z\"/></svg>";
    if (strcmp(name, "menu") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><line x1=\"3\" y1=\"12\" x2=\"21\" y2=\"12\"/><line x1=\"3\" y1=\"6\" x2=\"21\" y2=\"6\"/><line x1=\"3\" y1=\"18\" x2=\"21\" y2=\"18\"/></svg>";
    if (strcmp(name, "rocket") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M4.5 16.5c-1.5 1.26-2 5-2 5s3.74-.5 5-2c.71-.84.7-2.13-.09-2.91a2.18 2.18 0 0 0-2.91-.09z\"/><path d=\"M12 15l-3-3a22 22 0 0 1 2-3.95A12.88 12.88 0 0 1 22 2c0 2.72-.78 7.5-6 11a22.35 22.35 0 0 1-4 2z\"/></svg>";
    if (strcmp(name, "heart") == 0) return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><path d=\"M20.84 4.61a5.5 5.5 0 0 0-7.78 0L12 5.67l-1.06-1.06a5.5 5.5 0 0 0-7.78 7.78l1.06 1.06L12 21.23l7.78-7.78 1.06-1.06a5.5 5.5 0 0 0 0-7.78z\"/></svg>";
    /* Fallback — empty SVG placeholder */
    return "<svg width=\"24\" height=\"24\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\"><circle cx=\"12\" cy=\"12\" r=\"10\"/></svg>";
}

/* HSL color shift helper */
static void hex_to_hsl(const char *hex, double *h, double *s, double *l) {
    int r_i = 0, g_i = 0, b_i = 0;
    if (hex && hex[0] == '#' && strlen(hex) >= 7) {
        sscanf(hex + 1, "%02x%02x%02x", &r_i, &g_i, &b_i);
    }
    double r = r_i / 255.0, g = g_i / 255.0, b = b_i / 255.0;
    double mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    double mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    *l = (mx + mn) / 2.0;
    if (mx == mn) { *h = *s = 0; return; }
    double d = mx - mn;
    *s = *l > 0.5 ? d / (2.0 - mx - mn) : d / (mx + mn);
    if (mx == r) *h = ((g - b) / d + (g < b ? 6.0 : 0.0)) / 6.0;
    else if (mx == g) *h = ((b - r) / d + 2.0) / 6.0;
    else *h = ((r - g) / d + 4.0) / 6.0;
}

static double hue2rgb(double p, double q, double t) {
    if (t < 0) t += 1; if (t > 1) t -= 1;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 0.5) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

static void shift_hue(const char *hex, int degrees, char *out, int outlen) {
    double h, s, l;
    hex_to_hsl(hex, &h, &s, &l);
    h = fmod(h * 360.0 + degrees, 360.0) / 360.0;
    double r, g, b;
    if (s == 0) { r = g = b = l; }
    else {
        double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        double p = 2.0 * l - q;
        r = hue2rgb(p, q, h + 1.0/3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0/3.0);
    }
    snprintf(out, (size_t)outlen, "#%02x%02x%02x",
             (int)(r * 255 + 0.5), (int)(g * 255 + 0.5), (int)(b * 255 + 0.5));
}

/* ── Render a single AST node to HTML ──────────────────── */
static void render_node(StrBuf *sb, const NcASTNode *n, const NcUIAST *ast);

static void render_children(StrBuf *sb, NcASTNode *const*children, int count, const NcUIAST *ast) {
    for (int i = 0; i < count; i++) {
        render_node(sb, children[i], ast);
    }
}

static void render_node(StrBuf *sb, const NcASTNode *n, const NcUIAST *ast) {
    if (!n) return;

    switch (n->type) {
    case NODE_HEADING: {
        bool gradient = n->style_str[0] && strcmp(n->style_str, "gradient") == 0;
        int level = (n->grid_cols >= 1 && n->grid_cols <= 6) ? n->grid_cols : 1;
        sb_printf(sb, "<h%d class=\"ncui-heading%s\">", level, gradient ? " ncui-gradient-text" : "");
        sb_append_escaped(sb, n->text);
        sb_printf(sb, "</h%d>\n", level);
        break;
    }
    case NODE_SUBHEADING:
        sb_append(sb, "<p class=\"ncui-sub\">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</p>\n");
        break;
    case NODE_TEXT: {
        bool has_interp = strstr(n->text, "{{") != NULL;
        if (has_interp) {
            sb_printf(sb, "<p class=\"ncui-text\" data-ncui-text=\"%s\">", n->text);
            sb_append_escaped(sb, n->text);
        } else if (strchr(n->text, '.')) {
            /* state.field reference */
            sb_printf(sb, "<p class=\"ncui-text\" data-ncui-text=\"{{%s}}\">", n->text);
        } else {
            sb_append(sb, "<p class=\"ncui-text\">");
            sb_append_escaped(sb, n->text);
        }
        sb_append(sb, "</p>\n");
        break;
    }
    case NODE_BADGE:
        sb_append(sb, "<span class=\"ncui-badge\">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</span>\n");
        break;
    case NODE_BUTTON: {
        /* Determine CSS classes */
        char cls[256] = "ncui-btn";
        if (n->style_str[0]) {
            strcat(cls, " ncui-btn-");
            strncat(cls, n->style_str, sizeof(cls) - strlen(cls) - 1);
        } else {
            strcat(cls, " ncui-btn-primary");
        }
        if (n->is_full_width) strcat(cls, " ncui-btn-full");

        sb_printf(sb, "<button class=\"%s\"", cls);
        if (n->action_name[0])
            sb_printf(sb, " data-ncui-click=\"%s\"", n->action_name);
        if (n->nav_to[0])
            sb_printf(sb, " data-ncui-link=\"%s\"", n->nav_to);
        if (n->is_disabled)
            sb_append(sb, " disabled");
        sb_append(sb, ">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</button>\n");
        break;
    }
    case NODE_LINK:
        if (n->nav_to[0] || n->href[0]) {
            const char *h = n->href[0] ? n->href : n->nav_to;
            sb_printf(sb, "<a class=\"ncui-link\" href=\"%s\"", h);
            /* If starts with / or #, make it a client-side route link */
            if (h[0] == '/' && ast->has_routes)
                sb_printf(sb, " data-ncui-link=\"%s\"", h);
            sb_append(sb, ">");
        } else {
            sb_append(sb, "<a class=\"ncui-link\" href=\"#\">");
        }
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</a>\n");
        break;
    case NODE_IMAGE:
        sb_printf(sb, "<img class=\"ncui-img%s%s\" src=\"",
                  n->class_str[0] ? " " : "",
                  n->class_str[0] ? n->class_str : "");
        sb_append_escaped(sb, n->href);
        sb_append(sb, "\" alt=\"\" loading=\"lazy\">\n");
        break;
    case NODE_VIDEO:
        sb_printf(sb, "<video class=\"ncui-video\" src=\"");
        sb_append_escaped(sb, n->href);
        sb_append(sb, "\" controls></video>\n");
        break;
    case NODE_DIVIDER:
        sb_append(sb, "<hr class=\"ncui-divider\">\n");
        break;
    case NODE_SPACER:
        sb_append(sb, "<div class=\"ncui-spacer\"></div>\n");
        break;
    case NODE_ICON:
        sb_append(sb, get_icon_svg(n->icon_name));
        sb_append(sb, "\n");
        break;
    case NODE_STAT:
        sb_append(sb, "<div class=\"ncui-stat\"><span class=\"ncui-stat-val\">");
        sb_append_escaped(sb, n->stat_value);
        sb_append(sb, "</span><span class=\"ncui-stat-label\">");
        sb_append_escaped(sb, n->stat_label);
        sb_append(sb, "</span></div>\n");
        break;
    case NODE_PROGRESS: {
        int val = (int)n->progress_val;
        if (val < 0) val = 0; if (val > 100) val = 100;
        sb_printf(sb, "<div class=\"ncui-progress\"><span class=\"ncui-progress-label\">");
        sb_append_escaped(sb, n->stat_label);
        sb_printf(sb, "</span><div class=\"ncui-progress-bar\"><div class=\"ncui-progress-fill ncui-progress-fill-%d\" data-ncui-progress=\"%d\"></div></div></div>\n", val, val);
        break;
    }
    case NODE_ALERT: {
        const char *var = n->variant[0] ? n->variant : "info";
        sb_printf(sb, "<div class=\"ncui-alert ncui-alert-%s\"", var);
        if (n->is_dismissible) sb_append(sb, " data-ncui-dismissible=\"true\"");
        sb_append(sb, "><div class=\"ncui-alert-body\"><p class=\"ncui-alert-text\">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</p></div>");
        if (n->is_dismissible) sb_append(sb, "<button class=\"ncui-alert-close\">&times;</button>");
        sb_append(sb, "</div>\n");
        break;
    }
    case NODE_LOADING: {
        bool skeleton = n->style_str[0] && strcmp(n->style_str, "skeleton") == 0;
        if (skeleton) {
            sb_append(sb, "<div class=\"ncui-loading ncui-loading-skeleton\">");
            sb_append(sb, "<div class=\"ncui-skeleton-line ncui-skeleton-line-1\"></div>");
            sb_append(sb, "<div class=\"ncui-skeleton-line ncui-skeleton-line-2\"></div>");
            sb_append(sb, "<div class=\"ncui-skeleton-line ncui-skeleton-line-3\"></div>");
            sb_append(sb, "</div>\n");
        } else {
            sb_append(sb, "<div class=\"ncui-loading ncui-loading-spinner\">");
            sb_append(sb, "<div class=\"ncui-spinner\" aria-hidden=\"true\"></div>");
            if (n->text[0]) {
                sb_append(sb, "<p class=\"ncui-loading-text\">");
                sb_append_escaped(sb, n->text);
                sb_append(sb, "</p>");
            }
            sb_append(sb, "</div>\n");
        }
        break;
    }
    case NODE_INPUT: {
        char id[128];
        snprintf(id, sizeof(id), "f-%s", n->text);
        /* lowercase and replace spaces */
        for (char *p = id; *p; p++) { if (*p == ' ') *p = '-'; *p = (char)tolower((unsigned char)*p); }
        const char *itype = n->style_str[0] ? n->style_str : "text";
        sb_append(sb, "<div class=\"ncui-field\">");
        sb_printf(sb, "<label for=\"%s\">", id);
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</label>");
        sb_printf(sb, "<input id=\"%s\" type=\"%s\"", id, itype);
        if (n->bind_field[0])
            sb_printf(sb, " data-ncui-bind=\"%s\" data-ncui-field=\"%s\" name=\"%s\"",
                      n->bind_field, n->bind_field, n->bind_field);
        else {
            /* derive name from label */
            char fname[128];
            snprintf(fname, sizeof(fname), "%s", n->text);
            for (char *p = fname; *p; p++) { if (*p == ' ') *p = '_'; *p = (char)tolower((unsigned char)*p); }
            sb_printf(sb, " data-ncui-field=\"%s\" name=\"%s\"", fname, fname);
        }
        if (n->placeholder[0])
            sb_printf(sb, " placeholder=\"%s\"", n->placeholder);
        else
            sb_printf(sb, " placeholder=\"%s\"", n->text);
        if (n->is_required) sb_append(sb, " required");
        sb_printf(sb, " aria-describedby=\"%s-error\"", id);
        sb_append(sb, ">");
        sb_printf(sb, "<div id=\"%s-error\" class=\"ncui-field-error\"></div>", id);
        sb_append(sb, "</div>\n");
        break;
    }
    case NODE_TEXTAREA: {
        char id[128];
        snprintf(id, sizeof(id), "f-%s", n->text);
        for (char *p = id; *p; p++) { if (*p == ' ') *p = '-'; *p = (char)tolower((unsigned char)*p); }
        int rows = n->grid_cols > 0 ? n->grid_cols : 4;
        sb_append(sb, "<div class=\"ncui-field\">");
        sb_printf(sb, "<label for=\"%s\">", id);
        sb_append_escaped(sb, n->text);
        sb_printf(sb, "</label><textarea id=\"%s\" rows=\"%d\"", id, rows);
        if (n->bind_field[0])
            sb_printf(sb, " data-ncui-bind=\"%s\" name=\"%s\"", n->bind_field, n->bind_field);
        if (n->placeholder[0])
            sb_printf(sb, " placeholder=\"%s\"", n->placeholder);
        if (n->is_required) sb_append(sb, " required");
        sb_append(sb, "></textarea></div>\n");
        break;
    }
    case NODE_FORM: {
        sb_append(sb, "<form class=\"ncui-form\"");
        if (n->action_name[0])
            sb_printf(sb, " data-ncui-submit=\"%s\"", n->action_name);
        if (n->action_url[0])
            sb_printf(sb, " data-ncui-action-url=\"%s\" action=\"%s\"", n->action_url, n->action_url);
        if (n->method_str[0])
            sb_printf(sb, " method=\"%s\"", n->method_str);
        if (n->with_auth)
            sb_append(sb, " data-ncui-auth=\"true\"");
        sb_append(sb, ">\n");
        sb_append(sb, "<div class=\"ncui-form-status\" data-ncui-form-status></div>\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</form>\n");
        break;
    }
    case NODE_CARD: {
        sb_append(sb, "<div class=\"ncui-card");
        if (n->class_str[0]) sb_printf(sb, " %s", n->class_str);
        sb_append(sb, "\">\n");
        if (n->icon_name[0]) {
            sb_append(sb, "<div class=\"ncui-card-icon\">");
            sb_append(sb, get_icon_svg(n->icon_name));
            sb_append(sb, "</div>\n");
        }
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    }
    case NODE_GRID: {
        int cols = n->grid_cols > 0 ? n->grid_cols : 3;
        sb_printf(sb, "<div class=\"ncui-grid ncui-grid-%d\">\n", cols);
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    }
    case NODE_ROW:
        sb_append(sb, "<div class=\"ncui-row\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    case NODE_COLUMN:
        sb_append(sb, "<div>\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    case NODE_SECTION: {
        sb_append(sb, "<section");
        if (n->id_str[0]) sb_printf(sb, " id=\"%s\"", n->id_str);
        sb_append(sb, " class=\"ncui-section");
        if (n->is_hero) sb_append(sb, " ncui-hero");
        if (n->is_centered) sb_append(sb, " ncui-centered");
        if (n->is_fullscreen) sb_append(sb, " ncui-fullscreen");
        /* Check for animate child */
        const char *anim = NULL;
        for (int i = 0; i < n->child_count; i++) {
            if (n->children[i] && n->children[i]->type == NODE_ANIMATE) {
                anim = n->children[i]->animate_type;
                break;
            }
        }
        if (anim && anim[0]) sb_printf(sb, " ncui-anim-%s", anim);
        sb_append(sb, "\">\n<div class=\"ncui-container\">\n");
        for (int i = 0; i < n->child_count; i++) {
            if (n->children[i] && n->children[i]->type != NODE_ANIMATE)
                render_node(sb, n->children[i], ast);
        }
        sb_append(sb, "</div>\n</section>\n");
        break;
    }
    case NODE_HEADER: {
        sb_append(sb, "<header class=\"ncui-section\">\n<div class=\"ncui-container\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n</header>\n");
        break;
    }
    case NODE_NAV: {
        sb_append(sb, "<nav class=\"ncui-nav\"><div class=\"ncui-container ncui-nav-inner\">");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "<button class=\"ncui-nav-toggle\" data-ncui-nav-toggle=\"true\">");
        sb_append(sb, get_icon_svg("menu"));
        sb_append(sb, "</button></div></nav>\n");
        break;
    }
    case NODE_BRAND:
        sb_append(sb, "<a class=\"ncui-brand\" href=\"/\">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</a>");
        break;
    case NODE_LINKS:
        sb_append(sb, "<div class=\"ncui-nav-links\">");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>");
        break;
    case NODE_FOOTER:
        sb_append(sb, "<footer class=\"ncui-footer\"><div class=\"ncui-container\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div></footer>\n");
        break;
    case NODE_MODAL: {
        static int modal_id = 0;
        char mid[32]; snprintf(mid, sizeof(mid), "ncui-modal-%d", modal_id++);
        const char *trig = n->trigger_text[0] ? n->trigger_text : "Open";
        sb_printf(sb, "<button class=\"ncui-btn ncui-btn-primary\" data-ncui-modal-open=\"%s\">", mid);
        sb_append_escaped(sb, trig);
        sb_append(sb, "</button>\n");
        sb_printf(sb, "<div id=\"%s\" class=\"ncui-modal\" data-ncui-modal>", mid);
        sb_append(sb, "<div class=\"ncui-modal-content\">");
        sb_append(sb, "<button class=\"ncui-modal-close\" data-ncui-modal-close=\"true\">&times;</button>");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div></div>\n");
        break;
    }
    case NODE_TABS: {
        static int tab_group = 0;
        char gid[32]; snprintf(gid, sizeof(gid), "ncui-tabs-%d", tab_group++);
        sb_printf(sb, "<div class=\"ncui-tabs\" id=\"%s\">", gid);
        /* header */
        sb_append(sb, "<div class=\"ncui-tabs-header\">");
        for (int i = 0; i < n->child_count; i++) {
            if (n->children[i] && n->children[i]->type == NODE_TAB) {
                sb_printf(sb, "<button class=\"ncui-tab-btn%s\" data-ncui-tab-btn=\"%s\" data-ncui-tab-index=\"%d\">",
                          i == 0 ? " ncui-tab-active" : "", gid, i);
                sb_append_escaped(sb, n->children[i]->text);
                sb_append(sb, "</button>");
            }
        }
        sb_append(sb, "</div>");
        /* panels */
        for (int i = 0; i < n->child_count; i++) {
            if (n->children[i] && n->children[i]->type == NODE_TAB) {
                sb_printf(sb, "<div class=\"ncui-tab-panel%s\" data-tab-group=\"%s\" data-tab-index=\"%d\">",
                          i == 0 ? "" : " ncui-tab-panel-hidden", gid, i);
                render_children(sb, n->children[i]->children, n->children[i]->child_count, ast);
                sb_append(sb, "</div>");
            }
        }
        sb_append(sb, "</div>\n");
        break;
    }
    case NODE_IF:
        if (ast->has_state) {
            sb_printf(sb, "<div data-ncui-if=\"%s\">", n->condition);
            render_children(sb, n->children, n->child_count, ast);
            sb_append(sb, "</div>");
            if (n->else_child_count > 0) {
                sb_printf(sb, "<div data-ncui-if=\"!%s\">", n->condition);
                render_children(sb, n->else_children, n->else_child_count, ast);
                sb_append(sb, "</div>");
            }
        } else {
            render_children(sb, n->children, n->child_count, ast);
        }
        sb_append(sb, "\n");
        break;
    case NODE_REPEAT:
        if (ast->has_state) {
            sb_printf(sb, "<template id=\"ncui-tpl-%s\">", n->collection);
            render_children(sb, n->children, n->child_count, ast);
            sb_printf(sb, "</template><div data-ncui-repeat=\"%s\" data-ncui-repeat-tpl=\"ncui-tpl-%s\"></div>\n",
                      n->collection, n->collection);
        } else {
            render_children(sb, n->children, n->child_count, ast);
        }
        break;
    case NODE_USE: {
        /* Find component and render its children inline */
        for (int i = 0; i < ast->comp_count; i++) {
            if (strcmp(ast->components[i].name, n->component_name) == 0) {
                render_children(sb, ast->components[i].children, ast->components[i].child_count, ast);
                break;
            }
        }
        break;
    }
    case NODE_SLOT:
        render_children(sb, n->children, n->child_count, ast);
        break;
    case NODE_ANIMATE:
        /* handled by parent section */
        break;
    case NODE_SHELL:
        sb_append(sb, "<div class=\"ncui-shell\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    case NODE_SIDEBAR:
        sb_append(sb, "<aside class=\"ncui-sidebar\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</aside>\n");
        break;
    case NODE_TOPBAR:
        sb_append(sb, "<div class=\"ncui-topbar\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div>\n");
        break;
    case NODE_PANEL:
        sb_append(sb, "<div class=\"ncui-panel\">\n");
        if (n->text[0]) {
            sb_append(sb, "<div class=\"ncui-panel-title\">");
            sb_append_escaped(sb, n->text);
            sb_append(sb, "</div>\n");
        }
        sb_append(sb, "<div class=\"ncui-panel-body\">");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</div></div>\n");
        break;
    case NODE_BANNER: {
        const char *var = n->variant[0] ? n->variant : "info";
        sb_printf(sb, "<div class=\"ncui-banner ncui-banner-%s\"><p class=\"ncui-banner-text\">", var);
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</p></div>\n");
        break;
    }
    case NODE_TABLE:
        sb_append(sb, "<div class=\"ncui-table-wrap\"><table class=\"ncui-table\">");
        if (n->text[0])
            sb_printf(sb, "<caption>%s</caption>", n->text);
        sb_append(sb, "<tbody></tbody></table></div>\n");
        break;
    case NODE_LIST:
        sb_append(sb, "<ul class=\"ncui-list\">\n");
        render_children(sb, n->children, n->child_count, ast);
        sb_append(sb, "</ul>\n");
        break;
    case NODE_ITEM:
        sb_append(sb, "<li class=\"ncui-list-item\">");
        sb_append_escaped(sb, n->text);
        sb_append(sb, "</li>\n");
        break;
    case NODE_TAB:
        /* Rendered by parent tabs */
        break;
    }
}

/* ── Generate CSS string ────────────────────────────────── */
static void generate_css(StrBuf *sb, const NcUIAST *ast) {
    bool is_dark = strcmp(ast->theme, "dark") == 0;
    const char *accent = ast->accent[0] ? ast->accent : "#6366f1";
    char accent_shift[16], accent40[16], accent120[16];
    shift_hue(accent, 60, accent_shift, sizeof(accent_shift));
    shift_hue(accent, 40, accent40, sizeof(accent40));
    shift_hue(accent, 120, accent120, sizeof(accent120));

    const char *bg       = ast->bg[0] ? ast->bg : (is_dark ? "#0a0a0f" : "#fafafa");
    const char *bg_card  = is_dark ? "rgba(255,255,255,0.04)" : "rgba(0,0,0,0.03)";
    const char *bg_card_h= is_dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.06)";
    const char *border   = is_dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.08)";
    const char *text_m   = ast->fg[0] ? ast->fg : (is_dark ? "#f0f0f5" : "#1a1a2e");
    const char *text_s   = is_dark ? "rgba(255,255,255,0.65)" : "rgba(0,0,0,0.6)";
    const char *text_mut = is_dark ? "rgba(255,255,255,0.4)" : "rgba(0,0,0,0.4)";
    const char *input_bg = is_dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.04)";
    const char *font = ast->font[0] ? ast->font : "Inter";

    sb_printf(sb,
        ":root{--ncui-accent:%s;--ncui-accent-shift:%s;--ncui-bg:%s;--ncui-bg-card:%s;"
        "--ncui-bg-card-hover:%s;--ncui-border:%s;--ncui-text:%s;--ncui-text-sub:%s;"
        "--ncui-text-muted:%s;--ncui-input-bg:%s;--ncui-font:'%s',system-ui,sans-serif}\n",
        accent, accent_shift, bg, bg_card, bg_card_h, border, text_m, text_s, text_mut, input_bg, font);

    sb_append(sb,
        "*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}\n"
        "html{scroll-behavior:smooth;-webkit-font-smoothing:antialiased}\n"
        "body{font-family:var(--ncui-font);background:var(--ncui-bg);color:var(--ncui-text);line-height:1.7;overflow-x:hidden}\n"
        ".ncui-container{max-width:1200px;margin:0 auto;padding:0 24px}\n"
    );

    /* Navigation */
    sb_printf(sb,
        ".ncui-nav{position:fixed;top:0;left:0;right:0;z-index:1000;padding:16px 0;transition:all .3s;"
        "backdrop-filter:blur(20px);background:%s;border-bottom:1px solid var(--ncui-border)}\n"
        ".ncui-nav.ncui-nav-compact{padding:10px 0}\n"
        ".ncui-nav-inner{display:flex;align-items:center;justify-content:space-between}\n"
        ".ncui-brand{font-weight:700;font-size:1.25rem;color:var(--ncui-text);text-decoration:none;letter-spacing:-0.02em}\n"
        ".ncui-nav-links{display:flex;gap:32px;align-items:center}\n"
        ".ncui-nav-links .ncui-link{font-size:.9rem;font-weight:500}\n"
        ".ncui-nav-toggle{display:none;background:none;border:none;color:var(--ncui-text);cursor:pointer;width:28px;height:28px}\n",
        is_dark ? "rgba(10,10,15,0.8)" : "rgba(250,250,250,0.85)");
    sb_printf(sb,
        "@media(max-width:768px){\n"
        ".ncui-nav-links{display:none;position:absolute;top:100%%;left:0;right:0;flex-direction:column;padding:24px;gap:16px;"
        "background:%s;border-bottom:1px solid var(--ncui-border);backdrop-filter:blur(20px)}\n"
        ".ncui-nav-inner.open .ncui-nav-links{display:flex}\n"
        ".ncui-nav-toggle{display:block}}\n",
        is_dark ? "rgba(10,10,15,0.95)" : "rgba(250,250,250,0.95)");

    /* Section */
    sb_append(sb,
        ".ncui-section{padding:100px 0;position:relative}\n"
        ".ncui-hero{min-height:100vh;display:flex;align-items:center;padding-top:80px}\n"
        ".ncui-hero .ncui-container{width:100%}\n"
        ".ncui-centered{text-align:center}\n"
        ".ncui-centered .ncui-row{justify-content:center}\n"
        ".ncui-fullscreen{min-height:100vh;display:flex;align-items:center}\n"
    );

    /* Typography */
    sb_append(sb,
        ".ncui-heading{font-size:clamp(2rem,5vw,3.5rem);font-weight:800;letter-spacing:-0.03em;line-height:1.15;margin-bottom:16px}\n"
        ".ncui-gradient-text{background:linear-gradient(135deg,var(--ncui-accent),var(--ncui-accent-shift));"
        "-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}\n"
        ".ncui-sub{font-size:clamp(1.1rem,2.5vw,1.5rem);color:var(--ncui-text-sub);font-weight:400;margin-bottom:12px}\n"
        ".ncui-text{color:var(--ncui-text-sub);font-size:1.05rem;max-width:640px;margin-bottom:16px;line-height:1.8}\n"
        ".ncui-centered .ncui-text{margin-left:auto;margin-right:auto}\n"
    );

    /* Buttons */
    sb_printf(sb,
        ".ncui-btn{display:inline-flex;align-items:center;gap:8px;padding:14px 32px;border-radius:12px;"
        "font-size:.95rem;font-weight:600;text-decoration:none;border:none;cursor:pointer;"
        "transition:all .3s cubic-bezier(.4,0,.2,1);font-family:inherit}\n"
        ".ncui-btn-primary{background:linear-gradient(135deg,var(--ncui-accent),%s);color:#fff;box-shadow:0 4px 24px %s44}\n"
        ".ncui-btn-primary:hover{transform:translateY(-2px);box-shadow:0 8px 32px %s66}\n"
        ".ncui-btn-outline{background:transparent;color:var(--ncui-accent);border:1.5px solid var(--ncui-accent)}\n"
        ".ncui-btn-outline:hover{background:%s18;transform:translateY(-2px)}\n"
        ".ncui-btn-glass{background:%s;color:var(--ncui-text);border:1px solid var(--ncui-border);backdrop-filter:blur(12px)}\n"
        ".ncui-btn-gradient{background:linear-gradient(135deg,var(--ncui-accent),%s);color:#fff}\n",
        accent40, accent, accent, accent,
        is_dark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.05)", accent120);

    /* Grid, Row, Card */
    sb_append(sb,
        ".ncui-row{display:flex;flex-wrap:wrap;gap:16px;margin:24px 0;align-items:center}\n"
        ".ncui-grid{display:grid;gap:24px;margin:40px 0}\n"
        ".ncui-grid-1{grid-template-columns:repeat(1,1fr)}\n"
        ".ncui-grid-2{grid-template-columns:repeat(2,1fr)}\n"
        ".ncui-grid-3{grid-template-columns:repeat(3,1fr)}\n"
        ".ncui-grid-4{grid-template-columns:repeat(4,1fr)}\n"
        "@media(max-width:900px){.ncui-grid-3,.ncui-grid-4{grid-template-columns:repeat(2,1fr)}}\n"
        "@media(max-width:600px){.ncui-grid{grid-template-columns:1fr !important}}\n"
    );
    sb_printf(sb,
        ".ncui-card{background:var(--ncui-bg-card);border:1px solid var(--ncui-border);border-radius:20px;"
        "padding:36px 28px;transition:all .4s cubic-bezier(.4,0,.2,1);position:relative;overflow:hidden}\n"
        ".ncui-card::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;"
        "background:linear-gradient(90deg,transparent,var(--ncui-accent),transparent);opacity:0;transition:opacity .4s}\n"
        ".ncui-card:hover{transform:translateY(-6px);background:var(--ncui-bg-card-hover);border-color:%s33;"
        "box-shadow:0 20px 60px %s}\n"
        ".ncui-card:hover::before{opacity:1}\n"
        ".ncui-card-icon{width:52px;height:52px;border-radius:14px;background:linear-gradient(135deg,%s22,%s08);"
        "display:flex;align-items:center;justify-content:center;margin-bottom:20px;color:var(--ncui-accent)}\n"
        ".ncui-card .ncui-heading{font-size:1.25rem;margin-bottom:8px}\n.ncui-card .ncui-text{font-size:.95rem;margin-bottom:0}\n",
        accent, is_dark ? "rgba(0,0,0,0.4)" : "rgba(0,0,0,0.08)", accent, accent);

    /* Links, Lists */
    sb_append(sb,
        ".ncui-link{color:var(--ncui-text-sub);text-decoration:none;transition:color .2s;font-weight:500}\n"
        ".ncui-link:hover{color:var(--ncui-accent)}\n"
        ".ncui-list{list-style:none;margin:20px 0}\n"
        ".ncui-list-item{display:flex;align-items:center;gap:12px;padding:10px 0;color:var(--ncui-text-sub);font-size:1.05rem}\n"
    );

    /* Badge */
    sb_printf(sb,
        ".ncui-badge{display:inline-block;padding:6px 16px;border-radius:100px;font-size:.8rem;font-weight:600;"
        "background:%s18;color:var(--ncui-accent);letter-spacing:0.03em}\n", accent);

    /* Stats */
    sb_append(sb,
        ".ncui-stat{text-align:center;padding:20px}\n"
        ".ncui-stat-val{display:block;font-size:2.5rem;font-weight:800;letter-spacing:-0.03em;"
        "background:linear-gradient(135deg,var(--ncui-accent),var(--ncui-accent-shift));"
        "-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}\n"
        ".ncui-stat-label{display:block;font-size:.9rem;color:var(--ncui-text-muted);margin-top:4px;font-weight:500}\n"
    );

    /* Forms */
    sb_printf(sb,
        ".ncui-form{max-width:520px;margin:32px 0;display:flex;flex-direction:column;gap:20px}\n"
        ".ncui-centered .ncui-form{margin-left:auto;margin-right:auto}\n"
        ".ncui-form-status{min-height:20px;font-size:.92rem;color:var(--ncui-text-sub)}\n"
        ".ncui-field{display:flex;flex-direction:column;gap:6px}\n"
        ".ncui-field label{font-size:.85rem;font-weight:600;color:var(--ncui-text-sub);text-transform:uppercase;letter-spacing:0.05em}\n"
        ".ncui-field input,.ncui-field textarea{background:var(--ncui-input-bg);border:1.5px solid var(--ncui-border);"
        "border-radius:12px;padding:14px 18px;color:var(--ncui-text);font-size:1rem;font-family:inherit;"
        "transition:all .3s;outline:none}\n"
        ".ncui-field input:focus,.ncui-field textarea:focus{border-color:var(--ncui-accent);box-shadow:0 0 0 3px %s22}\n"
        ".ncui-field input::placeholder,.ncui-field textarea::placeholder{color:var(--ncui-text-muted)}\n"
        ".ncui-field-error{min-height:18px;font-size:.82rem;color:#f87171}\n"
        ".ncui-form .ncui-btn{align-self:flex-start;margin-top:8px}\n", accent);

    /* Alerts */
    sb_printf(sb,
        ".ncui-alert{display:flex;align-items:flex-start;gap:12px;padding:16px 18px;border-radius:16px;"
        "border:1px solid var(--ncui-border);margin:18px 0;position:relative}\n"
        ".ncui-alert-info{border-color:%s44;background:%s14}\n"
        ".ncui-alert-success{border-color:rgba(74,222,128,0.42);background:rgba(74,222,128,0.12)}\n"
        ".ncui-alert-warning{border-color:rgba(251,191,36,0.42);background:rgba(251,191,36,0.12)}\n"
        ".ncui-alert-error{border-color:rgba(248,113,113,0.42);background:rgba(248,113,113,0.12)}\n"
        ".ncui-alert-body{flex:1}\n"
        ".ncui-alert-text{margin:0;color:var(--ncui-text-sub);max-width:none}\n"
        ".ncui-alert-close{background:none;border:none;color:var(--ncui-text-muted);font-size:1.25rem;cursor:pointer}\n",
        accent, accent);

    /* Loading */
    sb_printf(sb,
        ".ncui-loading{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;padding:18px 0}\n"
        ".ncui-loading-spinner{min-height:80px}\n"
        ".ncui-spinner{width:32px;height:32px;border-radius:50%%;"
        "border:3px solid %s;border-top-color:var(--ncui-accent);animation:ncui-spin .85s linear infinite}\n"
        ".ncui-loading-text{margin:0;color:var(--ncui-text-sub);font-size:.95rem}\n"
        ".ncui-loading-skeleton{align-items:stretch}\n"
        ".ncui-skeleton-line{height:14px;border-radius:999px;background:linear-gradient(90deg,%s,%s,%s);"
        "background-size:200%% 100%%;animation:ncui-shimmer 1.4s ease-in-out infinite}\n"
        ".ncui-skeleton-line-1{width:100%%}.ncui-skeleton-line-2{width:82%%}.ncui-skeleton-line-3{width:66%%}\n",
        is_dark ? "rgba(255,255,255,0.14)" : "rgba(0,0,0,0.12)",
        is_dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.05)",
        is_dark ? "rgba(255,255,255,0.14)" : "rgba(0,0,0,0.1)",
        is_dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.05)");

    /* Progress */
    sb_append(sb,
        ".ncui-progress{margin:12px 0}\n"
        ".ncui-progress-label{font-size:.85rem;font-weight:600;color:var(--ncui-text-sub);margin-bottom:6px;display:block}\n"
        ".ncui-progress-bar{height:8px;border-radius:4px;background:var(--ncui-input-bg);overflow:hidden}\n"
        ".ncui-progress-fill{height:100%;border-radius:4px;"
        "background:linear-gradient(90deg,var(--ncui-accent),var(--ncui-accent-shift));"
        "transition:width 1s cubic-bezier(.4,0,.2,1)}\n"
    );
    for (int i = 0; i <= 100; i++)
        sb_printf(sb, ".ncui-progress-fill-%d{width:%d%%}\n", i, i);

    /* Modals, Tabs */
    sb_append(sb,
        ".ncui-modal{display:none;position:fixed;inset:0;z-index:2000;background:rgba(0,0,0,0.6);"
        "backdrop-filter:blur(8px);justify-content:center;align-items:center}\n"
        ".ncui-modal.ncui-modal-open{display:flex}\n"
        ".ncui-modal-content{background:var(--ncui-bg);border:1px solid var(--ncui-border);border-radius:20px;"
        "padding:40px;max-width:560px;width:90%;position:relative;max-height:80vh;overflow-y:auto}\n"
        ".ncui-modal-close{position:absolute;top:16px;right:16px;background:none;border:none;"
        "color:var(--ncui-text-sub);font-size:1.5rem;cursor:pointer}\n"
        ".ncui-tabs{margin:24px 0}\n"
        ".ncui-tabs-header{display:flex;gap:4px;border-bottom:1px solid var(--ncui-border);margin-bottom:20px}\n"
        ".ncui-tab-btn{background:none;border:none;padding:12px 24px;color:var(--ncui-text-sub);"
        "font-size:.95rem;font-weight:600;cursor:pointer;border-bottom:2px solid transparent;transition:all .2s;font-family:inherit}\n"
        ".ncui-tab-btn.ncui-tab-active{color:var(--ncui-accent);border-bottom-color:var(--ncui-accent)}\n"
        ".ncui-tab-panel-hidden{display:none}\n"
    );

    /* Shell layout */
    sb_printf(sb,
        ".ncui-shell{display:grid;grid-template-columns:280px 1fr;min-height:100vh}\n"
        ".ncui-shell-main{display:flex;flex-direction:column;min-width:0}\n"
        ".ncui-sidebar{position:sticky;top:0;align-self:start;min-height:100vh;padding:24px 18px;"
        "background:%s;border-right:1px solid var(--ncui-border);backdrop-filter:blur(20px)}\n"
        ".ncui-topbar{display:flex;align-items:center;justify-content:space-between;gap:16px;"
        "padding:16px 24px;border-bottom:1px solid var(--ncui-border);background:%s;"
        "backdrop-filter:blur(20px);position:sticky;top:0;z-index:20}\n"
        ".ncui-panel{border:1px solid var(--ncui-border);border-radius:20px;padding:18px;margin:18px 0}\n"
        ".ncui-panel-title{font-weight:700;color:var(--ncui-text);margin-bottom:12px}\n"
        ".ncui-banner{padding:12px 18px;border-bottom:1px solid var(--ncui-border)}\n"
        ".ncui-banner-info{background:%s14}\n"
        ".ncui-banner-warning{background:rgba(251,191,36,0.12)}\n"
        ".ncui-banner-error{background:rgba(248,113,113,0.12)}\n"
        ".ncui-banner-text{margin:0;color:var(--ncui-text-sub)}\n"
        "@media(max-width:960px){.ncui-shell{grid-template-columns:1fr}"
        ".ncui-sidebar{position:relative;min-height:auto;border-right:none;border-bottom:1px solid var(--ncui-border)}}\n",
        is_dark ? "rgba(255,255,255,0.03)" : "rgba(255,255,255,0.86)",
        is_dark ? "rgba(10,10,15,0.72)" : "rgba(255,255,255,0.86)", accent);

    /* Animations */
    sb_append(sb,
        ".ncui-anim-fade-up .ncui-container>*{opacity:0;transform:translateY(30px);transition:all .7s cubic-bezier(.4,0,.2,1)}\n"
        ".ncui-anim-fade-up .ncui-container>.ncui-visible{opacity:1;transform:translateY(0)}\n"
        ".ncui-anim-fade-in .ncui-container>*{opacity:0;transition:opacity .7s cubic-bezier(.4,0,.2,1)}\n"
        ".ncui-anim-fade-in .ncui-container>.ncui-visible{opacity:1}\n"
        ".ncui-anim-slide-left .ncui-container>*{opacity:0;transform:translateX(-40px);transition:all .7s cubic-bezier(.4,0,.2,1)}\n"
        ".ncui-anim-slide-left .ncui-container>.ncui-visible{opacity:1;transform:translateX(0)}\n"
        ".ncui-anim-stagger .ncui-container>*{opacity:0;transform:translateY(30px);transition:all .7s cubic-bezier(.4,0,.2,1)}\n"
        ".ncui-anim-stagger .ncui-container>.ncui-visible{opacity:1;transform:translateY(0)}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(1){transition-delay:.1s}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(2){transition-delay:.2s}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(3){transition-delay:.3s}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(4){transition-delay:.4s}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(5){transition-delay:.5s}\n"
        ".ncui-anim-stagger .ncui-container>*:nth-child(6){transition-delay:.6s}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card{opacity:0;transform:translateY(30px);transition:all .6s cubic-bezier(.4,0,.2,1)}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card.ncui-visible{opacity:1;transform:translateY(0)}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card:nth-child(1){transition-delay:.1s}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card:nth-child(2){transition-delay:.2s}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card:nth-child(3){transition-delay:.3s}\n"
        ".ncui-anim-stagger .ncui-grid>.ncui-card:nth-child(4){transition-delay:.4s}\n"
    );

    /* Misc */
    sb_printf(sb,
        ".ncui-divider{border:none;height:1px;background:var(--ncui-border);margin:40px 0}\n"
        ".ncui-spacer{height:60px}\n.ncui-hidden{display:none !important}\n"
        ".ncui-img{width:100%%;border-radius:16px;margin:20px 0}\n"
        ".ncui-video{width:100%%;border-radius:16px;margin:20px 0}\n"
        ".ncui-footer{padding:60px 0;border-top:1px solid var(--ncui-border);text-align:center}\n"
        ".ncui-footer .ncui-text{color:var(--ncui-text-muted);font-size:.9rem}\n"
        ".ncui-footer .ncui-row{justify-content:center}\n"
        ".ncui-footer .ncui-link{font-size:.9rem}\n"
        ".ncui-table-wrap{margin:28px 0;overflow:auto;border:1px solid var(--ncui-border);border-radius:18px}\n"
        ".ncui-table{width:100%%;border-collapse:collapse}\n"
        ".ncui-table-head{text-align:left;padding:14px 16px;font-size:.82rem;letter-spacing:.08em;"
        "text-transform:uppercase;color:var(--ncui-text-muted);border-bottom:1px solid var(--ncui-border)}\n"
        ".ncui-table-cell{padding:14px 16px;border-bottom:1px solid var(--ncui-border)}\n"
        "@keyframes ncui-spin{to{transform:rotate(360deg)}}\n"
        "@keyframes ncui-shimmer{0%%{background-position:200%% 0}100%%{background-position:-200%% 0}}\n"
        "body::before{content:'';position:fixed;top:-50%%;left:-50%%;width:200%%;height:200%%;"
        "background:radial-gradient(circle at 30%% 20%%,%s08 0%%,transparent 50%%),"
        "radial-gradient(circle at 70%% 80%%,%s06 0%%,transparent 50%%);pointer-events:none;z-index:-1}\n"
        "::selection{background:%s33;color:var(--ncui-text)}\n"
        "::-webkit-scrollbar{width:8px}::-webkit-scrollbar-track{background:transparent}"
        "::-webkit-scrollbar-thumb{background:var(--ncui-border);border-radius:4px}\n",
        accent, accent120, accent);
}

/* ── Generate Runtime JS ────────────────────────────────── */
static void generate_runtime_js(StrBuf *sb) {
    sb_append(sb,
        "(function(){\n"
        "var obs=new IntersectionObserver(function(entries){\n"
        "entries.forEach(function(entry){if(entry.isIntersecting){entry.target.classList.add('ncui-visible');}});\n"
        "},{threshold:0.1,rootMargin:'0px 0px -40px 0px'});\n"
        "document.querySelectorAll('[class*=ncui-anim-] .ncui-container>*,[class*=ncui-anim-] .ncui-grid>.ncui-card').forEach(function(el){obs.observe(el);});\n"
        "var nav=document.querySelector('.ncui-nav');\n"
        "if(nav){window.addEventListener('scroll',function(){nav.classList.toggle('ncui-nav-compact',window.scrollY>50);},{passive:true});}\n"
        "document.addEventListener('click',function(e){\n"
        "var navToggle=e.target.closest('[data-ncui-nav-toggle]');\n"
        "if(navToggle){var navInner=navToggle.closest('.ncui-nav-inner');if(navInner)navInner.classList.toggle('open');return;}\n"
        "var modalOpen=e.target.closest('[data-ncui-modal-open]');\n"
        "if(modalOpen){var mid=modalOpen.getAttribute('data-ncui-modal-open');var m=document.getElementById(mid);if(m)m.classList.add('ncui-modal-open');return;}\n"
        "var modalClose=e.target.closest('[data-ncui-modal-close]');\n"
        "if(modalClose){var mr=modalClose.closest('[data-ncui-modal]');if(mr)mr.classList.remove('ncui-modal-open');return;}\n"
        "var modalBg=e.target.matches('[data-ncui-modal]')?e.target:null;\n"
        "if(modalBg){modalBg.classList.remove('ncui-modal-open');return;}\n"
        "var tabBtn=e.target.closest('[data-ncui-tab-btn]');\n"
        "if(tabBtn){var gId=tabBtn.getAttribute('data-ncui-tab-btn');var idx=tabBtn.getAttribute('data-ncui-tab-index');\n"
        "var panels=document.querySelectorAll('[data-tab-group=\"'+gId+'\"]');\n"
        "var tabsEl=document.getElementById(gId);if(!tabsEl)return;\n"
        "var btns=tabsEl.querySelectorAll('.ncui-tab-btn');\n"
        "for(var i=0;i<panels.length;i++){panels[i].classList.toggle('ncui-tab-panel-hidden',String(i)!==String(idx));}\n"
        "for(var j=0;j<btns.length;j++){btns[j].classList.toggle('ncui-tab-active',String(j)===String(idx));}}\n"
        "var dismiss=e.target.closest('[data-ncui-dismissible] .ncui-alert-close');\n"
        "if(dismiss){var alert=dismiss.closest('[data-ncui-dismissible]');if(alert)alert.remove();return;}\n"
        "var link=e.target.closest('[data-ncui-link]');\n"
        "if(link){e.preventDefault();history.pushState(null,'',link.getAttribute('data-ncui-link'));if(typeof _resolve==='function')_resolve();}\n"
        "});\n"
        "})();\n"
    );
}

/* ── Generate App JS (state, actions, reactivity) ─────── */
static void generate_app_js(StrBuf *sb, const NcUIAST *ast) {
    if (!ast->has_state && !ast->has_actions) return;

    sb_append(sb, "(function(){\n");
    sb_append(sb, "var _state={};\n");

    /* Initialize state */
    for (int i = 0; i < ast->state_count; i++) {
        const NcStateSlot *s = &ast->states[i];
        if (strcmp(s->type_hint, "string") == 0) {
            sb_printf(sb, "_state[\"%s\"]=\"%s\";\n", s->name, s->initial);
        } else if (strcmp(s->type_hint, "boolean") == 0) {
            sb_printf(sb, "_state[\"%s\"]=%s;\n", s->name, s->initial);
        } else if (strcmp(s->type_hint, "number") == 0) {
            sb_printf(sb, "_state[\"%s\"]=%s;\n", s->name, s->initial);
        } else if (strcmp(s->type_hint, "array") == 0) {
            sb_printf(sb, "_state[\"%s\"]=%s;\n", s->name, s->initial);
        } else {
            sb_printf(sb, "_state[\"%s\"]=null;\n", s->name);
        }
    }

    /* State helpers */
    sb_append(sb,
        "function _get(k){return _state[k];}\n"
        "function _set(k,v){_state[k]=v;_notify();}\n"
        "function _toggle(k){_state[k]=!_state[k];_notify();}\n"
        "function _addTo(k,v){if(Array.isArray(_state[k]))_state[k].push(v);_notify();}\n"
        "function _removeFrom(k,idx){if(Array.isArray(_state[k]))_state[k].splice(Number(idx),1);_notify();}\n"
    );

    /* Notify: update DOM bindings */
    sb_append(sb,
        "function _notify(){\n"
        "document.querySelectorAll('[data-ncui-text]').forEach(function(el){\n"
        "var tpl=el.getAttribute('data-ncui-text');\n"
        "el.textContent=tpl.replace(/\\{\\{([^}]+)\\}\\}/g,function(_,e){\n"
        "var parts=e.trim().split('.');var v=_state;for(var i=0;i<parts.length;i++){if(v==null)return '';v=v[parts[i]];}return v!=null?v:'';});\n"
        "});\n"
        "document.querySelectorAll('[data-ncui-if]').forEach(function(el){\n"
        "var k=el.getAttribute('data-ncui-if');var neg=k.charAt(0)==='!';\n"
        "var rk=neg?k.slice(1):k;var show=neg?!_state[rk]:!!_state[rk];\n"
        "el.classList.toggle('ncui-hidden',!show);});\n"
        "document.querySelectorAll('[data-ncui-bind]').forEach(function(field){\n"
        "var key=field.getAttribute('data-ncui-bind');if(!key)return;\n"
        "if(document.activeElement===field)return;\n"
        "var v=_state[key];field.value=v==null?'':v;});\n"
        "}\n"
    );

    /* Bind inputs */
    sb_append(sb,
        "document.querySelectorAll('[data-ncui-bind]').forEach(function(field){\n"
        "field.addEventListener('input',function(){\n"
        "var key=field.getAttribute('data-ncui-bind');if(!key)return;\n"
        "_state[key]=field.value;_notify();});});\n"
    );

    /* Action functions */
    for (int i = 0; i < ast->action_count; i++) {
        const NcAction *a = &ast->actions[i];
        sb_printf(sb, "async function action_%s(%s){\n", a->name, a->param);
        for (int j = 0; j < a->stmt_count; j++) {
            const NcActionStmt *st = &a->stmts[j];
            if (strcmp(st->stmt_type, "set") == 0) {
                /* Check if expr is a param reference or a literal */
                if (st->expr[0] == '"' || st->expr[0] == '\'' || (st->expr[0] >= '0' && st->expr[0] <= '9') ||
                    strcmp(st->expr, "true") == 0 || strcmp(st->expr, "false") == 0) {
                    sb_printf(sb, "  _set(\"%s\",%s);\n", st->target, st->expr);
                } else if (a->param[0] && strcmp(st->expr, a->param) == 0) {
                    sb_printf(sb, "  _set(\"%s\",%s);\n", st->target, st->expr);
                } else {
                    sb_printf(sb, "  _set(\"%s\",\"%s\");\n", st->target, st->expr);
                }
            } else if (strcmp(st->stmt_type, "add") == 0) {
                if (a->param[0] && strcmp(st->expr, a->param) == 0)
                    sb_printf(sb, "  _addTo(\"%s\",%s);\n", st->target, st->expr);
                else
                    sb_printf(sb, "  _addTo(\"%s\",\"%s\");\n", st->target, st->expr);
            } else if (strcmp(st->stmt_type, "remove") == 0) {
                sb_printf(sb, "  _removeFrom(\"%s\",%s);\n", st->target, st->expr);
            } else if (strcmp(st->stmt_type, "toggle") == 0) {
                sb_printf(sb, "  _toggle(\"%s\");\n", st->target);
            } else if (strcmp(st->stmt_type, "fetch") == 0) {
                sb_printf(sb, "  {var res=await fetch(\"%s\");var data=await res.json();\n", st->expr);
                if (st->save_as[0])
                    sb_printf(sb, "  _set(\"%s\",data);\n", st->save_as);
                sb_append(sb, "  }\n");
            } else if (strcmp(st->stmt_type, "redirect") == 0) {
                sb_printf(sb, "  history.pushState(null,'',\"%s\");if(typeof _resolve==='function')_resolve();\n", st->expr);
            }
        }
        sb_append(sb, "}\n");
        sb_printf(sb, "window.action_%s=action_%s;\n", a->name, a->name);
    }

    /* Click handlers */
    sb_append(sb,
        "document.querySelectorAll('[data-ncui-click]').forEach(function(el){\n"
        "el.addEventListener('click',function(e){e.preventDefault();\n"
        "var name=el.getAttribute('data-ncui-click');\n"
        "var fn=window['action_'+name];if(typeof fn==='function')fn();});});\n"
    );

    /* Form submit handlers */
    sb_append(sb,
        "document.querySelectorAll('.ncui-form').forEach(function(form){\n"
        "form.addEventListener('submit',async function(e){e.preventDefault();\n"
        "var name=form.getAttribute('data-ncui-submit');\n"
        "var fn=name?window['action_'+name]:null;\n"
        "var data={};\n"
        "form.querySelectorAll('[data-ncui-bind]').forEach(function(f){\n"
        "var k=f.getAttribute('data-ncui-bind');if(k)data[k]=f.value;});\n"
        "if(typeof fn==='function'){await fn(data);}\n"
        "else{var btn=form.querySelector('.ncui-btn');if(btn){var orig=btn.textContent;"
        "btn.textContent='Sent!';setTimeout(function(){btn.textContent=orig;},2000);}}\n"
        "});});\n"
    );

    sb_append(sb, "_notify();\n})();\n");
}

/* ═══════════════════════════════════════════════════════════
 *  Full HTML Document Generation
 * ═══════════════════════════════════════════════════════════ */

static bool generate_html(FILE *out, const char *source) {
    /* 1. Parse source into AST */
    NcUIAST ast;
    memset(&ast, 0, sizeof(ast));
    strncpy(ast.title, "NC UI App", sizeof(ast.title));
    strncpy(ast.theme, "dark", sizeof(ast.theme));
    strncpy(ast.accent, "#6366f1", sizeof(ast.accent));
    strncpy(ast.font, "Inter", sizeof(ast.font));

    NcHTMLParser parser;
    memset(&parser, 0, sizeof(parser));
    lex_init(&parser.lex, source);
    parser.ast = &ast;
    parser_advance(&parser); /* prime first token */
    parse_top_level(&parser);
    lex_free(&parser.lex);

    if (parser.had_error) {
        fprintf(stderr, "  [NC UI] Parse error: %s\n", parser.error_msg);
        /* Don't abort — generate what we can */
    }

    /* 2. Generate CSS */
    StrBuf css_buf; sb_init(&css_buf);
    generate_css(&css_buf, &ast);

    /* 3. Generate body HTML */
    StrBuf body_buf; sb_init(&body_buf);
    for (int i = 0; i < ast.body_count; i++) {
        render_node(&body_buf, ast.body[i], &ast);
    }

    /* 4. Generate runtime JS */
    StrBuf rt_buf; sb_init(&rt_buf);
    generate_runtime_js(&rt_buf);

    /* 5. Generate app JS (state + actions) */
    StrBuf app_buf; sb_init(&app_buf);
    generate_app_js(&app_buf, &ast);

    /* 6. Write HTML document */
    fprintf(out, "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n");
    fprintf(out, "<meta charset=\"UTF-8\">\n");
    fprintf(out, "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(out, "<title>%s</title>\n", ast.title);
    /* Security headers */
    fprintf(out, "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' data: https:; font-src 'self' https://fonts.googleapis.com https://fonts.gstatic.com;\">\n");
    fprintf(out, "<meta http-equiv=\"X-Frame-Options\" content=\"DENY\">\n");
    fprintf(out, "<meta http-equiv=\"X-Content-Type-Options\" content=\"nosniff\">\n");
    fprintf(out, "<meta name=\"referrer\" content=\"strict-origin-when-cross-origin\">\n");
    /* Font */
    fprintf(out, "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n");
    fprintf(out, "<link href=\"https://fonts.googleapis.com/css2?family=%s:wght@300;400;500;600;700;800;900&display=swap\" rel=\"stylesheet\">\n", ast.font);
    /* CSS */
    fprintf(out, "<style>\n%s</style>\n", css_buf.data ? css_buf.data : "");
    fprintf(out, "</head>\n<body>\n");
    /* Body HTML */
    fprintf(out, "%s", body_buf.data ? body_buf.data : "");
    /* Router outlet */
    if (ast.has_routes)
        fprintf(out, "<div id=\"ncui-router-outlet\"></div>\n");
    /* Runtime JS */
    fprintf(out, "<script>\n%s</script>\n", rt_buf.data ? rt_buf.data : "");
    if (app_buf.len > 0)
        fprintf(out, "<script>\n%s</script>\n", app_buf.data);
    fprintf(out, "</body>\n</html>\n");

    /* 7. Cleanup */
    sb_free(&css_buf);
    sb_free(&body_buf);
    sb_free(&rt_buf);
    sb_free(&app_buf);

    /* Free AST body nodes */
    for (int i = 0; i < ast.body_count; i++) free_node(ast.body[i]);
    for (int i = 0; i < ast.comp_count; i++) {
        for (int j = 0; j < ast.components[i].child_count; j++)
            free_node(ast.components[i].children[j]);
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════ */

bool nc_ui_compile_file(const char *input_path, const char *output_dir) {
    printf("  [NC UI] Building %s...\n", input_path);

    FILE *f = fopen(input_path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", input_path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source = (char *)malloc((size_t)fsize + 1);
    if (!source) { fclose(f); return false; }
    fread(source, 1, (size_t)fsize, f);
    fclose(f);
    source[fsize] = '\0';

    /* Derive output filename */
    char output_path[512];
    const char *basename = strrchr(input_path, '/');
    if (!basename) basename = strrchr(input_path, '\\');
    if (!basename) basename = input_path;
    else basename++;

    char name_no_ext[256];
    strncpy(name_no_ext, basename, sizeof(name_no_ext) - 1);
    name_no_ext[sizeof(name_no_ext) - 1] = '\0';
    char *dot = strrchr(name_no_ext, '.');
    if (dot) *dot = '\0';

    snprintf(output_path, sizeof(output_path), "%s/%s.html", output_dir, name_no_ext);

    FILE *outF = fopen(output_path, "wb");
    if (!outF) {
        fprintf(stderr, "Error: Cannot write to %s\n", output_path);
        free(source);
        return false;
    }

    bool ok = generate_html(outF, source);
    fclose(outF);
    free(source);

    if (ok) printf("  [NC UI] Successfully compiled to %s\n", output_path);
    return ok;
}

bool nc_ui_dev_server(const char *input_path, int port) {
    printf("  [NC UI] Dev server starting for %s on port %d...\n", input_path, port);

    /* Compile the file first */
    bool ok = nc_ui_compile_file(input_path, ".");
    if (!ok) return false;

    /* Derive output filename to serve */
    const char *basename = strrchr(input_path, '/');
    if (!basename) basename = strrchr(input_path, '\\');
    if (!basename) basename = input_path;
    else basename++;

    char name_no_ext[256];
    strncpy(name_no_ext, basename, sizeof(name_no_ext) - 1);
    name_no_ext[sizeof(name_no_ext) - 1] = '\0';
    char *dot = strrchr(name_no_ext, '.');
    if (dot) *dot = '\0';

    printf("  [NC UI] Compiled. Open ./%s.html in your browser.\n", name_no_ext);
    printf("  [NC UI] (Live reload server is not yet implemented — rebuild with `nc ui build`)\n");
    return true;
}
