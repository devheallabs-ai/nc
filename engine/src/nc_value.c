/*
 * nc_value.c — Value system, strings, lists, maps for the NC runtime.
 *
 * This is the foundation — like CPython's Objects/object.c.
 * All NC values are tagged unions with reference-counted heap objects.
 */

#include "../include/nc.h"

/* ═══════════════════════════════════════════════════════════
 *  Atomic refcount helpers — thread-safe inc/dec
 * ═══════════════════════════════════════════════════════════ */

static inline int rc_inc(_Atomic int *rc) { return atomic_fetch_add(rc, 1) + 1; }
static inline int rc_dec(_Atomic int *rc) { return atomic_fetch_sub(rc, 1) - 1; }

/* Forward declaration — releases refcounted values inside collections */
void nc_value_release(NcValue v);

/* ═══════════════════════════════════════════════════════════
 *  String (refcounted, hashed)
 * ═══════════════════════════════════════════════════════════ */

uint32_t nc_hash_string(const char *key, int length) {
    /* FNV-1a hash */
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

/* String intern table — deduplicates strings for O(1) comparison */
#define NC_INTERN_CAP 4096
#define NC_INTERN_TOMBSTONE ((NcString *)(uintptr_t)1)
static NcString *intern_table[NC_INTERN_CAP];
static bool intern_initialized = false;
#include "../include/nc_platform.h"
static nc_mutex_t intern_mutex = NC_MUTEX_INITIALIZER;

static NcString *intern_find(const char *chars, int length, uint32_t hash) {
    if (!intern_initialized) {
        memset(intern_table, 0, sizeof(intern_table));
        intern_initialized = true;
    }
    uint32_t slot = hash & (NC_INTERN_CAP - 1);
    for (int probe = 0; probe < NC_INTERN_CAP; probe++) {
        uint32_t idx = (slot + probe) & (NC_INTERN_CAP - 1);
        NcString *s = intern_table[idx];
        if (!s) return NULL;
        if (s == NC_INTERN_TOMBSTONE) continue;
        if (s->hash == hash && s->length == length &&
            memcmp(s->chars, chars, length) == 0) {
            return s;
        }
    }
    return NULL;
}

static void intern_insert(NcString *s) {
    if (!intern_initialized) {
        memset(intern_table, 0, sizeof(intern_table));
        intern_initialized = true;
    }
    uint32_t slot = s->hash & (NC_INTERN_CAP - 1);
    for (int probe = 0; probe < NC_INTERN_CAP; probe++) {
        uint32_t idx = (slot + probe) & (NC_INTERN_CAP - 1);
        if (!intern_table[idx] || intern_table[idx] == NC_INTERN_TOMBSTONE) {
            intern_table[idx] = s;
            return;
        }
    }
}

NcString *nc_string_new(const char *chars, int length) {
    if (!chars || length < 0) return NULL;
    uint32_t hash = nc_hash_string(chars, length);

    nc_mutex_lock(&intern_mutex);
    NcString *existing = intern_find(chars, length, hash);
    if (existing) {
        rc_inc(&existing->refcount);
        nc_mutex_unlock(&intern_mutex);
        return existing;
    }

    NcString *s = malloc(sizeof(NcString) + length + 1);
    if (!s) { nc_mutex_unlock(&intern_mutex); return NULL; }
    atomic_init(&s->refcount, 1);
    s->length = length;
    s->chars = (char *)(s + 1);
    memcpy(s->chars, chars, length);
    s->chars[length] = '\0';
    s->hash = hash;

    intern_insert(s);
    nc_mutex_unlock(&intern_mutex);
    return s;
}

NcString *nc_string_from_cstr(const char *cstr) {
    if (!cstr) return nc_string_new("", 0);
    return nc_string_new(cstr, (int)strlen(cstr));
}

NcString *nc_string_ref(NcString *s) {
    if (s) rc_inc(&s->refcount);
    return s;
}

void nc_string_free(NcString *s) {
    if (!s) return;
    if (rc_dec(&s->refcount) > 0) return;
    {
        nc_mutex_lock(&intern_mutex);
        if (intern_initialized) {
            uint32_t slot = s->hash & (NC_INTERN_CAP - 1);
            for (int probe = 0; probe < NC_INTERN_CAP; probe++) {
                uint32_t idx = (slot + probe) & (NC_INTERN_CAP - 1);
                if (!intern_table[idx]) break;
                if (intern_table[idx] == NC_INTERN_TOMBSTONE) continue;
                if (intern_table[idx] == s) {
                    intern_table[idx] = NC_INTERN_TOMBSTONE;
                    break;
                }
            }
        }
        nc_mutex_unlock(&intern_mutex);
        bool inline_chars = (s->chars == (char *)(s + 1));
        if (!inline_chars) free(s->chars);
        free(s);
    }
}

NcString *nc_string_concat(NcString *a, NcString *b) {
    if (!a) return b ? nc_string_ref(b) : nc_string_new("", 0);
    if (!b) return nc_string_ref(a);
    if (a->length == 0) return nc_string_ref(b);
    if (b->length == 0) return nc_string_ref(a);
    int len = a->length + b->length;
    uint32_t hash = nc_hash_string(a->chars, a->length);
    for (int i = 0; i < b->length; i++) {
        hash ^= (uint8_t)b->chars[i];
        hash *= 16777619;
    }

    nc_mutex_lock(&intern_mutex);
    NcString *existing = intern_find(NULL, 0, 0);
    (void)existing;

    NcString *s = malloc(sizeof(NcString) + len + 1);
    if (!s) { nc_mutex_unlock(&intern_mutex); return nc_string_new("", 0); }
    atomic_init(&s->refcount, 1);
    s->length = len;
    s->chars = (char *)(s + 1);
    memcpy(s->chars, a->chars, a->length);
    memcpy(s->chars + a->length, b->chars, b->length);
    s->chars[len] = '\0';
    s->hash = hash;

    NcString *found = intern_find(s->chars, len, hash);
    if (found) {
        rc_inc(&found->refcount);
        nc_mutex_unlock(&intern_mutex);
        free(s);
        return found;
    }
    intern_insert(s);
    nc_mutex_unlock(&intern_mutex);
    return s;
}

bool nc_string_equal(NcString *a, NcString *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->length != b->length) return false;
    return memcmp(a->chars, b->chars, a->length) == 0;
}

