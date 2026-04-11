/*
 * nc_nova_stubs.h -- NOVA AI Engine Stubs (Open Source Build)
 *
 * The NOVA inference engine, tokenizer, model architecture, and training
 * pipeline are proprietary components distributed as compiled binaries in
 * official NC releases. See NOVA_ARCHITECTURE.md for details.
 *
 * AI commands print a message directing users to the pre-built binary.
 */
#ifndef NC_NOVA_STUBS_H
#define NC_NOVA_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NC_NOVA_UNAVAILABLE() \
    fprintf(stderr, "\nNOVA AI engine requires the pre-built NC binary.\n"); \
    fprintf(stderr, "Install: curl -sSL https://nc.devheallabs.in/install.sh | bash\n\n")

#define NOVA_MAGIC 0x4E4F5641  /* "NOVA" */

/* ---- NCModel ----------------------------------------------------------- */
typedef struct NCModel {
    int dim;
    int n_layers;
    int n_heads;
    int vocab_size;
    int max_seq;
    int chars;
    int _stub;
} NCModel;

typedef struct {
    int dim;
    int n_layers;
    int n_heads;
    int vocab_size;
    int hidden_dim;
    int max_seq;
    int batch_size;
    int n_experts;
    int n_active;
} NCModelConfig;

/* ---- NCTokenizer ------------------------------------------------------- */
typedef struct NCTokenizer {
    int vocab_size;
    int n_merges;
    int _stub;
} NCTokenizer;

/* ---- NovaModel --------------------------------------------------------- */
typedef struct NovaConfig {
    int   dim;
    int   n_layers;
    int   vocab_size;
    int   max_seq;
    float lr;
    int   use_metal;
    int   size;
    int   _stub;
} NovaConfig;

typedef struct NovaModel {
    NovaConfig config;
    int _stub;
} NovaModel;

typedef struct NovaOptimizer { int _stub; } NovaOptimizer;

typedef struct {
    double ms_per_token;
    double tokens_per_sec;
    int64_t params;
} NovaBenchmark;

typedef struct {
    int        total_steps;
    int        seq_len;
    const char *save_path;
    float      lr;
    int        batch_size;
    int        use_graph;
    int        use_hebbian;
    int        use_nce;
    int        nce_negatives;
    int        use_cache;
    int        use_cgr;
    int        use_papt;
    int        use_hrl_warmup;
    int        epochs;
    int        warmup_steps;
} NovaTrainConfig;

typedef enum {
    NOVA_SIZE_MICRO=0, NOVA_SIZE_SMALL, NOVA_SIZE_BASE, NOVA_SIZE_LARGE
} NovaModelSize;

/* ---- NCDistillConfig --------------------------------------------------- */
typedef struct {
    const char *teacher_url;
    const char *teacher_model;
    const char *teacher_key;
    int         n_prompts;
    int         _stub;
} NCDistillConfig;

/* ---- NCReasonConfig / NCReasonEngine / NCReasonChain ------------------ */
typedef struct { int max_steps; float temperature; int use_math; int verbose; } NCReasonConfig;
typedef struct { int _stub; } NCReasonEngine;
typedef struct { int n_steps; float confidence; } NCReasonChain;

/* ---- NCMathResult ------------------------------------------------------ */
typedef struct { int valid; double value; char unit[32]; } NCMathResult;

/* ---- NCTrainConfig ----------------------------------------------------- */
typedef struct { int epochs; float lr; int batch_size; int total_steps; const char *checkpoint_dir; int warmup_steps; float grad_clip; } NCTrainConfig;

/* ---- NCQuantType ------------------------------------------------------- */
typedef enum { NC_QUANT_NONE=0, NC_QUANT_INT8, NC_QUANT_INT4 } NCQuantType;
typedef struct { int _stub; } NCBatch;
typedef struct { int _stub; } NCBatchResult;

