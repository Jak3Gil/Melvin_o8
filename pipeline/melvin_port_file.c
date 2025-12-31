/*
 * Melvin Unified File Port Implementation
 * 
 * Production-ready file-based port system for brain.m
 * Handles both input (reading from files) and output (writing to files)
 * 
 * ========================================================================
 * HOW PORTS CONNECT TO BRAIN.M
 * ========================================================================
 * 
 * Port Architecture:
 * - Ports operate OUTSIDE the .m file (external layer)
 * - All ports managed by MelvinPortManager (one manager per .m file)
 * - Ports package data in CAN bus format before sending to brain.m
 * - Brain.m processes bytes through unified graph (no port separation inside)
 * - Routing table maps input_port_id → output_port_id for output delivery
 * 
 * Data Flow:
 * 1. Input Port reads data → packages as PortFrame with port_id
 * 2. PortFrame serialized → written to brain.m universal_input
 * 3. brain.m processes through unified graph (all data types together)
 * 4. Output collected from graph → written to brain.m universal_output
 * 5. Routing table determines output_port_id based on input_port_id
 * 6. Output Port receives data → writes to file/device
 * 
 * Cross-Modal Associations:
 * - All nodes (audio, text, video) exist in unified graph
 * - Nodes store actual data bytes (data type preserved)
 * - Edges connect nodes across modalities:
 *   * Co-activation: nodes that activate together → edges form
 *   * Similarity: similar patterns → edges form
 *   * Context: wave propagation paths → edges form
 * - Audio input can influence text output (and vice versa) through edges
 * - Output routing determines delivery, but graph learns cross-modal associations
 * 
 * Port Pairs Example:
 * - Input Port 1 (dataset file) → routes to → Output Port 2 (text output file)
 * - Input Port 3 (USB mic) → routes to → Output Port 4 (USB speaker)
 * - All flow through same brain.m unified graph
 */

#include "melvin_ports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

/* ========================================================================
 * FILE PORT STATE STRUCTURE
 * ======================================================================== */

/* File Port State (shared for input and output) */
typedef struct {
    FILE *file_handle;
    bool is_eof;              /* For input ports only */
    size_t chunk_size;        /* For input ports: read chunk size */
    size_t total_bytes_read;  /* Statistics for input ports */
    size_t total_bytes_written; /* Statistics for output ports */
    size_t total_chunks_read;  /* Statistics for input ports */
    size_t total_chunks_written; /* Statistics for output ports */
    bool loop_on_eof;         /* For input ports: loop file on EOF */
    bool append_mode;         /* For output ports: append vs overwrite */
} FilePortState;

/* ========================================================================
 * SHARED HELPER FUNCTIONS
 * ======================================================================== */

/* Parse device_path to extract file path, chunk_size, and flags
 * Format: "filepath" or "filepath:chunk_size" or "filepath:chunk_size:loop"
 *         or "filepath:append" or "filepath:overwrite" (for output)
 */
static void parse_file_device_path(const char *device_path, 
                                   char *file_path, 
                                   size_t file_path_size,
                                   size_t *chunk_size,
                                   bool *loop_on_eof,
                                   bool *append_mode) {
    if (!device_path || !file_path || file_path_size == 0) return;
    
    *chunk_size = 4096;  /* Default chunk size */
    *loop_on_eof = false;
    *append_mode = false;  /* Default: overwrite mode */
    
    /* Find first colon */
    char *first_colon = strchr(device_path, ':');
    if (!first_colon) {
        /* No colon, use path as-is */
        strncpy(file_path, device_path, file_path_size - 1);
        file_path[file_path_size - 1] = '\0';
        return;
    }
    
    /* Extract file path (everything before first colon) */
    size_t path_len = first_colon - device_path;
    if (path_len >= file_path_size) path_len = file_path_size - 1;
    memcpy(file_path, device_path, path_len);
    file_path[path_len] = '\0';
    
    /* Check for output mode flags */
    if (strstr(device_path, ":append") != NULL) {
        *append_mode = true;
        return;  /* Output mode flag, no chunk size needed */
    }
    if (strstr(device_path, ":overwrite") != NULL) {
        *append_mode = false;
        return;  /* Explicit overwrite mode */
    }
    
    /* Check for loop flag (input ports) */
    if (strstr(device_path, ":loop") != NULL) {
        *loop_on_eof = true;
    }
    
    /* Extract chunk size (between first colon and second colon or end) */
    char *second_colon = strchr(first_colon + 1, ':');
    if (second_colon && strstr(second_colon + 1, ":loop") == NULL) {
        /* Chunk size between first and second colon (if second colon isn't just ":loop") */
        char chunk_str[32];
        size_t chunk_len = second_colon - (first_colon + 1);
        if (chunk_len < sizeof(chunk_str)) {
            memcpy(chunk_str, first_colon + 1, chunk_len);
            chunk_str[chunk_len] = '\0';
            *chunk_size = (size_t)strtoul(chunk_str, NULL, 10);
            if (*chunk_size == 0) *chunk_size = 4096;
        }
    } else {
        /* Only chunk size, no second colon (or second colon is just ":loop") */
        *chunk_size = (size_t)strtoul(first_colon + 1, NULL, 10);
        if (*chunk_size == 0) *chunk_size = 4096;
    }
}

