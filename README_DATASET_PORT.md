# Dataset Port for brain.m

Production-ready file-based port implementation for feeding datasets to `brain.m` through the port system.

## Overview

The dataset port allows you to feed any file-based dataset to `brain.m` efficiently. It reads data in configurable chunks and packages it through the standard port system, making it transparent to the rest of the system.

## Features

- **Production-ready**: Proper error handling, resource management, and cleanup
- **Configurable chunk sizes**: Optimize for your dataset size and memory constraints
- **Loop mode**: Option to continuously loop through the dataset for training
- **Statistics tracking**: Monitor bytes processed, frames processed, and processing rates
- **Auto-save**: Automatically saves `brain.m` every 30 seconds during processing

## Usage

### Building

The dataset port is included in the main library build:

```bash
make melvin_lib
make test_dataset_port
```

### Running the Test Program

```bash
./test/test_dataset_port <dataset_file> [brain.m] [--chunk-size SIZE] [--loop]
```

**Arguments:**
- `dataset_file`: Path to your dataset file (required)
- `brain.m`: Path to brain file (default: `brain.m`)
- `--chunk-size SIZE`: Size of each read chunk in bytes (default: 4096)
- `--loop`: Loop dataset file when EOF is reached (for continuous training)

**Examples:**

```bash
# Basic usage: feed a dataset file
./test/test_dataset_port my_dataset.txt

# Use custom chunk size
./test/test_dataset_port my_dataset.txt --chunk-size 8192

# Loop through dataset continuously
./test/test_dataset_port my_dataset.txt --loop

# Specify brain file and chunk size
./test/test_dataset_port my_dataset.txt custom_brain.m --chunk-size 2048 --loop
```

## Programmatic Usage

### Basic Registration

```c
#include "melvin_ports.h"
#include "melvin_m.h"

// Open brain.m
MelvinMFile *mfile = melvin_m_open("brain.m");
MelvinPortManager *manager = melvin_port_manager_create(mfile);

// Register dataset port
MelvinPort *dataset_port = melvin_port_register_dataset_file(
    manager, 
    "my_dataset.txt",  // file path
    1,                 // port ID
    4096               // chunk size
);

// Open port
melvin_port_open(dataset_port);

// Process
while (running) {
    melvin_port_manager_process_all(manager);
}

// Cleanup
melvin_port_close(dataset_port);
melvin_port_manager_free(manager);
melvin_m_close(mfile);
```

### With Loop Mode

```c
MelvinPort *dataset_port = melvin_port_register_dataset_file_loop(
    manager,
    "my_dataset.txt",
    1,
    4096
);
```

## Implementation Details

### Port Type

The dataset port uses `MELVIN_PORT_DATASET_FILE` (port type 5).

### Data Flow

1. Dataset port reads data in chunks from the file
2. Data is packaged into `PortFrame` structures (CAN bus format)
3. Frames are serialized and written to `brain.m` universal input
4. `melvin_m_process_input()` processes the data through the graph
5. Output is available via `melvin_m_universal_output_read()`

### Chunk Size Guidelines

- **Small datasets (< 1MB)**: Use default 4096 bytes or smaller
- **Medium datasets (1-100MB)**: 4KB-16KB chunks work well
- **Large datasets (> 100MB)**: 16KB-64KB chunks for better performance
- **Very large datasets**: 64KB-256KB chunks, monitor memory usage

### Performance Considerations

- Larger chunk sizes reduce function call overhead but use more memory
- The port system buffers one chunk at a time
- Processing rate depends on graph complexity and dataset characteristics
- Auto-save occurs every 30 seconds (configurable in test program)

## Files

- `melvin_port_dataset.c`: Dataset port implementation
- `test/test_dataset_port.c`: Production-ready test program
- Header declarations in `melvin_ports.h`

## Error Handling

The implementation includes:
- File existence and accessibility checks
- Proper resource cleanup on errors
- Graceful handling of EOF conditions
- Error reporting via errno

## Statistics

The test program displays:
- Graph state (nodes, edges, adaptations)
- Port activity (frames/bytes processed)
- Overall statistics (total processed, processing rate)
- Last output preview

Press Ctrl-C to stop processing gracefully (saves state before exit).