/* ═══════════════════════════════════════════════════════════
 *  List (dynamic array)
 * ═══════════════════════════════════════════════════════════ */

NcList *nc_list_new(void) {
    NcList *l = malloc(sizeof(NcList));
    if (!l) return NULL;
    atomic_init(&l->refcount, 1);
    l->count = 0;
    l->capacity = 8;
    l->items = malloc(sizeof(NcValue) * l->capacity);
    if (!l->items) { free(l); return NULL; }
    return l;
}

void nc_list_push(NcList *l, NcValue v) {
    if (!l) return;
    if (l->count >= l->capacity) {
        int new_cap = l->capacity * 2;
        NcValue *new_items = realloc(l->items, sizeof(NcValue) * new_cap);
        if (!new_items) return;
        l->items = new_items;
        l->capacity = new_cap;
    }
    nc_value_retain(v);
    l->items[l->count++] = v;
}

NcValue nc_list_get(NcList *l, int index) {
    if (index < 0 || index >= l->count) return NC_NONE();
    return l->items[index];
}

void nc_list_free(NcList *l) {
    if (!l) return;
    if (rc_dec(&l->refcount) > 0) return;
    /* Save and clear to guard against re-entrant calls (circular refs) */
    int saved_count = l->count;
    NcValue *saved_items = l->items;
    l->count = 0;
    l->items = NULL;
    for (int i = 0; i < saved_count; i++)
        nc_value_release(saved_items[i]);
    free(saved_items);
    free(l);
}

