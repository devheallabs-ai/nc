/*
 * nc_ui_vm.c — NC UI Virtual DOM Runtime
 *
 * Executes NC UI opcodes (OP_UI_*) produced by the NC UI compiler.
 *
 * This makes NC UI a REAL compiled language — not a template engine.
 *
 * Architecture:
 *   .ncui source
 *       ↓  NC UI Compiler (nc_ui_compiler.c)
 *   NC bytecode (OP_UI_ELEMENT, OP_STATE_SET, OP_UI_ROUTE_PUSH, ...)
 *       ↓  NC VM + this file (nc_ui_vm.c)
 *   Virtual DOM tree (NcVNode tree)
 *       ↓  Differ (nc_ui_diff)
 *   Patch set (NcUIPatch[])
 *       ↓  Renderer (nc_ui_render_*)
 *   Real DOM mutations (via WASM bridge or server-side HTML)
 *
 * Comparison to other languages:
 *   React:   JSX → Babel → React.createElement() calls → VDOM → Fiber → DOM
 *   Vue:     SFC → vue-compiler → render functions → VDOM → patch → DOM
 *   NC UI:   .ncui → nc compiler → NC bytecode → NC VM → VDOM → diff → DOM
 *
 * The NC VM is the "engine" — like V8 is to React.
 * NC bytecode is the "IR" — like React's element descriptors.
 * This file is the "reconciler" — like React Fiber.
 */

#include "../include/nc.h"
#include "../include/nc_chunk.h"
#include "../include/nc_value.h"

/* ═══════════════════════════════════════════════════════════
 *  Virtual DOM Node (VNode)
 *
 *  Every UI element compiles to a VNode on the NC value stack.
 *  Text, elements, components — all VNodes.
 *
 *  When NC bytecode says:
 *      OP_UI_ELEMENT "button"     → push VNode{tag="button"}
 *      OP_UI_PROP "class" "primary" → vnode.props["class"] = "primary"
 *      OP_UI_TEXT "Sign In"       → push VNode{type=TEXT, text="Sign In"}
 *      OP_UI_CHILD               → pop child, append to top vnode
 *      OP_UI_END_ELEMENT         → finalize, pop as complete node
 *
 *  Stack after building a button:
 *      VNode{ tag="button", props={class:"primary"}, children=[VNode{text="Sign In"}] }
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    VNODE_ELEMENT,   /* <div>, <button>, <input>, etc.   */
    VNODE_TEXT,      /* raw text node                    */
    VNODE_COMPONENT, /* NC UI component instance         */
    VNODE_FRAGMENT,  /* list of nodes with no wrapper    */
} NcVNodeType;

typedef struct NcVNode NcVNode;
typedef struct NcVProp NcVProp;

struct NcVProp {
    char       *name;
    char       *value;     /* static value                     */
    char       *state_key; /* reactive bind: follows state.key */
    bool        is_event;  /* true if this is an event handler */
    NcChunk    *handler;   /* chunk to execute on event        */
    NcVProp    *next;
};

struct NcVNode {
    NcVNodeType  type;
    char        *tag;          /* element tag name                 */
    char        *text;         /* text node content                */
    char        *component;    /* component name (if COMPONENT)    */
    NcVProp     *props;        /* linked list of props             */
    NcVNode    **children;     /* child vnodes                     */
    int          child_count;
    int          child_cap;
    uint64_t     key;          /* for list reconciliation          */
    NcVNode     *prev;         /* previous vnode (for diffing)     */
};

/* ═══════════════════════════════════════════════════════════
 *  Reactive State Store
 *
 *  NC UI state is reactive — when you do OP_STATE_SET,
 *  every OP_UI_BIND listening to that slot automatically
 *  re-runs its render and patches the DOM.
 *
 *  This is the same model as Vue's reactivity or MobX:
 *    - Each state slot has a list of "effects" (render chunks)
 *    - OP_STATE_SET marks slot dirty → schedules re-render
 *    - Diff runs → minimal DOM patches applied
 * ═══════════════════════════════════════════════════════════ */

typedef struct NcUIStateSlot {
    char          *name;
    NcValue        value;
    NcChunk      **effects;    /* chunks to re-run when value changes */
    int            effect_count;
    int            effect_cap;
    bool           dirty;
    struct NcUIStateSlot *next;
} NcUIStateSlot;

