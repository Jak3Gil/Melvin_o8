# Melvin Testing Interface

## Overview

The Melvin test system allows you to test .m files without writing code. The same .m file is reused across multiple tests, allowing the system to learn and adapt persistently.

## Quick Start

### Single Input Test
```bash
./melvin_test_runner test.m -i "hello"
```

### Multiple Inputs from File
```bash
./melvin_test_runner test.m -f test_cases.txt
```

### Analyze Current State
```bash
./melvin_test_runner test.m -a
```

## Test File Format

Create a text file with test cases:

```
# Comments start with #
test_name: input_data
test2: world
test3: hex:48656C6C6F
test4: file:input.bin
```

## Features

### Persistent Learning
- The same .m file is reused across all tests
- Graph state persists between test runs
- Each test builds on previous learning

### Automatic Analysis
- Output/Input ratio
- Similarity percentage
- Detection of echo, extension, or compression
- Byte distribution statistics
- Graph growth tracking

### No Code Required
- Just provide inputs and get analyzed outputs
- Test cases in simple text format
- Automatic graph state tracking

## Example Workflow

1. **Create initial test file:**
   ```bash
   ./melvin_test_runner mytest.m -i "hello"
   ```

2. **Add more tests (reuses same .m file):**
   ```bash
   ./melvin_test_runner mytest.m -i "world"
   ./melvin_test_runner mytest.m -i "hello world"
   ```

3. **Run test suite:**
   ```bash
   ./melvin_test_runner mytest.m -f test_cases.txt
   ```

4. **Check final state:**
   ```bash
   ./melvin_test_runner mytest.m -a
   ```

## Output Analysis

The system automatically analyzes:
- **Echo behavior**: Output matches input exactly
- **Extension**: Output longer than input (learned continuation)
- **Compression**: Output shorter than input (abstraction)
- **Similarity**: Percentage of matching bytes
- **Graph growth**: Nodes and edges added per test

## Graph State Tracking

Each test shows:
- **Before**: Graph state before processing
- **After**: Graph state after processing
- **Adaptations**: Number of times the file has adapted

The graph grows and learns with each test, building persistent knowledge in the .m file.

