/*
 * nc_cuda.c — NVIDIA CUDA GPU acceleration via dynamic library loading.
 *
 * This file dynamically loads CUDA and cuBLAS at runtime using
 * LoadLibrary/dlopen, so NO CUDA toolkit is needed at compile time.
 * Only the NVIDIA display driver needs to be installed.
 *
 * On Windows: loads nvcuda.dll + cublas64_*.dll
 * On Linux:   loads libcuda.so + libcublas.so
 *
 * If loading fails, all functions return false and the engine
 * falls back to CPU (BLAS/SIMD) transparently.
 *
 * The approach:
 *   1. nc_cuda_init() loads libraries + resolves function pointers
 *   2. Allocates persistent GPU buffers (A, B, C) sized for max matrix
 *   3. nc_cuda_sgemm() copies data to GPU, runs cuBLAS, copies back
 *   4. nc_cuda_shutdown() frees GPU memory and unloads libraries
 */

#include "nc_cuda.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════
 *  Platform-specific dynamic loading
 * ═══════════════════════════════════════════════════════════ */

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  typedef HMODULE LibHandle;
  #define LIB_OPEN(name) LoadLibraryA(name)
  #define LIB_SYM(lib, name) (void *)GetProcAddress(lib, name)
  #define LIB_CLOSE(lib) FreeLibrary(lib)
  #define LIB_ERR() "LoadLibrary failed"
#else
  #include <dlfcn.h>
  typedef void *LibHandle;
  #define LIB_OPEN(name) dlopen(name, RTLD_LAZY)
  #define LIB_SYM(lib, name) dlsym(lib, name)
  #define LIB_CLOSE(lib) dlclose(lib)
  #define LIB_ERR() dlerror()
#endif

/* ═══════════════════════════════════════════════════════════
 *  CUDA types and enums (from cuda.h / cublas_v2.h)
 *  Defined here to avoid needing CUDA headers.
 * ═══════════════════════════════════════════════════════════ */

typedef int CUresult;
typedef int CUdevice;
typedef void *CUcontext;

typedef void *cublasHandle_t;

/* CUDA error codes */
#define CUDA_SUCCESS 0

/* cuBLAS status */
typedef int cublasStatus_t;
#define CUBLAS_STATUS_SUCCESS 0

/* cuBLAS operation type */
typedef int cublasOperation_t;
#define CUBLAS_OP_N 0
#define CUBLAS_OP_T 1

/* ═══════════════════════════════════════════════════════════
 *  Function pointer types for dynamically loaded APIs
 * ═══════════════════════════════════════════════════════════ */

/* CUDA Driver API */
typedef CUresult (*pfn_cuInit)(unsigned int);
typedef CUresult (*pfn_cuDeviceGet)(CUdevice *, int);
typedef CUresult (*pfn_cuDeviceGetName)(char *, int, CUdevice);
typedef CUresult (*pfn_cuDeviceGetCount)(int *);
typedef CUresult (*pfn_cuDeviceTotalMem)(size_t *, CUdevice);
typedef CUresult (*pfn_cuDeviceGetAttribute)(int *, int, CUdevice);
typedef CUresult (*pfn_cuCtxCreate)(CUcontext *, unsigned int, CUdevice);
typedef CUresult (*pfn_cuCtxDestroy)(CUcontext);
typedef CUresult (*pfn_cuMemAlloc)(void **, size_t);
typedef CUresult (*pfn_cuMemFree)(void *);
typedef CUresult (*pfn_cuMemcpyHtoD)(void *, const void *, size_t);
typedef CUresult (*pfn_cuMemcpyDtoH)(void *, const void *, size_t);
typedef CUresult (*pfn_cuCtxSynchronize)(void);

/* cuBLAS API */
typedef cublasStatus_t (*pfn_cublasCreate)(cublasHandle_t *);
typedef cublasStatus_t (*pfn_cublasDestroy)(cublasHandle_t);
typedef cublasStatus_t (*pfn_cublasSgemm)(cublasHandle_t, cublasOperation_t, cublasOperation_t,
                                          int, int, int,
                                          const float *, const float *, int,
                                          const float *, int,
                                          const float *, float *, int);

