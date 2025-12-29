# Melvin External Port System

## Overview

The Melvin External Port System provides unified I/O for external devices (USB mics, speakers, cameras, CAN bus adapters). All ports feed into the same `.m` file's unified graph, maintaining the system's universal data processing philosophy.

**Current Status**: Mac implementation complete. Linux/Jetson support can be added by implementing platform-specific drivers.

## Architecture

### Port Frame Format (CAN Bus Style)

All device data is packaged in a **PortFrame** structure:

```c
typedef struct {
    uint8_t port_id;        /* Port identifier (1-255) */
    uint64_t timestamp;     /* Microseconds since epoch */
    uint32_t data_size;     /* Size of device data */
    uint8_t data[];         /* Raw device data */
} PortFrame;
```

**Serialized format**: `[port_id (1 byte)] + [timestamp (8 bytes)] + [data_size (4 bytes)] + [data (N bytes)]`

### Port Processing Flow

1. **Device reads data** (audio samples, video frame, CAN frame, etc.)
2. **Package into PortFrame** with port_id and timestamp
3. **Serialize frame** to buffer
4. **Write to .m file universal input**
5. **Process through graph** (wave propagation, learning, etc.)
6. **Read output** from universal output
7. **Route to output port** based on routing table
8. **Write to device** (speaker, CAN bus, etc.)

### Port Routing

Port routing maps input ports to output ports:
- Input port 1 (mic) → Output port 2 (speaker)
- Input port 3 (camera) → Output port 2 (speaker)
- Input port 4 (CAN bus) → Output port 4 (CAN bus)

Routing is configured at runtime, not hardcoded.

## Current Implementations

### Mac (Complete)

- **USB Microphone**: CoreAudio AudioQueue for recording
- **USB Speaker**: CoreAudio AudioQueue for playback
- **USB Camera**: AVFoundation framework (placeholder structure)
- **USB-to-CAN**: Placeholder structure (ready for implementation)

### Linux/Jetson (Ready for Implementation)

- **USB Microphone**: ALSA/PulseAudio (not yet implemented)
- **USB Speaker**: ALSA/PulseAudio (not yet implemented)
- **USB Camera**: V4L2 (Video4Linux2) (not yet implemented)
- **USB-to-CAN**: SocketCAN (not yet implemented)

## Usage Example

```c
#include "melvin_ports.h"
#include "melvin_m.h"

int main() {
    /* Open .m file */
    MelvinMFile *mfile = melvin_m_open("brain.m");
    
    /* Create port manager */
    MelvinPortManager *manager = melvin_port_manager_create(mfile);
    
    /* Register USB microphone */
    MelvinPort *mic = melvin_port_register_usb_mic(manager, NULL, 1);
    melvin_port_open(mic);
    
    /* Register USB speaker */
    MelvinPort *speaker = melvin_port_register_usb_speaker(manager, NULL, 2);
    melvin_port_open(speaker);
    
    /* Set routing: mic -> speaker */
    melvin_port_set_route(manager, 1, 2);
    
    /* Main loop */
    while (running) {
        melvin_port_manager_process_all(manager);
        usleep(10000);  /* 10ms */
    }
    
    /* Cleanup */
    melvin_port_close(mic);
    melvin_port_close(speaker);
    melvin_port_manager_free(manager);
    melvin_m_close(mfile);
}
```

## Building

The port system is included in the main Makefile:

```bash
make                    # Builds with port system
make example_ports      # Builds example program
```

**Mac**: Automatically links AudioToolbox and CoreAudio frameworks

**Linux/Jetson**: Will link ALSA, V4L2, etc. when implemented

## Adding Linux/Jetson Support

To add Linux support, create platform-specific files:

1. **`melvin_port_linux_audio.c`**: ALSA/PulseAudio implementation
   - Replace CoreAudio calls with ALSA API
   - Same callback pattern (read/write functions)

2. **`melvin_port_linux_camera.c`**: V4L2 implementation
   - Use `open()`, `ioctl()` for V4L2
   - Capture video frames

3. **`melvin_port_linux_can.c`**: SocketCAN implementation
   - Use `socket(PF_CAN, SOCK_RAW, CAN_RAW)`
   - Read/write CAN frames

The port manager is cross-platform - only device-specific code needs to change.

## Port Types

- `MELVIN_PORT_USB_MIC = 1`: USB microphone (input)
- `MELVIN_PORT_USB_SPEAKER = 2`: USB speaker (output)
- `MELVIN_PORT_USB_CAMERA = 3`: USB camera (input)
- `MELVIN_PORT_USB_CAN = 4`: USB-to-CAN bus adapter (input/output)

Port IDs can be any value 1-255, assigned when registering.

## Unified Graph Philosophy

**Key Point**: All ports feed into the **same unified graph**. The `.m` file doesn't distinguish between data from a microphone, camera, or CAN bus - it's all just patterns to learn.

- Microphone audio → learns audio patterns
- Camera frames → learns visual patterns
- CAN bus data → learns CAN message patterns
- All patterns can connect and generalize across modalities

The port system is just a transport layer - the intelligence is in the unified graph.

## Future Enhancements

- [ ] Full AVFoundation camera implementation (requires Objective-C bridge)
- [ ] Linux audio implementation (ALSA)
- [ ] Linux camera implementation (V4L2)
- [ ] Linux CAN bus implementation (SocketCAN)
- [ ] Port statistics and monitoring
- [ ] Dynamic port discovery
- [ ] Port hot-plugging support

