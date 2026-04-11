/*
 * nc_ast.h — Abstract Syntax Tree nodes for NC.
 *
 * Defines the AST produced by the parser and consumed by
 * the compiler, interpreter, LLVM backend, and semantic analyzer.
 */

#ifndef NC_AST_H
#define NC_AST_H

#include "nc_value.h"

typedef struct NcASTNode NcASTNode;

typedef enum {
    NODE_PROGRAM, NODE_IMPORT, NODE_MIDDLEWARE, NODE_DEFINITION, NODE_FIELD, NODE_BEHAVIOR, NODE_PARAM,
    /* Statements */
    NODE_GATHER, NODE_ASK_AI, NODE_IF, NODE_REPEAT, NODE_MATCH, NODE_WHEN,
    NODE_RUN, NODE_RESPOND, NODE_NOTIFY, NODE_WAIT, NODE_SET,
    NODE_STORE, NODE_LOG, NODE_SHOW, NODE_EMIT, NODE_APPLY, NODE_CHECK,
    NODE_TRY, NODE_STOP, NODE_SKIP, NODE_EXPR_STMT, NODE_APPEND,
    NODE_WHILE, NODE_FOR_COUNT, NODE_SET_INDEX,
    NODE_ASSERT, NODE_TEST_BLOCK,
    NODE_ASYNC_CALL, NODE_AWAIT, NODE_YIELD,
    NODE_STREAM_RESPOND,
    /* Expressions */
    NODE_COMPARISON, NODE_LOGIC, NODE_NOT, NODE_MATH, NODE_DOT,
    NODE_INDEX, NODE_SLICE, NODE_CALL, NODE_LIST_LIT, NODE_MAP_LIT,
    NODE_IDENT, NODE_STRING_LIT, NODE_INT_LIT, NODE_FLOAT_LIT,
    NODE_BOOL_LIT, NODE_NONE_LIT, NODE_TEMPLATE,
    /* API */
    NODE_ROUTE, NODE_EVENT_HANDLER, NODE_SCHEDULE_HANDLER,
    /* Generics & Interfaces */
    NODE_INTERFACE, NODE_IMPL, NODE_TYPE_PARAM,
    /* Decorators */
    NODE_DECORATOR,
    /* Operator overloading */
    NODE_OPERATOR_DEF,
    /* Pattern matching extensions */
    NODE_MATCH_GUARD, NODE_DESTRUCTURE, NODE_REST_PATTERN,
    /* Agents */
    NODE_AGENT_DEF, NODE_RUN_AGENT,
} NcNodeType;

struct NcASTNode {
    NcNodeType type;
    int line;

    union {
        struct {
            NcString *service_name, *version, *model, *description;
            NcASTNode **imports;      int import_count;
            NcASTNode **definitions;  int def_count;
            NcASTNode **behaviors;    int beh_count;
            NcASTNode **routes;       int route_count;
            NcASTNode **events;       int event_count;
            NcASTNode **middleware;   int mw_count;
            NcASTNode **agents;      int agent_count;
            NcMap     *configure;
        } program;

        struct {
            NcString *name;
            NcASTNode **fields; int field_count;
            NcString **implements; int impl_count;  /* interfaces this type implements */
            NcASTNode **operators; int op_count;     /* operator overloads */
        } definition;
        struct { NcString *name; NcString *type_name; bool optional; } field;

        struct {
            NcString *name;
            NcString *purpose;
            NcASTNode **params;    int param_count;
            NcASTNode **body;      int body_count;
            NcASTNode *needs_approval;
            NcASTNode **decorators; int decorator_count;  /* @log, @cache, etc. */
            NcString **type_params; int type_param_count; /* generics: of type T, U */
        } behavior;
        struct { NcString *name; } param;

        struct { NcString *target; NcString *source; NcMap *options; } gather;

        struct {
            NcString *prompt;
            NcString **using;      int using_count;
            NcString *save_as;
            NcString *model;
            double    confidence;
            NcMap    *options;
        } ask_ai;

        struct {
            NcASTNode *condition;
            NcASTNode **then_body;  int then_count;
            NcASTNode **else_body;  int else_count;
        } if_stmt;

        struct {
            NcString *variable;
            NcString *key_variable;
            NcASTNode *iterable;
            NcASTNode **body;  int body_count;
        } repeat;

        struct {
            NcASTNode *subject;
            NcASTNode **cases;  int case_count;
            NcASTNode **otherwise; int otherwise_count;
        } match_stmt;
        struct { NcASTNode *value; NcASTNode **body; int body_count; } when_clause;

