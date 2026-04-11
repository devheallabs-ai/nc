/*
 * nc_cuda.h — NVIDIA CUDA GPU acceleration for NC model operations.
 *
 * Uses dynamic loading of CUDA/cuBLAS libraries so no CUDA toolkit
 * is required at compile time. Only the NVIDIA driver (which includes
 * nvcuda.dll / cublas64_*.dll) needs to be installed.
 *
 * Falls back gracefully to CPU when CUDA is unavailable.
 *
 * Mirrors the nc_metal.h interface for consistent GPU dispatch.
 */

#ifndef NC_CUDA_H
#define NC_CUDA_H

#include <stdbool.h>

/* Initialize CUDA GPU pipeline via dynamic library loading.
 * Loads nvcuda.dll + cublas64_*.dll at runtime.
 * Returns true if GPU is available and libraries loaded. */
bool nc_cuda_init(void);

/* Shutdown CUDA and release GPU resources. */
void nc_cuda_shutdown(void);

/* Check if CUDA GPU is available and initialized. */
bool nc_cuda_available(void);

/* Get GPU info string (name, memory, compute capability). */
const char *nc_cuda_device_info(void);

/* GPU-accelerated matrix multiply: C = A @ B
 * A: [M, K]  B: [K, N]  C: [M, N]
 * All arrays must be host (CPU) memory — data is copied to/from GPU.
 * Returns true on success, false on failure (caller should fallback to CPU). */
bool nc_cuda_sgemm(int M, int K, int N,
                   const float *A, const float *B, float *C);

/* GPU-accelerated batched matmul: C[b] = A[b] @ B[b] for b in [0, batch) */
bool nc_cuda_sgemm_batched(int batch, int M, int K, int N,
                           const float *A, const float *B, float *C);

/* GPU element-wise add: C = A + B (same size) */
bool nc_cuda_vadd(const float *A, const float *B, float *C, int n);

/* GPU scalar multiply: B = A * scalar */
bool nc_cuda_vsmul(const float *A, float scalar, float *B, int n);

#endif /* NC_CUDA_H */
