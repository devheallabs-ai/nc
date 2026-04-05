/*
 * nc_async.h — Async / concurrency primitives for NC.
 */

#ifndef NC_ASYNC_H
#define NC_ASYNC_H

#include "nc_value.h"
#include "nc_platform.h"

typedef NcValue (*NcTaskFunc)(void *arg);
typedef NcValue (*NcAsyncFn)(void *arg);

/* ── Thread pool ──────────────────────────────────────────── */

void    nc_event_loop_init(void);
int     nc_event_loop_run(void);
void    nc_event_loop_stop(void);
void    nc_pool_init(void);
void    nc_pool_shutdown(void);
int     nc_pool_worker_count(void);
NcValue nc_parallel_map(NcTaskFunc func, void **args, int count);
NcValue nc_parallel_find_similar(NcList *query_vec, NcList *all_vectors,
                                  NcList *documents, int top_k);

/* ── Promise / Future ─────────────────────────────────────── */

typedef struct NcPromise NcPromise;

typedef NcValue (*NcPromiseCallback)(NcValue value, void *ctx);

struct NcPromise {
    nc_mutex_t  mutex;
    nc_cond_t   cond;
    NcValue     result;
    bool        resolved;
    bool        rejected;
    char        error[256];

    /* Chaining callbacks (set via then/catch) */
    NcPromiseCallback  then_cb;
    void              *then_ctx;
    NcPromise         *then_promise;

    NcPromiseCallback  catch_cb;
    void              *catch_ctx;
    NcPromise         *catch_promise;
};

NcPromise  *nc_promise_new(void);
void        nc_promise_resolve(NcPromise *p, NcValue val);
void        nc_promise_reject(NcPromise *p, const char *error);
NcValue     nc_promise_await(NcPromise *p, int timeout_ms);
void        nc_promise_free(NcPromise *p);

NcPromise  *nc_promise_then(NcPromise *p, NcPromiseCallback cb, void *ctx);
NcPromise  *nc_promise_catch(NcPromise *p, NcPromiseCallback cb, void *ctx);

NcPromise  *nc_async_run(NcAsyncFn fn, void *arg);

NcPromise  *nc_promise_all(NcPromise **promises, int count);
NcPromise  *nc_promise_race(NcPromise **promises, int count);

#endif /* NC_ASYNC_H */
