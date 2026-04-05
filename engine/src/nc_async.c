/*
 * nc_async.c — Thread pool, async/await, and concurrency for NC.
 *
 * Performance philosophy: NC should be as fast as C allows.
 *
 * This file provides:
 *   - Thread pool with per-worker deques + work-stealing
 *   - Parallel map/gather (fan-out N tasks, join results)
 *   - Parallel for-each (partition list across cores)
 *   - Coroutines (cooperative multitasking)
 *   - Event loop (non-blocking I/O scheduling)
 *   - Generators (lazy iteration)
 *   - Promise/future with chaining, all, and race
 *
 * Work-stealing design: each worker has its own lock-free deque.
 * Tasks are pushed to the submitting worker's deque; idle workers
 * steal from the tail of other workers' deques. This reduces
 * contention vs. a single global queue, matching designs in
 * Cilk, Go's scheduler, and Java ForkJoinPool.
 *
 * The thread pool starts lazily on first use. Worker count
 * defaults to the number of CPU cores, configurable via
 * NC_WORKERS env var.
 */

#include "../include/nc.h"
#include "../include/nc_platform.h"
#include <stdatomic.h>

/* ═══════════════════════════════════════════════════════════
 *  Thread Pool — per-worker deques with work-stealing
 *
 *  Each worker owns a deque (double-ended queue). The owner
 *  pushes/pops from the bottom (LIFO — cache-friendly), while
 *  thieves steal from the top (FIFO — load-balancing). A
 *  global queue catches overflow and external submissions.
 *
 *  Fallback: if a worker's local deque is full, the task goes
 *  to the global queue. Workers check global queue last.
 * ═══════════════════════════════════════════════════════════ */

#define NC_DEQUE_CAP    1024
#define NC_GLOBAL_CAP   4096

typedef struct {
    NcTaskFunc  func;
    void       *arg;
    NcValue    *result_slot;
    nc_mutex_t *done_mutex;
    nc_cond_t  *done_cond;
    bool       *done_flag;
} PoolTask;

typedef struct {
    PoolTask        tasks[NC_DEQUE_CAP];
    atomic_int      bottom;       /* owner push/pop end   */
    atomic_int      top;          /* thief steal end      */
    nc_mutex_t      steal_lock;   /* protects steal CAS   */
} WorkDeque;

static void deque_init(WorkDeque *d) {
    atomic_store(&d->bottom, 0);
    atomic_store(&d->top, 0);
    nc_mutex_init(&d->steal_lock);
}

static void deque_destroy(WorkDeque *d) {
    nc_mutex_destroy(&d->steal_lock);
}

static bool deque_push(WorkDeque *d, PoolTask task) {
    int b = atomic_load(&d->bottom);
    int t = atomic_load(&d->top);
    if (b - t >= NC_DEQUE_CAP) return false;
    d->tasks[b % NC_DEQUE_CAP] = task;
    atomic_thread_fence(memory_order_release);
    atomic_store(&d->bottom, b + 1);
    return true;
}

static bool deque_pop(WorkDeque *d, PoolTask *out) {
    int b = atomic_load(&d->bottom) - 1;
    atomic_store(&d->bottom, b);
    atomic_thread_fence(memory_order_seq_cst);
    int t = atomic_load(&d->top);
    if (t <= b) {
        *out = d->tasks[b % NC_DEQUE_CAP];
        if (t == b) {
            if (!atomic_compare_exchange_strong(&d->top, &t, t + 1)) {
                atomic_store(&d->bottom, b + 1);
                return false;
            }
            atomic_store(&d->bottom, b + 1);
        }
        return true;
    }
    atomic_store(&d->bottom, b + 1);
    return false;
}

static bool deque_steal(WorkDeque *d, PoolTask *out) {
    nc_mutex_lock(&d->steal_lock);
    int t = atomic_load(&d->top);
    atomic_thread_fence(memory_order_seq_cst);
    int b = atomic_load(&d->bottom);
    if (t < b) {
        *out = d->tasks[t % NC_DEQUE_CAP];
        if (atomic_compare_exchange_strong(&d->top, &t, t + 1)) {
            nc_mutex_unlock(&d->steal_lock);
            return true;
        }
    }
    nc_mutex_unlock(&d->steal_lock);
    return false;
}

typedef struct {
    nc_thread_t    *threads;
    int             thread_count;
    WorkDeque      *deques;
    PoolTask        global_queue[NC_GLOBAL_CAP];
    int             global_head;
    int             global_tail;
    int             global_size;
    nc_mutex_t      global_mutex;
    nc_cond_t       global_cond;
    bool            shutdown;
    bool            initialized;
} NcThreadPool;

static NcThreadPool pool = {0};
static nc_thread_local int tl_worker_id = -1;

static void execute_task(PoolTask *task) {
    NcValue result = task->func(task->arg);
    if (task->result_slot) *task->result_slot = result;
    if (task->done_flag) {
        nc_mutex_lock(task->done_mutex);
        *task->done_flag = true;
        nc_cond_signal(task->done_cond);
        nc_mutex_unlock(task->done_mutex);
    }
}

