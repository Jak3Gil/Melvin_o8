# HTTP Range Request Port for brain.m

Production-ready HTTP port implementation for downloading large files via HTTP Range requests without storing the entire file in memory. Perfect for continuous learning from large datasets hosted on HTTP servers.

## Overview

The HTTP Range Request port allows you to:
- Download large files in chunks via HTTP Range requests
- Process data incrementally without downloading the entire file first
- Auto-discover file size via HEAD request (if not provided)
- Loop through files continuously for training
- Feed data directly to `brain.m` for learning

## Features

- **Memory Efficient**: Only one chunk in memory at a time
- **Resumable**: Can restart from any position using range requests
- **Auto-Discovery**: Automatically discovers file size via HEAD request
- **Continuous Learning**: Supports looping through files for continuous training
- **Production-Ready**: Proper error handling, resource management, and cleanup

## Building

The HTTP port is included in the main library build. It requires `libcurl`:

```bash
# Install libcurl (if not already installed)
# macOS:
brew install curl

# Linux:
sudo apt-get install libcurl4-openssl-dev  # Debian/Ubuntu
sudo yum install libcurl-devel            # RHEL/CentOS

# Build library
make melvin_lib

# Build test program
make test_http_range
```

## Usage

### Command Line Test Program

```bash
./test/test_http_range <url> [brain.m] [--chunk-size SIZE] [--total-size SIZE] [--loop]
```

**Arguments:**
- `url`: HTTP/HTTPS URL to download (required)
- `brain.m`: Path to brain file (default: `brain.m`)
- `--chunk-size SIZE`: Size of each range request in bytes (default: 65536 = 64KB)
- `--total-size SIZE`: Total file size in bytes (0 = auto-discover via HEAD, default: 0)
- `--loop`: Loop back to start when EOF is reached (for continuous training)

**Examples:**

```bash
# Basic usage: download and process a large file
./test/test_http_range https://example.com/large-dataset.bin

# Use custom chunk size (128KB chunks)
./test/test_http_range https://example.com/large-dataset.bin --chunk-size 131072

# Specify total size (skip HEAD request)
./test/test_http_range https://example.com/large-dataset.bin --total-size 1073741824

# Loop continuously for training
./test/test_http_range https://example.com/large-dataset.bin --chunk-size 65536 --loop

# Full example with all options
./test/test_http_range https://example.com/large-dataset.bin custom_brain.m --chunk-size 131072 --total-size 1073741824 --loop
```

### Programmatic Usage

```c
#include "melvin_ports.h"
#include "melvin_m.h"

// Open brain.m
MelvinMFile *mfile = melvin_m_open("brain.m");
MelvinPortManager *manager = melvin_port_manager_create(mfile);

// Register HTTP range request port
MelvinPort *http_port = melvin_port_register_http_range(
    manager,
    "https://example.com/large-dataset.bin",  // URL
    1,                                        // port_id
    65536,                                    // chunk_size (64KB)
    0,                                        // total_size (0 = auto-discover)
    false                                     // loop_on_eof
);

// Open port
melvin_port_open(http_port);

// Process continuously
while (running) {
    melvin_port_manager_process_all(manager);
    usleep(10000);  // 10ms delay
}

// Cleanup
melvin_port_close(http_port);
melvin_port_manager_free(manager);
melvin_m_close(mfile);
```

### With Loop Mode (Continuous Training)

```c
MelvinPort *http_port = melvin_port_register_http_range(
    manager,
    "https://example.com/training-data.bin",
    1,
    65536,    // 64KB chunks
    0,        // auto-discover size
    true      // loop on EOF - continuously train
);
```

## How It Works

1. **Initialization**: Port opens and optionally performs HEAD request to discover file size
2. **Range Requests**: Each read operation sends HTTP Range request for specific byte range
3. **Chunk Processing**: Data is downloaded in chunks and immediately fed to `brain.m`
4. **Memory Efficiency**: Only one chunk is in memory at a time
5. **Continuous Learning**: If `loop_on_eof` is true, restarts from beginning when EOF reached

### HTTP Range Request Format

The port uses standard HTTP Range requests:
```
Range: bytes=start-end
```

For example:
- First chunk (0-65535): `Range: bytes=0-65535`
- Second chunk (65536-131071): `Range: bytes=65536-131071`
- And so on...

## Chunk Size Guidelines

- **Small files (< 10MB)**: 16KB-32KB chunks
- **Medium files (10-100MB)**: 32KB-64KB chunks
- **Large files (100MB-1GB)**: 64KB-256KB chunks
- **Very large files (> 1GB)**: 128KB-512KB chunks

Larger chunks reduce HTTP overhead but use more memory. The default 64KB is a good balance for most use cases.

## Server Requirements

The HTTP server must support:
- **HTTP Range requests** (RFC 7233)
- **206 Partial Content** responses for range requests
- **Content-Length** header (for auto-discovery)

Most modern web servers (Apache, Nginx, AWS S3, etc.) support range requests by default.

## Error Handling

The implementation handles:
- Network errors (timeouts, connection failures)
- HTTP errors (404, 403, 500, etc.)
- Invalid range responses
- EOF conditions

On error, the port marks itself as EOF and stops processing. The port can be reopened to retry.

## Statistics

The test program displays:
- Graph state (nodes, edges, adaptations)
- Port activity (frames/bytes processed)
- Overall statistics (total processed, processing rate)
- Last output preview

## Performance Considerations

- **Network Speed**: Processing rate limited by download speed
- **Server Performance**: Range request overhead depends on server
- **Chunk Size**: Larger chunks reduce HTTP overhead
- **Memory Usage**: Only one chunk in memory (configurable)

## Example Use Cases

1. **Large Dataset Training**: Download and process multi-GB datasets
2. **Continuous Learning**: Loop through training data indefinitely
3. **Streaming Processing**: Process data as it's downloaded
4. **Resumable Downloads**: Can restart from any position
5. **Cloud Storage**: Process files from S3, GCS, Azure Blob, etc.

## Limitations

- **Read-Only**: HTTP range ports are input-only (no write support)
- **Sequential**: Downloads chunks sequentially (not parallel)
- **Server Support**: Requires server to support range requests
- **Network Dependent**: Performance depends on network speed

## Troubleshooting

**Error: "Could not open HTTP range request port"**
- Check URL is accessible
- Verify network connectivity
- Ensure server supports range requests

**Slow Performance**
- Increase chunk size (reduce HTTP overhead)
- Check network speed
- Verify server performance

**EOF Reached Immediately**
- Check if URL is valid
- Verify server supports range requests
- Check Content-Length header

## Files

- `melvin_port_http.c`: HTTP range request port implementation
- `test/test_http_range.c`: Production-ready test program
- Header declarations in `melvin_ports.h`

## See Also

- `README_DATASET_PORT.md`: File-based dataset port
- `PORT_ARCHITECTURE.md`: Port architecture overview
- `README_PORTS.md`: General port documentation