/* ═══════════════════════════════════════════════════════════
 *  Map — CPython 3.7-style compact dict
 *
 *  Dense keys[] and values[] arrays preserve insertion order
 *  and allow O(n) iteration via 0..count-1 (no NULL checks).
 *  A separate index[] hash table provides O(1) lookup by
 *  mapping (hash % index_capacity) → position in keys/values.
 *  index[slot] == -1 means the slot is empty.
 *
 *  This gives us:
 *    - O(1) get/set/has  (hash probe → dense array lookup)
 *    - O(n) iteration in insertion order (same as before)
 *    - Cache-friendly sequential scans for serialization
 * ═══════════════════════════════════════════════════════════ */

static void map_build_index(NcMap *m) {
    free(m->index);
    m->index_capacity = m->capacity * 2;
    if (m->index_capacity < 16) m->index_capacity = 16;
    /* Ensure power of 2 */
    int cap = 16;
    while (cap < m->index_capacity) cap *= 2;
    m->index_capacity = cap;

    m->index = malloc(m->index_capacity * sizeof(int));
    if (!m->index) { m->index_capacity = 0; return; }
    memset(m->index, -1, m->index_capacity * sizeof(int));

    for (int i = 0; i < m->count; i++) {
        uint32_t slot = m->keys[i]->hash & (m->index_capacity - 1);
        while (m->index[slot] != -1)
            slot = (slot + 1) & (m->index_capacity - 1);
        m->index[slot] = i;
    }
}

NcMap *nc_map_new(void) {
    NcMap *m = malloc(sizeof(NcMap));
    if (!m) return NULL;
    atomic_init(&m->refcount, 1);
    m->count = 0;
    m->capacity = 16;
    m->keys = calloc(m->capacity, sizeof(NcString *));
    m->values = calloc(m->capacity, sizeof(NcValue));
    m->index = NULL;
    m->index_capacity = 0;
    if (!m->keys || !m->values) {
        free(m->keys); free(m->values); free(m); return NULL;
    }
    return m;
}

static int map_find_dense(NcMap *m, NcString *key) {
    if (!m->index || m->index_capacity == 0) {
        /* Linear scan when no index built */
        for (int i = 0; i < m->count; i++) {
            if (m->keys[i] && nc_string_equal(m->keys[i], key))
                return i;
        }
        return -1;
    }

    /* Hash index lookup */
    uint32_t slot = key->hash & (m->index_capacity - 1);
    for (int probes = 0; probes < m->index_capacity; probes++) {
        int idx = m->index[slot];
        if (idx == -1) break;
        if (idx < m->count && nc_string_equal(m->keys[idx], key))
            return idx;
        slot = (slot + 1) & (m->index_capacity - 1);
    }

    /* Fallback: linear scan if hash index missed (handles index corruption,
     * hash collisions with tombstones, or index/key mismatch edge cases) */
    for (int i = 0; i < m->count; i++) {
        if (m->keys[i] && nc_string_equal(m->keys[i], key))
            return i;
    }
    return -1;
}

void nc_map_set(NcMap *m, NcString *key, NcValue val) {
    if (!m || !key) return;

    int idx = map_find_dense(m, key);
    if (idx >= 0) {
        NcValue old = m->values[idx];
        nc_value_retain(val);
        m->values[idx] = val;
        nc_value_release(old);
        return;
    }

    if (m->count >= m->capacity) {
        int new_cap = m->capacity * 2;
        NcString **nk = realloc(m->keys, sizeof(NcString *) * new_cap);
        NcValue *nv = realloc(m->values, sizeof(NcValue) * new_cap);
        if (!nk || !nv) return;
        m->keys = nk;
        m->values = nv;
        m->capacity = new_cap;
    }

    m->keys[m->count] = nc_string_ref(key);
    nc_value_retain(val);
    m->values[m->count] = val;
    m->count++;

    /* Rebuild index when needed (load factor > 66% or first time) */
    if (!m->index || m->count * 3 > m->index_capacity * 2) {
        map_build_index(m);
        if (!m->index) return;
    } else {
        /* Fast path: just insert new entry into existing index */
        if (!m->index) return;
        uint32_t slot = key->hash & (m->index_capacity - 1);
        while (m->index[slot] != -1)
            slot = (slot + 1) & (m->index_capacity - 1);
        m->index[slot] = m->count - 1;
    }
}

