/*
 * nc_json.c — Complete JSON parser and serializer for NC.
 *
 * Parses JSON strings into NcValue (maps, lists, strings, numbers, bools).
 * Serializes NcValue back to JSON strings.
 * Used for: AI API responses, MCP tool results, config files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/nc_json.h"

/* ═══════════════════════════════════════════════════════════
 *  JSON Parser — recursive descent
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *src;
    int         pos;
    int         len;
    char        error[256];
    bool        had_error;
} JsonParser;

static void jp_skip_ws(JsonParser *jp) {
    while (jp->pos < jp->len) {
        char c = jp->src[jp->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') jp->pos++;
        else break;
    }
}

static char jp_peek(JsonParser *jp) {
    return (jp->pos < jp->len) ? jp->src[jp->pos] : '\0';
}

static char jp_advance(JsonParser *jp) {
    return (jp->pos < jp->len) ? jp->src[jp->pos++] : '\0';
}

static bool jp_match(JsonParser *jp, char c) {
    jp_skip_ws(jp);
    if (jp_peek(jp) == c) { jp->pos++; return true; }
    return false;
}

static NcValue jp_parse_value(JsonParser *jp);

static NcValue jp_parse_string(JsonParser *jp) {
    jp->pos++; /* skip opening " */
    int buf_cap = 8192;
    char *buf = malloc(buf_cap);
    if (!buf) return NC_NONE();
    int bi = 0;
    while (jp->pos < jp->len && jp->src[jp->pos] != '"') {
        if (bi >= buf_cap - 4) {
            buf_cap *= 2;
            char *tmp = realloc(buf, buf_cap);
            if (!tmp) { free(buf); return NC_NONE(); }
            buf = tmp;
        }
        if (jp->src[jp->pos] == '\\' && jp->pos + 1 < jp->len) {
            jp->pos++;
            switch (jp->src[jp->pos]) {
                case 'n':  buf[bi++] = '\n'; break;
                case 't':  buf[bi++] = '\t'; break;
                case 'r':  buf[bi++] = '\r'; break;
                case '"':  buf[bi++] = '"'; break;
                case '\\': buf[bi++] = '\\'; break;
                case '/':  buf[bi++] = '/'; break;
                case 'u': {
                    /* \uXXXX — pos points to 'u'; hex digits at pos+1..pos+4 */
                    if (jp->pos + 5 > jp->len) break;
                    bool valid_hex = true;
                    for (int hi = 1; hi <= 4; hi++) {
                        char hc = jp->src[jp->pos + hi];
                        if (!isxdigit((unsigned char)hc)) { valid_hex = false; break; }
                    }
                    if (valid_hex) {
                        unsigned int cp = 0;
                        for (int hi = 1; hi <= 4; hi++) {
                            char hc = jp->src[jp->pos + hi];
                            cp = (cp << 4) | (unsigned int)(hc >= '0' && hc <= '9' ? hc - '0' :
                                               hc >= 'a' && hc <= 'f' ? hc - 'a' + 10 :
                                               hc - 'A' + 10);
                        }
                        if (cp < 0x80) {
                            buf[bi++] = (char)cp;
                        } else if (cp < 0x800) {
                            if (bi + 2 < buf_cap) {
                                buf[bi++] = (char)(0xC0 | (cp >> 6));
                                buf[bi++] = (char)(0x80 | (cp & 0x3F));
                            }
                        } else {
                            if (bi + 3 < buf_cap) {
                                buf[bi++] = (char)(0xE0 | (cp >> 12));
                                buf[bi++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                                buf[bi++] = (char)(0x80 | (cp & 0x3F));
                            }
                        }
                        jp->pos += 4;
                    } else {
                        buf[bi++] = '?';
                    }
                    break;
                }
                default:   buf[bi++] = jp->src[jp->pos]; break;
            }
        } else {
            buf[bi++] = jp->src[jp->pos];
        }
        jp->pos++;
    }
    if (jp->pos < jp->len) jp->pos++; /* skip closing " */
    NcString *result = nc_string_new(buf, bi);
    free(buf);
    return NC_STRING(result);
}

