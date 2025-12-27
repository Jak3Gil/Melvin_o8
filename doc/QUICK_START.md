# Melvin Quick Start Guide

## Testing Without Writing Code

The Melvin test system lets you test .m files by simply feeding inputs and analyzing outputs. No code required!

## Basic Usage

### 1. Single Input Test
```bash
./melvin_test_runner mytest.m -i "hello"
```

### 2. Multiple Tests on Same File
```bash
./melvin_test_runner mytest.m -i "hello"
./melvin_test_runner mytest.m -i "world"    # Reuses same .m file!
./melvin_test_runner mytest.m -i "hello world"
```

### 3. Test File (Multiple Inputs)
Create `my_tests.txt`:
```
test1: hello
test2: world
test3: hello world
test4: hello
```

Run:
```bash
./melvin_test_runner mytest.m -f my_tests.txt
```

### 4. Check Current State
```bash
./melvin_test_runner mytest.m -a
```

## Key Features

✅ **Persistent Learning**: Same .m file reused across all tests  
✅ **Auto-Analysis**: Automatic output analysis (similarity, extension, compression)  
✅ **Graph Tracking**: Shows nodes/edges/adaptations before and after each test  
✅ **No Code**: Just provide inputs, get analyzed outputs  

## What Gets Analyzed

- **Output/Input ratio**: How output size compares to input
- **Similarity**: Percentage of matching bytes
- **Behavior detection**: Echo, extension, or compression
- **Byte distribution**: Unique values and frequency
- **Graph growth**: Nodes and edges added per test

## Example Output

```
Test 1: hello
  Before: Nodes=0, Edges=0, Adaptations=0
  After: Nodes=4, Edges=4, Adaptations=1
  → Echo/pass-through behavior

Test 2: world  
  Before: Nodes=4, Edges=4, Adaptations=1
  After: Nodes=7, Edges=18, Adaptations=2
  → Echo/pass-through behavior
```

The graph learns and grows with each test!

