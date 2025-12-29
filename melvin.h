/*
 * Melvin: Emergent Intelligence System - Header
 * 
 * Core Philosophy:
 * - Self-regulation through local measurements only
 * - No hardcoded limits or thresholds
 * - Nodes only know themselves and their edges
 * - All measurements are local (no global state)
 * - Nodes can scale from 1 byte to very large payloads
 */

#ifndef MELVIN_H
#define MELVIN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ========================================
 * CORE STRUCTURES
 * ======================================== */

/* Forward declarations */
typedef struct Node Node;
typedef struct VisitedSet VisitedSet;
typedef struct WaveStatistics WaveStatistics;

/* Edge: Simple connection between two nodes */
typedef struct Edge {
    /* Node pointers for direct access (no searching) - variables with multiple jobs */
    Node *from_node;      /* Source node (also stores from_id via node->id) */
    Node *to_node;        /* Target node (also stores to_id via node->id) */
    bool direction;       /* true = from->to, false = to->from */
    bool activation;      /* Binary: 1 or 0 */
    float weight;         /* Activation history (local measurement) - also serves as decision basis */
} Edge;

/* Node: Core unit of the system */
typedef struct Node {
    char id[9];           /* 8-byte unique ID string + null terminator */
    
    size_t payload_size;  /* Size of payload in bytes (can be 1 to very large) */
    float activation_strength;  /* Computed activation (0.0-1.0), replaces bool activation */
    float weight;         /* Activation history (local measurement) */
    float bias;           /* Self-regulating bias term (relative to local context) */
    uint32_t abstraction_level;  /* 0 = raw data, 1+ = hierarchy levels */
    
    /* Edge pointers: nodes only know their edges */
    Edge **outgoing_edges;  /* Edges where this node is 'from' */
    size_t outgoing_count;
    size_t outgoing_capacity;
    
    Edge **incoming_edges;  /* Edges where this node is 'to' */
    size_t incoming_count;
    size_t incoming_capacity;
    
    /* Cached local state (O(1) access - maintained incrementally) */
    float outgoing_weight_sum;  /* Sum of all outgoing edge weights (maintained incrementally) */
    float incoming_weight_sum;  /* Sum of all incoming edge weights (maintained incrementally) */
    
    /* Adaptive learning rate tracking (rolling window) */
    float *recent_weight_changes;  /* Dynamic rolling window - adapts to change rate */
    size_t weight_change_capacity;  /* Current window size (adaptive) */
    size_t weight_change_count;     /* How many values stored */
    int weight_change_index;  /* Circular buffer index */
    float change_rate_avg;  /* Average change rate for adapting window size */
    
    /* Payload: actual data storage (flexible array member - data stored inline) */
    uint8_t payload[];     /* Flexible array - data is stored directly in the node */
} Node;

/* Graph: Container for nodes and edges (no global state in operations) */
typedef struct MelvinGraph {
    Node **nodes;
    size_t node_count;
    size_t node_capacity;
    
    Edge **edges;
    size_t edge_count;
    size_t edge_capacity;
    
    /* Context: Last activated nodes (seeds for next input) */
    /* Memory = weights, Context = activation */
    Node **last_activated;
    size_t last_activated_count;
    size_t last_activated_capacity;
} MelvinGraph;

/* ========================================
 * .M FILE FORMAT (Live, Executable Program)
 * ======================================== */

/* Magic number for .m files: "MELVIN\0\0" */
#define MELVIN_M_MAGIC 0x4D454C56494E0000ULL  /* "MELVIN\0\0" in ASCII */
#define MELVIN_M_VERSION 1

/* .m File Header - persistent state of the live program */
typedef struct MelvinMHeader {
    uint64_t magic;         /* Magic number: MELVIN_M_MAGIC */
    uint32_t version;       /* File format version */
    uint32_t flags;         /* Format flags */
    
    /* Graph structure */
    uint64_t node_count;    /* Number of nodes */
    uint64_t edge_count;    /* Number of edges */
    
    /* Universal I/O port (data-driven execution) */
    uint64_t universal_input_size;  /* Size of universal input buffer */
    uint64_t universal_input_offset; /* Offset to universal input data */
    
    uint64_t universal_output_size;  /* Size of universal output buffer */
    uint64_t universal_output_offset; /* Offset to universal output data */
    
    /* Data offsets */
    uint64_t nodes_offset;  /* Offset to node data section */
    uint64_t edges_offset;  /* Offset to edge data section */
    uint64_t payloads_offset; /* Offset to payload data section */
    
    /* Adaptive metadata (self-regulating) */
    uint64_t last_modified; /* Timestamp of last modification */
    uint64_t adaptation_count; /* Number of adaptations */
} MelvinMHeader;

/* .m File - The live, executable program driven by data */
typedef struct MelvinMFile {
    FILE *file;
    char *filename;
    MelvinMHeader header;
    MelvinGraph *graph;     /* The executing graph (uses melvin.c rules) */
    uint8_t *universal_input; /* I/O port: input buffer (any binary input) */
    size_t universal_input_capacity;
    uint8_t *universal_output; /* I/O port: output buffer (wave propagation results) */
    size_t universal_output_capacity;
    uint8_t last_input_port_id; /* Last input port ID extracted from input (ephemeral, for routing) */
    bool is_dirty;           /* True if file needs auto-save (self-regulating) */
} MelvinMFile;

/* ========================================
 * FUNCTION DECLARATIONS
 * ======================================== */

