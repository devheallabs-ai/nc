/* Platform/system info */
NcValue nc_stdlib_platform_system(void);
NcValue nc_stdlib_platform_architecture(void);

NcValue nc_stdlib_platform_info(void);
NcValue nc_stdlib_platform_hostname(void);
NcValue nc_stdlib_platform_user(void);
NcValue nc_stdlib_platform_home_dir(void);
NcValue nc_stdlib_platform_temp_dir(void);
/*
 * nc_stdlib.h — Standard library functions for NC.
 *
 * String ops, math, file I/O, time, list ops, caching.
 * These are registered as built-in functions in the VM.
 */

#ifndef NC_STDLIB_H
#define NC_STDLIB_H

#include "nc_value.h"

/* String operations */
NcValue nc_stdlib_upper(NcString *s);
NcValue nc_stdlib_lower(NcString *s);
NcValue nc_stdlib_trim(NcString *s);
NcValue nc_stdlib_contains(NcString *haystack, NcString *needle);
NcValue nc_stdlib_starts_with(NcString *s, NcString *prefix);
NcValue nc_stdlib_ends_with(NcString *s, NcString *suffix);
NcValue nc_stdlib_replace(NcString *s, NcString *old, NcString *new_str);
NcValue nc_stdlib_split(NcString *s, NcString *delimiter);
NcValue nc_stdlib_join(NcList *list, NcString *separator);

/* Math */
NcValue nc_stdlib_abs(double x);
NcValue nc_stdlib_sqrt(double x);
NcValue nc_stdlib_ceil(double x);
NcValue nc_stdlib_floor(double x);
NcValue nc_stdlib_round(double x);
NcValue nc_stdlib_pow(double base, double exp);
NcValue nc_stdlib_min(double a, double b);
NcValue nc_stdlib_max(double a, double b);
NcValue nc_stdlib_random(void);

/* List operations */
NcValue nc_stdlib_list_append(NcList *list, NcValue val);
NcValue nc_stdlib_list_length(NcList *list);
NcValue nc_stdlib_list_reverse(NcList *list);
NcValue nc_stdlib_list_sort(NcList *list);
NcValue nc_stdlib_list_sort_by(NcList *list, NcString *field);
NcValue nc_stdlib_list_max_by(NcList *list, NcString *field);
NcValue nc_stdlib_list_min_by(NcList *list, NcString *field);
NcValue nc_stdlib_list_filter_by(NcList *list, NcString *field, const char *op, double threshold);
NcValue nc_stdlib_list_filter_by_str(NcList *list, NcString *field, const char *op, NcString *threshold);
NcValue nc_stdlib_list_sum_by(NcList *list, NcString *field);
NcValue nc_stdlib_list_map_field(NcList *list, NcString *field);

/* File I/O */
NcValue nc_stdlib_read_file(const char *path);
NcValue nc_stdlib_write_file(const char *path, const char *content);
NcValue nc_stdlib_file_exists(const char *path);
NcValue nc_stdlib_delete_file(const char *path);
NcValue nc_stdlib_mkdir(const char *path);

/* Time */
NcValue nc_stdlib_time_now(void);
NcValue nc_stdlib_time_ms(void);
NcValue nc_stdlib_time_format(double timestamp, const char *fmt);
NcValue nc_stdlib_time_iso(double timestamp);

/* Environment */
NcValue nc_stdlib_env_get(const char *name);

/* Cache */
NcValue nc_stdlib_cache_set(const char *key, NcValue value);
NcValue nc_stdlib_cache_get(const char *key);
bool    nc_stdlib_cache_has(const char *key);

/* Data format parsers */
NcValue nc_stdlib_yaml_parse(const char *yaml_str);
char   *nc_stdlib_yaml_serialize(NcValue v, int indent);
NcValue nc_stdlib_xml_parse(const char *xml_str);
char   *nc_stdlib_xml_serialize(NcValue v, const char *root_tag, int indent);
NcValue nc_stdlib_csv_parse(const char *csv_str);
char   *nc_stdlib_csv_serialize(NcValue v);
NcValue nc_stdlib_toml_parse(const char *toml_str);
NcValue nc_stdlib_ini_parse(const char *ini_str);

/* RAG primitives */
NcValue nc_stdlib_chunk(NcString *text, int chunk_size, int overlap);
NcValue nc_stdlib_top_k(NcList *items, int k);
NcValue nc_stdlib_find_similar(NcList *query_vec, NcList *all_vectors,
                               NcList *documents, int top_k);
int     nc_stdlib_token_count(const char *text);

/* Python model integration */
NcValue nc_stdlib_load_model(const char *model_path);
NcValue nc_stdlib_predict(NcValue model_handle, NcList *features);
void    nc_stdlib_unload_model(int handle_id);

