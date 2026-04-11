/*
 * nc_metal.h — Metal GPU acceleration for NC model operations.
 *
 * Uses Apple Metal Performance Shaders (MPS) for GPU-accelerated
 * matrix multiplication on macOS/iOS with Apple Silicon.
 *
 * Falls back gracefully to CPU BLAS when Metal is unavailable.
 */

#ifndef NC_METAL_H
#define NC_METAL_H

#include <stdbool.h>

/* Initialize Metal GPU compute pipeline. Returns true if GPU available. */
bool nc_metal_init(void);

/* Shutdown Metal and release GPU resources. */
void nc_metal_shutdown(void);

/* Check if Metal GPU is available and initialized. */
bool nc_metal_available(void);

/* GPU-accelerated matrix multiply: C = A @ B
 * A: [M, K]  B: [K, N]  C: [M, N]
 * All arrays must be host (CPU) memory — data is copied to/from GPU.
 * Returns true on success, false on failure (caller should fallback to CPU). */
bool nc_metal_sgemm(int M, int K, int N,
                    const float *A, const float *B, float *C);

/* GPU-accelerated batched matmul: C[b] = A[b] @ B[b] for b in [0, batch) */
bool nc_metal_sgemm_batched(int batch, int M, int K, int N,
                            const float *A, const float *B, float *C);

/* GPU element-wise add: C = A + B (same size) */
bool nc_metal_vadd(const float *A, const float *B, float *C, int n);

/* GPU scalar multiply: B = A * scalar */
bool nc_metal_vsmul(const float *A, float scalar, float *B, int n);

#endif /* NC_METAL_H */
