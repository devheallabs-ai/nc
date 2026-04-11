/*
 * nc_gc.h — Garbage collector for NC.
 */

#ifndef NC_GC_H
#define NC_GC_H

#include "nc_value.h"

typedef struct NcVM NcVM;

void nc_gc_init(void);
void nc_gc_shutdown(void);
void nc_gc_collect(void);
void nc_gc_maybe_collect(void);
void nc_gc_push_root(NcValue val);
void nc_gc_pop_root(void);
void nc_gc_stats(void);
void nc_gc_register_vm(NcVM *vm);
void nc_gc_unregister_vm(NcVM *vm);

NcString *nc_gc_alloc_string(const char *chars, int length);
NcList   *nc_gc_alloc_list(void);
NcMap    *nc_gc_alloc_map(void);

#endif /* NC_GC_H */