/* ========================================================================
 * FILE INPUT PORT (Reads from file, sends to brain.m)
 * ======================================================================== */

/* Open file input port */
static bool file_input_open(MelvinPort *port) {
    if (!port) return false;
    
    FilePortState *state = (FilePortState*)calloc(1, sizeof(FilePortState));
    if (!state) return false;
    
    char file_path[512];
    size_t chunk_size;
    bool loop_on_eof;
    bool append_mode;  /* Not used for input, but needed for parsing */
    
    parse_file_device_path(port->device_path, file_path, sizeof(file_path),
                          &chunk_size, &loop_on_eof, &append_mode);
    
    /* Open file for reading */
    state->file_handle = fopen(file_path, "rb");
    if (!state->file_handle) {
        free(state);
        return false;
    }
    
    state->chunk_size = chunk_size;
    state->is_eof = false;
    state->loop_on_eof = loop_on_eof;
    state->total_bytes_read = 0;
    state->total_chunks_read = 0;
    state->append_mode = false;  /* Not applicable for input */
    
    port->device_handle = state;
    return true;
}

/* Close file input port */
static void file_input_close(MelvinPort *port) {
    if (!port || !port->device_handle) return;
    
    FilePortState *state = (FilePortState*)port->device_handle;
    if (state->file_handle) {
        fclose(state->file_handle);
        state->file_handle = NULL;
    }
    free(state);
    port->device_handle = NULL;
}

/* Read chunk from file input port */
static size_t file_input_read(MelvinPort *port, uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    FilePortState *state = (FilePortState*)port->device_handle;
    if (!state->file_handle) return 0;
    
    /* Always try to read first - don't check EOF beforehand */
    size_t read_size = (size < state->chunk_size) ? size : state->chunk_size;
    size_t bytes_read = fread(buffer, 1, read_size, state->file_handle);
    
    /* If we successfully read data */
    if (bytes_read > 0) {
        state->total_bytes_read += bytes_read;
        state->total_chunks_read++;
        state->is_eof = false;
        return bytes_read;
    }
    
    /* No data read - check if we hit EOF */
    bool at_eof = feof(state->file_handle);
    bool has_error = ferror(state->file_handle);
    
    if (at_eof || has_error) {
        if (state->loop_on_eof) {
            /* EOF or error reached and looping enabled - rewind and read again */
            rewind(state->file_handle);
            clearerr(state->file_handle);  /* Clear EOF and error flags */
            state->is_eof = false;
            
            /* Immediately try reading again after rewind */
            bytes_read = fread(buffer, 1, read_size, state->file_handle);
            
            if (bytes_read > 0) {
                /* Successfully read after rewind */
                state->total_bytes_read += bytes_read;
                state->total_chunks_read++;
                return bytes_read;
            }
            /* If still 0 after rewind, return 0 but keep is_eof false */
            /* This allows the next call to try again */
            state->is_eof = false;
            return 0;
        } else {
            /* Not looping - EOF/error reached */
            state->is_eof = true;
            return 0;
        }
    }
    
    /* Not at EOF/error but no data - very unusual, but for looping ports allow retry */
    if (state->loop_on_eof) {
        state->is_eof = false;  /* Don't block future reads */
    } else {
        state->is_eof = true;
    }
    return 0;
}

/* Write to file input port (not supported - input only) */
static size_t file_input_write(MelvinPort *port, const uint8_t *buffer, size_t size) {
    (void)port;
    (void)buffer;
    (void)size;
    return 0;  /* Input ports are read-only */
}

/* ========================================================================
 * FILE OUTPUT PORT (Receives from brain.m, writes to file)
 * ======================================================================== */

/* Open file output port */
static bool file_output_open(MelvinPort *port) {
    if (!port) return false;
    
    FilePortState *state = (FilePortState*)calloc(1, sizeof(FilePortState));
    if (!state) return false;
    
    char file_path[512];
    size_t chunk_size;  /* Not used for output, but needed for parsing */
    bool loop_on_eof;   /* Not used for output */
    bool append_mode;
    
    parse_file_device_path(port->device_path, file_path, sizeof(file_path),
                          &chunk_size, &loop_on_eof, &append_mode);
    
    /* Open file for writing (append or overwrite mode) */
    const char *mode = append_mode ? "ab" : "wb";
    state->file_handle = fopen(file_path, mode);
    if (!state->file_handle) {
        free(state);
        return false;
    }
    
    state->append_mode = append_mode;
    state->is_eof = false;  /* Not applicable for output */
    state->loop_on_eof = false;  /* Not applicable for output */
    state->chunk_size = 0;  /* Not used for output */
    state->total_bytes_written = 0;
    state->total_chunks_written = 0;
    state->total_bytes_read = 0;  /* Not applicable for output */
    state->total_chunks_read = 0;  /* Not applicable for output */
    
    port->device_handle = state;
    return true;
}

