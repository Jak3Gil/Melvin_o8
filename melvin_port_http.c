/*
 * Melvin HTTP Range Request Port Implementation
 * 
 * Downloads large files via HTTP Range requests without storing entire file in memory.
 * Reads data in chunks and feeds to brain.m for continuous learning.
 * 
 * Uses libcurl for HTTP operations.
 */

#include "melvin_ports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <curl/curl.h>

/* ========================================================================
 * HTTP PORT STATE STRUCTURE
 * ======================================================================== */

/* HTTP Port State */
typedef struct {
    CURL *curl_handle;
    char *url;
    size_t chunk_size;
    size_t total_size;        /* Total file size (0 = unknown, will discover) */
    size_t current_offset;    /* Current read position in file */
    bool loop_on_eof;          /* Loop back to start when EOF reached */
    bool is_eof;               /* End of file reached */
    bool size_discovered;      /* Whether we've discovered file size */
    uint8_t *read_buffer;      /* Buffer for current chunk */
    size_t read_buffer_size;   /* Actual bytes in buffer */
    size_t total_bytes_read;   /* Statistics */
    size_t total_chunks_read;  /* Statistics */
} HttpPortState;

/* ========================================================================
 * CURL CALLBACK FOR WRITING DATA
 * ======================================================================== */

/* Write callback for curl - stores data in our buffer */
static size_t http_port_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    HttpPortState *state = (HttpPortState*)userp;
    if (!state || !state->read_buffer) return 0;
    
    size_t total_size = size * nmemb;
    size_t available = state->chunk_size - state->read_buffer_size;
    
    if (total_size > available) {
        total_size = available;  /* Truncate if buffer full */
    }
    
    if (total_size > 0) {
        memcpy(state->read_buffer + state->read_buffer_size, contents, total_size);
        state->read_buffer_size += total_size;
    }
    
    return total_size;
}

/* ========================================================================
 * HTTP PORT OPERATIONS
 * ======================================================================== */

/* Initialize curl (call once at startup) */
static bool curl_initialized = false;

static void ensure_curl_initialized(void) {
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }
}

