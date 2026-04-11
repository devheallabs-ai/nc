/*
 * k6 Stress Test for NC Service — find the breaking point
 *
 * Run:
 *   k6 run tests/load/k6-stress-test.js
 */

import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate } from 'k6/metrics';

const errorRate = new Rate('errors');
const BASE_URL = __ENV.NC_BASE_URL || 'http://localhost:8080';

export const options = {
  stages: [
    { duration: '1m', target: 50 },
    { duration: '2m', target: 100 },
    { duration: '2m', target: 200 },
    { duration: '2m', target: 500 },
    { duration: '2m', target: 1000 },
    { duration: '1m', target: 0 },
  ],
  thresholds: {
    http_req_duration: ['p(95)<5000'],
  },
};

export default function () {
  const responses = http.batch([
    ['GET', `${BASE_URL}/health`],
    ['GET', `${BASE_URL}/metrics`],
  ]);

  responses.forEach((res) => {
    check(res, { 'status is 200 or 429': (r) => r.status === 200 || r.status === 429 });
    errorRate.add(res.status >= 500);
  });

  sleep(0.1 + Math.random() * 0.5);
}
