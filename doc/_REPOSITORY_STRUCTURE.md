# Repository Structure

This repository is organized into clear directories:

## Directory Structure

```
Melvin_o8/
├── doc/                    # All documentation files (.md)
├── test/                   # All test files and test data
│   ├── test_*.c           # Test source files
│   ├── *.m                # Test .m files (persistent knowledge files)
│   ├── *.sh               # Test scripts
│   └── *.txt              # Test data/output files
├── melvin.c               # Core system implementation
├── melvin.h               # Core system header
├── melvin_m.c             # .m file format implementation
├── melvin_m.h             # .m file format header
├── melvin_output_logger.c # Output logging (if separate)
└── README.md              # Main README (stays in root)
```

## Core Files (Root Directory)

- **melvin.c** - Core Melvin system implementation
- **melvin.h** - Core system header file
- **melvin_m.c** - .m file format implementation
- **melvin_m.h** - .m file format header
- **README.md** - Main project documentation

## Documentation (doc/)

All markdown documentation files (.md) are located in the `doc/` directory:

- Architecture documents
- Implementation guides
- Testing guides
- Analysis documents
- Philosophy and design documents

## Tests (test/)

All test-related files are located in the `test/` directory:

- **Test source files** (`test_*.c`) - Test program source code
- **Test executables** - Compiled test programs
- **.m files** - Persistent knowledge files created by tests
- **Test scripts** (`.sh`) - Automated test runners
- **Test data** (`.txt`) - Test input/output data

## Building Tests

To compile tests, change to the test directory or use paths:

```bash
# From root directory
gcc -o test/test_program test/test_program.c melvin.c melvin_m.c -lm

# Or from test directory
cd test
gcc -o test_program test_program.c ../melvin.c ../melvin_m.c -lm
```

## Running Tests

```bash
# From root directory
./test/test_program

# Or from test directory
cd test
./test_program
```

