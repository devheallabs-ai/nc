/*
 * nc_chunk.h — Bytecode opcodes and chunk container.
 *
 * A chunk holds compiled bytecode for one behavior.
 * Include this when working with the compiler or VM.
 */

#ifndef NC_CHUNK_H
#define NC_CHUNK_H

#include "nc_value.h"

#define NC_LOCALS_MAX 256

typedef enum {
    /* ── Core: Data ─────────────────────────────────────────── */
    OP_CONSTANT,        /* push constants[operand] onto stack       */
    OP_CONSTANT_LONG,   /* push constants[2-byte operand]           */
    OP_NONE,            /* push nil                                 */
    OP_TRUE,            /* push true                                */
    OP_FALSE,           /* push false                               */
    OP_POP,             /* discard top of stack                     */

    /* ── Core: Variables ────────────────────────────────────── */
    OP_GET_VAR,         /* push value of named global variable      */
    OP_SET_VAR,         /* pop → named global variable              */
    OP_GET_LOCAL,       /* push local slot[operand]                 */
    OP_SET_LOCAL,       /* pop → local slot[operand]                */

    /* ── Core: Fields / Indexes ─────────────────────────────── */
    OP_GET_FIELD,       /* pop map → push map[field_name]           */
    OP_SET_FIELD,       /* pop val, pop map → map[field_name] = val */
    OP_GET_INDEX,       /* pop idx, pop list → push list[idx]       */
    OP_SET_INDEX,       /* pop val, pop idx, pop list → list[idx]=v */
    OP_SLICE,           /* pop end, pop start, pop list → slice     */

    /* ── Core: Arithmetic ───────────────────────────────────── */
    OP_ADD,             /* pop b, pop a → push a + b                */
    OP_SUBTRACT,        /* pop b, pop a → push a - b                */
    OP_MULTIPLY,        /* pop b, pop a → push a * b                */
    OP_DIVIDE,          /* pop b, pop a → push a / b                */
    OP_MODULO,          /* pop b, pop a → push a % b                */
    OP_NEGATE,          /* pop a → push -a                          */

    /* ── Core: Logic ────────────────────────────────────────── */
    OP_NOT,             /* pop a → push !a                          */
    OP_EQUAL,           /* pop b, pop a → push a == b               */
    OP_NOT_EQUAL,       /* pop b, pop a → push a != b               */
    OP_ABOVE,           /* pop b, pop a → push a > b                */
    OP_BELOW,           /* pop b, pop a → push a < b                */
    OP_AT_LEAST,        /* pop b, pop a → push a >= b               */
    OP_AT_MOST,         /* pop b, pop a → push a <= b               */
    OP_IN,              /* pop b, pop a → push a in b               */
    OP_AND,             /* short-circuit AND                        */
    OP_OR,              /* short-circuit OR                         */

    /* ── Core: Control Flow ─────────────────────────────────── */
    OP_JUMP,            /* ip += 2-byte offset (unconditional)      */
    OP_JUMP_IF_FALSE,   /* if top is false: ip += offset            */
    OP_LOOP,            /* ip -= 2-byte offset (backward jump)      */

    /* ── Core: Functions ────────────────────────────────────── */
    OP_CALL,            /* call NC function with arg count          */
    OP_CALL_NATIVE,     /* call registered C function               */
    OP_RETURN,          /* return from current function             */
    OP_HALT,            /* stop execution                           */

    /* ── Core: Collections ──────────────────────────────────── */
    OP_MAKE_LIST,       /* pop N items → push list                  */
    OP_MAKE_MAP,        /* pop N key/val pairs → push map           */

    /* ── NC-Specific: I/O & AI ──────────────────────────────── */
    OP_GATHER,          /* read file/db/url → push content          */
    OP_STORE,           /* write file/db: pop path, pop content     */
    OP_ASK_AI,          /* call LLM with prompt → push response     */
    OP_NOTIFY,          /* send push notification                   */
    OP_LOG,             /* print to console/log output              */
    OP_EMIT,            /* emit event to listeners                  */
    OP_WAIT,            /* sleep N milliseconds                     */
    OP_RESPOND,         /* send HTTP response and return            */

    /* ══════════════════════════════════════════════════════════
     *  NC UI OPCODES — Virtual DOM / Reactive UI Runtime
     *
     *  These opcodes make NC UI a first-class compiled language,
     *  not just a template engine. The NC VM executes these to
     *  drive a virtual DOM, just as V8 drives React's reconciler.
     *
     *  Execution model:
     *    1. .ncui source → NC UI compiler → NC bytecode (these ops)
     *    2. NC VM executes bytecode on server OR in WASM in browser
     *    3. Virtual DOM tree built on stack, diffed, patched to real DOM
     *
     *  Stack discipline for UI opcodes:
     *    OP_UI_ELEMENT pushes a node handle onto the NC value stack.
     *    OP_UI_PROP / OP_UI_CHILD operate on the top node.
     *    OP_UI_END_ELEMENT pops the node, appending it to its parent.
     * ══════════════════════════════════════════════════════════ */

    /* ── NC UI: Virtual DOM ──────────────────────────────────── */
    OP_UI_ELEMENT,      /* push new VNode(tag_name) onto stack      */
                        /*   operand: constant index → tag string   */
                        /*   e.g. "div", "button", "input"          */

    OP_UI_PROP,         /* pop value, set prop on top VNode         */
                        /*   operand: constant index → prop name    */
                        /*   e.g. OP_UI_PROP "class" pops "card"    */

    OP_UI_PROP_EXPR,    /* same as OP_UI_PROP but value is computed */
                        /*   used for: style if state.x then "a"    */

    OP_UI_TEXT,         /* pop string → push VNode(text, content)   */
                        /*   used for: text "hello world"           */

    OP_UI_CHILD,        /* pop child VNode, append to top VNode     */
                        /*   builds the tree bottom-up on stack     */

    OP_UI_END_ELEMENT,  /* finalize top VNode, pop it as child      */
                        /*   marks end of element's children        */

    /* ── NC UI: State ───────────────────────────────────────── */
    OP_STATE_DECLARE,   /* declare reactive state slot              */
                        /*   operand: constant index → slot name    */
                        /*   initial value popped from stack        */

    OP_STATE_GET,       /* push value of reactive state slot        */
                        /*   operand: constant index → slot name    */
                        /*   always reads current reactive value    */

    OP_STATE_SET,       /* pop value → reactive state slot          */
                        /*   triggers diff/patch on all bindings    */
                        /*   operand: constant index → slot name    */

    OP_STATE_COMPUTED,  /* declare computed state (memoized)        */
                        /*   operand: function chunk index          */
                        /*   re-runs only when dependencies change  */

    OP_STATE_WATCH,     /* watch state slot → run callback chunk    */
                        /*   operand: slot name, callback chunk idx */

    /* ── NC UI: Bindings ────────────────────────────────────── */
    OP_UI_BIND,         /* bind state slot to top VNode's property  */
                        /*   creates live reactive link             */
                        /*   op1: prop name, op2: state slot name   */
                        /*   when state changes, DOM updates        */

    OP_UI_BIND_INPUT,   /* two-way bind: input value ↔ state slot   */
                        /*   wires onInput event + state_set        */
                        /*   op1: state slot name                   */

    /* ── NC UI: Events ──────────────────────────────────────── */
    OP_UI_ON_EVENT,     /* attach event handler chunk to top VNode  */
                        /*   op1: event name ("click","submit")     */
                        /*   op2: chunk index for handler           */

    /* ── NC UI: Component Lifecycle ────────────────────────── */
    OP_UI_COMPONENT,    /* define a reusable component              */
                        /*   operand: constant index → component name */
                        /*   pops render chunk from stack           */

    OP_UI_MOUNT,        /* mount component tree to real DOM         */
                        /*   triggers on-mount lifecycle callbacks  */

    OP_UI_UNMOUNT,      /* tear down component, run on-unmount      */
                        /*   frees event listeners, watchers        */

    OP_UI_ON_MOUNT,     /* register on-mount callback               */
                        /*   operand: chunk index                   */

    OP_UI_ON_UNMOUNT,   /* register on-unmount callback             */
                        /*   operand: chunk index                   */

    /* ── NC UI: Diffing & Patching ──────────────────────────── */
    OP_UI_RENDER,       /* run render chunk → produce VNode tree    */
                        /*   pushed result is new virtual tree      */

    OP_UI_DIFF,         /* pop new VNode tree, pop old VNode tree   */
                        /*   → push patch set (minimal DOM changes) */

    OP_UI_PATCH,        /* pop patch set → apply to real DOM        */
                        /*   this is the reconciliation step        */

    /* ── NC UI: Routing ─────────────────────────────────────── */
    OP_UI_ROUTE_DEF,    /* define route: path → component           */
                        /*   op1: path string, op2: component name  */

    OP_UI_ROUTE_PUSH,   /* navigate to path (push to history)       */
                        /*   operand: constant index → path string  */
                        /*   OR pops path from stack if operand=0   */

    OP_UI_ROUTE_GUARD,  /* check route access before mounting       */
                        /*   op1: condition chunk, op2: redirect    */
                        /*   if condition false → navigate redirect */

    OP_UI_ROUTE_MATCH,  /* push current matched route params        */
                        /*   pushes {path, params, query} map       */

    /* ── NC UI: Async / Fetch ───────────────────────────────── */
    OP_UI_FETCH,        /* async HTTP fetch with loading state      */
                        /*   pops: url, method, body, options map   */
                        /*   sets loading=true, pushes response     */
                        /*   automatically handles auth headers     */

    OP_UI_FETCH_AUTH,   /* same as OP_UI_FETCH + bearer token       */
                        /*   reads token from auth state            */

    /* ── NC UI: Control Flow (UI context) ───────────────────── */
    OP_UI_IF,           /* conditional render                       */
                        /*   pops condition, pops true-branch VNode */
                        /*   pops false-branch VNode (or nil)       */
                        /*   pushes one of them onto stack          */

    OP_UI_FOR_EACH,     /* repeat render for each item in list      */
                        /*   op1: state slot or local holding list  */
                        /*   op2: render chunk (receives item)      */
                        /*   pushes list of VNodes                  */

    OP_UI_SHOW,         /* set visibility binding on top VNode      */
                        /*   pops boolean condition                 */
                        /*   does NOT remove from DOM (display:none)*/

    /* ── NC UI: Forms ───────────────────────────────────────── */
    OP_UI_FORM,         /* define form with validation config       */
                        /*   op1: form name, op2: action chunk      */

    OP_UI_VALIDATE,     /* run validation rule on top input         */
                        /*   operand: constant index → rule name    */
                        /*   e.g. "required", "email", "min-length" */

    OP_UI_FORM_SUBMIT,  /* intercept submit, run validation, then   */
                        /*   call action chunk if all valid         */

    /* ── NC UI: Auth (RBAC / Guards) ───────────────────────── */
    OP_UI_AUTH_CHECK,   /* push true if user is authenticated       */

    OP_UI_ROLE_CHECK,   /* push true if user has role               */
                        /*   operand: constant index → role name    */

    OP_UI_PERM_CHECK,   /* push true if user has permission         */
                        /*   operand: constant index → "action:res" */

} NcOpCode;

typedef struct NcChunk NcChunk;

struct NcChunk {
    uint8_t  *code;
    int       count;
    int       capacity;
    int      *lines;
    NcValue  *constants;
    int       const_count;
    int       const_capacity;
    NcString **var_names;
    int       var_count;
    int       var_capacity;
};

NcChunk *nc_chunk_new(void);
void     nc_chunk_write(NcChunk *c, uint8_t byte, int line);
int      nc_chunk_add_constant(NcChunk *c, NcValue val);
int      nc_chunk_add_var(NcChunk *c, NcString *name);
void     nc_chunk_free(NcChunk *c);
void     nc_disassemble_chunk(NcChunk *chunk, const char *name);

#endif /* NC_CHUNK_H */
