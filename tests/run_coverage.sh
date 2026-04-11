#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Running NC coverage suite..."
echo "Minimum line coverage: ${NC_COVERAGE_MIN:-100}%"

cd "$PROJECT_ROOT/nc"
make coverage
