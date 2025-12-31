# Unified File Port Implementation

## Overview

The unified file port system (`melvin_port_file.c`) provides both input and output file ports for feeding data to and receiving data from `brain.m`. All functional port code for file I/O is consolidated in this single file.

## Architecture

### How Ports Connect to brain.m

1. **Input Ports**: Read from files, package data as `PortFrame`, send to `brain.m` universal input
2. **brain.m Processing**: Processes bytes through unified graph (all data types together)
3. **Output Ports**: Receive data from `brain.m` universal output, write to files
4. **Routing**: External routing table maps `input_port_id → output_port_id`

### Cross-Modal Associations

- All nodes (audio, text, video) exist in unified graph
- Nodes store actual data bytes (data type preserved)
- Edges connect nodes across modalities (co-activation, similarity, context)
- Audio input can influence text output through learned associations
- Output routing determines delivery, but graph learns cross-modal patterns

## API Functions

### Input Ports

```c
// Register file input port (reads from file, sends to brain.m)
MelvinPort* melvin_port_register_file_input(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    size_t chunk_size);

// Register file input port with loop on EOF
MelvinPort* melvin_port_register_file_input_loop(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    size_t chunk_size);
```

### Output Ports

```c
// Register file output port (receives from brain.m, writes to file)
MelvinPort* melvin_port_register_file_output(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    bool append_mode);  // true = append, false = overwrite
```

### Legacy Compatibility

The following functions are still supported for backwards compatibility (they map to the unified functions):

```c
MelvinPort* melvin_port_register_dataset_file(...);
MelvinPort* melvin_port_register_dataset_file_loop(...);
```

## Usage Example

```c
// Create port manager (one per brain.m file)
MelvinMFile *brain_m = melvin_m_open("brain.m");
MelvinPortManager *manager = melvin_port_manager_create(brain_m);

// Register input port: reads from dataset.txt
MelvinPort *input = melvin_port_register_file_input(
    manager, "dataset.txt", 1, 4096);

// Register output port: appends to output.txt
MelvinPort *output = melvin_port_register_file_output(
    manager, "output.txt", 2, true);

// Set routing: input port 1 → output port 2
melvin_port_set_route(manager, 1, 2);

// Open ports
melvin_port_open(input);
melvin_port_open(output);

// Process: reads input → brain.m → writes output
melvin_port_manager_process_all(manager);

// Cleanup
melvin_port_close(input);
melvin_port_close(output);
melvin_port_manager_free(manager);
melvin_m_close(brain_m);
```

## File Format Specifications

### Input Port Device Path Format

- `"filepath"` - Use default chunk size (4096 bytes)
- `"filepath:chunk_size"` - Specify chunk size (e.g., `"data.txt:8192"`)
- `"filepath:chunk_size:loop"` - Loop file on EOF (e.g., `"data.txt:4096:loop"`)

### Output Port Device Path Format

- `"filepath:append"` - Append mode (accumulates outputs)
- `"filepath:overwrite"` - Overwrite mode (new file each time)

## Data Flow

```
Input File → Input Port → PortFrame → brain.m universal_input
                                        ↓
                                   Unified Graph
                                   (all data types)
                                        ↓
                        brain.m universal_output → Output Port → Output File
```

## Key Features

- **Unified Implementation**: Input and output in single file
- **Production-Ready**: Error handling, resource management, statistics
- **Flexible**: Configurable chunk sizes, loop modes, append/overwrite
- **Compatible**: Legacy function names still work
- **Documented**: Comprehensive comments on architecture and data flow

## Port Pair Pattern

For complete I/O workflows:

1. Register input port (file_input)
2. Register output port (file_output)
3. Set routing: `melvin_port_set_route(manager, input_id, output_id)`
4. Open both ports
5. Call `melvin_port_manager_process_all()` - handles everything automatically

See `PORT_ARCHITECTURE.md` for detailed architecture documentation.

