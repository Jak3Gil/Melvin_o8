#!/bin/bash
# Quick start script for live learning from large database

# Default values
URL="${1:-}"
BRAIN_FILE="${2:-/Volumes/512GB/brain.m}"
CHUNK_SIZE="${3:-131072}"  # 128KB chunks
LOOP="${4:---loop}"        # Loop continuously

if [ -z "$URL" ]; then
    echo "Usage: $0 <url> [brain_file] [chunk_size] [--loop|--no-loop]"
    echo ""
    echo "Examples:"
    echo "  $0 https://example.com/dataset.bin"
    echo "  $0 https://example.com/dataset.bin /Volumes/512GB/brain.m"
    echo "  $0 https://example.com/dataset.bin /Volumes/512GB/brain.m 65536 --loop"
    echo ""
    echo "Default brain file: /Volumes/512GB/brain.m"
    echo "Default chunk size: 131072 (128KB)"
    echo "Default: loops continuously"
    exit 1
fi

# Check if SD card is mounted
if [ ! -d "/Volumes/512GB" ]; then
    echo "Warning: SD card not found at /Volumes/512GB"
    echo "Using local brain.m instead"
    BRAIN_FILE="brain.m"
fi

echo "Starting live learning..."
echo "URL: $URL"
echo "Brain file: $BRAIN_FILE"
echo "Chunk size: $CHUNK_SIZE bytes"
echo ""

./test_live_learning "$URL" "$BRAIN_FILE" --chunk-size "$CHUNK_SIZE" "$LOOP"

