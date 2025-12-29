#!/bin/bash
# Monitor brain.m statistics every second

BRAIN_FILE="${1:-/Volumes/512GB/Melvin/brain.m}"

# Fallback to local if SD card file doesn't exist
if [ ! -f "$BRAIN_FILE" ]; then
    BRAIN_FILE="brain.m"
fi

echo "Monitoring: $BRAIN_FILE"
echo "Press Ctrl-C to stop"
echo ""

while true; do
    clear
    echo "=== Brain.m Monitor ==="
    echo "Time: $(date '+%Y-%m-%d %H:%M:%S')"
    echo ""
    
    if [ -f "$BRAIN_FILE" ]; then
        # File size
        SIZE=$(ls -lh "$BRAIN_FILE" | awk '{print $5}')
        SIZE_BYTES=$(stat -f%z "$BRAIN_FILE" 2>/dev/null || stat -c%s "$BRAIN_FILE" 2>/dev/null)
        
        echo "File: $BRAIN_FILE"
        echo "Size: $SIZE ($SIZE_BYTES bytes)"
        echo ""
        
        # Try to get graph stats if we can (would need to query the .m file)
        # For now, just show file info and growth rate
        
        # Calculate growth (compare to previous size)
        if [ -f "/tmp/brain_size_prev" ]; then
            PREV_SIZE=$(cat /tmp/brain_size_prev)
            if [ -n "$PREV_SIZE" ] && [ "$PREV_SIZE" != "0" ]; then
                GROWTH=$((SIZE_BYTES - PREV_SIZE))
                if [ $GROWTH -gt 0 ]; then
                    echo "Growth: +$GROWTH bytes (since last check)"
                elif [ $GROWTH -lt 0 ]; then
                    echo "Growth: $GROWTH bytes (file shrunk?)"
                else
                    echo "Growth: 0 bytes (no change)"
                fi
            fi
        fi
        echo "$SIZE_BYTES" > /tmp/brain_size_prev
        
        # Last modified time
        MOD_TIME=$(stat -f%Sm "$BRAIN_FILE" 2>/dev/null || stat -c%y "$BRAIN_FILE" 2>/dev/null)
        echo "Last modified: $MOD_TIME"
        
        # Check if SD card
        if [[ "$BRAIN_FILE" == *"/Volumes/"* ]]; then
            echo "Location: SD Card"
            # Check SD card space
            SD_PATH=$(dirname "$BRAIN_FILE")
            if [ -d "$SD_PATH" ]; then
                AVAIL=$(df -h "$SD_PATH" | tail -1 | awk '{print $4}')
                echo "SD Card free space: $AVAIL"
            fi
        else
            echo "Location: Local"
        fi
    else
        echo "File not found: $BRAIN_FILE"
        echo "Waiting for file to be created..."
    fi
    
    echo ""
    echo "Updating every second... (Ctrl-C to stop)"
    
    sleep 1
done

