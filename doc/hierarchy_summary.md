# Hierarchy Formation Analysis

## What's Working:
1. ✅ **Hierarchy IS forming** - Level 1 nodes with 2-byte payloads ("he", "el", "lo") are being created
2. ✅ **Edge weights are updating** - Edges strengthen with repeated patterns
3. ✅ **Hierarchy formation logic triggers** - When one edge dominates significantly

## Current Behavior:
- **Level 0**: Single-byte nodes (h, e, l, o)
- **Level 1**: Two-byte combined nodes (he, el, lo)
- **Level 2+**: Not forming yet

## Issues:
1. **Duplicates**: Multiple hierarchy nodes with same payload (nodes 4, 7, 10 all have "he")
   - Wave exploration needs to find existing hierarchy nodes before creating new ones
   
2. **No deeper hierarchy**: Level 1 nodes have `outgoing_count=0`, so they can't combine further
   - Need edges between hierarchy nodes to form Level 2+ patterns
   
3. **Hierarchy nodes are terminal**: They're created but don't connect to other hierarchy nodes
   - "he" → "el" should create edge, allowing "hello" (Level 2) to form

## How It Should Work:
1. **Pattern Recognition**: Repeated "hello" → "h"→"e"→"l"→"l"→"o" edges strengthen
2. **Level 1 Formation**: Strong "h"→"e" edge → creates "he" node
3. **Edge Creation**: "he" should connect to "el", "el" to "lo"
4. **Level 2 Formation**: Strong "he"→"el" edge → creates "hello" (4 bytes, Level 2)
5. **Generalization**: "hello" pattern becomes reusable abstraction

## Key Insight:
- Hierarchy nodes need **edges between them** to build deeper levels
- Current: Hierarchy nodes are created but remain isolated (outgoing_count=0)
- Needed: Edges should form between hierarchy nodes, allowing compounding abstraction
