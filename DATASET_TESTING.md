# Dataset Testing with SD Card Storage

## Quick Start

Run dataset testing with automatic SD card storage:

```bash
./run_dataset_test.sh [dataset_file] [brain_file] [chunk_size] [--loop|--no-loop]
```

**Default values:**
- Dataset: `test_dataset_1gb.txt` (1GB test file)
- Brain file: `/Volumes/512GB/Melvin/brain.m` (SD card) or `brain.m` (local if SD not mounted)
- Chunk size: 4096 bytes
- Loop mode: Enabled (continuous processing)

## Examples

```bash
# Basic usage with defaults
./run_dataset_test.sh

# Use a different dataset
./run_dataset_test.sh my_dataset.txt

# Custom chunk size (16KB for better performance on large files)
./run_dataset_test.sh test_dataset_1gb.txt "" 16384

# No looping (process once and stop)
./run_dataset_test.sh test_dataset_1gb.txt "" 4096 --no-loop

# Use local brain.m instead of SD card
./run_dataset_test.sh test_dataset_1gb.txt brain.m
```

## What Happens

1. **Dataset Processing**: Reads dataset file in chunks and feeds to brain.m
2. **Wave Propagation**: Each chunk triggers wave propagation through the graph
3. **Learning**: System learns patterns, creates nodes/edges, builds hierarchy
4. **Output Generation**: Outputs written to `{dataset_file}.output`
5. **Auto-Save**: brain.m automatically saves every 30 seconds
6. **Statistics**: Real-time display of:
   - Graph state (nodes, edges, adaptations)
   - Processing rate (frames/sec, KB/sec)
   - Port activity
   - Output preview

## Monitoring

The test program displays live statistics:
- **Brain State**: Nodes, edges, adaptation count
- **Port Activity**: Frames/bytes processed per port
- **Processing Rate**: Frames per second, KB per second
- **Last Output**: Preview of generated output

Press **Ctrl-C** to stop gracefully (saves state before exit).

## SD Card Setup

The script automatically detects SD card at `/Volumes/512GB/Melvin/`:
- If mounted: Uses `/Volumes/512GB/Melvin/brain.m`
- If not mounted: Falls back to local `brain.m`

## Output Files

- **brain.m**: The learning brain (stored on SD card)
- **{dataset_file}.output**: Generated outputs from processing

## Performance Tips

- **Small datasets (< 1MB)**: Use default 4KB chunks
- **Medium datasets (1-100MB)**: 4KB-16KB chunks
- **Large datasets (> 100MB)**: 16KB-64KB chunks
- **Very large datasets**: 64KB-256KB chunks

Larger chunks = better performance but more memory usage.

## Stopping

Press **Ctrl-C** to stop. The program will:
1. Finish current chunk processing
2. Save brain.m state
3. Display final statistics
4. Clean up resources

## Troubleshooting

**SD card not found:**
- Check if SD card is mounted: `ls /Volumes/512GB`
- Script will automatically use local brain.m

**Out of memory:**
- Reduce chunk size (e.g., 2048 bytes)
- Process smaller datasets first

**Slow processing:**
- Increase chunk size (e.g., 16384 bytes)
- Check SD card performance