static bool try_get_task(int worker_id, PoolTask *out) {
    /* 1) Pop from own deque (LIFO — cache-friendly) */
    if (deque_pop(&pool.deques[worker_id], out))
        return true;

    /* 2) Steal from a random peer (FIFO — load-balance) */
    int n = pool.thread_count;
    int start = (worker_id + 1) % n;
    for (int i = 0; i < n - 1; i++) {
        int victim = (start + i) % n;
        if (deque_steal(&pool.deques[victim], out))
            return true;
    }

    /* 3) Check global overflow queue */
    nc_mutex_lock(&pool.global_mutex);
    if (pool.global_size > 0) {
        *out = pool.global_queue[pool.global_head];
        pool.global_head = (pool.global_head + 1) % NC_GLOBAL_CAP;
        pool.global_size--;
        nc_mutex_unlock(&pool.global_mutex);
        return true;
    }
    nc_mutex_unlock(&pool.global_mutex);
    return false;
}

#ifdef NC_WINDOWS
static unsigned __stdcall pool_worker(void *arg) {
#else
static void *pool_worker(void *arg) {
#endif
    int id = (int)(intptr_t)arg;
    tl_worker_id = id;

    for (;;) {
        PoolTask task;
        if (try_get_task(id, &task)) {
            execute_task(&task);
            continue;
        }

        nc_mutex_lock(&pool.global_mutex);
        if (pool.shutdown) {
            nc_mutex_unlock(&pool.global_mutex);
            break;
        }
        nc_cond_timedwait(&pool.global_cond, &pool.global_mutex, 1);
        nc_mutex_unlock(&pool.global_mutex);

        if (pool.shutdown) break;
    }
#ifdef NC_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

void nc_pool_init(void) {
    if (pool.initialized) return;

    const char *env = getenv("NC_WORKERS");
    int n = env ? atoi(env) : nc_cpu_count();
    if (n < 1) n = 1;
    if (n > 256) n = 256;

    pool.deques = calloc(n, sizeof(WorkDeque));
    if (!pool.deques) return;

    for (int i = 0; i < n; i++)
        deque_init(&pool.deques[i]);

    nc_mutex_init(&pool.global_mutex);
    nc_cond_init(&pool.global_cond);
    pool.global_head = 0;
    pool.global_tail = 0;
    pool.global_size = 0;
    pool.shutdown = false;
    pool.thread_count = n;
    pool.threads = calloc(n, sizeof(nc_thread_t));
    if (!pool.threads) {
        for (int i = 0; i < n; i++) deque_destroy(&pool.deques[i]);
        free(pool.deques);
        pool.deques = NULL;
        nc_mutex_destroy(&pool.global_mutex);
        nc_cond_destroy(&pool.global_cond);
        return;
    }

    for (int i = 0; i < n; i++)
        nc_thread_create(&pool.threads[i], (nc_thread_func_t)pool_worker, (void *)(intptr_t)i);

    pool.initialized = true;
}

void nc_pool_shutdown(void) {
    if (!pool.initialized) return;

    nc_mutex_lock(&pool.global_mutex);
    pool.shutdown = true;
    nc_cond_broadcast(&pool.global_cond);
    nc_mutex_unlock(&pool.global_mutex);

    for (int i = 0; i < pool.thread_count; i++)
        nc_thread_join(pool.threads[i]);

    for (int i = 0; i < pool.thread_count; i++)
        deque_destroy(&pool.deques[i]);

    free(pool.deques);
    free(pool.threads);
    nc_mutex_destroy(&pool.global_mutex);
    nc_cond_destroy(&pool.global_cond);
    pool.initialized = false;
}

int nc_pool_worker_count(void) {
    if (!pool.initialized) nc_pool_init();
    return pool.thread_count;
}

static void pool_submit_task(NcTaskFunc func, void *arg,
                             NcValue *result_slot,
                             nc_mutex_t *done_mutex,
                             nc_cond_t *done_cond,
                             bool *done_flag) {
    if (!pool.initialized) nc_pool_init();

    PoolTask t = {
        .func = func, .arg = arg, .result_slot = result_slot,
        .done_mutex = done_mutex, .done_cond = done_cond,
        .done_flag = done_flag,
    };

    /* Try local deque first (if called from a worker thread) */
    if (tl_worker_id >= 0 && deque_push(&pool.deques[tl_worker_id], t)) {
        nc_cond_signal(&pool.global_cond);
        return;
    }

    /* Round-robin into a worker deque */
    static atomic_int rr_counter = 0;
    int start = atomic_fetch_add(&rr_counter, 1) % pool.thread_count;
    for (int i = 0; i < pool.thread_count; i++) {
        if (deque_push(&pool.deques[(start + i) % pool.thread_count], t)) {
            nc_cond_signal(&pool.global_cond);
            return;
        }
    }

    /* All deques full — push to global overflow queue */
    nc_mutex_lock(&pool.global_mutex);
    while (pool.global_size >= NC_GLOBAL_CAP) {
        nc_mutex_unlock(&pool.global_mutex);
        nc_sleep_ms(1);
        nc_mutex_lock(&pool.global_mutex);
    }
    pool.global_queue[pool.global_tail] = t;
    pool.global_tail = (pool.global_tail + 1) % NC_GLOBAL_CAP;
    pool.global_size++;
    nc_cond_signal(&pool.global_cond);
    nc_mutex_unlock(&pool.global_mutex);
}

/* ═══════════════════════════════════════════════════════════
 *  Parallel map — fan out N tasks, collect results
 *
 *  Used by: parallel gather, parallel repeat for each,
 *  parallel find_similar, etc.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_parallel_map(NcTaskFunc func, void **args, int count) {
    if (count <= 0) return NC_LIST(nc_list_new());
    if (!pool.initialized) nc_pool_init();

    NcValue *results = calloc(count, sizeof(NcValue));
    bool *dones = calloc(count, sizeof(bool));
    nc_mutex_t *mutexes = calloc(count, sizeof(nc_mutex_t));
    nc_cond_t *conds = calloc(count, sizeof(nc_cond_t));

    for (int i = 0; i < count; i++) {
        dones[i] = false;
        results[i] = NC_NONE();
        nc_mutex_init(&mutexes[i]);
        nc_cond_init(&conds[i]);
        pool_submit_task(func, args[i], &results[i],
                         &mutexes[i], &conds[i], &dones[i]);
    }

    NcList *list = nc_list_new();
    for (int i = 0; i < count; i++) {
        nc_mutex_lock(&mutexes[i]);
        while (!dones[i])
            nc_cond_wait(&conds[i], &mutexes[i]);
        nc_mutex_unlock(&mutexes[i]);
        nc_list_push(list, results[i]);
        nc_mutex_destroy(&mutexes[i]);
        nc_cond_destroy(&conds[i]);
    }

    free(results);
    free(dones);
    free(mutexes);
    free(conds);
    return NC_LIST(list);
}

/* ═══════════════════════════════════════════════════════════
 *  Parallel cosine similarity — partition vectors across cores
 *
 *  For RAG workloads with thousands of embeddings, this turns
 *  O(n*d) into O(n*d / cores).
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcList *query_vec;
    NcList *all_vectors;
    int     start;
    int     end;
    double *scores;
} SimChunk;

static NcValue sim_worker(void *arg) {
    SimChunk *c = (SimChunk *)arg;
    int qdims = c->query_vec->count;
    for (int i = c->start; i < c->end; i++) {
        NcValue vec_val = c->all_vectors->items[i];
        if (!IS_LIST(vec_val)) { c->scores[i] = 0; continue; }
        NcList *vec = AS_LIST(vec_val);
        int dims = qdims < vec->count ? qdims : vec->count;
        double dot = 0, mag_q = 0, mag_v = 0;
        for (int d = 0; d < dims; d++) {
            double q = IS_FLOAT(c->query_vec->items[d]) ? AS_FLOAT(c->query_vec->items[d]) :
                       IS_INT(c->query_vec->items[d]) ? (double)AS_INT(c->query_vec->items[d]) : 0;
            double v = IS_FLOAT(vec->items[d]) ? AS_FLOAT(vec->items[d]) :
                       IS_INT(vec->items[d]) ? (double)AS_INT(vec->items[d]) : 0;
            dot += q * v;
            mag_q += q * q;
            mag_v += v * v;
        }
        c->scores[i] = (mag_q > 0 && mag_v > 0) ? dot / (sqrt(mag_q) * sqrt(mag_v)) : 0;
    }
    return NC_NONE();
}

NcValue nc_parallel_find_similar(NcList *query_vec, NcList *all_vectors,
                                 NcList *documents, int top_k) {
    if (!query_vec || !all_vectors || all_vectors->count == 0 || top_k <= 0)
        return NC_LIST(nc_list_new());

    int n = all_vectors->count;
    int has_docs = (documents && documents->count == n);

    if (!pool.initialized) nc_pool_init();
    int nworkers = pool.thread_count;
    if (nworkers > n) nworkers = n;

    double *scores = calloc(n, sizeof(double));
    SimChunk *chunks = calloc(nworkers, sizeof(SimChunk));
    void **chunk_ptrs = calloc(nworkers, sizeof(void *));

    int per_worker = n / nworkers;
    int remainder = n % nworkers;
    int offset = 0;

    for (int i = 0; i < nworkers; i++) {
        chunks[i].query_vec = query_vec;
        chunks[i].all_vectors = all_vectors;
        chunks[i].scores = scores;
        chunks[i].start = offset;
        int chunk_size = per_worker + (i < remainder ? 1 : 0);
        chunks[i].end = offset + chunk_size;
        offset += chunk_size;
        chunk_ptrs[i] = &chunks[i];
    }

    nc_parallel_map(sim_worker, chunk_ptrs, nworkers);

    /* Top-K selection */
    int k = top_k < n ? top_k : n;
    int *indices = calloc(n, sizeof(int));
    for (int i = 0; i < n; i++) indices[i] = i;

    for (int i = 0; i < k; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (scores[j] > scores[best]) best = j;
        if (best != i) {
            double ts = scores[i]; scores[i] = scores[best]; scores[best] = ts;
            int ti = indices[i]; indices[i] = indices[best]; indices[best] = ti;
        }
    }

    NcList *result = nc_list_new();
    for (int i = 0; i < k; i++) {
        NcList *pair = nc_list_new();
        nc_list_push(pair, NC_FLOAT(scores[i]));
        if (has_docs)
            nc_list_push(pair, documents->items[indices[i]]);
        else
            nc_list_push(pair, NC_INT(indices[i]));
        nc_list_push(result, NC_LIST(pair));
    }

    free(scores);
    free(indices);
    free(chunks);
    free(chunk_ptrs);
    return NC_LIST(result);
}

/* ═══════════════════════════════════════════════════════════
 *  Parallel HTTP gather — fan out multiple HTTP requests
 *
 *  When NC encounters multiple independent gather statements
 *  (or a gather with multiple sources), fire them all at once.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    const char *source;
    NcMap      *options;
} GatherArg;

static NcValue gather_worker(void *arg) {
    GatherArg *ga = (GatherArg *)arg;
    return nc_gather_from(ga->source, ga->options);
}

NcValue nc_parallel_gather(const char **sources, NcMap **options_list,
                            int count) {
    if (count <= 0) return NC_LIST(nc_list_new());

    GatherArg *args = calloc(count, sizeof(GatherArg));
    void **arg_ptrs = calloc(count, sizeof(void *));

    for (int i = 0; i < count; i++) {
        args[i].source = sources[i];
        args[i].options = options_list[i];
        arg_ptrs[i] = &args[i];
    }

    NcValue result = nc_parallel_map(gather_worker, arg_ptrs, count);
    free(args);
    free(arg_ptrs);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  Parallel for-each — partition list across worker threads
 *
 *  Used by "repeat for each" when the list is large enough
 *  to benefit from parallelism. The caller provides a function
 *  that processes one element and returns a result. Results
 *  are collected in order.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcTaskFunc      func;
    NcValue        *items;
    NcValue        *results;
    int             start;
    int             end;
} ForEachChunk;

static NcValue foreach_chunk_worker(void *arg) {
    ForEachChunk *c = (ForEachChunk *)arg;
    for (int i = c->start; i < c->end; i++)
        c->results[i] = c->func(&c->items[i]);
    return NC_NONE();
}

NcValue nc_parallel_for_each(NcTaskFunc func, NcList *items) {
    if (!items || items->count == 0) return NC_LIST(nc_list_new());
    if (!pool.initialized) nc_pool_init();

    int n = items->count;
    int nworkers = pool.thread_count;
    if (nworkers > n) nworkers = n;

    NcValue *results = calloc(n, sizeof(NcValue));
    for (int i = 0; i < n; i++) results[i] = NC_NONE();

    ForEachChunk *chunks = calloc(nworkers, sizeof(ForEachChunk));
    void **chunk_ptrs = calloc(nworkers, sizeof(void *));

    int per_worker = n / nworkers;
    int remainder = n % nworkers;
    int offset = 0;

    for (int i = 0; i < nworkers; i++) {
        chunks[i].func = func;
        chunks[i].items = items->items;
        chunks[i].results = results;
        chunks[i].start = offset;
        int chunk_size = per_worker + (i < remainder ? 1 : 0);
        chunks[i].end = offset + chunk_size;
        offset += chunk_size;
        chunk_ptrs[i] = &chunks[i];
    }

    nc_parallel_map(foreach_chunk_worker, chunk_ptrs, nworkers);

    NcList *result_list = nc_list_new();
    for (int i = 0; i < n; i++)
        nc_list_push(result_list, results[i]);

    free(results);
    free(chunks);
    free(chunk_ptrs);
    return NC_LIST(result_list);
}

/* ═══════════════════════════════════════════════════════════
 *  Coroutine — lightweight cooperative task using ucontext
 *
 *  Real stackful coroutines: each coroutine gets its own stack
 *  and can yield/resume at any point. This enables non-blocking
 *  AI calls — the coroutine yields while waiting for a response,
 *  and the thread pool picks up other work.
 * ═══════════════════════════════════════════════════════════ */

#define CORO_STACK_SIZE (64 * 1024)

typedef enum {
    CORO_READY,
    CORO_RUNNING,
    CORO_SUSPENDED,
    CORO_FINISHED,
} CoroState;

typedef void (*NcCoroFunc)(void *arg);

#ifdef _WIN32
/* Windows: use fibers for coroutines */
#include <windows.h>
typedef struct NcCoroutine {
    int             id;
    CoroState       state;
    NcValue         result;
    NcChunk        *chunk;
    int             ip;
    NcValue         stack[256];
    int             stack_top;
    NcMap          *locals;
    struct NcCoroutine *next;
    void           *fiber;
    void           *caller_fiber;
    char           *coro_stack;
    NcCoroFunc      func;
    void           *func_arg;
} NcCoroutine;
#elif defined(__APPLE__)
/* macOS: use setjmp/longjmp — ucontext is deprecated by Apple.
 * We manually set up each coroutine's stack pointer in the jmp_buf. */
#include <setjmp.h>
typedef struct NcCoroutine {
    int             id;
    CoroState       state;
    NcValue         result;
    NcChunk        *chunk;
    int             ip;
    NcValue         stack[256];
    int             stack_top;
    NcMap          *locals;
    struct NcCoroutine *next;
    jmp_buf         ctx;
    jmp_buf         caller_ctx;
    char           *coro_stack;
    NcCoroFunc      func;
    void           *func_arg;
    bool            ctx_initialized;
} NcCoroutine;
#else
/* Linux/POSIX: use ucontext for coroutines */
#define _XOPEN_SOURCE 600
#include <ucontext.h>
typedef struct NcCoroutine {
    int             id;
    CoroState       state;
    NcValue         result;
    NcChunk        *chunk;
    int             ip;
    NcValue         stack[256];
    int             stack_top;
    NcMap          *locals;
    struct NcCoroutine *next;
    ucontext_t      ctx;
    ucontext_t      caller_ctx;
    char           *coro_stack;
    NcCoroFunc      func;
    void           *func_arg;
} NcCoroutine;
#endif

static NcCoroutine *coro_queue = NULL;
static int coro_id_counter = 0;
static nc_thread_local NcCoroutine *current_coro = NULL;

void coro_entry(void) {
    NcCoroutine *c = current_coro;
    if (c && c->func) {
        c->func(c->func_arg);
    }
    c->state = CORO_FINISHED;
#ifdef _WIN32
    SwitchToFiber(c->caller_fiber);
#elif defined(__APPLE__)
    longjmp(c->caller_ctx, 1);
#else
    swapcontext(&c->ctx, &c->caller_ctx);
#endif
}

NcCoroutine *nc_coro_create(NcChunk *chunk) {
    NcCoroutine *c = calloc(1, sizeof(NcCoroutine));
    c->id = coro_id_counter++;
    c->state = CORO_READY;
    c->chunk = chunk;
    c->ip = 0;
    c->stack_top = 0;
    c->locals = nc_map_new();
    c->result = NC_NONE();
    c->func = NULL;
    c->func_arg = NULL;

    c->coro_stack = malloc(CORO_STACK_SIZE);
#ifdef _WIN32
    c->fiber = CreateFiber(CORO_STACK_SIZE, (LPFIBER_START_ROUTINE)coro_entry, c);
#elif defined(__APPLE__)
    c->ctx_initialized = false;
#else
    getcontext(&c->ctx);
    c->ctx.uc_stack.ss_sp = c->coro_stack;
    c->ctx.uc_stack.ss_size = CORO_STACK_SIZE;
    c->ctx.uc_link = &c->caller_ctx;
    makecontext(&c->ctx, (void (*)(void))coro_entry, 0);
#endif

    c->next = coro_queue;
    coro_queue = c;
    return c;
}

NcCoroutine *nc_coro_create_func(NcCoroFunc func, void *arg) {
    NcCoroutine *c = calloc(1, sizeof(NcCoroutine));
    c->id = coro_id_counter++;
    c->state = CORO_READY;
    c->locals = nc_map_new();
    c->result = NC_NONE();
    c->func = func;
    c->func_arg = arg;

    c->coro_stack = malloc(CORO_STACK_SIZE);
#ifdef _WIN32
    c->fiber = CreateFiber(CORO_STACK_SIZE, (LPFIBER_START_ROUTINE)coro_entry, c);
#elif defined(__APPLE__)
    c->ctx_initialized = false;
#else
    getcontext(&c->ctx);
    c->ctx.uc_stack.ss_sp = c->coro_stack;
    c->ctx.uc_stack.ss_size = CORO_STACK_SIZE;
    c->ctx.uc_link = &c->caller_ctx;
    makecontext(&c->ctx, (void (*)(void))coro_entry, 0);
#endif

    c->next = coro_queue;
    coro_queue = c;
    return c;
}

void nc_coro_yield(NcCoroutine *c, NcValue value) {
    c->state = CORO_SUSPENDED;
    c->result = value;
#ifdef _WIN32
    SwitchToFiber(c->caller_fiber);
#elif defined(__APPLE__)
    if (setjmp(c->ctx) == 0)
        longjmp(c->caller_ctx, 1);
#else
    swapcontext(&c->ctx, &c->caller_ctx);
#endif
}

void nc_coro_yield_current(NcValue value) {
    if (current_coro)
        nc_coro_yield(current_coro, value);
}

NcValue nc_coro_resume(NcCoroutine *c) {
    if (c->state == CORO_FINISHED) return c->result;

    NcCoroutine *prev = current_coro;
    current_coro = c;
    c->state = CORO_RUNNING;

#ifdef _WIN32
    c->caller_fiber = GetCurrentFiber();
    SwitchToFiber(c->fiber);
#elif defined(__APPLE__)
    if (setjmp(c->caller_ctx) == 0) {
        if (!c->ctx_initialized) {
            /* First resume: bootstrap onto the coroutine's stack and call entry */
            c->ctx_initialized = true;
            char *stack_top = c->coro_stack + CORO_STACK_SIZE;
            /* Align stack to 16 bytes (required by ARM64 and x86_64 ABIs) */
            stack_top = (char *)((uintptr_t)stack_top & ~(uintptr_t)0xF);
            /* Switch stack pointer and call coro_entry.
             * We use inline assembly to pivot the stack; this works on
             * both x86_64 and ARM64 macOS. */
#if defined(__aarch64__)
            __asm__ volatile (
                "mov sp, %0\n\t"
                "bl _coro_entry\n\t"
                : : "r"(stack_top) : "memory"
            );
#elif defined(__x86_64__)
            __asm__ volatile (
                "movq %0, %%rsp\n\t"
                "callq _coro_entry\n\t"
                : : "r"(stack_top) : "memory"
            );
#endif
        } else {
            longjmp(c->ctx, 1);
        }
    }
#else
    swapcontext(&c->caller_ctx, &c->ctx);
#endif

    current_coro = prev;
    return c->result;
}

void nc_coro_free(NcCoroutine *c) {
    if (!c) return;
    /* Remove from queue */
    NcCoroutine **pp = &coro_queue;
    while (*pp) {
        if (*pp == c) { *pp = c->next; break; }
        pp = &(*pp)->next;
    }
    nc_map_free(c->locals);
    free(c->coro_stack);
    free(c);
}

/* ═══════════════════════════════════════════════════════════
 *  Event Loop — single-threaded async runtime
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    EVT_TIMER,
    EVT_IO,
    EVT_CALLBACK,
} EventType;

typedef struct NcEvent {
    EventType       type;
    double          fire_at;
    NcCoroutine    *coro;
    NcValue         data;
    struct NcEvent *next;
} NcEvent;

typedef struct {
    NcEvent *queue;
    int      count;
    bool     running;
    double   current_time;
} NcEventLoop;

static NcEventLoop event_loop = {0};

void nc_event_loop_init(void) {
    event_loop.queue = NULL;
    event_loop.count = 0;
    event_loop.running = false;
}

void nc_event_loop_schedule(NcCoroutine *coro, double delay_ms) {
    NcEvent *evt = calloc(1, sizeof(NcEvent));
    evt->type = EVT_TIMER;
    evt->fire_at = nc_realtime_ms() + delay_ms;
    evt->coro = coro;
    evt->next = event_loop.queue;
    event_loop.queue = evt;
    event_loop.count++;
}

int nc_event_loop_run(void) {
    event_loop.running = true;
    int processed = 0;

    while (event_loop.running && event_loop.queue) {
        double now = nc_realtime_ms();

        NcEvent **prev = &event_loop.queue;
        NcEvent *evt = event_loop.queue;

        while (evt) {
            if (evt->type == EVT_TIMER && now >= evt->fire_at) {
                if (evt->coro) nc_coro_resume(evt->coro);
                *prev = evt->next;
                NcEvent *to_free = evt;
                evt = evt->next;
                free(to_free);
                event_loop.count--;
                processed++;
            } else {
                prev = &evt->next;
                evt = evt->next;
            }
        }

        if (event_loop.queue) {
            nc_sleep_ms(1);
        }
    }
    return processed;
}

void nc_event_loop_stop(void) {
    event_loop.running = false;
}

/* ═══════════════════════════════════════════════════════════
 *  Worker Threads — parallel behavior execution (VM-level)
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcChunk *chunk;
    NcMap   *args;
    NcValue  result;
    bool     done;
} WorkerTask;

#ifdef NC_WINDOWS
static unsigned __stdcall vm_worker_thread(void *arg) {
#else
static void *vm_worker_thread(void *arg) {
#endif
    WorkerTask *task = (WorkerTask *)arg;
    NcVM *vm = nc_vm_new();
    task->result = nc_vm_execute(vm, task->chunk);
    task->done = true;
    nc_vm_free(vm);
#ifdef NC_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

NcValue nc_parallel_run(NcChunk **chunks, int count) {
    nc_thread_t *threads = calloc(count, sizeof(nc_thread_t));
    WorkerTask *tasks = calloc(count, sizeof(WorkerTask));

    for (int i = 0; i < count; i++) {
        tasks[i].chunk = chunks[i];
        tasks[i].done = false;
        nc_thread_create(&threads[i], (nc_thread_func_t)vm_worker_thread, &tasks[i]);
    }

    NcList *results = nc_list_new();
    for (int i = 0; i < count; i++) {
        nc_thread_join(threads[i]);
        nc_list_push(results, tasks[i].result);
    }

    free(threads);
    free(tasks);
    return NC_LIST(results);
}

/* ═══════════════════════════════════════════════════════════
 *  Generator — yield values one at a time
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    int      current;
    int      end;
    int      step;
} NcRangeGen;

NcRangeGen *nc_range_new(int start, int end, int step) {
    NcRangeGen *g = malloc(sizeof(NcRangeGen));
    g->current = start;
    g->end = end;
    g->step = step > 0 ? step : 1;
    return g;
}

bool nc_range_has_next(NcRangeGen *g) {
    return g->current < g->end;
}

int nc_range_next(NcRangeGen *g) {
    int val = g->current;
    g->current += g->step;
    return val;
}

void nc_range_free(NcRangeGen *g) { free(g); }

/* ═══════════════════════════════════════════════════════════
 *  Promise / Future — thread-safe async result container
 *
 *  Promises bridge the thread pool and the VM: an async task
 *  returns a promise that can be awaited (blocking with timeout),
 *  chained (.then/.catch), or composed (all/race).
 *
 *  All operations are mutex-protected. Resolve/reject are
 *  one-shot: the first call wins, subsequent calls are no-ops.
 * ═══════════════════════════════════════════════════════════ */

NcPromise *nc_promise_new(void) {
    NcPromise *p = calloc(1, sizeof(NcPromise));
    if (!p) return NULL;
    nc_mutex_init(&p->mutex);
    nc_cond_init(&p->cond);
    p->result = NC_NONE();
    p->resolved = false;
    p->rejected = false;
    p->error[0] = '\0';
    p->then_cb = NULL;
    p->then_ctx = NULL;
    p->then_promise = NULL;
    p->catch_cb = NULL;
    p->catch_ctx = NULL;
    p->catch_promise = NULL;
    return p;
}

void nc_promise_resolve(NcPromise *p, NcValue val) {
    if (!p) return;
    nc_mutex_lock(&p->mutex);
    if (p->resolved || p->rejected) {
        /* Already settled — ignore */
        nc_mutex_unlock(&p->mutex);
        return;
    }
    p->result = val;
    p->resolved = true;
    nc_cond_broadcast(&p->cond);

    /* Snapshot chained callbacks under lock, then invoke outside */
    NcPromiseCallback then_cb = p->then_cb;
    void *then_ctx = p->then_ctx;
    NcPromise *then_promise = p->then_promise;
    nc_mutex_unlock(&p->mutex);

    /* Fire .then() chain if registered */
    if (then_cb && then_promise) {
        NcValue chained = then_cb(val, then_ctx);
        nc_promise_resolve(then_promise, chained);
    }
}

void nc_promise_reject(NcPromise *p, const char *error) {
    if (!p) return;
    nc_mutex_lock(&p->mutex);
    if (p->resolved || p->rejected) {
        nc_mutex_unlock(&p->mutex);
        return;
    }
    p->rejected = true;
    if (error) {
        snprintf(p->error, sizeof(p->error), "%s", error);
    } else {
        snprintf(p->error, sizeof(p->error), "unknown error");
    }
    p->result = NC_NONE();
    nc_cond_broadcast(&p->cond);

    /* Snapshot chained callbacks under lock, then invoke outside */
    NcPromiseCallback catch_cb = p->catch_cb;
    void *catch_ctx = p->catch_ctx;
    NcPromise *catch_promise = p->catch_promise;

    NcPromiseCallback then_cb = p->then_cb;
    NcPromise *then_promise = p->then_promise;
    nc_mutex_unlock(&p->mutex);

    /* Fire .catch() chain if registered */
    if (catch_cb && catch_promise) {
        NcValue recovered = catch_cb(NC_STRING(nc_string_from_cstr(p->error)), catch_ctx);
        nc_promise_resolve(catch_promise, recovered);
    } else if (then_promise) {
        /* No catch handler — propagate rejection down the chain */
        nc_promise_reject(then_promise, p->error);
    }
}

NcValue nc_promise_await(NcPromise *p, int timeout_ms) {
    if (!p) return NC_NONE();
    nc_mutex_lock(&p->mutex);

    if (timeout_ms <= 0) {
        /* Wait indefinitely */
        while (!p->resolved && !p->rejected)
            nc_cond_wait(&p->cond, &p->mutex);
    } else {
        /* Wait with timeout — poll in chunks to handle spurious wakes */
        int remaining = timeout_ms;
        while (!p->resolved && !p->rejected && remaining > 0) {
            int chunk = remaining < 50 ? remaining : 50;
            nc_cond_timedwait(&p->cond, &p->mutex, chunk);
            remaining -= chunk;
        }
    }

    NcValue result = p->result;
    bool rejected = p->rejected;
    nc_mutex_unlock(&p->mutex);

    if (rejected) {
        /* Return an error map: {"error": "message"} */
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
                   NC_STRING(nc_string_from_cstr(p->error)));
        return NC_MAP(err);
    }
    if (!p->resolved) {
        /* Timeout — return an error map */
        NcMap *err = nc_map_new();
        nc_map_set(err, nc_string_from_cstr("error"),
                   NC_STRING(nc_string_from_cstr("promise await timed out")));
        return NC_MAP(err);
    }
    return result;
}

