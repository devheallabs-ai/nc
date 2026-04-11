/*
 * nc_suggestions.c — "Did you mean?" suggestions for NC.
 *
 * Uses Levenshtein distance to suggest close matches when a
 * variable, behavior, or keyword is not found.  Inspired by
 * CPython's suggestions.c — adapted for plain English syntax.
 *
 * This makes NC dramatically more user-friendly, especially
 * for AI-generated code that may contain typos.
 */

#include "../include/nc.h"

#define MOVE_COST 2
#define CASE_COST 1
#define MAX_CANDIDATE_LEN 64
#define MAX_CANDIDATES 256

/* ═══════════════════════════════════════════════════════════
 *  Levenshtein distance (edit distance)
 *
 *  Measures how many single-character edits (insertions,
 *  deletions, substitutions) are needed to change one
 *  string into another.  Case changes cost less.
 * ═══════════════════════════════════════════════════════════ */

static int substitution_cost(char a, char b) {
    if (a == b) return 0;
    char la = (a >= 'A' && a <= 'Z') ? a + ('a' - 'A') : a;
    char lb = (b >= 'A' && b <= 'Z') ? b + ('a' - 'A') : b;
    if (la == lb) return CASE_COST;
    return MOVE_COST;
}

static int levenshtein(const char *a, int a_len, const char *b, int b_len, int max_cost) {
    if (a == b) return 0;
    if (a_len == 0) return b_len * MOVE_COST;
    if (b_len == 0) return a_len * MOVE_COST;
    if (a_len > MAX_CANDIDATE_LEN || b_len > MAX_CANDIDATE_LEN)
        return max_cost + 1;

    /* Trim common prefix and suffix */
    while (a_len > 0 && b_len > 0 && a[0] == b[0]) { a++; a_len--; b++; b_len--; }
    while (a_len > 0 && b_len > 0 && a[a_len-1] == b[b_len-1]) { a_len--; b_len--; }
    if (a_len == 0 || b_len == 0) return (a_len + b_len) * MOVE_COST;

    /* Ensure a is the shorter string */
    if (b_len < a_len) {
        const char *t = a; a = b; b = t;
        int tl = a_len; a_len = b_len; b_len = tl;
    }

    if ((b_len - a_len) * MOVE_COST > max_cost) return max_cost + 1;

    /* Full DP matrix for Damerau-Levenshtein (supports transposition) */
    int matrix[MAX_CANDIDATE_LEN + 1][MAX_CANDIDATE_LEN + 1];
    for (int i = 0; i <= a_len; i++) matrix[i][0] = i * MOVE_COST;
    for (int j = 0; j <= b_len; j++) matrix[0][j] = j * MOVE_COST;

    for (int i = 1; i <= a_len; i++) {
        for (int j = 1; j <= b_len; j++) {
            int sub = matrix[i-1][j-1] + substitution_cost(a[i-1], b[j-1]);
            int del = matrix[i-1][j] + MOVE_COST;
            int ins = matrix[i][j-1] + MOVE_COST;
            int best = sub < del ? sub : del;
            if (ins < best) best = ins;
            /* Transposition: swap two adjacent characters (lne → len) */
            if (i > 1 && j > 1 && a[i-1] == b[j-2] && a[i-2] == b[j-1]) {
                int trans = matrix[i-2][j-2] + MOVE_COST;
                if (trans < best) best = trans;
            }
            matrix[i][j] = best;
        }
    }
    return matrix[a_len][b_len];
}

/* ═══════════════════════════════════════════════════════════
 *  Suggest closest match from a list of candidates
 *
 *  Returns the closest match, or NULL if nothing is close enough.
 *  "Close enough" = edit distance ≤ (length of input) * MOVE_COST / 2
 * ═══════════════════════════════════════════════════════════ */

const char *nc_suggest_from_list(const char *input, const char **candidates, int count) {
    if (!input || !candidates || count <= 0) return NULL;
    int input_len = (int)strlen(input);
    if (input_len == 0 || input_len > MAX_CANDIDATE_LEN) return NULL;

    int max_cost = (input_len + 1) * MOVE_COST / 2;
    if (max_cost < MOVE_COST * 2) max_cost = MOVE_COST * 2;

    const char *best = NULL;
    int best_cost = max_cost + 1;
    int best_len_diff = 999;

    for (int i = 0; i < count; i++) {
        if (!candidates[i]) continue;
        int cand_len = (int)strlen(candidates[i]);
        int dist = levenshtein(input, input_len, candidates[i], cand_len, max_cost);
        if (dist < best_cost) {
            best_cost = dist;
            best = candidates[i];
            best_len_diff = abs(cand_len - input_len);
        } else if (dist == best_cost) {
            /* On tie, prefer candidate with closer length to input */
            int ld = abs(cand_len - input_len);
            if (ld < best_len_diff) {
                best = candidates[i];
                best_len_diff = ld;
            }
        }
    }

    return (best_cost <= max_cost) ? best : NULL;
}

