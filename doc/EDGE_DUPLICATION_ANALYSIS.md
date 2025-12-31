# Edge Duplication Analysis

## Critical Finding: `graph_add_edge` Does NOT Check for Duplicates

**Line 3754-3796**: `graph_add_edge` simply appends edges to the graph array and node arrays without checking if the edge already exists. This means **if we call `graph_add_edge` with a duplicate edge, it WILL be added, creating duplicates**.

## Edge Creation Flow

### Main Orchestrator: `wave_form_intelligent_edges` (Line 3249)
Calls these mechanisms in order:
1. **Co-activation** - `wave_create_edges_from_coactivation`
2. **Context** - `wave_create_edges_from_context`  
3. **Similarity** - `wave_create_edges_from_similarity`
4. **Generalization** - `wave_form_universal_generalizations`
5. **Homeostatic** - `wave_create_homeostatic_edges`

### Issues Found:

#### ✅ 1. Co-Activation Edges (Line 3314) - CORRECT
- ✅ Checks for existing edges before creating: `node_find_edge_to(from, to)` (Line 3329)
- ✅ Strengthens existing edges via `edge_update_weight_local` (Line 3334)
- ✅ Only creates new edges if they don't exist

#### ✅ 2. Similarity Edges (Line 2728) - CORRECT (after our fix)
- ✅ Checks both directions: `node_find_edge_to(node, similar)` and `node_find_edge_to(similar, node)` (Lines 2749-2750)
- ✅ Strengthens existing edges (Lines 2753-2772)
- ✅ Only creates missing directions

#### ✅ 3. Context Edges (Line 2848) - CORRECT (after our fix)
- ✅ Checks both directions (Lines 2874-2875)
- ✅ Strengthens existing edges (Lines 2878-2895)
- ✅ Only creates missing directions

#### ❌ 4. Blank Node Edge Creation (Lines 2596-2604, 2641-2649, 2661-2671) - **NO CHECKS**
These create edges **WITHOUT checking if they already exist**:

**Location 1: Lines 2596-2604** (when connecting new pattern to accepting blank)
```c
Edge *edge1 = edge_create(new_pattern_node, accepting_blank, true);
Edge *edge2 = edge_create(accepting_blank, new_pattern_node, true);
if (edge1) {
    edge1->weight = acceptance_strength;
    graph_add_edge(g, edge1, new_pattern_node, accepting_blank);  // NO CHECK!
}
if (edge2) {
    edge2->weight = acceptance_strength;
    graph_add_edge(g, edge2, accepting_blank, new_pattern_node);  // NO CHECK!
}
```

**Location 2: Lines 2641-2649** (when creating blank bridge to similar pattern)
```c
Edge *edge1 = edge_create(blank_bridge, found_match, true);
Edge *edge2 = edge_create(found_match, blank_bridge, true);
if (edge1) {
    edge1->weight = match_strength;
    graph_add_edge(g, edge1, blank_bridge, found_match);  // NO CHECK!
}
if (edge2) {
    edge2->weight = match_strength;
    graph_add_edge(g, edge2, found_match, blank_bridge);  // NO CHECK!
}
```

**Location 3: Lines 2661-2671** (when connecting new pattern to blank bridge)
```c
Edge *edge1 = edge_create(new_pattern_node, blank_bridge, true);
Edge *edge2 = edge_create(blank_bridge, new_pattern_node, true);
if (edge1) {
    edge1->weight = match_strength;
    graph_add_edge(g, edge1, new_pattern_node, blank_bridge);  // NO CHECK!
}
if (edge2) {
    edge2->weight = match_strength;
    graph_add_edge(g, edge2, blank_bridge, new_pattern_node);  // NO CHECK!
}
```

**Problem**: These run in `wave_process_sequential_patterns` which executes BEFORE `wave_form_intelligent_edges`. So if blank nodes are created/connected, and then similarity/context mechanisms try to create edges between the same nodes, we might get duplicates.

#### ⚠️ 5. Generalization Edges (Line 3050-3068) - **NO CHECKS**
```c
Edge *e1 = edge_create(node1, generalization, true);
Edge *e2 = edge_create(generalization, node1, true);
Edge *e3 = edge_create(node2, generalization, true);
Edge *e4 = edge_create(generalization, node2, true);
// ... no checks before graph_add_edge
graph_add_edge(g, e1, node1, generalization);
graph_add_edge(g, e2, generalization, node1);
graph_add_edge(g, e3, node2, generalization);
graph_add_edge(g, e4, generalization, node2);
```

**Problem**: If a generalization node already exists and already has edges to node1/node2, this will create duplicates.

#### ⚠️ 6. Edge Transfer in Hierarchy (Lines 2571, 2581, 4228, 4239, 4252) - **NO CHECKS**
When transferring edges to hierarchy nodes, edges are created without checking:
```c
Edge *new_edge = edge_create(old_edge->from_node, combined, true);
if (new_edge) {
    new_edge->weight = old_edge->weight;
    graph_add_edge(g, new_edge, old_edge->from_node, combined);  // NO CHECK!
}
```

**Problem**: If hierarchy node already has edges from the same source, duplicates are created.

## Root Cause Analysis

1. **`graph_add_edge` doesn't prevent duplicates** - It's a low-level function that assumes the caller has already checked
2. **Multiple code paths create edges without checks** - Blank node code, generalization, hierarchy transfer
3. **These run at different times** - Some before `wave_form_intelligent_edges`, some during, creating timing issues

## Impact

- **High edge counts**: ~44 edges/node instead of expected ~5-10
- **Duplicates between same node pairs**: Multiple edges from A→B
- **Performance degradation**: More edges = slower traversal
- **Weakened learning**: Instead of strengthening existing edges, new weak duplicates are created

## Solution

All edge creation sites need to:
1. Check for existing edges using `node_find_edge_to` before creating
2. If edge exists, strengthen it via `edge_update_weight_local` instead
3. Only create new edges if they don't exist

This includes:
- Blank node edge creation (3 locations)
- Generalization edge creation
- Hierarchy edge transfer

