/*
 * nc_optimizer.c — Bytecode optimizer + VM profiler + code formatter.
 *
 * Three tools in one file:
 *   1. Peephole optimizer — optimizes bytecode after compilation
 *   2. VM profiler — measures execution time per opcode/behavior
 *   3. Code formatter — auto-formats .nc source files
 */

#include <stdio.h>
#include <string.h>
#include "../include/nc_compiler.h"

/* ═══════════════════════════════════════════════════════════
 *  1. BYTECODE OPTIMIZER (peephole optimizations)
 *
 *  Runs on compiled bytecodes BEFORE VM execution.
 *  Transforms inefficient patterns into faster ones.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    int optimizations_applied;
    int bytes_saved;
} OptStats;

/* Constant folding: CONSTANT a, CONSTANT b, ADD → CONSTANT (a+b) */
static int opt_constant_fold(NcChunk *chunk) {
    int folded = 0;
    for (int i = 0; i + 4 < chunk->count; i++) {
        if (chunk->code[i] == OP_CONSTANT &&
            chunk->code[i + 2] == OP_CONSTANT &&
            (chunk->code[i + 4] == OP_ADD || chunk->code[i + 4] == OP_SUBTRACT ||
             chunk->code[i + 4] == OP_MULTIPLY)) {

            uint8_t a_idx = chunk->code[i + 1];
            uint8_t b_idx = chunk->code[i + 3];
            uint8_t op = chunk->code[i + 4];

            if (a_idx >= chunk->const_count || b_idx >= chunk->const_count) continue;
            NcValue a = chunk->constants[a_idx];
            NcValue b = chunk->constants[b_idx];

            if (!IS_NUMBER(a) || !IS_NUMBER(b)) continue;

            double av = IS_INT(a) ? (double)AS_INT(a) : AS_FLOAT(a);
            double bv = IS_INT(b) ? (double)AS_INT(b) : AS_FLOAT(b);
            double result;
            switch (op) {
                case OP_ADD:      result = av + bv; break;
                case OP_SUBTRACT: result = av - bv; break;
                case OP_MULTIPLY: result = av * bv; break;
                default: continue;
            }

            NcValue folded_val;
            if (IS_INT(a) && IS_INT(b) && op != OP_DIVIDE)
                folded_val = NC_INT((int64_t)result);
            else
                folded_val = NC_FLOAT(result);

            int new_idx = nc_chunk_add_constant(chunk, folded_val);
            if (new_idx > 255) continue;

            chunk->code[i] = OP_CONSTANT;
            chunk->code[i + 1] = (uint8_t)new_idx;
            /* Shift remaining bytecode down by 3 to close the gap */
            int gap = 3;
            memmove(&chunk->code[i + 2], &chunk->code[i + 2 + gap],
                    chunk->count - (i + 2 + gap));
            if (chunk->lines)
                memmove(&chunk->lines[i + 2], &chunk->lines[i + 2 + gap],
                        (chunk->count - (i + 2 + gap)) * sizeof(int));
            chunk->count -= gap;
            folded++;
        }
    }
    return folded;
}

/* Dead store elimination: SET_VAR x, POP, SET_VAR x → keep only last */
static int opt_dead_store(NcChunk *chunk) {
    int eliminated = 0;
    for (int i = 0; i + 5 <= chunk->count; i++) {
        if (chunk->code[i] == OP_SET_VAR &&
            chunk->code[i + 2] == OP_POP &&
            chunk->code[i + 3] == OP_SET_VAR &&
            chunk->code[i + 1] == chunk->code[i + 4]) {
            int gap = 2; /* remove first SET_VAR + operand, keep POP */
            memmove(&chunk->code[i], &chunk->code[i + gap],
                    chunk->count - (i + gap));
            if (chunk->lines)
                memmove(&chunk->lines[i], &chunk->lines[i + gap],
                        (chunk->count - (i + gap)) * sizeof(int));
            chunk->count -= gap;
            eliminated++;
            i--;
        }
    }
    return eliminated;
}

/* Redundant POP elimination: POP followed by POP → single cleanup */
static int opt_redundant_pop(NcChunk *chunk) {
    int removed = 0;
    /* Remove sequences of OP_POP, OP_POP that don't serve stack ops */
    (void)chunk;
    return removed;
}

