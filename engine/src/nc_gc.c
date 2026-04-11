/*
 * nc_gc.c — Garbage collector for NC (mark & sweep).
 *
 * Manages heap-allocated NC objects (strings, lists, maps).
 * Uses tri-color mark & sweep — same algorithm as Go, Lua, Ruby.
 *
 * Every heap object is tracked. When memory pressure rises,
 * the GC runs: marks all reachable objects, sweeps unreachable ones.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/nc_gc.h"
#include "../include/nc_vm.h"

/* ═══════════════════════════════════════════════════════════
 *  GC Object header — prepended to every heap allocation
 * ═══════════════════════════════════════════════════════════ */

typedef enum { GC_WHITE, GC_GRAY, GC_BLACK } GCColor;

typedef enum {
    GC_OBJ_STRING,
    GC_OBJ_LIST,
    GC_OBJ_MAP,
} GCObjType;

typedef struct GCObject {
    struct GCObject *next;    /* intrusive linked list */
    GCObjType        type;
    GCColor          color;
    bool             pinned;  /* if true, never collected */
    union {
        NcString *string;
        NcList   *list;
        NcMap    *map;
    } as;
} GCObject;

/* ═══════════════════════════════════════════════════════════
 *  GC State
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    GCObject   *objects;       /* linked list of all objects */
    int         object_count;
    int         threshold;     /* trigger GC when count exceeds this */
    size_t      bytes_allocated;
    size_t      max_bytes;
    bool        enabled;

    /* Root set — values currently in use */
    NcValue    *roots;
    int         root_count;
    int         root_cap;

    /* VM root sources for automatic root scanning */
    NcVM       *active_vms[64];
    int         vm_count;

    /* Stats */
    int         collections;
    int         objects_freed;
    size_t      bytes_freed;
} NcGC;

static NcGC gc = {0};

#include "../include/nc_platform.h"
static nc_mutex_t gc_mutex = NC_MUTEX_INITIALIZER;

/* ═══════════════════════════════════════════════════════════
 *  Pointer → GCObject lookup table (eliminates O(N) scan in mark_value)
 *
 *  Simple open-addressing hash table mapping heap pointers
 *  (NcString*, NcList*, NcMap*) to their GCObject wrappers.
 *  Converts mark_value from O(N) to O(1) amortized.
 * ═══════════════════════════════════════════════════════════ */

#define GC_MAP_INIT_CAP 512
#define GC_MAP_LOAD_FACTOR 0.75

typedef struct {
    void     *key;    /* heap pointer (NcString*, NcList*, NcMap*) */
    GCObject *value;  /* corresponding GC wrapper */
} GCMapEntry;

typedef struct {
    GCMapEntry *entries;
    int         capacity;
    int         count;
} GCPtrMap;

static GCPtrMap gc_ptr_map = {0};

static void gc_ptr_map_init(void) {
    gc_ptr_map.capacity = GC_MAP_INIT_CAP;
    gc_ptr_map.count = 0;
    gc_ptr_map.entries = calloc(gc_ptr_map.capacity, sizeof(GCMapEntry));
}

static void gc_ptr_map_free(void) {
    free(gc_ptr_map.entries);
    memset(&gc_ptr_map, 0, sizeof(GCPtrMap));
}

static unsigned gc_ptr_hash(void *ptr) {
    uintptr_t h = (uintptr_t)ptr;
    h = (h >> 4) ^ (h >> 16);
    h *= 0x45d9f3b;
    return (unsigned)h;
}

static void gc_ptr_map_resize(void) {
    int old_cap = gc_ptr_map.capacity;
    GCMapEntry *old = gc_ptr_map.entries;
    gc_ptr_map.capacity *= 2;
    gc_ptr_map.entries = calloc(gc_ptr_map.capacity, sizeof(GCMapEntry));
    gc_ptr_map.count = 0;
    for (int i = 0; i < old_cap; i++) {
        if (old[i].key) {
            unsigned idx = gc_ptr_hash(old[i].key) % (unsigned)gc_ptr_map.capacity;
            while (gc_ptr_map.entries[idx].key)
                idx = (idx + 1) % (unsigned)gc_ptr_map.capacity;
            gc_ptr_map.entries[idx] = old[i];
            gc_ptr_map.count++;
        }
    }
    free(old);
}