/* ---- NCModel inline stubs ---------------------------------------------- */
static inline NCModelConfig nc_model_default_config(void) { NCModelConfig c={0}; return c; }
static inline NCModel *nc_model_create(NCModelConfig c) { (void)c; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline NCModel *nc_model_load(const char *p) { (void)p; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline NCModel *nc_model_load_quantized(const char *p) { (void)p; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline void nc_model_free(NCModel *m) { (void)m; }
static inline int  nc_model_save(const NCModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline int  nc_model_save_quantized(const NCModel *m, const char *p, NCQuantType t) {
    (void)m;(void)p;(void)t; return -1; }
static inline int  nc_model_generate(NCModel *m, const int *t, int n, int max, float temp, int *out) {
    (void)m;(void)t;(void)n;(void)max;(void)temp;(void)out; return 0; }
static inline int  nc_model_generate_advanced(NCModel *m, const int *t, int n, int max,
                                               float temp, int top_k, float top_p, int *out) {
    (void)m;(void)t;(void)n;(void)max;(void)temp;(void)top_k;(void)top_p;(void)out; return 0; }
static inline NCBatchResult nc_model_forward_batch(NCModel *m, NCBatch b) {
    (void)m;(void)b; NCBatchResult r={0}; return r; }

/* ---- NCTokenizer stubs ------------------------------------------------- */
static inline NCTokenizer *nc_tokenizer_create(void) { NC_NOVA_UNAVAILABLE(); return NULL; }
static inline NCTokenizer *nc_tokenizer_load(const char *p) { (void)p; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline void nc_tokenizer_free(NCTokenizer *t) { (void)t; }
static inline int  nc_tokenizer_save(const NCTokenizer *t, const char *p) { (void)t;(void)p; return -1; }
static inline int  nc_tokenizer_encode(NCTokenizer *t, const char *s, int *out, int max) {
    (void)t;(void)s;(void)out;(void)max; return 0; }
static inline int  nc_tokenizer_decode(NCTokenizer *t, const int *tok, int n, char *out, int max) {
    (void)t;(void)tok;(void)n;(void)out;(void)max; return 0; }
static inline void nc_tokenizer_train(NCTokenizer *t, const char **texts, int n, int vocab) {
    (void)t;(void)texts;(void)n;(void)vocab; }

/* ---- NovaModel stubs --------------------------------------------------- */
static inline NovaConfig nova_config_micro(void)  { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_small(void)  { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_base(void)   { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_large(void)  { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_1b(void)     { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_7b(void)     { NovaConfig c={0}; return c; }
static inline NovaConfig nova_config_20b(void)    { NovaConfig c={0}; return c; }
static inline NovaModel *nova_create(NovaConfig c) { (void)c; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline void       nova_free(NovaModel *m) { (void)m; }
static inline int64_t    nova_count_params(const NovaModel *m) { (void)m; return 0; }
static inline const char *nova_size_name(NovaModelSize s) { (void)s; return "unavailable"; }
static inline int  nova_save(const NovaModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline int  nova_load(NovaModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline int  nova_graph_save(const NovaModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline int  nova_graph_load(NovaModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline int  nova_graph_count(const NovaModel *m) { (void)m; return 0; }
static inline int  nova_export_onnx(const NovaModel *m, const char *p) { (void)m;(void)p; return -1; }
static inline NovaOptimizer *nova_optimizer_create(NovaModel *m, float lr) { (void)m;(void)lr; return NULL; }
static inline void nova_optimizer_free(NovaOptimizer *o) { (void)o; }
static inline void nova_hebbian_learn(NovaModel *m, const int *t, int n) { (void)m;(void)t;(void)n; }
static inline void nova_hebbian_decay(NovaModel *m) { (void)m; }
static inline NovaBenchmark nova_benchmark(NovaModel *m, int s, int n) {
    (void)m;(void)s;(void)n; NovaBenchmark b={0}; return b; }
static inline void nova_benchmark_print(const NovaBenchmark *b) { (void)b; }
static inline bool nova_validate_tokens(const int *t, int n, int v) { (void)t;(void)n;(void)v; return false; }
static inline bool nova_check_bounds(const NovaModel *m) { (void)m; return false; }
static inline bool nova_sanitize_graph_input(char *s, int max) { (void)s;(void)max; return false; }
static inline int  nova_generate_next(NovaModel *m, const int *t, int n, float temp) {
    (void)m;(void)t;(void)n;(void)temp; return 0; }
static inline int  nova_train_full(NovaModel *m, NCTokenizer *tok, const char **texts, int n, NovaTrainConfig c) {
    (void)m;(void)tok;(void)texts;(void)n;(void)c; NC_NOVA_UNAVAILABLE(); return -1; }
static inline NovaTrainConfig nova_train_default_config(void) { NovaTrainConfig c={0}; return c; }

/* ---- Training / Reasoning / Distill stubs ------------------------------ */
static inline NCTrainConfig nc_train_default_config(void) { NCTrainConfig c={0}; return c; }
static inline void nc_nova_reasoning(NovaModel *m, const char *p, char *out, int max) {
    (void)m;(void)p;(void)out;(void)max; NC_NOVA_UNAVAILABLE(); }
static inline NCDistillConfig nc_distill_default_config(void) { NCDistillConfig c={0}; return c; }
static inline int nc_distill(NCModel *m, NCTokenizer *tok, NCDistillConfig c) {
    (void)m;(void)tok;(void)c; NC_NOVA_UNAVAILABLE(); return -1; }
static inline NCReasonConfig nc_reason_default_config(void) { NCReasonConfig c={0}; return c; }
static inline NCReasonEngine *nc_reason_create(NCReasonConfig c) {
    (void)c; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline NCReasonChain *nc_reason_query(NCReasonEngine *e, const char *q) {
    (void)e;(void)q; NC_NOVA_UNAVAILABLE(); return NULL; }
static inline void nc_reason_free(NCReasonEngine *e) { (void)e; }
static inline NCReasonChain nc_reason_run(NCReasonEngine *e, const char *p) {
    (void)e;(void)p; NCReasonChain r={0}; return r; }
static inline NCMathResult nc_math_eval(const char *expr) {
    (void)expr; NCMathResult r={0}; return r; }
static inline NCMathResult nc_math_evaluate(const char *expr) {
    (void)expr; NCMathResult r={0}; return r; }

/* ---- Declarations for functions implemented in nc_generate_stubs.c ---- */
/* These were previously declared in nc_nova_reasoning.h / nc_training.h.  */

/* nc_reason_chain_* and nc_reason_stats_print */
void nc_reason_chain_print(const NCReasonChain *chain);
void nc_reason_chain_free(NCReasonChain *chain);
void nc_reason_stats_print(const NCReasonEngine *engine);

/* Prompt builders (used by nc_interp.c and main.c) */
int nc_reason_build_prompt(const char *query, char *out, int out_size,
                           bool is_agent, bool chain_of_thought);
int nc_reason_build_agent_prompt(const char *purpose, const char *task,
                                 const char *tool_descriptions,
                                 char *out, int out_size);
int nc_reason_build_grounded_prompt(const char *query, const char *url,
                                    char *out, int out_size);

/* Training helpers (used by main.c and nc_jit.c) */
void nc_train(NCModel *model, NCTokenizer *tokenizer,
              const char **data_files, int n_files,
              NCTrainConfig config);
int nc_learn_from_project(NCModel *model, NCTokenizer *tokenizer,
                          const char *project_dir, NCTrainConfig config);

#ifdef __cplusplus
}
#endif

#endif /* NC_NOVA_STUBS_H */
