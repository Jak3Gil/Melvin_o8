/*
 * Melvin: Emergent Intelligence System
 * 
 * Core Philosophy:
 * - Self-regulation through local measurements only
 * - No hardcoded limits or thresholds
 * - Nodes only know themselves and their edges
 * - All measurements are local (no global state)
 * - Nodes can scale from 1 byte to very large payloads
 * 
 * Bootstrap: Creates .m files - the persistent storage format
 */

#include "melvin.h"
#include "melvin_m.h"

/* ========================================
 * UTILITY FUNCTIONS
 * ======================================== */

/* Generate unique 8-byte ID string */
void generate_node_id(char *id_buffer) {
    static uint64_t id_counter = 0;
    snprintf(id_buffer, 9, "%08llu", (unsigned long long)id_counter++);
}

/* ========================================
 * NODE OPERATIONS (Local Only)
 * ======================================== */

/* Create a new node with payload (payload stored directly in node) */
Node* node_create(const uint8_t *payload_data, size_t payload_size) {
    /* Allocate node with space for payload (flexible array member) */
    Node *node = (Node*)calloc(1, sizeof(Node) + payload_size);
    if (!node) return NULL;
    
    /* Generate sequential binary ID (big-endian, 8 bytes) */
    static uint64_t id_counter = 0;
    for (int i = 0; i < 8; i++) {
        node->id[i] = (char)((id_counter >> (8 * (7 - i))) & 0xFF);
    }
    node->id[8] = '\0';
    id_counter++;
    
    /* Store payload size */
    node->payload_size = payload_size;
    
    /* Copy payload data directly into node (stored inline) */
    if (payload_data && payload_size > 0) {
        memcpy(node->payload, payload_data, payload_size);
    }
    
    /* Initialize state */
    node->activation = false;
    node->weight = 0.0f;
    
    /* Initialize edge arrays */
    node->outgoing_capacity = 4;
    node->outgoing_edges = (Edge**)calloc(node->outgoing_capacity, sizeof(Edge*));
    node->outgoing_count = 0;
    
    node->incoming_capacity = 4;
    node->incoming_edges = (Edge**)calloc(node->incoming_capacity, sizeof(Edge*));
    node->incoming_count = 0;
    
    return node;
}

/* Update node weight based on local activation history (relative to local context) */
void node_update_weight_local(Node *node) {
    if (!node) return;
    
    /* Learning rate relative to node's current weight and local edge averages */
    float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                       node_get_local_incoming_weight_avg(node)) / 2.0f;
    float rate = (local_avg > 0.0f) ? node->weight / (node->weight + local_avg) : 0.1f;
    
    /* Weight updates relative to activation state */
    node->weight = node->weight * (1.0f - rate) + (node->activation ? 1.0f : 0.0f) * rate;
}