/* CUDA Driver API — module/kernel loading for PTX */
typedef void *CUmodule;
typedef void *CUfunction;
typedef CUresult (*pfn_cuModuleLoadData)(CUmodule *, const void *);
typedef CUresult (*pfn_cuModuleUnload)(CUmodule);
typedef CUresult (*pfn_cuModuleGetFunction)(CUfunction *, CUmodule, const char *);
typedef CUresult (*pfn_cuLaunchKernel)(CUfunction, unsigned, unsigned, unsigned,
                                       unsigned, unsigned, unsigned,
                                       unsigned, void *, void **, void **);

/* ═══════════════════════════════════════════════════════════
 *  Global state
 * ═══════════════════════════════════════════════════════════ */

static struct {
    bool initialized;
    bool available;

    /* Library handles */
    LibHandle cuda_lib;
    LibHandle cublas_lib;

    /* CUDA driver functions */
    pfn_cuInit              cuInit;
    pfn_cuDeviceGet         cuDeviceGet;
    pfn_cuDeviceGetName     cuDeviceGetName;
    pfn_cuDeviceGetCount    cuDeviceGetCount;
    pfn_cuDeviceTotalMem    cuDeviceTotalMem;
    pfn_cuDeviceGetAttribute cuDeviceGetAttribute;
    pfn_cuCtxCreate         cuCtxCreate;
    pfn_cuCtxDestroy        cuCtxDestroy;
    pfn_cuMemAlloc          cuMemAlloc;
    pfn_cuMemFree           cuMemFree;
    pfn_cuMemcpyHtoD        cuMemcpyHtoD;
    pfn_cuMemcpyDtoH        cuMemcpyDtoH;
    pfn_cuCtxSynchronize    cuCtxSynchronize;

    /* cuBLAS functions */
    pfn_cublasCreate        cublasCreate;
    pfn_cublasDestroy       cublasDestroy;
    pfn_cublasSgemm         cublasSgemm;

    /* CUDA kernel loading (for PTX-based sgemm without cuBLAS) */
    pfn_cuModuleLoadData    cuModuleLoadData;
    pfn_cuModuleUnload      cuModuleUnload;
    pfn_cuModuleGetFunction cuModuleGetFunction;
    pfn_cuLaunchKernel      cuLaunchKernel;

    /* GPU state */
    CUcontext context;
    cublasHandle_t cublas_handle;
    CUdevice device;

    /* PTX kernel state */
    CUmodule ptx_module;
    CUfunction sgemm_kernel;

    /* Persistent GPU buffers for matrix multiply */
    void *d_A;          /* Device buffer for matrix A */
    void *d_B;          /* Device buffer for matrix B */
    void *d_C;          /* Device buffer for matrix C */
    size_t buf_A_size;  /* Current buffer sizes */
    size_t buf_B_size;
    size_t buf_C_size;

    /* Device info */
    char device_name[256];
    size_t total_mem;
    int compute_major;
    int compute_minor;
    char info_str[512];
} cuda_state = {0};

/* ═══════════════════════════════════════════════════════════
 *  Library loading helpers
 * ═══════════════════════════════════════════════════════════ */