static NcValue jp_parse_number(JsonParser *jp) {
    int start = jp->pos;
    bool is_float = false;
    if (jp->src[jp->pos] == '-') jp->pos++;
    while (jp->pos < jp->len && isdigit((unsigned char)jp->src[jp->pos])) jp->pos++;
    if (jp->pos < jp->len && jp->src[jp->pos] == '.') {
        is_float = true;
        jp->pos++;
        while (jp->pos < jp->len && isdigit((unsigned char)jp->src[jp->pos])) jp->pos++;
    }
    if (jp->pos < jp->len && (jp->src[jp->pos] == 'e' || jp->src[jp->pos] == 'E')) {
        is_float = true;
        jp->pos++;
        if (jp->pos < jp->len && (jp->src[jp->pos] == '+' || jp->src[jp->pos] == '-')) jp->pos++;
        while (jp->pos < jp->len && isdigit((unsigned char)jp->src[jp->pos])) jp->pos++;
    }
    /* Parse directly from source pointer — no fixed-size copy, no truncation.
     * strtod/strtoll handle arbitrarily long numeric strings correctly. */
    const char *num_start = jp->src + start;
    char *end_ptr = NULL;
    if (is_float) {
        double val = strtod(num_start, &end_ptr);
        return NC_FLOAT(val);
    }
    long long val = strtoll(num_start, &end_ptr, 10);
    return NC_INT(val);
}

static NcValue jp_parse_object(JsonParser *jp) {
    jp->pos++; /* skip { */
    NcMap *map = nc_map_new();
    jp_skip_ws(jp);
    if (jp_peek(jp) == '}') { jp->pos++; return NC_MAP(map); }

    while (!jp->had_error) {
        jp_skip_ws(jp);
        if (jp_peek(jp) != '"') {
            snprintf(jp->error, sizeof(jp->error), "Expected string key at pos %d", jp->pos);
            jp->had_error = true;
            break;
        }
        NcValue key = jp_parse_string(jp);
        jp_skip_ws(jp);
        if (!jp_match(jp, ':')) {
            jp->had_error = true; break;
        }
        jp_skip_ws(jp);
        NcValue val = jp_parse_value(jp);
        nc_map_set(map, AS_STRING(key), val);
        jp_skip_ws(jp);
        if (!jp_match(jp, ',')) break;
    }
    jp_match(jp, '}');
    return NC_MAP(map);
}

static NcValue jp_parse_array(JsonParser *jp) {
    jp->pos++; /* skip [ */
    NcList *list = nc_list_new();
    jp_skip_ws(jp);
    if (jp_peek(jp) == ']') { jp->pos++; return NC_LIST(list); }

    while (!jp->had_error) {
        jp_skip_ws(jp);
        nc_list_push(list, jp_parse_value(jp));
        jp_skip_ws(jp);
        if (!jp_match(jp, ',')) break;
    }
    jp_match(jp, ']');
    return NC_LIST(list);
}

static NcValue jp_parse_value(JsonParser *jp) {
    jp_skip_ws(jp);
    char c = jp_peek(jp);

    if (c == '"')  return jp_parse_string(jp);
    if (c == '{')  return jp_parse_object(jp);
    if (c == '[')  return jp_parse_array(jp);
    if (c == '-' || isdigit(c)) return jp_parse_number(jp);

    if (jp->pos + 4 <= jp->len && strncmp(jp->src + jp->pos, "true", 4) == 0) {
        jp->pos += 4; return NC_BOOL(true);
    }
    if (jp->pos + 5 <= jp->len && strncmp(jp->src + jp->pos, "false", 5) == 0) {
        jp->pos += 5; return NC_BOOL(false);
    }
    if (jp->pos + 4 <= jp->len && strncmp(jp->src + jp->pos, "null", 4) == 0) {
        jp->pos += 4; return NC_NONE();
    }

    snprintf(jp->error, sizeof(jp->error), "Unexpected char '%c' at pos %d", c, jp->pos);
    jp->had_error = true;
    return NC_NONE();
}

