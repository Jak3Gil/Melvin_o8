/*
 * Melvin .m File Format
 * 
 * Binary file format for storing and adapting Melvin graphs
 * - Uses rules from melvin.c
 * - Binary storage for efficiency
 * - Adaptive structure (file evolves with graph)
 * - Universal input mechanism
 * - External port definitions
 */

#ifndef MELVIN_M_H
#define MELVIN_M_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "melvin.h"

/* ========================================
 * .M FILE FORMAT STRUCTURE
 * ======================================== */

/* Magic number for .m files: "MELVIN\0\0" */
#define MELVIN_M_MAGIC 0x4D454C56494E0000ULL  /* "MELVIN\0\0" in ASCII */
#define MELVIN_M_VERSION 1

/* .m File Header */
typedef struct MelvinMHeader {
    uint64_t magic;         /* Magic number: MELVIN_M_MAGIC */
    uint32_t version;       /* File format version */
    uint32_t flags;         /* Format flags */
    
    /* Graph structure */
    uint64_t node_count;    /* Number of nodes */
    uint64_t edge_count;    /* Number of edges */
    
    /* Universal input buffer (any binary input) */
    uint64_t universal_input_size;  /* Size of universal input buffer */
    uint64_t universal_input_offset; /* Offset to universal input data */
    
    /* Universal output buffer (wave propagation output) */
    uint64_t universal_output_size;  /* Size of universal output buffer */
    uint64_t universal_output_offset; /* Offset to universal output data */
    
    /* Data offsets */
    uint64_t nodes_offset;  /* Offset to node data section */
    uint64_t edges_offset;  /* Offset to edge data section */
    uint64_t payloads_offset; /* Offset to payload data section */
    
    /* Adaptive metadata */
    uint64_t last_modified; /* Timestamp of last modification */
    uint64_t adaptation_count; /* Number of adaptations */
} MelvinMHeader;

/* .m File Context */
typedef struct MelvinMFile {
    FILE *file;
    char *filename;
    MelvinMHeader header;
    MelvinGraph *graph;     /* In-memory graph (uses melvin.c rules) */
    uint8_t *universal_input; /* Universal input buffer (any binary input) */
    size_t universal_input_capacity;
    uint8_t *universal_output; /* Universal output buffer (wave propagation output) */
    size_t universal_output_capacity;
    bool is_dirty;           /* True if file needs to be saved */
} MelvinMFile;

/* ========================================
 * .M FILE OPERATIONS
 * ======================================== */

/* Create a new .m file */
MelvinMFile* melvin_m_create(const char *filename);

/* Open an existing .m file */
MelvinMFile* melvin_m_open(const char *filename);

/* Save .m file (adaptive write - updates file structure) */
bool melvin_m_save(MelvinMFile *mfile);

/* Close .m file */
void melvin_m_close(MelvinMFile *mfile);

/* ========================================
 * UNIVERSAL INPUT OPERATIONS
 * ======================================== */

/* Write data to universal input buffer */
bool melvin_m_universal_input_write(MelvinMFile *mfile, const uint8_t *data, size_t size);

/* Read data from universal input buffer */
size_t melvin_m_universal_input_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size);

/* Get universal input buffer size */
size_t melvin_m_universal_input_size(MelvinMFile *mfile);

/* Clear universal input buffer */
void melvin_m_universal_input_clear(MelvinMFile *mfile);

/* ========================================
 * UNIVERSAL OUTPUT OPERATIONS (Wave Propagation)
 * ======================================== */

/* Read data from universal output buffer (wave propagation results) */
size_t melvin_m_universal_output_read(MelvinMFile *mfile, uint8_t *buffer, size_t buffer_size);

/* Get universal output buffer size */
size_t melvin_m_universal_output_size(MelvinMFile *mfile);

/* Clear universal output buffer */
void melvin_m_universal_output_clear(MelvinMFile *mfile);

/* ========================================
 * GRAPH OPERATIONS (using melvin.c rules)
 * ======================================== */

/* Get the graph from .m file (uses melvin.c structures) */
MelvinGraph* melvin_m_get_graph(MelvinMFile *mfile);

/* Add node to .m file (uses melvin.c node_create) */
Node* melvin_m_add_node(MelvinMFile *mfile, const uint8_t *payload_data, size_t payload_size);

/* Add edge to .m file (uses melvin.c edge_create) */
Edge* melvin_m_add_edge(MelvinMFile *mfile, Node *from, Node *to, bool direction);

/* Process universal input through graph via wave propagation (writes to universal output) */
bool melvin_m_process_input(MelvinMFile *mfile);

/* ========================================
 * ADAPTIVE OPERATIONS
 * ======================================== */

/* Mark file as needing adaptation */
void melvin_m_mark_dirty(MelvinMFile *mfile);

/* Check if file needs saving */
bool melvin_m_is_dirty(MelvinMFile *mfile);

/* Get adaptation count */
uint64_t melvin_m_get_adaptation_count(MelvinMFile *mfile);

#endif /* MELVIN_M_H */

