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
 * .M FILE FORMAT CONSTANTS
 * ======================================== */

/* Magic number for .m files: "MELVIN\0\0" */
#define MELVIN_M_MAGIC 0x4D454C56494E0000ULL  /* "MELVIN\0\0" in ASCII */
#define MELVIN_M_VERSION 1

/* Note: MelvinMHeader and MelvinMFile structures are defined in melvin.h */
/* This header provides the .m file operations API */

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

/* ========================================
 * PORT ROUTING OPERATIONS
 * ======================================== */

/* Get last input port ID from the most recent process_input call (for output routing) */
/* Port ID is extracted from input buffer (CAN bus format: first byte is port_id) */
uint8_t melvin_m_get_last_input_port_id(MelvinMFile *mfile);

#endif /* MELVIN_M_H */

