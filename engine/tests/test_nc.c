/*
 * test_nc.c — Comprehensive test suite for the NC language runtime.
 *
 * Covers: values, strings, lists, maps, lexer, parser, compiler,
 *         VM, JIT, interpreter, JSON, stdlib, and end-to-end programs.
 *
 * Run with: make test-unit
 */

#include "nc.h"
#include "../include/nc_platform.h"

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_total  = 0;

#define ASSERT(cond, msg) do { \
    tests_total++; \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("    FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) ASSERT((a) == (b), msg)
#define ASSERT_EQ_STR(a, b, msg) ASSERT(strcmp(a, b) == 0, msg)
#define ASSERT_TRUE(a, msg) ASSERT((a), msg)
#define ASSERT_FALSE(a, msg) ASSERT(!(a), msg)
#define ASSERT_NEAR(a, b, eps, msg) ASSERT(fabs((a)-(b)) < (eps), msg)

#define SECTION(name) printf("\n  ── %s ──\n", name)
#define SUBSECTION(name) printf("    %s\n", name)

/* ═══════════════════════════════════════════════════════════
 *  1. VALUE SYSTEM
 * ═══════════════════════════════════════════════════════════ */

static void test_value_constructors(void) {
    SECTION("Value Constructors");

    NcValue none = NC_NONE();
    ASSERT_TRUE(IS_NONE(none), "NC_NONE() is none");
    ASSERT_FALSE(IS_INT(none), "none is not int");

    NcValue b_true = NC_BOOL(true);
    NcValue b_false = NC_BOOL(false);
    ASSERT_TRUE(IS_BOOL(b_true), "NC_BOOL(true) is bool");
    ASSERT_TRUE(AS_BOOL(b_true), "true value is true");
    ASSERT_FALSE(AS_BOOL(b_false), "false value is false");

    NcValue i = NC_INT(42);
    ASSERT_TRUE(IS_INT(i), "NC_INT is int");
    ASSERT_EQ_INT(AS_INT(i), 42, "int value 42");
    ASSERT_TRUE(IS_NUMBER(i), "int is number");

    NcValue neg = NC_INT(-100);
    ASSERT_EQ_INT(AS_INT(neg), -100, "negative int");

    NcValue zero = NC_INT(0);
    ASSERT_EQ_INT(AS_INT(zero), 0, "zero int");

    NcValue f = NC_FLOAT(3.14);
    ASSERT_TRUE(IS_FLOAT(f), "NC_FLOAT is float");
    ASSERT_NEAR(AS_FLOAT(f), 3.14, 0.001, "float value 3.14");
    ASSERT_TRUE(IS_NUMBER(f), "float is number");

    NcString *s = nc_string_from_cstr("test");
    NcValue sv = NC_STRING(s);
    ASSERT_TRUE(IS_STRING(sv), "NC_STRING is string");
    ASSERT_EQ_STR(AS_STRING(sv)->chars, "test", "string value");

    NcList *l = nc_list_new();
    NcValue lv = NC_LIST(l);
    ASSERT_TRUE(IS_LIST(lv), "NC_LIST is list");

    NcMap *m = nc_map_new();
    NcValue mv = NC_MAP(m);
    ASSERT_TRUE(IS_MAP(mv), "NC_MAP is map");

    nc_string_free(s);
    nc_list_free(l);
    nc_map_free(m);
}

static void test_truthiness(void) {
    SECTION("Truthiness");

    ASSERT_FALSE(nc_truthy(NC_NONE()), "none is falsy");
    ASSERT_FALSE(nc_truthy(NC_BOOL(false)), "false is falsy");
    ASSERT_TRUE(nc_truthy(NC_BOOL(true)), "true is truthy");
    ASSERT_FALSE(nc_truthy(NC_INT(0)), "0 is falsy");
    ASSERT_TRUE(nc_truthy(NC_INT(1)), "1 is truthy");
    ASSERT_TRUE(nc_truthy(NC_INT(-1)), "-1 is truthy");
    ASSERT_FALSE(nc_truthy(NC_FLOAT(0.0)), "0.0 is falsy");
    ASSERT_TRUE(nc_truthy(NC_FLOAT(0.1)), "0.1 is truthy");

    NcString *empty = nc_string_from_cstr("");
    ASSERT_FALSE(nc_truthy(NC_STRING(empty)), "empty string falsy");
    NcString *nonempty = nc_string_from_cstr("x");
    ASSERT_TRUE(nc_truthy(NC_STRING(nonempty)), "nonempty string truthy");

    NcList *empty_list = nc_list_new();
    ASSERT_FALSE(nc_truthy(NC_LIST(empty_list)), "empty list falsy");
    NcList *full_list = nc_list_new();
    nc_list_push(full_list, NC_INT(1));
    ASSERT_TRUE(nc_truthy(NC_LIST(full_list)), "nonempty list truthy");

    NcMap *empty_map = nc_map_new();
    ASSERT_FALSE(nc_truthy(NC_MAP(empty_map)), "empty map falsy");
    NcMap *full_map = nc_map_new();
    nc_map_set(full_map, nc_string_from_cstr("k"), NC_INT(1));
    ASSERT_TRUE(nc_truthy(NC_MAP(full_map)), "nonempty map truthy");

    nc_string_free(empty);
    nc_string_free(nonempty);
    nc_list_free(empty_list);
    nc_list_free(full_list);
    nc_map_free(empty_map);
    nc_map_free(full_map);
}

/* ═══════════════════════════════════════════════════════════
 *  2. STRINGS
 * ═══════════════════════════════════════════════════════════ */

static void test_strings(void) {
    SECTION("Strings");

    NcString *s1 = nc_string_from_cstr("hello");
    ASSERT_EQ_INT(s1->length, 5, "string length");
    ASSERT_EQ_STR(s1->chars, "hello", "string content");

    NcString *s2 = nc_string_from_cstr("hello");
    ASSERT_TRUE(nc_string_equal(s1, s2), "interned strings equal");
    ASSERT_TRUE(s1 == s2, "interned strings are same pointer");

    NcString *s3 = nc_string_from_cstr("world");
    ASSERT_FALSE(nc_string_equal(s1, s3), "different strings not equal");

    NcString *concat = nc_string_concat(s1, s3);
    ASSERT_EQ_STR(concat->chars, "helloworld", "concat content");
    ASSERT_EQ_INT(concat->length, 10, "concat length");

    NcString *empty1 = nc_string_from_cstr("");
    NcString *empty2 = nc_string_from_cstr("");
    ASSERT_TRUE(nc_string_equal(empty1, empty2), "empty strings equal");
    ASSERT_EQ_INT(empty1->length, 0, "empty string length 0");

    NcString *concat_empty = nc_string_concat(s1, empty1);
    ASSERT_EQ_STR(concat_empty->chars, "hello", "concat with empty");

    NcString *s4 = nc_string_new("partial", 4);
    ASSERT_EQ_STR(s4->chars, "part", "string_new with partial length");
    ASSERT_EQ_INT(s4->length, 4, "partial string length");

    uint32_t h1 = nc_hash_string("test", 4);
    uint32_t h2 = nc_hash_string("test", 4);
    ASSERT_EQ_INT(h1, h2, "same string same hash");
    uint32_t h3 = nc_hash_string("tset", 4);
    ASSERT_TRUE(h1 != h3, "different string different hash");

    nc_string_free(s1);
    nc_string_free(s2);
    nc_string_free(s3);
    nc_string_free(concat);
    nc_string_free(empty1);
    nc_string_free(empty2);
    nc_string_free(concat_empty);
    nc_string_free(s4);
}

/* ═══════════════════════════════════════════════════════════
 *  3. LISTS
 * ═══════════════════════════════════════════════════════════ */

static void test_lists(void) {
    SECTION("Lists");

    NcList *l = nc_list_new();
    ASSERT_EQ_INT(l->count, 0, "empty list count 0");
    ASSERT_TRUE(IS_NONE(nc_list_get(l, 0)), "get from empty returns none");

    nc_list_push(l, NC_INT(10));
    nc_list_push(l, NC_INT(20));
    nc_list_push(l, NC_INT(30));
    ASSERT_EQ_INT(l->count, 3, "list count 3");
    ASSERT_EQ_INT(AS_INT(nc_list_get(l, 0)), 10, "list[0] = 10");
    ASSERT_EQ_INT(AS_INT(nc_list_get(l, 1)), 20, "list[1] = 20");
    ASSERT_EQ_INT(AS_INT(nc_list_get(l, 2)), 30, "list[2] = 30");

    ASSERT_TRUE(IS_NONE(nc_list_get(l, -1)), "negative index returns none");
    ASSERT_TRUE(IS_NONE(nc_list_get(l, 99)), "out of bounds returns none");

    nc_list_push(l, NC_STRING(nc_string_from_cstr("mixed")));
    ASSERT_EQ_INT(l->count, 4, "mixed-type list");
    ASSERT_TRUE(IS_STRING(nc_list_get(l, 3)), "mixed list has string");

    NcList *big = nc_list_new();
    for (int i = 0; i < 100; i++) nc_list_push(big, NC_INT(i));
    ASSERT_EQ_INT(big->count, 100, "list grows to 100");
    ASSERT_EQ_INT(AS_INT(nc_list_get(big, 99)), 99, "list[99] = 99");

    nc_list_free(l);
    nc_list_free(big);
}

/* ═══════════════════════════════════════════════════════════
 *  4. MAPS
 * ═══════════════════════════════════════════════════════════ */

static void test_maps(void) {
    SECTION("Maps");

    NcMap *m = nc_map_new();
    ASSERT_EQ_INT(m->count, 0, "empty map");

    NcString *k1 = nc_string_from_cstr("name");
    nc_map_set(m, k1, NC_STRING(nc_string_from_cstr("NC")));
    ASSERT_EQ_INT(m->count, 1, "map count 1");
    ASSERT_TRUE(nc_map_has(m, k1), "map has 'name'");
    ASSERT_EQ_STR(AS_STRING(nc_map_get(m, k1))->chars, "NC", "map get 'name'");

    NcString *k2 = nc_string_from_cstr("version");
    nc_map_set(m, k2, NC_INT(1));
    ASSERT_EQ_INT(m->count, 2, "map count 2");
    ASSERT_EQ_INT(AS_INT(nc_map_get(m, k2)), 1, "map get 'version'");

    nc_map_set(m, k1, NC_STRING(nc_string_from_cstr("NC2")));
    ASSERT_EQ_INT(m->count, 2, "overwrite doesn't increase count");
    ASSERT_EQ_STR(AS_STRING(nc_map_get(m, k1))->chars, "NC2", "overwritten value");

    NcString *missing = nc_string_from_cstr("nope");
    ASSERT_FALSE(nc_map_has(m, missing), "missing key");
    ASSERT_TRUE(IS_NONE(nc_map_get(m, missing)), "get missing returns none");

    NcMap *big = nc_map_new();
    for (int i = 0; i < 50; i++) {
        char key[16]; snprintf(key, sizeof(key), "key_%d", i);
        nc_map_set(big, nc_string_from_cstr(key), NC_INT(i));
    }
    ASSERT_EQ_INT(big->count, 50, "map with 50 entries");
    NcString *k25 = nc_string_from_cstr("key_25");
    ASSERT_EQ_INT(AS_INT(nc_map_get(big, k25)), 25, "map lookup key_25");
    nc_string_free(k25);

    nc_string_free(k1);
    nc_string_free(k2);
    nc_string_free(missing);
    nc_map_free(m);
    nc_map_free(big);
}

/* ═══════════════════════════════════════════════════════════
 *  5. LEXER
 * ═══════════════════════════════════════════════════════════ */

static int count_token_type(NcLexer *lex, NcTokenType type) {
    int count = 0;
    for (int i = 0; i < lex->token_count; i++)
        if (lex->tokens[i].type == type) count++;
    return count;
}

static void test_lexer(void) {
    SECTION("Lexer");

    SUBSECTION("Keywords");
    NcLexer *lex1 = nc_lexer_new("service version\nmodel\nimport define to with from", "<test>");
    nc_lexer_tokenize(lex1);
    ASSERT_EQ_INT(count_token_type(lex1, TOK_SERVICE), 1, "lex SERVICE");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_VERSION), 1, "lex VERSION");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_MODEL), 1, "lex MODEL");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_IMPORT), 1, "lex IMPORT");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_DEFINE), 1, "lex DEFINE");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_TO), 1, "lex TO");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_WITH), 1, "lex WITH");
    ASSERT_EQ_INT(count_token_type(lex1, TOK_FROM), 1, "lex FROM");
    nc_lexer_free(lex1);

    NcLexer *lex1b = nc_lexer_new("set model to \"gpt-4o\"", "<test>");
    nc_lexer_tokenize(lex1b);
    ASSERT_EQ_INT(count_token_type(lex1b, TOK_MODEL), 0, "lex MODEL contextual identifier");
    nc_lexer_free(lex1b);

    SUBSECTION("Control flow keywords");
    NcLexer *lex2 = nc_lexer_new("if otherwise repeat for each in while match when", "<test>");
    nc_lexer_tokenize(lex2);
    ASSERT_EQ_INT(count_token_type(lex2, TOK_IF), 1, "lex IF");
    ASSERT_EQ_INT(count_token_type(lex2, TOK_OTHERWISE), 1, "lex OTHERWISE");
    ASSERT_EQ_INT(count_token_type(lex2, TOK_REPEAT), 1, "lex REPEAT");
    ASSERT_EQ_INT(count_token_type(lex2, TOK_WHILE), 1, "lex WHILE");
    ASSERT_EQ_INT(count_token_type(lex2, TOK_MATCH), 1, "lex MATCH");
    ASSERT_EQ_INT(count_token_type(lex2, TOK_WHEN), 1, "lex WHEN");
    nc_lexer_free(lex2);

    SUBSECTION("Action keywords");
    NcLexer *lex3 = nc_lexer_new("gather ask respond run log show notify wait store emit set", "<test>");
    nc_lexer_tokenize(lex3);
    ASSERT_EQ_INT(count_token_type(lex3, TOK_GATHER), 1, "lex GATHER");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_ASK), 1, "lex ASK");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_RESPOND), 1, "lex RESPOND");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_RUN), 1, "lex RUN");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_LOG), 1, "lex LOG");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_NOTIFY), 1, "lex NOTIFY");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_WAIT), 1, "lex WAIT");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_STORE), 1, "lex STORE");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_EMIT), 1, "lex EMIT");
    ASSERT_EQ_INT(count_token_type(lex3, TOK_SET), 1, "lex SET");
    nc_lexer_free(lex3);

    SUBSECTION("Literals");
    NcLexer *lex4 = nc_lexer_new("42 3.14 \"hello\" true false nothing", "<test>");
    nc_lexer_tokenize(lex4);
    ASSERT_EQ_INT(count_token_type(lex4, TOK_INTEGER), 1, "lex integer");
    ASSERT_EQ_INT(count_token_type(lex4, TOK_FLOAT_LIT), 1, "lex float");
    ASSERT_EQ_INT(count_token_type(lex4, TOK_STRING), 1, "lex string");
    ASSERT_EQ_INT(count_token_type(lex4, TOK_TRUE), 1, "lex true");
    ASSERT_EQ_INT(count_token_type(lex4, TOK_FALSE), 1, "lex false");
    ASSERT_EQ_INT(count_token_type(lex4, TOK_NONE_LIT), 1, "lex nothing");
    nc_lexer_free(lex4);

    SUBSECTION("Indentation");
    NcLexer *lex5 = nc_lexer_new("to greet:\n    log \"hi\"\n    log \"bye\"", "<test>");
    nc_lexer_tokenize(lex5);
    ASSERT_EQ_INT(count_token_type(lex5, TOK_INDENT), 1, "indent emitted");
    ASSERT_EQ_INT(count_token_type(lex5, TOK_DEDENT), 1, "dedent emitted");
    nc_lexer_free(lex5);

    SUBSECTION("Nested indentation");
    NcLexer *lex6 = nc_lexer_new("if x:\n    if y:\n        log \"deep\"", "<test>");
    nc_lexer_tokenize(lex6);
    ASSERT_EQ_INT(count_token_type(lex6, TOK_INDENT), 2, "nested indent 2 levels");
    nc_lexer_free(lex6);

    SUBSECTION("Templates");
    NcLexer *lex7 = nc_lexer_new("{{name}}", "<test>");
    nc_lexer_tokenize(lex7);
    ASSERT_EQ_INT(count_token_type(lex7, TOK_TEMPLATE), 1, "lex template");
    nc_lexer_free(lex7);

    SUBSECTION("Symbols");
    NcLexer *lex8 = nc_lexer_new(": , . + - * / ( ) [ ] = ->", "<test>");
    nc_lexer_tokenize(lex8);
    ASSERT_EQ_INT(count_token_type(lex8, TOK_COLON), 1, "lex colon");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_COMMA), 1, "lex comma");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_DOT), 1, "lex dot");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_PLUS), 1, "lex plus");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_MINUS), 1, "lex minus");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_STAR), 1, "lex star");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_SLASH), 1, "lex slash");
    ASSERT_EQ_INT(count_token_type(lex8, TOK_ARROW), 1, "lex arrow");
    nc_lexer_free(lex8);

    SUBSECTION("Comments");
    NcLexer *lex9 = nc_lexer_new("set x to 1 // this is a comment\nset y to 2", "<test>");
    nc_lexer_tokenize(lex9);
    ASSERT_EQ_INT(count_token_type(lex9, TOK_SET), 2, "comments stripped: 2 set tokens");
    nc_lexer_free(lex9);

    SUBSECTION("HTTP methods");
    NcLexer *lex10 = nc_lexer_new("GET POST PUT DELETE PATCH", "<test>");
    nc_lexer_tokenize(lex10);
    ASSERT_EQ_INT(count_token_type(lex10, TOK_HTTP_GET), 1, "lex GET");
    ASSERT_EQ_INT(count_token_type(lex10, TOK_HTTP_POST), 1, "lex POST");
    ASSERT_EQ_INT(count_token_type(lex10, TOK_HTTP_PUT), 1, "lex PUT");
    ASSERT_EQ_INT(count_token_type(lex10, TOK_HTTP_DELETE), 1, "lex DELETE");
    ASSERT_EQ_INT(count_token_type(lex10, TOK_HTTP_PATCH), 1, "lex PATCH");
    nc_lexer_free(lex10);
}

