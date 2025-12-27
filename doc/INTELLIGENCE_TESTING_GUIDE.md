# Intelligence Testing Guide: Growth and Improvement

## Overview

This guide explains how to test the intelligence of the `.m` file system, specifically demonstrating:
1. **How inputs are organized** into a knowledge graph
2. **How the system grows** (nodes, edges, hierarchy) over time
3. **How it improves** (faster learning, better pattern recognition, stronger connections)
4. **What it outputs** (predictions, continuations, associations)

## Key Concept: "Semi-Static Compute"

The system uses the **same computational approach** (wave propagation, edge formation, hierarchy emergence) throughout, but gets **smarter over time** through:
- **Accumulated knowledge**: Each input adds to the graph structure
- **Compounding learning**: Existing knowledge accelerates new learning
- **Emergent organization**: Patterns naturally organize into hierarchies
- **Strengthening connections**: Repeated patterns strengthen edges

## How Inputs Are Processed

### Step 1: Input Reception
```
Input: "cat" (via port 1)
Format: [port_id: 1][data: "cat"]
```

### Step 2: Pattern Processing (`wave_process_sequential_patterns`)
- Breaks input into sequential patterns: `c`, `a`, `t`
- Finds or creates nodes for each pattern
- Activates matching nodes
- Creates new nodes if patterns don't exist

### Step 3: Edge Formation (`wave_form_intelligent_edges`)
- **Co-activation edges**: Nodes that activate together get connected
- **Similarity edges**: Similar patterns get connected
- **Context edges**: Patterns in similar contexts get connected
- **Homeostatic edges**: Prevents node isolation

### Step 4: Wave Propagation (`wave_propagate_multi_step`)
- Activation flows through edges
- Discovers related patterns
- Strengthens existing connections
- Explores graph structure

### Step 5: Hierarchy Formation (Emergent)
- When edges become strong through repetition, hierarchy nodes form
- Example: "cat" + "dog" → "cat dog" (hierarchy node)
- Enables matching larger chunks efficiently

### Step 6: Output Collection (`wave_collect_output`)
- Collects predictions/continuations from activated nodes
- Based on learned sequential patterns
- Outputs what the system "thinks" should come next

## How System Organizes Knowledge

### Level 0: Raw Patterns
- Individual bytes or small patterns
- Example: `c`, `a`, `t`, `d`, `o`, `g`

### Level 1: Combined Patterns
- Sequences that co-occur frequently
- Example: `cat`, `dog`, `hello`, `world`

### Level 2: Meta-Patterns
- Patterns of patterns
- Example: `cat dog` (greeting pattern)

### Level 3+: Abstract Concepts
- Higher-level abstractions
- Emerges naturally from repeated patterns

### Organization Mechanisms:
1. **Hierarchy**: Larger patterns combine smaller ones
2. **Blank Nodes**: Abstract categories learned from connections
3. **Edge Weights**: Strong connections = important relationships
4. **Abstraction Levels**: Tracks how abstract each node is

## Growth Metrics

### Node Growth
- **New nodes**: Created for new patterns
- **Hierarchy nodes**: Formed when patterns combine
- **Blank nodes**: Created for categories/generalizations

### Edge Growth
- **Co-activation edges**: Formed when patterns appear together
- **Similarity edges**: Formed between similar patterns
- **Context edges**: Formed in similar contexts
- **Edge strengthening**: Weights increase with repetition

### Knowledge Compression
- **Hierarchy enables compression**: 1000 patterns → 100 concepts → 10 meta-concepts
- **Storage efficiency**: Less storage needed as abstraction increases
- **Faster matching**: Larger patterns matched first (hierarchy-first)

## Improvement Indicators

### 1. Connection Strength
- **Edge weights increase** with repetition
- Stronger connections = better pattern recognition
- Measured by: `avg_edge_weight` over time

### 2. Learning Speed
- **Faster pattern recognition** as knowledge accumulates
- Existing edges guide exploration
- Measured by: Time to recognize repeated patterns

### 3. Abstraction Depth
- **Deeper hierarchies** = better organization
- More abstract concepts = better generalization
- Measured by: `max_abstraction_level`

### 4. Pattern Completion
- **Better predictions** as patterns strengthen
- System can complete partial patterns
- Measured by: Output quality/accuracy

### 5. Generalization
- **Blank nodes** learn categories
- System recognizes similar patterns
- Measured by: Number of blank nodes and their connections

## What System Outputs

### Output Types:

1. **Pattern Continuations**
   - Input: "cat"
   - Output: Next likely pattern (e.g., "dog" if "cat dog" was learned)

2. **Associations**
   - Input: "hello"
   - Output: Related patterns (e.g., "world" if "hello world" was learned)

3. **No Output (Learning Phase)**
   - When exploring new patterns
   - System is learning, not predicting
   - Output appears as knowledge accumulates

### Output Collection:
- Only from **direct input nodes** and **learned sequential patterns**
- Based on **edge weights** (stronger = more likely)
- Uses **confidence thresholds** (adaptive, data-driven)

## Running the Test

```bash
# Compile
gcc -o test_growth_and_improvement test_growth_and_improvement.c melvin.c melvin_m.c -lm

# Run
./test_growth_and_improvement
```

## Expected Results

### Growth:
- Nodes increase with each input
- Edges increase (connections form)
- Hierarchy nodes emerge
- Blank nodes appear (categories)

### Improvement:
- Edge weights strengthen over time
- Hierarchy depth increases
- Faster pattern recognition
- Better output predictions

### Organization:
- Knowledge organized by abstraction level
- Well-connected nodes (hubs)
- Isolated nodes (new patterns)
- Hierarchical structure emerges

## Key Insights

1. **Same Compute, Smarter System**: The computational approach doesn't change, but the accumulated knowledge makes the system smarter

2. **Compounding Learning**: Each new pattern builds on existing knowledge, accelerating learning

3. **Emergent Organization**: Structure emerges naturally from patterns, not programmed

4. **Adaptive Improvement**: System improves based on what it learns, not hardcoded rules

5. **Output Reflects Learning**: What the system outputs shows what it has learned

## Proving Growth and Improvement

To prove the system grows and improves:

1. **Measure growth**: Track nodes, edges, hierarchy over time
2. **Measure improvement**: Track edge weights, abstraction depth, output quality
3. **Compare snapshots**: Before/after each learning round
4. **Show compounding**: Later rounds should show faster learning
5. **Show organization**: Knowledge should organize into hierarchies

The test demonstrates all of these automatically!