/* Node Operations */
Node* node_create(const uint8_t *payload_data, size_t payload_size);
void node_update_weight_local(Node *node);
float node_compute_activation_strength(Node *node);  /* Compute activation from weighted inputs (mini neural net) */
float node_calculate_match_strength(Node *node, const uint8_t *pattern, size_t pattern_size);
float node_get_local_outgoing_weight_avg(Node *node);
float node_get_local_incoming_weight_avg(Node *node);
void node_free(Node *node);

/* Payload Expansion & Hierarchy (nodes combine into larger payloads when patterns repeat) */
Node* node_combine_payloads(Node *node1, Node *node2);  /* Combine nodes - hierarchy formation */
void node_transfer_incoming_to_hierarchy(MelvinGraph *g, Node *node1, Node *node2, Node *combined);  /* Transfer edges to hierarchy */
/* Hierarchy formation now emerges naturally from edge weight growth - see wave_create_edges_from_coactivation() */
Node* node_create_blank(void);  /* Create blank/template node for pattern matching */
Node* node_fill_blank(Node *blank_node, const uint8_t *pattern, size_t pattern_size, float match_strength);  /* Fill blank when pattern matches - returns new node */
/* REMOVED: blank_node_compute_category_match - now integrated into universal node_calculate_match_strength */
/* REMOVED: hierarchy_node_compute_abstraction() - all nodes use universal node_compute_activation_strength() */

/* Edge Operations */
Edge* edge_create(Node *from, Node *to, bool direction);
void edge_update_weight_local(Edge *edge);
float edge_transform_activation(Edge *edge, float input_activation);  /* Transform activation as it flows through edge */
void edge_free(Edge *edge);

/* ========================================
 * .M FILE OPERATIONS (Live Program Interface)
 * ======================================== */

/* Bootstrap: Create new .m file (the live program) */
MelvinMFile* melvin_bootstrap(const char *filename);

/* Open existing .m file (activates the live program) */
MelvinMFile* melvin_m_open(const char *filename);

/* Save .m file (auto-save on adaptation - self-regulating) */
bool melvin_m_save(MelvinMFile *mfile);

/* Close .m file (auto-saves if dirty) */
void melvin_m_close(MelvinMFile *mfile);

/* ========================================
 * I/O PORT OPERATIONS (Data-Driven Execution)
 * ======================================== */

/* Write data to I/O port (triggers execution) */
bool melvin_m_universal_input_write(MelvinMFile *mfile, const uint8_t *data, size_t size);

/* Read data from I/O port */
size_t melvin_m_universal_input_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size);
size_t melvin_m_universal_input_size(MelvinMFile *mfile);
void melvin_m_universal_input_clear(MelvinMFile *mfile);

/* Read output from I/O port (wave propagation results) */
size_t melvin_m_universal_output_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size);
size_t melvin_m_universal_output_size(MelvinMFile *mfile);
void melvin_m_universal_output_clear(MelvinMFile *mfile);

/* Process input through graph (data-driven execution - auto-saves after adaptation) */
bool melvin_m_process_input(MelvinMFile *mfile);

/* Graph access from .m file */
MelvinGraph* melvin_m_get_graph(MelvinMFile *mfile);

/* Add node/edge to .m file (marks dirty for auto-save) */
Node* melvin_m_add_node(MelvinMFile *mfile, const uint8_t *payload_data, size_t payload_size);
Edge* melvin_m_add_edge(MelvinMFile *mfile, Node *from, Node *to, bool direction);

/* Adaptive operations */
void melvin_m_mark_dirty(MelvinMFile *mfile);
bool melvin_m_is_dirty(MelvinMFile *mfile);
uint64_t melvin_m_get_adaptation_count(MelvinMFile *mfile);

/* Graph Operations (Internal - used by .m file operations) */
/* Nodes and edges created through wave propagation - no searching */
MelvinGraph* graph_create(void);
bool graph_add_node(MelvinGraph *g, Node *node);  /* Creation law */
bool graph_add_edge(MelvinGraph *g, Edge *edge, Node *from, Node *to);  /* Creation law */
void graph_free(MelvinGraph *g);

/* Wave Propagation */
Node** wave_propagate_from_node(Node *node);
void wave_propagate_multi_step(MelvinGraph *g, Node **initial_nodes, size_t initial_count);
Node** wave_process_sequential_patterns(MelvinGraph *g, const uint8_t *data, size_t data_size, size_t *out_count);  /* Process data to find sequential patterns */
void wave_create_edges_from_coactivation(MelvinGraph *g, Node **activated_nodes, size_t activated_count);  /* Create edges from co-activation (simple rule) */
void wave_create_edges_from_similarity(MelvinGraph *g, Node *node, float similarity_threshold);  /* Create edges between similar patterns */
void wave_create_edges_from_context(MelvinGraph *g, Node **recently_activated, size_t count,
                                    VisitedSet *context_visited);  /* Create edges based on wave propagation context (paths recently traveled) */
void wave_form_universal_generalizations(MelvinGraph *g, Node **co_activated, size_t count);  /* UNIVERSAL: Pattern generalization (abstraction) - all nodes can generalize */
void wave_form_universal_combinations(MelvinGraph *g, Node **co_activated, size_t count);  /* UNIVERSAL: Pattern combination (hierarchy) - all nodes can combine */
void wave_create_homeostatic_edges(MelvinGraph *g, Node *isolated_node);  /* Create edges to prevent node isolation */
void wave_form_intelligent_edges(MelvinGraph *g, Node **activated_nodes, size_t activated_count,
                                 VisitedSet *context_visited, WaveStatistics *stats);  /* Form intelligent edges using all creation laws, with wave propagation context */
void wave_collect_output(MelvinGraph *g, Node **direct_input_nodes, size_t direct_input_count, uint8_t **output, size_t *output_size);  /* Output from direct input nodes and sequential continuations only */

#endif /* MELVIN_H */