static void gc_ptr_map_set(void *key, GCObject *value) {
    if (!gc_ptr_map.entries) gc_ptr_map_init();
    if (gc_ptr_map.count >= (int)(gc_ptr_map.capacity * GC_MAP_LOAD_FACTOR))
        gc_ptr_map_resize();
    unsigned idx = gc_ptr_hash(key) % (unsigned)gc_ptr_map.capacity;
    while (gc_ptr_map.entries[idx].key && gc_ptr_map.entries[idx].key != key)
        idx = (idx + 1) % (unsigned)gc_ptr_map.capacity;
    if (!gc_ptr_map.entries[idx].key) gc_ptr_map.count++;
    gc_ptr_map.entries[idx].key = key;
    gc_ptr_map.entries[idx].value = value;
}

static GCObject *gc_ptr_map_get(void *key) {
    if (!gc_ptr_map.entries || !key) return NULL;
    unsigned idx = gc_ptr_hash(key) % (unsigned)gc_ptr_map.capacity;
    int probes = 0;
    while (gc_ptr_map.entries[idx].key && probes < gc_ptr_map.capacity) {
        if (gc_ptr_map.entries[idx].key == key)
            return gc_ptr_map.entries[idx].value;
        idx = (idx + 1) % (unsigned)gc_ptr_map.capacity;
        probes++;
    }
    return NULL;
}

static void gc_ptr_map_remove(void *key) {
    if (!gc_ptr_map.entries || !key) return;
    unsigned idx = gc_ptr_hash(key) % (unsigned)gc_ptr_map.capacity;
    int probes = 0;
    while (gc_ptr_map.entries[idx].key && probes < gc_ptr_map.capacity) {
        if (gc_ptr_map.entries[idx].key == key) {
            gc_ptr_map.entries[idx].key = NULL;
            gc_ptr_map.entries[idx].value = NULL;
            gc_ptr_map.count--;
            /* Rehash subsequent entries in the cluster */
            unsigned next = (idx + 1) % (unsigned)gc_ptr_map.capacity;
            while (gc_ptr_map.entries[next].key) {
                GCMapEntry e = gc_ptr_map.entries[next];
                gc_ptr_map.entries[next].key = NULL;
                gc_ptr_map.entries[next].value = NULL;
                gc_ptr_map.count--;
                gc_ptr_map_set(e.key, e.value);
                next = (next + 1) % (unsigned)gc_ptr_map.capacity;
            }
            return;
        }
        idx = (idx + 1) % (unsigned)gc_ptr_map.capacity;
        probes++;
    }
}

/* ═══════════════════════════════════════════════════════════
 *  GC Lifecycle
 * ═══════════════════════════════════════════════════════════ */

void nc_gc_init(void) {
    nc_mutex_lock(&gc_mutex);
    memset(&gc, 0, sizeof(NcGC));
    gc.threshold = 256;
    gc.max_bytes = 256 * 1024 * 1024; /* 256 MB */
    gc.enabled = true;
    gc.root_cap = 64;
    gc.roots = calloc(gc.root_cap, sizeof(NcValue));
    gc_ptr_map_init();
    nc_mutex_unlock(&gc_mutex);
}

void nc_gc_shutdown(void) {
    /* Free all objects */
    GCObject *obj = gc.objects;
    while (obj) {
        GCObject *next = obj->next;
        switch (obj->type) {
            case GC_OBJ_STRING: nc_string_free(obj->as.string); break;
            case GC_OBJ_LIST:   nc_list_free(obj->as.list); break;
            case GC_OBJ_MAP:    nc_map_free(obj->as.map); break;
        }
        free(obj);
        obj = next;
    }
    free(gc.roots);
    gc_ptr_map_free();
    memset(&gc, 0, sizeof(NcGC));
}

/* ═══════════════════════════════════════════════════════════
 *  Track allocations
 * ═══════════════════════════════════════════════════════════ */

