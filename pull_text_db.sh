#!/bin/bash
# Pull a text database for live learning

# Default text database URL (Project Gutenberg - Alice in Wonderland as example)
# Replace with your preferred text database URL
TEXT_DB_URL="${1:-https://www.gutenberg.org/files/11/11-0.txt}"

# Brain file on SD card
BRAIN_FILE="/Volumes/512GB/brain.m"

# If SD card not mounted, use local file
if [ ! -d "/Volumes/512GB" ]; then
    echo "SD card not found, using local brain.m"
    BRAIN_FILE="brain.m"
fi

echo "═══════════════════════════════════════════════════════════════"
echo "Pulling Text Database for Live Learning"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "URL: $TEXT_DB_URL"
echo "Brain file: $BRAIN_FILE"
echo ""
echo "Starting in 2 seconds..."
sleep 2

./test_live_learning "$TEXT_DB_URL" "$BRAIN_FILE" --chunk-size 65536 --loop
