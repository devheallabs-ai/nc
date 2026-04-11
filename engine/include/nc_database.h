/*
 * nc_database.h — Generic data backend for NC.
 *
 * NC programs use plain English:
 *   gather users from "https://api.example.com/users"
 *   store result into "my_collection"
 *
 * The engine resolves backends generically based on:
 *   1. URL strings (http://, https://) → HTTP GET/POST
 *   2. Named backends from configure block → user-defined endpoints
 *   3. NC_STORE_URL environment variable → default store
 *
 * No tool-specific code. No hardcoded vendor names.
 */

#ifndef NC_DATABASE_H
#define NC_DATABASE_H

#include "nc_value.h"

/* Generic query: route based on source string + options map */
NcValue nc_database_query(const char *source, NcMap *options);

/* Generic store: persist a value to the configured store backend */
NcValue nc_store_put(const char *target, const char *value_json);

/* Generic retrieve: load a value from the configured store backend */
NcValue nc_store_get(const char *key);

/* ═══════════════════════════════════════════════════════════
 *  Connection Pool — generic pool with max connections,
 *  timeout, and health tracking. Thread-safe via mutex.
 * ═══════════════════════════════════════════════════════════ */

#include "nc_platform.h"

typedef enum {
    NC_DB_POOL_SQL,         /* HTTP-based SQL proxy */
    NC_DB_POOL_REDIS,       /* RESP protocol over TCP */
    NC_DB_POOL_MONGO,       /* MongoDB Atlas Data API (REST) */
    NC_DB_POOL_GENERIC      /* Generic HTTP store */
} NcDBPoolType;

typedef struct {
    char         url[512];
    int          port;
    int          max_connections;
    int          active;
    int          idle;
    int          timeout_sec;
    NcDBPoolType type;
    nc_mutex_t   mutex;
} NcDBPool;

NcDBPool *nc_db_pool_new(const char *url, int max_conn);
void      nc_db_pool_free(NcDBPool *pool);

/* ═══════════════════════════════════════════════════════════
 *  SQL Adapter — sends SQL as JSON to an HTTP proxy endpoint
 *
 *  NC cannot link libpq or libmysql. Instead, a lightweight
 *  HTTP proxy (e.g., PostgREST, Hasura, or user's own)
 *  accepts JSON-wrapped SQL and returns JSON results.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_db_sql_query(NcDBPool *pool, const char *sql,
                        NcValue *params, int param_count);
NcValue nc_db_sql_exec(NcDBPool *pool, const char *sql);

/* ═══════════════════════════════════════════════════════════
 *  Redis Adapter — RESP protocol over raw TCP sockets
 *
 *  Implements the Redis Serialization Protocol directly:
 *    *2\r\n$3\r\nGET\r\n$3\r\nkey\r\n
 *
 *  Zero dependencies. No hiredis.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_db_redis_command(NcDBPool *pool, const char *cmd, ...);
NcValue nc_db_redis_set(NcDBPool *pool, const char *key, const char *value);
NcValue nc_db_redis_get(NcDBPool *pool, const char *key);

/* ═══════════════════════════════════════════════════════════
 *  MongoDB Adapter — MongoDB Atlas Data API (REST)
 *
 *  Uses the official Atlas Data API which is pure HTTP/JSON.
 *  No libmongoc. No BSON. Just REST.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_db_mongo_find(NcDBPool *pool, const char *collection,
                         NcMap *filter);
NcValue nc_db_mongo_insert(NcDBPool *pool, const char *collection,
                           NcMap *doc);

/* ═══════════════════════════════════════════════════════════
 *  Query Builder — type-safe SQL query construction
 *
 *  Builds queries as NcValue maps that can be serialized
 *  to SQL strings for the SQL adapter.
 * ═══════════════════════════════════════════════════════════ */

NcValue nc_db_select(const char *table, const char **columns, int col_count);
NcValue nc_db_where(NcValue query, const char *condition);
NcValue nc_db_insert_into(const char *table, NcMap *data);
char   *nc_db_query_to_sql(NcValue query);

/* ═══════════════════════════════════════════════════════════
 *  Migration Support — versioned schema migrations
 *
 *  Reads .sql files from a migrations directory, tracks
 *  applied versions, and runs them in order.
 * ═══════════════════════════════════════════════════════════ */

int nc_db_migrate_up(NcDBPool *pool, const char *migrations_dir);
int nc_db_migrate_down(NcDBPool *pool, const char *migrations_dir);

/* ═══════════════════════════════════════════════════════════
 *  File-backed persistent store — local JSON store
 *
 *  URL scheme: "file://./data.json" or "file:///abs/path.json"
 *
 *  Set NC_STORE_URL=file://./mydata.json to use it.
 *  No dependencies. Atomic writes. Thread-safe.
 * ═══════════════════════════════════════════════════════════ */

bool    nc_is_filedb_url(const char *url);
NcValue nc_filedb_put(const char *url, const char *key, const char *value_json);
NcValue nc_filedb_get(const char *url, const char *key);
NcValue nc_filedb_delete(const char *url, const char *key);
NcValue nc_filedb_list(const char *url);
NcValue nc_filedb_all(const char *url);

#endif /* NC_DATABASE_H */