NcValue nc_map_get(NcMap *m, NcString *key) {
    if (!m || !key || m->count == 0) return NC_NONE();
    int idx = map_find_dense(m, key);
    if (idx >= 0) return m->values[idx];
    return NC_NONE();
}

bool nc_map_has(NcMap *m, NcString *key) {
    if (!m || !key || m->count == 0) return false;
    return map_find_dense(m, key) >= 0;
}

void nc_value_release(NcValue v) {
    if (IS_STRING(v) && AS_STRING(v))
        nc_string_free(AS_STRING(v));
    else if (IS_LIST(v) && AS_LIST(v))
        nc_list_free(AS_LIST(v));
    else if (IS_MAP(v) && AS_MAP(v))
        nc_map_free(AS_MAP(v));
}

void nc_value_retain(NcValue v) {
    if (IS_STRING(v) && AS_STRING(v))
        rc_inc(&AS_STRING(v)->refcount);
    else if (IS_LIST(v) && AS_LIST(v))
        rc_inc(&AS_LIST(v)->refcount);
    else if (IS_MAP(v) && AS_MAP(v))
        rc_inc(&AS_MAP(v)->refcount);
}

void nc_map_free(NcMap *m) {
    if (!m) return;
    if (rc_dec(&m->refcount) > 0) return;
    /* Save and clear count to guard against re-entrant calls (circular refs) */
    int saved_count = m->count;
    NcString **saved_keys = m->keys;
    NcValue *saved_values = m->values;
    m->count = 0;
    m->keys = NULL;
    m->values = NULL;
    for (int i = 0; i < saved_count; i++) {
        nc_value_release(saved_values[i]);
        nc_string_free(saved_keys[i]);
    }
    free(saved_keys);
    free(saved_values);
    free(m->index);
    free(m);
}

/* ═══════════════════════════════════════════════════════════
 *  Chunk (bytecode container)
 * ═══════════════════════════════════════════════════════════ */

NcChunk *nc_chunk_new(void) {
    NcChunk *c = malloc(sizeof(NcChunk));
    if (!c) return NULL;
    c->count = 0;
    c->capacity = 256;
    c->code = malloc(c->capacity);
    c->lines = malloc(sizeof(int) * c->capacity);
    c->const_count = 0;
    c->const_capacity = 64;
    c->constants = malloc(sizeof(NcValue) * c->const_capacity);
    c->var_count = 0;
    c->var_capacity = 64;
    c->var_names = malloc(sizeof(NcString *) * c->var_capacity);
    if (!c->code || !c->lines || !c->constants || !c->var_names) {
        free(c->code); free(c->lines); free(c->constants); free(c->var_names);
        free(c);
        return NULL;
    }
    return c;
}

void nc_chunk_write(NcChunk *c, uint8_t byte, int line) {
    if (c->count >= c->capacity) {
        int new_cap = c->capacity * 2;
        void *tmp_code = realloc(c->code, new_cap);
        if (!tmp_code) return;
        c->code = tmp_code;
        void *tmp_lines = realloc(c->lines, sizeof(int) * new_cap);
        if (!tmp_lines) return;
        c->lines = tmp_lines;
        c->capacity = new_cap;
    }
    c->code[c->count] = byte;
    c->lines[c->count] = line;
    c->count++;
}

int nc_chunk_add_constant(NcChunk *c, NcValue val) {
    if (c->const_count >= c->const_capacity) {
        int new_cap = c->const_capacity * 2;
        NcValue *tmp = realloc(c->constants, sizeof(NcValue) * new_cap);
        if (!tmp) return -1;
        c->constants = tmp;
        c->const_capacity = new_cap;
    }
    c->constants[c->const_count] = val;
    return c->const_count++;
}

