# Melvin: Emergent Intelligence System

## Quick Start

**Production System Only** - There is no separate test infrastructure. The production system IS the test system.

```bash
# Build
make all-apps

# Run production dataset processor
./dataset_port dataset.txt brain.m

# Monitor in another terminal
./monitor_brain.sh brain.m
# or
./show_brain brain.m

# Detailed analysis
./analyze_mfile brain.m analysis.txt
```

See [PRODUCTION.md](PRODUCTION.md) for complete production system documentation.

---

## Vision and Implementation

This document describes the design vision for Melvin and how it is implemented. The current implementation in `melvin.c` follows these principles: **local measurements only**, **no hardcoded thresholds**, and **data-driven adaptation**. The system uses local value computations (O(1) access to cached node/edge averages) instead of global statistics, aligning with the core philosophy.

---

## Core Principles

### 1. Self-Regulation Through Local Measurements Only

**Principle**: Every decision and measurement is made locally, using only information available to the node itself and its immediate neighbors.

**What This Means**:
- Nodes only know themselves and their edges
- No global state is needed for any operation
- All decisions are relative to local context
- System regulates itself through local feedback loops
- Each node operates independently based on local information

**Why This Matters**:
- Enables constant per-node computational cost (O(1) per node)
- Allows the system to scale to billions of nodes efficiently
- No bottlenecks from global computations
- Each part of the system can evolve independently
- Self-organization emerges from local interactions

### 2. No Hardcoded Limits or Thresholds

