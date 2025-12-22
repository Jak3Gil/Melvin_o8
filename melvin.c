/*
 * Melvin: Emergent Intelligence System
 * 
 * Core Philosophy:
 * - Self-regulation through local measurements only
 * - No hardcoded limits or thresholds
 * - Nodes only know themselves and their edges
 * - All measurements are local (no global state)
 * - Nodes can scale from 1 byte to very large payloads
 */

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
    char from_id[9];      /* 8-byte ID string + null terminator */
    char to_id[9];        /* 8-byte ID string + null terminator */
    bool direction;       /* true = from->to, false = to->from */
    bool activation;      /* Binary: 1 or 0 */
    float weight;         /* Activation history (local measurement) */
} Edge;

/* Node: Core unit of the system */
typedef struct Node {
    char id[9];           /* 8-byte unique ID string + null terminator */
    
    /* Payload: actual data storage (flexible size) */
    uint8_t *payload;     /* Pointer to payload data */
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

/* Create a new node with payload */
Node* node_create(const uint8_t *payload_data, size_t payload_size) {
    Node *node = (Node*)calloc(1, sizeof(Node));
    if (!node) return NULL;
    
    /* Generate unique ID */
    generate_node_id(node->id);
    
    /* Allocate and copy payload */
    if (payload_data && payload_size > 0) {
        node->payload = (uint8_t*)malloc(payload_size);
        if (!node->payload) {
            free(node);
            return NULL;
        }
        memcpy(node->payload, payload_data, payload_size);
        node->payload_size = payload_size;
    } else {
        node->payload = NULL;
        node->payload_size = 0;
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

/* Update node weight based on local activation history */
void node_update_weight_local(Node *node) {
    if (!node) return;
    
    /* Local measurement: weight is activation history */
    /* Simple exponential moving average (local to this node) */
    float alpha = 0.1f;  /* Learning rate (could be made relative later) */
    if (node->activation) {
        node->weight = node->weight * (1.0f - alpha) + 1.0f * alpha;
    } else {
        node->weight = node->weight * (1.0f - alpha) + 0.0f * alpha;
    }
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

/* Free node and its payload */
void node_free(Node *node) {
    if (!node) return;
    
    if (node->payload) {
        free(node->payload);
    }
    if (node->outgoing_edges) {
        free(node->outgoing_edges);
    }
    if (node->incoming_edges) {
        free(node->incoming_edges);
    }
    free(node);
}

/* ========================================
 * EDGE OPERATIONS (Local Only)
 * ======================================== */

/* Create a new edge between two nodes */
Edge* edge_create(Node *from, Node *to, bool direction) {
    if (!from || !to) return NULL;
    
    Edge *edge = (Edge*)calloc(1, sizeof(Edge));
    if (!edge) return NULL;
    
    strncpy(edge->from_id, from->id, 8);
    edge->from_id[8] = '\0';
    strncpy(edge->to_id, to->id, 8);
    edge->to_id[8] = '\0';
    
    edge->direction = direction;
    edge->activation = false;
    edge->weight = 0.0f;
    
    return edge;
}

/* Update edge weight based on local activation history */
void edge_update_weight_local(Edge *edge) {
    if (!edge) return;
    
    /* Local measurement: weight is activation history */
    float alpha = 0.1f;  /* Learning rate (could be made relative later) */
    if (edge->activation) {
        edge->weight = edge->weight * (1.0f - alpha) + 1.0f * alpha;
    } else {
        edge->weight = edge->weight * (1.0f - alpha) + 0.0f * alpha;
    }
}

/* Free edge */
void edge_free(Edge *edge) {
    if (edge) {
        free(edge);
    }
}

/* ========================================
 * GRAPH OPERATIONS
 * ======================================== */

/* Create a new graph */
MelvinGraph* graph_create(void) {
    MelvinGraph *g = (MelvinGraph*)calloc(1, sizeof(MelvinGraph));
    if (!g) return NULL;
    
    g->node_capacity = 16;
    g->nodes = (Node**)calloc(g->node_capacity, sizeof(Node*));
    g->node_count = 0;
    
    g->edge_capacity = 32;
    g->edges = (Edge**)calloc(g->edge_capacity, sizeof(Edge*));
    g->edge_count = 0;
    
    return g;
}

/* Add node to graph */
bool graph_add_node(MelvinGraph *g, Node *node) {
    if (!g || !node) return false;
    
    /* Check if node already exists */
    for (size_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i] && strcmp(g->nodes[i]->id, node->id) == 0) {
            return false;  /* Node already exists */
        }
    }
    
    /* Resize if needed */
    if (g->node_count >= g->node_capacity) {
        size_t new_capacity = g->node_capacity * 2;
        Node **new_nodes = (Node**)realloc(g->nodes, new_capacity * sizeof(Node*));
        if (!new_nodes) return false;
        g->nodes = new_nodes;
        g->node_capacity = new_capacity;
    }
    
    g->nodes[g->node_count++] = node;
    return true;
}

/* Find node by ID */
Node* graph_find_node(MelvinGraph *g, const char *id) {
    if (!g || !id) return NULL;
    
    for (size_t i = 0; i < g->node_count; i++) {
        if (g->nodes[i] && strcmp(g->nodes[i]->id, id) == 0) {
            return g->nodes[i];
        }
    }
    return NULL;
}

/* Add edge to graph and connect to nodes */
bool graph_add_edge(MelvinGraph *g, Edge *edge) {
    if (!g || !edge) return false;
    
    /* Resize if needed */
    if (g->edge_count >= g->edge_capacity) {
        size_t new_capacity = g->edge_capacity * 2;
        Edge **new_edges = (Edge**)realloc(g->edges, new_capacity * sizeof(Edge*));
        if (!new_edges) return false;
        g->edges = new_edges;
        g->edge_capacity = new_capacity;
    }
    
    g->edges[g->edge_count++] = edge;
    
    /* Connect edge to nodes (local to nodes) */
    Node *from = graph_find_node(g, edge->from_id);
    Node *to = graph_find_node(g, edge->to_id);
    
    if (from) {
        /* Add to from node's outgoing edges */
        if (from->outgoing_count >= from->outgoing_capacity) {
            size_t new_cap = from->outgoing_capacity * 2;
            Edge **new_out = (Edge**)realloc(from->outgoing_edges, new_cap * sizeof(Edge*));
            if (!new_out) return false;
            from->outgoing_edges = new_out;
            from->outgoing_capacity = new_cap;
        }
        from->outgoing_edges[from->outgoing_count++] = edge;
    }
    
    if (to) {
        /* Add to to node's incoming edges */
        if (to->incoming_count >= to->incoming_capacity) {
            size_t new_cap = to->incoming_capacity * 2;
            Edge **new_in = (Edge**)realloc(to->incoming_edges, new_cap * sizeof(Edge*));
            if (!new_in) return false;
            to->incoming_edges = new_in;
            to->incoming_capacity = new_cap;
        }
        to->incoming_edges[to->incoming_count++] = edge;
    }
    
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

/* Propagate activation from a node through its outgoing edges (local) */
void wave_propagate_from_node(Node *node) {
    if (!node || !node->activation) return;
    
    /* Node only knows its own edges (local) */
    for (size_t i = 0; i < node->outgoing_count; i++) {
        Edge *edge = node->outgoing_edges[i];
        if (!edge) continue;
        
        /* Local measurement: edge weight relative to local average */
        float local_avg = node_get_local_outgoing_weight_avg(node);
        float relative_weight = (local_avg > 0.0f) ? edge->weight / local_avg : 0.0f;
        
        /* Activate edge if relative weight is significant (local decision) */
        if (relative_weight > 0.5f || edge->weight > 0.0f) {
            edge->activation = true;
            edge_update_weight_local(edge);
            
            /* Find target node and activate it (would need graph reference, but keeping local) */
            /* In full implementation, this would be handled differently */
        }
    }
    
    /* Update node weight locally */
    node_update_weight_local(node);
}

/* ========================================
 * MAIN / TESTING
 * ======================================== */

int main(void) {
    printf("Melvin: Emergent Intelligence System\n");
    printf("=====================================\n\n");
    
    /* Create graph */
    MelvinGraph *g = graph_create();
    if (!g) {
        fprintf(stderr, "Failed to create graph\n");
        return 1;
    }
    
    /* Create some test nodes with different payload sizes */
    uint8_t small_payload = 0x42;
    Node *node1 = node_create(&small_payload, 1);
    
    uint8_t medium_payload[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  /* "Hello" */
    Node *node2 = node_create(medium_payload, 5);
    
    uint8_t large_payload[100];
    for (int i = 0; i < 100; i++) {
        large_payload[i] = (uint8_t)(i % 256);
    }
    Node *node3 = node_create(large_payload, 100);
    
    /* Add nodes to graph */
    graph_add_node(g, node1);
    graph_add_node(g, node2);
    graph_add_node(g, node3);
    
    printf("Created %zu nodes:\n", g->node_count);
    printf("  Node %s: payload_size=%zu, activation=%d, weight=%.3f\n",
           node1->id, node1->payload_size, node1->activation, node1->weight);
    printf("  Node %s: payload_size=%zu, activation=%d, weight=%.3f\n",
           node2->id, node2->payload_size, node2->activation, node2->weight);
    printf("  Node %s: payload_size=%zu, activation=%d, weight=%.3f\n",
           node3->id, node3->payload_size, node3->activation, node3->weight);
    
    /* Create edges */
    Edge *edge1 = edge_create(node1, node2, true);
    Edge *edge2 = edge_create(node2, node3, true);
    
    graph_add_edge(g, edge1);
    graph_add_edge(g, edge2);
    
    printf("\nCreated %zu edges:\n", g->edge_count);
    printf("  Edge: %s -> %s, direction=%d, weight=%.3f\n",
           edge1->from_id, edge1->to_id, edge1->direction, edge1->weight);
    printf("  Edge: %s -> %s, direction=%d, weight=%.3f\n",
           edge2->from_id, edge2->to_id, edge2->direction, edge2->weight);
    
    /* Test local measurements */
    printf("\nLocal measurements (node %s):\n", node1->id);
    printf("  Outgoing weight avg: %.3f\n", node_get_local_outgoing_weight_avg(node1));
    printf("  Incoming weight avg: %.3f\n", node_get_local_incoming_weight_avg(node1));
    
    /* Test activation */
    node1->activation = true;
    node_update_weight_local(node1);
    printf("\nAfter activating node %s:\n", node1->id);
    printf("  Weight: %.3f\n", node1->weight);
    
    /* Cleanup */
    graph_free(g);
    
    printf("\nSystem test complete.\n");
    return 0;
}

