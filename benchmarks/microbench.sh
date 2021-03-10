#!/usr/bin/env bash
# Plinth Stream-Relational Engine Microbenchmark Suite
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=========================================================================="
echo "  PLINTH PERFORMANCE MICROBENCHMARK SUITE"
echo "  Frege Pure Functional Stream Engine on JVM | Castagnoli CRC-32C | SIMD"
echo "=========================================================================="

echo ""
echo "==> Running in-memory differential engine benchmark (10,000 stream events)..."
java -Xss8m -Xmx2g -cp "lib/frege.jar:build/classes/java:build/classes/frege" org.plinth.Main bench

echo ""
echo "==> Running Native C SIMD Vector Benchmark..."
if [ -f c_src/test_client ]; then
    ./c_src/test_client
else
    make -C c_src test
fi

echo ""
echo "=========================================================================="
echo "  MICROBENCHMARK SUITE COMPLETED SUCCESSFULLY"
echo "=========================================================================="
