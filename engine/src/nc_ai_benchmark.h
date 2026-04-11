#ifndef NC_AI_BENCHMARK_H
#define NC_AI_BENCHMARK_H

#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════
 *  NC AI Benchmark Suite — Measure AI Performance
 *
 *  Run: nc ai benchmark
 *  Measures: latency, quality, throughput
 *
 *  Copyright 2026 DevHeal Labs AI. All rights reserved.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *prompt;
    const char *category;       /* "codegen", "completion", "reasoning" */
    int         expected_min_len; /* minimum acceptable output length */
} NCBenchQuery;

typedef struct {
    double  latency_ms;
    int     output_len;         /* chars generated */
    int     tokens_generated;
    float   confidence;
    bool    passed;             /* met minimum length */
} NCBenchResult;

typedef struct {
    int     n_queries;
    int     n_passed;
    double  avg_latency_ms;
    double  p50_latency_ms;
    double  p99_latency_ms;
    double  min_latency_ms;
    double  max_latency_ms;
    double  total_time_ms;
    int     total_tokens;
    double  tokens_per_second;
    float   pass_rate;          /* n_passed / n_queries */
} NCBenchSummary;

void nc_benchmark_run(void);     /* run full benchmark, print report */

#endif