/* ═══════════════════════════════════════════════════════════
 *  Suggest from an NcMap's keys (for scope variable lookup)
 * ═══════════════════════════════════════════════════════════ */

const char *nc_suggest_from_map(const char *input, NcMap *map) {
    if (!input || !map || map->count == 0) return NULL;
    const char *candidates[MAX_CANDIDATES];
    int count = 0;
    for (int i = 0; i < map->count && count < MAX_CANDIDATES; i++) {
        if (map->keys[i]) candidates[count++] = map->keys[i]->chars;
    }
    return nc_suggest_from_list(input, candidates, count);
}

/* ═══════════════════════════════════════════════════════════
 *  Built-in NC names (keywords, built-in functions, types)
 *
 *  These are suggested when the parser encounters an unknown word.
 *  Generic — no company or model names, just NC language constructs.
 * ═══════════════════════════════════════════════════════════ */

static const char *nc_builtins[] = {
    /* control flow */
    "if", "otherwise", "repeat", "for", "each", "in", "while",
    "match", "when", "stop", "skip",
    /* declarations */
    "set", "to", "service", "version", "define", "as",
    "import", "from", "module",
    /* behaviors */
    "to", "with", "respond", "run",
    /* AI & data */
    "ask", "AI", "gather", "store", "emit", "notify", "check",
    "using", "save", "called", "configure",
    /* operators */
    "is", "above", "below", "equal", "not", "and", "or",
    "at", "least", "most", "between", "greater", "less", "than",
    /* types */
    "text", "number", "yesno", "list", "record", "nothing", "none",
    /* boolean */
    "yes", "no", "true", "false",
    /* time */
    "wait", "seconds", "minutes", "hours",
    /* built-in functions */
    "len", "str", "int", "print", "show", "log", "get",
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

const char *nc_suggest_builtin(const char *input) {
    int count = 0;
    while (nc_builtins[count]) count++;
    return nc_suggest_from_list(input, nc_builtins, count);
}

/* ═══════════════════════════════════════════════════════════
 *  Format a suggestion message (plain English style)
 *
 *  Returns a newly allocated string like:
 *  "  Hint: Did you mean 'print'?"
 * ═══════════════════════════════════════════════════════════ */

char *nc_format_suggestion(const char *input, const char *suggestion) {
    if (!suggestion) return NULL;
    char *msg = malloc(256);
    if (!msg) return NULL;
    snprintf(msg, 256, "  Hint: Is this what you meant → '%s'?", suggestion);
    return msg;
}

/* ═══════════════════════════════════════════════════════════
 *  Auto-correct with confidence level
 *
 *  Returns the best match AND its edit distance.
 *  The caller decides whether to auto-correct based on distance:
 *    distance ≤ 2  → auto-correct (1 typo, high confidence)
 *    distance ≤ 4  → suggest (might be a typo)
 *    distance > 4  → no suggestion (too far)
 * ═══════════════════════════════════════════════════════════ */

const char *nc_suggest_with_distance(const char *input, const char **candidates,
                                      int count, int *out_distance) {
    if (!input || !candidates || count <= 0) { *out_distance = 999; return NULL; }
    int input_len = (int)strlen(input);
    if (input_len == 0 || input_len > MAX_CANDIDATE_LEN) { *out_distance = 999; return NULL; }

    int max_cost = (input_len + 1) * MOVE_COST;
    const char *best = NULL;
    int best_cost = max_cost + 1;
    int best_len_diff = 999;

    for (int i = 0; i < count; i++) {
        if (!candidates[i]) continue;
        int cand_len = (int)strlen(candidates[i]);
        int dist = levenshtein(input, input_len, candidates[i], cand_len, max_cost);
        if (dist < best_cost) {
            best_cost = dist; best = candidates[i];
            best_len_diff = abs(cand_len - input_len);
        } else if (dist == best_cost) {
            int ld = abs(cand_len - input_len);
            if (ld < best_len_diff) { best = candidates[i]; best_len_diff = ld; }
        }
    }

    *out_distance = best_cost;
    return best;
}

const char *nc_autocorrect_from_map(const char *input, NcMap *map, int *out_distance) {
    if (!input || !map || map->count == 0) { *out_distance = 999; return NULL; }
    const char *candidates[MAX_CANDIDATES];
    int count = 0;
    for (int i = 0; i < map->count && count < MAX_CANDIDATES; i++)
        if (map->keys[i]) candidates[count++] = map->keys[i]->chars;
    return nc_suggest_with_distance(input, candidates, count, out_distance);
}

const char *nc_autocorrect_builtin(const char *input, int *out_distance) {
    int count = 0;
    while (nc_builtins[count]) count++;
    return nc_suggest_with_distance(input, nc_builtins, count, out_distance);
}
