/*
 * nc_metal_noop.c — Metal GPU stubs for non-Apple platforms.
 *
 * On macOS, nc_metal.m provides the real Metal implementation.
 * On Linux/Windows, this file provides no-op stubs so nc_model.c
 * can call nc_metal_sgemm() etc. without link errors.
 *
 * This file is ONLY compiled on non-Darwin platforms.
 */

#include "nc_metal.h"

bool nc_metal_init(void) { return false; }
void nc_metal_shutdown(void) {}
bool nc_metal_available(void) { return false; }

bool nc_metal_sgemm(int M, int K, int N,
                    const float *A, const float *B, float *C) {
    (void)M; (void)K; (void)N; (void)A; (void)B; (void)C;
    return false;
}

bool nc_metal_sgemm_batched(int batch, int M, int K, int N,
                            const float *A, const float *B, float *C) {
    (void)batch; (void)M; (void)K; (void)N; (void)A; (void)B; (void)C;
    return false;
}

bool nc_metal_vadd(const float *A, const float *B, float *C, int n) {
    (void)A; (void)B; (void)C; (void)n;
    return false;
}

bool nc_metal_vsmul(const float *A, float scalar, float *B, int n) {
    (void)A; (void)scalar; (void)B; (void)n;
    return false;
}
