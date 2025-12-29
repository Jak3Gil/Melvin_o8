#!/bin/bash
# Simple test runner for Melvin

cd /Users/jakegilbert/Desktop/Melvin_Reasearch/Melvin_o8

echo "=== Test: Initial Processing ==="
rm -f brain.m brain.m.output

# Run for 25 seconds
./test_live_learning https://www.gutenberg.org/files/11/11-0.txt brain.m --chunk-size 4096 &
TEST_PID=$!
sleep 25
kill $TEST_PID 2>/dev/null
wait $TEST_PID 2>/dev/null

echo ""
echo "=== Test: Re-processing (Edge Strengthening) ==="
if [ -f brain.m ] && [ -s brain.m ]; then
    ./test_live_learning https://www.gutenberg.org/files/11/11-0.txt brain.m --chunk-size 4096 &
    TEST_PID=$!
    sleep 25
    kill $TEST_PID 2>/dev/null
    wait $TEST_PID 2>/dev/null
else
    echo "brain.m not created, skipping re-process test"
fi

