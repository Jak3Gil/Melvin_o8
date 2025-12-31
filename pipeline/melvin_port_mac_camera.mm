/*
 * Melvin Mac Camera Port Implementation (AVFoundation)
 * 
 * USB camera support for macOS using AVFoundation
 * Objective-C++ implementation for AVFoundation API
 */

#include "melvin_ports.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __APPLE__

#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CoreMedia.h>

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

/* Forward declarations */
@class MelvinCameraDelegate;

/* Mac Camera State */
typedef struct {
    AVCaptureSession *capture_session;
    AVCaptureDevice *capture_device;
    AVCaptureVideoDataOutput *video_output;
    AVCaptureDeviceInput *device_input;
    MelvinCameraDelegate *delegate;
    dispatch_queue_t video_queue;
    bool is_capturing;
    
    /* Frame buffer */
    uint8_t *frame_buffer;
    size_t frame_buffer_size;
    size_t frame_buffer_capacity;
    pthread_mutex_t frame_mutex;
    uint32_t frame_width;
    uint32_t frame_height;
} MacCameraState;

/* AVCaptureVideoDataOutputSampleBufferDelegate */
@interface MelvinCameraDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (assign) MacCameraState *state;
@end

@implementation MelvinCameraDelegate

- (void)captureOutput:(AVCaptureOutput *)output 
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer 
       fromConnection:(AVCaptureConnection *)connection {
    if (!self.state) return;
    
    MacCameraState *state = (MacCameraState*)self.state;
    
    /* Get pixel buffer */
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;
    
    /* Lock pixel buffer */
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    
    /* Get frame dimensions */
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    size_t bytes_per_row = CVPixelBufferGetBytesPerRow(imageBuffer);
    
    /* Get base address */
    void *base_address = CVPixelBufferGetBaseAddress(imageBuffer);
    if (!base_address) {
        CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
        return;
    }
    
    /* Calculate data size (RGB format: 3 bytes per pixel) */
    size_t data_size = width * height * 3;
    
    /* Ensure buffer is large enough */
    pthread_mutex_lock(&state->frame_mutex);
    
    if (data_size > state->frame_buffer_capacity) {
        /* Reallocate buffer */
        free(state->frame_buffer);
        state->frame_buffer_capacity = data_size * 2;
        state->frame_buffer = (uint8_t*)malloc(state->frame_buffer_capacity);
        if (!state->frame_buffer) {
            state->frame_buffer_capacity = 0;
            pthread_mutex_unlock(&state->frame_mutex);
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            return;
        }
    }
    
    /* Convert BGRA to RGB (if needed) or copy directly */
    /* CoreVideo typically provides BGRA format */
    uint8_t *src = (uint8_t*)base_address;
    uint8_t *dst = state->frame_buffer;
    
    /* Convert BGRA to RGB */
    for (size_t y = 0; y < height; y++) {
        uint8_t *src_row = src + (y * bytes_per_row);
        uint8_t *dst_row = dst + (y * width * 3);
        
        for (size_t x = 0; x < width; x++) {
            /* BGRA -> RGB */
            dst_row[x * 3 + 0] = src_row[x * 4 + 2];  /* R */
            dst_row[x * 3 + 1] = src_row[x * 4 + 1];  /* G */
            dst_row[x * 3 + 2] = src_row[x * 4 + 0];  /* B */
        }
    }
    
    state->frame_buffer_size = data_size;
    state->frame_width = (uint32_t)width;
    state->frame_height = (uint32_t)height;
    
    pthread_mutex_unlock(&state->frame_mutex);
    
    /* Unlock pixel buffer */
    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
}

@end