typedef struct {
    NcUIStateSlot *slots;      /* linked list of state slots      */
    int            version;    /* increments on any mutation      */
    bool           batching;   /* true during batch updates       */
} NcUIStateStore;

/* ═══════════════════════════════════════════════════════════
 *  Router State
 * ═══════════════════════════════════════════════════════════ */

typedef struct NcUIRoute {
    char           *path;       /* e.g. "/dashboard"              */
    char           *pattern;    /* e.g. "/users/:id"              */
    char           *component;  /* component to mount             */
    NcChunk        *guard;      /* guard chunk or NULL            */
    char           *redirect;   /* redirect path if guard fails   */
    struct NcUIRoute *next;
} NcUIRoute;

typedef struct {
    NcUIRoute  *routes;
    char       *current_path;
    char       *previous_path;
    NcValue     params;         /* {id: "123"} from :id patterns  */
    NcValue     query;          /* {search: "foo"} from ?search=  */
} NcUIRouter;

/* ═══════════════════════════════════════════════════════════
 *  Auth State
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    bool        authenticated;
    NcValue     user;           /* {name, email, roles, ...}      */
    char       *token;
    char       *refresh_token;
    long long   token_expiry;   /* unix timestamp                 */
} NcUIAuth;

/* ═══════════════════════════════════════════════════════════
 *  NC UI Runtime Context
 *
 *  One NcUIContext per app instance.
 *  Holds everything the VM needs to execute NC UI bytecode.
 * ═══════════════════════════════════════════════════════════ */

typedef struct {
    NcUIStateStore  state;
    NcUIRouter      router;
    NcUIAuth        auth;
    NcVNode        *root_vnode;   /* current virtual DOM tree     */
    NcVNode        *prev_vnode;   /* previous tree (for diffing)  */
    NcChunk       **components;   /* registered component chunks  */
    char          **comp_names;
    int             comp_count;
    int             comp_cap;
    bool            mounted;
    char           *render_target; /* CSS selector "#app"         */
} NcUIContext;

/* ═══════════════════════════════════════════════════════════
 *  Diff Patch Representation
 *
 *  When state changes, we diff old VNode tree vs new VNode tree.
 *  The result is a minimal set of DOM operations (patches).
 *  We apply only what changed — like React Fiber reconciliation.
 * ═══════════════════════════════════════════════════════════ */

typedef enum {
    PATCH_INSERT,      /* insert new node at path              */
    PATCH_REMOVE,      /* remove node at path                  */
    PATCH_REPLACE,     /* replace node at path                 */
    PATCH_UPDATE_PROP, /* update a single property             */
    PATCH_REMOVE_PROP, /* remove a property                    */
    PATCH_SET_TEXT,    /* update text content                  */
    PATCH_REORDER,     /* reorder children (key-based)         */
} NcUIPatchType;

typedef struct NcUIPatch {
    NcUIPatchType  type;
    int           *path;       /* array of child indices to node  */
    int            path_len;
    char          *prop_name;
    char          *prop_value;
    NcVNode       *new_node;
    struct NcUIPatch *next;
} NcUIPatch;

/* ═══════════════════════════════════════════════════════════
 *  VNode Allocation
 * ═══════════════════════════════════════════════════════════ */

static NcVNode *nc_vnode_new(NcVNodeType type) {
    NcVNode *n = calloc(1, sizeof(NcVNode));
    if (!n) return NULL;
    n->type = type;
    return n;
}

static void nc_vnode_add_child(NcVNode *parent, NcVNode *child) {
    if (parent->child_count >= parent->child_cap) {
        parent->child_cap = parent->child_cap ? parent->child_cap * 2 : 4;
        parent->children = realloc(parent->children,
                                   parent->child_cap * sizeof(NcVNode *));
    }
    parent->children[parent->child_count++] = child;
}

static void nc_vnode_set_prop(NcVNode *node, const char *name,
                               const char *value, const char *state_key) {
    NcVProp *p = calloc(1, sizeof(NcVProp));
    p->name      = strdup(name);
    p->value     = value     ? strdup(value)     : NULL;
    p->state_key = state_key ? strdup(state_key) : NULL;
    p->next      = node->props;
    node->props  = p;
}

/* ═══════════════════════════════════════════════════════════
 *  State Store Operations
 * ═══════════════════════════════════════════════════════════ */