        struct { NcString *name; NcASTNode **args; int arg_count; } run_stmt;
        struct { NcASTNode *value; } single_expr;
        struct { NcASTNode *channel; NcASTNode *message; } notify;
        struct { double amount; NcString *unit; } wait_stmt;
        struct { NcString *target; NcString *field; NcString *subfield; NcASTNode *value; } set_stmt;
        struct { NcString *target; NcString *field; NcASTNode *index; NcASTNode *value; } set_index;
        struct { NcASTNode *value; NcString *target; } store_stmt;
        struct { NcASTNode *value; NcString *target; } append_stmt;
        struct { NcASTNode *left; NcASTNode *right; NcString *op; } comparison;
        struct { NcASTNode *left; NcASTNode *right; NcString *op; } logic;
        struct { NcASTNode *left; NcASTNode *right; char op; } math;
        struct { NcASTNode *object; NcASTNode *start; NcASTNode *end; } slice;
        struct { NcASTNode *object; NcString *member; } dot;
        struct { NcString *name; NcASTNode **args; int arg_count; } call;
        struct { NcString *value; } string_lit;
        struct { int64_t value; } int_lit;
        struct { double value; } float_lit;
        struct { bool value; } bool_lit;
        struct { NcString *expr; } template_lit;
        struct { NcString *name; } ident;
        struct { NcASTNode **elements; int count; } list_lit;
        struct { NcString **keys; NcASTNode **values; int count; } map_lit;
        struct { NcString *method; NcString *path; NcString *handler; } route;
        struct { NcString *target; NcString *using; NcMap *options; } apply;
        struct { NcString *desc; NcString *using; NcString *save_as; } check;

        struct {
            NcASTNode *condition;
            NcASTNode **body; int body_count;
        } while_stmt;

        struct {
            NcASTNode *count_expr;
            NcString *variable;
            NcASTNode **body; int body_count;
        } for_count;

        struct {
            NcASTNode **body; int body_count;
            NcASTNode **error_body; int error_count;
            NcASTNode **finally_body; int finally_count;
            NcString *error_type;  /* optional: catch specific error type */
        } try_stmt;

        /* assert <condition>, "message" */
        struct { NcASTNode *condition; NcASTNode *message; } assert_stmt;
        /* test "name": ... body ... */
        struct { NcString *name; NcASTNode **body; int body_count; } test_block;
        /* async run behavior / await result */
        struct { NcString *name; NcASTNode **args; int arg_count; } async_call;
        /* yield value — for streaming/generators */
        struct { NcASTNode *value; } yield_stmt;
        /* stream respond with ... — SSE streaming response */
        struct { NcASTNode *value; NcString *event_type; } stream_respond;

        struct { NcString *event_name; NcASTNode **body; int body_count; } event_handler;
        struct { NcString *interval; NcASTNode **body; int body_count; } schedule_handler;
        struct { NcString *module; NcString *alias; } import_decl;
        struct { NcString *name; NcMap *options; } middleware;

        /* interface Printable: requires display, requires format */
        struct {
            NcString *name;
            NcString **required_behaviors; int required_count;
        } interface_def;

        /* define User implements Printable, Serializable */
        struct {
            NcString **interfaces; int interface_count;
        } impl;

        /* @log @cache ttl 60 — decorator applied to next behavior */
        struct {
            NcString *name;
            NcMap *options;            /* decorator arguments */
        } decorator;

        /* operator + with other: ... */
        struct {
            char op;                   /* +, -, *, / */
            NcString *param_name;      /* "other" */
            NcASTNode **body; int body_count;
        } operator_def;

        /* when {name, age}: — destructured pattern */
        struct {
            NcString **fields; int field_count;
            NcASTNode *guard;          /* optional: if condition */
            NcASTNode **body; int body_count;
        } match_guard;

        /* [first, ...rest] — rest/spread pattern */
        struct {
            NcString *name;            /* "rest" */
        } rest_pattern;

        /* agent researcher: purpose "..." tools [...] max_steps N */
        struct {
            NcString *name;
            NcString *purpose;
            NcString *model;
            NcString **tools;    int tool_count;
            int       max_steps;       /* default 10 */
        } agent_def;

        /* run agent researcher with "prompt" save as answer */
        struct {
            NcString  *agent_name;
            NcASTNode *prompt;         /* expression for the task */
            NcString  *save_as;        /* variable to store result */
        } run_agent;
    } as;
};

NcASTNode *nc_ast_new(NcNodeType type, int line);
void       nc_ast_free(NcASTNode *node);

#endif /* NC_AST_H */
