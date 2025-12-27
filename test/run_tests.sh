#!/bin/bash
#
# Simple test runner script
# Runs multiple test cases on the same .m file

MELVIN_FILE="${1:-test.m}"

echo "=========================================="
echo "Melvin Test Suite"
echo "Using .m file: $MELVIN_FILE"
echo "=========================================="
echo ""

# Run test cases sequentially on the same .m file
echo "Test 1: Basic input"
./melvin_test_runner "$MELVIN_FILE" -i "hello" | tail -15
echo ""

echo "Test 2: Different input (should reuse same .m file)"
./melvin_test_runner "$MELVIN_FILE" -i "world" | tail -15
echo ""

echo "Test 3: Repeated input (should show learning)"
./melvin_test_runner "$MELVIN_FILE" -i "hello" | tail -15
echo ""

echo "Test 4: Similar pattern"
./melvin_test_runner "$MELVIN_FILE" -i "hell" | tail -15
echo ""

echo "=========================================="
echo "Final Analysis"
echo "=========================================="
./melvin_test_runner "$MELVIN_FILE" -a