static GCObject *gc_track(GCObjType type) {
    GCObject *obj = calloc(1, sizeof(GCObject));
    if (!obj) return NULL;
    obj->type = type;
    obj->color = GC_WHITE;
    obj->pinned = false;
    nc_mutex_lock(&gc_mutex);
    obj->next = gc.objects;
    gc.objects = obj;
    gc.object_count++;
    nc_mutex_unlock(&gc_mutex);
    return obj;
}

NcString *nc_gc_alloc_string(const char *chars, int length) {
    NcString *s = nc_string_new(chars, length);
    GCObject *obj = gc_track(GC_OBJ_STRING);
    if (!obj) { nc_string_free(s); return NULL; }
    obj->as.string = s;
    gc_ptr_map_set(s, obj);
    gc.bytes_allocated += sizeof(NcString) + length + 1;
    return s;
}

NcList *nc_gc_alloc_list(void) {
    NcList *l = nc_list_new();
    GCObject *obj = gc_track(GC_OBJ_LIST);
    if (!obj) { nc_list_free(l); return NULL; }
    obj->as.list = l;
    gc_ptr_map_set(l, obj);
    gc.bytes_allocated += sizeof(NcList);
    return l;
}

NcMap *nc_gc_alloc_map(void) {
    NcMap *m = nc_map_new();
    GCObject *obj = gc_track(GC_OBJ_MAP);
    if (!obj) { nc_map_free(m); return NULL; }
    obj->as.map = m;
    gc_ptr_map_set(m, obj);
    gc.bytes_allocated += sizeof(NcMap);
    return m;
}

/* ═══════════════════════════════════════════════════════════
 *  Root set management
 * ═══════════════════════════════════════════════════════════ */

void nc_gc_push_root(NcValue val) {
    nc_mutex_lock(&gc_mutex);
    if (gc.root_count >= gc.root_cap) {
        gc.root_cap *= 2;
        NcValue *tmp = realloc(gc.roots, sizeof(NcValue) * gc.root_cap);
        if (!tmp) { nc_mutex_unlock(&gc_mutex); return; }
        gc.roots = tmp;
    }
    gc.roots[gc.root_count++] = val;
    nc_mutex_unlock(&gc_mutex);
}

void nc_gc_pop_root(void) {
    nc_mutex_lock(&gc_mutex);
    if (gc.root_count > 0) gc.root_count--;
    nc_mutex_unlock(&gc_mutex);
}

/* ═══════════════════════════════════════════════════════════
 *  Mark phase — trace all reachable objects
 * ═══════════════════════════════════════════════════════════ */

static void mark_value(NcValue v);

static void mark_object(GCObject *obj) {
    if (!obj || obj->color != GC_WHITE) return;
    obj->color = GC_GRAY;

    switch (obj->type) {
    case GC_OBJ_STRING:
        break;  /* no references */
    case GC_OBJ_LIST:
        for (int i = 0; i < obj->as.list->count; i++)
            mark_value(obj->as.list->items[i]);
        break;
    case GC_OBJ_MAP: {
        NcMap *m = obj->as.map;
        for (int i = 0; i < m->count; i++) {
            mark_value(NC_STRING(m->keys[i]));
            mark_value(m->values[i]);
        }
        break;
    }
    }

    obj->color = GC_BLACK;
}

static void mark_value(NcValue v) {
    /* O(1) pointer lookup via hash table (was O(N) linear scan) */
    void *ptr = NULL;
    switch (v.type) {
    case VAL_STRING: ptr = v.as.string; break;
    case VAL_LIST:   ptr = v.as.list;   break;
    case VAL_MAP:    ptr = v.as.map;    break;
    default: return; /* primitives don't need GC */
    }
    if (!ptr) return;
    GCObject *obj = gc_ptr_map_get(ptr);
    if (obj) mark_object(obj);
}

/* ═══════════════════════════════════════════════════════════
 *  Sweep phase — free unreachable objects
 * ═══════════════════════════════════════════════════════════ */

