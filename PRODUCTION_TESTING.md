# Production Testing Guide

## Overview

The production test suite evaluates system capabilities and production readiness across multiple dimensions.

## Running Tests

### Build the Test Suite

```bash
make test_production
```

### Run All Tests

```bash
./test/test_production test_dataset_1gb.txt
```

### Run Specific Test

```bash
# Test 1: Chunk size performance
./test/test_production test_dataset_1gb.txt 1

# Test 2: Save/load integrity
./test/test_production test_dataset_1gb.txt 2

# Test 3: Long-running stability (60 seconds)
./test/test_production test_dataset_1gb.txt 3

# Test 4: Output quality analysis
./test/test_production test_dataset_1gb.txt 4
```

## Test Descriptions

### Test 1: Chunk Size Performance

**Purpose**: Evaluate performance across different chunk sizes

**What it tests**:
- Processing speed (frames/sec, KB/sec)
- Memory usage per chunk size
- Brain growth rate
- Optimal chunk size identification

**Metrics**:
- Frames processed
- Bytes processed
- Time elapsed
- Throughput (KB/sec, frames/sec)
- Memory delta
- Final brain size (nodes, edges)

**Expected results**:
- Larger chunk sizes should process faster (fewer function calls)
- Memory usage should be reasonable
- Brain should grow consistently

### Test 2: Save/Load Cycle Integrity

**Purpose**: Verify data persistence correctness

**What it tests**:
- Save operation success
- Load operation success
- Data integrity (nodes, edges, adaptations)
- State preservation

**Metrics**:
- Node count before/after
- Edge count before/after
- Adaptation count before/after
- Exact match verification

**Expected results**:
- All counts should match exactly
- No data loss during save/load

### Test 3: Long-Running Stability

**Purpose**: Test system stability over extended periods

**What it tests**:
- Memory leaks
- Performance degradation
- Brain growth patterns
- Auto-save functionality
- Resource management

**Metrics**:
- Total frames/bytes processed
- Memory usage over time
- Brain size growth
- Node/edge count progression
- Throughput consistency

**Expected results**:
- Memory should stabilize (no continuous growth)
- Performance should remain consistent
- Brain should grow steadily
- No crashes or hangs

### Test 4: Output Quality Analysis

**Purpose**: Evaluate output quality and learning

**What it tests**:
- Output file generation
- Text quality metrics
- Pattern learning evidence
- Output readability

**Metrics**:
- Output file size
- Printable character percentage
- Word count
- Line count
- Average line length
- Sample content inspection

**Expected results**:
- Output should contain readable text
- Patterns should emerge
- Output should reflect input patterns

## Production Readiness Criteria

### Performance
- [ ] Throughput > 100 KB/sec for 4KB chunks
- [ ] Memory usage < 500 MB for 1GB dataset
- [ ] No performance degradation over time
- [ ] Consistent frame processing rate

### Reliability
- [ ] Save/load cycles preserve all data
- [ ] No memory leaks during long runs
- [ ] Graceful handling of EOF
- [ ] No crashes or hangs
- [ ] Auto-save works correctly

### Quality
- [ ] Output contains meaningful patterns
- [ ] Brain grows with data (learns)
- [ ] Output reflects input characteristics
- [ ] Edge formation works correctly

### Scalability
- [ ] Handles 1GB+ datasets
- [ ] Memory usage scales reasonably
- [ ] Processing time scales linearly
- [ ] Brain size grows appropriately

## Monitoring During Tests

### Real-time Monitoring

```bash
# Watch brain.m growth
watch -n 1 'ls -lh brain.m'

# Monitor output file
tail -f test_dataset_1gb.txt.output

# Check process resources
top -pid $(pgrep test_production)
```

### Key Metrics to Watch

1. **Memory Usage**: Should stabilize, not grow continuously
2. **CPU Usage**: Should be consistent (not spiking)
3. **Brain Size**: Should grow steadily with data
4. **Output Size**: Should accumulate over time
5. **Processing Rate**: Should remain consistent

## Interpreting Results

### Good Signs
- Consistent throughput
- Stable memory usage
- Brain grows with data
- Output shows learned patterns
- Save/load preserves state

### Warning Signs
- Memory continuously growing (possible leak)
- Performance degrading over time
- Brain not growing (not learning)
- Output is gibberish (not learning patterns)
- Save/load data mismatch

### Failure Indicators
- Crashes or hangs
- Memory exhaustion
- Save/load corruption
- No output generation
- Zero throughput

## Advanced Testing

### Stress Testing

```bash
# Test with very large dataset (10GB+)
./test/test_production large_dataset.txt 3

# Test with many small files
for file in dataset*.txt; do
    ./test/test_production "$file" 1
done

# Test with different data types
./test/test_production binary_data.bin 4
```

### Concurrent Testing

```bash
# Run multiple instances (test isolation)
./test/test_production dataset1.txt 3 &
./test/test_production dataset2.txt 3 &
./test/test_production dataset3.txt 3 &
wait
```

### Resource Limits Testing

```bash
# Test with memory limits
ulimit -v 1048576  # 1GB limit
./test/test_production dataset.txt 3

# Test with time limits
timeout 300 ./test/test_production dataset.txt 3
```

## Production Deployment Checklist

- [ ] All tests pass
- [ ] Performance meets requirements
- [ ] Memory usage acceptable
- [ ] Save/load works correctly
- [ ] Long-running stability confirmed
- [ ] Output quality acceptable
- [ ] Error handling robust
- [ ] Resource limits tested
- [ ] Documentation complete