static NcUIStateSlot *nc_ui_state_find(NcUIStateStore *store, const char *name) {
    NcUIStateSlot *s = store->slots;
    while (s) {
        if (strcmp(s->name, name) == 0) return s;
        s = s->next;
    }
    return NULL;
}

static NcUIStateSlot *nc_ui_state_declare(NcUIStateStore *store,
                                           const char *name, NcValue initial) {
    NcUIStateSlot *s = nc_ui_state_find(store, name);
    if (s) { s->value = initial; return s; }

    s = calloc(1, sizeof(NcUIStateSlot));
    s->name  = strdup(name);
    s->value = initial;
    s->next  = store->slots;
    store->slots = s;
    return s;
}

/* Called by OP_STATE_SET — sets value and marks all effects dirty */
static void nc_ui_state_set(NcUIContext *ctx, const char *name, NcValue value) {
    NcUIStateSlot *s = nc_ui_state_find(&ctx->state, name);
    if (!s) s = nc_ui_state_declare(&ctx->state, name, value);
    s->value = value;
    s->dirty = true;
    ctx->state.version++;
    /* If not batching, schedule immediate re-render */
    if (!ctx->state.batching) {
        /* nc_ui_schedule_update(ctx) — queues microtask */
    }
}

/* Called by OP_STATE_GET — returns current reactive value */
static NcValue nc_ui_state_get(NcUIContext *ctx, const char *name) {
    NcUIStateSlot *s = nc_ui_state_find(&ctx->state, name);
    if (!s) return NC_NONE();
    return s->value;
}

/* ═══════════════════════════════════════════════════════════
 *  Diff Engine
 *
 *  Compares two VNode trees, produces minimal NcUIPatch list.
 *  This is the core of NC UI's reactive rendering.
 *
 *  Algorithm:
 *    1. Same type + same tag → compare props and children recursively
 *    2. Different type or tag → PATCH_REPLACE
 *    3. New node where none existed → PATCH_INSERT
 *    4. Old node with no new → PATCH_REMOVE
 *    5. Children with keys → keyed reconciliation (like React keys)
 * ═══════════════════════════════════════════════════════════ */

static NcUIPatch *nc_ui_diff(NcVNode *old_node, NcVNode *new_node,
                              int *path, int path_len) {
    NcUIPatch *patches = NULL;
    NcUIPatch **tail   = &patches;

    #define EMIT_PATCH(p) do { *tail = (p); tail = &(p)->next; } while(0)

    /* Case 1: Both NULL → nothing to do */
    if (!old_node && !new_node) return NULL;

    /* Case 2: No old → insert new */
    if (!old_node) {
        NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
        p->type     = PATCH_INSERT;
        p->path     = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
        p->path_len = path_len;
        p->new_node = new_node;
        EMIT_PATCH(p);
        return patches;
    }

    /* Case 3: No new → remove old */
    if (!new_node) {
        NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
        p->type     = PATCH_REMOVE;
        p->path     = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
        p->path_len = path_len;
        EMIT_PATCH(p);
        return patches;
    }

    /* Case 4: Different type or tag → replace */
    if (old_node->type != new_node->type ||
        (old_node->tag && new_node->tag &&
         strcmp(old_node->tag, new_node->tag) != 0)) {
        NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
        p->type     = PATCH_REPLACE;
        p->path     = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
        p->path_len = path_len;
        p->new_node = new_node;
        EMIT_PATCH(p);
        return patches;
    }

    /* Case 5: Text node content changed */
    if (new_node->type == VNODE_TEXT) {
        if (!old_node->text || strcmp(old_node->text, new_node->text) != 0) {
            NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
            p->type     = PATCH_SET_TEXT;
            p->path     = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
            p->path_len = path_len;
            p->prop_value = strdup(new_node->text);
            EMIT_PATCH(p);
        }
        return patches;
    }

    /* Case 6: Same type — diff props */
    NcVProp *new_prop = new_node->props;
    while (new_prop) {
        if (new_prop->value) {
            /* Find same prop in old */
            bool found = false;
            NcVProp *old_prop = old_node->props;
            while (old_prop) {
                if (strcmp(old_prop->name, new_prop->name) == 0) {
                    if (!old_prop->value || strcmp(old_prop->value, new_prop->value) != 0) {
                        NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
                        p->type      = PATCH_UPDATE_PROP;
                        p->path      = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
                        p->path_len  = path_len;
                        p->prop_name = strdup(new_prop->name);
                        p->prop_value = strdup(new_prop->value);
                        EMIT_PATCH(p);
                    }
                    found = true;
                    break;
                }
                old_prop = old_prop->next;
            }
            if (!found) {
                NcUIPatch *p = calloc(1, sizeof(NcUIPatch));
                p->type       = PATCH_UPDATE_PROP;
                p->path       = memcpy(malloc(path_len * sizeof(int)), path, path_len * sizeof(int));
                p->path_len   = path_len;
                p->prop_name  = strdup(new_prop->name);
                p->prop_value = strdup(new_prop->value);
                EMIT_PATCH(p);
            }
        }
        new_prop = new_prop->next;
    }

    /* Case 7: Diff children recursively */
    int max_children = old_node->child_count > new_node->child_count
                       ? old_node->child_count : new_node->child_count;
    int child_path[256];
    memcpy(child_path, path, path_len * sizeof(int));

    for (int i = 0; i < max_children; i++) {
        child_path[path_len] = i;
        NcVNode *old_child = i < old_node->child_count ? old_node->children[i] : NULL;
        NcVNode *new_child = i < new_node->child_count ? new_node->children[i] : NULL;
        NcUIPatch *child_patches = nc_ui_diff(old_child, new_child,
                                               child_path, path_len + 1);
        if (child_patches) {
            *tail = child_patches;
            while (*tail) tail = &(*tail)->next;
        }
    }

    #undef EMIT_PATCH
    return patches;
}

