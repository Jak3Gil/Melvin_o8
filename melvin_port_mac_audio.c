/*
 * Melvin Mac Audio Port Implementation (CoreAudio)
 * 
 * USB microphone and speaker support for macOS
 * Uses AudioQueue API for recording and playback
 */

#include "melvin_ports.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>

/* Audio configuration */
#define SAMPLE_RATE 44100
#define CHANNELS 2
#define BITS_PER_SAMPLE 16
#define BYTES_PER_SAMPLE (BITS_PER_SAMPLE / 8)
#define FRAMES_PER_BUFFER 512
#define BYTES_PER_FRAME (CHANNELS * BYTES_PER_SAMPLE)
#define BUFFER_SIZE (FRAMES_PER_BUFFER * BYTES_PER_FRAME)

/* Mac Audio Port State */
typedef struct {
    AudioQueueRef queue;
    AudioQueueBufferRef *buffers;
    size_t buffer_count;
    bool is_recording;  /* For mic */
    bool is_playing;    /* For speaker */
    
    /* Circular buffer for audio data */
    uint8_t *audio_buffer;
    size_t audio_buffer_size;
    size_t audio_buffer_capacity;
    size_t audio_buffer_read_pos;
    size_t audio_buffer_write_pos;
    pthread_mutex_t buffer_mutex;
} MacAudioState;

