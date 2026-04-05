/*
 * nc_value.h — Value system for NC (string, list, map, tagged union).
 *
 * This is the foundation type system — like CPython's PyObject.
 * Include this when you need NcValue, NcString, NcList, or NcMap.
 */

#ifndef NC_VALUE_H
#define NC_VALUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <math.h>
#include <ctype.h>

/* Forward declarations */
typedef struct NcValue    NcValue;
typedef struct NcString   NcString;
typedef struct NcList     NcList;
typedef struct NcMap      NcMap;
typedef struct NcVM       NcVM;
typedef struct NcTensorHandle NcTensorHandle;

/* ── Value types ──────────────────────────────────────────── */

typedef enum {
    VAL_NONE,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_LIST,
    VAL_MAP,
    VAL_FUNCTION,
    VAL_NATIVE_FN,
    VAL_AI_RESULT,
    VAL_TENSOR,        /* first-class tensor — zero-copy between ops */
} NcValueType;

typedef NcValue (*NcNativeFn)(NcVM *vm, int argc, NcValue *args);

/* ── Heap object structs ──────────────────────────────────── */

struct NcString {
    _Atomic int  refcount;
    int  length;
    char *chars;
    uint32_t hash;
};

struct NcList {
    _Atomic int refcount;
    int count;
    int capacity;
    NcValue *items;
};

struct NcMap {
    _Atomic int refcount;
    int count;
    int capacity;
    NcString **keys;
    NcValue  *values;
    int      *index;
    int       index_capacity;
};

/* ── Tensor handle — first-class tensor value ─────────────── */
/*    Eliminates list-of-lists conversion overhead.            */
/*    Tensors are refcounted and can carry gradients.          */

struct NcTensorHandle {
    _Atomic int  refcount;
    int          ndim;
    int         *shape;
    int         *strides;
    int          size;          /* total element count */
    float       *data;          /* parameter values (float32) */
    float       *grad;          /* gradient buffer (NULL if no grad) */
    bool         requires_grad; /* track gradients for training */
};

/* ── Tagged union value ───────────────────────────────────── */

struct NcValue {
    NcValueType type;
    union {
        bool    boolean;
        int64_t integer;
        double  floating;
        NcString       *string;
        NcList         *list;
        NcMap          *map;
        NcNativeFn      native_fn;
        NcTensorHandle *tensor;
    } as;
};

/* ── Value constructors ───────────────────────────────────── */

#define NC_NONE()        ((NcValue){VAL_NONE,      {.integer = 0}})
#define NC_BOOL(v)       ((NcValue){VAL_BOOL,      {.boolean = (v)}})
#define NC_INT(v)        ((NcValue){VAL_INT,       {.integer = (v)}})
#define NC_FLOAT(v)      ((NcValue){VAL_FLOAT,     {.floating = (v)}})
#define NC_STRING(s)     ((NcValue){VAL_STRING,    {.string = (s)}})
#define NC_STRING_CONST(cstr) NC_STRING(nc_string_from_cstr(cstr))
#define NC_LIST(l)       ((NcValue){VAL_LIST,      {.list = (l)}})
#define NC_MAP(m)        ((NcValue){VAL_MAP,       {.map = (m)}})
#define NC_NATIVE(fn)    ((NcValue){VAL_NATIVE_FN, {.native_fn = (fn)}})
#define NC_TENSOR(t)     ((NcValue){VAL_TENSOR,    {.tensor = (t)}})
#define NC_NUMBER(n)     NC_FLOAT((double)(n))
#define NC_POINTER(p)    ((NcValue){VAL_NATIVE_FN, {.native_fn = (NcNativeFn)(void*)(p)}})
#define AS_POINTER(v)    ((void*)(v).as.native_fn)

/* ── Type checks ──────────────────────────────────────────── */

#define IS_NONE(v)    ((v).type == VAL_NONE)
#define IS_BOOL(v)    ((v).type == VAL_BOOL)
#define IS_INT(v)     ((v).type == VAL_INT)
#define IS_FLOAT(v)   ((v).type == VAL_FLOAT)
#define IS_STRING(v)  ((v).type == VAL_STRING)
#define IS_LIST(v)    ((v).type == VAL_LIST)
#define IS_MAP(v)     ((v).type == VAL_MAP)
#define IS_NUMBER(v)  ((v).type == VAL_INT || (v).type == VAL_FLOAT)
#define IS_TENSOR(v)  ((v).type == VAL_TENSOR)