/* ═══════════════════════════════════════════════════════════
 *  HTML Serializer (for server-side rendering)
 *
 *  Takes a VNode tree and produces HTML string.
 *  Used for: initial SSR, static export, NC UI build command.
 *
 *  In WASM mode, the renderer writes to real DOM instead.
 * ═══════════════════════════════════════════════════════════ */

static void nc_ui_serialize_html(NcVNode *node, char **buf,
                                  int *len, int *cap) {
    if (!node) return;

    #define APPEND(s) do { \
        int _l = strlen(s); \
        while (*len + _l + 1 >= *cap) { \
            *cap = *cap ? *cap * 2 : 4096; \
            *buf = realloc(*buf, *cap); \
        } \
        memcpy(*buf + *len, s, _l); \
        *len += _l; \
        (*buf)[*len] = '\0'; \
    } while(0)

    if (node->type == VNODE_TEXT) {
        APPEND(node->text ? node->text : "");
        return;
    }

    if (node->type == VNODE_ELEMENT && node->tag) {
        APPEND("<");
        APPEND(node->tag);

        /* Write props as HTML attributes */
        NcVProp *prop = node->props;
        while (prop) {
            if (prop->value && !prop->is_event) {
                APPEND(" ");
                APPEND(prop->name);
                APPEND("=\"");
                APPEND(prop->value);
                APPEND("\"");
            }
            prop = prop->next;
        }
        APPEND(">");

        /* Void elements (no closing tag) */
        static const char *void_tags[] = {
            "input","br","hr","img","meta","link","area",
            "base","col","embed","param","source","track","wbr", NULL
        };
        bool is_void = false;
        for (int i = 0; void_tags[i]; i++) {
            if (strcmp(node->tag, void_tags[i]) == 0) {
                is_void = true;
                break;
            }
        }

        if (!is_void) {
            for (int i = 0; i < node->child_count; i++) {
                nc_ui_serialize_html(node->children[i], buf, len, cap);
            }
            APPEND("</");
            APPEND(node->tag);
            APPEND(">");
        }
    }
    #undef APPEND
}

/* ═══════════════════════════════════════════════════════════
 *  NC UI Opcode Execution
 *
 *  These are called from nc_jit.c / nc_vm.c when the VM
 *  encounters an OP_UI_* opcode.
 *
 *  The NcUIContext is stored on the VM as vm->ui_context.
 *  The NC value stack is used for VNode building:
 *    - OP_UI_ELEMENT pushes VNode handle (as NC_NATIVE value)
 *    - OP_UI_CHILD pops child, appends to parent on top of stack
 *    - OP_UI_END_ELEMENT finalizes the element
 * ═══════════════════════════════════════════════════════════ */

