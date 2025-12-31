# Output Threshold Implementation

## Summary

Implemented a relative threshold system that determines when wave propagation output should be sent to external ports, following the biological principle of "thinking" vs "output."

## Core Principle

**Thinking always happens; output is conditional on pattern maturity.**

- **Wave propagation** runs for every input (internal pattern updates)
- **Output generation** only happens when learned patterns exist (edges from input nodes)
- **No hardcoded thresholds** - all decisions based on relative measurements

## Implementation Details

### 1. Output Readiness Function (`melvin.c`)

```c
float compute_output_readiness(MelvinGraph *g, Node **input_nodes, size_t input_count)
```

- Measures outgoing edge strength from input nodes
- Returns 0 if no edges exist (novel pattern)
- Returns relative readiness score (0.0-1.0) based on average edge weight
- Formula: `avg_edge_weight / (avg_edge_weight + 1.0f)`
- Follows README principle: local measurements, relative comparison

### 2. Output Decision Logic (`melvin_m.c`)

In `melvin_m_process_input()`:

```c
float output_readiness = compute_output_readiness(mfile->graph, initial_nodes, initial_count);

if (output_readiness > 0.0f) {
    // Collect output from learned continuations
    wave_collect_output(mfile->graph, initial_nodes, initial_count, &output, &output_size);
} else {
    // Pure thinking mode - no output
    mfile->header.universal_output_size = 0;
}
```

### 3. Output Collection Changes (`melvin.c`)

**Removed input echoing:**
- Old behavior: Output contained direct input nodes (echoing)
- New behavior: Output only contains learned sequential continuations
- Input nodes marked as visited context, not included in output

**Fixed hardcoded threshold:**
- Old: `if (transformed > 0.2f && score > best_sequential_score)`
- New: `if (transformed > 0.0f && score > best_sequential_score)`
- Follows README principle: no hardcoded thresholds, relative comparisons only

## Behavior

### Novel Input (No Learned Patterns)
- Wave propagation runs (thinking)
- No edges from input nodes → readiness = 0
- No output generated
- Example: First time seeing "NOVEL"

### Repeated Input (Building Patterns)
- Wave propagation runs (thinking)
- Edges form through co-activation
- Initially: readiness = 0 (no strong patterns yet)
- After repetitions: readiness > 0 (edges exist)
- Output generated once patterns mature

### Familiar Input (Learned Patterns)
- Wave propagation runs (thinking)
- Strong edges exist → readiness > 0
- Output generated from learned continuations
- Example: After learning "HELLO", input "HELLO" generates continuations

## Biological Analogy

Matches biological systems:
- **Novel stimulus**: Internal processing only (no motor response)
- **Familiar stimulus**: Learned response (motor output)
- **All inputs**: Trigger neural activity (thinking/wave propagation)
- **Mature patterns**: Generate external output (speech/action)

## Test Results

```
Novel input "NOVEL": 0 bytes output (thinking only) ✓
Repeated "HELLO" 10 times:
  - Iterations 1-3: 0 bytes (thinking, building patterns)
  - Iterations 4-10: output generated (patterns mature)
Novel input "WORLD": 0 bytes output (thinking only) ✓
Familiar "HELLO": 11 bytes output (learned response) ✓
```

## Files Modified

1. **melvin.c**:
   - Added `compute_output_readiness()` function
   - Modified `wave_collect_output()` to remove input echoing
   - Fixed hardcoded threshold (0.2f → 0.0f with relative logic)

2. **melvin_m.c**:
   - Modified `melvin_m_process_input()` to check output readiness
   - Conditional output collection based on pattern maturity

3. **melvin.h**:
   - Added `compute_output_readiness()` declaration

4. **README.md**:
   - Updated processing steps to include output readiness decision
   - Added "Output Readiness (Thinking vs Output)" section
   - Documented biological-like behavior

## Principles Followed

✓ **Self-regulation**: Thresholds based on local measurements only  
✓ **No hardcoded limits**: All decisions relative to data  
✓ **Local context**: Nodes only know themselves and their edges  
✓ **Data-driven**: Behavior emerges from learned patterns  
✓ **Biological-like**: Matches how neural systems process stimuli  

## Future Considerations

- Output destination routing (port_id selection) based on learned patterns
- Internal feedback loops (port 255) for "rehearsal" without external output
- Confidence-based output strength modulation
- Multi-modal output decisions (different thresholds for different port types)

