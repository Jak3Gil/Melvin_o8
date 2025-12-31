# Repository Organization

## Directory Structure

```
Melvin_o8/
├── README.md              # Main documentation
├── Makefile               # Build system
├── melvin.c               # Core intelligence system
├── melvin.h               # Core headers
├── melvin_m.c             # .m file format
├── melvin_ports.c         # Port system
├── melvin_gpu.c           # GPU acceleration
├── show_brain.c           # Monitoring tool
├── analyze_mfile.c        # Analysis tool
├── monitor_brain.sh       # File monitoring
├── monitor_live.sh        # Live monitoring
│
├── pipeline/              # Production applications
│   ├── example_ports.c    # → builds as 'pipeline'
│   ├── test_dataset_port.c # → builds as 'dataset_port'
│   ├── test_http_range.c  # → builds as 'http_range'
│   └── test_performance.c # → builds as 'performance'
│
├── doc/                   # All documentation
│   ├── *.md              # All markdown documentation
│   └── ...
│
└── test/                  # Test data and results
    ├── *.txt             # Test input files
    ├── *.m               # Test brain files
    ├── *.output          # Test output files
    └── *.txt             # Analysis reports
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

## Running Production Apps

```bash
# Dataset processor
./dataset_port test/simple_test.txt test/test_brain.m

# Pipeline
./pipeline brain.m

# HTTP range
./http_range <url> brain.m

# Performance monitor
./performance brain.m
```

## Monitoring

```bash
# Quick stats
./show_brain brain.m

# Detailed analysis
./analyze_mfile brain.m doc/analysis.txt

# File monitoring
./monitor_brain.sh brain.m
```

