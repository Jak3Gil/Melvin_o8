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
        
        /* Write activation and weight */
        if (fwrite(&node->activation, sizeof(bool), 1, file) != 1) return false;
        if (fwrite(&node->weight, sizeof(float), 1, file) != 1) return false;
        
        /* Write payload size */
        uint64_t payload_size = node->payload_size;
        if (fwrite(&payload_size, sizeof(uint64_t), 1, file) != 1) return false;
        
        /* Write payload data if exists */
        if (node->payload && payload_size > 0) {
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
        bool activation;
        float weight;
        uint64_t payload_size;
        
        /* Read node ID */
        if (fread(id, 9, 1, file) != 1) return false;
        
        /* Read activation and weight */
        if (fread(&activation, sizeof(bool), 1, file) != 1) return false;
        if (fread(&weight, sizeof(float), 1, file) != 1) return false;
        
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
        node->activation = activation;
        node->weight = weight;
        
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
        
        /* Write edge data */
        if (fwrite(edge->from_id, 9, 1, file) != 1) return false;
        if (fwrite(edge->to_id, 9, 1, file) != 1) return false;
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
        
        /* Find nodes */
        Node *from = graph_find_node(graph, from_id);
        Node *to = graph_find_node(graph, to_id);
        
        if (!from || !to) continue; /* Skip invalid edges */
        
        /* Create edge using melvin.c rules */
        Edge *edge = edge_create(from, to, direction);
        if (!edge) continue;
        
        /* Set state */
        edge->activation = activation;
        edge->weight = weight;
        
        /* Add to graph */
        graph_add_edge(graph, edge);
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
    
    if (graph_add_edge(mfile->graph, edge)) {
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
    
    /* Process universal input through graph via wave propagation */
    /* Feed input into graph: activate nodes based on input data */
    if (mfile->universal_input && mfile->header.universal_input_size > 0) {
        /* For each node, check if input matches/activates it */
        for (size_t i = 0; i < mfile->graph->node_count; i++) {
            Node *node = mfile->graph->nodes[i];
            if (!node) continue;
            
            /* Simple activation: if node payload matches input pattern */
            /* In a full implementation, this would be more sophisticated */
            if (node->payload && node->payload_size > 0) {
                /* Activate node if payload matches input (simplified) */
                if (node->payload_size <= mfile->header.universal_input_size) {
                    if (memcmp(node->payload, mfile->universal_input, node->payload_size) == 0) {
                        node->activation = true;
                    }
                }
            }
        }
    }
    
    /* Run wave propagation from all activated nodes */
    for (size_t i = 0; i < mfile->graph->node_count; i++) {
        Node *node = mfile->graph->nodes[i];
        if (node && node->activation) {
            wave_propagate_from_node(node);
        }
    }
    
    /* Collect output: gather activated nodes' payloads into universal_output */
    size_t output_offset = 0;
    for (size_t i = 0; i < mfile->graph->node_count; i++) {
        Node *node = mfile->graph->nodes[i];
        if (!node || !node->activation) continue;
        
        if (node->payload && node->payload_size > 0) {
            /* Resize output buffer if needed */
            if (output_offset + node->payload_size > mfile->universal_output_capacity) {
                size_t new_capacity = (output_offset + node->payload_size) * 2;
                uint8_t *new_output = (uint8_t*)realloc(mfile->universal_output, new_capacity);
                if (!new_output) break;
                mfile->universal_output = new_output;
                mfile->universal_output_capacity = new_capacity;
            }
            
            /* Copy node payload to output */
            memcpy(mfile->universal_output + output_offset, node->payload, node->payload_size);
            output_offset += node->payload_size;
        }
    }
    
    mfile->header.universal_output_size = output_offset;
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

