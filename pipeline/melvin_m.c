/*
 * Melvin .m File Format Implementation
 * 
 * Implements binary .m file format using rules from melvin.c
 */

#include "melvin_m.h"
#include "melvin.h"
#include <string.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>

/* ========================================
 * INTERNAL HELPER FUNCTIONS
 * ======================================== */

/* Write header to file */
static bool write_header(FILE *file, const MelvinMHeader *header) {
    if (!file || !header) return false;
    
    if (fseek(file, 0, SEEK_SET) != 0) return false;
    if (fwrite(header, sizeof(MelvinMHeader), 1, file) != 1) return false;
    
    return true;
}

/* Read header from file */
static bool read_header(FILE *file, MelvinMHeader *header) {
    if (!file || !header) return false;
    
    if (fseek(file, 0, SEEK_SET) != 0) return false;
    if (fread(header, sizeof(MelvinMHeader), 1, file) != 1) return false;
    
    /* Validate magic number */
    if (header->magic != MELVIN_M_MAGIC) {
        return false;
    }
    
    return true;
}

/* Write nodes to file */
static bool write_nodes(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Write node count */
    uint64_t node_count = graph->node_count;
    if (fwrite(&node_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Write each node */
    for (size_t i = 0; i < graph->node_count; i++) {
        Node *node = graph->nodes[i];
        if (!node) continue;
        
        /* Write node ID */
        if (fwrite(node->id, 9, 1, file) != 1) return false;
        
        /* Write activation_strength, weight, and bias (replacing bool activation) */
        if (fwrite(&node->activation_strength, sizeof(float), 1, file) != 1) return false;
        if (fwrite(&node->weight, sizeof(float), 1, file) != 1) return false;
        if (fwrite(&node->bias, sizeof(float), 1, file) != 1) return false;
        
        /* Write payload size */
        uint64_t payload_size = node->payload_size;
        if (fwrite(&payload_size, sizeof(uint64_t), 1, file) != 1) return false;
        
        /* Write payload data if exists */
        if (payload_size > 0) {
            if (fwrite(node->payload, 1, payload_size, file) != payload_size) return false;
        }
        
        /* Note: Edge pointers are not stored - they're reconstructed from edges */
    }
    
    return true;
}

/* Read nodes from file */
static bool read_nodes(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Read node count */
    uint64_t node_count;
    if (fread(&node_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Read each node */
    for (uint64_t i = 0; i < node_count; i++) {
        char id[9];
        float activation_strength;
        float weight;
        float bias;
        uint64_t payload_size;
        
        /* Read node ID */
        if (fread(id, 9, 1, file) != 1) return false;
        
        /* Read activation_strength, weight, and bias */
        if (fread(&activation_strength, sizeof(float), 1, file) != 1) return false;
        if (fread(&weight, sizeof(float), 1, file) != 1) return false;
        if (fread(&bias, sizeof(float), 1, file) != 1) return false;
        
        /* Read payload size */
        if (fread(&payload_size, sizeof(uint64_t), 1, file) != 1) return false;
        
        /* Read payload data */
        uint8_t *payload = NULL;
        if (payload_size > 0) {
            payload = (uint8_t*)malloc(payload_size);
            if (!payload) return false;
            if (fread(payload, 1, payload_size, file) != payload_size) {
                free(payload);
                return false;
            }
        }
        
        /* Create node using melvin.c rules */
        Node *node = node_create(payload, payload_size);
        if (!node) {
            if (payload) free(payload);
            return false;
        }
        
        /* Override ID (since node_create generates new one) */
        strncpy(node->id, id, 9);
        node->id[8] = '\0';
        
        /* Set state */
        node->activation_strength = activation_strength;
        node->weight = weight;
        node->bias = bias;
        
        /* Add to graph */
        if (!graph_add_node(graph, node)) {
            node_free(node);
            if (payload) free(payload);
            return false;
        }
        
        if (payload) free(payload);
    }
    
    return true;
}

/* Write edges to file */
static bool write_edges(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Write edge count */
    uint64_t edge_count = graph->edge_count;
    if (fwrite(&edge_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Write each edge */
    for (size_t i = 0; i < graph->edge_count; i++) {
        Edge *edge = graph->edges[i];
        if (!edge) continue;
        
        /* Write edge data (store node IDs from node pointers) */
        if (fwrite(edge->from_node->id, 9, 1, file) != 1) return false;
        if (fwrite(edge->to_node->id, 9, 1, file) != 1) return false;
        if (fwrite(&edge->direction, sizeof(bool), 1, file) != 1) return false;
        if (fwrite(&edge->activation, sizeof(bool), 1, file) != 1) return false;
        if (fwrite(&edge->weight, sizeof(float), 1, file) != 1) return false;
    }
    
    return true;
}

/* Read edges from file */
static bool read_edges(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Read edge count */
    uint64_t edge_count;
    if (fread(&edge_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Read each edge */
    for (uint64_t i = 0; i < edge_count; i++) {
        char from_id[9], to_id[9];
        bool direction, activation;
        float weight;
        
        /* Read edge data */
        if (fread(from_id, 9, 1, file) != 1) return false;
        if (fread(to_id, 9, 1, file) != 1) return false;
        if (fread(&direction, sizeof(bool), 1, file) != 1) return false;
        if (fread(&activation, sizeof(bool), 1, file) != 1) return false;
        if (fread(&weight, sizeof(float), 1, file) != 1) return false;
        
        /* Find nodes by ID (temporary helper for file loading only) */
        Node *from = NULL, *to = NULL;
        for (size_t j = 0; j < graph->node_count; j++) {
            if (graph->nodes[j] && strcmp(graph->nodes[j]->id, from_id) == 0) {
                from = graph->nodes[j];
            }
            if (graph->nodes[j] && strcmp(graph->nodes[j]->id, to_id) == 0) {
                to = graph->nodes[j];
            }
        }
        
        if (!from || !to) continue; /* Skip invalid edges */
        
        /* Create edge using melvin.c rules */
        Edge *edge = edge_create(from, to, direction);
        if (!edge) continue;
        
        /* Set state */
        edge->activation = activation;
        edge->weight = weight;
        
        /* Add to graph */
        graph_add_edge(graph, edge, from, to);
    }
    
    return true;
}

/* Write universal output buffer */
static bool write_universal_output(FILE *file, const uint8_t *data, size_t size, uint64_t offset) {
    if (!file) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Write size */
    uint64_t data_size = size;
    if (fwrite(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Write data */
    if (size > 0 && data) {
        if (fwrite(data, 1, size, file) != size) return false;
    }
    
    return true;
}

/* Read universal output buffer */
static bool read_universal_output(FILE *file, uint8_t **data, size_t *size, uint64_t offset) {
    if (!file || !data || !size) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Read size */
    uint64_t data_size;
    if (fread(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    *size = data_size;
    if (data_size == 0) {
        *data = NULL;
        return true;
    }
    
    /* Allocate and read data */
    *data = (uint8_t*)malloc(data_size);
    if (!*data) return false;
    
    if (fread(*data, 1, data_size, file) != data_size) {
        free(*data);
        *data = NULL;
        return false;
    }
    
    return true;
}

/* Write universal input buffer */
static bool write_universal_input(FILE *file, const uint8_t *data, size_t size, uint64_t offset) {
    if (!file) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Write size */
    uint64_t data_size = size;
    if (fwrite(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    /* Write data */
    if (size > 0 && data) {
        if (fwrite(data, 1, size, file) != size) return false;
    }
    
    return true;
}

/* Read universal input buffer */
static bool read_universal_input(FILE *file, uint8_t **data, size_t *size, uint64_t offset) {
    if (!file || !data || !size) return false;
    
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    /* Read size */
    uint64_t data_size;
    if (fread(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    *size = data_size;
    if (data_size == 0) {
        *data = NULL;
        return true;
    }
    
    /* Allocate and read data */
    *data = (uint8_t*)malloc(data_size);
    if (!*data) return false;
    
    if (fread(*data, 1, data_size, file) != data_size) {
        free(*data);
        *data = NULL;
        return false;
    }
    
    return true;
}

/* Calculate file offsets (adaptive layout) */
static void calculate_offsets(MelvinMHeader *header) {
    uint64_t offset = sizeof(MelvinMHeader);
    
    /* Nodes section */
    header->nodes_offset = offset;
    offset += sizeof(uint64_t); /* Node count */
    /* Actual node data size calculated during write */
    
    /* Edges section (after nodes) */
    header->edges_offset = offset;
    offset += sizeof(uint64_t); /* Edge count */
    /* Actual edge data size calculated during write */
    
    /* Universal input section */
    header->universal_input_offset = offset;
    offset += sizeof(uint64_t); /* Size */
    offset += header->universal_input_size;
    
    /* Universal output section */
    header->universal_output_offset = offset;
    offset += sizeof(uint64_t); /* Size */
    offset += header->universal_output_size;
    
    /* Payloads are stored inline with nodes */
    header->payloads_offset = 0; /* Not used separately */
}

/* ========================================
 * PUBLIC API IMPLEMENTATION
 * ======================================== */

MelvinMFile* melvin_m_create(const char *filename) {
    if (!filename) return NULL;
    
    MelvinMFile *mfile = (MelvinMFile*)calloc(1, sizeof(MelvinMFile));
    if (!mfile) return NULL;
    
    mfile->filename = strdup(filename);
    if (!mfile->filename) {
        free(mfile);
        return NULL;
    }
    
    /* Create new file */
    mfile->file = fopen(filename, "wb+");
    if (!mfile->file) {
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Initialize header */
    mfile->header.magic = MELVIN_M_MAGIC;
    mfile->header.version = MELVIN_M_VERSION;
    mfile->header.flags = 0;
    mfile->header.node_count = 0;
    mfile->header.edge_count = 0;
    mfile->header.universal_input_size = 0;
    mfile->header.universal_output_size = 0;
    mfile->header.last_modified = (uint64_t)time(NULL);
    mfile->header.adaptation_count = 0;
    
    /* Create graph using melvin.c rules */
    mfile->graph = graph_create();
    if (!mfile->graph) {
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Initialize universal input */
    mfile->universal_input_capacity = 1024;
    mfile->universal_input = (uint8_t*)calloc(mfile->universal_input_capacity, 1);
    if (!mfile->universal_input) {
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Initialize universal output */
    mfile->universal_output_capacity = 1024;
    mfile->universal_output = (uint8_t*)calloc(mfile->universal_output_capacity, 1);
    if (!mfile->universal_output) {
        free(mfile->universal_input);
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    mfile->is_dirty = true;
    mfile->last_input_port_id = 0;  /* Initialize port ID tracking */
    
    return mfile;
}

MelvinMFile* melvin_m_open(const char *filename) {
    if (!filename) return NULL;
    
    MelvinMFile *mfile = (MelvinMFile*)calloc(1, sizeof(MelvinMFile));
    if (!mfile) return NULL;
    
    mfile->filename = strdup(filename);
    if (!mfile->filename) {
        free(mfile);
        return NULL;
    }
    
    /* Open existing file */
    mfile->file = fopen(filename, "rb+");
    if (!mfile->file) {
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Read header */
    if (!read_header(mfile->file, &mfile->header)) {
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Create graph using melvin.c rules */
    mfile->graph = graph_create();
    if (!mfile->graph) {
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Read nodes */
    if (!read_nodes(mfile->file, mfile->graph, mfile->header.nodes_offset)) {
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Read edges */
    if (!read_edges(mfile->file, mfile->graph, mfile->header.edges_offset)) {
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    
    /* Read universal input */
    size_t input_size;
    if (!read_universal_input(mfile->file, &mfile->universal_input, &input_size, 
                              mfile->header.universal_input_offset)) {
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    mfile->universal_input_capacity = input_size > 1024 ? input_size : 1024;
    
    /* Read universal output */
    size_t output_size;
    if (!read_universal_output(mfile->file, &mfile->universal_output, &output_size,
                               mfile->header.universal_output_offset)) {
        if (mfile->universal_input) free(mfile->universal_input);
        graph_free(mfile->graph);
        fclose(mfile->file);
        free(mfile->filename);
        free(mfile);
        return NULL;
    }
    mfile->universal_output_capacity = output_size > 1024 ? output_size : 1024;
    mfile->last_input_port_id = 0;  /* Initialize port ID tracking */
    
    mfile->is_dirty = false;
    
    return mfile;
}

bool melvin_m_save(MelvinMFile *mfile) {
    if (!mfile || !mfile->file) return false;
    
    /* Update header with current state */
    mfile->header.node_count = mfile->graph->node_count;
    mfile->header.edge_count = mfile->graph->edge_count;
    mfile->header.universal_input_size = 0; /* Calculate actual size */
    if (mfile->universal_input) {
        /* Find actual used size (simplified - could track this better) */
        mfile->header.universal_input_size = mfile->universal_input_capacity;
    }
    mfile->header.universal_output_size = 0; /* Calculate actual size */
    if (mfile->universal_output) {
        /* Find actual used size (simplified - could track this better) */
        mfile->header.universal_output_size = mfile->universal_output_capacity;
    }
    
    mfile->header.last_modified = (uint64_t)time(NULL);
    mfile->header.adaptation_count++;
    
    /* Calculate offsets */
    calculate_offsets(&mfile->header);
    
    /* Write header */
    if (!write_header(mfile->file, &mfile->header)) return false;
    
    /* Write nodes */
    if (!write_nodes(mfile->file, mfile->graph, mfile->header.nodes_offset)) return false;
    
    /* Write edges */
    if (!write_edges(mfile->file, mfile->graph, mfile->header.edges_offset)) return false;
    
    /* Write universal input */
    if (!write_universal_input(mfile->file, mfile->universal_input, 
                               mfile->header.universal_input_size,
                               mfile->header.universal_input_offset)) return false;
    
    /* Write universal output */
    if (!write_universal_output(mfile->file, mfile->universal_output,
                                mfile->header.universal_output_size,
                                mfile->header.universal_output_offset)) return false;
    
    fflush(mfile->file);
    mfile->is_dirty = false;
    
    return true;
}

void melvin_m_close(MelvinMFile *mfile) {
    if (!mfile) return;
    
    /* Save if dirty */
    if (mfile->is_dirty) {
        melvin_m_save(mfile);
    }
    
    if (mfile->file) {
        fclose(mfile->file);
    }
    
    if (mfile->graph) {
        graph_free(mfile->graph);
    }
    
    if (mfile->universal_input) {
        free(mfile->universal_input);
    }
    
    if (mfile->universal_output) {
        free(mfile->universal_output);
    }
    
    if (mfile->filename) {
        free(mfile->filename);
    }
    
    free(mfile);
}

/* Universal Input Operations */
bool melvin_m_universal_input_write(MelvinMFile *mfile, const uint8_t *data, size_t size) {
    if (!mfile || !data || size == 0) return false;
    
    /* Resize if needed */
    if (size > mfile->universal_input_capacity) {
        size_t new_capacity = size * 2;
        uint8_t *new_input = (uint8_t*)realloc(mfile->universal_input, new_capacity);
        if (!new_input) return false;
        mfile->universal_input = new_input;
        mfile->universal_input_capacity = new_capacity;
    }
    
    /* Write data */
    memcpy(mfile->universal_input, data, size);
    mfile->header.universal_input_size = size;
    melvin_m_mark_dirty(mfile);
    
    return true;
}

size_t melvin_m_universal_input_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size) {
    if (!mfile || !buffer || buffer_size == 0) return 0;
    
    size_t read_size = buffer_size < mfile->header.universal_input_size ? 
                       buffer_size : mfile->header.universal_input_size;
    
    if (mfile->universal_input && read_size > 0) {
        memcpy(buffer, mfile->universal_input, read_size);
    }
    
    return read_size;
}

size_t melvin_m_universal_input_size(MelvinMFile *mfile) {
    if (!mfile) return 0;
    return mfile->header.universal_input_size;
}

void melvin_m_universal_input_clear(MelvinMFile *mfile) {
    if (!mfile) return;
    
    if (mfile->universal_input) {
        memset(mfile->universal_input, 0, mfile->universal_input_capacity);
    }
    mfile->header.universal_input_size = 0;
    melvin_m_mark_dirty(mfile);
}

/* Universal Output Operations (Wave Propagation Results) */
size_t melvin_m_universal_output_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size) {
    if (!mfile || !buffer || buffer_size == 0) return 0;
    
    size_t read_size = buffer_size < mfile->header.universal_output_size ? 
                       buffer_size : mfile->header.universal_output_size;
    
    if (mfile->universal_output && read_size > 0) {
        memcpy(buffer, mfile->universal_output, read_size);
    }
    
    return read_size;
}

size_t melvin_m_universal_output_size(MelvinMFile *mfile) {
    if (!mfile) return 0;
    return mfile->header.universal_output_size;
}

void melvin_m_universal_output_clear(MelvinMFile *mfile) {
    if (!mfile) return;
    
    if (mfile->universal_output) {
        memset(mfile->universal_output, 0, mfile->universal_output_capacity);
    }
    mfile->header.universal_output_size = 0;
    melvin_m_mark_dirty(mfile);
}

/* Graph Operations */
MelvinGraph* melvin_m_get_graph(MelvinMFile *mfile) {
    if (!mfile) return NULL;
    return mfile->graph;
}

Node* melvin_m_add_node(MelvinMFile *mfile, const uint8_t *payload_data, size_t payload_size) {
    if (!mfile) return NULL;
    
    Node *node = node_create(payload_data, payload_size);
    if (!node) return NULL;
    
    if (graph_add_node(mfile->graph, node)) {
        melvin_m_mark_dirty(mfile);
        return node;
    }
    
    node_free(node);
    return NULL;
}

Edge* melvin_m_add_edge(MelvinMFile *mfile, Node *from, Node *to, bool direction) {
    if (!mfile || !from || !to) return NULL;
    
    Edge *edge = edge_create(from, to, direction);
    if (!edge) return NULL;
    
    if (graph_add_edge(mfile->graph, edge, from, to)) {
        melvin_m_mark_dirty(mfile);
        return edge;
    }
    
    edge_free(edge);
    return NULL;
}

bool melvin_m_process_input(MelvinMFile *mfile) {
    if (!mfile || !mfile->graph) return false;
    
    /* Clear universal output */
    melvin_m_universal_output_clear(mfile);
    
    /* Extract input port ID from input buffer (CAN bus format: first byte is port_id) */
    /* This is ephemeral context for routing output to correct port */
    /* Port ID stays in payload for pattern learning (unified graph), but we track it */
    /* separately for I/O routing purposes */
    mfile->last_input_port_id = 0;  /* Default: no port ID */
    if (mfile->universal_input && mfile->header.universal_input_size > 0) {
        mfile->last_input_port_id = mfile->universal_input[0];  /* First byte = port_id */
    }
    
    /* DON'T reset activations immediately - let wave propagation use them as seeds */
    /* Activations will be used for wave exploration to find existing nodes */
    /* They'll get reset naturally as new processing activates different nodes */
    
    /* Track initially activated nodes (those matching input) */
    Node **initial_nodes = NULL;
    size_t initial_count = 0;
    
    /* STEP 1: Process input data to find sequential patterns and activate/create nodes */
    /* This creates nodes for sequential parts of the data (e.g., "CAT" -> nodes for C, A, T) */
    Node **seq_nodes = NULL;
    size_t seq_count = 0;
    if (mfile->universal_input && mfile->header.universal_input_size > 0) {
        seq_nodes = wave_process_sequential_patterns(mfile->graph, 
            mfile->universal_input, mfile->header.universal_input_size, &seq_count);
        
        if (seq_nodes && seq_count > 0) {
            /* Use sequential nodes as initial nodes */
            initial_nodes = seq_nodes;
            initial_count = seq_count;
        }
    }
    
    /* STEP 2: Create edges from all mechanisms (intelligent edge formation) */
    /* Co-activation, similarity, context, hierarchy, and homeostatic edges */
    /* Multiple mechanisms create rich graph structure for semantic understanding */
    if (initial_nodes && initial_count > 0) {
        wave_form_intelligent_edges(mfile->graph, initial_nodes, initial_count, NULL, NULL);
    }
    
    /* STEP 3: Multi-step wave propagation (unified - all abstraction levels) */
    /* Now wave can propagate along the edges we just created */
    if (initial_nodes && initial_count > 0) {
        wave_propagate_multi_step(mfile->graph, initial_nodes, initial_count);
    }
    
    /* STEP 4: Hierarchy formation emerges naturally from edge weight growth during co-activation */
    /* No explicit check needed - hierarchy forms implicitly when patterns repeat and edges strengthen */
    /* See wave_create_edges_from_coactivation() for natural hierarchy emergence */
    
    /* STEP 5: Output collection from direct input nodes and sequential continuations only */
    /* KEY: Activation (exploration) != Output (intent) */
    /* Wave propagation activates nodes for context/exploration, but output should only */
    /* come from direct input nodes and learned sequential patterns (co-activation edges) */
    uint8_t *output = NULL;
    size_t output_size = 0;
    wave_collect_output(mfile->graph, initial_nodes, initial_count, &output, &output_size);
    
    if (output && output_size > 0) {
        if (output_size > mfile->universal_output_capacity) {
            mfile->universal_output_capacity = output_size * 2;
            mfile->universal_output = (uint8_t*)realloc(mfile->universal_output, mfile->universal_output_capacity);
            if (!mfile->universal_output) {
                free(output);
                if (seq_nodes) free(seq_nodes);
                return false;
            }
        }
        memcpy(mfile->universal_output, output, output_size);
        mfile->header.universal_output_size = output_size;
        free(output);
    } else {
        mfile->header.universal_output_size = 0;
    }
    
    if (seq_nodes) free(seq_nodes);
    melvin_m_mark_dirty(mfile);
    
    return true;
}

/* Adaptive Operations */
void melvin_m_mark_dirty(MelvinMFile *mfile) {
    if (mfile) {
        mfile->is_dirty = true;
    }
}

bool melvin_m_is_dirty(MelvinMFile *mfile) {
    if (!mfile) return false;
    return mfile->is_dirty;
}

uint64_t melvin_m_get_adaptation_count(MelvinMFile *mfile) {
    if (!mfile) return 0;
    return mfile->header.adaptation_count;
}

/* ========================================
 * PORT ROUTING OPERATIONS
 * ======================================== */

/* Get last input port ID from the most recent process_input call (for output routing) */
/* Port ID is extracted from input buffer (CAN bus format: first byte is port_id) */
/* This is ephemeral context - used to route output to correct port based on input port */
uint8_t melvin_m_get_last_input_port_id(MelvinMFile *mfile) {
    if (!mfile) return 0;
    return mfile->last_input_port_id;
}

