/*
 * Melvin External Port System - Core Implementation
 * 
 * Cross-platform port manager, frame handling, and routing
 * Device-specific implementations in separate files (melvin_port_mac_*.c, etc.)
 */

#include "melvin_ports.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

/* ========================================
 * PORT FRAME UTILITIES
 * ======================================== */

size_t melvin_port_frame_serialized_size(const PortFrame *frame) {
    if (!frame) return 0;
    return sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) + frame->data_size;
}

size_t melvin_port_frame_serialize(const PortFrame *frame, uint8_t *buffer, size_t buffer_size) {
    if (!frame || !buffer) return 0;
    
    size_t needed = melvin_port_frame_serialized_size(frame);
    if (buffer_size < needed) return 0;
    
    size_t offset = 0;
    
    /* Serialize: port_id (1) + timestamp (8) + data_size (4) + data (N) */
    buffer[offset++] = frame->port_id;
    
    memcpy(buffer + offset, &frame->timestamp, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    memcpy(buffer + offset, &frame->data_size, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (frame->data_size > 0 && frame->data) {
        memcpy(buffer + offset, frame->data, frame->data_size);
        offset += frame->data_size;
    }
    
    return offset;
}

PortFrame* melvin_port_frame_deserialize(const uint8_t *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < (sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t))) {
        return NULL;
    }
    
    size_t offset = 0;
    
    /* Deserialize frame */
    uint8_t port_id = buffer[offset++];
    
    uint64_t timestamp;
    memcpy(&timestamp, buffer + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    uint32_t data_size;
    memcpy(&data_size, buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (buffer_size < offset + data_size) {
        return NULL;  /* Buffer too small */
    }
    
    /* Allocate frame */
    PortFrame *frame = (PortFrame*)malloc(sizeof(PortFrame) + data_size);
    if (!frame) return NULL;
    
    frame->port_id = port_id;
    frame->timestamp = timestamp;
    frame->data_size = data_size;
    
    if (data_size > 0) {
        memcpy(frame->data, buffer + offset, data_size);
    }
    
    return frame;
}

void melvin_port_frame_free(PortFrame *frame) {
    if (frame) {
        free(frame);
    }
}

PortFrame* melvin_port_frame_create(uint8_t port_id, const uint8_t *data, size_t data_size) {
    PortFrame *frame = (PortFrame*)malloc(sizeof(PortFrame) + data_size);
    if (!frame) return NULL;
    
    frame->port_id = port_id;
    frame->timestamp = melvin_port_get_timestamp();
    frame->data_size = data_size;
    
    if (data_size > 0 && data) {
        memcpy(frame->data, data, data_size);
    }
    
    return frame;
}

uint64_t melvin_port_get_timestamp(void) {
#ifdef __APPLE__
    /* Mac: Use mach_absolute_time and convert to microseconds */
    static mach_timebase_info_data_t timebase = {0, 0};
    if (timebase.denom == 0) {
        mach_timebase_info(&timebase);
    }
    uint64_t abs_time = mach_absolute_time();
    uint64_t nano = (abs_time * timebase.numer) / timebase.denom;
    return nano / 1000;  /* Convert to microseconds */
#else
    /* Linux: Use clock_gettime */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
#endif
}

const char* melvin_port_type_name(MelvinPortType type) {
    switch (type) {
        case MELVIN_PORT_USB_MIC: return "USB_MIC";
        case MELVIN_PORT_USB_SPEAKER: return "USB_SPEAKER";
        case MELVIN_PORT_USB_CAMERA: return "USB_CAMERA";
        case MELVIN_PORT_USB_CAN: return "USB_CAN";
        case MELVIN_PORT_DATASET_FILE: return "DATASET_FILE";
        case MELVIN_PORT_HTTP_RANGE: return "HTTP_RANGE";
        default: return "UNKNOWN";
    }
}

/* ========================================
 * PORT MANAGER OPERATIONS
 * ======================================== */

MelvinPortManager* melvin_port_manager_create(MelvinMFile *mfile) {
    if (!mfile) return NULL;
    
    MelvinPortManager *manager = (MelvinPortManager*)calloc(1, sizeof(MelvinPortManager));
    if (!manager) return NULL;
    
    manager->mfile = mfile;
    manager->port_capacity = 16;
    manager->ports = (MelvinPort**)calloc(manager->port_capacity, sizeof(MelvinPort*));
    if (!manager->ports) {
        free(manager);
        return NULL;
    }
    
    /* Initialize routing table (0 = no route) */
    memset(manager->routing_table, 0, sizeof(manager->routing_table));
    
    return manager;
}

void melvin_port_manager_free(MelvinPortManager *manager) {
    if (!manager) return;
    
    /* Close and free all ports */
    for (size_t i = 0; i < manager->port_count; i++) {
        if (manager->ports[i]) {
            melvin_port_close(manager->ports[i]);
            free(manager->ports[i]->read_buffer);
            free(manager->ports[i]);
        }
    }
    
    free(manager->ports);
    free(manager);
}

bool melvin_port_manager_process_all(MelvinPortManager *manager) {
    if (!manager || !manager->mfile) return false;
    
    bool any_processed = false;
    
    /* Process all input ports (mic, camera, CAN bus) */
    for (size_t i = 0; i < manager->port_count; i++) {
        MelvinPort *port = manager->ports[i];
        if (!port || !port->is_open) continue;
        
        /* Only process input ports (skip output-only ports) */
        /* Output-only ports have write_func but no read_func */
        if (port->write_func && !port->read_func) continue;
        
        /* Read frame from port */
        PortFrame *frame = NULL;
        size_t frame_size = melvin_port_read_frame(port, &frame);
        
        if (frame && frame_size > 0) {
            /* Serialize frame for .m file */
            size_t serialized_size = melvin_port_frame_serialized_size(frame);
            if (serialized_size > 0) {
                /* Write to universal input */
                uint8_t *serialized = (uint8_t*)malloc(serialized_size);
                if (serialized) {
                    size_t written = melvin_port_frame_serialize(frame, serialized, serialized_size);
                    if (written > 0) {
                        melvin_m_universal_input_write(manager->mfile, serialized, written);
                        
                        /* Process through .m file */
                        if (melvin_m_process_input(manager->mfile)) {
                            any_processed = true;
                            
                            /* Route output to appropriate port */
                            uint8_t input_port_id = melvin_m_get_last_input_port_id(manager->mfile);
                            uint8_t output_port_id = melvin_port_get_route(manager, input_port_id);
                            
                            if (output_port_id != 0) {
                                MelvinPort *output_port = melvin_port_find(manager, output_port_id);
                                if (output_port && output_port->is_open) {
                                    /* Read output from .m file */
                                    size_t output_size = melvin_m_universal_output_size(manager->mfile);
                                    if (output_size > 0) {
                                        uint8_t *output_data = (uint8_t*)malloc(output_size);
                                        if (output_data) {
                                            size_t read = melvin_m_universal_output_read(manager->mfile, 
                                                                                         output_data, 
                                                                                         output_size);
                                            if (read > 0) {
                                                /* Create output frame with output port ID */
                                                /* Note: .m file output is raw data, not a PortFrame */
                                                /* We wrap it in a PortFrame for the output port */
                                                PortFrame *output_frame = melvin_port_frame_create(output_port_id,
                                                                                                    output_data,
                                                                                                    read);
                                                if (output_frame) {
                                                    /* Write to output port */
                                                    melvin_port_write_frame(output_port, output_frame);
                                                    melvin_port_frame_free(output_frame);
                                                }
                                            }
                                            free(output_data);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    free(serialized);
                }
            }
            
            melvin_port_frame_free(frame);
            port->frames_read++;
        }
    }
    
    return any_processed;
}

/* ========================================
 * PORT REGISTRATION
 * ======================================== */

MelvinPort* melvin_port_register(MelvinPortManager *manager, 
                                  MelvinPortType type, 
                                  const char *device_path, 
                                  uint8_t port_id) {
    if (!manager || !device_path || port_id == 0 || port_id > MELVIN_PORT_MAX) {
        return NULL;
    }
    
    /* Check if port_id already exists */
    if (melvin_port_find(manager, port_id) != NULL) {
        return NULL;  /* Port ID already in use */
    }
    
    /* Resize ports array if needed */
    if (manager->port_count >= manager->port_capacity) {
        size_t new_capacity = manager->port_capacity * 2;
        MelvinPort **new_ports = (MelvinPort**)realloc(manager->ports, 
                                                        new_capacity * sizeof(MelvinPort*));
        if (!new_ports) return NULL;
        manager->ports = new_ports;
        manager->port_capacity = new_capacity;
    }
    
    /* Create port */
    MelvinPort *port = (MelvinPort*)calloc(1, sizeof(MelvinPort));
    if (!port) return NULL;
    
    port->port_id = port_id;
    port->type = type;
    port->is_open = false;
    strncpy(port->device_path, device_path, sizeof(port->device_path) - 1);
    port->device_path[sizeof(port->device_path) - 1] = '\0';
    
    /* Initialize read buffer */
    port->read_buffer_capacity = 4096;
    port->read_buffer = (uint8_t*)malloc(port->read_buffer_capacity);
    if (!port->read_buffer) {
        free(port);
        return NULL;
    }
    
    /* Add to manager */
    manager->ports[manager->port_count++] = port;
    
    return port;
}

bool melvin_port_unregister(MelvinPortManager *manager, uint8_t port_id) {
    if (!manager) return false;
    
    /* Find port */
    for (size_t i = 0; i < manager->port_count; i++) {
        if (manager->ports[i] && manager->ports[i]->port_id == port_id) {
            MelvinPort *port = manager->ports[i];
            
            /* Close if open */
            melvin_port_close(port);
            
            /* Free resources */
            free(port->read_buffer);
            free(port);
            
            /* Remove from array (shift remaining) */
            for (size_t j = i; j < manager->port_count - 1; j++) {
                manager->ports[j] = manager->ports[j + 1];
            }
            manager->port_count--;
            
            /* Clear route */
            melvin_port_clear_route(manager, port_id);
            
            return true;
        }
    }
    
    return false;
}

MelvinPort* melvin_port_find(MelvinPortManager *manager, uint8_t port_id) {
    if (!manager) return NULL;
    
    for (size_t i = 0; i < manager->port_count; i++) {
        if (manager->ports[i] && manager->ports[i]->port_id == port_id) {
            return manager->ports[i];
        }
    }
    
    return NULL;
}

/* ========================================
 * PORT OPERATIONS
 * ======================================== */

bool melvin_port_open(MelvinPort *port) {
    if (!port || port->is_open) return false;
    
    if (port->open_func) {
        if (port->open_func(port)) {
            port->is_open = true;
            return true;
        }
    }
    
    return false;
}

void melvin_port_close(MelvinPort *port) {
    if (!port || !port->is_open) return;
    
    if (port->close_func) {
        port->close_func(port);
    }
    
    port->is_open = false;
}

bool melvin_port_is_open(MelvinPort *port) {
    return port && port->is_open;
}

/* ========================================
 * PORT DATA I/O
 * ======================================== */

size_t melvin_port_read_frame(MelvinPort *port, PortFrame **frame_out) {
    if (!port || !port->is_open || !port->read_func || !frame_out) {
        *frame_out = NULL;
        return 0;
    }
    
    *frame_out = NULL;
    
    /* Read from device */
    size_t bytes_read = port->read_func(port, port->read_buffer, port->read_buffer_capacity);
    if (bytes_read == 0) return 0;
    
    /* Create frame */
    PortFrame *frame = melvin_port_frame_create(port->port_id, 
                                                 port->read_buffer, 
                                                 bytes_read);
    if (!frame) return 0;
    
    port->bytes_read += bytes_read;
    *frame_out = frame;
    
    return melvin_port_frame_serialized_size(frame);
}

size_t melvin_port_write_frame(MelvinPort *port, const PortFrame *frame) {
    if (!port || !port->is_open || !port->write_func || !frame) {
        return 0;
    }
    
    if (frame->data_size == 0) return 0;
    
    /* Write to device */
    size_t bytes_written = port->write_func(port, frame->data, frame->data_size);
    
    if (bytes_written > 0) {
        port->bytes_written += bytes_written;
        port->frames_written++;
    }
    
    return bytes_written;
}

bool melvin_port_process_input(MelvinPort *port, MelvinMFile *mfile) {
    if (!port || !mfile) return false;
    
    PortFrame *frame = NULL;
    size_t frame_size = melvin_port_read_frame(port, &frame);
    
    if (!frame || frame_size == 0) return false;
    
    /* Serialize and write to .m file */
    uint8_t *serialized = (uint8_t*)malloc(frame_size);
    if (!serialized) {
        melvin_port_frame_free(frame);
        return false;
    }
    
    size_t written = melvin_port_frame_serialize(frame, serialized, frame_size);
    melvin_port_frame_free(frame);
    
    if (written == 0) {
        free(serialized);
        return false;
    }
    
    melvin_m_universal_input_write(mfile, serialized, written);
    free(serialized);
    
    return melvin_m_process_input(mfile);
}

/* ========================================
 * PORT ROUTING
 * ======================================== */

void melvin_port_set_route(MelvinPortManager *manager, uint8_t input_port, uint8_t output_port) {
    if (!manager || input_port == 0 || input_port > MELVIN_PORT_MAX) return;
    manager->routing_table[input_port] = output_port;
}

uint8_t melvin_port_get_route(MelvinPortManager *manager, uint8_t input_port) {
    if (!manager || input_port == 0 || input_port > MELVIN_PORT_MAX) return 0;
    return manager->routing_table[input_port];
}

void melvin_port_clear_route(MelvinPortManager *manager, uint8_t input_port) {
    if (!manager || input_port == 0 || input_port > MELVIN_PORT_MAX) return;
    manager->routing_table[input_port] = 0;
}

