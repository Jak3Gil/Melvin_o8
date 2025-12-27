# Output Logging and Analysis

## Overview

The Melvin system automatically logs all outputs over time, allowing you to track how the system's behavior changes as it learns and adapts.

## Automatic Logging

Every time you run a test, the output is automatically logged to a `.log` file:

```
test.m          → test.m.log
myfile.m        → myfile.m.log
```

The log file contains:
- Timestamp
- Test name
- Input and output data (hex format)
- Graph state (nodes, edges, adaptations)
- Similarity metrics

## Viewing Logs

### Analyze Log History

```bash
./melvin_test_runner test.m -a logs
```

This shows:
- **Overall Statistics**: Average similarity, output ratios, peak graph size
- **Trend Analysis**: How similarity and graph size change over time
- **Recent Entries**: Last 5 entries with full details

### Example Output

```
========================================
Output Log Analysis: test.m
========================================

Total entries: 10

=== Overall Statistics ===
Average similarity: 85.50%
Average output/input ratio: 1.05
Total input bytes: 50
Total output bytes: 52
Peak nodes: 15
Peak edges: 45

=== Trend Analysis ===
Similarity change: 100.00% -> 95.00% (-5.00% change)
Node growth: 4 -> 15 (+11 nodes)
→ Similarity stable

=== Recent Entries (last 5) ===
...
```

## What Gets Logged

Each log entry includes:

1. **Timestamp**: When the test ran
2. **Test Name**: Identifier for the test
3. **Adaptation Count**: How many times the file has adapted
4. **Input/Output Sizes**: Byte counts
5. **Similarity**: Percentage of matching bytes
6. **Graph State**: Current node and edge counts
7. **Hex Data**: Full input and output in hex format

## Use Cases

### 1. Track Learning Progress

See how similarity changes as the system learns:
```bash
./melvin_test_runner test.m -i "hello"
./melvin_test_runner test.m -i "hello"  # Repeated input
./melvin_test_runner test.m -a logs     # Check trends
```

### 2. Compare Outputs Over Time

The log preserves all outputs, so you can:
- See how outputs change for the same input
- Track when new patterns emerge
- Identify when the system starts generalizing

### 3. Graph Growth Analysis

Monitor how the graph grows:
- Node count trends
- Edge count trends
- Relationship between graph size and performance

### 4. Debug Output Issues

If outputs are unexpected:
- Check the log to see when it changed
- Compare entries before/after the issue
- Track which inputs caused the change

## Log File Format

Logs are stored in a simple text format:

```
ENTRY
timestamp=1703524531
test_name=Test 1
adaptation_count=1
input_size=5
output_size=5
similarity=1.0000
node_count=4
edge_count=4
input_hex=68656C6C6F
output_hex=68656C6C6F
END_ENTRY
```

## Integration

Logging is **automatic** - no code changes needed. Every test run automatically:
1. Processes input
2. Generates output
3. Logs the result
4. Saves to `.log` file

Just use the test runner normally, and logs accumulate over time!

