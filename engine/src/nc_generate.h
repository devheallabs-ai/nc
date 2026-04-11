/*
 * nc_generate.h — Code generation from natural language descriptions.
 *
 * Connects the NC AI model to the NC CLI so users can generate
 * NC code from plain English descriptions.
 *
 * Two modes:
 *   1. Template engine (always available, no model needed)
 *   2. Neural model (when ~/.nc/model/nc_model.bin exists)
 *
 * No external dependencies — pure C11 standard library.
 */

#ifndef NC_GENERATE_H
#define NC_GENERATE_H

#include "nc_model.h"
#include "nc_tokenizer.h"
#include "nc_training.h"

/* ═══════════════════════════════════════════════════════════
 *  Generation config
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    float temperature;   /* 0.7 default                                  */
    int   top_k;         /* 16                                           */
    float top_p;         /* 0.85                                         */
    int   max_tokens;    /* 1024                                         */
    int   validate;      /* 1 = validate generated code through compiler */
    int   repair_attempts;         /* retry weak drafts with stricter prompts */
    int   allow_template_fallback; /* 1 = use templates when AI still fails   */
} NCGenerateConfig;

static inline NCGenerateConfig nc_generate_default_config(void) {
    NCGenerateConfig c;
    c.temperature = 0.7f;
    c.top_k       = 16;
    c.top_p       = 0.85f;
    c.max_tokens  = 1024;
    c.validate    = 1;
    c.repair_attempts = 3;
    c.allow_template_fallback = 1;
    return c;
}

/* ═══════════════════════════════════════════════════════════
 *  Template types
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    NC_TMPL_SERVICE,      /* Backend API service      */
    NC_TMPL_NCUI_PAGE,    /* Frontend NC UI page      */
    NC_TMPL_FULL_APP,     /* Backend + Frontend       */
    NC_TMPL_CRUD,         /* CRUD API                 */
    NC_TMPL_AI_SERVICE,   /* AI-powered service       */
    NC_TMPL_MIDDLEWARE,    /* Middleware config         */
    NC_TMPL_TEST,         /* Test file                */
} NCTemplateType;

/* ═══════════════════════════════════════════════════════════
 *  Parsed user intent
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NCTemplateType  type;
    char           *name;          /* service/page name         */
    char          **features;      /* extracted feature strings  */
    int             n_features;
    char           *theme;         /* for UI: "dark" or "light" */
    char           *description;   /* original description       */
} NCIntent;

/* ═══════════════════════════════════════════════════════════
 *  Generator
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NCModel      *model;       /* neural model (if loaded)          */
    NCTokenizer  *tokenizer;   /* tokenizer  (if loaded)            */
    int           use_model;   /* 1 = neural, 0 = template          */
    NCGenerateConfig config;
} NCGenerator;

NCGenerator *nc_generator_create(void);
void         nc_generator_free(NCGenerator *gen);

/* ═══════════════════════════════════════════════════════════
 *  Intent parsing (rule-based NLU)
 * ═══════════════════════════════════════════════════════════ */

NCIntent *nc_parse_intent(const char *description);
void      nc_intent_free(NCIntent *intent);

/* ═══════════════════════════════════════════════════════════
 *  Code generation
 * ═══════════════════════════════════════════════════════════ */

char *nc_generate_code(NCGenerator *gen, const char *description);
char *nc_generate_from_intent(NCGenerator *gen, NCIntent *intent);

/* ═══════════════════════════════════════════════════════════
 *  Template-based generation (always available, no model)
 * ═══════════════════════════════════════════════════════════ */

char *nc_template_service(NCIntent *intent);
char *nc_template_ncui_page(NCIntent *intent);
char *nc_template_full_app(NCIntent *intent);
char *nc_template_crud(NCIntent *intent);
char *nc_template_ai_service(NCIntent *intent);
char *nc_template_middleware(NCIntent *intent);
char *nc_template_test(NCIntent *intent);

/* ═══════════════════════════════════════════════════════════
 *  Validation
 * ═══════════════════════════════════════════════════════════ */

int nc_validate_generated(const char *code, const char *type);

/* Shared neural-output cleanup helpers used by project generation paths. */
char *nc_generate_decode_hardened_alloc(const NCTokenizer *tok,
                                        const int *tokens,
                                        int n_tokens);
char *nc_generate_postprocess_model_output(const char *code,
                                           NCTemplateType type);

/* ═══════════════════════════════════════════════════════════
 *  CLI entry points — called from main.c
 * ═══════════════════════════════════════════════════════════ */

int nc_cmd_generate(int argc, char **argv);
int nc_cmd_train(int argc, char **argv);

#endif /* NC_GENERATE_H */