void nc_promise_free(NcPromise *p) {
    if (!p) return;
    nc_mutex_destroy(&p->mutex);
    nc_cond_destroy(&p->cond);
    free(p);
}

NcPromise *nc_promise_then(NcPromise *p, NcPromiseCallback cb, void *ctx) {
    if (!p || !cb) return NULL;
    NcPromise *next = nc_promise_new();
    if (!next) return NULL;

    nc_mutex_lock(&p->mutex);
    if (p->resolved) {
        /* Already resolved — invoke callback immediately */
        NcValue val = p->result;
        nc_mutex_unlock(&p->mutex);
        NcValue chained = cb(val, ctx);
        nc_promise_resolve(next, chained);
    } else if (p->rejected) {
        /* Already rejected — propagate rejection */
        nc_mutex_unlock(&p->mutex);
        nc_promise_reject(next, p->error);
    } else {
        /* Pending — store callback for later */
        p->then_cb = cb;
        p->then_ctx = ctx;
        p->then_promise = next;
        nc_mutex_unlock(&p->mutex);
    }
    return next;
}

NcPromise *nc_promise_catch(NcPromise *p, NcPromiseCallback cb, void *ctx) {
    if (!p || !cb) return NULL;
    NcPromise *next = nc_promise_new();
    if (!next) return NULL;

    nc_mutex_lock(&p->mutex);
    if (p->rejected) {
        /* Already rejected — invoke catch callback immediately */
        nc_mutex_unlock(&p->mutex);
        NcValue recovered = cb(NC_STRING(nc_string_from_cstr(p->error)), ctx);
        nc_promise_resolve(next, recovered);
    } else if (p->resolved) {
        /* Already resolved — pass through */
        NcValue val = p->result;
        nc_mutex_unlock(&p->mutex);
        nc_promise_resolve(next, val);
    } else {
        /* Pending — store callback for later */
        p->catch_cb = cb;
        p->catch_ctx = ctx;
        p->catch_promise = next;
        nc_mutex_unlock(&p->mutex);
    }
    return next;
}