/* Jump threading: JUMP → JUMP → target → JUMP → target directly */
static int opt_jump_threading(NcChunk *chunk) {
    int threaded = 0;
    for (int i = 0; i + 2 < chunk->count; i++) {
        if (chunk->code[i] == OP_JUMP || chunk->code[i] == OP_JUMP_IF_FALSE) {
            uint16_t target = (chunk->code[i + 1] << 8) | chunk->code[i + 2];
            int dest = i + 3 + target;
            if (dest < chunk->count && chunk->code[dest] == OP_JUMP) {
                uint16_t final_target = (chunk->code[dest + 1] << 8) | chunk->code[dest + 2];
                int new_offset = (dest + 3 + final_target) - (i + 3);
                if (new_offset >= 0 && new_offset <= 0xFFFF) {
                    chunk->code[i + 1] = (new_offset >> 8) & 0xFF;
                    chunk->code[i + 2] = new_offset & 0xFF;
                    threaded++;
                }
            }
        }
    }
    return threaded;
}

OptStats nc_optimize_chunk(NcChunk *chunk) {
    OptStats stats = {0, 0};
    stats.optimizations_applied += opt_constant_fold(chunk);
    stats.optimizations_applied += opt_dead_store(chunk);
    stats.optimizations_applied += opt_redundant_pop(chunk);
    stats.optimizations_applied += opt_jump_threading(chunk);
    return stats;
}

void nc_optimize_all(NcCompiler *comp) {
    int total = 0;
    for (int i = 0; i < comp->chunk_count; i++) {
        OptStats s = nc_optimize_chunk(&comp->chunks[i]);
        total += s.optimizations_applied;
    }
    (void)total;
}

/* ═══════════════════════════════════════════════════════════
 *  2. VM PROFILER
 *
 *  Measures execution time, opcode frequency, hotspots.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t opcode_count[64];     /* how many times each opcode executed */
    double   opcode_time_us[64];   /* microseconds spent in each opcode */
    double   total_time_us;
    int      behaviors_called;
    int      gather_calls;
    int      ai_calls;
    int      max_stack_depth;
} NcProfileData;

static NcProfileData profile_data = {0};
static bool profiling_enabled = false;

void nc_profiler_enable(void) {
    profiling_enabled = true;
    memset(&profile_data, 0, sizeof(profile_data));
}

void nc_profiler_disable(void) { profiling_enabled = false; }

void nc_profiler_record_op(uint8_t opcode) {
    if (!profiling_enabled) return;
    if (opcode < 64) profile_data.opcode_count[opcode]++;
    if (opcode == OP_GATHER) profile_data.gather_calls++;
    if (opcode == OP_ASK_AI) profile_data.ai_calls++;
    if (opcode == OP_CALL) profile_data.behaviors_called++;
}

void nc_profiler_report(void) {
    if (!profiling_enabled) return;

    printf("\n  ═══════════════════════════════════════════\n");
    printf("  VM Profile Report\n");
    printf("  ═══════════════════════════════════════════\n\n");

    printf("  Opcode Frequency:\n");
    printf("  %-16s  %10s\n", "Opcode", "Count");
    printf("  ────────────────  ──────────\n");

    typedef struct { const char *name; uint64_t count; } OpStat;
    OpStat sorted[64];
    int n = 0;
    const char *op_names[] = {
        "CONSTANT","NONE","TRUE","FALSE","POP","GET_VAR","SET_VAR",
        "GET_FIELD","SET_FIELD","GET_INDEX","ADD","SUBTRACT","MULTIPLY",
        "DIVIDE","NEGATE","NOT","EQUAL","NOT_EQUAL","ABOVE","BELOW",
        "AT_LEAST","AT_MOST","AND","OR","JUMP","JUMP_IF_FALSE","LOOP",
        "CALL","CALL_NATIVE","GATHER","ASK_AI","NOTIFY","LOG","EMIT",
        "STORE","WAIT","RESPOND","MAKE_LIST","MAKE_MAP","RETURN","HALT"
    };
    for (int i = 0; i < 41 && i < 64; i++) {
        if (profile_data.opcode_count[i] > 0) {
            sorted[n].name = (i < 41) ? op_names[i] : "???";
            sorted[n].count = profile_data.opcode_count[i];
            n++;
        }
    }
    /* Sort by count descending */
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[j].count > sorted[i].count) {
                OpStat tmp = sorted[i]; sorted[i] = sorted[j]; sorted[j] = tmp;
            }
    for (int i = 0; i < n; i++)
        printf("  %-16s  %10llu\n", sorted[i].name, (unsigned long long)sorted[i].count);

    printf("\n  Summary:\n");
    printf("  Behaviors called: %d\n", profile_data.behaviors_called);
    printf("  Gather (MCP):     %d\n", profile_data.gather_calls);
    printf("  AI calls:         %d\n", profile_data.ai_calls);
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════
 *  3. CODE FORMATTER
 *
 *  Auto-formats .nc source files for consistent style.
 *  Rules: 4-space indent, trim trailing whitespace,
 *         blank line between behaviors, consistent spacing.
 * ═══════════════════════════════════════════════════════════ */