/* Close file output port */
static void file_output_close(MelvinPort *port) {
    if (!port || !port->device_handle) return;
    
    FilePortState *state = (FilePortState*)port->device_handle;
    if (state->file_handle) {
        fclose(state->file_handle);
        state->file_handle = NULL;
    }
    free(state);
    port->device_handle = NULL;
}

/* Read from file output port (not supported - output only) */
static size_t file_output_read(MelvinPort *port, uint8_t *buffer, size_t size) {
    (void)port;
    (void)buffer;
    (void)size;
    return 0;  /* Output ports are write-only */
}

/* Write data to file output port */
static size_t file_output_write(MelvinPort *port, const uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    FilePortState *state = (FilePortState*)port->device_handle;
    if (!state->file_handle) return 0;
    
    /* Write data to file */
    size_t bytes_written = fwrite(buffer, 1, size, state->file_handle);
    
    if (bytes_written > 0) {
        state->total_bytes_written += bytes_written;
        state->total_chunks_written++;
        fflush(state->file_handle);  /* Ensure data is written */
    }
    
    return bytes_written;
}

/* ========================================================================
 * PORT REGISTRATION FUNCTIONS
 * ======================================================================== */

/* Register file input port (reads from file, sends to brain.m) */
MelvinPort* melvin_port_register_file_input(MelvinPortManager *manager,
                                            const char *file_path,
                                            uint8_t port_id,
                                            size_t chunk_size) {
    if (!manager || !file_path) return NULL;
    
    /* Build device_path string with chunk_size */
    char device_path[512];
    if (chunk_size > 0) {
        snprintf(device_path, sizeof(device_path), "%s:%zu", file_path, chunk_size);
    } else {
        strncpy(device_path, file_path, sizeof(device_path) - 1);
        device_path[sizeof(device_path) - 1] = '\0';
    }
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_DATASET_FILE, device_path, port_id);
    if (!port) return NULL;
    
    /* Set callbacks for input port */
    port->open_func = file_input_open;
    port->close_func = file_input_close;
    port->read_func = file_input_read;
    port->write_func = NULL;  /* Input ports are read-only */
    
    return port;
}

/* Register file input port with loop on EOF */
MelvinPort* melvin_port_register_file_input_loop(MelvinPortManager *manager,
                                                  const char *file_path,
                                                  uint8_t port_id,
                                                  size_t chunk_size) {
    if (!manager || !file_path) return NULL;
    
    /* Build device_path string with chunk_size and loop flag */
    char device_path[512];
    if (chunk_size > 0) {
        snprintf(device_path, sizeof(device_path), "%s:%zu:loop", file_path, chunk_size);
    } else {
        snprintf(device_path, sizeof(device_path), "%s:4096:loop", file_path);
    }
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_DATASET_FILE, device_path, port_id);
    if (!port) return NULL;
    
    /* Set callbacks for input port */
    port->open_func = file_input_open;
    port->close_func = file_input_close;
    port->read_func = file_input_read;
    port->write_func = NULL;  /* Input ports are read-only */
    
    return port;
}

/* Register file output port (receives from brain.m, writes to file) */
MelvinPort* melvin_port_register_file_output(MelvinPortManager *manager,
                                             const char *file_path,
                                             uint8_t port_id,
                                             bool append_mode) {
    if (!manager || !file_path) return NULL;
    
    /* Build device_path string with mode flag */
    char device_path[512];
    if (append_mode) {
        snprintf(device_path, sizeof(device_path), "%s:append", file_path);
    } else {
        snprintf(device_path, sizeof(device_path), "%s:overwrite", file_path);
    }
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_DATASET_FILE, device_path, port_id);
    if (!port) return NULL;
    
    /* Set callbacks for output port */
    port->open_func = file_output_open;
    port->close_func = file_output_close;
    port->read_func = NULL;  /* Output ports are write-only */
    port->write_func = file_output_write;
    
    return port;
}

/* ========================================================================
 * LEGACY COMPATIBILITY (map old function names to new unified functions)
 * ======================================================================== */

/* Legacy: Register dataset file port (maps to file input) */
MelvinPort* melvin_port_register_dataset_file(MelvinPortManager *manager,
                                               const char *file_path,
                                               uint8_t port_id,
                                               size_t chunk_size) {
    return melvin_port_register_file_input(manager, file_path, port_id, chunk_size);
}

/* Legacy: Register dataset file port with loop (maps to file input loop) */
MelvinPort* melvin_port_register_dataset_file_loop(MelvinPortManager *manager,
                                                    const char *file_path,
                                                    uint8_t port_id,
                                                    size_t chunk_size) {
    return melvin_port_register_file_input_loop(manager, file_path, port_id, chunk_size);
}