/* ═══════════════════════════════════════════════════════════
 *  6. PARSER
 * ═══════════════════════════════════════════════════════════ */

static NcASTNode *parse_src(const char *src, NcParser **out_parser) {
    NcLexer *lex = nc_lexer_new(src, "<test>");
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, "<test>");
    NcASTNode *prog = nc_parser_parse(parser);
    *out_parser = parser;
    return prog;
}

static void test_parser(void) {
    SECTION("Parser");

    SUBSECTION("Service metadata");
    NcParser *p1;
    NcASTNode *prog1 = parse_src("service \"my-app\"\nversion \"2.0\"\nmodel \"my-model\"", &p1);
    ASSERT_FALSE(p1->had_error, "parse metadata no error");
    ASSERT_EQ_STR(prog1->as.program.service_name->chars, "my-app", "service name");
    ASSERT_EQ_STR(prog1->as.program.version->chars, "2.0", "version");
    ASSERT_EQ_STR(prog1->as.program.model->chars, "my-model", "model");
    nc_parser_free(p1);

    SUBSECTION("Behavior with params");
    NcParser *p2;
    NcASTNode *prog2 = parse_src(
        "service \"t\"\nto greet with name:\n    respond with name", &p2);
    ASSERT_FALSE(p2->had_error, "parse behavior no error");
    ASSERT_EQ_INT(prog2->as.program.beh_count, 1, "1 behavior");
    NcASTNode *beh = prog2->as.program.behaviors[0];
    ASSERT_EQ_INT(beh->as.behavior.param_count, 1, "1 param");
    ASSERT_EQ_INT(beh->as.behavior.body_count, 1, "1 body stmt");
    nc_parser_free(p2);

    SUBSECTION("Multiple behaviors");
    NcParser *p3;
    NcASTNode *prog3 = parse_src(
        "service \"t\"\nto a:\n    respond with 1\nto b:\n    respond with 2\nto c:\n    respond with 3", &p3);
    ASSERT_FALSE(p3->had_error, "parse 3 behaviors no error");
    ASSERT_EQ_INT(prog3->as.program.beh_count, 3, "3 behaviors");
    nc_parser_free(p3);

    SUBSECTION("If/otherwise");
    NcParser *p4;
    NcASTNode *prog4 = parse_src(
        "service \"t\"\nto check:\n    if x is above 5:\n        respond with \"yes\"\n    otherwise:\n        respond with \"no\"", &p4);
    ASSERT_FALSE(p4->had_error, "parse if/otherwise no error");
    nc_parser_free(p4);

    SUBSECTION("Repeat loop");
    NcParser *p5;
    NcASTNode *prog5 = parse_src(
        "service \"t\"\nto loop:\n    repeat for each item in items:\n        log item", &p5);
    ASSERT_FALSE(p5->had_error, "parse repeat no error");
    nc_parser_free(p5);

    SUBSECTION("Match/when");
    NcParser *p6;
    NcASTNode *prog6 = parse_src(
        "service \"t\"\nto m:\n    match x:\n        when \"a\":\n            log \"found a\"", &p6);
    ASSERT_FALSE(p6->had_error, "parse match/when no error");
    nc_parser_free(p6);

    SUBSECTION("Define type");
    NcParser *p7;
    NcASTNode *prog7 = parse_src(
        "service \"t\"\ndefine User as:\n    name is text\n    age is number", &p7);
    ASSERT_FALSE(p7->had_error, "parse define no error");
    ASSERT_EQ_INT(prog7->as.program.def_count, 1, "1 definition");
    ASSERT_EQ_INT(prog7->as.program.definitions[0]->as.definition.field_count, 2, "2 fields");
    nc_parser_free(p7);

    SUBSECTION("API routes");
    NcParser *p8;
    NcASTNode *prog8 = parse_src(
        "service \"t\"\nto greet:\n    respond with \"hi\"\napi:\n    GET /hello runs greet\n    POST /data runs greet", &p8);
    ASSERT_FALSE(p8->had_error, "parse api no error");
    ASSERT_EQ_INT(prog8->as.program.route_count, 2, "2 routes");
    nc_parser_free(p8);

    SUBSECTION("Try/on error");
    NcParser *p9;
    NcASTNode *prog9 = parse_src(
        "service \"t\"\nto safe:\n    try:\n        log \"attempt\"\n    on error:\n        log \"failed\"", &p9);
    ASSERT_FALSE(p9->had_error, "parse try no error");
    nc_parser_free(p9);

    SUBSECTION("Expression precedence");
    NcParser *p10;
    NcASTNode *prog10 = parse_src(
        "service \"t\"\nto calc:\n    respond with 2 + 3 * 4", &p10);
    ASSERT_FALSE(p10->had_error, "parse expression no error");
    NcASTNode *resp = prog10->as.program.behaviors[0]->as.behavior.body[0];
    NcASTNode *expr = resp->as.single_expr.value;
    ASSERT_EQ_INT(expr->type, NODE_MATH, "top is math");
    ASSERT_TRUE(expr->as.math.op == '+', "top op is +");
    ASSERT_EQ_INT(expr->as.math.right->type, NODE_MATH, "right is math");
    ASSERT_TRUE(expr->as.math.right->as.math.op == '*', "right op is *");
    nc_parser_free(p10);

    SUBSECTION("List literal");
    NcParser *p11;
    NcASTNode *prog11 = parse_src(
        "service \"t\"\nto items:\n    respond with [1, 2, 3]", &p11);
    ASSERT_FALSE(p11->had_error, "parse list literal no error");
    nc_parser_free(p11);

    SUBSECTION("While loop");
    NcParser *p12;
    NcASTNode *prog12 = parse_src(
        "service \"t\"\nto loop:\n    set x to 0\n    while x is below 10:\n        set x to x + 1\n    respond with x", &p12);
    ASSERT_FALSE(p12->had_error, "parse while no error");
    nc_parser_free(p12);

    SUBSECTION("Repeat while loop");
    NcParser *p13;
    NcASTNode *prog13 = parse_src(
        "service \"t\"\nto loop:\n    set x to 0\n    repeat while x is below 10:\n        set x to x + 1\n    respond with x", &p13);
    ASSERT_FALSE(p13->had_error, "parse repeat while no error");
    NcASTNode *beh13 = prog13->as.program.behaviors[0];
    NcASTNode *rw_node = beh13->as.behavior.body[1];
    ASSERT_EQ_INT(rw_node->type, NODE_WHILE, "repeat while produces NODE_WHILE");
    nc_parser_free(p13);
}

/* ═══════════════════════════════════════════════════════════
 *  7. COMPILER + VM (bytecode pipeline)
 * ═══════════════════════════════════════════════════════════ */

static NcValue compile_and_run(const char *src, const char *behavior) {
    NcLexer *lex = nc_lexer_new(src, "<test>");
    nc_lexer_tokenize(lex);
    NcParser *parser = nc_parser_new(lex->tokens, lex->token_count, "<test>");
    NcASTNode *program = nc_parser_parse(parser);
    if (parser->had_error) {
        nc_parser_free(parser); nc_lexer_free(lex);
        return NC_NONE();
    }
    NcCompiler *comp = nc_compiler_new();
    if (!nc_compiler_compile(comp, program)) {
        nc_compiler_free(comp); nc_parser_free(parser); nc_lexer_free(lex);
        return NC_NONE();
    }
    nc_optimize_all(comp);

    NcValue result = NC_NONE();
    for (int i = 0; i < comp->chunk_count; i++) {
        if (strcmp(comp->beh_names[i]->chars, behavior) == 0) {
            NcVM *vm = nc_vm_new();
            result = nc_vm_execute_fast(vm, &comp->chunks[i]);
            nc_vm_free(vm);
            break;
        }
    }
    nc_compiler_free(comp);
    nc_parser_free(parser);
    nc_lexer_free(lex);
    return result;
}

static void test_compiler_vm(void) {
    SECTION("Compiler + VM Pipeline");

    SUBSECTION("Integer respond");
    NcValue r1 = compile_and_run("service \"t\"\nto calc:\n    respond with 42", "calc");
    ASSERT_TRUE(IS_INT(r1), "vm int respond type");
    ASSERT_EQ_INT(AS_INT(r1), 42, "vm int value 42");

    SUBSECTION("String respond");
    NcValue r2 = compile_and_run("service \"t\"\nto greet:\n    respond with \"hello\"", "greet");
    ASSERT_TRUE(IS_STRING(r2), "vm string respond type");
    ASSERT_EQ_STR(AS_STRING(r2)->chars, "hello", "vm string value");

    SUBSECTION("Boolean respond");
    NcValue r3 = compile_and_run("service \"t\"\nto check:\n    respond with true", "check");
    ASSERT_TRUE(IS_BOOL(r3), "vm bool respond type");
    ASSERT_TRUE(AS_BOOL(r3), "vm bool true");

    SUBSECTION("Addition");
    NcValue r4 = compile_and_run("service \"t\"\nto add:\n    respond with 10 + 20", "add");
    ASSERT_EQ_INT(AS_INT(r4), 30, "vm 10 + 20 = 30");

    SUBSECTION("Subtraction");
    NcValue r5 = compile_and_run("service \"t\"\nto sub:\n    respond with 50 - 15", "sub");
    ASSERT_EQ_INT(AS_INT(r5), 35, "vm 50 - 15 = 35");

    SUBSECTION("Multiplication");
    NcValue r6 = compile_and_run("service \"t\"\nto mul:\n    respond with 6 * 7", "mul");
    ASSERT_EQ_INT(AS_INT(r6), 42, "vm 6 * 7 = 42");

    SUBSECTION("Division");
    NcValue r7 = compile_and_run("service \"t\"\nto div:\n    respond with 100 / 4", "div");
    ASSERT_NEAR(AS_FLOAT(r7), 25.0, 0.001, "vm 100 / 4 = 25");

    SUBSECTION("String concatenation");
    NcValue r8 = compile_and_run(
        "service \"t\"\nto greet:\n    respond with \"hello\" + \" \" + \"world\"", "greet");
    ASSERT_EQ_STR(AS_STRING(r8)->chars, "hello world", "vm string concat");

    SUBSECTION("Set and respond");
    NcValue r9 = compile_and_run(
        "service \"t\"\nto calc:\n    set x to 42\n    respond with x", "calc");
    ASSERT_EQ_INT(AS_INT(r9), 42, "vm set and respond");

    SUBSECTION("Set compound expression");
    NcValue r10 = compile_and_run(
        "service \"t\"\nto calc:\n    set x to 10\n    set y to 20\n    set z to x + y\n    respond with z", "calc");
    ASSERT_EQ_INT(AS_INT(r10), 30, "vm compound expression");

    SUBSECTION("If true branch");
    NcValue r11 = compile_and_run(
        "service \"t\"\nto check:\n    if true:\n        respond with \"yes\"\n    otherwise:\n        respond with \"no\"", "check");
    ASSERT_EQ_STR(AS_STRING(r11)->chars, "yes", "vm if true branch");

    SUBSECTION("If false branch");
    NcValue r12 = compile_and_run(
        "service \"t\"\nto check:\n    if false:\n        respond with \"yes\"\n    otherwise:\n        respond with \"no\"", "check");
    ASSERT_EQ_STR(AS_STRING(r12)->chars, "no", "vm if false branch");

    SUBSECTION("Comparison: above");
    NcValue r13 = compile_and_run(
        "service \"t\"\nto check:\n    set x to 100\n    if x is above 50:\n        respond with \"big\"\n    otherwise:\n        respond with \"small\"", "check");
    ASSERT_EQ_STR(AS_STRING(r13)->chars, "big", "vm comparison above");

    SUBSECTION("Comparison: equal");
    NcValue r14 = compile_and_run(
        "service \"t\"\nto check:\n    set x to 5\n    if x is equal to 5:\n        respond with \"match\"", "check");
    ASSERT_EQ_STR(AS_STRING(r14)->chars, "match", "vm comparison equal");

    SUBSECTION("List literal");
    NcValue r15 = compile_and_run(
        "service \"t\"\nto items:\n    respond with [1, 2, 3]", "items");
    ASSERT_TRUE(IS_LIST(r15), "vm list literal type");
    ASSERT_EQ_INT(AS_LIST(r15)->count, 3, "vm list count 3");

    SUBSECTION("None respond");
    NcValue r16 = compile_and_run(
        "service \"t\"\nto nothing:\n    respond with nothing", "nothing");
    ASSERT_TRUE(IS_NONE(r16), "vm none respond");

    SUBSECTION("Float literal");
    NcValue r17 = compile_and_run(
        "service \"t\"\nto pi:\n    respond with 3.14", "pi");
    ASSERT_TRUE(IS_FLOAT(r17), "vm float type");
    ASSERT_NEAR(AS_FLOAT(r17), 3.14, 0.001, "vm float value");

    SUBSECTION("Mixed arithmetic");
    NcValue r18 = compile_and_run(
        "service \"t\"\nto calc:\n    respond with 2 + 3 * 4", "calc");
    ASSERT_EQ_INT(AS_INT(r18), 14, "vm precedence 2 + 3*4 = 14");

    SUBSECTION("String + number concat");
    NcValue r19 = compile_and_run(
        "service \"t\"\nto msg:\n    set x to 42\n    respond with \"value: \" + x", "msg");
    ASSERT_TRUE(IS_STRING(r19), "vm string+int concat type");
}

