/*
 * Melvin External Port System
 * 
 * Manages external device ports (USB mics, speakers, cameras, CAN bus adapters)
 * Provides unified interface for device I/O, packages data in CAN bus format
 * 
 * Cross-platform: Mac implementation first, Linux/Jetson support added later
 */

#ifndef MELVIN_PORTS_H
#define MELVIN_PORTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "melvin_m.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Port Types */
typedef enum {
    MELVIN_PORT_USB_MIC = 1,
    MELVIN_PORT_USB_SPEAKER = 2,
    MELVIN_PORT_USB_CAMERA = 3,
    MELVIN_PORT_USB_CAN = 4,
    MELVIN_PORT_DATASET_FILE = 5,      /* File port (can be input or output) */
    MELVIN_PORT_HTTP_RANGE = 6,         /* HTTP range request port (large files) */
    MELVIN_PORT_MAX = 255
} MelvinPortType;

/* Port Frame (CAN Bus Format) - matches README design */
/* Serialized format: [port_id (1)] + [timestamp (8)] + [data_size (4)] + [data (N)] */
typedef struct {
    uint8_t port_id;        /* Port identifier */
    uint64_t timestamp;     /* Timestamp in microseconds since epoch */
    uint32_t data_size;     /* Size of actual device data */
    uint8_t data[];         /* Raw device data (flexible array) */
} PortFrame;

/* Forward declaration */
typedef struct MelvinPort MelvinPort;

/* Device-specific callbacks */
typedef size_t (*PortReadFunc)(MelvinPort *port, uint8_t *buffer, size_t size);
typedef size_t (*PortWriteFunc)(MelvinPort *port, const uint8_t *buffer, size_t size);
typedef bool (*PortOpenFunc)(MelvinPort *port);
typedef void (*PortCloseFunc)(MelvinPort *port);

/* Port Device Structure */
struct MelvinPort {
    uint8_t port_id;
    MelvinPortType type;
    void *device_handle;    /* Device-specific handle (CoreAudio, AVFoundation, etc.) */
    bool is_open;
    char device_path[256];  /* Device identifier (Mac: CoreAudio ID, Linux: /dev/ path) */
    
    /* Device-specific callbacks */
    PortReadFunc read_func;
    PortWriteFunc write_func;
    PortOpenFunc open_func;
    PortCloseFunc close_func;
    
    /* Statistics */
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t frames_read;
    uint64_t frames_written;
    
    /* Internal: Buffer for reading */
    uint8_t *read_buffer;
    size_t read_buffer_size;
    size_t read_buffer_capacity;
};

/* Port Manager */
typedef struct {
    MelvinPort **ports;
    size_t port_count;
    size_t port_capacity;
    
    /* Routing table: input_port_id -> output_port_id */
    uint8_t routing_table[256];  /* 0 = no route */
    
    /* .m file reference */
    MelvinMFile *mfile;
} MelvinPortManager;

/* ========================================
 * PORT MANAGER OPERATIONS
 * ======================================== */

/* Create port manager (associates with .m file) */
MelvinPortManager* melvin_port_manager_create(MelvinMFile *mfile);

/* Free port manager and all ports */
void melvin_port_manager_free(MelvinPortManager *manager);

/* Process all ports: read inputs, send to .m file, route outputs */
bool melvin_port_manager_process_all(MelvinPortManager *manager);

/* ========================================
 * PORT REGISTRATION
 * ======================================== */

/* Register a port (creates port structure, doesn't open device) */
MelvinPort* melvin_port_register(MelvinPortManager *manager, 
                                  MelvinPortType type, 
                                  const char *device_path, 
                                  uint8_t port_id);

/* Unregister a port (closes and removes) */
bool melvin_port_unregister(MelvinPortManager *manager, uint8_t port_id);

/* Find port by ID */
MelvinPort* melvin_port_find(MelvinPortManager *manager, uint8_t port_id);

/* ========================================
 * PORT OPERATIONS
 * ======================================== */

/* Open port device */
bool melvin_port_open(MelvinPort *port);

/* Close port device */
void melvin_port_close(MelvinPort *port);

/* Check if port is open */
bool melvin_port_is_open(MelvinPort *port);

/* ========================================
 * PORT DATA I/O
 * ======================================== */

/* Read frame from port (reads device, packages into PortFrame) */
/* Returns size of packaged frame, 0 on error */
size_t melvin_port_read_frame(MelvinPort *port, PortFrame **frame_out);

/* Write frame to port (unpackages frame, writes to device) */
/* Returns bytes written to device, 0 on error */
size_t melvin_port_write_frame(MelvinPort *port, const PortFrame *frame);