/* Get local average weight from outgoing edges (local measurement) */
float node_get_local_outgoing_weight_avg(Node *node) {
    if (!node || node->outgoing_count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (size_t i = 0; i < node->outgoing_count; i++) {
        if (node->outgoing_edges[i]) {
            sum += node->outgoing_edges[i]->weight;
        }
    }
    return sum / (float)node->outgoing_count;
}

/* Get local average weight from incoming edges (local measurement) */
float node_get_local_incoming_weight_avg(Node *node) {
    if (!node || node->incoming_count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (size_t i = 0; i < node->incoming_count; i++) {
        if (node->incoming_edges[i]) {
            sum += node->incoming_edges[i]->weight;
        }
    }
    return sum / (float)node->incoming_count;
}

/* Free node (payload is stored inline, so freeing node frees payload too) */
void node_free(Node *node) {
    if (!node) return;
    
    if (node->outgoing_edges) {
        free(node->outgoing_edges);
    }
    if (node->incoming_edges) {
        free(node->incoming_edges);
    }
    /* Payload is stored inline (flexible array member), so freeing node frees payload */
    free(node);
}

/* ========================================
 * EDGE OPERATIONS (Local Only)
 * ======================================== */

/* Create a new edge between two nodes (direct node pointers - no searching) */
Edge* edge_create(Node *from, Node *to, bool direction) {
    if (!from || !to) return NULL;
    
    Edge *edge = (Edge*)calloc(1, sizeof(Edge));
    if (!edge) return NULL;
    
    /* Store node pointers directly (no IDs needed - IDs in nodes) */
    edge->from_node = from;
    edge->to_node = to;
    edge->direction = direction;
    edge->activation = false;
    edge->weight = 0.0f;
    
    return edge;
}

/* Update edge weight based on local activation history (relative to edge's current state) */
void edge_update_weight_local(Edge *edge) {
    if (!edge) return;
    
    /* Learning rate relative to edge's current weight (self-relative) */
    float rate = edge->weight / (edge->weight + 1.0f);
    
    /* Weight updates relative to activation state */
    edge->weight = edge->weight * (1.0f - rate) + (edge->activation ? 1.0f : 0.0f) * rate;
}

/* Free edge */
void edge_free(Edge *edge) {
    if (edge) {
        free(edge);
    }
}

/* ========================================
 * BOOTSTRAP: .M FILE CREATION
 * ======================================== */

/* Bootstrap: Create a new .m file (primary entry point) */
MelvinMFile* melvin_bootstrap(const char *filename) {
    /* Create .m file - this is the bootstrap */
    return melvin_m_create(filename);
}

/* ========================================
 * GRAPH OPERATIONS (Internal - used by .m file operations)
 * ======================================== */

/* Create a new graph (internal use - for .m file operations) */
MelvinGraph* graph_create(void) {
    MelvinGraph *g = (MelvinGraph*)calloc(1, sizeof(MelvinGraph));
    if (!g) return NULL;
    
    /* No capacity limits - start with 0, allocate dynamically on first add */
    g->node_capacity = 0;
    g->nodes = NULL;
    g->node_count = 0;
    
    g->edge_capacity = 0;
    g->edges = NULL;
    g->edge_count = 0;
    
    return g;
}

/* Add node to graph (creation law - nodes created through wave propagation) */
bool graph_add_node(MelvinGraph *g, Node *node) {
    if (!g || !node) return false;
    
    /* Resize if needed (no capacity limits - allocate dynamically) */
    if (g->node_count >= g->node_capacity) {
        size_t new_capacity = (g->node_capacity == 0) ? 1 : g->node_capacity * 2;
        Node **new_nodes = (Node**)realloc(g->nodes, new_capacity * sizeof(Node*));
        if (!new_nodes) return false;
        g->nodes = new_nodes;
        g->node_capacity = new_capacity;
    }
    
    g->nodes[g->node_count++] = node;
    return true;
}

/* Add edge to graph and connect to nodes (creation law - nodes created through wave prop) */
bool graph_add_edge(MelvinGraph *g, Edge *edge, Node *from, Node *to) {
    if (!g || !edge || !from || !to) return false;
    
    /* Resize if needed (no capacity limits - allocate dynamically) */
    if (g->edge_count >= g->edge_capacity) {
        size_t new_capacity = (g->edge_capacity == 0) ? 1 : g->edge_capacity * 2;
        Edge **new_edges = (Edge**)realloc(g->edges, new_capacity * sizeof(Edge*));
        if (!new_edges) return false;
        g->edges = new_edges;
        g->edge_capacity = new_capacity;
    }
    
    g->edges[g->edge_count++] = edge;
    
    /* Connect edge to nodes (local to nodes) - no searching, direct connection */
        /* Add to from node's outgoing edges */
        if (from->outgoing_count >= from->outgoing_capacity) {
        size_t new_cap = (from->outgoing_capacity == 0) ? 1 : from->outgoing_capacity * 2;
            Edge **new_out = (Edge**)realloc(from->outgoing_edges, new_cap * sizeof(Edge*));
            if (!new_out) return false;
            from->outgoing_edges = new_out;
            from->outgoing_capacity = new_cap;
        }
        from->outgoing_edges[from->outgoing_count++] = edge;
    
        /* Add to to node's incoming edges */
        if (to->incoming_count >= to->incoming_capacity) {
        size_t new_cap = (to->incoming_capacity == 0) ? 1 : to->incoming_capacity * 2;
            Edge **new_in = (Edge**)realloc(to->incoming_edges, new_cap * sizeof(Edge*));
            if (!new_in) return false;
            to->incoming_edges = new_in;
            to->incoming_capacity = new_cap;
        }
        to->incoming_edges[to->incoming_count++] = edge;
    
    return true;
}

/* Free graph and all nodes/edges */
void graph_free(MelvinGraph *g) {
    if (!g) return;
    
    /* Free all nodes */
    for (size_t i = 0; i < g->node_count; i++) {
        node_free(g->nodes[i]);
    }
    free(g->nodes);
    
    /* Free all edges */
    for (size_t i = 0; i < g->edge_count; i++) {
        edge_free(g->edges[i]);
    }
    free(g->edges);
    
    free(g);
}

/* ========================================
 * WAVE PROPAGATION (Local Operations Only)
 * ======================================== */

/* Propagate activation from a node through its outgoing edges (local, relative) */
/* Node acts as mini neural net - "thinks" by combining edge weights + payload structure */
/* Wave prop "selects" nodes by activating edges based on relative influence */
void wave_propagate_from_node(Node *node) {
    if (!node || !node->activation) return;
    
    /* Local average serves multiple jobs: normalization + decision basis + neural net input */
    float local_avg = node_get_local_outgoing_weight_avg(node);
    
    for (size_t i = 0; i < node->outgoing_count; i++) {
        Edge *edge = node->outgoing_edges[i];
        if (!edge) continue;
        
        /* Node "thinks" by computing relative influence (edge weight + payload structure) */
        /* Edge weight relative to local average (relative measurement) */
        float influence = (local_avg > 0.0f) ? edge->weight / local_avg : edge->weight;
        
        /* Payload bytes naturally influence (relative to payload structure) - mini neural net */
        /* Payload acts as node's "brain" - influences which edges to activate */
        if (node->payload_size > 0) {
            float payload_val = (float)node->payload[i % node->payload_size];
            float payload_max = (node->payload_size > 0) ? 255.0f : 1.0f;
            /* Combine relative to each other (no hardcoded weights) */
            influence = (influence + payload_val / payload_max) / 2.0f;
        }
        
        /* Wave prop "selects" edge if influence > 0 (relative decision, no absolute threshold) */
        if (influence > 0.0f) {
            edge->activation = true;
            edge_update_weight_local(edge);
            
            /* Activate target node directly (no searching - edge has node pointer) */
            if (edge->to_node) {
                edge->to_node->activation = true;
                /* Recursively propagate from target node (wave continues) */
                wave_propagate_from_node(edge->to_node);
            }
        }
    }
    
    node_update_weight_local(node);
}

/* ========================================
 * PAYLOAD EXPANSION & HIERARCHY (Relative Operations)
 * ======================================== */

/* Combine nodes into larger payload when patterns repeat (hierarchy formation) */
/* Payload expands by combining - all relative, no hardcoded rules */
Node* node_combine_payloads(Node *node1, Node *node2) {
    if (!node1 || !node2) return NULL;
    
    /* Combined size relative to both nodes */
    size_t combined_size = node1->payload_size + node2->payload_size;
    
    /* Allocate combined payload */
    uint8_t *combined = (uint8_t*)malloc(combined_size);
    if (!combined) return NULL;
    
    /* Combine payloads (relative combination) */
    memcpy(combined, node1->payload, node1->payload_size);
    memcpy(combined + node1->payload_size, node2->payload, node2->payload_size);
    
    /* Create new node with combined payload (hierarchy) */
    Node *combined_node = node_create(combined, combined_size);
    free(combined);
    
    /* Weight relative to both nodes */
    if (combined_node) {
        combined_node->weight = (node1->weight + node2->weight) / 2.0f;
    }
    
    return combined_node;
}

/* Create blank/template node (for pattern matching and generalization) */
/* Blank nodes have empty payload - can be filled when patterns match */
Node* node_create_blank(void) {
    return node_create(NULL, 0);
}

/* Fill blank node with payload when pattern matches (relative to match strength) */
/* Returns new node with filled payload, original blank_node unchanged */
Node* node_fill_blank(Node *blank_node, const uint8_t *pattern, size_t pattern_size, float match_strength) {
    if (!blank_node || !pattern || pattern_size == 0 || match_strength <= 0.0f) return NULL;
    
    /* Fill size relative to match strength */
    size_t fill_size = (size_t)(pattern_size * match_strength);
    if (fill_size == 0) fill_size = 1;
    
    /* Create new node with filled payload (relative to match strength) */
    Node *filled_node = node_create(pattern, fill_size);
    if (!filled_node) return NULL;
    
    /* Weight relative to match strength and original blank weight */
    filled_node->weight = (blank_node->weight + match_strength) / 2.0f;
    
    return filled_node;
}