static void sweep(void) {
    GCObject **prev = &gc.objects;
    GCObject *obj = gc.objects;

    while (obj) {
        if (obj->color == GC_WHITE && !obj->pinned) {
            /* Unreachable — free it */
            GCObject *unreached = obj;
            *prev = obj->next;
            obj = obj->next;

            switch (unreached->type) {
            case GC_OBJ_STRING:
                gc_ptr_map_remove(unreached->as.string);
                gc.bytes_freed += sizeof(NcString) + unreached->as.string->length;
                nc_string_free(unreached->as.string);
                break;
            case GC_OBJ_LIST:
                gc_ptr_map_remove(unreached->as.list);
                gc.bytes_freed += sizeof(NcList);
                nc_list_free(unreached->as.list);
                break;
            case GC_OBJ_MAP:
                gc_ptr_map_remove(unreached->as.map);
                gc.bytes_freed += sizeof(NcMap);
                nc_map_free(unreached->as.map);
                break;
            }
            free(unreached);
            gc.object_count--;
            gc.objects_freed++;
        } else {
            obj->color = GC_WHITE; /* reset for next cycle */
            prev = &obj->next;
            obj = obj->next;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  VM Integration — register VMs so GC scans their roots
 * ═══════════════════════════════════════════════════════════ */

void nc_gc_register_vm(NcVM *vm) {
    nc_mutex_lock(&gc_mutex);
    if (!vm || gc.vm_count >= 64) { nc_mutex_unlock(&gc_mutex); return; }
    gc.active_vms[gc.vm_count++] = vm;
    nc_mutex_unlock(&gc_mutex);
}

void nc_gc_unregister_vm(NcVM *vm) {
    nc_mutex_lock(&gc_mutex);
    for (int i = 0; i < gc.vm_count; i++) {
        if (gc.active_vms[i] == vm) {
            gc.active_vms[i] = gc.active_vms[--gc.vm_count];
            nc_mutex_unlock(&gc_mutex);
            return;
        }
    }
    nc_mutex_unlock(&gc_mutex);
}

static void gc_scan_vm_roots(void) {
    for (int v = 0; v < gc.vm_count; v++) {
        NcVM *vm = gc.active_vms[v];
        if (!vm) continue;
        for (int i = 0; i < vm->stack_top; i++)
            mark_value(vm->stack[i]);
        if (vm->globals) {
            for (int i = 0; i < vm->globals->count; i++)
                mark_value(vm->globals->values[i]);
        }
        for (int f = 0; f < vm->frame_count; f++) {
            for (int i = 0; i < vm->frames[f].local_count; i++)
                mark_value(vm->frames[f].locals[i]);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Collect — run a full GC cycle
 * ═══════════════════════════════════════════════════════════ */

void nc_gc_collect(void) {
    nc_mutex_lock(&gc_mutex);
    if (!gc.enabled) { nc_mutex_unlock(&gc_mutex); return; }

    /* Mark explicit roots */
    for (int i = 0; i < gc.root_count; i++)
        mark_value(gc.roots[i]);

    /* Mark all reachable objects from active VMs */
    gc_scan_vm_roots();

    /* Sweep unmarked (including objects in cycles) */
    sweep();

    gc.collections++;
    gc.threshold = gc.object_count * 2;
    if (gc.threshold < 256) gc.threshold = 256;
    nc_mutex_unlock(&gc_mutex);
}

/* Auto-collect when threshold exceeded */
void nc_gc_maybe_collect(void) {
    nc_mutex_lock(&gc_mutex);
    bool should_collect = (gc.object_count >= gc.threshold);
    nc_mutex_unlock(&gc_mutex);
    if (should_collect)
        nc_gc_collect();
}

/* ═══════════════════════════════════════════════════════════
 *  GC Stats
 * ═══════════════════════════════════════════════════════════ */

void nc_gc_stats(void) {
    printf("\n  GC Statistics:\n");
    printf("  ────────────────────────────────\n");
    printf("  Live objects:    %d\n", gc.object_count);
    printf("  Bytes allocated: %zu\n", gc.bytes_allocated);
    printf("  Collections:     %d\n", gc.collections);
    printf("  Objects freed:   %d\n", gc.objects_freed);
    printf("  Bytes freed:     %zu\n", gc.bytes_freed);
    printf("\n");
}
