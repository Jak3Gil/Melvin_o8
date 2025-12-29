#!/bin/bash
# Live monitor that shows both test output and brain file stats

BRAIN_FILE="/Volumes/512GB/Melvin/brain.m"
LOG_FILE="/tmp/dataset_test.log"

# Fallback to local if SD card file doesn't exist
if [ ! -f "$BRAIN_FILE" ]; then
    BRAIN_FILE="brain.m"
fi

echo "=== Live Dataset Test Monitor ==="
echo "Brain file: $BRAIN_FILE"
echo "Log file: $LOG_FILE"
echo "Updating every second..."
echo "Press Ctrl-C to stop"
echo ""

while true; do
    clear
    echo "=== Live Dataset Test Monitor ==="
    echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    
    # Show brain file stats
    echo "--- Brain File Stats ---"
    if [ -f "$BRAIN_FILE" ]; then
        SIZE=$(ls -lh "$BRAIN_FILE" | awk '{print $5}')
        SIZE_BYTES=$(stat -f%z "$BRAIN_FILE" 2>/dev/null || stat -c%s "$BRAIN_FILE" 2>/dev/null)
        MOD_TIME=$(stat -f%Sm "$BRAIN_FILE" 2>/dev/null || stat -c%y "$BRAIN_FILE" 2>/dev/null | cut -d'.' -f1)
        
        echo "File: $BRAIN_FILE"
        echo "Size: $SIZE ($SIZE_BYTES bytes)"
        echo "Last modified: $MOD_TIME"
        
        # Growth tracking
        if [ -f "/tmp/brain_size_prev" ]; then
            PREV_SIZE=$(cat /tmp/brain_size_prev)
            if [ -n "$PREV_SIZE" ] && [ "$PREV_SIZE" != "0" ]; then
                GROWTH=$((SIZE_BYTES - PREV_SIZE))
                if [ $GROWTH -gt 0 ]; then
                    echo "Growth: +$GROWTH bytes/sec"
                fi
            fi
        fi
        echo "$SIZE_BYTES" > /tmp/brain_size_prev
        
        # SD card space
        if [[ "$BRAIN_FILE" == *"/Volumes/"* ]]; then
            SD_PATH=$(dirname "$BRAIN_FILE")
            if [ -d "$SD_PATH" ]; then
                AVAIL=$(df -h "$SD_PATH" | tail -1 | awk '{print $4}')
                USED=$(df -h "$SD_PATH" | tail -1 | awk '{print $3}')
                echo "SD Card: $USED used, $AVAIL free"
            fi
        fi
    else
        echo "Brain file not found (waiting...)"
    fi
    
    echo ""
    echo "--- Test Output (last 20 lines) ---"
    
    # Show recent test output
    if [ -f "$LOG_FILE" ]; then
        tail -20 "$LOG_FILE" 2>/dev/null | grep -v "^$" | tail -15
    else
        echo "Log file not found. Test may not be running."
        echo ""
        echo "To start test, run:"
        echo "  ./run_dataset_test.sh > $LOG_FILE 2>&1 &"
    fi
    
    echo ""
    echo "--- Process Status ---"
    if pgrep -f "test_dataset_port" > /dev/null; then
        echo "✓ Test is running (PID: $(pgrep -f 'test_dataset_port'))"
    else
        echo "✗ Test is not running"
    fi
    
    echo ""
    echo "Refreshing every second... (Ctrl-C to stop)"
    
    sleep 1
done

