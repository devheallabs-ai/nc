/*
 * k6 Load Test for NC Service
 *
 * Install: brew install k6  (or download from https://k6.io)
 *
 * Run:
 *   k6 run tests/load/k6-load-test.js
 *   k6 run --vus 50 --duration 5m tests/load/k6-load-test.js
 *
 * With environment:
 *   NC_BASE_URL=http://localhost:8080 k6 run tests/load/k6-load-test.js
 */

import http from 'k6/http';
import { check, sleep, group } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const errorRate = new Rate('errors');
const latencyP95 = new Trend('latency_p95');

const BASE_URL = __ENV.NC_BASE_URL || 'http://localhost:8080';
const AUTH_TOKEN = __ENV.NC_AUTH_TOKEN || '';

export const options = {
  stages: [
    // Ramp-up
    { duration: '30s', target: 10 },
    { duration: '1m', target: 50 },
    // Sustained load
    { duration: '3m', target: 50 },
    // Spike test
    { duration: '30s', target: 100 },
    { duration: '1m', target: 100 },
    // Ramp-down
    { duration: '30s', target: 0 },
  ],

  thresholds: {
    http_req_duration: ['p(95)<2000', 'p(99)<5000'],
    http_req_failed: ['rate<0.01'],
    errors: ['rate<0.05'],
  },
};

function getHeaders() {
  const headers = { 'Content-Type': 'application/json' };
  if (AUTH_TOKEN) {
    headers['Authorization'] = `Bearer ${AUTH_TOKEN}`;
  }
  return headers;
}

export default function () {
  group('Health Check', function () {
    const res = http.get(`${BASE_URL}/health`);
    check(res, {
      'health returns 200': (r) => r.status === 200,
      'health response has status': (r) => {
        try { return JSON.parse(r.body).status === 'healthy'; }
        catch { return false; }
      },
    });
    errorRate.add(res.status !== 200);
    latencyP95.add(res.timings.duration);
  });

  group('API Discovery', function () {
    const res = http.get(`${BASE_URL}/`, { headers: getHeaders() });
    check(res, {
      'root returns 200': (r) => r.status === 200,
      'root has routes': (r) => {
        try { return JSON.parse(r.body).routes !== undefined; }
        catch { return false; }
      },
    });
    errorRate.add(res.status !== 200);
  });

  group('Metrics Endpoint', function () {
    const res = http.get(`${BASE_URL}/metrics`);
    check(res, {
      'metrics returns 200': (r) => r.status === 200,
      'metrics has request count': (r) => r.body.includes('nc_requests_total'),
    });
  });

  sleep(0.5 + Math.random() * 1.5);
}

export function handleSummary(data) {
  const summary = {
    timestamp: new Date().toISOString(),
    vus_max: data.metrics.vus_max ? data.metrics.vus_max.values.max : 0,
    requests_total: data.metrics.http_reqs ? data.metrics.http_reqs.values.count : 0,
    rps: data.metrics.http_reqs ? data.metrics.http_reqs.values.rate : 0,
    latency_avg: data.metrics.http_req_duration ? data.metrics.http_req_duration.values.avg : 0,
    latency_p95: data.metrics.http_req_duration ? data.metrics.http_req_duration.values['p(95)'] : 0,
    latency_p99: data.metrics.http_req_duration ? data.metrics.http_req_duration.values['p(99)'] : 0,
    error_rate: data.metrics.http_req_failed ? data.metrics.http_req_failed.values.rate : 0,
  };

  return {
    'tests/load/results.json': JSON.stringify(summary, null, 2),
    stdout: `
╔═══════════════════════════════════════════════════════════╗
║  NC Load Test Results                                      ║
╠═══════════════════════════════════════════════════════════╣
║  Total Requests: ${String(summary.requests_total).padEnd(40)}║
║  RPS:            ${String(summary.rps.toFixed(1)).padEnd(40)}║
║  Avg Latency:    ${String(summary.latency_avg.toFixed(1) + 'ms').padEnd(40)}║
║  P95 Latency:    ${String(summary.latency_p95.toFixed(1) + 'ms').padEnd(40)}║
║  P99 Latency:    ${String(summary.latency_p99.toFixed(1) + 'ms').padEnd(40)}║
║  Error Rate:     ${String((summary.error_rate * 100).toFixed(2) + '%').padEnd(40)}║
╚═══════════════════════════════════════════════════════════╝
`,
  };
}
