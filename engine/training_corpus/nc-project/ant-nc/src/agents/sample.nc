// ══════════════════════════════════════════════════════════════════
//  SAMPLE DATA — For demo without real monitoring
// ══════════════════════════════════════════════════════════════════

to sample_data:
    respond with {"service_name": "checkout-api", "description": "Checkout failing with HTTP 500 errors since 15:42 UTC", "logs": "2024-03-15 15:42:03 ERROR [checkout-api] FATAL: password authentication failed for user \"checkout_svc\"\n2024-03-15 15:42:03 ERROR [checkout-api] pg_connect(): Unable to connect to PostgreSQL at 10.0.2.15:5432\n2024-03-15 15:42:04 ERROR [checkout-api] ConnectionPool exhausted: 0/20 connections available\n2024-03-15 15:42:05 WARN  [checkout-api] Circuit breaker OPEN for database connections\n2024-03-15 15:42:05 ERROR [payments-api] Upstream timeout: checkout-api did not respond within 5000ms\n2024-03-15 15:42:06 ERROR [checkout-api] Request failed: POST /api/checkout - 503 Service Unavailable\n2024-03-15 15:41:55 INFO  [deploy-bot] Deployed checkout-api v2.4.1 (changes: updated db credentials config)", "metrics": "checkout-api latency_p99: 5200ms (normal: 45ms)\ncheckout-api error_rate: 94% (normal: 0.1%)\ncheckout-api cpu: 12% (normal: 8%)\npayments-api latency_p99: 5100ms (normal: 55ms)\npayments-api error_rate: 34% (normal: 0.1%)\npostgresql connections: 148/200 (normal: 45/200)\npostgresql latency: 8ms (normal: 8ms)", "errors": "FATAL: password authentication failed for user \"checkout_svc\" (x342 in last 1 min)\nConnectionPool exhausted: 0/20 connections available (x156)\nUpstream timeout from checkout-api (x89)", "deploy_history": "15:42 UTC - checkout-api v2.4.1 deployed by deploy-bot\n  Changes: Updated database credentials config, Migrated to new secrets manager\n09:15 UTC - users-api v1.8.0 deployed by ci-pipeline\n  Changes: Added rate limiting middleware"}

// ══════════════════════════════════════════════════════════════════
//  UI
// ══════════════════════════════════════════════════════════════════

to home:
    set html to read_file("public/index.html")
    respond with html

to ui:
    set html to read_file("public/index.html")
    respond with html

to health:
    respond with {"status": "healthy", "service": "swarmops", "version": "6.0.0"}