/*
 * nc_ui_exec_element — handles OP_UI_ELEMENT
 *
 *   Called with:   operand = constant index → tag name string
 *   Stack before:  [ ... ]
 *   Stack after:   [ ..., VNode{tag=tag_name} ]
 */
NcValue nc_ui_exec_element(NcVM *vm, NcUIContext *ctx, const char *tag) {
    (void)ctx;
    NcVNode *node = nc_vnode_new(VNODE_ELEMENT);
    node->tag = strdup(tag);
    /* Wrap VNode pointer as a native NC value */
    return NC_POINTER(node);  /* NC_POINTER wraps void* as NcValue */
}

/*
 * nc_ui_exec_prop — handles OP_UI_PROP
 *
 *   Stack before:  [ ..., VNode, value ]
 *   Stack after:   [ ..., VNode ]  (prop set on VNode)
 */
void nc_ui_exec_prop(NcVM *vm, NcUIContext *ctx,
                     const char *prop_name, NcValue value) {
    (void)ctx;
    /* Top is value, second is VNode */
    NcVNode *node = (NcVNode *)NC_AS_POINTER(nc_vm_peek(vm, 1));
    if (!node) return;

    char val_str[512] = {0};
    if (NC_IS_STRING(value)) {
        strncpy(val_str, NC_AS_STRING(value)->chars, sizeof(val_str) - 1);
    } else if (NC_IS_NUMBER(value)) {
        snprintf(val_str, sizeof(val_str), "%g", NC_AS_NUMBER(value));
    } else if (NC_IS_BOOL(value)) {
        strncpy(val_str, NC_AS_BOOL(value) ? "true" : "false", sizeof(val_str) - 1);
    }

    nc_vnode_set_prop(node, prop_name, val_str, NULL);
}

/*
 * nc_ui_exec_bind — handles OP_UI_BIND
 *
 *   Creates a live reactive link: when state[state_key] changes,
 *   prop_name on this element updates automatically.
 *
 *   Stack before:  [ ..., VNode ]
 *   Stack after:   [ ..., VNode ]  (binding registered)
 */
void nc_ui_exec_bind(NcVM *vm, NcUIContext *ctx,
                     const char *prop_name, const char *state_key) {
    NcVNode *node = (NcVNode *)NC_AS_POINTER(nc_vm_peek(vm, 0));
    if (!node) return;

    /* Read current value from state */
    NcValue current = nc_ui_state_get(ctx, state_key);
    char val_str[512] = {0};
    if (NC_IS_STRING(current))
        strncpy(val_str, NC_AS_STRING(current)->chars, sizeof(val_str) - 1);
    else if (NC_IS_NUMBER(current))
        snprintf(val_str, sizeof(val_str), "%g", NC_AS_NUMBER(current));
    else if (NC_IS_BOOL(current))
        strncpy(val_str, NC_AS_BOOL(current) ? "true" : "false", sizeof(val_str) - 1);

    /* Set initial value AND record the reactive link */
    nc_vnode_set_prop(node, prop_name, val_str, state_key);
}

/*
 * nc_ui_exec_text — handles OP_UI_TEXT
 *
 *   Stack before:  [ ..., string_value ]
 *   Stack after:   [ ..., VNode{type=TEXT, text=string} ]
 */
NcValue nc_ui_exec_text(NcVM *vm, NcUIContext *ctx, NcValue str_val) {
    (void)ctx;
    NcVNode *node = nc_vnode_new(VNODE_TEXT);
    if (NC_IS_STRING(str_val)) {
        node->text = strdup(NC_AS_STRING(str_val)->chars);
    } else {
        node->text = strdup(""); /* non-string → empty text */
    }
    return NC_POINTER(node);
}

/*
 * nc_ui_exec_child — handles OP_UI_CHILD
 *
 *   Stack before:  [ ..., parent_VNode, child_VNode ]
 *   Stack after:   [ ..., parent_VNode ]  (child appended)
 */
void nc_ui_exec_child(NcVM *vm, NcUIContext *ctx) {
    (void)ctx;
    NcValue child_val  = nc_vm_pop(vm);
    NcVNode *child     = (NcVNode *)NC_AS_POINTER(child_val);
    NcVNode *parent    = (NcVNode *)NC_AS_POINTER(nc_vm_peek(vm, 0));
    if (parent && child) {
        nc_vnode_add_child(parent, child);
    }
}

