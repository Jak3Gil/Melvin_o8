/*
 * Melvin Production Pipeline
 * 
 * Continuous processing with monitoring of nodes, edges, and outputs
 * Processes mic, camera inputs and routes to speakers
 */

#include "melvin_ports.h"
#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>

static volatile bool running = true;

void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\n[Shutting down...]\n");
}

void print_status(MelvinPortManager *manager, time_t start_time) {
    if (!manager || !manager->mfile) return;
    
    MelvinGraph *graph = melvin_m_get_graph(manager->mfile);
    if (!graph) return;
    
    time_t now = time(NULL);
    time_t uptime = now - start_time;
    
    /* Clear screen and print status */
    printf("\033[2J\033[H");  /* ANSI clear screen and home */
    printf("=== Melvin Production Pipeline ===\n");
    printf("Uptime: %ld seconds\n\n", uptime);
    
    /* Graph statistics */
    printf("Graph State:\n");
    printf("  Nodes: %zu\n", graph->node_count);
    printf("  Edges: %zu\n", graph->edge_count);
    printf("  Adaptations: %llu\n", (unsigned long long)melvin_m_get_adaptation_count(manager->mfile));
    printf("\n");
    
    /* Port statistics */
    printf("Port Activity:\n");
    for (size_t i = 0; i < manager->port_count; i++) {
        MelvinPort *port = manager->ports[i];
        if (!port) continue;
        
        const char *type_name = "Unknown";
        switch (port->type) {
            case MELVIN_PORT_USB_MIC: type_name = "Microphone"; break;
            case MELVIN_PORT_USB_SPEAKER: type_name = "Speaker"; break;
            case MELVIN_PORT_USB_CAMERA: type_name = "Camera"; break;
            case MELVIN_PORT_USB_CAN: type_name = "CAN Bus"; break;
            default: type_name = "Unknown"; break;
        }
        
        printf("  %s (Port %d): %s\n", type_name, port->port_id, 
               port->is_open ? "Open" : "Closed");
        if (port->is_open) {
            printf("    Frames read: %llu, Bytes: %llu\n", 
                   (unsigned long long)port->frames_read,
                   (unsigned long long)port->bytes_read);
            if (port->type == MELVIN_PORT_USB_SPEAKER) {
                printf("    Frames written: %llu, Bytes: %llu\n",
                       (unsigned long long)port->frames_written,
                       (unsigned long long)port->bytes_written);
            }
        }
    }
    printf("\n");
    
    /* Output information */
    size_t output_size = melvin_m_universal_output_size(manager->mfile);
    if (output_size > 0) {
        printf("Last Output: %zu bytes\n", output_size);
        /* Show first 32 bytes as hex */
        uint8_t output_preview[32];
        size_t preview_size = (output_size < 32) ? output_size : 32;
        melvin_m_universal_output_read(manager->mfile, output_preview, preview_size);
        printf("  Preview: ");
        for (size_t i = 0; i < preview_size; i++) {
            printf("%02x ", output_preview[i]);
        }
        printf("\n");
    } else {
        printf("Last Output: (none)\n");
    }
    
    printf("\nPress Ctrl-C to stop\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    const char *mfile_name = "../brain.m";  /* Always use brain.m in root directory */
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create or open .m file */
    MelvinMFile *mfile = melvin_m_open(mfile_name);
    if (!mfile) {
        printf("Creating new .m file: %s\n", mfile_name);
        mfile = melvin_m_create(mfile_name);
        if (!mfile) {
            fprintf(stderr, "Error: Could not create .m file: %s\n", mfile_name);
            return 1;
        }
    } else {
        printf("Opened existing .m file: %s\n", mfile_name);
    }
    
    /* Create port manager */
    MelvinPortManager *manager = melvin_port_manager_create(mfile);
    if (!manager) {
        fprintf(stderr, "Error: Could not create port manager\n");
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Register ports */
    const uint8_t MIC_PORT_ID = 1;
    const uint8_t SPEAKER_PORT_ID = 2;
    const uint8_t CAMERA_PORT_ID = 3;
    
    MelvinPort *mic = melvin_port_register_usb_mic(manager, NULL, MIC_PORT_ID);
    MelvinPort *speaker = melvin_port_register_usb_speaker(manager, NULL, SPEAKER_PORT_ID);
    MelvinPort *camera = melvin_port_register_usb_camera(manager, NULL, CAMERA_PORT_ID);
    
    if (!mic || !speaker || !camera) {
        fprintf(stderr, "Error: Could not register ports\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Open ports */
    if (!melvin_port_open(mic)) {
        fprintf(stderr, "Warning: Could not open microphone\n");
    }
    if (!melvin_port_open(speaker)) {
        fprintf(stderr, "Warning: Could not open speaker\n");
    }
    if (!melvin_port_open(camera)) {
        fprintf(stderr, "Warning: Could not open camera\n");
    }
    
    /* Set routing: mic -> speaker, camera -> speaker (optional) */
    melvin_port_set_route(manager, MIC_PORT_ID, SPEAKER_PORT_ID);
    
    printf("Starting production pipeline...\n");
    printf("Processing inputs continuously on: %s\n\n", mfile_name);
    
    time_t start_time = time(NULL);
    int iterations = 0;
    time_t last_status_time = 0;
    
    /* Main processing loop */
    while (running) {
        /* Process all ports */
        bool processed = melvin_port_manager_process_all(manager);
        if (processed) {
            iterations++;
        }
        
        /* Print status every second */
        time_t now = time(NULL);
        if (now - last_status_time >= 1) {
            print_status(manager, start_time);
            last_status_time = now;
        }
        
        /* Small delay to prevent CPU spinning */
        usleep(10000);  /* 10ms */
    }
    
    /* Final status before shutdown */
    printf("\nFinal Status:\n");
    print_status(manager, start_time);
    printf("\nTotal iterations processed: %d\n", iterations);
    
    /* Cleanup */
    melvin_port_close(mic);
    melvin_port_close(speaker);
    melvin_port_close(camera);
    melvin_port_manager_free(manager);
    melvin_m_close(mfile);
    
    printf("Pipeline stopped.\n");
    return 0;
}
