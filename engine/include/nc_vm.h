/*
 * nc_vm.h — Stack-based virtual machine for NC bytecode.
 *
 * This is the execution engine — like CPython's ceval.c.
 */

#ifndef NC_VM_H
#define NC_VM_H

#include "nc_value.h"
#include "nc_chunk.h"

#define NC_STACK_MAX  8192
#define NC_FRAMES_MAX 256

typedef struct {
    NcChunk *chunk;
    uint8_t *ip;
    NcValue *slots;
    NcValue  locals[NC_LOCALS_MAX];
    int      local_count;
} NcCallFrame;

struct NcVM {
    NcCallFrame frames[NC_FRAMES_MAX];
    int         frame_count;
    NcValue     stack[NC_STACK_MAX];
    int         stack_top;
    NcMap      *globals;
    NcMap      *behaviors;

    /* Behavior chunks for OP_CALL (behavior-to-behavior calls) */
    NcChunk    *behavior_chunks;
    int         behavior_chunk_count;

    /* Pluggable handlers (like Python C extensions) */
    NcValue (*ai_handler)(NcVM *vm, NcString *prompt, NcMap *context, NcString *model);
    NcValue (*mcp_handler)(NcVM *vm, NcString *source, NcMap *options);
    NcValue (*notify_handler)(NcVM *vm, NcString *channel, NcString *message);

    char   **output;
    int      output_count;
    int      output_capacity;

    bool     had_error;
    char     error_msg[2048];

    /* NC UI context (set when running UI mode) */
    void    *ui_ctx;
};

NcVM   *nc_vm_new(void);
NcValue nc_vm_run(NcVM *vm, const char *behavior_name);
NcValue nc_vm_call(NcVM *vm, const char *behavior_name, NcMap *args);
NcValue nc_vm_execute(NcVM *vm, NcChunk *chunk);
void    nc_vm_free(NcVM *vm);

/* JIT fast-path dispatch (computed goto) */
NcValue nc_vm_execute_fast(NcVM *vm, NcChunk *chunk);
void    nc_jit_record_call(NcString *behavior_name);
void    nc_jit_report(void);

/* Stack access (used by nc_ui_vm.c and plugins) */
void    nc_vm_push(NcVM *vm, NcValue v);
NcValue nc_vm_pop(NcVM *vm);
NcValue nc_vm_peek(NcVM *vm, int distance);

#endif /* NC_VM_H */