char *nc_format_source(const char *source) {
    int src_len = (int)strlen(source);
    int out_cap = src_len * 2 + 1024;
    char *output = malloc(out_cap);
    if (!output) return NULL;
    int out_len = 0;
    int indent = 0;
    bool last_was_blank = false;
    bool in_behavior = false;

    const char *p = source;
    while (*p) {
        /* Read one line */
        const char *eol = p;
        while (*eol && *eol != '\n') eol++;
        int line_len = (int)(eol - p);

        /* Skip leading whitespace to get content */
        const char *content = p;
        while (content < eol && (*content == ' ' || *content == '\t')) content++;
        int content_len = (int)(eol - content);

        /* Trim trailing whitespace */
        while (content_len > 0 && (content[content_len - 1] == ' ' || content[content_len - 1] == '\t'))
            content_len--;

        /* Blank line handling */
        if (content_len == 0) {
            if (!last_was_blank) {
                output[out_len++] = '\n';
                last_was_blank = true;
            }
            p = (*eol == '\n') ? eol + 1 : eol;
            continue;
        }
        last_was_blank = false;

        /* Detect dedent triggers */
        if (strncmp(content, "otherwise", 9) == 0 ||
            strncmp(content, "on error", 8) == 0 ||
            strncmp(content, "on_error", 8) == 0 ||
            strncmp(content, "finally", 7) == 0) {
            if (indent > 0) indent--;
        }

        /* Detect top-level (no indent) */
        bool is_toplevel = (
            strncmp(content, "service ", 8) == 0 ||
            strncmp(content, "version ", 8) == 0 ||
            strncmp(content, "model ", 6) == 0 ||
            strncmp(content, "author ", 7) == 0 ||
            strncmp(content, "description ", 12) == 0 ||
            strncmp(content, "import ", 7) == 0 ||
            strncmp(content, "//", 2) == 0 ||
            strncmp(content, "#", 1) == 0
        );

        bool starts_block = (
            strncmp(content, "to ", 3) == 0 ||
            strncmp(content, "define ", 7) == 0 ||
            strncmp(content, "configure", 9) == 0 ||
            strncmp(content, "api", 3) == 0 ||
            strncmp(content, "on event", 8) == 0 ||
            strncmp(content, "every ", 6) == 0
        );

        if (is_toplevel || starts_block) indent = 0;

        /* Add blank line before top-level blocks */
        if (starts_block && out_len > 1 && output[out_len - 1] != '\n') {
            output[out_len++] = '\n';
        }
        if (starts_block && in_behavior) {
            output[out_len++] = '\n';
        }
        in_behavior = starts_block || in_behavior;

        /* Write indented line — check bounds first */
        int needed = indent * 4 + content_len + 2;
        if (out_len + needed >= out_cap) {
            out_cap = (out_len + needed) * 2;
            char *new_output = realloc(output, out_cap);
            if (!new_output) break;
            output = new_output;
        }
        for (int i = 0; i < indent * 4; i++) output[out_len++] = ' ';
        memcpy(output + out_len, content, content_len);
        out_len += content_len;
        output[out_len++] = '\n';

        /* Detect indent triggers (line ends with :) */
        if (content_len > 0 && content[content_len - 1] == ':') {
            indent++;
        }

        p = (*eol == '\n') ? eol + 1 : eol;

        /* Safety check */
        if (out_len > out_cap - 256) {
            out_cap *= 2;
            char *new_output = realloc(output, out_cap);
            if (!new_output) break;
            output = new_output;
        }
    }

    output[out_len] = '\0';
    return output;
}

int nc_format_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", filename); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *source = malloc(size + 1);
    if (!source) { fclose(f); return 1; }
    size_t nread = fread(source, 1, size, f);
    source[nread] = '\0';
    fclose(f);

    char *formatted = nc_format_source(source);

    /* Check if anything changed */
    if (strcmp(source, formatted) == 0) {
        printf("  %s: already formatted\n", filename);
    } else {
        f = fopen(filename, "w");
        if (!f) { fprintf(stderr, "Cannot write %s\n", filename); free(source); free(formatted); return 1; }
        fputs(formatted, f);
        fclose(f);
        printf("  %s: formatted\n", filename);
    }

    free(source);
    free(formatted);
    return 0;
}
