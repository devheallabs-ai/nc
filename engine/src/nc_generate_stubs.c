/*
 * nc_generate_stubs.c — Open-source stubs for NOVA AI generation functions.
 *
 * The following components are proprietary and ship as compiled binaries:
 *   nc_generate.c     — AI code generation pipeline
 *   nc_ai_benchmark.c — AI benchmark suite
 *   nc_training.c     — Model training (nc_train, nc_learn_from_project)
 *   nc_tensor.c       — Tensor operations (nc_ncfn_tensor_*)
 *   nc_nova_reasoning.c — Reasoning engine (nc_reason_build_*)
 *
 * This file provides no-op stubs so the open-source build compiles and links.
 * AI features print a message directing users to the pre-built binary.
 *
 * Install: curl -sSL https://nc.devheallabs.in/install.sh | bash
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/nc_value.h"
#include "nc_generate.h"
#include "nc_ai_benchmark.h"
#include "nc_nova_stubs.h"

#define NOVA_UNAVAIL() \
    fprintf(stderr, "\nThis NC AI feature requires the pre-built NC binary.\n"); \
    fprintf(stderr, "Install: curl -sSL https://nc.devheallabs.in/install.sh | bash\n\n")

/* ── nc_generate.h stubs ──────────────────────────────────────────────── */

NCGenerator *nc_generator_create(void) { NOVA_UNAVAIL(); return NULL; }
void nc_generator_free(NCGenerator *gen) { (void)gen; }
NCIntent *nc_parse_intent(const char *d) { (void)d; return NULL; }
void nc_intent_free(NCIntent *i) { (void)i; }
char *nc_generate_code(NCGenerator *g, const char *d) { (void)g;(void)d; NOVA_UNAVAIL(); return NULL; }
char *nc_generate_from_intent(NCGenerator *g, NCIntent *i) { (void)g;(void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_service(NCIntent *i)    { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_ncui_page(NCIntent *i)  { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_full_app(NCIntent *i)   { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_crud(NCIntent *i)       { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_ai_service(NCIntent *i) { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_middleware(NCIntent *i) { (void)i; NOVA_UNAVAIL(); return NULL; }
char *nc_template_test(NCIntent *i)       { (void)i; NOVA_UNAVAIL(); return NULL; }
int nc_validate_generated(const char *c, const char *t) { (void)c;(void)t; return 0; }

char *nc_generate_decode_hardened_alloc(const NCTokenizer *tok,
                                        const int *tokens, int n_tokens) {
    (void)tok; (void)tokens; (void)n_tokens; return NULL;
}
char *nc_generate_postprocess_model_output(const char *code, NCTemplateType type) {
    (void)type; (void)code; return NULL;
}
int nc_cmd_generate(int argc, char **argv) { (void)argc;(void)argv; NOVA_UNAVAIL(); return 1; }
int nc_cmd_train(int argc, char **argv)    { (void)argc;(void)argv; NOVA_UNAVAIL(); return 1; }

/* ── nc_ai_benchmark.h stubs ─────────────────────────────────────────── */

void nc_benchmark_run(void) { NOVA_UNAVAIL(); }

/* ── nc_training.c stubs (nc_train, nc_learn_from_project) ───────────── */

void nc_train(NCModel *model, NCTokenizer *tokenizer,
              const char **data_files, int n_files,
              NCTrainConfig config) {
    (void)model;(void)tokenizer;(void)data_files;(void)n_files;(void)config;
    NOVA_UNAVAIL();
}

int nc_learn_from_project(NCModel *model, NCTokenizer *tokenizer,
                          const char *project_dir, NCTrainConfig config) {
    (void)model;(void)tokenizer;(void)project_dir;(void)config;
    NOVA_UNAVAIL();
    return -1;
}

/* ── nc_tensor.c stubs (nc_ncfn_tensor_*) ────────────────────────────── */

NcValue nc_ncfn_tensor_create(int rows, int cols)         { (void)rows;(void)cols; return NC_NONE(); }
NcValue nc_ncfn_tensor_ones(int rows, int cols)           { (void)rows;(void)cols; return NC_NONE(); }
NcValue nc_ncfn_tensor_random(int rows, int cols)         { (void)rows;(void)cols; return NC_NONE(); }
NcValue nc_ncfn_tensor_matmul(NcValue va, NcValue vb)     { (void)va;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_add(NcValue va, NcValue vb)        { (void)va;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_sub(NcValue va, NcValue vb)        { (void)va;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_mul(NcValue va, NcValue vb)        { (void)va;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_scale(NcValue va, double scalar)   { (void)va;(void)scalar; return NC_NONE(); }
NcValue nc_ncfn_tensor_transpose(NcValue va)              { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_softmax(NcValue va)                { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_gelu(NcValue va)                   { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_relu(NcValue va)                   { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_tanh(NcValue va)                   { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_layer_norm(NcValue va, NcValue vg, NcValue vb) {
    (void)va;(void)vg;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_add_bias(NcValue va, NcValue vb)   { (void)va;(void)vb; return NC_NONE(); }
NcValue nc_ncfn_tensor_embedding(NcValue vt, NcValue vi)  { (void)vt;(void)vi; return NC_NONE(); }
NcValue nc_ncfn_tensor_causal_mask(int seq_len)           { (void)seq_len; return NC_NONE(); }
NcValue nc_ncfn_tensor_shape(NcValue va)                  { (void)va; return NC_NONE(); }
NcValue nc_ncfn_tensor_cross_entropy(NcValue vl, NcValue vt) { (void)vl;(void)vt; return NC_NONE(); }
NcValue nc_ncfn_tensor_save(NcValue va, const char *path) { (void)va;(void)path; return NC_NONE(); }
NcValue nc_ncfn_tensor_load(const char *path)             { (void)path; return NC_NONE(); }

/* ── nc_nova_reasoning.c stubs (nc_reason_build_*, nc_reason_chain_*) ── */

int nc_reason_build_prompt(const char *query, char *out, int out_size,
                           bool is_agent, bool chain_of_thought) {
    (void)query;(void)is_agent;(void)chain_of_thought;
    if (out && out_size > 0) out[0] = '\0';
    return 0;
}

int nc_reason_build_agent_prompt(const char *purpose, const char *task,
                                 const char *tool_descriptions,
                                 char *out, int out_size) {
    (void)purpose;(void)task;(void)tool_descriptions;
    if (out && out_size > 0) out[0] = '\0';
    return 0;
}

int nc_reason_build_grounded_prompt(const char *query, const char *url,
                                    char *out, int out_size) {
    (void)query;(void)url;
    if (out && out_size > 0) out[0] = '\0';
    return 0;
}

void nc_reason_chain_print(const NCReasonChain *chain) { (void)chain; }
void nc_reason_chain_free(NCReasonChain *chain)        { (void)chain; }
void nc_reason_stats_print(const NCReasonEngine *engine) { (void)engine; }