/* ── Value accessors ──────────────────────────────────────── */

#define AS_BOOL(v)    ((v).as.boolean)
#define AS_INT(v)     ((v).as.integer)
#define AS_FLOAT(v)   ((v).as.floating)
#define AS_STRING(v)  ((v).as.string)
#define AS_LIST(v)    ((v).as.list)
#define AS_MAP(v)     ((v).as.map)
#define AS_NATIVE(v)  ((v).as.native_fn)
#define AS_TENSOR(v)  ((v).as.tensor)

/* ── NC_ prefixed aliases (used by nc_ui_vm.c and plugins) ─ */

#ifndef NC_IS_NONE
#define NC_IS_NONE(v)    IS_NONE(v)
#define NC_IS_BOOL(v)    IS_BOOL(v)
#define NC_IS_INT(v)     IS_INT(v)
#define NC_IS_FLOAT(v)   IS_FLOAT(v)
#define NC_IS_STRING(v)  IS_STRING(v)
#define NC_IS_LIST(v)    IS_LIST(v)
#define NC_IS_MAP(v)     IS_MAP(v)
#define NC_IS_NUMBER(v)  IS_NUMBER(v)
#define NC_IS_TENSOR(v)  IS_TENSOR(v)
#define NC_AS_BOOL(v)    AS_BOOL(v)
#define NC_AS_INT(v)     AS_INT(v)
#define NC_AS_FLOAT(v)   AS_FLOAT(v)
#define NC_AS_STRING(v)  AS_STRING(v)
#define NC_AS_LIST(v)    AS_LIST(v)
#define NC_AS_MAP(v)     AS_MAP(v)
#define NC_AS_NATIVE(v)  AS_NATIVE(v)
#define NC_AS_TENSOR(v)  AS_TENSOR(v)
#define NC_AS_POINTER(v) AS_POINTER(v)
#define NC_AS_NUMBER(v)  (((v).type == VAL_INT) ? (double)(v).as.integer : (v).as.floating)
#endif

/* ── Truthiness ───────────────────────────────────────────── */

static inline bool nc_truthy(NcValue v) {
    switch (v.type) {
        case VAL_NONE:   return false;
        case VAL_BOOL:   return v.as.boolean;
        case VAL_INT:    return v.as.integer != 0;
        case VAL_FLOAT:  return v.as.floating != 0.0;
        case VAL_STRING: return v.as.string->length > 0;
        case VAL_LIST:   return v.as.list->count > 0;
        case VAL_MAP:    return v.as.map->count > 0;
        case VAL_TENSOR: return v.as.tensor != NULL && v.as.tensor->size > 0;
        default:         return true;
    }
}

/* ── String API ───────────────────────────────────────────── */

NcString *nc_string_new(const char *chars, int length);
NcString *nc_string_from_cstr(const char *cstr);
NcString *nc_string_concat(NcString *a, NcString *b);
void      nc_string_free(NcString *s);
NcString *nc_string_ref(NcString *s);
bool      nc_string_equal(NcString *a, NcString *b);
uint32_t  nc_hash_string(const char *key, int length);

/* ── List API ─────────────────────────────────────────────── */

NcList  *nc_list_new(void);
void     nc_list_push(NcList *l, NcValue v);
NcValue  nc_list_get(NcList *l, int index);
void     nc_list_free(NcList *l);

/* ── Map API ──────────────────────────────────────────────── */

NcMap   *nc_map_new(void);
void     nc_map_set(NcMap *m, NcString *key, NcValue val);
NcValue  nc_map_get(NcMap *m, NcString *key);
bool     nc_map_has(NcMap *m, NcString *key);
void     nc_map_free(NcMap *m);
void     nc_value_release(NcValue v);
void     nc_value_retain(NcValue v);

/* ── Tensor API ──────────────────────────────────────────── */

NcTensorHandle *nc_tensor_handle_new(int ndim, int *shape, bool requires_grad);
NcTensorHandle *nc_tensor_handle_ref(NcTensorHandle *t);
void            nc_tensor_handle_free(NcTensorHandle *t);
void            nc_tensor_handle_zero_grad(NcTensorHandle *t);

/* ── Value utilities ──────────────────────────────────────── */

void      nc_value_print(NcValue v, FILE *out);
NcString *nc_value_to_string(NcValue v);

#endif /* NC_VALUE_H */
