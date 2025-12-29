#!/bin/bash
# Simple watch script that shows test output
while true; do
    clear
    echo "=== Dataset Test Monitor (updates every second) ==="
    echo "Time: $(date '+%H:%M:%S')"
    echo ""
    
    # Brain file info
    BRAIN="/Volumes/512GB/Melvin/brain.m"
    [ ! -f "$BRAIN" ] && BRAIN="brain.m"
    
    if [ -f "$BRAIN" ]; then
        echo "Brain: $(ls -lh "$BRAIN" | awk '{print $5, $9}')"
        echo "Modified: $(stat -f%Sm "$BRAIN" 2>/dev/null || stat -c%y "$BRAIN" 2>/dev/null | cut -d'.' -f1)"
    fi
    
    echo ""
    echo "--- Test Process ---"
    ps aux | grep "test_dataset_port" | grep -v grep | head -1 | awk '{print "PID:", $2, "CPU:", $3"%", "MEM:", $4"%"}'
    
    echo ""
    echo "--- Recent Activity (last 10 lines) ---"
    tail -10 /tmp/dataset_test.log 2>/dev/null || echo "No log file yet"
    
    echo ""
    echo "Press Ctrl-C to stop"
    sleep 1
done
