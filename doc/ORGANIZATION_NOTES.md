# Repository Organization Notes

## Organization Completed

The repository has been organized into clear directories:

### Structure

- **doc/** - All documentation files (24+ .md files)
- **test/** - All test files, test executables, .m files, and test scripts (200+ files)
- **Root** - Core source files only (melvin.c, melvin.h, melvin_m.c, melvin_m.h, README.md)

### Files Moved

1. **Documentation (→ doc/)**:
   - All .md files except README.md (which stays in root)
   - Analysis documents, testing guides, implementation plans, etc.

2. **Tests (→ test/)**:
   - All test_*.c files and executables
   - All .m files (persistent knowledge files)
   - Test scripts (*.sh)
   - Test data files (*.txt, *.log)
   - Example programs (example_m.c)
   - Verification programs (verify_*.c)
   - Analysis programs (analyze_*.c)

### Core Files (Stayed in Root)

- melvin.c - Core system implementation
- melvin.h - Core system header
- melvin_m.c - .m file format implementation
- melvin_m.h - .m file format header
- melvin_output_logger.c - Output logging
- README.md - Main project documentation

### Building Tests

When compiling tests, use relative paths:

```bash
# From root directory
gcc -o test/test_program test/test_program.c melvin.c melvin_m.c -lm

# Or from test directory
cd test
gcc -o test_program test_program.c ../melvin.c ../melvin_m.c -lm
```

### Running Tests

```bash
# From root directory
./test/test_program

# Or from test directory
cd test
./test_program
```