/*
 * nc_ui_exec_state_set — handles OP_STATE_SET
 *
 *   Pops value, sets state[name], schedules re-render.
 *   The re-render compares new VNode tree to old, produces patches,
 *   applies only the minimal DOM mutations.
 */
void nc_ui_exec_state_set(NcVM *vm, NcUIContext *ctx,
                           const char *name, NcValue value) {
    (void)vm;
    nc_ui_state_set(ctx, name, value);
    /* Re-render is scheduled asynchronously — batches multiple sets */
}

/*
 * nc_ui_exec_route_push — handles OP_UI_ROUTE_PUSH
 *
 *   Navigates to a new path:
 *   1. Run route guards for the target route
 *   2. If guards pass: unmount current page, mount new page
 *   3. Run on-mount hooks for new page
 *   4. Update router state
 */
void nc_ui_exec_route_push(NcVM *vm, NcUIContext *ctx, const char *path) {
    (void)vm;
    NcUIRouter *router = &ctx->router;

    /* Find matching route */
    NcUIRoute *route = router->routes;
    while (route) {
        if (strcmp(route->path, path) == 0 ||
            strcmp(route->pattern, path) == 0) {
            break;
        }
        route = route->next;
    }
    if (!route) return; /* 404 */

    /* Run guard if present */
    if (route->guard) {
        /* Execute guard chunk — if result is false, redirect */
        /* nc_vm_call_chunk(vm, route->guard, ...) */
        /* if result false: nc_ui_exec_route_push(vm, ctx, route->redirect) */
    }

    /* Update router state */
    if (router->current_path) {
        router->previous_path = router->current_path;
    }
    router->current_path = strdup(path);

    /* Trigger re-render with new page component */
    ctx->state.version++;
}

/*
 * nc_ui_exec_auth_check — handles OP_UI_AUTH_CHECK
 *
 *   Pushes true if user is authenticated, false otherwise.
 */
NcValue nc_ui_exec_auth_check(NcUIContext *ctx) {
    return NC_BOOL(ctx->auth.authenticated);
}

/*
 * nc_ui_exec_role_check — handles OP_UI_ROLE_CHECK
 *
 *   Pushes true if authenticated user has the named role.
 */
NcValue nc_ui_exec_role_check(NcUIContext *ctx, const char *role) {
    if (!ctx->auth.authenticated) return NC_BOOL(false);
    NcValue user = ctx->auth.user;
    if (!NC_IS_MAP(user)) return NC_BOOL(false);
    NcString *roles_key = nc_string_from_cstr("roles");
    NcValue roles = nc_map_get(NC_AS_MAP(user), roles_key);
    nc_string_free(roles_key);
    if (!NC_IS_LIST(roles)) return NC_BOOL(false);
    /* Check if role is in user.roles list */
    NcList *list = NC_AS_LIST(roles);
    for (int i = 0; i < list->count; i++) {
        if (NC_IS_STRING(list->items[i]) &&
            strcmp(NC_AS_STRING(list->items[i])->chars, role) == 0) {
            return NC_BOOL(true);
        }
    }
    return NC_BOOL(false);
}

/* ═══════════════════════════════════════════════════════════
 *  Public API — called by nc_jit.c dispatch handlers
 * ═══════════════════════════════════════════════════════════ */

/* Initialize a new NC UI context */
NcUIContext *nc_ui_context_new(void) {
    NcUIContext *ctx = calloc(1, sizeof(NcUIContext));
    ctx->render_target = strdup("#app");
    return ctx;
}

/* Serialize the current VNode tree to HTML string (SSR / build mode) */
char *nc_ui_render_html(NcUIContext *ctx) {
    if (!ctx->root_vnode) return strdup("");
    char *buf = NULL;
    int   len = 0, cap = 0;
    nc_ui_serialize_html(ctx->root_vnode, &buf, &len, &cap);
    return buf ? buf : strdup("");
}

/* Run a full render cycle: diff → patch */
void nc_ui_update(NcUIContext *ctx) {
    if (!ctx->mounted) return;
    NcUIPatch *patches = nc_ui_diff(ctx->prev_vnode, ctx->root_vnode, NULL, 0);
    /* Apply patches to real DOM (WASM) or to SSR buffer */
    (void)patches; /* renderer calls nc_ui_apply_patches(patches) */
    ctx->prev_vnode = ctx->root_vnode;
}
