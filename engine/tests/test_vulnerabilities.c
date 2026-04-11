#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../include/nc.h"

/* Forward declarations for functions not in headers */
void nc_mw_cors_apply(char *response_headers, int max_len);
bool nc_mw_rate_limit_check(const char *client_ip);

static int tests_passed = 0;
static int tests_failed = 0;
static int tests_total  = 0;

#define ASSERT(cond, msg) do { \
    tests_total++; \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("    FAIL: %s\n", msg); } \
} while(0)

#define ASSERT_TRUE(a, msg) ASSERT((a), msg)
#define ASSERT_FALSE(a, msg) ASSERT(!(a), msg)

void test_rate_limit_bypass_fix() {
    printf("  Testing Rate Limit Bypass Fix...\n");
    
    // We need to fill RL_MAX_CLIENTS (1024)
    // The rate limiter uses a static table internally.
    
    char ip[32];
    for (int i = 0; i < 1024; i++) {
        sprintf(ip, "192.168.0.%d", i);
        bool allowed = nc_mw_rate_limit_check(ip);
        if (!allowed) {
            printf("    FAIL: Request %d was unexpectedly blocked\n", i);
            tests_failed++;
            return;
        }
    }
    
    // Now the 1025th unique IP should be BLOCKED because the table is full and no entries are expired
    // (since we just added them all right now).
    bool blocked = nc_mw_rate_limit_check("1.1.1.1");
    ASSERT_FALSE(blocked, "1025th request blocked when table is full (Bypass Fixed)");
}

void test_cors_default_fix() {
    printf("  Testing CORS Default Fix...\n");
    
    char headers[2048];
    
    // Case 1: No env vars set
    #ifdef _WIN32
    _putenv("NC_CORS_ORIGIN=");
    _putenv("NC_API_KEY=");
    #else
    unsetenv("NC_CORS_ORIGIN");
    unsetenv("NC_API_KEY");
    #endif
    
    memset(headers, 0, sizeof(headers));
    nc_mw_cors_apply(headers, sizeof(headers));
    ASSERT_TRUE(strstr(headers, "Access-Control-Allow-Origin: null") != NULL, "CORS defaults to 'null' (Safe)");
    ASSERT_TRUE(strstr(headers, "Access-Control-Allow-Origin: *") == NULL, "CORS does NOT default to '*' (Security Bypass Fixed)");
}

int main() {
    printf("\n  NC Security Vulnerability Regression Tests\n");
    printf("  ──────────────────────────────────────────\n");
    
    test_rate_limit_bypass_fix();
    test_cors_default_fix();
    
    printf("\n  Result: %d passed, %d failed out of %d\n\n", tests_passed, tests_failed, tests_total);
    return tests_failed > 0 ? 1 : 0;
}