/* Conversation memory */
NcValue nc_stdlib_memory_new(int max_turns);
NcValue nc_stdlib_memory_add(NcValue handle, const char *role, const char *content);
NcValue nc_stdlib_memory_get(NcValue handle);
NcValue nc_stdlib_memory_clear(NcValue handle);
NcValue nc_stdlib_memory_summary(NcValue handle);
NcValue nc_stdlib_memory_save(NcValue handle, const char *path);
NcValue nc_stdlib_memory_load(const char *path, int max_turns);
NcValue nc_stdlib_memory_store(const char *path, const char *kind,
                               const char *content, NcValue metadata,
                               double reward);
NcValue nc_stdlib_memory_search(const char *path, const char *query, int top_k);
NcValue nc_stdlib_memory_context(const char *path, const char *query, int top_k);
NcValue nc_stdlib_memory_reflect(const char *path, const char *task,
                                 const char *worked, const char *failed,
                                 double confidence, const char *next_action);

/* Reward-scored policy memory */
NcValue nc_stdlib_policy_update(const char *path, const char *action, double reward);
NcValue nc_stdlib_policy_choose(const char *path, NcList *actions, double epsilon);
NcValue nc_stdlib_policy_stats(const char *path);

/* Response validation */
NcValue nc_stdlib_validate(NcValue response, NcList *required_fields);

/* AI retry with fallback models */
NcValue nc_stdlib_ai_with_fallback(const char *prompt, NcMap *context, NcList *models);

/* Generic HTTP request — direct HTTP calls from NC code */
NcValue nc_stdlib_http_request(const char *method, const char *url,
                                NcMap *headers, NcValue body);

/* Cross-language execution — call any language from NC
 *   exec("python3", "script.py")
 *   exec("node", "process.js")
 *   exec("bash", "deploy.sh")
 *   exec("go", "run", "main.go")
 *   shell("ls -la")
 */
NcValue nc_stdlib_exec(NcList *args);
NcValue nc_stdlib_shell(const char *command);
NcValue nc_stdlib_shell_exec(const char *command);

/* Atomic file write — writes to temp file then renames for crash safety */
NcValue nc_stdlib_write_file_atomic(const char *path, const char *content);

/* ── Cryptographic Hashing ────────────────────────────────
 * Generic, no vendor dependencies. Pure C implementations.
 *   hash_sha256("data")           → hex digest
 *   hash_password("secret")       → salted hash for storage
 *   verify_password("secret", h)  → true/false
 *   hash_hmac("data", "key")      → HMAC-SHA256 hex digest
 */
NcValue nc_stdlib_hash_sha256(const char *data);
NcValue nc_stdlib_hash_password(const char *password);
NcValue nc_stdlib_verify_password(const char *password, const char *stored_hash);
NcValue nc_stdlib_hash_hmac(const char *data, const char *key);

/* ── Request Header Access ────────────────────────────────
 * Access HTTP request headers inside behaviors.
 *   set token to request_header("Authorization")
 *   set client to request_header("User-Agent")
 */
NcValue nc_stdlib_request_header(const char *header_name);

/* ── Session Management ───────────────────────────────────
 * Server-side session storage. No external dependencies.
 *   set sid to session_create()
 *   session_set(sid, "user", "alice")
 *   set user to session_get(sid, "user")
 *   session_destroy(sid)
 */
NcValue nc_stdlib_session_create(void);
NcValue nc_stdlib_session_set(const char *session_id, const char *key, NcValue value);
NcValue nc_stdlib_session_get(const char *session_id, const char *key);
NcValue nc_stdlib_session_destroy(const char *session_id);
NcValue nc_stdlib_session_exists(const char *session_id);

/* ── Streaming Response ───────────────────────────────────
 * Send chunked/SSE responses from behaviors.
 *   stream_start()
 *   stream_send("partial data")
 *   stream_end()
 */
NcValue nc_stdlib_stream_start(void);
NcValue nc_stdlib_stream_send(const char *data);
NcValue nc_stdlib_stream_end(void);

/* ── Connection Pool ──────────────────────────────────────
 * Persistent HTTP connections for high-throughput scenarios.
 * Managed automatically — users just call http_request().
 */
void nc_connpool_init(void);
void nc_connpool_cleanup(void);

/* ── Tensor operations for NC AI ─────────────────────────────
 * These bridge NC lists ↔ C tensors for fast math.
 * Matrices are represented as list-of-lists in NC.
 *
 *   set w to tensor_random(128, 256)
 *   set result to tensor_matmul(x, w)
 *   set activated to tensor_gelu(result)
 *   set normed to tensor_layer_norm(activated, gamma, beta)
 */