/* Audio callback for recording (microphone) */
static void mac_audio_input_callback(void *user_data,
                                     AudioQueueRef queue,
                                     AudioQueueBufferRef buffer,
                                     const AudioTimeStamp *start_time,
                                     UInt32 num_packets,
                                     const AudioStreamPacketDescription *packet_descs) {
    MacAudioState *state = (MacAudioState*)user_data;
    if (!state || !state->is_recording) return;
    
    /* Copy audio data to circular buffer */
    pthread_mutex_lock(&state->buffer_mutex);
    
    size_t data_size = buffer->mAudioDataByteSize;
    size_t available = state->audio_buffer_capacity - 
                       ((state->audio_buffer_write_pos - state->audio_buffer_read_pos + 
                         state->audio_buffer_capacity) % state->audio_buffer_capacity);
    
    if (data_size <= available) {
        /* Write to circular buffer */
        size_t write_pos = state->audio_buffer_write_pos;
        size_t wrap = state->audio_buffer_capacity - write_pos;
        
        if (data_size <= wrap) {
            memcpy(state->audio_buffer + write_pos, buffer->mAudioData, data_size);
        } else {
            memcpy(state->audio_buffer + write_pos, buffer->mAudioData, wrap);
            memcpy(state->audio_buffer, (uint8_t*)buffer->mAudioData + wrap, data_size - wrap);
        }
        
        state->audio_buffer_write_pos = (write_pos + data_size) % state->audio_buffer_capacity;
        state->audio_buffer_size += data_size;
    }
    
    pthread_mutex_unlock(&state->buffer_mutex);
    
    /* Re-enqueue buffer */
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

/* Audio callback for playback (speaker) */
static void mac_audio_output_callback(void *user_data,
                                      AudioQueueRef queue,
                                      AudioQueueBufferRef buffer) {
    MacAudioState *state = (MacAudioState*)user_data;
    if (!state || !state->is_playing) {
        memset(buffer->mAudioData, 0, buffer->mAudioDataByteSize);
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
        return;
    }
    
    /* Read from circular buffer */
    pthread_mutex_lock(&state->buffer_mutex);
    
    size_t to_read = buffer->mAudioDataByteSize;
    size_t available = state->audio_buffer_size;
    size_t read_size = (to_read < available) ? to_read : available;
    
    if (read_size > 0) {
        size_t read_pos = state->audio_buffer_read_pos;
        size_t wrap = state->audio_buffer_capacity - read_pos;
        
        if (read_size <= wrap) {
            memcpy(buffer->mAudioData, state->audio_buffer + read_pos, read_size);
        } else {
            memcpy(buffer->mAudioData, state->audio_buffer + read_pos, wrap);
            memcpy((uint8_t*)buffer->mAudioData + wrap, state->audio_buffer, read_size - wrap);
        }
        
        state->audio_buffer_read_pos = (read_pos + read_size) % state->audio_buffer_capacity;
        state->audio_buffer_size -= read_size;
    }
    
    /* Fill rest with silence if needed */
    if (read_size < to_read) {
        memset((uint8_t*)buffer->mAudioData + read_size, 0, to_read - read_size);
    }
    
    pthread_mutex_unlock(&state->buffer_mutex);
    
    /* Enqueue buffer */
    buffer->mAudioDataByteSize = to_read;
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

/* Open Mac audio port (microphone) */
static bool mac_audio_mic_open(MelvinPort *port) {
    if (!port || port->device_handle) return false;
    
    MacAudioState *state = (MacAudioState*)calloc(1, sizeof(MacAudioState));
    if (!state) return false;
    
    /* Initialize circular buffer */
    state->audio_buffer_capacity = BUFFER_SIZE * 4;  /* 4 buffers worth */
    state->audio_buffer = (uint8_t*)malloc(state->audio_buffer_capacity);
    if (!state->audio_buffer) {
        free(state);
        return false;
    }
    
    pthread_mutex_init(&state->buffer_mutex, NULL);
    
    /* Setup audio format */
    AudioStreamBasicDescription format;
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = BYTES_PER_FRAME;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = BYTES_PER_FRAME;
    format.mChannelsPerFrame = CHANNELS;
    format.mBitsPerChannel = BITS_PER_SAMPLE;
    format.mReserved = 0;
    
    /* Create audio queue */
    OSStatus err = AudioQueueNewInput(&format,
                                      mac_audio_input_callback,
                                      state,
                                      NULL,
                                      kCFRunLoopCommonModes,
                                      0,
                                      &state->queue);
    
    if (err != noErr) {
        free(state->audio_buffer);
        free(state);
        return false;
    }
    
    /* Allocate buffers */
    state->buffer_count = 3;
    state->buffers = (AudioQueueBufferRef*)calloc(state->buffer_count, sizeof(AudioQueueBufferRef));
    
    for (size_t i = 0; i < state->buffer_count; i++) {
        err = AudioQueueAllocateBuffer(state->queue, BUFFER_SIZE, &state->buffers[i]);
        if (err == noErr) {
            AudioQueueEnqueueBuffer(state->queue, state->buffers[i], 0, NULL);
        }
    }
    
    /* Start queue */
    err = AudioQueueStart(state->queue, NULL);
    if (err != noErr) {
        for (size_t i = 0; i < state->buffer_count; i++) {
            if (state->buffers[i]) {
                AudioQueueFreeBuffer(state->queue, state->buffers[i]);
            }
        }
        free(state->buffers);
        AudioQueueDispose(state->queue, true);
        free(state->audio_buffer);
        pthread_mutex_destroy(&state->buffer_mutex);
        free(state);
        return false;
    }
    
    state->is_recording = true;
    port->device_handle = state;
    
    return true;
}

/* Open Mac audio port (speaker) */
static bool mac_audio_speaker_open(MelvinPort *port) {
    if (!port || port->device_handle) return false;
    
    MacAudioState *state = (MacAudioState*)calloc(1, sizeof(MacAudioState));
    if (!state) return false;
    
    /* Initialize circular buffer */
    state->audio_buffer_capacity = BUFFER_SIZE * 4;
    state->audio_buffer = (uint8_t*)malloc(state->audio_buffer_capacity);
    if (!state->audio_buffer) {
        free(state);
        return false;
    }
    
    pthread_mutex_init(&state->buffer_mutex, NULL);
    
    /* Setup audio format */
    AudioStreamBasicDescription format;
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = BYTES_PER_FRAME;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = BYTES_PER_FRAME;
    format.mChannelsPerFrame = CHANNELS;
    format.mBitsPerChannel = BITS_PER_SAMPLE;
    format.mReserved = 0;
    
    /* Create audio queue */
    OSStatus err = AudioQueueNewOutput(&format,
                                       mac_audio_output_callback,
                                       state,
                                       NULL,
                                       kCFRunLoopCommonModes,
                                       0,
                                       &state->queue);
    
    if (err != noErr) {
        free(state->audio_buffer);
        pthread_mutex_destroy(&state->buffer_mutex);
        free(state);
        return false;
    }
    
    /* Allocate buffers */
    state->buffer_count = 3;
    state->buffers = (AudioQueueBufferRef*)calloc(state->buffer_count, sizeof(AudioQueueBufferRef));
    
    for (size_t i = 0; i < state->buffer_count; i++) {
        err = AudioQueueAllocateBuffer(state->queue, BUFFER_SIZE, &state->buffers[i]);
        if (err == noErr) {
            memset(state->buffers[i]->mAudioData, 0, BUFFER_SIZE);
            state->buffers[i]->mAudioDataByteSize = BUFFER_SIZE;
            AudioQueueEnqueueBuffer(state->queue, state->buffers[i], 0, NULL);
        }
    }
    
    /* Start queue */
    err = AudioQueueStart(state->queue, NULL);
    if (err != noErr) {
        for (size_t i = 0; i < state->buffer_count; i++) {
            if (state->buffers[i]) {
                AudioQueueFreeBuffer(state->queue, state->buffers[i]);
            }
        }
        free(state->buffers);
        AudioQueueDispose(state->queue, true);
        free(state->audio_buffer);
        pthread_mutex_destroy(&state->buffer_mutex);
        free(state);
        return false;
    }
    
    state->is_playing = true;
    port->device_handle = state;
    
    return true;
}

/* Close Mac audio port */
static void mac_audio_close(MelvinPort *port) {
    if (!port || !port->device_handle) return;
    
    MacAudioState *state = (MacAudioState*)port->device_handle;
    
    if (state->queue) {
        if (state->is_recording || state->is_playing) {
            AudioQueueStop(state->queue, true);
        }
        
        for (size_t i = 0; i < state->buffer_count; i++) {
            if (state->buffers[i]) {
                AudioQueueFreeBuffer(state->queue, state->buffers[i]);
            }
        }
        free(state->buffers);
        
        AudioQueueDispose(state->queue, true);
    }
    
    pthread_mutex_destroy(&state->buffer_mutex);
    free(state->audio_buffer);
    free(state);
    port->device_handle = NULL;
}

/* Read from microphone */
static size_t mac_audio_mic_read(MelvinPort *port, uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    MacAudioState *state = (MacAudioState*)port->device_handle;
    size_t read_size = 0;
    
    pthread_mutex_lock(&state->buffer_mutex);
    
    size_t available = state->audio_buffer_size;
    read_size = (size < available) ? size : available;
    
    if (read_size > 0) {
        size_t read_pos = state->audio_buffer_read_pos;
        size_t wrap = state->audio_buffer_capacity - read_pos;
        
        if (read_size <= wrap) {
            memcpy(buffer, state->audio_buffer + read_pos, read_size);
        } else {
            memcpy(buffer, state->audio_buffer + read_pos, wrap);
            memcpy(buffer + wrap, state->audio_buffer, read_size - wrap);
        }
        
        state->audio_buffer_read_pos = (read_pos + read_size) % state->audio_buffer_capacity;
        state->audio_buffer_size -= read_size;
    }
    
    pthread_mutex_unlock(&state->buffer_mutex);
    
    return read_size;
}

/* Write to speaker */
static size_t mac_audio_speaker_write(MelvinPort *port, const uint8_t *buffer, size_t size) {
    if (!port || !port->device_handle || !buffer || size == 0) return 0;
    
    MacAudioState *state = (MacAudioState*)port->device_handle;
    size_t written = 0;
    
    pthread_mutex_lock(&state->buffer_mutex);
    
    size_t available = state->audio_buffer_capacity - 
                       ((state->audio_buffer_write_pos - state->audio_buffer_read_pos + 
                         state->audio_buffer_capacity) % state->audio_buffer_capacity);
    
    written = (size < available) ? size : available;
    
    if (written > 0) {
        size_t write_pos = state->audio_buffer_write_pos;
        size_t wrap = state->audio_buffer_capacity - write_pos;
        
        if (written <= wrap) {
            memcpy(state->audio_buffer + write_pos, buffer, written);
        } else {
            memcpy(state->audio_buffer + write_pos, buffer, wrap);
            memcpy(state->audio_buffer, buffer + wrap, written - wrap);
        }
        
        state->audio_buffer_write_pos = (write_pos + written) % state->audio_buffer_capacity;
        state->audio_buffer_size += written;
    }
    
    pthread_mutex_unlock(&state->buffer_mutex);
    
    return written;
}

/* Register USB microphone port (Mac) */
MelvinPort* melvin_port_register_usb_mic(MelvinPortManager *manager, 
                                          const char *device_id, 
                                          uint8_t port_id) {
    if (!manager) return NULL;
    
    /* device_id can be NULL for default device on Mac */
    const char *dev_path = device_id ? device_id : "default";
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_USB_MIC, dev_path, port_id);
    if (!port) return NULL;
    
    port->open_func = mac_audio_mic_open;
    port->close_func = mac_audio_close;
    port->read_func = mac_audio_mic_read;
    port->write_func = NULL;  /* Mic is input-only */
    
    return port;
}

/* Register USB speaker port (Mac) */
MelvinPort* melvin_port_register_usb_speaker(MelvinPortManager *manager,
                                              const char *device_id,
                                              uint8_t port_id) {
    if (!manager) return NULL;
    
    /* device_id can be NULL for default device on Mac */
    const char *dev_path = device_id ? device_id : "default";
    
    MelvinPort *port = melvin_port_register(manager, MELVIN_PORT_USB_SPEAKER, dev_path, port_id);
    if (!port) return NULL;
    
    port->open_func = mac_audio_speaker_open;
    port->close_func = mac_audio_close;
    port->read_func = NULL;  /* Speaker is output-only */
    port->write_func = mac_audio_speaker_write;
    
    return port;
}

#else
/* Non-Mac: Stub implementations */
MelvinPort* melvin_port_register_usb_mic(MelvinPortManager *manager, 
                                          const char *device_id, 
                                          uint8_t port_id) {
    (void)manager; (void)device_id; (void)port_id;
    return NULL;
}

MelvinPort* melvin_port_register_usb_speaker(MelvinPortManager *manager,
                                              const char *device_id,
                                              uint8_t port_id) {
    (void)manager; (void)device_id; (void)port_id;
    return NULL;
}
#endif