int nc_chunk_add_var(NcChunk *c, NcString *name) {
    for (int i = 0; i < c->var_count; i++) {
        if (nc_string_equal(c->var_names[i], name)) return i;
    }
    if (c->var_count >= c->var_capacity) {
        int new_cap = c->var_capacity * 2;
        NcString **tmp = realloc(c->var_names, sizeof(NcString *) * new_cap);
        if (!tmp) return -1;
        c->var_names = tmp;
        c->var_capacity = new_cap;
    }
    c->var_names[c->var_count] = nc_string_ref(name);
    return c->var_count++;
}

void nc_chunk_free(NcChunk *c) {
    if (!c) return;
    free(c->code);
    free(c->lines);
    free(c->constants);
    for (int i = 0; i < c->var_count; i++) nc_string_free(c->var_names[i]);
    free(c->var_names);
    free(c);
}

/* ═══════════════════════════════════════════════════════════
 *  AST Node allocation
 * ═══════════════════════════════════════════════════════════ */

NcASTNode *nc_ast_new(NcNodeType type, int line) {
    NcASTNode *node = calloc(1, sizeof(NcASTNode));
    if (!node) return NULL;
    node->type = type;
    node->line = line;
    return node;
}

void nc_ast_free(NcASTNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_PROGRAM:
        for (int i = 0; i < node->as.program.import_count; i++)
            nc_ast_free(node->as.program.imports[i]);
        free(node->as.program.imports);
        for (int i = 0; i < node->as.program.def_count; i++)
            nc_ast_free(node->as.program.definitions[i]);
        free(node->as.program.definitions);
        for (int i = 0; i < node->as.program.beh_count; i++)
            nc_ast_free(node->as.program.behaviors[i]);
        free(node->as.program.behaviors);
        for (int i = 0; i < node->as.program.route_count; i++)
            nc_ast_free(node->as.program.routes[i]);
        free(node->as.program.routes);
        for (int i = 0; i < node->as.program.event_count; i++)
            nc_ast_free(node->as.program.events[i]);
        free(node->as.program.events);
        for (int i = 0; i < node->as.program.mw_count; i++)
            nc_ast_free(node->as.program.middleware[i]);
        free(node->as.program.middleware);
        for (int i = 0; i < node->as.program.agent_count; i++)
            nc_ast_free(node->as.program.agents[i]);
        free(node->as.program.agents);
        break;

    case NODE_BEHAVIOR:
        for (int i = 0; i < node->as.behavior.param_count; i++)
            nc_ast_free(node->as.behavior.params[i]);
        free(node->as.behavior.params);
        for (int i = 0; i < node->as.behavior.body_count; i++)
            nc_ast_free(node->as.behavior.body[i]);
        free(node->as.behavior.body);
        break;

    case NODE_IF:
        nc_ast_free(node->as.if_stmt.condition);
        for (int i = 0; i < node->as.if_stmt.then_count; i++)
            nc_ast_free(node->as.if_stmt.then_body[i]);
        free(node->as.if_stmt.then_body);
        for (int i = 0; i < node->as.if_stmt.else_count; i++)
            nc_ast_free(node->as.if_stmt.else_body[i]);
        free(node->as.if_stmt.else_body);
        break;

    case NODE_REPEAT:
        nc_ast_free(node->as.repeat.iterable);
        for (int i = 0; i < node->as.repeat.body_count; i++)
            nc_ast_free(node->as.repeat.body[i]);
        free(node->as.repeat.body);
        break;

    case NODE_WHILE:
        nc_ast_free(node->as.while_stmt.condition);
        for (int i = 0; i < node->as.while_stmt.body_count; i++)
            nc_ast_free(node->as.while_stmt.body[i]);
        free(node->as.while_stmt.body);
        break;

    case NODE_FOR_COUNT:
        nc_ast_free(node->as.for_count.count_expr);
        for (int i = 0; i < node->as.for_count.body_count; i++)
            nc_ast_free(node->as.for_count.body[i]);
        free(node->as.for_count.body);
        break;

    case NODE_TRY:
        for (int i = 0; i < node->as.try_stmt.body_count; i++)
            nc_ast_free(node->as.try_stmt.body[i]);
        free(node->as.try_stmt.body);
        for (int i = 0; i < node->as.try_stmt.error_count; i++)
            nc_ast_free(node->as.try_stmt.error_body[i]);
        free(node->as.try_stmt.error_body);
        for (int i = 0; i < node->as.try_stmt.finally_count; i++)
            nc_ast_free(node->as.try_stmt.finally_body[i]);
        free(node->as.try_stmt.finally_body);
        break;

    case NODE_MATCH:
        nc_ast_free(node->as.match_stmt.subject);
        for (int i = 0; i < node->as.match_stmt.case_count; i++)
            nc_ast_free(node->as.match_stmt.cases[i]);
        free(node->as.match_stmt.cases);
        break;

    case NODE_WHEN:
        nc_ast_free(node->as.when_clause.value);
        for (int i = 0; i < node->as.when_clause.body_count; i++)
            nc_ast_free(node->as.when_clause.body[i]);
        free(node->as.when_clause.body);
        break;

    case NODE_DEFINITION:
        for (int i = 0; i < node->as.definition.field_count; i++)
            nc_ast_free(node->as.definition.fields[i]);
        free(node->as.definition.fields);
        break;

    case NODE_RUN:
        for (int i = 0; i < node->as.run_stmt.arg_count; i++)
            nc_ast_free(node->as.run_stmt.args[i]);
        free(node->as.run_stmt.args);
        break;

    case NODE_CALL:
        for (int i = 0; i < node->as.call.arg_count; i++)
            nc_ast_free(node->as.call.args[i]);
        free(node->as.call.args);
        break;

    case NODE_MATH:
        nc_ast_free(node->as.math.left);
        nc_ast_free(node->as.math.right);
        break;

    case NODE_COMPARISON:
        nc_ast_free(node->as.comparison.left);
        nc_ast_free(node->as.comparison.right);
        break;

    case NODE_LOGIC:
        nc_ast_free(node->as.logic.left);
        nc_ast_free(node->as.logic.right);
        break;

    case NODE_NOT:
        nc_ast_free(node->as.logic.left);
        break;

    case NODE_DOT:
        nc_ast_free(node->as.dot.object);
        break;

    case NODE_INDEX:
        nc_ast_free(node->as.math.left);
        nc_ast_free(node->as.math.right);
        break;

    case NODE_LIST_LIT:
        for (int i = 0; i < node->as.list_lit.count; i++)
            nc_ast_free(node->as.list_lit.elements[i]);
        free(node->as.list_lit.elements);
        break;

    case NODE_SET:
        nc_ast_free(node->as.set_stmt.value);
        break;

    case NODE_SET_INDEX:
        nc_ast_free(node->as.set_index.index);
        nc_ast_free(node->as.set_index.value);
        break;

    case NODE_RESPOND: case NODE_LOG: case NODE_SHOW: case NODE_EMIT:
        nc_ast_free(node->as.single_expr.value);
        break;

    case NODE_STORE:
        nc_ast_free(node->as.store_stmt.value);
        break;

    case NODE_EVENT_HANDLER:
        for (int i = 0; i < node->as.event_handler.body_count; i++)
            nc_ast_free(node->as.event_handler.body[i]);
        free(node->as.event_handler.body);
        break;

    case NODE_SCHEDULE_HANDLER:
        for (int i = 0; i < node->as.schedule_handler.body_count; i++)
            nc_ast_free(node->as.schedule_handler.body[i]);
        free(node->as.schedule_handler.body);
        break;

    case NODE_AGENT_DEF:
        free(node->as.agent_def.tools);
        break;

    case NODE_RUN_AGENT:
        nc_ast_free(node->as.run_agent.prompt);
        break;

    default:
        break;
    }
    free(node);
}