static bool load_cuda_driver(void) {
#ifdef _WIN32
    cuda_state.cuda_lib = LIB_OPEN("nvcuda.dll");
#else
    cuda_state.cuda_lib = LIB_OPEN("libcuda.so.1");
    if (!cuda_state.cuda_lib)
        cuda_state.cuda_lib = LIB_OPEN("libcuda.so");
#endif
    if (!cuda_state.cuda_lib) return false;

    /* Resolve all CUDA driver functions */
    #define LOAD_CUDA(name) \
        cuda_state.name = (pfn_##name)LIB_SYM(cuda_state.cuda_lib, #name); \
        if (!cuda_state.name) { \
            fprintf(stderr, "[NC CUDA] Failed to load: %s\n", #name); \
            LIB_CLOSE(cuda_state.cuda_lib); \
            cuda_state.cuda_lib = NULL; \
            return false; \
        }

    LOAD_CUDA(cuInit);
    LOAD_CUDA(cuDeviceGet);
    LOAD_CUDA(cuDeviceGetName);
    LOAD_CUDA(cuDeviceGetCount);
    LOAD_CUDA(cuDeviceTotalMem);
    LOAD_CUDA(cuDeviceGetAttribute);
    LOAD_CUDA(cuCtxCreate);
    LOAD_CUDA(cuCtxDestroy);
    LOAD_CUDA(cuMemAlloc);
    LOAD_CUDA(cuMemFree);
    LOAD_CUDA(cuMemcpyHtoD);
    LOAD_CUDA(cuMemcpyDtoH);
    LOAD_CUDA(cuCtxSynchronize);

    /* Kernel loading functions (for PTX-based sgemm) */
    cuda_state.cuModuleLoadData = (pfn_cuModuleLoadData)LIB_SYM(cuda_state.cuda_lib, "cuModuleLoadData");
    cuda_state.cuModuleUnload = (pfn_cuModuleUnload)LIB_SYM(cuda_state.cuda_lib, "cuModuleUnload");
    cuda_state.cuModuleGetFunction = (pfn_cuModuleGetFunction)LIB_SYM(cuda_state.cuda_lib, "cuModuleGetFunction");
    cuda_state.cuLaunchKernel = (pfn_cuLaunchKernel)LIB_SYM(cuda_state.cuda_lib, "cuLaunchKernel");

    #undef LOAD_CUDA
    return true;
}

static bool load_cublas(void) {
#ifdef _WIN32
    /* Try common cuBLAS DLL names (version varies with driver) */
    const char *cublas_names[] = {
        "cublas64_12.dll", "cublas64_11.dll", "cublas64_10.dll",
        "cublas64_100.dll", "cublas64_90.dll",
        "cublasLt64_12.dll", "cublasLt64_11.dll",
        NULL
    };
    for (int i = 0; cublas_names[i]; i++) {
        cuda_state.cublas_lib = LIB_OPEN(cublas_names[i]);
        if (cuda_state.cublas_lib) break;
    }
#else
    cuda_state.cublas_lib = LIB_OPEN("libcublas.so.12");
    if (!cuda_state.cublas_lib)
        cuda_state.cublas_lib = LIB_OPEN("libcublas.so.11");
    if (!cuda_state.cublas_lib)
        cuda_state.cublas_lib = LIB_OPEN("libcublas.so");
#endif
    if (!cuda_state.cublas_lib) return false;

    /* Resolve cuBLAS functions */
    #define LOAD_CUBLAS(name) \
        cuda_state.name = (pfn_##name)LIB_SYM(cuda_state.cublas_lib, #name "_v2"); \
        if (!cuda_state.name) \
            cuda_state.name = (pfn_##name)LIB_SYM(cuda_state.cublas_lib, #name); \
        if (!cuda_state.name) { \
            fprintf(stderr, "[NC CUDA] Failed to load cuBLAS: %s\n", #name); \
            LIB_CLOSE(cuda_state.cublas_lib); \
            cuda_state.cublas_lib = NULL; \
            return false; \
        }

    LOAD_CUBLAS(cublasCreate);
    LOAD_CUBLAS(cublasDestroy);
    LOAD_CUBLAS(cublasSgemm);

    #undef LOAD_CUBLAS
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  PTX-based SGEMM kernel (no CUDA toolkit needed)
 *
 *  Embedded PTX assembly for matrix multiply on any SM >= 3.0.
 *  Each thread computes one element of C using a dot-product loop.
 *  For 7.9M param model matrices (~256x256), this is fast enough.
 *
 *  PTX is NVIDIA's virtual instruction set — loaded at runtime
 *  by the CUDA driver and JIT-compiled to the target GPU.
 * ═══════════════════════════════════════════════════════════ */

static const char nc_sgemm_ptx[] =
    ".version 6.0\n"
    ".target sm_30\n"
    ".address_size 64\n"
    "\n"
    "// sgemm_kernel(float *A, float *B, float *C, int M, int K, int N)\n"
    ".visible .entry sgemm_kernel(\n"
    "    .param .u64 param_A,\n"
    "    .param .u64 param_B,\n"
    "    .param .u64 param_C,\n"
    "    .param .u32 param_M,\n"
    "    .param .u32 param_K,\n"
    "    .param .u32 param_N\n"
    ")\n"
    "{\n"
    "    .reg .u32 %r<20>;\n"
    "    .reg .u64 %rd<20>;\n"
    "    .reg .f32 %f<8>;\n"
    "    .reg .pred %p<4>;\n"
    "\n"
    "    // row = blockIdx.y * blockDim.y + threadIdx.y\n"
    "    mov.u32 %r0, %ctaid.y;\n"
    "    mov.u32 %r1, %ntid.y;\n"
    "    mul.lo.u32 %r2, %r0, %r1;\n"
    "    mov.u32 %r3, %tid.y;\n"
    "    add.u32 %r4, %r2, %r3;  // row\n"
    "\n"
    "    // col = blockIdx.x * blockDim.x + threadIdx.x\n"
    "    mov.u32 %r5, %ctaid.x;\n"
    "    mov.u32 %r6, %ntid.x;\n"
    "    mul.lo.u32 %r7, %r5, %r6;\n"
    "    mov.u32 %r8, %tid.x;\n"
    "    add.u32 %r9, %r7, %r8;  // col\n"
    "\n"
    "    // Load M, K, N\n"
    "    ld.param.u32 %r10, [param_M];\n"
    "    ld.param.u32 %r11, [param_K];\n"
    "    ld.param.u32 %r12, [param_N];\n"
    "\n"
    "    // Bounds check: if (row >= M || col >= N) return\n"
    "    setp.ge.u32 %p0, %r4, %r10;\n"
    "    setp.ge.u32 %p1, %r9, %r12;\n"
    "    or.pred %p2, %p0, %p1;\n"
    "    @%p2 bra DONE;\n"
    "\n"
    "    // Load base pointers\n"
    "    ld.param.u64 %rd0, [param_A];\n"
    "    ld.param.u64 %rd1, [param_B];\n"
    "    ld.param.u64 %rd2, [param_C];\n"
    "\n"
    "    // sum = 0.0\n"
    "    mov.f32 %f0, 0f00000000;\n"
    "\n"
    "    // Loop: for k = 0; k < K; k++\n"
    "    mov.u32 %r13, 0;  // k = 0\n"
    "LOOP:\n"
    "    setp.ge.u32 %p3, %r13, %r11; // k >= K?\n"
    "    @%p3 bra STORE;\n"
    "\n"
    "    // A[row * K + k]\n"
    "    mul.lo.u32 %r14, %r4, %r11;  // row * K\n"
    "    add.u32 %r14, %r14, %r13;     // + k\n"
    "    mul.wide.u32 %rd3, %r14, 4;   // * sizeof(float)\n"
    "    add.u64 %rd4, %rd0, %rd3;     // &A[row*K+k]\n"
    "    ld.global.f32 %f1, [%rd4];\n"
    "\n"
    "    // B[k * N + col]\n"
    "    mul.lo.u32 %r15, %r13, %r12;  // k * N\n"
    "    add.u32 %r15, %r15, %r9;       // + col\n"
    "    mul.wide.u32 %rd5, %r15, 4;\n"
    "    add.u64 %rd6, %rd1, %rd5;     // &B[k*N+col]\n"
    "    ld.global.f32 %f2, [%rd6];\n"
    "\n"
    "    // sum += A_val * B_val\n"
    "    fma.rn.f32 %f0, %f1, %f2, %f0;\n"
    "\n"
    "    add.u32 %r13, %r13, 1; // k++\n"
    "    bra LOOP;\n"
    "\n"
    "STORE:\n"
    "    // C[row * N + col] = sum\n"
    "    mul.lo.u32 %r16, %r4, %r12;  // row * N\n"
    "    add.u32 %r16, %r16, %r9;      // + col\n"
    "    mul.wide.u32 %rd7, %r16, 4;\n"
    "    add.u64 %rd8, %rd2, %rd7;     // &C[row*N+col]\n"
    "    st.global.f32 [%rd8], %f0;\n"
    "\n"
    "DONE:\n"
    "    ret;\n"
    "}\n";

static bool load_ptx_sgemm(void) {
    if (!cuda_state.cuModuleLoadData || !cuda_state.cuModuleGetFunction ||
        !cuda_state.cuLaunchKernel) {
        return false;
    }

    CUresult err = cuda_state.cuModuleLoadData(&cuda_state.ptx_module, nc_sgemm_ptx);
    if (err != CUDA_SUCCESS) {
        fprintf(stderr, "[NC CUDA] PTX module load failed: %d\n", err);
        cuda_state.ptx_module = NULL;
        return false;
    }

    err = cuda_state.cuModuleGetFunction(&cuda_state.sgemm_kernel, cuda_state.ptx_module, "sgemm_kernel");
    if (err != CUDA_SUCCESS) {
        fprintf(stderr, "[NC CUDA] Failed to find sgemm_kernel in PTX: %d\n", err);
        cuda_state.cuModuleUnload(cuda_state.ptx_module);
        cuda_state.ptx_module = NULL;
        return false;
    }

    fprintf(stderr, "[NC CUDA] PTX sgemm kernel loaded — native GPU matmul active\n");
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  GPU buffer management — auto-resize persistent buffers
 * ═══════════════════════════════════════════════════════════ */

static bool ensure_gpu_buffer(void **d_buf, size_t *cur_size, size_t need) {
    if (*cur_size >= need) return true;

    /* Free old buffer */
    if (*d_buf) {
        cuda_state.cuMemFree(*d_buf);
        *d_buf = NULL;
        *cur_size = 0;
    }

    /* Allocate with 25% headroom to reduce reallocations */
    size_t alloc_size = need + (need >> 2);

    /* Safety: don't exceed 75% of GPU memory */
    size_t max_alloc = (cuda_state.total_mem * 3) / 4;
    if (alloc_size > max_alloc) {
        alloc_size = need;  /* Try exact size */
        if (alloc_size > max_alloc) {
            fprintf(stderr, "[NC CUDA] Matrix too large for GPU: %zu bytes (GPU has %zu MB)\n",
                    need, cuda_state.total_mem / (1024 * 1024));
            return false;
        }
    }

    CUresult err = cuda_state.cuMemAlloc(d_buf, alloc_size);
    if (err != CUDA_SUCCESS) {
        fprintf(stderr, "[NC CUDA] cuMemAlloc failed: %d (requested %zu bytes)\n",
                err, alloc_size);
        *d_buf = NULL;
        return false;
    }
    *cur_size = alloc_size;
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Public API
 * ═══════════════════════════════════════════════════════════ */

bool nc_cuda_init(void) {
    if (cuda_state.initialized) return cuda_state.available;
    cuda_state.initialized = true;
    cuda_state.available = false;

    /* Load CUDA driver library */
    if (!load_cuda_driver()) {
        fprintf(stderr, "[NC CUDA] NVIDIA driver not found — using CPU\n");
        return false;
    }

    /* Initialize CUDA */
    CUresult err = cuda_state.cuInit(0);
    if (err != CUDA_SUCCESS) {
        fprintf(stderr, "[NC CUDA] cuInit failed: %d\n", err);
        LIB_CLOSE(cuda_state.cuda_lib);
        cuda_state.cuda_lib = NULL;
        return false;
    }

    /* Check device count */
    int device_count = 0;
    cuda_state.cuDeviceGetCount(&device_count);
    if (device_count == 0) {
        fprintf(stderr, "[NC CUDA] No CUDA-capable GPU found\n");
        LIB_CLOSE(cuda_state.cuda_lib);
        cuda_state.cuda_lib = NULL;
        return false;
    }

    /* Get device 0 */
    cuda_state.cuDeviceGet(&cuda_state.device, 0);
    cuda_state.cuDeviceGetName(cuda_state.device_name, sizeof(cuda_state.device_name), cuda_state.device);
    cuda_state.cuDeviceTotalMem(&cuda_state.total_mem, cuda_state.device);

    /* Compute capability */
    cuda_state.cuDeviceGetAttribute(&cuda_state.compute_major, 75 /* CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR */, cuda_state.device);
    cuda_state.cuDeviceGetAttribute(&cuda_state.compute_minor, 76 /* CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR */, cuda_state.device);

    /* Create context */
    err = cuda_state.cuCtxCreate(&cuda_state.context, 0, cuda_state.device);
    if (err != CUDA_SUCCESS) {
        fprintf(stderr, "[NC CUDA] cuCtxCreate failed: %d\n", err);
        LIB_CLOSE(cuda_state.cuda_lib);
        cuda_state.cuda_lib = NULL;
        return false;
    }

    /* Load cuBLAS */
    if (!load_cublas()) {
        fprintf(stderr, "[NC CUDA] cuBLAS not found — trying native PTX sgemm\n");
        /* Load our embedded PTX kernel as fallback */
        if (!load_ptx_sgemm()) {
            fprintf(stderr, "[NC CUDA] PTX kernel also failed — GPU sgemm disabled\n");
        }
    }

    /* Create cuBLAS handle if available */
    if (cuda_state.cublas_lib) {
        cublasStatus_t status = cuda_state.cublasCreate(&cuda_state.cublas_handle);
        if (status != CUBLAS_STATUS_SUCCESS) {
            fprintf(stderr, "[NC CUDA] cublasCreate failed: %d\n", status);
            cuda_state.cublas_handle = NULL;
        }
    }

    /* Build info string */
    const char *sgemm_status = cuda_state.cublas_handle ? "cuBLAS" :
                               cuda_state.sgemm_kernel ? "PTX" : "disabled";
    snprintf(cuda_state.info_str, sizeof(cuda_state.info_str),
             "%s (%.0f MB, SM %d.%d, sgemm: %s)",
             cuda_state.device_name,
             (double)cuda_state.total_mem / (1024.0 * 1024.0),
             cuda_state.compute_major, cuda_state.compute_minor,
             sgemm_status);

    fprintf(stderr, "[NC CUDA] GPU initialized: %s\n", cuda_state.info_str);
    cuda_state.available = true;
    return true;
}

void nc_cuda_shutdown(void) {
    if (!cuda_state.initialized) return;

    /* Free GPU buffers */
    if (cuda_state.d_A) { cuda_state.cuMemFree(cuda_state.d_A); cuda_state.d_A = NULL; }
    if (cuda_state.d_B) { cuda_state.cuMemFree(cuda_state.d_B); cuda_state.d_B = NULL; }
    if (cuda_state.d_C) { cuda_state.cuMemFree(cuda_state.d_C); cuda_state.d_C = NULL; }

    /* Destroy cuBLAS handle */
    if (cuda_state.cublas_handle) {
        cuda_state.cublasDestroy(cuda_state.cublas_handle);
        cuda_state.cublas_handle = NULL;
    }

    /* Unload PTX module */
    if (cuda_state.ptx_module && cuda_state.cuModuleUnload) {
        cuda_state.cuModuleUnload(cuda_state.ptx_module);
        cuda_state.ptx_module = NULL;
        cuda_state.sgemm_kernel = NULL;
    }

    /* Destroy CUDA context */
    if (cuda_state.context) {
        cuda_state.cuCtxDestroy(cuda_state.context);
        cuda_state.context = NULL;
    }

    /* Unload libraries */
    if (cuda_state.cublas_lib) { LIB_CLOSE(cuda_state.cublas_lib); cuda_state.cublas_lib = NULL; }
    if (cuda_state.cuda_lib) { LIB_CLOSE(cuda_state.cuda_lib); cuda_state.cuda_lib = NULL; }

    cuda_state.initialized = false;
    cuda_state.available = false;
    fprintf(stderr, "[NC CUDA] GPU shutdown complete\n");
}

bool nc_cuda_available(void) {
    return cuda_state.available;
}

const char *nc_cuda_device_info(void) {
    return cuda_state.available ? cuda_state.info_str : "No CUDA GPU";
}

/* ═══════════════════════════════════════════════════════════
 *  GPU matrix multiply via cuBLAS
 *
 *  C = A @ B   where A:[M,K] B:[K,N] C:[M,N]
 *
 *  cuBLAS uses column-major, but we store row-major.
 *  Trick: compute B^T * A^T = (A*B)^T in col-major,
 *  which reads our row-major data as already-transposed col-major.
 *
 *  So we call: cublasSgemm(N, N, N, M, K, 1, B, N, A, K, 0, C, N)
 * ═══════════════════════════════════════════════════════════ */

bool nc_cuda_sgemm(int M, int K, int N,
                   const float *A, const float *B, float *C) {
    if (!cuda_state.available) return false;
    if (!cuda_state.cublas_handle && !cuda_state.sgemm_kernel) return false;

    /* Skip tiny matrices — GPU overhead not worth it */
    if (M * K < 4096 && K * N < 4096) return false;

    size_t size_A = (size_t)M * K * sizeof(float);
    size_t size_B = (size_t)K * N * sizeof(float);
    size_t size_C = (size_t)M * N * sizeof(float);

    /* Ensure GPU buffers are large enough */
    if (!ensure_gpu_buffer(&cuda_state.d_A, &cuda_state.buf_A_size, size_A)) return false;
    if (!ensure_gpu_buffer(&cuda_state.d_B, &cuda_state.buf_B_size, size_B)) return false;
    if (!ensure_gpu_buffer(&cuda_state.d_C, &cuda_state.buf_C_size, size_C)) return false;

    /* Copy input matrices to GPU */
    CUresult err;
    err = cuda_state.cuMemcpyHtoD(cuda_state.d_A, A, size_A);
    if (err != CUDA_SUCCESS) return false;
    err = cuda_state.cuMemcpyHtoD(cuda_state.d_B, B, size_B);
    if (err != CUDA_SUCCESS) return false;

    if (cuda_state.cublas_handle) {
        /* cuBLAS path: col-major trick for row-major data */
        float alpha = 1.0f, beta = 0.0f;
        cublasStatus_t status = cuda_state.cublasSgemm(
            cuda_state.cublas_handle,
            CUBLAS_OP_N, CUBLAS_OP_N,
            N, M, K,
            &alpha,
            (const float *)cuda_state.d_B, N,
            (const float *)cuda_state.d_A, K,
            &beta,
            (float *)cuda_state.d_C, N
        );
        if (status != CUBLAS_STATUS_SUCCESS) {
            fprintf(stderr, "[NC CUDA] cublasSgemm failed: %d\n", status);
            return false;
        }
    } else {
        /* PTX kernel path — launch our embedded sgemm */
        void *d_A_ptr = cuda_state.d_A;
        void *d_B_ptr = cuda_state.d_B;
        void *d_C_ptr = cuda_state.d_C;
        void *args[] = { &d_A_ptr, &d_B_ptr, &d_C_ptr, &M, &K, &N };

        /* Block: 16x16 threads, Grid: ceil(N/16) x ceil(M/16) */
        unsigned bx = 16, by = 16;
        unsigned gx = (N + bx - 1) / bx;
        unsigned gy = (M + by - 1) / by;

        err = cuda_state.cuLaunchKernel(
            cuda_state.sgemm_kernel,
            gx, gy, 1,       /* grid dim */
            bx, by, 1,       /* block dim */
            0, NULL,          /* shared mem, stream */
            args, NULL        /* kernel args */
        );
        if (err != CUDA_SUCCESS) {
            fprintf(stderr, "[NC CUDA] PTX sgemm launch failed: %d\n", err);
            return false;
        }
        cuda_state.cuCtxSynchronize();
    }

    /* Copy result back to CPU */
    err = cuda_state.cuMemcpyDtoH(C, cuda_state.d_C, size_C);
    if (err != CUDA_SUCCESS) return false;

    return true;
}

bool nc_cuda_sgemm_batched(int batch, int M, int K, int N,
                           const float *A, const float *B, float *C) {
    if (!cuda_state.available || !cuda_state.cublas_handle) return false;

    /* For each batch element, run sgemm */
    size_t stride_A = (size_t)M * K;
    size_t stride_B = (size_t)K * N;
    size_t stride_C = (size_t)M * N;

    for (int b = 0; b < batch; b++) {
        if (!nc_cuda_sgemm(M, K, N,
                           A + b * stride_A,
                           B + b * stride_B,
                           C + b * stride_C)) {
            return false;
        }
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Element-wise GPU operations (using CUDA driver API)
 *  These use simple kernel-less approaches via cuBLAS axipy/scal
 * ═══════════════════════════════════════════════════════════ */

bool nc_cuda_vadd(const float *A, const float *B, float *C, int n) {
    /* For element-wise add, CPU is fast enough for our model size.
     * GPU transfer overhead dominates for vectors < 1M elements.
     * Only accelerate if we add dedicated CUDA kernels later. */
    (void)A; (void)B; (void)C; (void)n;
    return false;  /* Let CPU handle it */
}

bool nc_cuda_vsmul(const float *A, float scalar, float *B, int n) {
    (void)A; (void)scalar; (void)B; (void)n;
    return false;  /* Let CPU handle it */
}
