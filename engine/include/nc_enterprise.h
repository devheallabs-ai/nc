/*
 * nc_enterprise.h — Enterprise features: sandbox, audit, OIDC, feature flags,
 *                    circuit breaker, secrets management, multi-tenancy.
 */

#ifndef NC_ENTERPRISE_H
#define NC_ENTERPRISE_H

#include <stdbool.h>
#include <time.h>

/* ── Sandbox ──────────────────────────────────────────────── */

bool nc_sandbox_check(const char *operation);
bool nc_auth_validate(const char *key);

/* ── Audit Logging (SOC 2 / HIPAA compliant) ─────────────── */

typedef enum {
    NC_AUDIT_AUTH_SUCCESS,
    NC_AUDIT_AUTH_FAILURE,
    NC_AUDIT_ACCESS_GRANTED,
    NC_AUDIT_ACCESS_DENIED,
    NC_AUDIT_DATA_READ,
    NC_AUDIT_DATA_WRITE,
    NC_AUDIT_DATA_DELETE,
    NC_AUDIT_CONFIG_CHANGE,
    NC_AUDIT_ADMIN_ACTION,
    NC_AUDIT_RATE_LIMITED,
    NC_AUDIT_CUSTOM,
} NcAuditEventType;

typedef struct {
    time_t           timestamp;
    NcAuditEventType event_type;
    char             user[128];
    char             action[128];
    char             target[256];
    char             result[64];
    char             ip[48];
    char             tenant_id[64];
    char             trace_id[64];
    char             session_id[64];
    char             user_agent[256];
    int              status_code;
    double           duration_ms;
} NcAuditEntry;

void nc_audit_log(const char *user, const char *action,
                  const char *target, const char *result);
void nc_audit_log_ext(NcAuditEntry *entry);
void nc_audit_print(int last_n);
void nc_audit_emit_json(NcAuditEntry *entry);
int  nc_audit_export(const char *filepath);
int  nc_conformance_run(void);

/* ── OIDC / SSO ───────────────────────────────────────────── */

typedef struct {
    char issuer[256];
    char authorization_endpoint[512];
    char token_endpoint[512];
    char userinfo_endpoint[512];
    char jwks_uri[512];
    char client_id[256];
    char client_secret[256];
    char redirect_uri[512];
    char scopes[256];
    bool enabled;
    bool require_https;
    int  token_expiry_leeway;
} NcOIDCConfig;

void nc_oidc_init(void);
NcOIDCConfig *nc_oidc_get_config(void);
bool nc_oidc_validate_token(const char *token, char *user_out, int user_max,
                             char *role_out, int role_max);

/* ── Circuit Breaker ──────────────────────────────────────── */

typedef enum {
    NC_CB_CLOSED,
    NC_CB_OPEN,
    NC_CB_HALF_OPEN,
} NcCircuitState;

typedef struct {
    char           name[64];
    NcCircuitState state;
    int            failure_count;
    int            success_count;
    int            failure_threshold;
    int            success_threshold;
    int            timeout_seconds;
    time_t         last_failure_time;
    time_t         opened_at;
} NcCircuitBreaker;

NcCircuitBreaker *nc_cb_get(const char *name);
bool              nc_cb_allow(const char *name);
void              nc_cb_record_success(const char *name);
void              nc_cb_record_failure(const char *name);

/* ── Feature Flags ────────────────────────────────────────── */

typedef struct {
    char name[64];
    bool enabled;
    int  rollout_pct;
    char allowed_tenants[16][64];
    int  allowed_tenant_count;
} NcFeatureFlag;

void nc_ff_init(void);
bool nc_ff_is_enabled(const char *flag_name, const char *tenant_id);
void nc_ff_set(const char *flag_name, bool enabled, int rollout_pct);

/* ── Secrets Management ───────────────────────────────────── */

typedef enum {
    NC_SECRET_ENV,
    NC_SECRET_FILE,
    NC_SECRET_VAULT,  /* generic external store */
} NcSecretSource;

const char *nc_secret_get(const char *key);
void        nc_secret_init(void);

/* ── Multi-tenancy (enhanced) ─────────────────────────────── */

int  nc_tenant_create(const char *id, int memory_limit_mb);
void nc_tenant_destroy(const char *id);

/* ── API Versioning ───────────────────────────────────────── */

typedef struct {
    char version[16];
    char base_path[64];
    bool deprecated;
    char sunset_date[32];
} NcApiVersion;

void nc_api_version_init(void);
const char *nc_api_version_resolve(const char *path, char *resolved, int max_len);

#endif /* NC_ENTERPRISE_H */