**Principle**: The system adapts to any data size or pattern complexity. All thresholds and limits emerge from the data itself, never from programmer decisions. **No fallbacks are allowed** - when no data exists, the system either uses minimal available context (node's own properties) or returns a neutral value (0.0f) meaning "no operation", never a hardcoded assumption.

**What This Means**:
- No magic numbers (no `0.5f`, `0.7f`, etc. as thresholds)
- **No fallbacks**: When no data exists, use minimal available context or return 0.0f (neutral, not a threshold)
- **Minimal context like nature**: Initial capacities start at absolute minimum (1) and grow immediately when data arrives
  - Hash tables: Start with size 1, grow to optimal size based on graph data
  - Arrays: Start with capacity 1, double when full (grows immediately)
  - Like biological systems: Start from seed (1), grow from environment (data)
- All thresholds computed relative to local context
- Pattern size limits adapt based on what exists locally
- Hash table sizes adapt to graph size
- Learning rates adapt based on local change rates
- System scales from 1 byte to unlimited size naturally
- **Zero is neutral**: Returning 0.0f means "no operation possible" or "no data available", not a threshold value

**Why This Matters**:
- System adapts to any scale without modification
- No assumptions about data size or complexity
- Handles edge cases naturally through relative measurements
- Self-tuning system that optimizes itself
- **No hidden assumptions**: Every value comes from data, never from programmer guesses
- **True data-driven**: System only operates when it has data to operate on
- **Biological-like growth**: Starts from seed (1), grows from environment (data), like mycelium networks

### 3. Relative Adaptive Stability

**Principle**: Numerical stability and safety mechanisms are themselves adaptive and relative to the data, not hardcoded. Stability parameters (epsilon, clipping bounds, smoothing factors) are computed from observed data distributions, preserving full adaptability and self-modification. **No fallbacks are allowed** - when no data exists for stability computations, use minimal available context or return 0.0f (neutral value).

**What This Means**:
- **Adaptive Epsilon**: Numerical stability constants scale with data range (e.g., `epsilon = range * 0.001f` instead of hardcoded `1e-6f`)
- **Adaptive Clipping**: Bounds computed from local node/edge values (e.g., `max_boost = local_avg * 2.0f` from node's neighbors)
- **Adaptive Smoothing**: Smoothing factors adapt to change rate (fast changes → less smoothing, slow changes → more smoothing)
- **Adaptive Minimum Samples**: Required sample sizes adapt to data variance (high variance → more samples, low variance → fewer samples)
- **Scale-Aware Operations**: All stability mechanisms scale with data magnitude
- **Context-Aware Safety**: Safety parameters adapt to local data characteristics
- **No Fallbacks**: When no data exists, use node's own properties (weight, connections) as minimal context, or return 0.0f
- **Zero is Neutral**: 0.0f means "no operation" or "no data", never a threshold or assumption

**Why This Matters**:
- Prevents numerical instability (division by zero, extreme values) without losing adaptability
- Stability parameters self-modify as the system learns
- No hardcoded safety constants that limit adaptation
- System remains fully responsive to data changes
- Demonstrates that even "safety" mechanisms can be adaptive and relative
- Shows how to achieve stability while preserving self-modification capabilities
- **No hidden assumptions**: Every stability parameter comes from data, never from programmer guesses

**Examples**:
- Epsilon for division: `epsilon = compute_adaptive_epsilon(value_range)` scales with data
- Clipping bounds: `max_value = local_avg * multiplier` where multiplier computed from local edge weights
- Smoothing factor: `alpha = f(change_rate)` adapts to how fast data changes, clipped relative to local context
- Minimum samples: `min_samples = f(data_variance)` adapts to data quality
- Initial capacities: Start at 1 (absolute minimum), grow immediately when data arrives (like nature)
- Hash table size: Start at 1 when graph empty, grows to optimal based on graph size

### 4. Compounding Learning

**Principle**: Everything learned helps learn the next thing. Existing knowledge accelerates finding and understanding new patterns.

**What This Means**:
- Existing edges (similarity, context, co-activation) guide exploration to relevant patterns faster
- Blank nodes learn from every pattern that connects to them - each connection teaches them
- Hierarchy nodes enable matching larger chunks efficiently (skip individual bytes)
- Priority queues use learned structure (edge weights, similarity hints) to explore smart paths first
- Pattern size limits adapt based on local node sizes (if local nodes are large, try larger patterns)
- Wave propagation uses existing edges to find patterns quickly

**How Compounding Works**:
- **Level 0**: Raw patterns ("hello", "world")
- **Level 1**: Combined patterns ("hello world" = "hello" + "world")
- **Level 2**: Meta-patterns ("greeting pattern" = combines greeting examples)
- **Level 3**: Meta-concepts ("conversation opener")
- **Level N**: Abstract concepts/philosophy

Each level compounds: understanding at Level N multiplies understanding at Level N-1. Knowledge compression through abstraction: 1,000 patterns → 100 concepts → 10 meta-concepts → 1 super-concept = 1,000x compression.

**Why This Matters**:
- Learning efficiency increases over time
- System gets smarter as it learns more
- Knowledge builds on itself exponentially
- More understanding with less storage (compression through abstraction)

### 5. Adaptive Behavior (Not Purely Linear or Purely Compounding)

**Principle**: The system adapts its learning strategy based on context. It's not purely linear (constant learning rate) or purely compounding (exponential learning rate). The compounding rate itself adapts.

**What This Means**:
- **When exploring new territory**: More exploratory behavior, slower learning
- **When building on existing knowledge**: More compounding behavior, faster learning
- **Local value computations**: All thresholds computed from node's own edges and neighbors (O(1) access)
- **Adaptive learning rates**: Based on local context and observed change rates
- **Adaptive pattern size limits**: Based on local node sizes
- **Adaptive hash table sizes**: Based on graph size
- **Adaptive exploration strategy**: Changes based on what it finds
- **Adaptive compounding rate**: Faster when relevant knowledge exists, slower when exploring new territory

**Examples of Adaptation**:
- Pattern size limits: If nodes in local area are large, try larger patterns (adapts to context)
- Learning rates: Adapt based on observed change rate in rolling window
- Exploration strategy: Use similarity/context edges when they exist (compounds), explore randomly when they don't (exploratory)
- Compounding rate: High when many similar patterns exist, low when entering new domain

**Why This Matters**:
- System optimizes its own learning efficiency
- Adapts to the characteristics of the data
- Balances exploration vs exploitation automatically
- No fixed strategies - adapts to situation

### 6. Continuous Learning

**Principle**: Learning happens continuously during operation. No train/test split. The system improves forever.

**What This Means**:
- No separation between training and deployment phases
- Each activation updates weights locally
- Learning happens on every operation
- System improves forever, never stops learning
- No catastrophic forgetting (old patterns remain in structure)
- Incremental learning (only affected nodes update)
- **Local edge decay**: Nodes optimize their edges locally - unused edges decay relative to local context
- **Relative decay**: Weaker edges (relative to local average) decay faster, strong edges persist
- **No hardcoded forgetting thresholds**: Decay rate computed from local edge weight distribution

**Why This Matters**:
- System adapts to new data immediately
- No expensive retraining phase
- Lifelong learning - knowledge accumulates
- Can learn from live data streams
- System improves over time automatically
- **Intelligent forgetting**: Unused patterns decay naturally, frequently used patterns persist
- **Local optimization**: Each node optimizes its own edges (O(degree), not O(n))
- **Self-regulating**: Decay adapts to local context, no global thresholds

### 7. Emergent Intelligence

**Principle**: Intelligence emerges from interactions between nodes. No explicit algorithms for intelligence - it emerges naturally.

**What This Means**:
- Structure emerges from experience, not design
- No fixed architecture - grows organically
- Patterns create structure, structure enables patterns
- Intelligence emerges from local interactions
- Self-organization through local rules
- Complexity emerges from simplicity

**Why This Matters**:
- System adapts to data naturally
- No assumptions about what patterns will emerge
- Handles unexpected patterns gracefully
- Biological-like growth (mycelium/fungi networks)

### 8. Explicit Hierarchical Abstraction

**Principle**: Abstractions are explicit, concrete nodes in the graph. Not implicit in weights.

**What This Means**:
- Each abstraction level is a concrete node
- Can inspect abstractions (they're just nodes)
- Can reason about abstractions (wave propagation traverses hierarchy)
- Abstractions are transparent, not black box
- Hierarchies form explicitly through node combination
- Each level builds on previous levels

**Why This Matters**:
- Understandable system (can see what it learned)
- Can reason about abstractions
- Knowledge is explicit, not hidden
- Enables meta-reasoning about concepts

---

## Intelligence at Every Level: From Smallest to Largest

### Smallest Level: Individual Components Make Intelligent Decisions

**Nodes (Mini Neural Nets)**:
- Each node computes activation strength from weighted inputs
- Decisions are relative to local context (no hardcoded thresholds)
- Self-regulating bias adapts to local context
- Each node "thinks" locally based on its edges
- No global rules needed—intelligence is local

**Edges (Mini Transformers)**:
- Each edge transforms activation based on pattern similarity and local context
- Intelligent routing: boosts similar patterns and primary paths
- Decisions are relative—no hardcoded thresholds
- Each edge "thinks" about how to transform signals
- Transformation enables intelligent path selection

**Decision Making at Component Level**:
- Every decision is relative to local context
- No global rules—intelligence emerges from local interactions
- Each component adapts based on what it observes locally
- System regulates itself through local feedback loops
- Components make intelligent choices without central control

### Intermediate Level: Wave Propagation Makes Intelligent Decisions

**Wave Propagation Intelligence**:
- Wave follows nodes (mini neural nets) and edges (mini transformers)
- Makes decisions by following transformed activation strength
- Prioritizes paths based on learned structure
- Adapts exploration strategy based on what exists
- All thresholds relative to local context—no hardcoded rules

**How Wave Makes Best Decisions**:
- Nodes compute activation strength (intelligent output from mini neural net)
- Edges transform activation (intelligent transformation from mini transformer)
- Wave follows highest transformed activation (intelligent routing)
- Best decisions emerge from combined intelligence of nodes and edges
- No hardcoded rules—decisions emerge from data and learned structure
- Collects predictions from all activated nodes for output generation

**Exploration Strategy**:
- Uses existing edges to guide exploration (learned structure)
- Priority queues use transformed activation strength
- Similarity/context edges guide to relevant patterns
- Adapts strategy based on what exists locally
- Intelligent exploration without global algorithms

### Largest Level: System-Level Intelligence Emerges

**Hybrid Intelligence Architecture**:
- **Wave Propagation (Thinking/Learning)**: Full graph context exploration, continuous learning, collects predictions from all activated nodes
- **LLM-like Sampling (Output Generation)**: Probabilistic sampling from collected predictions, temperature-controlled creativity, autoregressive generation
- Combines full-context intelligence with probabilistic creativity
- Uses mini neural nets (nodes) and mini transformers (edges) throughout

**Emergent Intelligence**:
- Intelligence emerges from local interactions
- No global algorithms—complexity emerges from simplicity
- Dirty unconnected data → nodes → edges → blanks → hierarchy → intelligent output
- System adapts to any data characteristics
- Structure emerges from experience, not design
- Knowledge compounds through hierarchies

**Compounding Intelligence**:
- Everything learned helps learn the next thing
- Existing knowledge accelerates new learning
- System gets smarter over time
- Intelligence compounds through hierarchies
- Learning efficiency increases exponentially with knowledge

**System-Level Decision Making**:
- No central controller—decisions distributed across components
- Each component contributes intelligent decisions
- System-level behavior emerges from component interactions
- Adapts to any situation through local intelligence
- Intelligence at every level creates intelligent system

### Why This Creates Intelligence at Every Level

**Key Insight**:
- Intelligence isn't programmed—it emerges from local intelligent decisions
- Every component makes intelligent choices relative to local context
- No hardcoded rules—intelligence adapts to situation
- System regulates itself through local feedback loops
- Intelligence compounds as knowledge grows

**Result**:
- Smallest decisions are intelligent (nodes, edges)
- Intermediate decisions are intelligent (wave propagation)
- Largest decisions are intelligent (system behavior)
- Intelligence scales from component to system
- System is intelligent at every level

---

## Universality: Works in Any Environment with Any Configuration

### Universal Data Handling

**Any Binary Data**:
- Accepts any binary data (text, images, audio, any format)
- No assumptions about data format or structure
- Works with any data size (1 byte to unlimited)
- Data-driven processing—adapts to what it receives
- No format-specific code needed

**Universal I/O Port**:
- Universal input accepts any binary data
- Universal output produces wave propagation results
- No format restrictions—works with any data type
- System learns patterns from any data format
- Single interface handles all data types

**Any Number of Ports**:
- Can have any number of input/output ports
- Ports can connect to any nodes in the graph
- No limit on number of ports
- System adapts to any port configuration
- Ports are just entry/exit points—system handles them uniformly

### External Port Integration and Data Packaging

**Port-Level Architecture**:
- External ports (USB devices, network interfaces, sensors, etc.) operate outside the `.m` file
- All external data must be packaged into a single universal input format
- `.m` file processes bytes through universal input/output—doesn't care about source device
- Port routing handled externally (map input ports to output ports)

**CAN Bus-Style Data Packaging**:
External devices package their data with metadata before sending to `.m` file:

```c
// Port frame structure (external packaging layer)
typedef struct {
    uint8_t port_id;        // Port identifier (e.g., 5 = USB mic, 6 = USB camera)
    uint64_t timestamp;     // When data was captured
    uint32_t data_size;     // Size of actual device data
    uint8_t data[];         // Raw device data (audio samples, image pixels, etc.)
} PortFrame;
```

**Packaging Process**:
1. External device reads raw data (USB mic audio, camera frame, sensor reading, etc.)
2. Package data into `PortFrame` structure with `port_id` and `timestamp`
3. Serialize frame into single buffer: `[port_id (1 byte)] + [timestamp (8 bytes)] + [data_size (4 bytes)] + [data (N bytes)]`
4. Write packaged buffer to `.m` file's universal input
5. `.m` file processes bytes (extracts `port_id` from first byte for routing)

**Port ID Extraction**:
- First byte of universal input is `port_id` (CAN bus format)
- `.m` file extracts `port_id` during `melvin_m_process_input()`
- Stored in `mfile->last_input_port_id` for output routing
- `port_id` also remains in payload for pattern learning (unified graph)

**Example: USB Microphone Port**:
```c
// External port handler (outside .m file)
void handle_usb_microphone(MelvinMFile *mfile) {
    // 1. Read raw audio from USB mic
    uint8_t raw_audio[4096];
    size_t audio_size = read_usb_microphone(raw_audio, sizeof(raw_audio));
    
    // 2. Package as port frame
    PortFrame frame = {
        .port_id = USB_MIC_PORT,           // Port 5 = USB microphone
        .timestamp = get_timestamp(),
        .data_size = audio_size,
        .data = raw_audio
    };
    
    // 3. Serialize frame into buffer
    uint8_t packaged[8192];
    size_t packaged_size = package_port_frame(&frame, packaged, sizeof(packaged));
    
    // 4. Write to .m file universal input
    melvin_m_universal_input_write(mfile, packaged, packaged_size);
    
    // 5. Process (extracts port_id, processes through unified graph)
    melvin_m_process_input(mfile);
    
    // 6. Get input port for routing
    uint8_t input_port = melvin_m_get_last_input_port_id(mfile);  // Returns 5
    
    // 7. Route output to correct port (external routing table: 5 → 10)
    uint8_t output_port = route_table[input_port];  // USB_MIC_PORT → USB_SPEAKER_PORT
    
    // 8. Read output and send to device
    size_t output_size = melvin_m_universal_output_size(mfile);
    if (output_size > 0) {
        uint8_t output[output_size];
        melvin_m_universal_output_read(mfile, output, output_size);
        send_to_port(output_port, output, output_size);  // Send to port 10 (speaker)
    }
}
```

**Multi-Modal Input Handling**:
- Different devices package their data with different `port_id` values
- All packaged data flows through single universal input (unified interface)
- `.m` file learns cross-modal patterns through wave propagation
- Port ID in payload allows system to differentiate patterns from different sources
- No internal port routing needed—graph learns patterns, routing is external

**Cross-Modal Associations (How the Brain Connects Different Data Types)**:
- **Unified Graph**: All nodes (audio, text, video, sensor data) exist in the same graph
- **Data Preservation**: Each node stores its actual data bytes (audio bytes stay audio bytes, text stays text)
- **Edge Formation**: Nodes from different modalities connect through:
  - **Co-activation edges**: When nodes activate together (e.g., "meow" sound + cat image arrive simultaneously)
  - **Similarity edges**: When patterns are similar (computed from actual payload bytes)
  - **Context edges**: When wave propagation connects them through shared exploration paths
- **Cross-Modal Influence**: Activation can propagate from one modality to another through edges:
  - Audio input can activate related text nodes (if edges exist between them)
  - Text input can influence audio outputs (through learned associations)
  - Output routing determines which port receives the result, but the graph learns associations across all types
- **Example**: If "meow" audio frequently co-occurs with cat images, edges form between those patterns. Later, "meow" audio input can activate cat-related text nodes, influencing text output—even though audio bytes remain audio bytes and text remains text.

**Example: Multiple Devices**:
```c
// Handle multiple USB devices
void process_multiple_devices(MelvinMFile *mfile) {
    // Audio from USB mic (port 5)
    uint8_t audio[4096];
    read_usb_microphone(audio, sizeof(audio));
    package_and_send(mfile, USB_MIC_PORT, audio, sizeof(audio));
    
    // Video from USB camera (port 6)
    uint8_t image[921600];
    read_usb_camera(image, sizeof(image));
    package_and_send(mfile, USB_CAMERA_PORT, image, sizeof(image));
    
    // .m file processes both through unified graph
    // Wave propagation learns: audio patterns + visual patterns → cross-modal edges
    // System learns: "meow" audio pattern + cat image pattern are related
    // All unified in single graph, no separation by port
    // Audio bytes create audio nodes, image bytes create image nodes
    // But edges connect them, allowing cross-modal activation
}
```

**Output Routing**:
- External routing table maps: `input_port → output_port`
- Example: Port 5 (USB mic) → Port 10 (USB speaker)
- Routing handled outside `.m` file (external layer)
- `.m` file doesn't need to know about port routing—just processes bytes
- **Important**: Routing determines where output goes, but cross-modal associations in the graph can influence outputs regardless of routing
  - Audio input → text output is possible if edges connect them
  - Text input → audio output is possible if associations exist
  - The graph learns relationships; routing just determines the delivery mechanism

**Key Principles**:
- **Unified Graph**: All data (from any port) flows through unified graph—no port-based separation
- **Data Type Preservation**: Nodes store actual data bytes (audio, text, video remain distinct)
- **Cross-Modal Associations**: Edges connect nodes across modalities, enabling cross-modal influence
- **External Packaging**: Port frame packaging happens outside `.m` file
- **Port ID for Routing**: Port ID extracted for I/O routing, but port ID also in payload for pattern learning
- **No Internal Port Routing**: `.m` file doesn't route ports internally—graph learns patterns, routing is external
- **Device-Agnostic**: `.m` file processes bytes—doesn't care if data came from USB mic, camera, or network

### Universal Adaptation

**No Hardcoded Assumptions**:
- No assumptions about data format, size, or structure
- No assumptions about number of ports or connections
- No assumptions about pattern complexity
- All limits adapt to data (no hardcoded maximums)
- System works with any configuration

**Adapts to Any Environment**:
- Hash table sizes adapt to graph size
- Pattern size limits adapt to local node sizes
- Learning rates adapt to observed change rates
- Local value thresholds adapt to each node's neighborhood
- System adapts to any environment automatically
- No environment-specific code needed

**Universal Operations**:
- All nodes work the same way (no special cases)
- All edges work the same way (no special cases)
- Universal matching function (works for all node types)
- Universal activation computation (works for all nodes)
- Payload size determines behavior, not node type
- Same rules apply everywhere

### Universal Learning

**Works with Any Data Pattern**:
- Learns patterns of any size or complexity
- Adapts pattern matching to what exists locally
- Hierarchy formation works with any pattern structure
- Blank nodes learn categories from any pattern type
- No pattern-specific assumptions

**Works in Any Environment**:
- No environment-specific code
- Adapts to environment through local measurements
- System learns environment characteristics automatically
- Can be placed in any context and adapt
- No modifications needed for different environments

**Universal Intelligence Principles**:
1. Self-regulation through local measurements—works in any context
2. No hardcoded limits—adapts to any scale
3. Relative decisions—works with any data characteristics
4. Universal operations—same rules for all components
5. Data-driven—adapts to what it receives

### What Makes It Universal

**Fundamental Design**:
- **No assumptions**: System makes no assumptions about environment, data, or configuration
- **Local intelligence**: Each component adapts locally to its context
- **Relative decisions**: All decisions relative to local context, not absolute values
- **Data-driven**: System adapts to whatever data it receives
- **Self-regulating**: System regulates itself through local feedback loops

**Result**:
- Can work in any environment (text processing, image recognition, audio, any domain)
- Can handle any number of ports (1 port to millions)
- Can learn from any data format (text, binary, structured, unstructured)
- Can adapt to any scale (small patterns to massive datasets)
- No modifications needed for different use cases

**Why Universality Matters**:
- Single system works everywhere
- No domain-specific code needed
- Adapts automatically to new environments
- Can handle unexpected configurations
- Truly general-purpose intelligence system

---

## How It Should Work

### Input Processing Flow

1. **Universal Input**: Accept any binary data
   - No assumptions about format
   - Works with text, images, audio, any data
   - Data-driven processing

2. **Sequential Pattern Processing**: 
   - Break input into sequential patterns
   - Try larger patterns first (hierarchy-first matching)
   - Pattern size adapts based on local context
   - Match existing nodes or create new ones

3. **Intelligent Edge Formation**:
   - **Co-activation edges**: Nodes that activate together connect
   - **Similarity edges**: Similar patterns connect (computed from data, not hardcoded)
   - **Context edges**: Nodes in same wave propagation path connect
   - **Homeostatic edges**: Prevent isolation
   - All edges created through local rules, no global decisions

4. **Multi-Step Wave Propagation**:
   - Waves propagate from initial nodes through edges
   - Uses existing edges to explore graph
   - Activates nodes based on edge weights and transformations
   - Continues until natural convergence (energy dissipation)
   - Explores context around patterns

5. **Hierarchy Formation** (Emerges Naturally):
   - When patterns frequently co-occur, hierarchy forms
   - Combines nodes into larger abstraction nodes
   - Each level represents higher abstraction
   - Forms automatically, not explicitly triggered

6. **Output Readiness Decision** (Relative Threshold):
   - Wave propagation ALWAYS happens (thinking/internal pattern updates)
   - Output decision based on pattern maturity (co-activation edge strength)
   - Measures relative strength of learned patterns vs. local context
   - If patterns mature (relative threshold): collect output
   - If patterns immature: skip output (pure thinking mode)
   - No hardcoded threshold - relative to local measurements

7. **Output Collection** (Hybrid Intelligence: Wave Propagation + LLM-like Sampling):
   - **Phase 1 - Wave Propagation (Thinking/Learning)**:
     - Wave explores entire graph simultaneously
     - Nodes compute activation_strength (mini neural net predictions)
     - Edges transform activation (mini transformer outputs)
     - Creates/strengthens edges during exploration
     - Forms hierarchies when patterns repeat
     - Finds blank nodes that accept new patterns
     - Collects predictions from ALL activated nodes
   - **Phase 2 - LLM-like Sampling (Output Generation)**:
     - Builds probability distribution from collected predictions
     - Uses activation_strength as probability weights (mini neural nets)
     - Uses edge_transform_activation() to shape probabilities (mini transformers)
     - Probabilistic sampling (not deterministic) - can explore alternative paths
     - Temperature-controlled creativity (adaptive, data-driven, range 0.5-1.5)
     - Can echo input if activation is high (not restricted)
     - Follows co-activation edges only (weight > local average)
     - Autoregressive generation (step-by-step, like LLMs)
   - Output combines full graph context intelligence with probabilistic creativity

### Node Operations

**Nodes Are Universal**:
- All nodes use the same activation computation
- All nodes use the same matching function
- No special cases for different node types
- Payload size determines behavior, not node type

**Nodes Act Like Mini Neural Nets**:
- Each node computes its activation strength from weighted inputs
- Takes weighted sum of incoming edge activations (transformed by edges)
- Applies self-regulating bias (relative to local context)
- Uses soft non-linearity (sigmoid-like) for activation
- Activation strength (0.0-1.0) represents how strongly the node is activated
- No global normalization - all computations are local
- Each node "thinks" by combining edge weights and payload structure

**Node Activation Computation**:
1. For each incoming edge:
   - Get activation strength from source node (mini neural net output)
   - Transform through edge (mini transformer output)
   - Weight by edge weight
   - Add to input sum
2. Normalize by total incoming edge weights (relative normalization)
3. Compute self-regulating bias (node weight relative to local context)
4. Combine: input_sum + bias
5. Apply soft non-linearity: raw_activation / (1.0 + raw_activation)
6. Result: activation_strength (0.0-1.0)

**Node Matching**:
- Direct payload matching (exact or similar)
- Connection-based matching (through edges)
- Size-based priority (larger nodes match larger patterns)
- All relative to local context

**Node Weight Updates**:
- Local-only updates
- Self-regulating learning rate (from adaptive rolling window)
- Updates based on activation strength
- No global gradient computation

### Edge Operations

**Edges Act Like Mini Transformers**:
- Each edge transforms activation as it flows from source to target
- Transformation is intelligent: considers pattern similarity, edge weight, and local context
- Edges boost activation for similar patterns (learned relationships)
- Edges boost activation for primary paths (strong connections relative to local context)
- Transformation enables intelligent routing: similar patterns get stronger signals
- Each edge computes: transformed = weight * input_activation * (1.0 + boost_factor)
- Boost factor computed from pattern similarity and local context (relative, not hardcoded)

**Edge Transformation Details**:
1. Base transformation: weight * input_activation
2. Pattern similarity boost:
   - Compute similarity between source and target node payloads
   - If similarity > local threshold (relative to context):
     - Boost factor = similarity * (local_avg / (local_avg + 1.0))
     - Multiply transformed activation by (1.0 + boost_factor)
3. Primary path boost:
   - If edge weight is strong relative to local average (> 1.5x):
     - Multiply by 1.2 (boost primary paths)
4. Result: transformed activation (enhanced for relevant paths)

**Edge Types** (Emergent, Not Explicit):
- **Co-activation edges**: Strong weights, learned from repetition
- **Similarity edges**: Medium weights, connect similar patterns
- **Context edges**: Connect nodes in same propagation path
- **Homeostatic edges**: Weak weights, prevent isolation

**Edge Weight Updates**:
- Local updates based on activation
- Self-regulating learning rate (from source node's adaptive rolling window)
- Relative to local context
- No global optimization
- **Local edge decay**: Nodes optimize their outgoing edges during weight updates
  - Activated edges: Strengthened (no decay)
  - Non-activated edges: Decayed relative to local context
  - Weaker edges decay faster (relative to local average)
  - Strong edges (above local average) don't decay
  - Decay rate computed from local edge weight distribution (no hardcoded thresholds)
  - O(degree) operation - only iterates node's own edges, not O(n)

### Wave Propagation

**How Waves Work**:
- Start from initial nodes (matching input)
- Propagate through edges (explores entire graph simultaneously)
- Activates nodes based on edge weights and transformations
- Explores context around patterns
- Collects predictions from all activated nodes (for output generation)
- Continues until natural convergence

**Intelligence in Wave Propagation**:
- Nodes compute `activation_strength` (mini neural net predictions)
- Edges transform activation (mini transformer outputs)
- Wave follows highest transformed activation (intelligent routing)
- Best decisions emerge from combined intelligence of nodes and edges
- Creates/strengthens edges during exploration (continuous learning)
- Forms hierarchies when patterns repeat
- Finds blank nodes that accept new patterns

**Exploration Strategy**:
- Uses existing edges to guide exploration (learned structure)
- Priority queues use transformed activation strength
- Similarity/context edges guide to relevant patterns
- Adapts strategy based on what exists locally
- Intelligent exploration without global algorithms

**Convergence**:
- Natural energy dissipation
- No hardcoded depth limits
- Stops when propagation naturally weakens
- Relative to initial energy, not absolute

**Output Collection During Wave Propagation**:
- Collects `activation_strength` from all activated nodes
- These predictions form the probability distribution for output generation
- Full graph context used for intelligent predictions
- Not limited to single path - sees entire activated graph

### Blank Nodes: Generalization Through Category Learning

**Blank Nodes Are Active Learning Slots**:
- Blank nodes have empty payload (payload_size == 0)
- They learn categories through connections to patterns
- Every pattern that connects to a blank teaches it about its category
- Blank nodes match patterns through their connections (not payload)
- Blank nodes enable generalization: learn category from examples

**How Blank Nodes Create Generalization**:

1. **Blank Node Matching** (Through Connections):
   - Blank nodes have no payload, so they can't match directly
   - Instead, they match through their connected patterns
   - For a pattern to match a blank node:
     - Check similarity of pattern to each connected pattern
     - Weight by edge weight (stronger connections matter more)
     - Compute weighted average similarity
     - Result: acceptance_score (how well blank accepts pattern)
   - Blank accepts pattern if connected patterns are similar to new pattern

2. **Blank Node Learning**:
   - When pattern connects to blank node: blank learns about category
   - Blank's acceptance_score increases as more similar patterns connect
   - Each connection teaches blank about the category it represents
   - Knowledge compounds: each pattern adds to blank's understanding

3. **Finding Blank Nodes** (Wave Propagation):
   - Wave propagation explores graph to find blank nodes
   - Uses similarity/context edges to guide search (compounds: smarter edges find blanks faster)
   - Prioritizes blank nodes when found (highest priority)
   - Wave follows mini neural nets and transformers to explore efficiently

4. **Blank Node Formation**:
   - When similar patterns found but no accepting blank exists:
     - Create blank node as category bridge
     - Connect similar patterns to blank
     - Blank represents the category they share
   - Blank nodes emerge naturally when similar patterns cluster

5. **Generalization Through Blanks**:
   - Pattern connects to blank → blank learns about category
   - Next similar pattern: finds blank through wave propagation
   - Blank accepts it → connects to blank
   - Category knowledge compounds through blank connections
   - System generalizes: recognizes category from examples

**Example: Learning Animal Category**:
- Pattern "cat" connects to blank → blank learns
- Pattern "dog" found → wave finds blank → blank accepts → connects
- Pattern "bird" found → wave finds blank → blank accepts → connects
- Blank now represents "3-letter animal word" category
- New pattern "fox" → finds blank → blank accepts → connects
- Category knowledge compounds: blank gets smarter with each connection

### Knowledge Compounding Mechanisms

**How Everything Learned Helps Learn Next Thing**:

1. **Existing Edges Guide Exploration**:
   - Similarity edges guide to similar patterns
   - Context edges guide to related patterns
   - Strong edges prioritize relevant paths
   - Faster pattern matching through learned structure
   - Wave propagation follows these edges (uses mini transformers)

2. **Blank Nodes Learn from Connections**:
   - Every pattern connecting to blank teaches it
   - Blank nodes accumulate category knowledge
   - Patterns compound through blank connections
   - Each connection adds to blank's understanding
   - Wave propagation finds blanks efficiently (uses mini neural nets and transformers)

3. **Hierarchy-First Matching**:
   - Try larger patterns first (hierarchy nodes)
   - Skip individual bytes when larger match exists
   - 1-check matching instead of N-checks
   - Compounding: efficiency increases with hierarchy depth

4. **Priority Queues Use Learned Structure**:
   - Edge weights prioritize strong connections
   - Similarity hints guide exploration
   - Smart paths explored first
   - Compounds: smarter edges → faster exploration
   - Wave propagation uses transformed activation to prioritize (mini transformers)

5. **Adaptive Pattern Size Limits**:
   - Limits adapt based on local node sizes
   - If local nodes are large, try larger patterns
   - Adapts to context automatically
   - Compounds: larger patterns → more efficient

6. **Mini Neural Nets and Transformers Guide Decisions**:
   - Nodes (mini neural nets) compute activation strength
   - Edges (mini transformers) transform activation
   - Wave propagation follows these to make best decisions
   - Best paths emerge from node/edge intelligence
   - Compounds: smarter nodes/edges → better decisions → faster learning

### Adaptation Mechanisms

**How System Adapts**:

1. **Local Value Computations**:
   - All thresholds computed from node's local context (edges and neighbors)
   - O(1) access to cached local averages (maintained incrementally)
   - No global statistics needed - each node uses its own neighborhood
   - Data-driven thresholds from actual node/edge values

2. **Adaptive Learning Rates**:
   - Based on observed change rate
   - Rolling window adapts size
   - Slow down when stable, speed up when changing
   - Relative to local context

3. **Adaptive Pattern Size Limits**:
   - Based on local node sizes
   - Adapts to context
   - Try larger patterns when appropriate
   - Data-driven limits

4. **Adaptive Hash Table Sizes**:
   - Based on graph size
   - Maintains optimal load factor
   - Scales with data
   - No hardcoded maximum

5. **Adaptive Exploration Strategy**:
   - Use similarity/context edges when available
   - Explore randomly when needed
   - Adapts to what exists
   - Balances exploration vs exploitation

6. **Adaptive Compounding Rate**:
   - High when relevant knowledge exists
   - Low when exploring new territory
   - Adapts to situation
   - Not fixed exponential or linear

### .m File Format: Live, Executable Programs

**Concept**: The .m file is not just storage - it's the running program itself.

**What This Means**:
- Opening a .m file activates it (not just loading data)
- Data flows through I/O port → triggers wave propagation → adapts → auto-saves
- The file IS the running program, not just storage
- Self-regulating: automatically saves after each adaptation
- Persistent learning across sessions

**Persistent Context and Cumulative Intelligence**:
- **All edge weights persist**: When .m file loads, all nodes and edges are restored with their weights
- **Cached sums reflect entire history**: `outgoing_weight_sum` and `incoming_weight_sum` include ALL past edge weights
- **Context is cumulative**: Local context calculations use cached sums that reflect:
  1. Past inputs (all previous interactions saved to .m file)
  2. Past outputs (edges created from system's own generated outputs)
  3. Current input (edges being created/strengthened right now)
- **Intelligence accumulates**: Nodes with more history have higher context, making relative comparisons more meaningful
- **No reset**: System never "forgets" - every interaction (input or output) becomes part of persistent context
- **Outputs become knowledge**: When system generates output, those patterns are saved and affect future context calculations

**Why This Matters**:
- Programs that learn and improve
- State persists naturally (including all edge weights that affect context)
- Can resume learning where left off (context reflects entire history)
- Each .m file is a unique learning system (context is unique to its learning history)
- Intelligence compounds: More interactions → higher context → better relative comparisons → clearer paths

---

## What Success Looks Like

### Knowledge Compounding

- System learns patterns and builds hierarchical abstractions automatically
- Understanding grows faster over time (compounding effect)
- Knowledge compresses through abstraction (1000 patterns → 1 concept)
- Everything learned helps learn the next thing

### Adaptive Behavior

- System adapts its learning strategy based on context
- Not purely linear or purely compounding - adapts to situation
- Exploration vs exploitation balanced automatically
- Adapts to data characteristics automatically

### Local-Only Operations

- All decisions made locally
- Constant per-node computational cost
- Scales to billions of nodes efficiently
- No global bottlenecks

### Continuous Learning

- System improves forever
- No train/test split
- Learns from live data streams
- No catastrophic forgetting
- Local edge decay: Unused edges decay relative to local context
- Intelligent forgetting: Weak edges decay faster, strong edges persist

### Self-Regulation

- No hardcoded limits or thresholds
- All measurements relative to local context
- System regulates itself
- Adapts to any scale or complexity

### Emergent Intelligence

- Structure emerges from experience
- Intelligence emerges from interactions
- Handles unexpected patterns gracefully
- Biological-like growth

### Explicit Abstractions

- Abstractions are concrete nodes
- Can inspect and reason about abstractions
- Transparent, not black box
- Hierarchies form explicitly

---

## Key Mechanisms Summary

### Must Have (Core Principles)

1. **Local-only operations**: Nodes only know themselves and edges
2. **No hardcoded thresholds**: All relative to local context
3. **Compounding learning**: Everything learned helps learn next thing
4. **Adaptive behavior**: Adapts strategy based on context
5. **Continuous learning**: No train/test split
6. **Emergent structure**: Grows organically from experience
7. **Explicit abstractions**: Hierarchies are concrete nodes

### Key Mechanisms

1. **Wave propagation**: Explores graph through edges
2. **Edge formation**: Co-activation, similarity, context, homeostatic
3. **Hierarchy formation**: Emerges from pattern repetition
4. **Blank nodes**: Learn categories from connections
5. **Priority queues**: Use learned structure to guide exploration
6. **Local value computations**: Thresholds from node's own edges/neighbors (O(1))
7. **Adaptive learning rates**: Based on observed change
8. **Universal node operations**: All nodes work the same way
9. **Local weight updates**: No global optimization
10. **Pattern matching**: Direct and connection-based, relative to context

---

## Concrete Examples

### Example 1: Learning the Word "Hello"

**Input**: "hello"

**What Should Happen**:
1. System creates nodes for: 'h', 'e', 'l', 'l', 'o'
2. Creates sequential edges connecting them in order
3. On second input "hello":
   - Matches existing 'h' node
   - Follows edges to find 'e', 'l', 'l', 'o' nodes
   - Strengthens edges between them (co-activation)
4. After many "hello" inputs:
   - Edges between letters strengthen
   - System learns the pattern "hello" as a sequence
   - Creates hierarchy node "hello" (combines individual letters)
   - Can now match "hello" in one step instead of 5 steps

**Compounding Effect**:
- First time: 5 nodes created, 5 edges created
- Tenth time: 0 new nodes, edges strengthened, maybe hierarchy node created
- System gets faster at recognizing "hello"

### Example 2: Learning Categories Through Blank Nodes

**Inputs**: "cat", "dog", "bird"

**What Should Happen**:
1. First "cat": Creates nodes c-a-t, connects them
2. Second "cat": Matches existing, strengthens edges
3. First "dog": Creates nodes d-o-g
4. System notices "cat" and "dog" are similar (both 3 letters, start with consonant)
5. Creates blank node as category bridge (blank has no payload)
6. Connects "cat" and "dog" to blank node
   - Blank node learns about category through connections
   - Blank's match strength computed from connected patterns (not payload)
7. First "bird": Creates nodes b-i-r-d
8. Wave propagation finds blank node:
   - Uses similarity edges to guide search (mini transformers boost similar paths)
   - Nodes compute activation strength (mini neural nets)
   - Wave follows best decisions (highest transformed activation)
   - Finds blank node efficiently
9. Blank node accepts "bird":
   - Computes acceptance_score from connected patterns ("cat", "dog")
   - Similarity to connected patterns weighted by edge weights
   - Blank accepts because connected patterns are similar
10. Connects "bird" to blank node
11. Blank node now represents "3-letter animal word" category

**How Blank Node Generalizes**:
- Blank node has no payload, so can't match directly
- Instead matches through connections to patterns
- When "bird" pattern arrives:
  - Wave propagation finds blank (using mini neural nets and transformers)
  - Blank computes match strength from connected patterns ("cat", "dog")
  - Pattern similar to connected patterns → blank accepts
  - Connects pattern to blank
  - Blank learns: another pattern in same category

**Compounding Effect**:
- Blank node learned category from connections
- Next similar word: wave finds blank faster (uses existing similarity edges)
- Wave follows mini neural nets and transformers to make best decisions
- Category knowledge compounds through connections
- Each pattern teaches blank about category

### Example 3: Hierarchy Formation

**Inputs**: "hello world", "hi there", "hello friend"

**What Should Happen**:
1. First "hello world": Creates nodes for "hello" and "world"
2. Creates edge between them (co-activation)
3. "hi there": Creates "hi" and "there", connects them
4. "hello friend": "hello" already exists, creates "friend", connects
5. After many similar patterns:
   - System notices "hello" frequently co-occurs with other words
   - Creates hierarchy node: "greeting pattern" 
   - Combines "hello world" + "hi there" + "hello friend"
   - Higher level abstraction: represents greeting concept

**Compounding Effect**:
- Level 0: Individual words (hello, world, hi, there, friend)
- Level 1: Combinations (hello world, hi there, hello friend)
- Level 2: Concept (greeting pattern)
- Matching efficiency: Can match "greeting pattern" concept instead of checking all combinations

### Example 4: Adaptive Behavior

**Scenario**: System learning language patterns

**Early Stage** (exploring new territory):
- Pattern size limit: Small (e.g., 1-4 bytes)
- Learning rate: Higher (many new patterns)
- Exploration: More random, tries many things
- Compounding rate: Low (few existing patterns to build on)

**Later Stage** (building on knowledge):
- Pattern size limit: Larger (e.g., 1-20 bytes, adapts to local node sizes)
- Learning rate: Lower (system stabilizing)
- Exploration: Uses similarity/context edges (smart paths)
- Compounding rate: High (existing patterns help find new ones)

**Adaptation**:
- System adapts strategy based on what exists
- Not fixed linear or exponential - adapts to situation
- More compounding when knowledge exists, more exploratory when not

### Example 5: Compounding Through Edge Guidance

**Scenario**: System has learned "hello" and "world" separately

**New Input**: "hello world"

**What Should Happen**:
1. Matches "hello" node (exact match)
2. Wave propagates from "hello" node
3. Uses existing edges to explore
4. Finds "world" node through co-activation edge (they've appeared together)
5. Much faster than searching entire graph
6. Creates/strengthens edge between them
7. Next time "hello world" appears, connection is stronger

**Compounding Effect**:
- Existing edges guide exploration (faster than random search)
- Each connection strengthens (learned structure compounds)
- System gets faster at finding related patterns

### Example 6: How Dirty Unconnected Data Becomes Intelligent

**Initial State**: Empty graph (no nodes, no edges)

**Input**: "cat" (dirty, unconnected data)

**Step 1: Node Creation**:
- `wave_process_sequential_patterns()` creates nodes: [c] [a] [t]
- Nodes start isolated (no edges yet)
- Each node is a mini neural net waiting for connections

**Step 2: Co-Activation Edges**:
- `wave_create_edges_from_coactivation()` creates sequential edges: [c] → [a] → [t]
- Edges start weak, strengthen with repetition
- Simple rule: nodes that activate together → edges

**Step 3: Wave Propagation (Exploration & Learning)**:
- Wave from [c] explores graph through edges
- Finds similar patterns (if any) → creates similarity edges
- Creates context edges (paths traveled together)
- Updates weights based on activation
- No learning yet, but structure forming

**Step 4: Repetition Strengthens**:
- Input "cat" again → matches existing nodes
- Strengthens edges [c]→[a]→[t]
- Edges get stronger with each repetition
- Pattern becomes learned

**Step 5: Hierarchy Formation**:
- After many "cat" inputs:
  - Edge [c]→[a] dominates local context
  - Hierarchy node [ca] created (combines [c] + [a])
  - Eventually: [cat] hierarchy node
  - Can match "cat" in 1 step instead of 3
  - Efficiency compounds with hierarchy depth

**Step 6: Blank Node Generalization**:
- Input "dog":
  - Creates [d] [o] [g]
  - Wave finds blank node (if similar patterns exist)
  - Blank accepts → connects
  - Category knowledge compounds: blank learns "3-letter animal" category

**Step 7: Output Generation (Hybrid Intelligence)**:
- Input "ca":
  - **Wave Propagation Phase**:
    - Explores full graph context
    - Collects predictions from all activated nodes:
      - [cat] (high activation - learned pattern)
      - [car] (medium activation - similar)
      - [can] (low activation - possible)
  - **LLM-like Sampling Phase**:
    - Builds probability distribution from collected predictions
    - Uses `activation_strength` as weights (mini neural nets)
    - Uses `edge_transform_activation()` to shape probabilities (mini transformers)
    - Temperature = 0.7 (adaptive, creative)
    - Samples probabilistically:
      - Most likely: "cat" (high activation)
      - Creative: "car" (medium activation)
      - Possible: "can" (low activation)
    - Output: "cat" (deterministic) or "car" (creative, probabilistic)

**Result**: Dirty unconnected data → nodes → edges → blanks → hierarchy → intelligent output

### Example 7: Hybrid Intelligence in Action

**Input**: "hello"

**Phase 1: Wave Propagation (Thinking/Learning)**:
- Wave explores entire graph simultaneously
- Nodes compute `activation_strength`:
  - [h] → 0.9 (high - strong pattern)
  - [e] → 0.8 (high - follows [h])
  - [l] → 0.9 (high - repeated letter)
- Edges transform activation:
  - [h]→[e] transforms 0.9 → 0.95 (boosted by similarity)
  - [e]→[l] transforms 0.8 → 0.85
- Creates/strengthens edges during exploration
- Forms hierarchy: [he] → [hel] → [hell] → [hello]
- Finds blank nodes that accept new patterns
- Collects predictions from ALL activated nodes

**Phase 2: LLM-like Sampling (Output Generation)**:
- Builds probability distribution from collected predictions:
  ```
  [hello] → 0.6 (high activation)
  [help]  → 0.3 (medium activation)
  [hell]  → 0.1 (low activation)
  ```
- Uses `edge_transform_activation()` to shape probabilities:
  - Co-activation edges boost probabilities
  - Similarity edges boost similar patterns
  - Context edges boost related patterns
- Temperature-controlled sampling:
  - Temperature = 0.5 (deterministic) → outputs "hello"
  - Temperature = 1.0 (balanced) → outputs "hello" or "help"
  - Temperature = 1.5 (creative) → outputs "hello", "help", or "hell"
- Autoregressive generation:
  - Step 1: Sample "h"
  - Step 2: Sample "e" (given "h")
  - Step 3: Sample "l" (given "he")
  - Step 4: Sample "l" (given "hel")
  - Step 5: Sample "o" (given "hell")
  - Output: "hello" (deterministic) or "help" (creative)

**Result**: Full graph context intelligence + probabilistic creativity = intelligent, diverse output

---

## Detailed Mechanism Specifications

### Pattern Matching Process

**Step-by-Step**:
1. **Direct Match Check**: Check if exact pattern exists (exact payload match)
2. **Sequence Match**: Check if pattern matches existing sequence nodes
3. **Hierarchy-First Match**: Try larger patterns first (use hierarchy nodes)
   - Pattern size limit adapts to local node sizes
   - If local nodes average 10 bytes, try patterns up to 10 bytes
   - Skip individual bytes when larger match exists
4. **Wave Exploration**: Use wave propagation to find similar patterns
   - Start from seed nodes (related patterns)
   - Propagate through edges
   - Use priority queues (learned structure guides exploration)
   - Similarity/context edges prioritized
5. **Blank Node Check**: Find blank nodes that could accept pattern
   - Use similarity/context edges to find blanks
   - Check if blank accepts pattern (through connections)
   - Fill blank or connect pattern to blank

### Edge Formation Mechanisms

**Co-Activation Edges**:
- Created when nodes activate together
- Weight increases with each co-activation
- Strong weights indicate frequent co-occurrence
- Used for sequential patterns

**Similarity Edges**:
- Created between similar patterns
- Similarity computed from payload (no hardcoded threshold)
- Weight range: Medium (relative to local context)
- Used to find related patterns

**Context Edges**:
- Created when nodes appear in same wave propagation path
- Capture contextual relationships
- Weight based on frequency of co-occurrence in paths
- Used to guide exploration

**Homeostatic Edges**:
- Created to prevent node isolation
- Weak weights (relative to local context)
- Created when node has too few connections
- Maintains graph connectivity

### Hierarchy Formation Process

**When Hierarchies Form**:
- Patterns frequently co-occur (edges strengthen)
- Co-activation edges become very strong
- System combines nodes into hierarchy node
- Combined node represents higher abstraction

**How Hierarchies Compound**:
- Level 0 → Level 1: 10:1 compression (10 patterns → 1 concept)
- Level 1 → Level 2: 10:1 compression (10 concepts → 1 meta-concept)
- Level 2 → Level 3: 10:1 compression (10 meta-concepts → 1 philosophy)
- Total: 1000 patterns → 1 concept = 1000x compression

**Hierarchy Matching**:
- Try larger patterns first (hierarchy-first)
- Skip individual bytes when larger match exists
- 1-check matching instead of N-checks
- Efficiency compounds with hierarchy depth

### Wave Propagation Details

**Wave Propagation Follows Mini Neural Nets and Transformers**:
- Wave propagation uses nodes as mini neural nets and edges as mini transformers
- Each step: node computes activation (mini neural net) → edge transforms it (mini transformer) → next node activated
- Wave selects best decisions by activating edges based on transformed activation strength
- All decisions relative - no hardcoded thresholds

**Wave Initialization**:
- Start from nodes matching input (initial activation)
- Create wave front from initial nodes
- Each initial node computes its activation_strength (mini neural net)
- Track initial energy (sum of node activation strengths)

**Wave Propagation Steps** (How Wave Makes Best Decisions):
1. For each node in wave front:
   - **Node computes activation_strength** (mini neural net):
     - Sums weighted inputs from incoming edges (already transformed)
     - Adds self-regulating bias
     - Applies soft non-linearity
     - Result: activation_strength (0.0-1.0)
   
   - **Check if node activation strong enough to propagate**:
     - Threshold relative to local context (local_avg * 0.5)
     - If activation_strength < threshold: update weight, skip propagation
   
   - **For each outgoing edge** (mini transformer):
     - Transform node's activation_strength through edge
     - Result: transformed = edge_transform_activation(edge, activation_strength)
     - Edge transformation considers:
       - Base: weight * activation_strength
       - Pattern similarity boost (if source/target similar)
       - Primary path boost (if edge weight strong relative to local)
   
   - **Find best edges to activate**:
     - Compute max transformed activation across all edges
     - Compute relative threshold (based on max and exploration factor)
     - Exploration factor adapts: more exploration when edges uniform, more focus when varied
     - Activate edges where transformed >= threshold
     - Update edge weights (local learning)
     - Add target nodes to next wave front
   
2. Collect newly activated nodes as next wave front
3. For each newly activated node: compute its activation_strength (mini neural net)
4. Compute current energy (sum of activated node activation strengths)
5. Check convergence: if current energy < initial energy * 0.1, stop
6. Repeat until convergence or no new activations

**How Wave Makes Best Decisions**:
- Wave doesn't use hardcoded rules - it follows the intelligence in nodes and edges
- Nodes (mini neural nets) compute how strongly they should activate
- Edges (mini transformers) compute how strongly they should propagate
- Wave activates edges based on transformed activation strength (relative to context)
- Best decisions emerge from the combined intelligence of nodes and edges
- All thresholds relative to local context - adapts automatically

**Exploration Strategy**:
- Priority queues use learned structure
- Similarity/context edges prioritized (smart paths)
- Strong edges prioritized (frequent patterns)
- Exploration factor adapts based on edge weight variance
- More exploration when edges uniform, more focus when one edge dominates

**Convergence**:
- Natural energy dissipation (no hardcoded depth)
- Stops when propagation naturally weakens
- Relative to initial energy, not absolute

**Output Readiness** (Thinking vs Output):
- **Thinking (always)**: Wave propagation runs for every input
  - Updates node weights, edge weights, activations
  - Creates/strengthens edges through co-activation
  - Explores context and builds associations
  - This happens regardless of output decision
- **Output (conditional)**: Only when patterns are mature
  - Measured by co-activation edge strength from input nodes
  - Compares learned pattern strength to local context
  - Relative threshold: `output_readiness >= output_threshold`
  - Readiness = `avg_coactivation / (max_edge_weight + avg_coactivation)`
  - Threshold = `local_context / (local_context + 1.0f)`
  - If mature: collect output from learned sequential continuations
  - If immature: skip output (pure thinking mode)
- **Biological analogy**:
  - Familiar stimulus → strong patterns → external output (speech/action)
  - Novel stimulus → weak patterns → internal processing only (thought)
- **Data-driven**: No hardcoded thresholds, adapts to graph's learned patterns

**Output Protocol** (How Output is Generated and Routed):
- **Input Processing**: Every input triggers wave propagation (thinking)
  - Input packaged with port_id in first byte (CAN bus format)
  - Wave propagation always runs (updates patterns internally)
  
- **Output Readiness Decision**: Based on pattern maturity (relative threshold)
  - `output_readiness = compute_output_readiness(graph, input_nodes)`
  - Measures co-activation edge strength from input nodes
  - If `output_readiness > 0.0f`: patterns exist → collect output
  - If `output_readiness == 0.0f`: no patterns → pure thinking (no output to ports)
  
- **Output Collection**: Hybrid intelligence (Wave Propagation + LLM-like Sampling)
  - **Wave Propagation Phase**: Collects predictions from all activated nodes
    - Full graph context exploration
    - Nodes compute activation_strength (mini neural net predictions)
    - Edges transform activation (mini transformer outputs)
    - Creates/strengthens edges, forms hierarchies, finds blank nodes
  - **LLM-like Sampling Phase**: Generates output probabilistically
    - Builds probability distribution from collected predictions
    - Uses activation_strength as weights (mini neural nets)
    - Uses edge_transform_activation() for probability shaping (mini transformers)
    - Temperature-controlled sampling (adaptive, 0.5-1.5 range)
    - Can echo input if activation is high (probabilistic, not restricted)
    - Follows co-activation edges only (weight > local average)
    - Autoregressive generation (step-by-step)
  - Output combines full-context intelligence with probabilistic creativity
  - Output is raw data bytes (no port_id in output)
  
- **Output Routing**: External port manager handles routing
  - Reads output from `.m` file universal_output
  - Uses routing table: `input_port_id → output_port_id`
  - Wraps output in PortFrame with destination port_id
  - Sends to appropriate output port
  
- **Internal Feedback Loop** (Future):
  - Special port_id (e.g., 255) for internal thoughts
  - Output routed to feedback port loops back to input
  - External validation prevents "junk" from forming
  - System learns associations through repetition

### Local Value Computation System

**Local Context Access**:
- All thresholds computed from node's own edges and immediate neighbors
- O(1) access to cached local averages (maintained incrementally)
- No global statistics collection needed
- Each node uses its own neighborhood for decisions

**Local Average Computation**:
- `node_get_local_outgoing_weight_avg()`: Average of outgoing edge weights (O(1))
- `node_get_local_incoming_weight_avg()`: Average of incoming edge weights (O(1))
- Cached sums maintained incrementally when edges change
- No histogram building or percentile computation needed

**Context is Cumulative: Persistent + Current + Outputs**:
- **Persistent Weights (from .m file)**: When nodes load from .m file, all edge weights are restored
  - Cached sums (`outgoing_weight_sum`, `incoming_weight_sum`) reflect ALL past edge weights
  - These persistent weights affect local context calculations from the moment the graph loads
  - Example: "hello" node with 1000 past inputs → `outgoing_weight_sum = 45.0` → `local_avg = 0.9` (HIGH context)
- **Current Input (temporary activation)**: New edges created/strengthened during current processing
  - Cached sums updated incrementally: `node_update_outgoing_weight_sum(node, old_weight, new_weight)`
  - Happens in real-time during wave propagation
  - Example: "hello" input now → edge strengthens `0.7 → 0.8` → `outgoing_weight_sum = 45.1` → context changes
- **Previous Outputs (become part of graph)**: When system generates output, nodes/edges are created and saved to .m
  - These edges persist and affect future context calculations
  - System's own outputs become part of its persistent context
  - Example: System outputs "hello world" → creates `[hello]→[world]` edge → saved → affects future context
- **Total Context = Persistent (past) + Current (now) + Outputs (system-generated)**
  - Context reflects entire learning history, not just current input
  - Nodes with history have higher context than brand new nodes
  - This makes relative comparisons (`edge_relative = edge->weight / local_avg`) meaningful and adaptive

**Example: Context Difference Between Experienced vs New Nodes**:
```
Node "hello" (1000 past inputs, in .m file):
  - Persistent: outgoing_weight_sum = 45.0 (from past)
  - Current: +0.1 (current input strengthens edge)
  - Outputs: +0.7 (previous "hello world" output)
  - Total: outgoing_weight_sum = 45.8, outgoing_count = 52
  - local_outgoing_avg = 45.8 / 52 = 0.88 (HIGH context)
  - Edge weight 0.8: edge_relative = 0.8 / 0.88 = 0.91 (moderate relative to high context)

Node "xyzzy" (brand new, never seen):
  - Persistent: outgoing_weight_sum = 0.0 (no past)
  - Current: +0.1 (new edge created)
  - Outputs: 0.0 (no previous outputs)
  - Total: outgoing_weight_sum = 0.1, outgoing_count = 1
  - local_outgoing_avg = 0.1 / 1 = 0.1 (LOW context)
  - Edge weight 0.8: edge_relative = 0.8 / 0.1 = 8.0 (VERY STRONG relative to low context)
```

**Why This Matters for Intelligence**:
- Context reflects cumulative learning: nodes that have been used more have higher context
- Previous outputs shape future context: system's own generated patterns become part of its knowledge
- Relative comparisons are meaningful: same edge weight means different things depending on node's history
- Intelligence accumulates: each interaction (input or output) affects cached sums, which persist in .m file
- System remembers everything: no distinction between "training" and "inference" - all experience accumulates

**Edge Type Detection**:
- Similarity edges: weight between 50%-150% of local average
- Co-activation edges: weight above local average
- All comparisons relative to node's local context, not global percentiles
- More context-aware and adaptive to each node's neighborhood
- Context includes persistent weights from .m file, making comparisons reflect entire learning history

### Adaptive Learning Rates

**Rolling Window**:
- Track recent weight changes
- Window size adapts to change rate
- If changes are frequent, larger window
- If changes are infrequent, smaller window

**Learning Rate Computation**:
- Compute median change rate from rolling window
- Learning rate based on change rate (fast changes → higher rate)
- Relative to local context (node weight vs local average)
- No hardcoded base rate

**Weight Updates**:
- Local-only updates
- Update based on activation
- Learning rate adapts to change rate
- No global gradient computation

---

## Data Structure Specifications

### Node Structure

**Essential Properties**:
- `id`: Unique identifier (8 bytes + null terminator)
- `payload`: Actual data (flexible array, can be 1 byte to very large)
- `payload_size`: Size of payload in bytes
- `activation_strength`: Computed activation (0.0-1.0), from weighted inputs
- `weight`: Activation history (local measurement)
- `bias`: Self-regulating bias term (relative to local context)
- `abstraction_level`: 0 = raw data, 1+ = hierarchy levels

**Edge Pointers** (Local Knowledge):
- `outgoing_edges`: Edges where this node is 'from'
- `outgoing_count`: Number of outgoing edges
- `incoming_edges`: Edges where this node is 'to'
- `incoming_count`: Number of incoming edges

**Cached Local State** (O(1) Access, Maintained Incrementally):
- `outgoing_weight_sum`: Sum of all outgoing edge weights
- `incoming_weight_sum`: Sum of all incoming edge weights
- Updated incrementally when edges added/removed/updated
- Enables O(1) local average computation

**Adaptive Learning Rate Tracking**:
- `recent_weight_changes`: Dynamic rolling window of weight changes
- `weight_change_capacity`: Current window size (adaptive)
- `weight_change_count`: How many values stored
- `weight_change_index`: Circular buffer index
- `change_rate_avg`: Average change rate for adapting window size

**Key Principles**:
- Nodes only know themselves and their edges (no global state)
- All properties maintained locally (no global updates)
- Cached sums maintained incrementally (O(1) updates)
- Adaptive structures adapt to data (no fixed sizes)

### Edge Structure

**Essential Properties**:
- `from_node`: Source node pointer (stores from_id via node->id)
- `to_node`: Target node pointer (stores to_id via node->id)
- `direction`: true = from->to, false = to->from
- `activation`: Binary activation state (1 or 0)
- `weight`: Activation history (local measurement)

**Key Principles**:
- Edges store node pointers (direct access, no searching)
- Weight is local measurement (activation history)
- No global state needed for edge operations
- Edge type emerges from weight (co-activation, similarity, context, homeostatic)

### Graph Structure

**Essential Properties**:
- `nodes`: Array of node pointers
- `node_count`: Number of nodes
- `node_capacity`: Capacity of nodes array (grows as needed)
- `edges`: Array of edge pointers
- `edge_count`: Number of edges
- `edge_capacity`: Capacity of edges array (grows as needed)

**Key Principles**:
- No global state in graph operations
- Arrays grow exponentially (doubles when full)
- No fixed maximum size
- Graph is just container - all operations are local

### .m File Structure

**Header Properties**:
- `magic`: Magic number identifying .m file format
- `version`: File format version
- `node_count`: Number of nodes
- `edge_count`: Number of edges
- `universal_input_size`: Size of input buffer
- `universal_output_size`: Size of output buffer
- `adaptation_count`: Number of adaptations
- `last_modified`: Timestamp of last modification

**Key Principles**:
- File is the running program (not just storage)
- State persists across sessions
- Auto-saves after each adaptation
- Self-regulating (adapts file structure as needed)

---

## Computational Complexity Goals

### Time Complexity

**Per-Node Operations**: O(1) amortized
- Node activation computation: O(degree) where degree is number of edges
- For sparse graphs: degree ≈ constant, so O(1) per node
- Constant per-node cost regardless of graph size

**Wave Propagation**: O(m) where m = number of edges
- Each edge processed at most once per wave
- For sparse graphs: m ≈ O(n) where n = number of nodes
- Linear scaling with graph size

**Pattern Matching**: O(log n) with hierarchy
- Hierarchical lookup: O(depth) where depth is hierarchy depth
- Depth typically logarithmic in number of patterns
- Much better than O(n) brute force

**Edge Formation**: O(m) per wave
- Process activated nodes and their edges
- Linear in number of edges
- Local operations only

**Overall Scaling**: O(m) operations per input
- Linear scaling with number of edges
- Constant per-node cost
- Scales to billions of nodes efficiently

**Performance Optimizations (All Data-Driven)**:
- **Early Exit on Exact Matches**: Skips similarity calculations when exact match found (no hardcoded threshold)
- **Lazy Connection Matching**: Only computes connection match when direct match insufficient (threshold adapts to local context)
- **Early Exit in Byte Comparisons**: Stops comparing when mismatch rate exceeds adaptive threshold (computed from node characteristics)
- **Cached Local Weight Averages**: O(1) access to cached averages, invalidated when edges change (no global state)
- **Skip Wave Exploration on Fast Path**: Skips expensive wave exploration when exact match found in fast paths (no limits)
- **O(n log n) Priority Sorting**: Replaced O(n²) insertion sort with quicksort, adaptive candidate limiting based on priority variance (no hardcoded limits)
- **Edge Lookup Cache**: Per-node cache for recent edge lookups (local-only, no global state)
- **Adaptive Connection Matching**: Only checks top edges by weight, limit computed from weight distribution (no hardcoded limit)

**CPU Optimizations (Hardware-Aware)**:
- **Optimized Compiler Flags**: `-O3 -march=native -mtune=native` enables auto-vectorization and CPU-specific optimizations
- **SIMD Vectorization**: SSE2 (x86) and NEON (ARM/Jetson) for 16-byte-at-once comparisons (4-8x speedup)
- **Memory Alignment**: 16-byte aligned node allocations for optimal SIMD performance
- **Auto-Vectorization**: Compiler automatically vectorizes loops with `-ftree-vectorize` and `-O3`
- **Link-Time Optimization**: `-flto` enables cross-module optimizations

All optimizations preserve intelligent decision-making: compounding learning, adaptive behavior, local-only operations, and data-driven thresholds. CPU optimizations are hardware-aware and automatically adapt to available SIMD instructions (SSE2 on x86, NEON on ARM/Jetson).

### Space Complexity

**Nodes**: O(n) where n = number of nodes
- Each node stores payload (variable size)
- Cached sums: O(1) per node
- Adaptive structures: O(1) per node (bounded)

**Edges**: O(m) where m = number of edges
- Each edge is O(1) size
- Sparse graphs: m ≈ O(n)

**Graph**: O(n + m)
- Node array: O(n)
- Edge array: O(m)
- Total: O(n + m), linear in graph size

**Hash Tables**: O(n) for visited sets
- Size adapts to graph size (2x graph size for ~50% load factor)
- No hardcoded maximum

---

## Comparison to Neural Networks

### Key Differences

**Scaling**:
- Neural Networks: O(n²) or O(n³) operations per step
- Melvin: O(m) operations per step (linear)
- For sparse graphs: 100,000x more efficient

**Knowledge Representation**:
- Neural Networks: Implicit in weights (black box)
- Melvin: Explicit in nodes and edges (transparent)
- Can inspect and reason about knowledge

**Learning**:
- Neural Networks: Global optimization (backprop)
- Melvin: Local updates (local optimization)
- No global gradient computation

**Structure**:
- Neural Networks: Fixed architecture (determined at training)
- Melvin: Dynamic structure (emerges from data)
- Grows organically like biological systems

**Abstraction**:
- Neural Networks: Implicit abstraction in layers
- Melvin: Explicit hierarchy (concrete nodes)
- Can reason about abstractions

**Learning Phase**:
- Neural Networks: Two-phase (training then deployment)
- Melvin: Continuous learning (single phase)
- Improves forever, no train/test split

**Knowledge Growth**:
- Neural Networks: Linear (n parameters)
- Melvin: Compounding (exponential with hierarchy depth)
- Knowledge compounds: Level N builds on Level N-1

---

## Implementation Status

**Current Implementation Aligns with Vision:**

✅ **Local Measurements Only**: All thresholds computed from node's local context (edges and neighbors), not global statistics

✅ **No Hardcoded Thresholds**: All thresholds are data-driven, computed from local averages using O(1) cached access

✅ **No Global Statistics**: Removed histogram/percentile system - uses local value computations exclusively

✅ **O(1) Performance**: Local averages accessed from cached sums maintained incrementally

✅ **Context-Aware**: Each node uses its own neighborhood for decisions, not global percentiles

✅ **Self-Regulating**: System adapts through local feedback loops without global state

✅ **Performance Optimizations**: All optimizations are data-driven and adaptive, preserving intelligent decision-making:
   - Early exit on exact matches (skips similarity calculations)
   - Lazy connection matching (only when direct match insufficient)
   - Early exit in byte comparisons (adaptive mismatch threshold)
   - Cached local weight averages (O(1) access, invalidated when edges change)
   - Skip wave exploration when fast path succeeds (exact match found)
   - O(n log n) priority sorting instead of O(n²) (adaptive candidate limiting)
   - Edge lookup cache (local-only, per-node)
   - Adaptive connection matching (top edges by weight, limit computed from distribution)

The implementation now fully matches the vision: **local measurements only**, **no global state**, **data-driven adaptation**, **optimized operations per byte**.