/* ═══════════════════════════════════════════════════════════
 *  Async task wrapper — submit work to thread pool, get promise
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcAsyncFn   fn;
    void       *arg;
    NcPromise  *promise;
} AsyncRunCtx;

static NcValue async_run_worker(void *raw) {
    AsyncRunCtx *ctx = (AsyncRunCtx *)raw;
    NcValue result = ctx->fn(ctx->arg);
    nc_promise_resolve(ctx->promise, result);
    free(ctx);
    return NC_NONE();
}

NcPromise *nc_async_run(NcAsyncFn fn, void *arg) {
    if (!fn) return NULL;
    if (!pool.initialized) nc_pool_init();

    NcPromise *p = nc_promise_new();
    if (!p) return NULL;

    AsyncRunCtx *ctx = malloc(sizeof(AsyncRunCtx));
    if (!ctx) { nc_promise_free(p); return NULL; }
    ctx->fn = fn;
    ctx->arg = arg;
    ctx->promise = p;

    pool_submit_task(async_run_worker, ctx, NULL, NULL, NULL, NULL);
    return p;
}

/* ═══════════════════════════════════════════════════════════
 *  Promise.all — wait for all promises, collect results in order
 *
 *  Returns a promise that resolves with a list of results when
 *  all input promises resolve, or rejects on the first rejection.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcPromise      **promises;
    int              count;
    NcPromise       *out;
} PromiseAllCtx;

static NcValue promise_all_worker(void *raw) {
    PromiseAllCtx *ctx = (PromiseAllCtx *)raw;
    NcList *results = nc_list_new();

    for (int i = 0; i < ctx->count; i++) {
        NcPromise *p = ctx->promises[i];
        /* Wait indefinitely for each promise */
        nc_mutex_lock(&p->mutex);
        while (!p->resolved && !p->rejected)
            nc_cond_wait(&p->cond, &p->mutex);

        if (p->rejected) {
            char err_copy[256];
            snprintf(err_copy, sizeof(err_copy), "%s", p->error);
            nc_mutex_unlock(&p->mutex);
            nc_list_free(results);
            nc_promise_reject(ctx->out, err_copy);
            free(ctx->promises);
            free(ctx);
            return NC_NONE();
        }

        NcValue val = p->result;
        nc_mutex_unlock(&p->mutex);
        nc_list_push(results, val);
    }

    nc_promise_resolve(ctx->out, NC_LIST(results));
    free(ctx->promises);
    free(ctx);
    return NC_NONE();
}