/* ═══════════════════════════════════════════════════════════
 *  8. INTERPRETER (tree-walk)
 * ═══════════════════════════════════════════════════════════ */

static void test_interpreter(void) {
    SECTION("Interpreter (tree-walk)");
    NcMap *args = nc_map_new();

    SUBSECTION("Respond with string");
    NcValue r1 = nc_call_behavior(
        "service \"t\"\nto greet:\n    respond with \"hello world\"",
        "<test>", "greet", args);
    ASSERT_TRUE(IS_STRING(r1), "interp string type");
    ASSERT_EQ_STR(AS_STRING(r1)->chars, "hello world", "interp string value");

    SUBSECTION("Math operations");
    NcValue r2 = nc_call_behavior(
        "service \"t\"\nto add:\n    respond with 3 + 7",
        "<test>", "add", args);
    ASSERT_EQ_INT(AS_INT(r2), 10, "interp 3 + 7 = 10");

    NcValue r3 = nc_call_behavior(
        "service \"t\"\nto sub:\n    respond with 20 - 8",
        "<test>", "sub", args);
    ASSERT_EQ_INT(AS_INT(r3), 12, "interp 20 - 8 = 12");

    NcValue r4 = nc_call_behavior(
        "service \"t\"\nto mul:\n    respond with 6 * 7",
        "<test>", "mul", args);
    ASSERT_EQ_INT(AS_INT(r4), 42, "interp 6 * 7 = 42");

    SUBSECTION("Set and respond");
    NcValue r5 = nc_call_behavior(
        "service \"t\"\nto calc:\n    set x to 42\n    respond with x",
        "<test>", "calc", args);
    ASSERT_EQ_INT(AS_INT(r5), 42, "interp set + respond");

    SUBSECTION("If/otherwise");
    NcValue r6 = nc_call_behavior(
        "service \"t\"\nto check:\n    set x to 100\n    if x is above 50:\n        respond with \"big\"\n    otherwise:\n        respond with \"small\"",
        "<test>", "check", args);
    ASSERT_EQ_STR(AS_STRING(r6)->chars, "big", "interp if above");

    SUBSECTION("String concat");
    NcValue r7 = nc_call_behavior(
        "service \"t\"\nto greet:\n    respond with \"hello\" + \" \" + \"world\"",
        "<test>", "greet", args);
    ASSERT_EQ_STR(AS_STRING(r7)->chars, "hello world", "interp concat");

    SUBSECTION("Repeat loop with accumulator");
    NcValue r8 = nc_call_behavior(
        "service \"t\"\nto sum:\n    set total to 0\n    set nums to [1, 2, 3, 4, 5]\n    repeat for each n in nums:\n        set total to total + n\n    respond with total",
        "<test>", "sum", args);
    ASSERT_EQ_INT(AS_INT(r8), 15, "interp repeat sum 1+2+3+4+5 = 15");

    SUBSECTION("Match/when");
    NcValue r9 = nc_call_behavior(
        "service \"t\"\nto classify:\n    set status to \"critical\"\n    match status:\n        when \"healthy\":\n            respond with \"ok\"\n        when \"critical\":\n            respond with \"alert\"\n        otherwise:\n            respond with \"unknown\"",
        "<test>", "classify", args);
    ASSERT_EQ_STR(AS_STRING(r9)->chars, "alert", "interp match critical");

    SUBSECTION("Nested if");
    NcValue r10 = nc_call_behavior(
        "service \"t\"\nto grade:\n    set score to 85\n    if score is above 90:\n        respond with \"A\"\n    otherwise:\n        if score is above 80:\n            respond with \"B\"\n        otherwise:\n            respond with \"C\"",
        "<test>", "grade", args);
    ASSERT_EQ_STR(AS_STRING(r10)->chars, "B", "interp nested if grade B");

    SUBSECTION("Boolean logic");
    NcValue r11 = nc_call_behavior(
        "service \"t\"\nto check:\n    if true and true:\n        respond with \"both\"",
        "<test>", "check", args);
    ASSERT_EQ_STR(AS_STRING(r11)->chars, "both", "interp and logic");

    NcValue r12 = nc_call_behavior(
        "service \"t\"\nto check:\n    if false or true:\n        respond with \"either\"",
        "<test>", "check", args);
    ASSERT_EQ_STR(AS_STRING(r12)->chars, "either", "interp or logic");

    SUBSECTION("List operations");
    NcValue r13 = nc_call_behavior(
        "service \"t\"\nto count:\n    set items to [10, 20, 30]\n    respond with len(items)",
        "<test>", "count", args);
    ASSERT_EQ_INT(AS_INT(r13), 3, "interp len([10,20,30]) = 3");

    SUBSECTION("Repeat while loop");
    NcValue rw1 = nc_call_behavior(
        "service \"t\"\nto counter:\n    set n to 0\n    repeat while n is below 5:\n        set n to n + 1\n    respond with n",
        "<test>", "counter", args);
    ASSERT_EQ_INT(AS_INT(rw1), 5, "interp repeat while counts to 5");

    SUBSECTION("Repeat while with accumulator");
    NcValue rw2 = nc_call_behavior(
        "service \"t\"\n"
        "to accum:\n"
        "    set total to 0\n"
        "    set i to 1\n"
        "    repeat while i is below 6:\n"
        "        set total to total + i\n"
        "        set i to i + 1\n"
        "    respond with total",
        "<test>", "accum", args);
    ASSERT_EQ_INT(AS_INT(rw2), 15, "interp repeat while sum 1..5 = 15");

    nc_map_free(args);
}

/* ═══════════════════════════════════════════════════════════
 *  9. JSON
 * ═══════════════════════════════════════════════════════════ */

static void test_json(void) {
    SECTION("JSON");

    SUBSECTION("Parse object");
    NcValue v1 = nc_json_parse("{\"name\":\"NC\",\"version\":1}");
    ASSERT_TRUE(IS_MAP(v1), "json parse object");
    NcString *nk = nc_string_from_cstr("name");
    ASSERT_EQ_STR(AS_STRING(nc_map_get(AS_MAP(v1), nk))->chars, "NC", "json name field");
    nc_string_free(nk);

    SUBSECTION("Parse array");
    NcValue v2 = nc_json_parse("[1, 2, 3]");
    ASSERT_TRUE(IS_LIST(v2), "json parse array");
    ASSERT_EQ_INT(AS_LIST(v2)->count, 3, "json array length 3");
    ASSERT_EQ_INT(AS_INT(nc_list_get(AS_LIST(v2), 0)), 1, "json array[0] = 1");

    SUBSECTION("Parse primitives");
    ASSERT_TRUE(AS_BOOL(nc_json_parse("true")), "json true");
    ASSERT_FALSE(AS_BOOL(nc_json_parse("false")), "json false");
    ASSERT_TRUE(IS_NONE(nc_json_parse("null")), "json null");
    ASSERT_EQ_INT(AS_INT(nc_json_parse("42")), 42, "json int");
    ASSERT_NEAR(AS_FLOAT(nc_json_parse("3.14")), 3.14, 0.01, "json float");

    SUBSECTION("Parse string");
    NcValue v3 = nc_json_parse("\"hello world\"");
    ASSERT_TRUE(IS_STRING(v3), "json parse string");
    ASSERT_EQ_STR(AS_STRING(v3)->chars, "hello world", "json string value");

    SUBSECTION("Parse nested");
    NcValue v4 = nc_json_parse("{\"user\":{\"name\":\"Alice\",\"age\":30}}");
    ASSERT_TRUE(IS_MAP(v4), "json nested object");
    NcString *uk = nc_string_from_cstr("user");
    NcValue user = nc_map_get(AS_MAP(v4), uk);
    ASSERT_TRUE(IS_MAP(user), "json nested user is map");
    nc_string_free(uk);

    SUBSECTION("Parse empty");
    ASSERT_TRUE(IS_MAP(nc_json_parse("{}")), "json empty object");
    ASSERT_TRUE(IS_LIST(nc_json_parse("[]")), "json empty array");
    ASSERT_EQ_INT(AS_LIST(nc_json_parse("[]"))->count, 0, "json empty array count 0");

    SUBSECTION("Serialize");
    char *j1 = nc_json_serialize(NC_INT(42), false);
    ASSERT_EQ_STR(j1, "42", "serialize int");
    free(j1);

    char *j2 = nc_json_serialize(NC_STRING(nc_string_from_cstr("test")), false);
    ASSERT_EQ_STR(j2, "\"test\"", "serialize string");
    free(j2);

    char *j3 = nc_json_serialize(NC_BOOL(true), false);
    ASSERT_EQ_STR(j3, "true", "serialize bool");
    free(j3);

    char *j4 = nc_json_serialize(NC_NONE(), false);
    ASSERT_EQ_STR(j4, "null", "serialize null");
    free(j4);

    SUBSECTION("Roundtrip");
    NcMap *m = nc_map_new();
    nc_map_set(m, nc_string_from_cstr("x"), NC_INT(10));
    nc_map_set(m, nc_string_from_cstr("y"), NC_STRING(nc_string_from_cstr("hello")));
    char *j5 = nc_json_serialize(NC_MAP(m), false);
    NcValue rt = nc_json_parse(j5);
    ASSERT_TRUE(IS_MAP(rt), "roundtrip produces map");
    NcString *xk = nc_string_from_cstr("x");
    ASSERT_EQ_INT(AS_INT(nc_map_get(AS_MAP(rt), xk)), 10, "roundtrip x=10");
    nc_string_free(xk);
    free(j5);
}

/* ═══════════════════════════════════════════════════════════
 *  10. STDLIB
 * ═══════════════════════════════════════════════════════════ */

static void test_stdlib(void) {
    SECTION("Standard Library");

    SUBSECTION("String ops");
    NcString *hello = nc_string_from_cstr("Hello World");
    NcValue upper = nc_stdlib_upper(hello);
    ASSERT_EQ_STR(AS_STRING(upper)->chars, "HELLO WORLD", "upper()");
    NcValue lower = nc_stdlib_lower(hello);
    ASSERT_EQ_STR(AS_STRING(lower)->chars, "hello world", "lower()");

    NcString *padded = nc_string_from_cstr("  hello  ");
    NcValue trimmed = nc_stdlib_trim(padded);
    ASSERT_EQ_STR(AS_STRING(trimmed)->chars, "hello", "trim()");

    NcString *hay = nc_string_from_cstr("hello world");
    NcString *needle = nc_string_from_cstr("world");
    ASSERT_TRUE(AS_BOOL(nc_stdlib_contains(hay, needle)), "contains('world')");
    NcString *missing = nc_string_from_cstr("xyz");
    ASSERT_FALSE(AS_BOOL(nc_stdlib_contains(hay, missing)), "not contains('xyz')");

    NcString *pre = nc_string_from_cstr("hel");
    ASSERT_TRUE(AS_BOOL(nc_stdlib_starts_with(hay, pre)), "starts_with('hel')");
    NcString *suf = nc_string_from_cstr("rld");
    ASSERT_TRUE(AS_BOOL(nc_stdlib_ends_with(hay, suf)), "ends_with('rld')");

    NcString *src = nc_string_from_cstr("foo bar foo");
    NcString *old = nc_string_from_cstr("foo");
    NcString *new_s = nc_string_from_cstr("baz");
    NcValue replaced = nc_stdlib_replace(src, old, new_s);
    ASSERT_EQ_STR(AS_STRING(replaced)->chars, "baz bar baz", "replace()");

    NcString *csv = nc_string_from_cstr("a,b,c");
    NcString *delim = nc_string_from_cstr(",");
    NcValue parts = nc_stdlib_split(csv, delim);
    ASSERT_EQ_INT(AS_LIST(parts)->count, 3, "split count 3");

    NcList *words = nc_list_new();
    nc_list_push(words, NC_STRING(nc_string_from_cstr("hello")));
    nc_list_push(words, NC_STRING(nc_string_from_cstr("world")));
    NcString *sep = nc_string_from_cstr(" ");
    NcValue joined = nc_stdlib_join(words, sep);
    ASSERT_EQ_STR(AS_STRING(joined)->chars, "hello world", "join()");

    nc_string_free(hello);
    nc_string_free(padded);
    nc_string_free(hay);
    nc_string_free(needle);
    nc_string_free(missing);
    nc_string_free(pre);
    nc_string_free(suf);
    nc_string_free(src);
    nc_string_free(old);
    nc_string_free(new_s);
    nc_string_free(csv);
    nc_string_free(delim);
    nc_string_free(sep);
    nc_list_free(words);

    SUBSECTION("Math ops");
    ASSERT_NEAR(AS_FLOAT(nc_stdlib_abs(-5.0)), 5.0, 0.001, "abs(-5) = 5");
    ASSERT_NEAR(AS_FLOAT(nc_stdlib_sqrt(9.0)), 3.0, 0.001, "sqrt(9) = 3");
    ASSERT_EQ_INT(AS_INT(nc_stdlib_ceil(2.3)), 3, "ceil(2.3) = 3");
    ASSERT_EQ_INT(AS_INT(nc_stdlib_floor(2.9)), 2, "floor(2.9) = 2");
    ASSERT_EQ_INT(AS_INT(nc_stdlib_round(2.5)), 3, "round(2.5) = 3");
    ASSERT_NEAR(AS_FLOAT(nc_stdlib_pow(2.0, 10.0)), 1024.0, 0.001, "pow(2,10) = 1024");
    ASSERT_NEAR(AS_FLOAT(nc_stdlib_min(3.0, 7.0)), 3.0, 0.001, "min(3,7) = 3");
    ASSERT_NEAR(AS_FLOAT(nc_stdlib_max(3.0, 7.0)), 7.0, 0.001, "max(3,7) = 7");

    NcValue rnd = nc_stdlib_random();
    ASSERT_TRUE(IS_FLOAT(rnd), "random() returns float");
    ASSERT_TRUE(AS_FLOAT(rnd) >= 0.0 && AS_FLOAT(rnd) <= 1.0, "random() in [0,1]");

    SUBSECTION("List ops");
    NcList *lst = nc_list_new();
    nc_list_push(lst, NC_INT(3));
    nc_list_push(lst, NC_INT(1));
    nc_list_push(lst, NC_INT(2));

    NcValue sorted = nc_stdlib_list_sort(lst);
    ASSERT_EQ_INT(AS_INT(nc_list_get(AS_LIST(sorted), 0)), 1, "sort[0] = 1");
    ASSERT_EQ_INT(AS_INT(nc_list_get(AS_LIST(sorted), 1)), 2, "sort[1] = 2");
    ASSERT_EQ_INT(AS_INT(nc_list_get(AS_LIST(sorted), 2)), 3, "sort[2] = 3");

    NcValue reversed = nc_stdlib_list_reverse(lst);
    ASSERT_EQ_INT(AS_INT(nc_list_get(AS_LIST(reversed), 0)), 2, "reverse[0]");

    NcValue appended = nc_stdlib_list_append(lst, NC_INT(99));
    ASSERT_EQ_INT(AS_LIST(appended)->count, 4, "append count 4");

    nc_list_free(lst);

    SUBSECTION("Time");
    NcValue now = nc_stdlib_time_now();
    ASSERT_TRUE(IS_FLOAT(now), "time_now returns float");
    ASSERT_TRUE(AS_FLOAT(now) > 1000000000.0, "time_now > epoch 2001");

    NcValue ms = nc_stdlib_time_ms();
    ASSERT_TRUE(IS_FLOAT(ms), "time_ms returns float");
    ASSERT_TRUE(AS_FLOAT(ms) > 1000000000000.0, "time_ms > epoch ms");
}

