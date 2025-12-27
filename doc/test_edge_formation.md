# Edge Formation Implementation Summary

## Simple Explicit Rule (Do More With Less):
**"Nodes that activate together form edges"**

This single rule applies to:
1. Sequential input data → sequential edges
2. Wave propagation co-activations → wave-prop edges
3. Hierarchy nodes → hierarchy connections

## What Changed:

1. **Renamed function**: `wave_create_sequential_edges` → `wave_create_edges_from_coactivation`
   - More general: applies to any co-activation, not just sequential data
   - Simpler: one rule, multiple contexts

2. **Wave propagation creates edges**: `wave_propagate_multi_step` now creates edges between nodes co-activated in the same step
   - Simple: collect co-activated nodes → create edges
   - Complex behavior emerges: patterns form naturally through wave propagation

3. **Hierarchy nodes connect**: `node_transfer_incoming_to_hierarchy` now also transfers outgoing edges
   - Simple: preserve connectivity
   - Complex behavior: hierarchy nodes can participate in graph and form deeper levels

## Biological Principles:
- **Neurons**: "Fire together, wire together" → co-activation creates edges
- **Fungi**: Growth toward signals → connections form from proximity/activation
- **Melvin**: Co-activation → edges (one simple rule)

## Result:
- Simpler code (fewer explicit rules)
- More emergent behavior (complexity from simple mechanism)
- Hierarchy nodes can now connect and form deeper abstraction