NcValue nc_ncfn_tensor_create(int rows, int cols);
NcValue nc_ncfn_tensor_ones(int rows, int cols);
NcValue nc_ncfn_tensor_random(int rows, int cols);
NcValue nc_ncfn_tensor_matmul(NcValue va, NcValue vb);
NcValue nc_ncfn_tensor_add(NcValue va, NcValue vb);
NcValue nc_ncfn_tensor_sub(NcValue va, NcValue vb);
NcValue nc_ncfn_tensor_mul(NcValue va, NcValue vb);
NcValue nc_ncfn_tensor_scale(NcValue va, double scalar);
NcValue nc_ncfn_tensor_transpose(NcValue va);
NcValue nc_ncfn_tensor_softmax(NcValue va);
NcValue nc_ncfn_tensor_gelu(NcValue va);
NcValue nc_ncfn_tensor_relu(NcValue va);
NcValue nc_ncfn_tensor_tanh(NcValue va);
NcValue nc_ncfn_tensor_layer_norm(NcValue va, NcValue vg, NcValue vb);
NcValue nc_ncfn_tensor_add_bias(NcValue va, NcValue vb);
NcValue nc_ncfn_tensor_causal_mask(int seq_len);
NcValue nc_ncfn_tensor_embedding(NcValue vtable, NcValue vids);
NcValue nc_ncfn_tensor_shape(NcValue va);
NcValue nc_ncfn_tensor_cross_entropy(NcValue vlogits, NcValue vtargets);
NcValue nc_ncfn_tensor_save(NcValue va, const char *path);
NcValue nc_ncfn_tensor_load(const char *path);

/* ── Regex module ─────────────────────────────────────────
 * Custom regex engine: . * + ? ^ $ \d \w \s [...] | ()
 */
NcValue nc_stdlib_re_match(const char *pattern, const char *string);
NcValue nc_stdlib_re_search(const char *pattern, const char *string);
NcValue nc_stdlib_re_findall(const char *pattern, const char *string);
NcValue nc_stdlib_re_replace(const char *pattern, const char *replacement, const char *string);
NcValue nc_stdlib_re_split(const char *pattern, const char *string);

/* ── CSV module (enhanced) ────────────────────────────────
 * Handles quoted fields, escaped quotes, custom delimiter
 */
NcValue nc_stdlib_csv_parse_delim(const char *text, char delimiter);
NcValue nc_stdlib_csv_stringify(NcValue data, char delimiter);

/* ── OS module ────────────────────────────────────────────
 * File system, env, process execution
 */
NcValue nc_stdlib_os_env(const char *name);
NcValue nc_stdlib_os_cwd(void);
NcValue nc_stdlib_os_listdir(const char *path);
NcValue nc_stdlib_os_exists(const char *path);
NcValue nc_stdlib_os_mkdir_p(const char *path);
NcValue nc_stdlib_os_remove(const char *path);
NcValue nc_stdlib_os_read_file(const char *path);
NcValue nc_stdlib_os_write_file(const char *path, const char *content);
NcValue nc_stdlib_os_exec(const char *command);
NcValue nc_stdlib_os_glob(const char *dir, const char *pattern);
NcValue nc_stdlib_os_walk(const char *dir);
NcValue nc_stdlib_os_is_dir(const char *path);
NcValue nc_stdlib_os_is_file(const char *path);
NcValue nc_stdlib_os_file_size(const char *path);
NcValue nc_stdlib_os_path_join(NcList *parts);
NcValue nc_stdlib_os_basename(const char *path);
NcValue nc_stdlib_os_dirname(const char *path);
NcValue nc_stdlib_os_setenv(const char *name, const char *value);

/* ── DateTime module ──────────────────────────────────────
 * ISO 8601 timestamps, parsing, formatting, arithmetic
 */
NcValue nc_stdlib_datetime_now(void);
NcValue nc_stdlib_datetime_parse(const char *str, const char *format);
NcValue nc_stdlib_datetime_format(double timestamp, const char *format);
NcValue nc_stdlib_datetime_diff(double a, double b);
NcValue nc_stdlib_datetime_add(double timestamp, double seconds);

/* ── Base64 module ────────────────────────────────────────
 * Encode/decode base64 strings
 */
NcValue nc_stdlib_base64_encode(const char *data);
NcValue nc_stdlib_base64_decode(const char *data);

/* ── Hashlib module ───────────────────────────────────────
 * MD5 and SHA-256 (pure C, no OpenSSL)
 */
NcValue nc_stdlib_hash_md5(const char *data);

/* ── UUID module ──────────────────────────────────────────
 * UUID v4 (random)
 */
NcValue nc_stdlib_uuid_v4(void);

#endif /* NC_STDLIB_H */
