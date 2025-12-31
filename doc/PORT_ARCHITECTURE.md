# Port Architecture: How Ports Connect to brain.m

## Overview

Ports are the interface between external devices/files and the unified brain.m graph. All ports operate **outside** the .m file, and data flows through a unified graph where cross-modal associations are learned.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Port Manager (External)                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Input Port 1 │  │ Input Port 2 │  │ Output Port  │      │
│  │ (File)       │  │ (USB Mic)    │  │ (File)       │      │
│  └──────┬───────┘  └──────┬───────┘  └──────▲───────┘      │
│         │                  │                  │              │
│         │ Package          │ Package          │ Write        │
│         │ PortFrame        │ PortFrame        │ Data         │
│         ▼                  ▼                  │              │
│  ┌──────────────────────────────────────────┐ │              │
│  │      Universal Input (brain.m)           │ │              │
│  │  [port_id + timestamp + data_size + data]│ │              │
│  └──────────────────┬───────────────────────┘ │              │
│                     │                          │              │
│                     ▼                          │              │
│  ┌──────────────────────────────────────────┐ │              │
│  │      Unified Graph (brain.m)             │ │              │
│  │  - All nodes in single graph             │ │              │
│  │  - Audio nodes, text nodes, video nodes  │ │              │
│  │  - Edges connect across modalities       │ │              │
│  │  - Cross-modal associations learned      │ │              │
│  └──────────────────┬───────────────────────┘ │              │
│                     │                          │              │
│                     │ Wave Propagation         │              │
│                     │                          │              │
│                     ▼                          │              │
│  ┌──────────────────────────────────────────┐ │              │
│  │      Universal Output (brain.m)          │ │              │
│  │  [raw output data]                       │ │              │
│  └──────────────────┬───────────────────────┘ │              │
│                     │                          │              │
│                     │ Routing Table:           │              │
│                     │  Input Port 1 → Output Port  │          │
│                     │  Input Port 2 → Output Port  │          │
│                     ▼                          │              │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

### 1. Input Port → brain.m

```c
// Input port reads data
uint8_t data[4096];
size_t data_size = port->read_func(port, data, sizeof(data));

// Package as PortFrame (CAN bus format)
PortFrame frame = {
    .port_id = port->port_id,        // e.g., 1
    .timestamp = get_timestamp(),
    .data_size = data_size,
    .data = data
};

// Serialize frame: [port_id (1)] + [timestamp (8)] + [data_size (4)] + [data (N)]
uint8_t serialized[8192];
size_t serialized_size = melvin_port_frame_serialize(&frame, serialized, sizeof(serialized));

// Write to brain.m universal input
melvin_m_universal_input_write(brain_m, serialized, serialized_size);
```

### 2. brain.m Processing

```c
// Process input through unified graph
melvin_m_process_input(brain_m);

// What happens inside:
// 1. Extract port_id from first byte (for routing)
// 2. Create/activate nodes from data bytes (wave_process_sequential_patterns)
// 3. Create edges between nodes (co-activation, similarity, context)
// 4. Wave propagation across unified graph
// 5. Collect output from activated nodes
```

### 3. brain.m → Output Port

```c
// Get input port ID for routing
uint8_t input_port_id = melvin_m_get_last_input_port_id(brain_m);

// Look up routing table: input_port_id → output_port_id
uint8_t output_port_id = routing_table[input_port_id];

// Read output from brain.m
size_t output_size = melvin_m_universal_output_size(brain_m);
uint8_t output_data[output_size];
melvin_m_universal_output_read(brain_m, output_data, output_size);

// Write to output port
PortFrame output_frame = {
    .port_id = output_port_id,
    .timestamp = get_timestamp(),
    .data_size = output_size,
    .data = output_data
};
melvin_port_write_frame(output_port, &output_frame);
```

## Cross-Modal Associations

### Key Concept

- **Unified Graph**: All nodes exist in the same graph (audio, text, video, sensor data)
- **Data Preservation**: Each node stores actual data bytes (audio bytes ≠ text bytes)
- **Edge Formation**: Nodes connect across modalities through:
  - **Co-activation edges**: Nodes that activate together
  - **Similarity edges**: Similar patterns (computed from bytes)
  - **Context edges**: Shared wave propagation paths

### Example Flow

```
Input: Audio "meow" sound (port 1)
  ↓
Creates nodes: [audio_byte_patterns]
  ↓
Wave propagation activates related nodes
  ↓
If cat image nodes exist (from previous port 2 input):
  → Co-activation edges connect them
  → Text nodes related to "cat" also activate
  ↓
Output: Text "cat" (routed to port 3)
```

**Important**: Audio bytes stay as audio bytes, text stays as text. But edges connect them, enabling cross-modal influence.

## Port Pairs and Routing

### Registration Pattern

```c
// Create port manager (one per brain.m file)
MelvinPortManager *manager = melvin_port_manager_create(brain_m);

// Register input ports
MelvinPort *text_input = melvin_port_register_file_input(
    manager, "dataset.txt", 1, 4096);
MelvinPort *audio_input = melvin_port_register_usb_mic(manager, NULL, 2);

// Register output ports
MelvinPort *text_output = melvin_port_register_file_output(
    manager, "output.txt:append", 3, true);
MelvinPort *audio_output = melvin_port_register_usb_speaker(manager, NULL, 4);

// Set routing: input_port_id → output_port_id
melvin_port_set_route(manager, 1, 3);  // text input → text output
melvin_port_set_route(manager, 2, 4);  // audio input → audio output

// Processing handles routing automatically
melvin_port_manager_process_all(manager);
```

### Routing Table

The routing table is a simple mapping:
- `routing_table[input_port_id] = output_port_id`
- 0 means no route (output discarded)
- Multiple inputs can route to same output
- One input can route to one output

## Unified File Port Implementation

The `melvin_port_file.c` implements both input and output file ports:

### Input Port Functions

```c
// Register file input port
MelvinPort* melvin_port_register_file_input(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    size_t chunk_size);

// Register with loop on EOF
MelvinPort* melvin_port_register_file_input_loop(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    size_t chunk_size);
```

### Output Port Functions

```c
// Register file output port
MelvinPort* melvin_port_register_file_output(
    MelvinPortManager *manager,
    const char *file_path,
    uint8_t port_id,
    bool append_mode);  // true = append, false = overwrite
```

### Usage Example

```c
// Setup
MelvinPortManager *manager = melvin_port_manager_create(brain_m);

// Input port: reads from dataset.txt in 4KB chunks
MelvinPort *input = melvin_port_register_file_input(
    manager, "dataset.txt", 1, 4096);

// Output port: appends to output.txt
MelvinPort *output = melvin_port_register_file_output(
    manager, "output.txt", 2, true);

// Route input → output
melvin_port_set_route(manager, 1, 2);

// Open ports
melvin_port_open(input);
melvin_port_open(output);

// Process (reads input, processes through brain.m, writes output)
melvin_port_manager_process_all(manager);
```

## Key Principles

1. **Ports are External**: All port code operates outside brain.m
2. **Unified Graph**: All data flows through single graph in brain.m
3. **Cross-Modal Learning**: Edges connect nodes across data types
4. **Routing is External**: Routing table maps input → output ports
5. **Data Type Preservation**: Nodes store actual bytes (type preserved)
6. **Association Through Edges**: Cross-modal influence via edges, not data mixing