/* ═══════════════════════════════════════════════════════════
 *  11. CHUNK / BYTECODE
 * ═══════════════════════════════════════════════════════════ */

static void test_chunks(void) {
    SECTION("Bytecode Chunks");

    NcChunk *c = nc_chunk_new();
    ASSERT_EQ_INT(c->count, 0, "empty chunk");

    nc_chunk_write(c, OP_CONSTANT, 1);
    nc_chunk_write(c, 0, 1);
    nc_chunk_write(c, OP_HALT, 1);
    ASSERT_EQ_INT(c->count, 3, "chunk has 3 bytes");
    ASSERT_EQ_INT(c->code[0], OP_CONSTANT, "first byte is OP_CONSTANT");
    ASSERT_EQ_INT(c->code[2], OP_HALT, "last byte is OP_HALT");

    int idx = nc_chunk_add_constant(c, NC_INT(42));
    ASSERT_EQ_INT(idx, 0, "first constant at index 0");
    ASSERT_EQ_INT(AS_INT(c->constants[0]), 42, "constant value 42");

    int idx2 = nc_chunk_add_constant(c, NC_STRING(nc_string_from_cstr("test")));
    ASSERT_EQ_INT(idx2, 1, "second constant at index 1");

    int vi = nc_chunk_add_var(c, nc_string_from_cstr("x"));
    ASSERT_EQ_INT(vi, 0, "first var at index 0");
    int vi2 = nc_chunk_add_var(c, nc_string_from_cstr("x"));
    ASSERT_EQ_INT(vi2, 0, "duplicate var returns same index");
    int vi3 = nc_chunk_add_var(c, nc_string_from_cstr("y"));
    ASSERT_EQ_INT(vi3, 1, "new var at index 1");

    nc_chunk_free(c);
}

/* ═══════════════════════════════════════════════════════════
 *  12. VALUE TO STRING
 * ═══════════════════════════════════════════════════════════ */

static void test_value_to_string(void) {
    SECTION("Value to String");

    NcString *s1 = nc_value_to_string(NC_NONE());
    ASSERT_EQ_STR(s1->chars, "nothing", "none -> nothing");
    nc_string_free(s1);

    NcString *s2 = nc_value_to_string(NC_BOOL(true));
    ASSERT_EQ_STR(s2->chars, "yes", "true -> yes");
    nc_string_free(s2);

    NcString *s3 = nc_value_to_string(NC_BOOL(false));
    ASSERT_EQ_STR(s3->chars, "no", "false -> no");
    nc_string_free(s3);

    NcString *s4 = nc_value_to_string(NC_INT(42));
    ASSERT_EQ_STR(s4->chars, "42", "int -> string");
    nc_string_free(s4);

    NcString *s5 = nc_value_to_string(NC_INT(0));
    ASSERT_EQ_STR(s5->chars, "0", "zero -> string");
    nc_string_free(s5);

    NcString *s6 = nc_value_to_string(NC_INT(-100));
    ASSERT_EQ_STR(s6->chars, "-100", "negative -> string");
    nc_string_free(s6);

    NcString *orig = nc_string_from_cstr("hello");
    NcString *s7 = nc_value_to_string(NC_STRING(orig));
    ASSERT_EQ_STR(s7->chars, "hello", "string passthrough");
    nc_string_free(s7);
    nc_string_free(orig);
}

/* ═══════════════════════════════════════════════════════════
 *  13. END-TO-END PROGRAMS
 * ═══════════════════════════════════════════════════════════ */

static void test_end_to_end(void) {
    SECTION("End-to-End Programs");
    NcMap *args = nc_map_new();

    SUBSECTION("Fibonacci-style accumulation");
    NcValue fib = nc_call_behavior(
        "service \"t\"\n"
        "to fib:\n"
        "    set a to 0\n"
        "    set b to 1\n"
        "    set nums to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]\n"
        "    repeat for each i in nums:\n"
        "        set temp to b\n"
        "        set b to a + b\n"
        "        set a to temp\n"
        "    respond with b",
        "<test>", "fib", args);
    ASSERT_EQ_INT(AS_INT(fib), 89, "fib(10) = 89");

    SUBSECTION("Nested match");
    NcValue nm = nc_call_behavior(
        "service \"t\"\n"
        "to classify:\n"
        "    set status to \"degraded\"\n"
        "    match status:\n"
        "        when \"healthy\":\n"
        "            respond with 1\n"
        "        when \"degraded\":\n"
        "            respond with 2\n"
        "        when \"critical\":\n"
        "            respond with 3\n"
        "        otherwise:\n"
        "            respond with 0",
        "<test>", "classify", args);
    ASSERT_EQ_INT(AS_INT(nm), 2, "match degraded = 2");

    SUBSECTION("String building in loop");
    NcValue sb = nc_call_behavior(
        "service \"t\"\n"
        "to build:\n"
        "    set result to \"\"\n"
        "    set words to [\"hello\", \" \", \"world\"]\n"
        "    repeat for each w in words:\n"
        "        set result to result + w\n"
        "    respond with result",
        "<test>", "build", args);
    ASSERT_EQ_STR(AS_STRING(sb)->chars, "hello world", "string build in loop");

    SUBSECTION("Comparison chain");
    NcValue cc = nc_call_behavior(
        "service \"t\"\n"
        "to grade:\n"
        "    set score to 75\n"
        "    if score is above 90:\n"
        "        respond with \"A\"\n"
        "    otherwise:\n"
        "        if score is above 80:\n"
        "            respond with \"B\"\n"
        "        otherwise:\n"
        "            if score is above 70:\n"
        "                respond with \"C\"\n"
        "            otherwise:\n"
        "                respond with \"D\"",
        "<test>", "grade", args);
    ASSERT_EQ_STR(AS_STRING(cc)->chars, "C", "grade 75 = C");

    SUBSECTION("Built-in function: len");
    NcValue ln = nc_call_behavior(
        "service \"t\"\n"
        "to check:\n"
        "    set items to [1, 2, 3, 4, 5]\n"
        "    set count to len(items)\n"
        "    respond with count",
        "<test>", "check", args);
    ASSERT_EQ_INT(AS_INT(ln), 5, "len([1..5]) = 5");

    SUBSECTION("Direct string conversion");
    NcValue sv = nc_call_behavior(
        "service \"t\"\n"
        "to convert:\n"
        "    respond with \"value: \" + 42",
        "<test>", "convert", args);
    ASSERT_TRUE(IS_STRING(sv), "string + int is string");
    if (IS_STRING(sv))
        ASSERT_EQ_STR(AS_STRING(sv)->chars, "value: 42", "string + int concat");

    SUBSECTION("Repeat while with stop");
    NcValue rws = nc_call_behavior(
        "service \"t\"\n"
        "to find:\n"
        "    set n to 0\n"
        "    repeat while n is below 100:\n"
        "        set n to n + 1\n"
        "        if n is equal to 7:\n"
        "            stop\n"
        "    respond with n",
        "<test>", "find", args);
    ASSERT_EQ_INT(AS_INT(rws), 7, "repeat while + stop breaks at 7");

    SUBSECTION("Repeat while with skip");
    NcValue rwsk = nc_call_behavior(
        "service \"t\"\n"
        "to sum_odd:\n"
        "    set total to 0\n"
        "    set i to 0\n"
        "    repeat while i is below 10:\n"
        "        set i to i + 1\n"
        "        if i % 2 is equal to 0:\n"
        "            skip\n"
        "        set total to total + i\n"
        "    respond with total",
        "<test>", "sum_odd", args);
    ASSERT_EQ_INT(AS_INT(rwsk), 25, "repeat while + skip sums odd 1..9 = 25");

    SUBSECTION("Repeat while list accumulation");
    NcValue rwla = nc_call_behavior(
        "service \"t\"\n"
        "to build:\n"
        "    set result to []\n"
        "    set i to 0\n"
        "    repeat while i is below 4:\n"
        "        set result to result + [i]\n"
        "        set i to i + 1\n"
        "    respond with len(result)",
        "<test>", "build", args);
    ASSERT_EQ_INT(AS_INT(rwla), 4, "repeat while builds list of len 4");

    SUBSECTION("Nested repeat while");
    NcValue rwn = nc_call_behavior(
        "service \"t\"\n"
        "to matrix:\n"
        "    set total to 0\n"
        "    set i to 0\n"
        "    repeat while i is below 3:\n"
        "        set j to 0\n"
        "        repeat while j is below 3:\n"
        "            set total to total + 1\n"
        "            set j to j + 1\n"
        "        set i to i + 1\n"
        "    respond with total",
        "<test>", "matrix", args);
    ASSERT_EQ_INT(AS_INT(rwn), 9, "nested repeat while 3x3 = 9");

    nc_map_free(args);
}

/* ═══════════════════════════════════════════════════════════
 *  14. MIDDLEWARE — AUTH
 * ═══════════════════════════════════════════════════════════ */

static void test_middleware_auth(void) {
    SECTION("Middleware - Auth");

    /* Empty auth header should not authenticate */
    NcAuthContext ctx1 = nc_mw_auth_check("");
    ASSERT_FALSE(ctx1.authenticated, "empty auth header rejected");

    NcAuthContext ctx2 = nc_mw_auth_check(NULL);
    ASSERT_FALSE(ctx2.authenticated, "NULL auth header rejected");

    /* Malformed JWT (no dots) should be rejected */
    NcAuthContext ctx3 = nc_mw_auth_check("Bearer invalidtoken");
    ASSERT_FALSE(ctx3.authenticated, "malformed JWT rejected");

    /* Short bearer token should be rejected */
    NcAuthContext ctx4 = nc_mw_auth_check("Bearer abc");
    ASSERT_FALSE(ctx4.authenticated, "short bearer token rejected");

    /* API key with no NC_API_KEYS set — should reject */
    NcAuthContext ctx5 = nc_mw_auth_check("ApiKey some-random-key");
    ASSERT_FALSE(ctx5.authenticated, "API key without NC_API_KEYS rejected");

    /*
     * JWT tests with HMAC-SHA256 signature verification.
     * Secret: "test-secret-key-for-nc-unit-tests"
     *
     * Valid token (exp=9999999999, sub="testuser"):
     *   Header:  {"alg":"HS256","typ":"JWT"}
     *   Payload: {"sub":"testuser","exp":9999999999}
     *   Signed with HMAC-SHA256 using the secret above.
     *
     * Expired token (exp=1000000000, sub="old"):
     *   Same secret, but expiry is in the past.
     *
     * These tokens are pre-computed. To regenerate:
     *   python3 -c "import jwt; print(jwt.encode({'sub':'testuser','exp':9999999999}, 'test-secret-key-for-nc-unit-tests', algorithm='HS256'))"
     */
    nc_setenv("NC_JWT_SECRET", "test-secret-key-for-nc-unit-tests", 1);

    /* Valid JWT — HMAC-SHA256 signed with secret above
     * Payload: {"sub":"testuser","exp":9999999999} */
    NcAuthContext ctx6 = nc_mw_auth_check(
        "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJzdWIiOiJ0ZXN0dXNlciIsImV4cCI6OTk5OTk5OTk5OX0."
        "A9j_Ve6xMdyZb2WFwG1Q9GUyXJFWWSfkE1kKTZngsNg");
    ASSERT_TRUE(ctx6.authenticated, "valid signed JWT accepted");
    ASSERT_EQ_STR(ctx6.user_id, "testuser", "JWT sub claim extracted");

    /* Expired JWT — HMAC-SHA256 signed with same secret
     * Payload: {"sub":"old","exp":1000000000} */
    NcAuthContext ctx7 = nc_mw_auth_check(
        "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJzdWIiOiJvbGQiLCJleHAiOjEwMDAwMDAwMDB9."
        "VovL2gzZ-F1ezT7JCsGN1iYAi2qMXnaQGiglvZqw5_w");
    ASSERT_FALSE(ctx7.authenticated, "expired signed JWT rejected");

    /* Unsigned JWT (empty sig) should be rejected when secret is set */
    NcAuthContext ctx8 = nc_mw_auth_check(
        "Bearer eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ0ZXN0dXNlciIsImV4cCI6OTk5OTk5OTk5OX0.");
    ASSERT_FALSE(ctx8.authenticated, "unsigned JWT rejected when secret set");
}

/* ═══════════════════════════════════════════════════════════
 *  15. MIDDLEWARE — RATE LIMITING
 * ═══════════════════════════════════════════════════════════ */

static void test_middleware_rate_limit(void) {
    SECTION("Middleware - Rate Limiting");

    /* First request from an IP should pass */
    bool r1 = nc_mw_rate_limit_check("192.168.1.100");
    ASSERT_TRUE(r1, "first request passes");

    /* Second request from same IP should pass */
    bool r2 = nc_mw_rate_limit_check("192.168.1.100");
    ASSERT_TRUE(r2, "second request passes");

    /* Different IP should also pass */
    bool r3 = nc_mw_rate_limit_check("10.0.0.1");
    ASSERT_TRUE(r3, "different IP passes");
}

/* ═══════════════════════════════════════════════════════════
 *  16. GARBAGE COLLECTOR
 * ═══════════════════════════════════════════════════════════ */

static void test_gc(void) {
    SECTION("Garbage Collector");

    /* Init should succeed */
    nc_gc_init();

    /* Allocate some objects */
    NcString *s1 = nc_gc_alloc_string("hello", 5);
    ASSERT_TRUE(s1 != NULL, "gc alloc string");
    ASSERT_EQ_STR(s1->chars, "hello", "gc string content");

    NcList *l1 = nc_gc_alloc_list();
    ASSERT_TRUE(l1 != NULL, "gc alloc list");

    NcMap *m1 = nc_gc_alloc_map();
    ASSERT_TRUE(m1 != NULL, "gc alloc map");

    /* Push root and collect — objects should survive */
    nc_gc_push_root(NC_STRING(s1));
    nc_gc_collect();
    /* s1 should still be valid since it's a root */
    ASSERT_EQ_STR(s1->chars, "hello", "gc root survives collection");
    nc_gc_pop_root();

    /* Stats should not crash */
    nc_gc_stats();

    nc_gc_shutdown();
    nc_gc_init();
}