/* Open Mac camera port */
static bool mac_camera_open(MelvinPort *port) {
    if (!port) return false;
    
    MacCameraState *state = (MacCameraState*)calloc(1, sizeof(MacCameraState));
    if (!state) return false;
    
    pthread_mutex_init(&state->frame_mutex, NULL);
    
    /* Initialize frame buffer */
    state->frame_buffer_capacity = 1920 * 1080 * 3;  /* Full HD RGB */
    state->frame_buffer = (uint8_t*)malloc(state->frame_buffer_capacity);
    if (!state->frame_buffer) {
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    /* Create capture session */
    state->capture_session = [[AVCaptureSession alloc] init];
    if (!state->capture_session) {
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    /* Find default video device */
    AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!device) {
        /* Try to find any video device using modern API */
        AVCaptureDeviceDiscoverySession *discovery = [AVCaptureDeviceDiscoverySession 
            discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                              AVCaptureDeviceTypeExternal]
            mediaType:AVMediaTypeVideo
            position:AVCaptureDevicePositionUnspecified];
        NSArray *devices = discovery.devices;
        if (devices && [devices count] > 0) {
            device = devices[0];
        }
    }
    
    if (!device) {
        [state->capture_session release];
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    state->capture_device = device;  /* Not retained, owned by session */
    
    NSError *error = nil;
    
    /* Create device input */
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (!input || error) {
        [state->capture_session release];
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    state->device_input = input;
    
    /* Add input to session */
    if ([state->capture_session canAddInput:input]) {
        [state->capture_session addInput:input];
    } else {
        [state->capture_session release];
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    /* Create video output */
    state->video_output = [[AVCaptureVideoDataOutput alloc] init];
    if (!state->video_output) {
        [state->capture_session release];
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    /* Configure video output */
    NSDictionary *video_settings = @{
        (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
    };
    [state->video_output setVideoSettings:video_settings];
    
    /* Set frame rate (30 FPS) */
    state->video_output.alwaysDiscardsLateVideoFrames = YES;
    
    /* Create delegate */
    MelvinCameraDelegate *delegate = [[MelvinCameraDelegate alloc] init];
    delegate.state = state;
    state->delegate = delegate;  /* Retain delegate */
    
    /* Create serial queue for video output */
    state->video_queue = dispatch_queue_create("com.melvin.camera.video", DISPATCH_QUEUE_SERIAL);
    [state->video_output setSampleBufferDelegate:delegate queue:state->video_queue];
    
    /* Add output to session */
    if ([state->capture_session canAddOutput:state->video_output]) {
        [state->capture_session addOutput:state->video_output];
    } else {
        [state->video_output release];
        [state->capture_session release];
        free(state->frame_buffer);
        pthread_mutex_destroy(&state->frame_mutex);
        free(state);
        return false;
    }
    
    /* Set session preset (medium quality for performance) */
    if ([state->capture_session canSetSessionPreset:AVCaptureSessionPresetMedium]) {
        [state->capture_session setSessionPreset:AVCaptureSessionPresetMedium];
    }
    
    /* Start capture session */
    [state->capture_session startRunning];
    
    state->is_capturing = true;
    port->device_handle = state;
    
    return true;
}

/* Close Mac camera port */
static void mac_camera_close(MelvinPort *port) {
    if (!port || !port->device_handle) return;
    
    MacCameraState *state = (MacCameraState*)port->device_handle;
    
    if (state->capture_session) {
        if (state->is_capturing) {
            [state->capture_session stopRunning];
        }
        
        /* Release video output */
        if (state->video_output) {
            [state->video_output setSampleBufferDelegate:nil queue:NULL];
            [state->video_output release];
        }
        
        /* Release delegate */
        if (state->delegate) {
            [state->delegate release];
        }
        
        /* Release session */
        [state->capture_session release];
    }
    
    /* Release video queue */
    if (state->video_queue) {
        dispatch_release(state->video_queue);
        state->video_queue = NULL;
    }
    
    pthread_mutex_destroy(&state->frame_mutex);
    free(state->frame_buffer);
    free(state);
    port->device_handle = NULL;
}

/* Read frame from camera */
static size_t mac_camera_read(MelvinPort *port, uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    MacCameraState *state = (MacCameraState*)port->device_handle;
    size_t read_size = 0;
    
    pthread_mutex_lock(&state->frame_mutex);
    
    if (state->frame_buffer_size > 0) {
        read_size = (size < state->frame_buffer_size) ? size : state->frame_buffer_size;
        memcpy(buffer, state->frame_buffer, read_size);
        state->frame_buffer_size = 0;  /* Consume frame */
    }
    
    pthread_mutex_unlock(&state->frame_mutex);
    
    return read_size;
}

/* Register USB camera port (Mac) */
MelvinPort* melvin_port_register_usb_camera(MelvinPortManager *manager,
                                            const char *device_id,
                                            uint8_t port_id) {
    if (!manager) return NULL;
    
    /* device_id can be NULL for default camera on Mac */
    const char *dev_path = device_id ? device_id : "default";
    
    /* Use enum value directly */
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_USB_CAMERA, dev_path, port_id);
    if (!port) return NULL;
    
    port->open_func = mac_camera_open;
    port->close_func = mac_camera_close;
    port->read_func = mac_camera_read;
    port->write_func = NULL;  /* Camera is input-only */
    
    return port;
}

#else
/* Non-Mac: Stub implementation */
#include "melvin_ports.h"

MelvinPort* melvin_port_register_usb_camera(MelvinPortManager *manager,
                                            const char *device_id,
                                            uint8_t port_id) {
    (void)manager; (void)device_id; (void)port_id;
    return NULL;
}
#endif