NcPromise *nc_promise_all(NcPromise **promises, int count) {
    if (!promises || count <= 0) {
        NcPromise *p = nc_promise_new();
        if (p) nc_promise_resolve(p, NC_LIST(nc_list_new()));
        return p;
    }
    if (!pool.initialized) nc_pool_init();

    NcPromise *out = nc_promise_new();
    if (!out) return NULL;

    /* Copy the promise array so callers can free theirs */
    NcPromise **copy = malloc(count * sizeof(NcPromise *));
    if (!copy) { nc_promise_free(out); return NULL; }
    memcpy(copy, promises, count * sizeof(NcPromise *));

    PromiseAllCtx *ctx = malloc(sizeof(PromiseAllCtx));
    if (!ctx) { free(copy); nc_promise_free(out); return NULL; }
    ctx->promises = copy;
    ctx->count = count;
    ctx->out = out;

    pool_submit_task(promise_all_worker, ctx, NULL, NULL, NULL, NULL);
    return out;
}

/* ═══════════════════════════════════════════════════════════
 *  Promise.race — first promise to settle wins
 *
 *  Returns a promise that resolves/rejects as soon as any
 *  input promise settles. Uses a shared atomic counter so
 *  only the first settler propagates.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcPromise      **promises;
    int              count;
    NcPromise       *out;
} PromiseRaceCtx;

static NcValue promise_race_worker(void *raw) {
    PromiseRaceCtx *ctx = (PromiseRaceCtx *)raw;

    /* Poll all promises until one settles.
     * We spin with a short sleep to avoid burning CPU while
     * keeping latency low. */
    for (;;) {
        for (int i = 0; i < ctx->count; i++) {
            NcPromise *p = ctx->promises[i];
            nc_mutex_lock(&p->mutex);
            if (p->resolved) {
                NcValue val = p->result;
                nc_mutex_unlock(&p->mutex);
                nc_promise_resolve(ctx->out, val);
                free(ctx->promises);
                free(ctx);
                return NC_NONE();
            }
            if (p->rejected) {
                char err_copy[256];
                snprintf(err_copy, sizeof(err_copy), "%s", p->error);
                nc_mutex_unlock(&p->mutex);
                nc_promise_reject(ctx->out, err_copy);
                free(ctx->promises);
                free(ctx);
                return NC_NONE();
            }
            nc_mutex_unlock(&p->mutex);
        }
        nc_sleep_ms(1);
    }
}

NcPromise *nc_promise_race(NcPromise **promises, int count) {
    if (!promises || count <= 0) {
        /* No promises — return a pending promise (never settles) */
        return nc_promise_new();
    }
    if (!pool.initialized) nc_pool_init();

    NcPromise *out = nc_promise_new();
    if (!out) return NULL;

    NcPromise **copy = malloc(count * sizeof(NcPromise *));
    if (!copy) { nc_promise_free(out); return NULL; }
    memcpy(copy, promises, count * sizeof(NcPromise *));

    PromiseRaceCtx *ctx = malloc(sizeof(PromiseRaceCtx));
    if (!ctx) { free(copy); nc_promise_free(out); return NULL; }
    ctx->promises = copy;
    ctx->count = count;
    ctx->out = out;

    pool_submit_task(promise_race_worker, ctx, NULL, NULL, NULL, NULL);
    return out;
}
