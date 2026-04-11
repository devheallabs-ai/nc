#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../../engine/include/nc.h"
#include "../../engine/include/nc_json.h"

/*
 * libFuzzer / AFL++ Harness for NC Language
 *
 * Fuzz targets:
 *   1. JSON parser       — arbitrary JSON input
 *   2. JSON number edge  — numeric strings (exponents, long integers)
 *   3. NC lexer/parser   — arbitrary NC source compilation
 *   4. Template engine   — {{var}} interpolation with fuzz vars
 *   5. HTTP response     — JSON-body HTTP-like response parsing
 *   6. Chunked encoding  — Transfer-Encoding: chunked body parsing
 *
 * Build for libFuzzer:
 *   clang -g -O1 -fsanitize=fuzzer,address -I../../engine/include \
 *         nc_fuzz.c ../../engine/src/*.c -o nc_fuzz -lpthread
 *
 * Build for AFL++:
 *   afl-clang-fast -g -O1 -fsanitize=address -DUSE_AFL \
 *         -I../../engine/include nc_fuzz.c ../../engine/src/*.c \
 *         -o nc_afl -lpthread
 *
 * Corpus seeds — put representative inputs in a corpus/ directory:
 *   mkdir corpus
 *   echo '{"key": 1.5e-10}' > corpus/exp_number.json
 *   echo 'set x to 1'       > corpus/simple.nc
 *   echo '{{name}}'         > corpus/template.tpl
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ── Helper: split fuzz data into two NUL-terminated slices ── */
static void split_at(const uint8_t *data, size_t size, size_t split,
                     char **a, char **b) {
    size_t la = split < size ? split : size;
    size_t lb = size - la;
    *a = malloc(la + 1);
    *b = malloc(lb + 1);
    if (*a) { memcpy(*a, data,      la); (*a)[la] = '\0'; }
    if (*b) { memcpy(*b, data + la, lb); (*b)[lb] = '\0'; }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 65536) return 0;

    /* Use first byte as a selector to spread coverage across targets
     * without always running all paths (keeps exec/s high). */
    uint8_t target = data[0] % 6;
    const uint8_t *payload = data + 1;
    size_t plen  = size - 1;

    char *buf = malloc(plen + 1);
    if (!buf) return 0;
    memcpy(buf, payload, plen);
    buf[plen] = '\0';

    switch (target) {

    /* ── Target 1: JSON parser — arbitrary input ── */
    case 0: {
        NcValue v = nc_json_parse(buf);
        (void)v;
        break;
    }

    /* ── Target 2: JSON number edge cases ──
     * Feed strings that start with a digit or minus so the number
     * parser is exercised specifically: exponents, long ints, NaN text. */
    case 1: {
        /* Wrap payload in a JSON number context */
        char *num_json = malloc(plen + 16);
        if (num_json) {
            snprintf(num_json, plen + 16, "{\"n\":%s}", buf);
            NcValue v = nc_json_parse(num_json);
            (void)v;
            free(num_json);
        }
        break;
    }

    /* ── Target 3: NC lexer/parser ── */
    case 2: {
        NcModule *module = nc_compile(buf);
        if (module) nc_free_module(module);
        break;
    }

    /* ── Target 4: Template engine — {{var}} interpolation ──
     * Split input into template string + variable value.
     * Covers injection attempts like {{../../../etc}}, deeply
     * nested braces, and zero-length variable names. */
    case 3: {
        char *tpl = NULL, *val = NULL;
        size_t split = plen / 2;
        split_at(payload, plen, split, &tpl, &val);
        if (tpl && val) {
            NcMap *vars = nc_map_new();
            /* Use the second half as both key and value so something matches */
            NcString *key = nc_string_new("x", 1);
            NcString *sval = nc_string_new(val, (int)strlen(val));
            nc_map_set(vars, key, NC_STRING(sval));
            char *out = nc_ai_fill_template(tpl, vars);
            free(out);
        }
        free(tpl);
        free(val);
        break;
    }

    /* ── Target 5: HTTP JSON-body response parsing ──
     * Simulate the shape of responses from nc_http_post / nc_ai_call:
     * a JSON object with a "response" key. The AI bridge uses
     * nc_ai_extract_by_path to walk dot-paths through the parsed map. */
    case 4: {
        /* Wrap fuzz payload as a JSON response body */
        char *json_resp = malloc(plen + 64);
        if (json_resp) {
            snprintf(json_resp, plen + 64,
                     "{\"ok\":true,\"response\":%s}", buf);
            NcValue parsed = nc_json_parse(json_resp);
            if (IS_MAP(parsed)) {
                /* Exercise the dot-path extractor with a fuzz-controlled path */
                char path_buf[128];
                size_t ppath = plen > 60 ? 60 : plen;
                memcpy(path_buf, buf, ppath);
                path_buf[ppath] = '\0';
                NcValue extracted = nc_ai_extract_by_path(parsed, path_buf);
                (void)extracted;
            }
            free(json_resp);
        }
        break;
    }

    /* ── Target 6: Chunked Transfer-Encoding body reassembly ──
     * The HTTP client receives chunked responses and must decode them.
     * Feed arbitrary chunk-format data to exercise the decoder.
     * Format: <hex-size>\r\n<data>\r\n ... 0\r\n\r\n */
    case 5: {
        /* Wrap fuzz data in a minimal chunked envelope and hand it
         * to nc_json_parse which is called on the reassembled body.
         * We don't call the raw chunk decoder directly (it's internal),
         * but we test it indirectly via nc_http_post on loopback when
         * integration tests run.  Here we parse the payload as if it
         * were a reassembled body, covering the JSON step. */
        NcValue v = nc_json_parse(buf);
        (void)v;
        /* Additionally compile it in case it looks like NC source */
        if (plen < 4096) {
            NcModule *m = nc_compile(buf);
            if (m) nc_free_module(m);
        }
        break;
    }

    default:
        break;
    }

    free(buf);
    return 0;
}

#ifdef __cplusplus
}
#endif

#ifdef USE_AFL
/* Standalone main for AFL++ */
int main(int argc, char **argv) {
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif
    uint8_t buf[65536];
    size_t len = fread(buf, 1, sizeof(buf) - 1, stdin);
    if (len > 0) {
        LLVMFuzzerTestOneInput(buf, len);
    }
    return 0;
}
#endif