/* ═══════════════════════════════════════════════════════════
 *  17. PLATFORM ABSTRACTIONS
 * ═══════════════════════════════════════════════════════════ */

static void test_platform(void) {
    SECTION("Platform Abstractions");

    /* nc_tempdir should return a non-empty path */
    const char *tmp = nc_tempdir();
    ASSERT_TRUE(tmp != NULL, "nc_tempdir not NULL");
    ASSERT_TRUE(tmp[0] != '\0', "nc_tempdir not empty");

    /* nc_getpid should return positive */
    int pid = nc_getpid();
    ASSERT_TRUE(pid > 0, "nc_getpid positive");

    /* nc_cpu_count should return at least 1 */
    int cpus = nc_cpu_count();
    ASSERT_TRUE(cpus >= 1, "nc_cpu_count >= 1");

    /* nc_sleep_ms should not crash */
    nc_sleep_ms(1);
    ASSERT_TRUE(true, "nc_sleep_ms(1) OK");

    /* nc_clock_ms should return positive */
    double t = nc_clock_ms();
    ASSERT_TRUE(t > 0, "nc_clock_ms positive");

    /* nc_realtime_ms should return large number (epoch ms) */
    double rt = nc_realtime_ms();
    ASSERT_TRUE(rt > 1000000000000.0, "nc_realtime_ms epoch ms");

    /* nc_isatty on stdout */
    /* Can't assert specific value (depends on environment) but should not crash */
    (void)nc_isatty(nc_fileno(stdout));
    ASSERT_TRUE(true, "nc_isatty/nc_fileno OK");

    /* nc_mkdir on existing temp dir should be fine (EEXIST) */
    nc_mkdir(nc_tempdir());
    ASSERT_TRUE(true, "nc_mkdir existing dir OK");

    /* nc_setenv / getenv roundtrip */
    nc_setenv("NC_TEST_VAR", "test_value_123", 1);
    const char *v = getenv("NC_TEST_VAR");
    ASSERT_TRUE(v != NULL, "nc_setenv sets variable");
    if (v) ASSERT_EQ_STR(v, "test_value_123", "nc_setenv value correct");
}

/* ═══════════════════════════════════════════════════════════
 *  18. JSON EDGE CASES
 * ═══════════════════════════════════════════════════════════ */

static void test_json_edge_cases(void) {
    SECTION("JSON Edge Cases");

    /* Empty string */
    NcValue empty = nc_json_parse("");
    ASSERT_TRUE(IS_NONE(empty), "empty string parses to none");

    /* Null input */
    NcValue null_in = nc_json_parse(NULL);
    ASSERT_TRUE(IS_NONE(null_in), "NULL parses to none");

    /* Nested objects */
    NcValue nested = nc_json_parse("{\"a\":{\"b\":{\"c\":42}}}");
    ASSERT_TRUE(IS_MAP(nested), "nested JSON is map");

    /* Array of mixed types */
    NcValue arr = nc_json_parse("[1, \"hello\", true, null, 3.14]");
    ASSERT_TRUE(IS_LIST(arr), "mixed array is list");
    if (IS_LIST(arr)) {
        ASSERT_EQ_INT(AS_LIST(arr)->count, 5, "mixed array has 5 elements");
    }

    /* Serialize and re-parse roundtrip */
    NcMap *m = nc_map_new();
    nc_map_set(m, nc_string_from_cstr("key"), NC_STRING(nc_string_from_cstr("value")));
    nc_map_set(m, nc_string_from_cstr("num"), NC_INT(42));
    char *json = nc_json_serialize(NC_MAP(m), false);
    ASSERT_TRUE(json != NULL, "serialize produces output");
    if (json) {
        NcValue reparsed = nc_json_parse(json);
        ASSERT_TRUE(IS_MAP(reparsed), "roundtrip produces map");
        free(json);
    }
    nc_map_free(m);

    /* Large string should not truncate */
    char big[16384];
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    char json_big[16400];
    snprintf(json_big, sizeof(json_big), "\"%s\"", big);
    NcValue big_val = nc_json_parse(json_big);
    ASSERT_TRUE(IS_STRING(big_val), "large string parses");
    if (IS_STRING(big_val)) {
        ASSERT_TRUE(AS_STRING(big_val)->length > 8000, "large string not truncated");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  19. THREAD POOL / ASYNC
 * ═══════════════════════════════════════════════════════════ */

static void test_async_pool(void) {
    SECTION("Thread Pool / Async");

    /* Pool init should succeed */
    nc_pool_init();
    int wc = nc_pool_worker_count();
    ASSERT_TRUE(wc >= 1, "pool has workers");

    /* Parallel map with no tasks */
    NcValue empty = nc_parallel_map(NULL, NULL, 0);
    ASSERT_TRUE(IS_LIST(empty), "parallel_map(0) returns list");

    nc_pool_shutdown();
    ASSERT_TRUE(true, "pool shutdown OK");

    /* Reinit for rest of tests */
    nc_pool_init();
}

/* ═══════════════════════════════════════════════════════════
 *  20. ISSUE FIXES (#1–#8)
 *
 *  Cross-platform regression tests for all 8 issues.
 *  These tests run identically on Linux, macOS, and Windows.
 * ═══════════════════════════════════════════════════════════ */

static void test_issue_fixes(void) {
    SECTION("Issue Fixes (#1–#8)");
    NcMap *args = nc_map_new();

    /* ── ISSUE 1: Sandbox warning on blocked write_file ──
     * Tested by checking nc_sandbox_check returns false without env var.
     * The warning goes to stderr (verified manually); here we verify the
     * return value is correct on all platforms. */
    SUBSECTION("Issue 1: sandbox check returns false without env");
    {
#ifdef NC_WINDOWS
        _putenv("NC_ALLOW_FILE_WRITE=");
#else
        unsetenv("NC_ALLOW_FILE_WRITE");
#endif
        bool allowed = nc_sandbox_check("file_write");
        ASSERT_FALSE(allowed, "file_write blocked without NC_ALLOW_FILE_WRITE");

        nc_setenv("NC_ALLOW_FILE_WRITE", "1", 1);
        bool allowed2 = nc_sandbox_check("file_write");
        ASSERT_TRUE(allowed2, "file_write allowed with NC_ALLOW_FILE_WRITE=1");

#ifdef NC_WINDOWS
        _putenv("NC_ALLOW_FILE_WRITE=");
#else
        unsetenv("NC_ALLOW_FILE_WRITE");
#endif
    }

    /* ── ISSUE 2: Multi-line strings (triple-quote) ──
     * Verify the lexer correctly tokenizes triple-quoted strings.
     * This is cross-platform: no OS-specific behavior. */
    SUBSECTION("Issue 2: triple-quote strings");
    {
        NcLexer *lex = nc_lexer_new(
            "set x to \"\"\"line one\nline two\nline three\"\"\"", "<test>");
        nc_lexer_tokenize(lex);
        int str_count = count_token_type(lex, TOK_STRING);
        ASSERT_EQ_INT(str_count, 1, "triple-quote produces 1 string token");
        /* Find the string token and verify it contains newlines */
        for (int i = 0; i < lex->token_count; i++) {
            if (lex->tokens[i].type == TOK_STRING) {
                ASSERT_TRUE(lex->tokens[i].length > 10,
                    "triple-quote string has substantial length");
                break;
            }
        }
        nc_lexer_free(lex);
    }

    SUBSECTION("Issue 2: escaped strings in function calls");
    {
        NcLexer *lex = nc_lexer_new(
            "set x to trim(\"\\n\\thello\\n\\t\")", "<test>");
        nc_lexer_tokenize(lex);
        ASSERT_EQ_INT(count_token_type(lex, TOK_RPAREN), 1,
            "closing paren preserved after escaped string");
        nc_lexer_free(lex);
    }

    /* ── ISSUE 3: Dynamic map key access ──
     * Verify map[variable] works via interpreter. */
    SUBSECTION("Issue 3: map[variable] read access");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set db to {\"alice\": 100, \"bob\": 200}\n"
            "    set key to \"bob\"\n"
            "    respond with db[key]",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 200, "map[variable] returns correct value");
    }

    SUBSECTION("Issue 3: set map[variable] write access");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set scores to {}\n"
            "    set name to \"alice\"\n"
            "    set scores[name] to 99\n"
            "    respond with scores[\"alice\"]",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 99, "set map[variable] stores correctly");
    }

    SUBSECTION("Issue 3: set map.field[variable] write access");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set db to {\"edges\": {}}\n"
            "    set eid to \"a_to_b\"\n"
            "    set db.edges[eid] to 42\n"
            "    respond with db.edges[\"a_to_b\"]",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 42, "set map.field[variable] works");
    }

    /* ── ISSUE 4: 'check' as variable name ──
     * Verify 'check' is treated as identifier after 'set'. */
    SUBSECTION("Issue 4: 'check' as variable name");
    {
        NcLexer *lex = nc_lexer_new(
            "to test:\n    set check to 42\n    respond with check", "<test>");
        nc_lexer_tokenize(lex);
        /* After SET, 'check' should be TOK_IDENTIFIER, not TOK_CHECK */
        int check_as_kw = 0;
        int check_as_id = 0;
        for (int i = 0; i < lex->token_count; i++) {
            if (lex->tokens[i].type == TOK_CHECK) check_as_kw++;
            if (lex->tokens[i].type == TOK_IDENTIFIER &&
                lex->tokens[i].length == 5 &&
                memcmp(lex->tokens[i].start, "check", 5) == 0)
                check_as_id++;
        }
        ASSERT_EQ_INT(check_as_kw, 0, "'check' not tokenized as keyword after set");
        ASSERT_TRUE(check_as_id >= 1, "'check' tokenized as identifier after set");
        nc_lexer_free(lex);
    }

    SUBSECTION("Issue 4: 'check' as variable in interpreter");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set check to \"passed\"\n"
            "    respond with check",
            "<test>", "test", args);
        ASSERT_TRUE(IS_STRING(r), "check variable is string");
        ASSERT_EQ_STR(AS_STRING(r)->chars, "passed", "check variable value");
    }

    /* ── ISSUE 5: Missing params default to none ──
     * When a behavior param is missing from args, it should be none,
     * not the global service config. */
    SUBSECTION("Issue 5: missing param is none, not config");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "configure:\n"
            "    port: 7700\n"
            "to test with config:\n"
            "    if config is equal to nothing:\n"
            "        respond with \"none\"\n"
            "    otherwise:\n"
            "        respond with \"has_value\"",
            "<test>", "test", args);
        ASSERT_TRUE(IS_STRING(r), "issue5 returns string");
        ASSERT_EQ_STR(AS_STRING(r)->chars, "none",
            "missing param is none, not service config");
    }

    /* ── ISSUE 6: split() edge cases ──
     * split on empty string, split result type, etc. */
    SUBSECTION("Issue 6: split basic");
    {
        NcString *csv = nc_string_from_cstr("a,b,c");
        NcString *delim = nc_string_from_cstr(",");
        NcValue result = nc_stdlib_split(csv, delim);
        ASSERT_TRUE(IS_LIST(result), "split returns list");
        ASSERT_EQ_INT(AS_LIST(result)->count, 3, "split('a,b,c', ',') = 3 parts");
        nc_string_free(csv);
        nc_string_free(delim);
    }

    SUBSECTION("Issue 6: split empty string returns empty list");
    {
        NcString *empty = nc_string_from_cstr("");
        NcString *delim = nc_string_from_cstr(",");
        NcValue result = nc_stdlib_split(empty, delim);
        ASSERT_TRUE(IS_LIST(result), "split('') returns list");
        ASSERT_EQ_INT(AS_LIST(result)->count, 0, "split('') has 0 elements");
        nc_string_free(empty);
        nc_string_free(delim);
    }

    SUBSECTION("Issue 6: split with no delimiter match");
    {
        NcString *text = nc_string_from_cstr("no_commas_here");
        NcString *delim = nc_string_from_cstr(",");
        NcValue result = nc_stdlib_split(text, delim);
        ASSERT_TRUE(IS_LIST(result), "split no-match returns list");
        ASSERT_EQ_INT(AS_LIST(result)->count, 1, "split no-match has 1 element");
        if (AS_LIST(result)->count > 0) {
            ASSERT_EQ_STR(AS_STRING(AS_LIST(result)->items[0])->chars,
                "no_commas_here", "split no-match preserves original");
        }
        nc_string_free(text);
        nc_string_free(delim);
    }

    SUBSECTION("Issue 6: split newlines");
    {
        NcString *text = nc_string_from_cstr("line1\nline2\nline3");
        NcString *delim = nc_string_from_cstr("\n");
        NcValue result = nc_stdlib_split(text, delim);
        ASSERT_TRUE(IS_LIST(result), "split newlines returns list");
        ASSERT_EQ_INT(AS_LIST(result)->count, 3, "split newlines = 3 lines");
        nc_string_free(text);
        nc_string_free(delim);
    }

    SUBSECTION("Issue 6: split via interpreter with type coercion");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set parts to split(\"a-b-c\", \"-\")\n"
            "    respond with len(parts)",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 3, "split in interpreter returns 3");
    }

    /* ── ISSUE 7: time_format() and time_iso() ── */
    SUBSECTION("Issue 7: time_iso returns ISO 8601 string");
    {
        NcValue iso = nc_stdlib_time_iso((double)time(NULL));
        ASSERT_TRUE(IS_STRING(iso), "time_iso returns string");
        ASSERT_TRUE(AS_STRING(iso)->length >= 19, "ISO 8601 has >= 19 chars");
        /* Should contain 'T' separator and 'Z' suffix */
        ASSERT_TRUE(strchr(AS_STRING(iso)->chars, 'T') != NULL,
            "ISO 8601 contains T");
        ASSERT_TRUE(strchr(AS_STRING(iso)->chars, 'Z') != NULL,
            "ISO 8601 contains Z");
    }

    SUBSECTION("Issue 7: time_format with custom format");
    {
        NcValue fmt = nc_stdlib_time_format((double)time(NULL), "%Y");
        ASSERT_TRUE(IS_STRING(fmt), "time_format returns string");
        ASSERT_EQ_INT(AS_STRING(fmt)->length, 4, "year format is 4 chars");
    }

    SUBSECTION("Issue 7: time_format default format");
    {
        NcValue fmt = nc_stdlib_time_format((double)time(NULL), NULL);
        ASSERT_TRUE(IS_STRING(fmt), "time_format(default) returns string");
        ASSERT_TRUE(AS_STRING(fmt)->length >= 19,
            "default format YYYY-MM-DD HH:MM:SS >= 19 chars");
    }

    SUBSECTION("Issue 7: time_iso via interpreter");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set iso to time_iso()\n"
            "    respond with contains(iso, \"T\")",
            "<test>", "test", args);
        ASSERT_TRUE(IS_BOOL(r) && AS_BOOL(r), "time_iso() in NC contains T");
    }

    /* ── ISSUE 8: Configure block syntax ──
     * Verify all three separator styles parse without error. */
    SUBSECTION("Issue 8: configure block with colon syntax");
    {
        NcParser *p;
        NcASTNode *prog = parse_src(
            "service \"t\"\n"
            "configure:\n"
            "    port: 7700\n"
            "    debug: true\n"
            "to test:\n"
            "    respond with 1", &p);
        ASSERT_FALSE(p->had_error, "colon syntax parses");
        ASSERT_TRUE(prog->as.program.configure != NULL, "configure block exists");
        if (prog->as.program.configure) {
            NcString *pk = nc_string_from_cstr("port");
            ASSERT_TRUE(nc_map_has(prog->as.program.configure, pk),
                "port key exists with colon syntax");
            nc_string_free(pk);
        }
        nc_parser_free(p);
    }

    SUBSECTION("Issue 8: configure block with is syntax");
    {
        NcParser *p;
        NcASTNode *prog = parse_src(
            "service \"t\"\n"
            "configure:\n"
            "    port is 8080\n"
            "    model is \"my-model\"\n"
            "to test:\n"
            "    respond with 1", &p);
        ASSERT_FALSE(p->had_error, "is syntax parses");
        if (prog->as.program.configure) {
            NcString *pk = nc_string_from_cstr("port");
            ASSERT_TRUE(nc_map_has(prog->as.program.configure, pk),
                "port key exists with is syntax");
            nc_string_free(pk);
        }
        nc_parser_free(p);
    }

    SUBSECTION("Issue 8: configure block with = syntax");
    {
        NcParser *p;
        NcASTNode *prog = parse_src(
            "service \"t\"\n"
            "configure:\n"
            "    port = 9090\n"
            "    debug = false\n"
            "to test:\n"
            "    respond with 1", &p);
        ASSERT_FALSE(p->had_error, "= syntax parses");
        if (prog->as.program.configure) {
            NcString *pk = nc_string_from_cstr("port");
            ASSERT_TRUE(nc_map_has(prog->as.program.configure, pk),
                "port key exists with = syntax");
            nc_string_free(pk);
        }
        nc_parser_free(p);
    }

    SUBSECTION("Issue 8: configure mixed syntax");
    {
        NcParser *p;
        NcASTNode *prog = parse_src(
            "service \"t\"\n"
            "configure:\n"
            "    port: 7700\n"
            "    model is \"my-model\"\n"
            "    debug = true\n"
            "to test:\n"
            "    respond with 1", &p);
        ASSERT_FALSE(p->had_error, "mixed syntax parses");
        if (prog->as.program.configure) {
            ASSERT_EQ_INT(prog->as.program.configure->count, 3,
                "3 config keys with mixed syntax");
        }
        nc_parser_free(p);
    }

    /* ── Cross-platform: path separators in strings ── */
    SUBSECTION("Cross-platform: Windows-style backslash paths");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set path to \"C:\\\\Users\\\\test\\\\file.txt\"\n"
            "    respond with contains(path, \"Users\")",
            "<test>", "test", args);
        ASSERT_TRUE(IS_BOOL(r) && AS_BOOL(r),
            "backslash path contains 'Users'");
    }

    SUBSECTION("Cross-platform: Unix-style forward slash paths");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set path to \"/home/user/file.txt\"\n"
            "    respond with contains(path, \"user\")",
            "<test>", "test", args);
        ASSERT_TRUE(IS_BOOL(r) && AS_BOOL(r),
            "forward slash path contains 'user'");
    }

    /* ── Cross-platform: repeat while synonym ──
     * Verifies repeat while produces identical results to while
     * on all platforms (Linux, macOS, Windows). */
    SUBSECTION("Cross-platform: repeat while basic counter");
    {
        NcValue r_while = nc_call_behavior(
            "service \"t\"\nto test:\n    set n to 0\n    while n is below 10:\n        set n to n + 1\n    respond with n",
            "<test>", "test", args);
        NcValue r_repeat = nc_call_behavior(
            "service \"t\"\nto test:\n    set n to 0\n    repeat while n is below 10:\n        set n to n + 1\n    respond with n",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r_while), AS_INT(r_repeat),
            "repeat while == while (both count to 10)");
    }

    SUBSECTION("Cross-platform: repeat while with string building");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set result to \"\"\n"
            "    set i to 0\n"
            "    repeat while i is below 3:\n"
            "        set result to result + str(i)\n"
            "        set i to i + 1\n"
            "    respond with result",
            "<test>", "test", args);
        ASSERT_EQ_STR(AS_STRING(r)->chars, "012",
            "repeat while string build works cross-platform");
    }

    SUBSECTION("Cross-platform: repeat while with stop and respond");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set i to 0\n"
            "    repeat while i is below 1000:\n"
            "        set i to i + 1\n"
            "        if i is equal to 42:\n"
            "            respond with i\n"
            "    respond with -1",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 42,
            "repeat while + respond exits at 42");
    }

    SUBSECTION("Cross-platform: repeat while zero iterations");
    {
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to test:\n"
            "    set n to 100\n"
            "    repeat while n is below 0:\n"
            "        set n to n + 1\n"
            "    respond with n",
            "<test>", "test", args);
        ASSERT_EQ_INT(AS_INT(r), 100,
            "repeat while false condition = zero iterations");
    }

    nc_map_free(args);
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: SHA-256 HASHING
 * ═══════════════════════════════════════════════════════════ */