/* Process input from port: read, package, send to .m file, process */
bool melvin_port_process_input(MelvinPort *port, MelvinMFile *mfile);

/* ========================================
 * PORT FRAME UTILITIES
 * ======================================== */

/* Calculate serialized frame size */
size_t melvin_port_frame_serialized_size(const PortFrame *frame);

/* Package frame into buffer (returns bytes written) */
size_t melvin_port_frame_serialize(const PortFrame *frame, uint8_t *buffer, size_t buffer_size);

/* Unpackage frame from buffer (allocates frame, caller must free) */
PortFrame* melvin_port_frame_deserialize(const uint8_t *buffer, size_t buffer_size);

/* Free allocated frame */
void melvin_port_frame_free(PortFrame *frame);

/* Create frame from data (allocates frame) */
PortFrame* melvin_port_frame_create(uint8_t port_id, const uint8_t *data, size_t data_size);

/* ========================================
 * PORT ROUTING
 * ======================================== */

/* Set routing: input_port -> output_port */
void melvin_port_set_route(MelvinPortManager *manager, uint8_t input_port, uint8_t output_port);

/* Get routing: returns output_port for input_port, 0 if no route */
uint8_t melvin_port_get_route(MelvinPortManager *manager, uint8_t input_port);

/* Clear route */
void melvin_port_clear_route(MelvinPortManager *manager, uint8_t input_port);

/* ========================================
 * DEVICE-SPECIFIC REGISTRATION (Platform-specific)
 * ======================================== */

/* Register USB microphone port (Mac: CoreAudio, Linux: ALSA) */
MelvinPort* melvin_port_register_usb_mic(MelvinPortManager *manager, 
                                          const char *device_id, 
                                          uint8_t port_id);

/* Register USB speaker port (Mac: CoreAudio, Linux: ALSA) */
MelvinPort* melvin_port_register_usb_speaker(MelvinPortManager *manager,
                                              const char *device_id,
                                              uint8_t port_id);

/* Register USB camera port (Mac: AVFoundation, Linux: V4L2) */
MelvinPort* melvin_port_register_usb_camera(MelvinPortManager *manager,
                                             const char *device_id,
                                             uint8_t port_id);

/* Register USB-to-CAN bus adapter (cross-platform) */
MelvinPort* melvin_port_register_usb_can(MelvinPortManager *manager,
                                          const char *device_path,
                                          uint8_t port_id);

/* ========================================
 * UNIFIED FILE PORT REGISTRATION
 * ======================================== */

/* Register file input port (reads from file, sends to brain.m) */
MelvinPort* melvin_port_register_file_input(MelvinPortManager *manager,
                                            const char *file_path,
                                            uint8_t port_id,
                                            size_t chunk_size);

/* Register file input port with loop on EOF (continuous processing) */
MelvinPort* melvin_port_register_file_input_loop(MelvinPortManager *manager,
                                                  const char *file_path,
                                                  uint8_t port_id,
                                                  size_t chunk_size);

/* Register file output port (receives from brain.m, writes to file) */
MelvinPort* melvin_port_register_file_output(MelvinPortManager *manager,
                                             const char *file_path,
                                             uint8_t port_id,
                                             bool append_mode);

/* Legacy compatibility: Register dataset file port (maps to file input) */
MelvinPort* melvin_port_register_dataset_file(MelvinPortManager *manager,
                                               const char *file_path,
                                               uint8_t port_id,
                                               size_t chunk_size);

/* Legacy compatibility: Register dataset file port with loop (maps to file input loop) */
MelvinPort* melvin_port_register_dataset_file_loop(MelvinPortManager *manager,
                                                    const char *file_path,
                                                    uint8_t port_id,
                                                    size_t chunk_size);

/* ========================================
 * HTTP RANGE REQUEST PORT REGISTRATION
 * ======================================== */

/* Register HTTP range request port (downloads large files in chunks via HTTP Range requests) */
/* Downloads file in chunks without storing entire file in memory */
/* If total_size is 0, attempts to discover file size via HEAD request */
/* If loop_on_eof is true, loops back to start when end of file is reached */
MelvinPort* melvin_port_register_http_range(MelvinPortManager *manager,
                                            const char *url,
                                            uint8_t port_id,
                                            size_t chunk_size,
                                            size_t total_size,
                                            bool loop_on_eof);

/* ========================================
 * UTILITY FUNCTIONS
 * ======================================== */

/* Get current timestamp in microseconds */
uint64_t melvin_port_get_timestamp(void);

/* Get port type name (for debugging) */
const char* melvin_port_type_name(MelvinPortType type);

#ifdef __cplusplus
}
#endif

#endif /* MELVIN_PORTS_H */

