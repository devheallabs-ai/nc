/*
 * k6 Soak Test for NC Service — detect memory leaks and degradation
 *
 * Run:
 *   k6 run tests/load/k6-soak-test.js
 *
 * Extended soak (4 hours):
 *   k6 run --duration 4h tests/load/k6-soak-test.js
 */

import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

const errorRate = new Rate('errors');
const latency = new Trend('request_latency');
const BASE_URL = __ENV.NC_BASE_URL || 'http://localhost:8080';

export const options = {
  stages: [
    { duration: '2m', target: 30 },
    { duration: '56m', target: 30 },
    { duration: '2m', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<3000'],
    http_req_failed: ['rate<0.01'],
    errors: ['rate<0.01'],
  },
};

export default function () {
  const res = http.get(`${BASE_URL}/health`);
  check(res, {
    'health ok': (r) => r.status === 200,
    'latency under 1s': (r) => r.timings.duration < 1000,
  });
  errorRate.add(res.status !== 200);
  latency.add(res.timings.duration);

  sleep(1 + Math.random() * 2);
}