/* Discover file size via HEAD request */
static bool discover_file_size(HttpPortState *state) {
    if (!state || !state->curl_handle || !state->url) return false;
    
    CURLcode res;
    long response_code;
    double content_length = -1.0;
    
    /* Set HEAD request */
    curl_easy_setopt(state->curl_handle, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(state->curl_handle, CURLOPT_URL, state->url);
    
    /* Perform HEAD request */
    res = curl_easy_perform(state->curl_handle);
    
    if (res == CURLE_OK) {
        /* Get response code */
        curl_easy_getinfo(state->curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code == 200 || response_code == 206) {
            /* Get content length */
            res = curl_easy_getinfo(state->curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
            if (res == CURLE_OK && content_length > 0) {
                state->total_size = (size_t)content_length;
                state->size_discovered = true;
                return true;
            }
        }
    }
    
    /* Reset for GET requests */
    curl_easy_setopt(state->curl_handle, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(state->curl_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEFUNCTION, http_port_write_callback);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEDATA, state);
    return false;
}

/* Open HTTP range port */
static bool http_range_open(MelvinPort *port) {
    if (!port) return false;
    
    /* Ensure curl is initialized */
    ensure_curl_initialized();
    
    HttpPortState *state = (HttpPortState*)calloc(1, sizeof(HttpPortState));
    if (!state) return false;
    
    /* Initialize curl */
    state->curl_handle = curl_easy_init();
    if (!state->curl_handle) {
        free(state);
        return false;
    }
    
    /* Copy URL from device_path */
    state->url = strdup(port->device_path);
    if (!state->url) {
        curl_easy_cleanup(state->curl_handle);
        free(state);
        return false;
    }
    
    /* Parse device_path for chunk_size, total_size, and loop flag
     * Format: "url" or "url:chunk_size" or "url:chunk_size:total_size" or "url:chunk_size:total_size:loop"
     */
    state->chunk_size = 65536;  /* Default 64KB */
    state->total_size = 0;       /* Unknown by default */
    state->loop_on_eof = false;
    
    char *colon1 = strchr(state->url, ':');
    if (colon1 && strncmp(colon1, "://", 3) != 0) {
        /* Not part of URL scheme, might be our format */
        /* But we need to be careful - URLs can have colons */
        /* Look for pattern after URL - assume format is "url|chunk|total|loop" */
        /* For simplicity, use a different separator or parse more carefully */
    }
    
    /* Parse device_path for chunk_size, total_size, and loop flag
     * Format: "url|chunk_size|total_size|loop" or variations
     * Note: "|" is safe separator as URLs don't contain it
     */
    char *pipe = strchr(state->url, '|');
    if (pipe) {
        *pipe = '\0';  /* Terminate URL at first pipe */
        pipe++;
        
        /* Parse chunk size (required after first pipe) */
        char *pipe2 = strchr(pipe, '|');
        if (pipe2) {
            *pipe2 = '\0';
            state->chunk_size = (size_t)strtoul(pipe, NULL, 10);
            if (state->chunk_size == 0) state->chunk_size = 65536;
            
            pipe2++;
            /* Parse total size (optional after second pipe) */
            char *pipe3 = strchr(pipe2, '|');
            if (pipe3) {
                *pipe3 = '\0';
                state->total_size = (size_t)strtoul(pipe2, NULL, 10);
                pipe3++;
                /* Check for loop flag */
                if (strcmp(pipe3, "loop") == 0) {
                    state->loop_on_eof = true;
                }
            } else {
                /* No third pipe - check if pipe2 is "loop" or a number */
                if (strcmp(pipe2, "loop") == 0) {
                    state->loop_on_eof = true;
                } else {
                    state->total_size = (size_t)strtoul(pipe2, NULL, 10);
                }
            }
        } else {
            /* Only one pipe - must be chunk size */
            state->chunk_size = (size_t)strtoul(pipe, NULL, 10);
            if (state->chunk_size == 0) state->chunk_size = 65536;
        }
    }
    
    /* Allocate read buffer */
    state->read_buffer = (uint8_t*)malloc(state->chunk_size);
    if (!state->read_buffer) {
        free(state->url);
        curl_easy_cleanup(state->curl_handle);
        free(state);
        return false;
    }
    
    /* Configure curl for GET requests */
    curl_easy_setopt(state->curl_handle, CURLOPT_URL, state->url);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEFUNCTION, http_port_write_callback);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEDATA, state);
    curl_easy_setopt(state->curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(state->curl_handle, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(state->curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(state->curl_handle, CURLOPT_HTTPGET, 1L);  /* Use GET method */
    
    state->current_offset = 0;
    state->is_eof = false;
    state->size_discovered = false;
    state->read_buffer_size = 0;
    state->total_bytes_read = 0;
    state->total_chunks_read = 0;
    
    /* Discover file size if not provided */
    if (state->total_size == 0) {
        if (discover_file_size(state)) {
            /* Successfully discovered size */
        } else {
            /* Failed to discover - will read until server returns less than requested */
            state->size_discovered = false;
        }
    } else {
        state->size_discovered = true;
    }
    
    port->device_handle = state;
    return true;
}

/* Close HTTP range port */
static void http_range_close(MelvinPort *port) {
    if (!port || !port->device_handle) return;
    
    HttpPortState *state = (HttpPortState*)port->device_handle;
    
    if (state->curl_handle) {
        curl_easy_cleanup(state->curl_handle);
        state->curl_handle = NULL;
    }
    
    if (state->url) {
        free(state->url);
        state->url = NULL;
    }
    
    if (state->read_buffer) {
        free(state->read_buffer);
        state->read_buffer = NULL;
    }
    
    free(state);
    port->device_handle = NULL;
}

/* Cleanup curl on program exit (optional) - declared in header if needed */

/* Read chunk from HTTP range port */
static size_t http_range_read(MelvinPort *port, uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    HttpPortState *state = (HttpPortState*)port->device_handle;
    if (!state->curl_handle || !state->url) return 0;
    
    /* If EOF and loop_on_eof, reset to start */
    if (state->is_eof && state->loop_on_eof) {
        state->current_offset = 0;
        state->is_eof = false;
    }
    
    if (state->is_eof) return 0;  /* No more data */
    
    /* Check if we've reached the end */
    if (state->size_discovered && state->current_offset >= state->total_size) {
        state->is_eof = true;
        return 0;
    }
    
    /* Calculate range for this request */
    size_t read_size = (size < state->chunk_size) ? size : state->chunk_size;
    
    /* Adjust read_size if we know total size and would exceed it */
    if (state->size_discovered) {
        size_t remaining = state->total_size - state->current_offset;
        if (read_size > remaining) {
            read_size = remaining;
        }
    }
    
    if (read_size == 0) {
        state->is_eof = true;
        return 0;
    }
    
    /* Clear buffer */
    state->read_buffer_size = 0;
    
    /* Set up range request */
    char range_header[128];
    size_t range_end = state->current_offset + read_size - 1;
    
    /* If we know total size, don't request beyond it */
    if (state->size_discovered && range_end >= state->total_size) {
        range_end = state->total_size - 1;
        read_size = range_end - state->current_offset + 1;
        if (read_size == 0) {
            state->is_eof = true;
            return 0;
        }
    }
    
    snprintf(range_header, sizeof(range_header), "%zu-%zu", 
             state->current_offset, range_end);
    
    /* Ensure curl is configured for GET request (not HEAD) */
    curl_easy_setopt(state->curl_handle, CURLOPT_NOBODY, 0L);
    curl_easy_setopt(state->curl_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(state->curl_handle, CURLOPT_URL, state->url);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEFUNCTION, http_port_write_callback);
    curl_easy_setopt(state->curl_handle, CURLOPT_WRITEDATA, state);
    
    /* Set range header */
    struct curl_slist *headers = NULL;
    char range_header_full[256];
    snprintf(range_header_full, sizeof(range_header_full), "Range: bytes=%s", range_header);
    headers = curl_slist_append(headers, range_header_full);
    curl_easy_setopt(state->curl_handle, CURLOPT_HTTPHEADER, headers);
    
    /* Perform request */
    CURLcode res = curl_easy_perform(state->curl_handle);
    
    /* Clean up headers */
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        /* Error - mark as EOF or retry? For now, mark as EOF */
        state->is_eof = true;
        return 0;
    }
    
    /* Check response code */
    long response_code;
    curl_easy_getinfo(state->curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code != 200 && response_code != 206) {
        /* Not OK or Partial Content - mark as EOF */
        state->is_eof = true;
        return 0;
    }
    
    /* Copy data from read buffer to output buffer */
    size_t bytes_to_copy = (state->read_buffer_size < read_size) ? 
                           state->read_buffer_size : read_size;
    
    if (bytes_to_copy > 0) {
        memcpy(buffer, state->read_buffer, bytes_to_copy);
        state->current_offset += bytes_to_copy;
        state->total_bytes_read += bytes_to_copy;
        state->total_chunks_read++;
    }
    
    /* Check if we've reached the end */
    if (state->size_discovered && state->current_offset >= state->total_size) {
        state->is_eof = true;
    } else if (bytes_to_copy < read_size) {
        /* Got less than requested - probably EOF */
        state->is_eof = true;
    }
    
    return bytes_to_copy;
}

/* Write to HTTP range port (not supported - input only) */
static size_t http_range_write(MelvinPort *port, const uint8_t *buffer, size_t size) {
    (void)port;
    (void)buffer;
    (void)size;
    return 0;  /* HTTP range ports are read-only */
}

/* ========================================================================
 * PORT REGISTRATION FUNCTION
 * ======================================================================== */

/* Register HTTP range request port */
MelvinPort* melvin_port_register_http_range(MelvinPortManager *manager,
                                             const char *url,
                                             uint8_t port_id,
                                             size_t chunk_size,
                                             size_t total_size,
                                             bool loop_on_eof) {
    if (!manager || !url) return NULL;
    
    /* Build device_path string with parameters
     * Format: "url|chunk_size|total_size|loop" (if loop) or "url|chunk_size|total_size"
     */
    char device_path[1024];
    if (loop_on_eof && total_size > 0) {
        snprintf(device_path, sizeof(device_path), "%s|%zu|%zu|loop", 
                url, chunk_size, total_size);
    } else if (total_size > 0) {
        snprintf(device_path, sizeof(device_path), "%s|%zu|%zu", 
                url, chunk_size, total_size);
    } else if (loop_on_eof) {
        snprintf(device_path, sizeof(device_path), "%s|%zu|0|loop", 
                url, chunk_size);
    } else {
        snprintf(device_path, sizeof(device_path), "%s|%zu", url, chunk_size);
    }
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_HTTP_RANGE, 
                                            device_path, port_id);
    if (!port) return NULL;
    
    /* Set callbacks */
    port->open_func = http_range_open;
    port->close_func = http_range_close;
    port->read_func = http_range_read;
    port->write_func = NULL;  /* HTTP range ports are read-only */
    
    return port;
}

