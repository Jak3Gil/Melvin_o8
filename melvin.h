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
    bool activation;      /* Binary: 1 or 0 */
    float weight;         /* Activation history (local measurement) */
    
    /* Edge pointers: nodes only know their edges */
    Edge **outgoing_edges;  /* Edges where this node is 'from' */
    size_t outgoing_count;
    size_t outgoing_capacity;
    
    Edge **incoming_edges;  /* Edges where this node is 'to' */
    size_t incoming_count;
    size_t incoming_capacity;
    
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
} MelvinGraph;

/* Forward declaration for .m file format */
typedef struct MelvinMFile MelvinMFile;

/* ========================================
 * FUNCTION DECLARATIONS
 * ======================================== */

/* Utility Functions */
void generate_node_id(char *id_buffer);

/* Node Operations */
Node* node_create(const uint8_t *payload_data, size_t payload_size);
void node_update_weight_local(Node *node);
float node_get_local_outgoing_weight_avg(Node *node);
float node_get_local_incoming_weight_avg(Node *node);
void node_free(Node *node);

/* Payload Expansion & Hierarchy (nodes combine into larger payloads when patterns repeat) */
Node* node_combine_payloads(Node *node1, Node *node2);  /* Combine nodes - hierarchy formation */
Node* node_create_blank(void);  /* Create blank/template node for pattern matching */
Node* node_fill_blank(Node *blank_node, const uint8_t *pattern, size_t pattern_size, float match_strength);  /* Fill blank when pattern matches - returns new node */

/* Edge Operations */
Edge* edge_create(Node *from, Node *to, bool direction);
void edge_update_weight_local(Edge *edge);
void edge_free(Edge *edge);

/* Bootstrap: Create .m file (primary entry point) */
MelvinMFile* melvin_bootstrap(const char *filename);

/* Graph Operations (Internal - used by .m file operations) */
/* Nodes and edges created through wave propagation - no searching */
MelvinGraph* graph_create(void);
bool graph_add_node(MelvinGraph *g, Node *node);  /* Creation law */
bool graph_add_edge(MelvinGraph *g, Edge *edge, Node *from, Node *to);  /* Creation law */
void graph_free(MelvinGraph *g);

/* Wave Propagation */
void wave_propagate_from_node(Node *node);

#endif /* MELVIN_H */