NcValue nc_json_parse(const char *json_str) {
    if (!json_str || !json_str[0]) return NC_NONE();
    JsonParser jp = { .src = json_str, .pos = 0, .len = (int)strlen(json_str), .had_error = false };
    NcValue result = jp_parse_value(&jp);
    if (jp.had_error) {
        return NC_NONE();
    }
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  JSON Serializer — NcValue → JSON string
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    char *buf;
    int   len;
    int   cap;
    int   indent;
    bool  pretty;
} JsonWriter;

static void jw_init(JsonWriter *jw, bool pretty) {
    jw->cap = 1024;
    jw->buf = malloc(jw->cap);
    jw->len = 0;
    jw->indent = 0;
    jw->pretty = pretty;
    jw->buf[0] = '\0';
}

static void jw_append(JsonWriter *jw, const char *s) {
    if (!jw->buf) return;
    int slen = (int)strlen(s);
    while (jw->len + slen + 1 >= jw->cap) {
        jw->cap *= 2;
        char *tmp = realloc(jw->buf, jw->cap);
        if (!tmp) {
            jw->buf = NULL;
            return;
        }
        jw->buf = tmp;
    }
    memcpy(jw->buf + jw->len, s, slen);
    jw->len += slen;
    jw->buf[jw->len] = '\0';
}

static void jw_char(JsonWriter *jw, char c) {
    char s[2] = {c, '\0'};
    jw_append(jw, s);
}

static void jw_newline(JsonWriter *jw) {
    if (!jw->pretty) return;
    jw_char(jw, '\n');
    for (int i = 0; i < jw->indent * 2; i++) jw_char(jw, ' ');
}

static void jw_write_string(JsonWriter *jw, const char *s) {
    jw_char(jw, '"');
    while (*s) {
        switch (*s) {
            case '"':  jw_append(jw, "\\\""); break;
            case '\\': jw_append(jw, "\\\\"); break;
            case '\n': jw_append(jw, "\\n"); break;
            case '\t': jw_append(jw, "\\t"); break;
            case '\r': jw_append(jw, "\\r"); break;
            default:   jw_char(jw, *s); break;
        }
        s++;
    }
    jw_char(jw, '"');
}

static void jw_write_value(JsonWriter *jw, NcValue v);

static void jw_write_value(JsonWriter *jw, NcValue v) {
    char num_buf[64];
    switch (v.type) {
    case VAL_NONE:
        jw_append(jw, "null");
        break;
    case VAL_BOOL:
        jw_append(jw, v.as.boolean ? "true" : "false");
        break;
    case VAL_INT:
        snprintf(num_buf, sizeof(num_buf), "%lld", (long long)v.as.integer);
        jw_append(jw, num_buf);
        break;
    case VAL_FLOAT:
        snprintf(num_buf, sizeof(num_buf), "%g", v.as.floating);
        jw_append(jw, num_buf);
        break;
    case VAL_STRING:
        jw_write_string(jw, v.as.string->chars);
        break;
    case VAL_LIST: {
        NcList *list = v.as.list;
        jw_char(jw, '[');
        jw->indent++;
        for (int i = 0; i < list->count; i++) {
            if (i > 0) jw_char(jw, ',');
            jw_newline(jw);
            jw_write_value(jw, list->items[i]);
        }
        jw->indent--;
        if (list->count > 0) jw_newline(jw);
        jw_char(jw, ']');
        break;
    }
    case VAL_MAP: {
        NcMap *map = v.as.map;
        jw_char(jw, '{');
        jw->indent++;
        for (int i = 0; i < map->count; i++) {
            if (i > 0) jw_char(jw, ',');
            jw_newline(jw);
            jw_write_string(jw, map->keys[i]->chars);
            jw_append(jw, jw->pretty ? ": " : ":");
            jw_write_value(jw, map->values[i]);
        }
        jw->indent--;
        if (map->count > 0) jw_newline(jw);
        jw_char(jw, '}');
        break;
    }
    default:
        jw_append(jw, "null");
        break;
    }
}

char *nc_json_serialize(NcValue v, bool pretty) {
    JsonWriter jw;
    jw_init(&jw, pretty);
    jw_write_value(&jw, v);
    return jw.buf; /* caller must free */
}