/* ═══════════════════════════════════════════════════════════
 *  Value printing (for debug / show / log)
 * ═══════════════════════════════════════════════════════════ */

void nc_value_print(NcValue v, FILE *out) {
    switch (v.type) {
        case VAL_NONE:   fprintf(out, "nothing"); break;
        case VAL_BOOL:   fprintf(out, "%s", v.as.boolean ? "yes" : "no"); break;
        case VAL_INT:    fprintf(out, "%lld", (long long)v.as.integer); break;
        case VAL_FLOAT:  fprintf(out, "%g", v.as.floating); break;
        case VAL_STRING: fprintf(out, "%s", v.as.string->chars); break;
        case VAL_LIST:
            fprintf(out, "[");
            for (int i = 0; i < v.as.list->count; i++) {
                if (i > 0) fprintf(out, ", ");
                nc_value_print(v.as.list->items[i], out);
            }
            fprintf(out, "]");
            break;
        case VAL_MAP:
            fprintf(out, "{");
            for (int i = 0; i < v.as.map->count; i++) {
                if (i > 0) fprintf(out, ", ");
                fprintf(out, "%s: ", v.as.map->keys[i]->chars);
                nc_value_print(v.as.map->values[i], out);
            }
            fprintf(out, "}");
            break;
        default: fprintf(out, "<value>"); break;
    }
}

