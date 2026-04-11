/*
 * nc_middleware.h — Authentication, rate limiting, request logging,
 *                    CORS, and security middleware.
 */

#ifndef NC_MIDDLEWARE_H
#define NC_MIDDLEWARE_H

#include "nc_value.h"

typedef struct {
    bool   authenticated;
    char   user_id[128];
    char   tenant_id[64];
    char   role[32];
} NcAuthContext;

NcAuthContext nc_mw_auth_check(const char *auth_header);
void nc_middleware_setup(NcMap *config);
void nc_mw_log_request(const char *method, const char *path,
                        int status, double duration_ms);
bool nc_mw_rate_limit_check(const char *client_ip);
bool nc_mw_rate_limit_check_user(const char *client_ip, const char *user_id,
                                  const char *role);
void nc_mw_rate_limit_get_headers(const char *key, char *headers, int max_len);
void nc_mw_rate_limit_set_role(const char *role, int limit);
void nc_mw_cors_apply(char *response_headers, int max_len);

/* JWT generation — create signed HS256 tokens from NC code */
NcValue nc_jwt_generate(const char *user_id, const char *role,
                         int expires_in_seconds, NcMap *extra_claims);

#endif /* NC_MIDDLEWARE_H */
