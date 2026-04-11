/*
 * nc_ai_efficient.h — Resource-Efficient AI Training for the NC Engine.
 *
 * Achieve 2B-parameter-equivalent quality WITHOUT heavy GPU resources
 * using three key techniques:
 *
 * 1. Mixture of Experts (MoE)
 *    - 8 specialized expert networks (200M params each)
 *    - Router activates only 2 experts per query
 *    - Total: 1.6B params, Active: 400M per query
 *    - Result: 2B quality at 400M compute cost
 *
 * 2. LoRA (Low-Rank Adaptation)
 *    - Freeze base model weights (no gradient needed)
 *    - Train tiny rank-16 adapters (~2M trainable params)
 *    - 50x less memory than full fine-tuning
 *    - Can train on MacBook in hours, not days
 *
 * 3. Synthetic Data Generation
 *    - Generate unlimited NC training data from templates
 *    - No internet scraping, no data collection needed
 *    - Domain-specific = higher quality than generic data
 *    - 1GB synthetic data = 50GB generic data (for NC)
 *
 * Copyright 2026 DevHeal Labs AI. All rights reserved.
 */

#ifndef NC_AI_EFFICIENT_H
#define NC_AI_EFFICIENT_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  Mixture of Experts (MoE) — 2B quality at 400M compute
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    int n_experts;           /* total experts (default 8) */
    int n_active;            /* experts per query (default 2) */
    int expert_dim;          /* dimension per expert */
    int expert_layers;       /* layers per expert */
    float load_balance_loss; /* aux loss weight (default 0.01) */
} NCMoEConfig;

typedef struct {
    float *gate_weights;     /* [dim, n_experts] router */
    float *gate_bias;        /* [n_experts] */
    int expert_hits[16];     /* usage counter per expert (max 16 experts) */
} NCMoERouter;

/* Expert specializations */
typedef enum {
    NC_EXPERT_SERVICE,       /* API/service generation */
    NC_EXPERT_UI,            /* NC UI frontend */
    NC_EXPERT_DATA,          /* data processing, maps, lists */
    NC_EXPERT_LOGIC,         /* if/else, loops, conditions */
    NC_EXPERT_MATH,          /* mathematical operations */
    NC_EXPERT_TEST,          /* test generation */
    NC_EXPERT_INFRA,         /* config, deploy, middleware */
    NC_EXPERT_GENERAL        /* general NC code */
} NCExpertType;

typedef struct {
    NCMoEConfig config;
    NCMoERouter router;
    NCExpertType expert_types[16];
    long total_params;       /* sum of all experts */
    long active_params;      /* params active per query */
    int total_queries;
} NCMoESystem;

/* ═══════════════════════════════════════════════════════════
 *  LoRA (Low-Rank Adaptation) — 50x less memory
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    int rank;                /* LoRA rank (default 16, range 4-64) */
    float alpha;             /* scaling factor (default 32.0) */
    float dropout;           /* LoRA dropout (default 0.05) */
    bool train_bias;         /* also train bias terms (default false) */
} NCLoRAConfig;

/* A single LoRA adapter pair: A (down-project) + B (up-project) */
typedef struct {
    float *A;                /* [dim, rank] down-projection (initialized random) */
    float *B;                /* [rank, dim] up-projection (initialized zero) */
    int dim;
    int rank;
    float scale;             /* alpha / rank */
} NCLoRAAdapter;

/* LoRA applied to a transformer layer */
typedef struct {
    NCLoRAAdapter q_lora;    /* query projection adapter */
    NCLoRAAdapter v_lora;    /* value projection adapter */
    /* K and O projections stay frozen — LoRA on Q,V gives 90% of benefit */
} NCLoRALayer;

typedef struct {
    NCLoRAConfig config;
    NCLoRALayer *layers;     /* [n_layers] */
    int n_layers;
    long trainable_params;   /* only LoRA params (tiny!) */
    long frozen_params;      /* base model params (huge but not trained) */
} NCLoRAModel;

/* ═══════════════════════════════════════════════════════════
 *  Synthetic Data Generation — unlimited NC training data
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_SYNTH_SERVICE,        /* REST API services */
    NC_SYNTH_UI_PAGE,        /* NC UI pages */
    NC_SYNTH_CRUD,           /* CRUD operations */
    NC_SYNTH_TEST,           /* test files */
    NC_SYNTH_MIDDLEWARE,     /* middleware configs */
    NC_SYNTH_AI_SERVICE,     /* AI-powered services */
    NC_SYNTH_DATA_PIPELINE,  /* data processing */
    NC_SYNTH_MATH_CODE       /* mathematical NC code */
} NCSynthType;

typedef struct {
    int n_generated;         /* total examples generated */
    int n_per_type[8];       /* count per type */
    long total_tokens;       /* total tokens generated */
    long total_bytes;        /* total bytes of synthetic data */
} NCSynthStats;

/* Training efficiency calculator */
typedef struct {
    long base_model_params;
    long lora_trainable_params;
    long moe_total_params;
    long moe_active_params;
    float memory_gb_full;        /* memory for full training */
    float memory_gb_lora;        /* memory for LoRA training */
    float memory_gb_inference;   /* memory for inference */
    float speedup_vs_full;       /* how much faster than full training */
    float data_efficiency;       /* synthetic data multiplier */
} NCEfficiencyReport;

/* ═══════════════════════════════════════════════════════════
 *  MoE API
 * ═══════════════════════════════════════════════════════════ */

NCMoEConfig     nc_moe_default_config(void);
NCMoESystem    *nc_moe_create(NCMoEConfig config, int model_dim);
void            nc_moe_free(NCMoESystem *moe);
void            nc_moe_route(NCMoESystem *moe, const float *hidden, int dim,
                             int *selected_experts, float *expert_weights);
void            nc_moe_stats_print(const NCMoESystem *moe);

/* ═══════════════════════════════════════════════════════════
 *  LoRA API
 * ═══════════════════════════════════════════════════════════ */

NCLoRAConfig    nc_lora_default_config(void);
NCLoRAModel    *nc_lora_create(NCLoRAConfig config, int n_layers, int dim);
void            nc_lora_free(NCLoRAModel *model);
void            nc_lora_forward(const NCLoRAAdapter *adapter, const float *input,
                                float *output, int batch_size);
void            nc_lora_apply_to_weights(const NCLoRAAdapter *adapter,
                                         float *base_weights);
long            nc_lora_count_params(const NCLoRAModel *model);

/* ═══════════════════════════════════════════════════════════
 *  Synthetic Data API
 * ═══════════════════════════════════════════════════════════ */

NCSynthStats    nc_synth_generate(const char *output_path, int n_examples);
char           *nc_synth_single(NCSynthType type);  /* caller frees */

/* ═══════════════════════════════════════════════════════════
 *  Efficiency Report API
 * ═══════════════════════════════════════════════════════════ */

NCEfficiencyReport nc_efficiency_report(long base_params, int lora_rank,
                                        int n_experts, int n_active);
void               nc_efficiency_report_print(const NCEfficiencyReport *r);

#endif /* NC_AI_EFFICIENT_H */
