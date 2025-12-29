#!/bin/bash
# Quick performance test - runs Test 1 only
echo "=== Quick Performance Test ==="
echo "Testing with test_dataset_1gb.txt"
echo ""
./test/test_production test_dataset_1gb.txt 1 2>&1 | tee /tmp/performance_test.log
echo ""
echo "Results saved to /tmp/performance_test.log"
