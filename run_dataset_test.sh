#!/bin/bash
# Run dataset test with SD card storage

# Default values
DATASET_FILE="${1:-test_dataset_1gb.txt}"
BRAIN_FILE="${2:-}"
CHUNK_SIZE="${3:-4096}"
LOOP="${4:---loop}"

# SD card location
SD_CARD_PATH="/Volumes/512GB/Melvin"
SD_BRAIN_FILE="$SD_CARD_PATH/brain.m"

# Check if SD card is mounted
if [ -d "$SD_CARD_PATH" ]; then
    echo "✓ SD card found at: $SD_CARD_PATH"
    
    # Create directory if it doesn't exist
    mkdir -p "$SD_CARD_PATH"
    
    # Use SD card brain file
    if [ -z "$BRAIN_FILE" ]; then
        BRAIN_FILE="$SD_BRAIN_FILE"
    fi
    
    echo "Using brain file: $BRAIN_FILE"
else
    echo "⚠ SD card not found at: $SD_CARD_PATH"
    echo "Using local brain.m instead"
    if [ -z "$BRAIN_FILE" ]; then
        BRAIN_FILE="brain.m"
    fi
fi

# Check if dataset file exists
if [ ! -f "$DATASET_FILE" ]; then
    echo "Error: Dataset file not found: $DATASET_FILE"
    exit 1
fi

# Check if test program exists
if [ ! -f "test/test_dataset_port" ]; then
    echo "Building test_dataset_port..."
    make test_dataset_port || {
        echo "Error: Failed to build test_dataset_port"
        exit 1
    }
fi

echo ""
echo "=== Dataset Test Configuration ==="
echo "Dataset file: $DATASET_FILE ($(ls -lh "$DATASET_FILE" | awk '{print $5}'))"
echo "Brain file: $BRAIN_FILE"
echo "Chunk size: $CHUNK_SIZE bytes"
echo "Loop mode: $([ "$LOOP" = "--loop" ] && echo "Yes" || echo "No")"
echo ""
echo "Starting dataset processing..."
echo "Press Ctrl-C to stop"
echo ""

# Run the test
./test/test_dataset_port "$DATASET_FILE" "$BRAIN_FILE" --chunk-size "$CHUNK_SIZE" "$LOOP"