static void test_hash_sha256(void) {
    SECTION("SHA-256 Hashing");

    SUBSECTION("Known hash vectors");
    {
        /* SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 */
        NcValue h = nc_stdlib_hash_sha256("");
        ASSERT_TRUE(IS_STRING(h), "empty string hashes to string");
        ASSERT_EQ_STR(AS_STRING(h)->chars,
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "SHA-256 of empty string");
    }
    {
        /* SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad */
        NcValue h = nc_stdlib_hash_sha256("abc");
        ASSERT_TRUE(IS_STRING(h), "abc hashes to string");
        ASSERT_EQ_STR(AS_STRING(h)->chars,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "SHA-256 of 'abc'");
    }
    {
        NcValue h = nc_stdlib_hash_sha256("hello world");
        ASSERT_TRUE(IS_STRING(h), "hello world hashes to string");
        ASSERT_EQ_INT((int)strlen(AS_STRING(h)->chars), 64, "SHA-256 digest is 64 hex chars");
    }

    SUBSECTION("Deterministic");
    {
        NcValue h1 = nc_stdlib_hash_sha256("test");
        NcValue h2 = nc_stdlib_hash_sha256("test");
        ASSERT_EQ_STR(AS_STRING(h1)->chars, AS_STRING(h2)->chars,
            "same input produces same hash");
    }

    SUBSECTION("Different inputs produce different hashes");
    {
        NcValue h1 = nc_stdlib_hash_sha256("hello");
        NcValue h2 = nc_stdlib_hash_sha256("world");
        ASSERT_TRUE(strcmp(AS_STRING(h1)->chars, AS_STRING(h2)->chars) != 0,
            "different inputs produce different hashes");
    }

    SUBSECTION("NULL input");
    {
        NcValue h = nc_stdlib_hash_sha256(NULL);
        ASSERT_TRUE(IS_NONE(h), "NULL returns none");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: PASSWORD HASHING
 * ═══════════════════════════════════════════════════════════ */

static void test_password_hashing(void) {
    SECTION("Password Hashing");

    SUBSECTION("Hash and verify correct password");
    {
        NcValue stored = nc_stdlib_hash_password("my_secret_password");
        ASSERT_TRUE(IS_STRING(stored), "hash_password returns string");
        const char *hash_str = AS_STRING(stored)->chars;

        /* Verify format: $nc$<32 hex chars>$<64 hex chars> */
        ASSERT_TRUE(strncmp(hash_str, "$nc$", 4) == 0, "hash starts with $nc$");
        ASSERT_TRUE(strlen(hash_str) == 4 + 32 + 1 + 64, "hash has correct length");

        NcValue ok = nc_stdlib_verify_password("my_secret_password", hash_str);
        ASSERT_TRUE(IS_BOOL(ok) && AS_BOOL(ok), "correct password verifies");
    }

    SUBSECTION("Reject wrong password");
    {
        NcValue stored = nc_stdlib_hash_password("correct_password");
        NcValue bad = nc_stdlib_verify_password("wrong_password", AS_STRING(stored)->chars);
        ASSERT_TRUE(IS_BOOL(bad) && !AS_BOOL(bad), "wrong password rejected");
    }

    SUBSECTION("Different salts each time");
    {
        NcValue h1 = nc_stdlib_hash_password("same_password");
        NcValue h2 = nc_stdlib_hash_password("same_password");
        ASSERT_TRUE(strcmp(AS_STRING(h1)->chars, AS_STRING(h2)->chars) != 0,
            "same password produces different hashes (random salt)");

        /* But both should verify */
        NcValue v1 = nc_stdlib_verify_password("same_password", AS_STRING(h1)->chars);
        NcValue v2 = nc_stdlib_verify_password("same_password", AS_STRING(h2)->chars);
        ASSERT_TRUE(AS_BOOL(v1), "first hash verifies");
        ASSERT_TRUE(AS_BOOL(v2), "second hash verifies");
    }

    SUBSECTION("Reject malformed stored hash");
    {
        NcValue r1 = nc_stdlib_verify_password("pw", "not-a-hash");
        ASSERT_FALSE(AS_BOOL(r1), "malformed hash rejected");

        NcValue r2 = nc_stdlib_verify_password("pw", "$nc$short$bad");
        ASSERT_FALSE(AS_BOOL(r2), "truncated hash rejected");

        NcValue r3 = nc_stdlib_verify_password(NULL, NULL);
        ASSERT_FALSE(AS_BOOL(r3), "NULL inputs rejected");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: HMAC-SHA256
 * ═══════════════════════════════════════════════════════════ */

static void test_hmac(void) {
    SECTION("HMAC-SHA256");

    SUBSECTION("Known vector");
    {
        NcValue h = nc_stdlib_hash_hmac("", "");
        ASSERT_TRUE(IS_STRING(h), "HMAC returns string");
        ASSERT_EQ_INT((int)strlen(AS_STRING(h)->chars), 64, "HMAC is 64 hex chars");
    }

    SUBSECTION("Deterministic");
    {
        NcValue h1 = nc_stdlib_hash_hmac("message", "key");
        NcValue h2 = nc_stdlib_hash_hmac("message", "key");
        ASSERT_EQ_STR(AS_STRING(h1)->chars, AS_STRING(h2)->chars,
            "same inputs produce same HMAC");
    }

    SUBSECTION("Different keys produce different MACs");
    {
        NcValue h1 = nc_stdlib_hash_hmac("data", "key1");
        NcValue h2 = nc_stdlib_hash_hmac("data", "key2");
        ASSERT_TRUE(strcmp(AS_STRING(h1)->chars, AS_STRING(h2)->chars) != 0,
            "different keys produce different MACs");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: SESSION MANAGEMENT
 * ═══════════════════════════════════════════════════════════ */

static void test_sessions(void) {
    SECTION("Session Management");

    SUBSECTION("Create session");
    {
        NcValue sid = nc_stdlib_session_create();
        ASSERT_TRUE(IS_STRING(sid), "session_create returns string");
        ASSERT_TRUE(strlen(AS_STRING(sid)->chars) > 10, "session ID has length");
        ASSERT_TRUE(strncmp(AS_STRING(sid)->chars, "nc_", 3) == 0, "session ID starts with nc_");
    }

    SUBSECTION("Set and get session data");
    {
        NcValue sid = nc_stdlib_session_create();
        const char *id = AS_STRING(sid)->chars;

        nc_stdlib_session_set(id, "username", NC_STRING(nc_string_from_cstr("alice")));
        nc_stdlib_session_set(id, "role", NC_STRING(nc_string_from_cstr("admin")));
        nc_stdlib_session_set(id, "score", NC_INT(42));

        NcValue user = nc_stdlib_session_get(id, "username");
        ASSERT_TRUE(IS_STRING(user), "session_get returns stored string");
        ASSERT_EQ_STR(AS_STRING(user)->chars, "alice", "session stores correct value");

        NcValue role = nc_stdlib_session_get(id, "role");
        ASSERT_EQ_STR(AS_STRING(role)->chars, "admin", "session stores role");

        NcValue score = nc_stdlib_session_get(id, "score");
        ASSERT_TRUE(IS_INT(score), "session stores int");
        ASSERT_EQ_INT((int)AS_INT(score), 42, "session int value correct");
    }

    SUBSECTION("Missing key returns none");
    {
        NcValue sid = nc_stdlib_session_create();
        NcValue missing = nc_stdlib_session_get(AS_STRING(sid)->chars, "nonexistent");
        ASSERT_TRUE(IS_NONE(missing), "missing key returns none");
    }

    SUBSECTION("Session exists");
    {
        NcValue sid = nc_stdlib_session_create();
        NcValue exists = nc_stdlib_session_exists(AS_STRING(sid)->chars);
        ASSERT_TRUE(AS_BOOL(exists), "created session exists");

        NcValue no = nc_stdlib_session_exists("nc_nonexistent_session_id");
        ASSERT_FALSE(AS_BOOL(no), "fake session does not exist");
    }

    SUBSECTION("Destroy session");
    {
        NcValue sid = nc_stdlib_session_create();
        const char *id = AS_STRING(sid)->chars;
        nc_stdlib_session_set(id, "key", NC_STRING(nc_string_from_cstr("val")));

        nc_stdlib_session_destroy(id);

        NcValue gone = nc_stdlib_session_exists(id);
        ASSERT_FALSE(AS_BOOL(gone), "destroyed session no longer exists");

        NcValue data = nc_stdlib_session_get(id, "key");
        ASSERT_TRUE(IS_NONE(data), "destroyed session data returns none");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: CIRCUIT BREAKER
 * ═══════════════════════════════════════════════════════════ */

static void test_circuit_breaker(void) {
    SECTION("Circuit Breaker");

    SUBSECTION("Initial state is closed (allows requests)");
    {
        ASSERT_TRUE(nc_cb_allow("test_service"), "new breaker allows requests");
    }

    SUBSECTION("Opens after failure threshold");
    {
        for (int i = 0; i < 5; i++)
            nc_cb_record_failure("cb_open_test");
        ASSERT_FALSE(nc_cb_allow("cb_open_test"), "breaker opens after 5 failures");
    }

    SUBSECTION("Success resets closed breaker");
    {
        nc_cb_record_success("cb_reset_test");
        ASSERT_TRUE(nc_cb_allow("cb_reset_test"), "success keeps breaker closed");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: FEATURE FLAGS
 * ═══════════════════════════════════════════════════════════ */

static void test_feature_flags(void) {
    SECTION("Feature Flags");

    nc_ff_init();

    SUBSECTION("Undefined flag defaults to false (no env)");
    {
        ASSERT_FALSE(nc_ff_is_enabled("undefined_flag", NULL),
            "undefined flag is disabled");
    }

    SUBSECTION("Set and check flag");
    {
        nc_ff_set("test_flag", true, 100);
        ASSERT_TRUE(nc_ff_is_enabled("test_flag", NULL), "enabled flag returns true");

        nc_ff_set("disabled_flag", false, 100);
        ASSERT_FALSE(nc_ff_is_enabled("disabled_flag", NULL), "disabled flag returns false");
    }

    SUBSECTION("Percentage rollout");
    {
        nc_ff_set("partial_flag", true, 50);
        /* Just verify it doesn't crash — actual percentage depends on hash */
        bool _ = nc_ff_is_enabled("partial_flag", "tenant_a");
        (void)_;
        ASSERT_TRUE(true, "percentage rollout does not crash");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: AUDIT LOGGING
 * ═══════════════════════════════════════════════════════════ */

static void test_audit_logging(void) {
    SECTION("Audit Logging");

    SUBSECTION("Basic audit log entry");
    {
        nc_audit_log("testuser", "login", "/api/auth", "success");
        /* No crash = pass */
        ASSERT_TRUE(true, "audit_log does not crash");
    }

    SUBSECTION("Extended audit entry");
    {
        NcAuditEntry entry = {0};
        entry.timestamp = time(NULL);
        entry.event_type = NC_AUDIT_AUTH_SUCCESS;
        strncpy(entry.user, "alice", 127);
        strncpy(entry.action, "authenticate", 127);
        strncpy(entry.target, "/api/login", 255);
        strncpy(entry.result, "success", 63);
        strncpy(entry.ip, "10.0.0.1", 47);
        strncpy(entry.tenant_id, "tenant_1", 63);
        entry.status_code = 200;
        entry.duration_ms = 12.5;

        nc_audit_log_ext(&entry);
        ASSERT_TRUE(true, "audit_log_ext does not crash");
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: REQUEST CONTEXT
 * ═══════════════════════════════════════════════════════════ */

static void test_request_context(void) {
    SECTION("Request Context");

    SUBSECTION("NULL context returns none");
    {
        nc_request_ctx_set(NULL);
        NcValue h = nc_stdlib_request_header("Authorization");
        ASSERT_TRUE(IS_NONE(h), "no context returns none");
    }

    SUBSECTION("Set and read context");
    {
        NcRequestContext ctx = {0};
        strncpy(ctx.headers[0][0], "Authorization", 255);
        strncpy(ctx.headers[0][1], "Bearer test-token-123", 255);
        strncpy(ctx.headers[1][0], "User-Agent", 255);
        strncpy(ctx.headers[1][1], "NC-Test/1.0", 255);
        ctx.header_count = 2;
        strncpy(ctx.client_ip, "192.168.1.1", 47);
        strncpy(ctx.method, "POST", 15);
        strncpy(ctx.path, "/api/test", 511);

        nc_request_ctx_set(&ctx);

        NcValue auth = nc_stdlib_request_header("Authorization");
        ASSERT_TRUE(IS_STRING(auth), "header returns string");
        ASSERT_EQ_STR(AS_STRING(auth)->chars, "Bearer test-token-123", "auth header value");

        NcValue ua = nc_stdlib_request_header("User-Agent");
        ASSERT_EQ_STR(AS_STRING(ua)->chars, "NC-Test/1.0", "user-agent value");

        NcValue missing = nc_stdlib_request_header("X-Nonexistent");
        ASSERT_TRUE(IS_NONE(missing), "missing header returns none");

        /* Case-insensitive matching */
        NcValue auth_lower = nc_stdlib_request_header("authorization");
        ASSERT_TRUE(IS_STRING(auth_lower), "case-insensitive header lookup");

        nc_request_ctx_set(NULL);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  ENTERPRISE: END-TO-END NC BEHAVIORS
 * ═══════════════════════════════════════════════════════════ */

static void test_enterprise_behaviors(void) {
    SECTION("Enterprise Behaviors (end-to-end)");

    NcMap *args = nc_map_new();

    SUBSECTION("hash_sha256 from NC code");
    {
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set h to hash_sha256(\"hello\")\n"
            "    respond with h",
            "<test>", "test", args);
        ASSERT_TRUE(IS_STRING(r), "hash_sha256 callable from NC");
        ASSERT_EQ_INT((int)strlen(AS_STRING(r)->chars), 64, "returns 64 hex chars");
    }

    SUBSECTION("hash_password + verify_password from NC code");
    {
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set stored to hash_password(\"secret123\")\n"
            "    set ok to verify_password(\"secret123\", stored)\n"
            "    respond with ok",
            "<test>", "test", args);
        ASSERT_TRUE(IS_BOOL(r) && AS_BOOL(r), "password hash+verify works from NC");
    }

    SUBSECTION("verify_password rejects wrong password from NC code");
    {
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set stored to hash_password(\"correct\")\n"
            "    set ok to verify_password(\"wrong\", stored)\n"
            "    respond with ok",
            "<test>", "test", args);
        ASSERT_TRUE(IS_BOOL(r) && !AS_BOOL(r), "wrong password rejected from NC");
    }

    SUBSECTION("session_create from NC code");
    {
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set sid to session_create()\n"
            "    respond with sid",
            "<test>", "test", args);
        ASSERT_TRUE(IS_STRING(r), "session_create returns string from NC");
    }

    SUBSECTION("session set/get from NC code");
    {
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set sid to session_create()\n"
            "    session_set(sid, \"name\", \"bob\")\n"
            "    set val to session_get(sid, \"name\")\n"
            "    respond with val",
            "<test>", "test", args);
        ASSERT_TRUE(IS_STRING(r), "session_get returns value from NC");
        ASSERT_EQ_STR(AS_STRING(r)->chars, "bob", "session stores and retrieves");
    }

    SUBSECTION("request_header without context returns nothing");
    {
        nc_request_ctx_set(NULL);
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set h to request_header(\"Authorization\")\n"
            "    if h is equal to nothing:\n"
            "        respond with \"no header\"\n"
            "    otherwise:\n"
            "        respond with h",
            "<test>", "test", args);
        NcString *rs = nc_value_to_string(r);
        ASSERT_EQ_STR(rs->chars, "no header", "request_header returns nothing without context");
        nc_string_free(rs);
    }

    SUBSECTION("feature flag from NC code");
    {
        nc_ff_init();
        nc_ff_set("nc_test_feature", true, 100);
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    if feature(\"nc_test_feature\"):\n"
            "        respond with \"enabled\"\n"
            "    otherwise:\n"
            "        respond with \"disabled\"",
            "<test>", "test", args);
        NcString *rs = nc_value_to_string(r);
        ASSERT_EQ_STR(rs->chars, "enabled", "feature flag works from NC");
        nc_string_free(rs);
    }

    nc_map_free(args);
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN
 * ═══════════════════════════════════════════════════════════ */

int main(void) {
    nc_gc_init();

    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║  NC Comprehensive Test Suite                     ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");

    test_value_constructors();
    test_truthiness();
    test_strings();
    test_lists();
    test_maps();
    test_lexer();
    test_parser();
    test_compiler_vm();
    test_interpreter();
    test_json();
    test_stdlib();
    test_chunks();
    test_value_to_string();
    test_end_to_end();
    test_middleware_auth();
    test_middleware_rate_limit();
    test_gc();
    test_platform();
    test_json_edge_cases();
    test_async_pool();
    test_issue_fixes();

    /* Enterprise feature tests */
    test_hash_sha256();
    test_password_hashing();
    test_hmac();
    test_sessions();
    test_circuit_breaker();
    test_feature_flags();
    test_audit_logging();
    test_request_context();
    test_enterprise_behaviors();

    /* Bug fix verifications */
    SECTION("Bug Fix Verification");

    SUBSECTION("max(list) returns correct max");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n    set nums to [3, 7, 1, 9, 4]\n    respond with max(nums)",
            "<test>", "test", a);
        ASSERT_TRUE(IS_INT(r) || IS_FLOAT(r), "max(list) returns number");
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 9.0, 0.01, "max([3,7,1,9,4]) = 9");
        nc_map_free(a);
    }

    SUBSECTION("min(list) returns correct min");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n    set nums to [3, 7, 1, 9, 4]\n    respond with min(nums)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 1.0, 0.01, "min([3,7,1,9,4]) = 1");
        nc_map_free(a);
    }

    SUBSECTION("round(val, decimals) preserves precision");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n    respond with round(3.14159, 2)",
            "<test>", "test", a);
        ASSERT_TRUE(IS_FLOAT(r), "round with 2 args returns float");
        ASSERT_NEAR(AS_FLOAT(r), 3.14, 0.001, "round(3.14159, 2) = 3.14");
        nc_map_free(a);
    }

    SUBSECTION("sort_by works on list of maps");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [{\"n\": \"c\", \"s\": 3}, {\"n\": \"a\", \"s\": 1}, {\"n\": \"b\", \"s\": 2}]\n"
            "    set sorted to sort_by(items, \"s\")\n"
            "    respond with sorted[0].n",
            "<test>", "test", a);
        NcString *rs = nc_value_to_string(r);
        ASSERT_EQ_STR(rs->chars, "a", "sort_by sorts by field");
        nc_string_free(rs);
        nc_map_free(a);
    }

    SUBSECTION("try/otherwise syntax");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    try:\n"
            "        set x to 42\n"
            "    otherwise:\n"
            "        set x to -1\n"
            "    respond with x",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 42.0, 0.01, "try/otherwise happy path returns 42");
        nc_map_free(a);
    }

    SUBSECTION("jwt_verify with invalid token returns false");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set result to jwt_verify(\"not.a.valid.token\")\n"
            "    respond with result",
            "<test>", "test", a);
        ASSERT_TRUE(IS_BOOL(r) && !AS_BOOL(r), "jwt_verify rejects invalid token");
        nc_map_free(a);
    }

    SUBSECTION("wait milliseconds parses");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n    wait 1 ms\n    respond with \"ok\"",
            "<test>", "test", a);
        NcString *rs = nc_value_to_string(r);
        ASSERT_EQ_STR(rs->chars, "ok", "wait milliseconds parses and runs");
        nc_string_free(rs);
        nc_map_free(a);
    }

    SUBSECTION("continue (skip) in loop");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set total to 0\n"
            "    set items to [1, 2, 3, 4, 5]\n"
            "    repeat for each item in items:\n"
            "        if item is equal to 3:\n"
            "            skip\n"
            "        set total to total + item\n"
            "    respond with total",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 12.0, 0.01, "skip (continue) skips item 3, total=12");
        nc_map_free(a);
    }

    /* ── BUG A: dot access on JSON string (VM parity) ── */
    SUBSECTION("Bug A: dot access on JSON string");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set data to \"{\\\"name\\\": \\\"alice\\\", \\\"score\\\": 95}\"\n"
            "    respond with data.name",
            "<test>", "test", a);
        NcString *rs = nc_value_to_string(r);
        ASSERT_EQ_STR(rs->chars, "alice", "dot access on JSON string returns field");
        nc_string_free(rs);
        nc_map_free(a);
    }

    /* ── BUG B: numeric types survive behavior boundaries ── */
    SUBSECTION("Bug B: list numeric types across run");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to make_list:\n"
            "    respond with [10, 20, 30]\n"
            "to test:\n"
            "    run make_list\n"
            "    respond with result[0] + result[1] + result[2]",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 60.0, 0.01, "list nums survive run boundary: 10+20+30=60");
        nc_map_free(a);
    }

    SUBSECTION("Bug B: map with list from run");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "service \"t\"\n"
            "to get_data:\n"
            "    respond with {\"values\": [1, 2, 3]}\n"
            "to test:\n"
            "    run get_data\n"
            "    respond with len(result.values)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 3.0, 0.01, "map with nested list survives run boundary");
        nc_map_free(a);
    }

    /* ── BUG C: while after repeat for each in same behavior ── */
    SUBSECTION("Bug C: while after repeat for each");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set total to 0\n"
            "    set items to [1, 2, 3]\n"
            "    repeat for each item in items:\n"
            "        set total to total + item\n"
            "    set count to 3\n"
            "    while count is above 0:\n"
            "        set total to total + count\n"
            "        set count to count - 1\n"
            "    respond with total",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 12.0, 0.01, "repeat for each + while in same behavior = 6+6=12");
        nc_map_free(a);
    }

    SUBSECTION("Bug C: repeat for each then while then repeat");
    {
        NcParser *pc;
        NcASTNode *progc = parse_src(
            "service \"t\"\n"
            "to test:\n"
            "    repeat for each x in [1,2]:\n"
            "        log x\n"
            "    while true:\n"
            "        stop\n"
            "    repeat 3 times:\n"
            "        log \"ok\"", &pc);
        ASSERT_FALSE(pc->had_error, "parse repeat+while+repeat no error");
        nc_parser_free(pc);
    }

    /* ── BUG E: blank lines inside behaviors ── */
    SUBSECTION("Bug E: blank lines between statements");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set a to 10\n"
            "\n"
            "    set b to 20\n"
            "\n"
            "\n"
            "    respond with a + b",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 30.0, 0.01, "blank lines between statements: 10+20=30");
        nc_map_free(a);
    }

    SUBSECTION("Bug E: blank lines inside loops");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set total to 0\n"
            "    set items to [1, 2, 3]\n"
            "\n"
            "    repeat for each item in items:\n"
            "        set total to total + item\n"
            "\n"
            "    respond with total",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 6.0, 0.01, "blank lines around loops: sum=6");
        nc_map_free(a);
    }

    /* ══════════════════════════════════════════════════════
     *  RUNTIME LIMITATION FIXES — regression tests
     * ══════════════════════════════════════════════════════ */

    /* ── Bug 1: Negative subtraction ──
     * 198.0 - 200.0 must return -2, not 2.
     * Real-world impact: RSI always returned 100 because losses
     * (which require negative subtraction) were never captured. */
    SECTION("Negative Subtraction");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set a to 198\n"
            "    set b to 200\n"
            "    respond with a - b",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, -2.0, 0.01, "198 - 200 = -2");
        nc_map_free(a);
    }

    SUBSECTION("float subtraction producing negatives");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set a to 198.0\n"
            "    set b to 200.0\n"
            "    respond with a - b",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, -2.0, 0.01, "198.0 - 200.0 = -2.0");
        nc_map_free(a);
    }

    SUBSECTION("RSI loss capture pattern");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set prices to [100.0, 102.0, 99.0, 97.0, 101.0]\n"
            "    set losses to 0.0\n"
            "    set prev to 100.0\n"
            "    repeat for each price in prices:\n"
            "        set change to price - prev\n"
            "        if change is below 0:\n"
            "            set losses to losses + abs(change)\n"
            "        set prev to price\n"
            "    respond with losses",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_TRUE(v > 0.0, "RSI loss capture: losses must be > 0 (got nonzero)");
        ASSERT_NEAR(v, 5.0, 0.01, "RSI loss capture: 3+2 = 5.0 total loss");
        nc_map_free(a);
    }

    SUBSECTION("negative result used in further arithmetic");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set a to 5\n"
            "    set b to 10\n"
            "    set diff to a - b\n"
            "    set doubled to diff * 2\n"
            "    respond with doubled",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, -10.0, 0.01, "(5 - 10) * 2 = -10");
        nc_map_free(a);
    }

    /* ── Bug 2: otherwise if ignored ──
     * Real-world impact: sector signal classifier —
     * strong_bullish was unreachable because bullish always overwrote it. */
    SECTION("otherwise if Chains");
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("score"), NC_INT(95));
        NcValue r = nc_call_behavior(
            "to test with score:\n"
            "    if score is above 90:\n"
            "        respond with \"A\"\n"
            "    otherwise if score is above 80:\n"
            "        respond with \"B\"\n"
            "    otherwise:\n"
            "        respond with \"C\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "otherwise if: score 95 returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "A", "otherwise if: score 95 -> A");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("score"), NC_INT(85));
        NcValue r = nc_call_behavior(
            "to test with score:\n"
            "    if score is above 90:\n"
            "        respond with \"A\"\n"
            "    otherwise if score is above 80:\n"
            "        respond with \"B\"\n"
            "    otherwise:\n"
            "        respond with \"C\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "otherwise if: score 85 returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "B", "otherwise if: score 85 -> B");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("score"), NC_INT(50));
        NcValue r = nc_call_behavior(
            "to test with score:\n"
            "    if score is above 90:\n"
            "        respond with \"A\"\n"
            "    otherwise if score is above 80:\n"
            "        respond with \"B\"\n"
            "    otherwise:\n"
            "        respond with \"C\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "otherwise if: score 50 returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "C", "otherwise if: score 50 -> C");
        nc_map_free(a);
    }

    SUBSECTION("sector signal classifier (set-based, no respond)");
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("strength"), NC_FLOAT(0.95));
        NcValue r = nc_call_behavior(
            "to test with strength:\n"
            "    set signal to \"neutral\"\n"
            "    if strength is above 0.8:\n"
            "        set signal to \"strong_bullish\"\n"
            "    otherwise if strength is above 0.5:\n"
            "        set signal to \"bullish\"\n"
            "    otherwise if strength is above 0.2:\n"
            "        set signal to \"neutral\"\n"
            "    otherwise:\n"
            "        set signal to \"bearish\"\n"
            "    respond with signal",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "sector signal: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "strong_bullish",
                "sector signal: 0.95 -> strong_bullish (not overwritten by bullish)");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("strength"), NC_FLOAT(0.6));
        NcValue r = nc_call_behavior(
            "to test with strength:\n"
            "    set signal to \"neutral\"\n"
            "    if strength is above 0.8:\n"
            "        set signal to \"strong_bullish\"\n"
            "    otherwise if strength is above 0.5:\n"
            "        set signal to \"bullish\"\n"
            "    otherwise if strength is above 0.2:\n"
            "        set signal to \"neutral\"\n"
            "    otherwise:\n"
            "        set signal to \"bearish\"\n"
            "    respond with signal",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "sector signal mid: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "bullish",
                "sector signal: 0.6 -> bullish");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("strength"), NC_FLOAT(0.1));
        NcValue r = nc_call_behavior(
            "to test with strength:\n"
            "    set signal to \"neutral\"\n"
            "    if strength is above 0.8:\n"
            "        set signal to \"strong_bullish\"\n"
            "    otherwise if strength is above 0.5:\n"
            "        set signal to \"bullish\"\n"
            "    otherwise if strength is above 0.2:\n"
            "        set signal to \"neutral\"\n"
            "    otherwise:\n"
            "        set signal to \"bearish\"\n"
            "    respond with signal",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "sector signal low: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "bearish",
                "sector signal: 0.1 -> bearish");
        nc_map_free(a);
    }

    /* ── Bug 3: respond with doesn't exit ──
     * respond with inside an if must terminate the function.
     * Real-world: can't use early-return pattern for exclusive branching. */
    SECTION("respond with Early Exit");
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("x"), NC_INT(-5));
        NcValue r = nc_call_behavior(
            "to test with x:\n"
            "    if x is below 0:\n"
            "        respond with \"negative\"\n"
            "    respond with \"positive\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "respond early exit: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "negative",
                "respond in if exits early — subsequent code must NOT run");
        nc_map_free(a);
    }

    SUBSECTION("respond with in otherwise if chain");
    {
        NcMap *a = nc_map_new();
        nc_map_set(a, nc_string_from_cstr("code"), NC_INT(404));
        NcValue r = nc_call_behavior(
            "to test with code:\n"
            "    if code is equal to 200:\n"
            "        respond with \"ok\"\n"
            "    otherwise if code is equal to 404:\n"
            "        respond with \"not_found\"\n"
            "    otherwise if code is equal to 500:\n"
            "        respond with \"server_error\"\n"
            "    respond with \"unknown\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "respond in elif chain: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "not_found",
                "respond in otherwise if exits — does NOT fall through to unknown");
        nc_map_free(a);
    }

    /* ── Bug 4: abs() unreliable ──
     * abs(negative_number) may return 0 instead of the positive value.
     * Real-world: ATR and RSI loss calculations produce zeros. */
    SECTION("abs() on Negative Numbers");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with abs(-5)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 5.0, 0.01, "abs(-5) = 5");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set x to -42\n"
            "    respond with abs(x)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 42.0, 0.01, "abs(-42) = 42");
        nc_map_free(a);
    }

    SUBSECTION("abs on result of subtraction (ATR pattern)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set high to 105.0\n"
            "    set low to 98.0\n"
            "    set prev_close to 100.0\n"
            "    set tr1 to high - low\n"
            "    set tr2 to abs(high - prev_close)\n"
            "    set tr3 to abs(low - prev_close)\n"
            "    respond with tr3",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 2.0, 0.01, "ATR: abs(98 - 100) = 2.0 (not 0)");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with abs(0)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 0.0, 0.01, "abs(0) = 0");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with abs(-3.14)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 3.14, 0.01, "abs(-3.14) = 3.14");
        nc_map_free(a);
    }

    /* ── Bug 5: average() on inline lists ──
     * average([10.0, 20.0, 30.0]) returned 0 for small inline float arrays.
     * Real-world: SMA and Bollinger Bands returned 0 with inline test data. */
    SECTION("average() on Inline Lists");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with average([10, 20, 30])",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 20.0, 0.01, "average([10, 20, 30]) = 20");
        nc_map_free(a);
    }

    SUBSECTION("inline float list (SMA pattern)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with average([10.0, 20.0, 30.0])",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 20.0, 0.01, "average([10.0, 20.0, 30.0]) = 20.0");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with average([5])",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 5.0, 0.01, "average([5]) = 5");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set nums to [100.0, 200.0, 300.0]\n"
            "    respond with average(nums)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 200.0, 0.01, "average(float variable list) = 200");
        nc_map_free(a);
    }

    SUBSECTION("sum on inline list (related)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with sum([10.0, 20.0, 30.0])",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 60.0, 0.01, "sum([10.0, 20.0, 30.0]) = 60");
        nc_map_free(a);
    }

    /* ── Bug 6: filter_by string equality ──
     * filter_by(list, "status", "equal", "PASS") returned ALL items.
     * Real-world: test pass counters reported 100% even with failures. */
    SECTION("filter_by with String Equality");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\n"
            "        {\"name\": \"a\", \"status\": \"PASS\"},\n"
            "        {\"name\": \"b\", \"status\": \"FAIL\"},\n"
            "        {\"name\": \"c\", \"status\": \"PASS\"}\n"
            "    ]\n"
            "    set passed to filter_by(items, \"status\", \"equal\", \"PASS\")\n"
            "    respond with len(passed)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 2.0, 0.01, "filter_by string equal: 2 of 3 have PASS (not 3)");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\n"
            "        {\"name\": \"a\", \"status\": \"PASS\"},\n"
            "        {\"name\": \"b\", \"status\": \"FAIL\"},\n"
            "        {\"name\": \"c\", \"status\": \"PASS\"}\n"
            "    ]\n"
            "    set failed to filter_by(items, \"status\", \"equal\", \"FAIL\")\n"
            "    respond with len(failed)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 1.0, 0.01, "filter_by string equal: exactly 1 has FAIL");
        nc_map_free(a);
    }

    SUBSECTION("filter_by with no matches");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\n"
            "        {\"name\": \"a\", \"status\": \"PASS\"},\n"
            "        {\"name\": \"b\", \"status\": \"PASS\"}\n"
            "    ]\n"
            "    set failed to filter_by(items, \"status\", \"equal\", \"FAIL\")\n"
            "    respond with len(failed)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 0.0, 0.01, "filter_by string equal: 0 items match FAIL");
        nc_map_free(a);
    }

    /* ── Bug 7: String is equal in loops ──
     * String comparison in repeat for each loops would always evaluate to yes.
     * Real-world: can't reliably count results by string field inside loops. */
    SECTION("String Equality in Loops");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\n"
            "        {\"name\": \"a\", \"status\": \"PASS\"},\n"
            "        {\"name\": \"b\", \"status\": \"FAIL\"},\n"
            "        {\"name\": \"c\", \"status\": \"PASS\"}\n"
            "    ]\n"
            "    set count to 0\n"
            "    repeat for each item in items:\n"
            "        if item.status is equal to \"PASS\":\n"
            "            set count to count + 1\n"
            "    respond with count",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 2.0, 0.01,
            "string equal in loop: 2 of 3 are PASS (not 3)");
        nc_map_free(a);
    }
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\"red\", \"blue\", \"red\", \"green\"]\n"
            "    set count to 0\n"
            "    repeat for each item in items:\n"
            "        if item is equal to \"red\":\n"
            "            set count to count + 1\n"
            "    respond with count",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 2.0, 0.01,
            "string equal in loop: 2 of 4 are red (not 4)");
        nc_map_free(a);
    }

    SUBSECTION("is not equal to in loop");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to [\"red\", \"blue\", \"red\", \"green\"]\n"
            "    set count to 0\n"
            "    repeat for each item in items:\n"
            "        if item is not equal to \"red\":\n"
            "            set count to count + 1\n"
            "    respond with count",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 2.0, 0.01,
            "string not-equal in loop: 2 of 4 are not red");
        nc_map_free(a);
    }

    SECTION("NONE vs String Equality");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set x to nothing\n"
            "    if x is equal to \"hello\":\n"
            "        respond with \"matched\"\n"
            "    respond with \"not_matched\"",
            "<test>", "test", a);
        ASSERT_TRUE(IS_STRING(r), "none != string: returns string");
        if (IS_STRING(r))
            ASSERT_EQ_STR(AS_STRING(r)->chars, "not_matched",
                "nothing is not equal to a string");
        nc_map_free(a);
    }

    /* ── slice() function ── */
    SECTION("slice() Function");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set vals to [100, 110, 120, 130, 140]\n"
            "    respond with slice(vals, 2, 5)",
            "<test>", "test", a);
        ASSERT_TRUE(IS_LIST(r), "slice returns list");
        if (IS_LIST(r)) {
            ASSERT_EQ_INT(AS_LIST(r)->count, 3, "slice(vals,2,5) has 3 elements");
            if (AS_LIST(r)->count >= 3) {
                double v0 = IS_INT(AS_LIST(r)->items[0]) ? (double)AS_INT(AS_LIST(r)->items[0]) : AS_FLOAT(AS_LIST(r)->items[0]);
                double v2 = IS_INT(AS_LIST(r)->items[2]) ? (double)AS_INT(AS_LIST(r)->items[2]) : AS_FLOAT(AS_LIST(r)->items[2]);
                ASSERT_NEAR(v0, 120.0, 0.01, "slice(vals,2,5)[0] = 120");
                ASSERT_NEAR(v2, 140.0, 0.01, "slice(vals,2,5)[2] = 140");
            }
        }
        nc_map_free(a);
    }

    SUBSECTION("slice with inline list");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    respond with slice([10, 20, 30, 40, 50], 1, 4)",
            "<test>", "test", a);
        ASSERT_TRUE(IS_LIST(r), "slice inline list returns list");
        if (IS_LIST(r))
            ASSERT_EQ_INT(AS_LIST(r)->count, 3, "slice([...],1,4) has 3 elements");
        nc_map_free(a);
    }

    SUBSECTION("slice with 2 args (start only)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set vals to [10, 20, 30, 40, 50]\n"
            "    respond with slice(vals, 3)",
            "<test>", "test", a);
        ASSERT_TRUE(IS_LIST(r), "slice(vals,3) returns list");
        if (IS_LIST(r))
            ASSERT_EQ_INT(AS_LIST(r)->count, 2, "slice(vals,3) has 2 elements (40,50)");
        nc_map_free(a);
    }

    SUBSECTION("slice with negative indices");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set vals to [10, 20, 30, 40, 50]\n"
            "    respond with len(slice(vals, -3))",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 3.0, 0.01, "slice(vals,-3) returns last 3 elements");
        nc_map_free(a);
    }

    SUBSECTION("slice with float indices (from arithmetic)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set vals to [10, 20, 30, 40, 50]\n"
            "    set s to 5.0 - 3.0\n"
            "    respond with len(slice(vals, s))",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 3.0, 0.01, "slice with float index from arithmetic works");
        nc_map_free(a);
    }

    SUBSECTION("slice used in RSI pattern");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set prices to [100, 102, 99, 97, 101, 103, 98]\n"
            "    set recent to slice(prices, 3)\n"
            "    respond with len(recent)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 4.0, 0.01, "slice(prices,3) gives last 4 prices");
        nc_map_free(a);
    }

    /* ── Stack limits ── */
    SECTION("Stack Limits");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to test:\n"
            "    set items to []\n"
            "    repeat for each i in range(500):\n"
            "        set items to items + [i]\n"
            "    respond with len(items)",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 500.0, 0.01, "build list of 500 items without stack overflow");
        nc_map_free(a);
    }

    /* ── Sequential run calls (stack reset) ── */
    SECTION("Sequential Run Calls (stack reset)");
    {
        NcMap *a = nc_map_new();
        NcValue r = nc_call_behavior(
            "to helper with x:\n"
            "    respond with x * 2\n"
            "\n"
            "to test:\n"
            "    set total to 0\n"
            "    repeat for each i in range(12):\n"
            "        run helper with i\n"
            "        set total to total + result\n"
            "    respond with total",
            "<test>", "test", a);
        double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
        ASSERT_NEAR(v, 132.0, 0.01,
            "12 sequential run calls: sum of 0*2+1*2+...+11*2 = 132");
        nc_map_free(a);
    }

    /* ── Multiple nc_call_behavior calls (leak test) ── */
    SUBSECTION("multiple nc_call_behavior calls (no leak)");
    {
        double total = 0;
        for (int call = 0; call < 20; call++) {
            NcMap *a = nc_map_new();
            nc_map_set(a, nc_string_from_cstr("x"), NC_INT(call));
            NcValue r = nc_call_behavior(
                "to test with x:\n"
                "    respond with x * 3",
                "<test>", "test", a);
            double v = IS_INT(r) ? (double)AS_INT(r) : AS_FLOAT(r);
            total += v;
            nc_map_free(a);
        }
        ASSERT_NEAR(total, 570.0, 0.01,
            "20 sequential nc_call_behavior calls: sum=570 (no stack leak)");
    }

    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    if (tests_failed == 0) {
        printf("  ║  ALL %d TESTS PASSED                           ║\n", tests_total);
    } else {
        printf("  ║  %d PASSED, %d FAILED (of %d)                   ║\n",
               tests_passed, tests_failed, tests_total);
    }
    printf("  ╚══════════════════════════════════════════════════╝\n\n");

    nc_gc_shutdown();
    return tests_failed > 0 ? 1 : 0;
}