/* Cached common strings to avoid repeated intern lookups */
static NcString *cached_nothing = NULL;
static NcString *cached_yes = NULL;
static NcString *cached_no = NULL;

static NcString *get_cached_nothing(void) {
    if (!cached_nothing) cached_nothing = nc_string_from_cstr("nothing");
    return nc_string_ref(cached_nothing);
}
static NcString *get_cached_yes(void) {
    if (!cached_yes) cached_yes = nc_string_from_cstr("yes");
    return nc_string_ref(cached_yes);
}
static NcString *get_cached_no(void) {
    if (!cached_no) cached_no = nc_string_from_cstr("no");
    return nc_string_ref(cached_no);
}

NcString *nc_value_to_string(NcValue v) {
    char buf[2048];
    switch (v.type) {
        case VAL_NONE:   return get_cached_nothing();
        case VAL_BOOL:   return v.as.boolean ? get_cached_yes() : get_cached_no();
        case VAL_INT:    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.integer); break;
        case VAL_FLOAT:  snprintf(buf, sizeof(buf), "%g", v.as.floating); break;
        case VAL_STRING: return nc_string_ref(v.as.string);
        case VAL_LIST: {
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "[");
            for (int i = 0; i < v.as.list->count && pos < 2000; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
                NcString *item = nc_value_to_string(v.as.list->items[i]);
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", item->chars);
                nc_string_free(item);
            }
            if (pos >= (int)sizeof(buf)) pos = (int)sizeof(buf) - 2;
            snprintf(buf + pos, sizeof(buf) - pos, "]");
            break;
        }
        case VAL_MAP: {
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "{");
            for (int i = 0; i < v.as.map->count && pos < 2000; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ", ");
                NcString *val_s = nc_value_to_string(v.as.map->values[i]);
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: %s",
                               v.as.map->keys[i]->chars, val_s->chars);
                nc_string_free(val_s);
            }
            if (pos >= (int)sizeof(buf)) pos = (int)sizeof(buf) - 2;
            snprintf(buf + pos, sizeof(buf) - pos, "}");
            break;
        }
        case VAL_TENSOR: {
            NcTensorHandle *t = v.as.tensor;
            int pos = 0;
            pos += snprintf(buf + pos, sizeof(buf) - pos, "<tensor [");
            for (int i = 0; i < t->ndim; i++) {
                if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, "x");
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%d", t->shape[i]);
            }
            pos += snprintf(buf + pos, sizeof(buf) - pos, "]");
            if (t->requires_grad) pos += snprintf(buf + pos, sizeof(buf) - pos, " grad");
            snprintf(buf + pos, sizeof(buf) - pos, ">");
            break;
        }
        default: return nc_string_from_cstr("<value>");
    }
    return nc_string_from_cstr(buf);
}
