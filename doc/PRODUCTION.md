# Melvin Production System

## Philosophy

**There is no difference between testing and production.** The production system IS the test system. You test by running production with test data.

## Production Applications

### 1. Dataset Port (`dataset_port`)
Processes dataset files through the port system.

```bash
make dataset_port
./dataset_port dataset.txt brain.m
```

**Options:**
- `--chunk-size SIZE` - Size of each read chunk (default: 4096)
- `--loop` - Loop dataset file when EOF is reached

**Features:**
- Auto-saves brain.m every 30 seconds
- Output routing to `.output` file
- Real-time statistics

### 2. Pipeline (`pipeline`)
Continuous processing with mic, camera, and speaker ports.

```bash
make pipeline
./pipeline brain.m
```

**Features:**
- Real-time status updates
- Port statistics
- Graph evolution tracking

### 3. HTTP Range (`http_range`)
Processes data from HTTP range requests.

```bash
make http_range
./http_range <url> brain.m
```

### 4. Performance Monitor (`performance`)
Measures operations per byte and throughput.

```bash
make performance
./performance brain.m
```

## Monitoring Tools

### Quick Stats (`show_brain`)
Quick view of nodes, edges, and output.

```bash
make show_brain
./show_brain brain.m
```

### Detailed Analysis (`analyze_mfile`)
Comprehensive graph analysis.

```bash
make analyze_mfile
./analyze_mfile brain.m report.txt
```

### File Monitor (`monitor_brain.sh`)
Watch brain.m file size and growth.

```bash
./monitor_brain.sh brain.m
```

## Workflow

1. **Run production system:**
   ```bash
   ./dataset_port test_data.txt brain.m
   ```

2. **In another terminal, monitor:**
   ```bash
   ./monitor_brain.sh brain.m
   # or
   ./show_brain brain.m
   ```

3. **Get detailed analysis:**
   ```bash
   ./analyze_mfile brain.m analysis.txt
   ```

## Building

```bash
# Build library
make

# Build all production apps
make all-apps

# Build specific app
make pipeline
make dataset_port
make http_range
make performance
make show_brain
make analyze_mfile
```

## No Test Infrastructure

There is no separate test system. You test by:
- Running production apps with test data
- Monitoring with the monitoring tools
- Analyzing results with `analyze_mfile`

If it works in production, it works. If it doesn't work in production, it doesn't work.

