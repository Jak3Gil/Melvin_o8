/*
 * Production HTTP Range Request Port Test
 * 
 * Downloads large files via HTTP Range requests and feeds to brain.m
 * Production-ready with proper error handling, statistics, and monitoring
 * 
 * Usage: test_http_range <url> [brain.m] [--chunk-size SIZE] [--total-size SIZE] [--loop]
 */

#include "melvin_ports.h"
#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

static volatile bool running = true;
static time_t start_time = 0;

void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\n[Shutting down...]\n");
}

/* Print statistics */
void print_status(MelvinPortManager *manager, size_t total_processed, size_t total_bytes) {
    if (!manager || !manager->mfile) return;
    
    MelvinGraph *graph = melvin_m_get_graph(manager->mfile);
    if (!graph) return;
    
    time_t now = time(NULL);
    time_t elapsed = now - start_time;
    
    /* Clear screen and print status */
    printf("\033[2J\033[H");  /* ANSI clear screen and home */
    printf("=== Melvin HTTP Range Request Port Test ===\n");
    printf("Elapsed time: %ld seconds\n\n", elapsed);
    
    /* Graph statistics */
    printf("Brain State:\n");
    printf("  Nodes: %zu\n", graph->node_count);
    printf("  Edges: %zu\n", graph->edge_count);
    printf("  Adaptations: %llu\n", (unsigned long long)melvin_m_get_adaptation_count(manager->mfile));
    printf("\n");
    
    /* Port statistics */
    printf("Port Activity:\n");
    for (size_t i = 0; i < manager->port_count; i++) {
        MelvinPort *port = manager->ports[i];
        if (!port) continue;
        
        printf("  Port %d (%s): %s\n", port->port_id,
               melvin_port_type_name(port->type),
               port->is_open ? "Open" : "Closed");
        if (port->is_open) {
            if (port->read_func) {
                printf("    Input: Frames: %llu, Bytes: %llu\n", 
                       (unsigned long long)port->frames_read,
                       (unsigned long long)port->bytes_read);
            }
            if (port->write_func) {
                printf("    Output: Frames: %llu, Bytes: %llu\n",
                       (unsigned long long)port->frames_written,
                       (unsigned long long)port->bytes_written);
            }
        }
    }
    printf("\n");
    
    /* Overall statistics */
    printf("Overall Statistics:\n");
    printf("  Total frames processed: %zu\n", total_processed);
    printf("  Total bytes downloaded: %zu\n", total_bytes);
    if (elapsed > 0) {
        printf("  Processing rate: %.2f frames/sec, %.2f KB/sec\n",
               (double)total_processed / elapsed,
               (double)total_bytes / elapsed / 1024.0);
    }
    printf("\n");
    
    /* Output information */
    size_t output_size = melvin_m_universal_output_size(manager->mfile);
    if (output_size > 0) {
        printf("Last Output: %zu bytes\n", output_size);
        /* Show first 64 bytes as hex */
        uint8_t output_preview[64];
        size_t preview_size = (output_size < 64) ? output_size : 64;
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
    const char *url = NULL;
    size_t chunk_size = 65536;  /* Default 64KB chunks */
    size_t total_size = 0;       /* 0 = auto-discover */
    bool loop_on_eof = false;
    
    /* Parse command line arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <url> [--chunk-size SIZE] [--total-size SIZE] [--loop]\n", argv[0]);
        fprintf(stderr, "  url: HTTP/HTTPS URL to download via range requests\n");
        fprintf(stderr, "  --chunk-size SIZE: Size of each range request in bytes (default: 65536)\n");
        fprintf(stderr, "  --total-size SIZE: Total file size (0 = auto-discover via HEAD, default: 0)\n");
        fprintf(stderr, "  --loop: Loop back to start when EOF is reached\n");
        fprintf(stderr, "\nNote: Always uses brain.m file\n");
        fprintf(stderr, "\nExample:\n");
        fprintf(stderr, "  %s https://example.com/large-file.bin\n", argv[0]);
        fprintf(stderr, "  %s https://example.com/large-file.bin --chunk-size 131072 --loop\n", argv[0]);
        return 1;
    }
    
    url = argv[1];
    
    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--loop") == 0) {
                loop_on_eof = true;
            } else if (strcmp(argv[i], "--chunk-size") == 0 && i + 1 < argc) {
                chunk_size = (size_t)strtoul(argv[i + 1], NULL, 10);
                if (chunk_size == 0) {
                    fprintf(stderr, "Error: Invalid chunk size\n");
                    return 1;
                }
                i++;  /* Skip next argument */
            } else if (strcmp(argv[i], "--total-size") == 0 && i + 1 < argc) {
                total_size = (size_t)strtoull(argv[i + 1], NULL, 10);
                i++;  /* Skip next argument */
            }
        }
    }
    
    printf("HTTP URL: %s\n", url);
    printf("Chunk size: %zu bytes (%.2f KB)\n", chunk_size, chunk_size / 1024.0);
    if (total_size > 0) {
        printf("Total size: %zu bytes (%.2f MB)\n", total_size, total_size / (1024.0 * 1024.0));
    } else {
        printf("Total size: Auto-discover via HEAD request\n");
    }
    printf("Loop on EOF: %s\n", loop_on_eof ? "Yes" : "No");
    printf("\n");
    
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
    
    /* Register HTTP range request port */
    const uint8_t HTTP_INPUT_PORT_ID = 1;
    const uint8_t FILE_OUTPUT_PORT_ID = 2;
    
    MelvinPort *http_port = melvin_port_register_http_range(
        manager, 
        url, 
        HTTP_INPUT_PORT_ID, 
        chunk_size, 
        total_size, 
        loop_on_eof
    );
    
    if (!http_port) {
        fprintf(stderr, "Error: Could not register HTTP range request port\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Register file output port (optional - writes brain.m output to file) */
    char output_file[512];
    snprintf(output_file, sizeof(output_file), "http_range_output.bin");
    MelvinPort *output_port = melvin_port_register_file_output(
        manager, 
        output_file, 
        FILE_OUTPUT_PORT_ID, 
        true  /* append mode */
    );
    
    if (!output_port) {
        fprintf(stderr, "Error: Could not register file output port\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Set routing: HTTP input port → file output port */
    melvin_port_set_route(manager, HTTP_INPUT_PORT_ID, FILE_OUTPUT_PORT_ID);
    
    /* Open ports */
    printf("Opening HTTP range request port...\n");
    if (!melvin_port_open(http_port)) {
        fprintf(stderr, "Error: Could not open HTTP range request port\n");
        fprintf(stderr, "  Check URL and network connectivity\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    if (!melvin_port_open(output_port)) {
        fprintf(stderr, "Error: Could not open output file: %s\n", output_file);
        fprintf(stderr, "  Error: %s\n", strerror(errno));
        melvin_port_close(http_port);
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    printf("HTTP range request port opened: %s\n", url);
    printf("File output port opened: %s (append mode)\n", output_file);
    printf("Routing: Port %d → Port %d\n", HTTP_INPUT_PORT_ID, FILE_OUTPUT_PORT_ID);
    printf("Starting HTTP range request processing...\n\n");
    
    start_time = time(NULL);
    size_t total_processed = 0;
    size_t total_bytes = 0;
    time_t last_status_time = 0;
    time_t last_save_time = 0;
    int no_data_count = 0;
    
    /* Main processing loop */
    while (running) {
        /* Process HTTP range port */
        bool processed = melvin_port_manager_process_all(manager);
        
        if (processed) {
            total_processed++;
            no_data_count = 0;
            /* Count bytes from the HTTP port */
            if (http_port && http_port->is_open) {
                total_bytes = http_port->bytes_read;
            }
        } else {
            no_data_count++;
            /* If no data for many iterations, might be EOF (for non-looping mode) */
            if (!loop_on_eof && no_data_count > 100 && http_port && http_port->frames_read > 0) {
                printf("\nHTTP download complete (EOF reached)\n");
                break;
            }
        }
        
        /* Print status every second */
        time_t now = time(NULL);
        if (now - last_status_time >= 1) {
            print_status(manager, total_processed, total_bytes);
            last_status_time = now;
        }
        
        /* Auto-save brain.m every 30 seconds */
        if (now - last_save_time >= 30) {
            if (melvin_m_is_dirty(mfile)) {
                printf("\n[Auto-saving brain.m...]\n");
                if (melvin_m_save(mfile)) {
                    printf("[Saved successfully]\n");
                } else {
                    fprintf(stderr, "[Warning: Save failed]\n");
                }
                last_save_time = now;
            }
        }
        
        /* Small delay to prevent CPU spinning */
        if (!processed) {
            usleep(10000);  /* 10ms when no data */
        }
    }
    
    /* Final save */
    printf("\n[Final save...]\n");
    if (melvin_m_is_dirty(mfile)) {
        if (melvin_m_save(mfile)) {
            printf("[Saved successfully]\n");
        } else {
            fprintf(stderr, "[Warning: Save failed]\n");
        }
    } else {
        printf("[No changes to save]\n");
    }
    
    /* Final status */
    printf("\nFinal Status:\n");
    print_status(manager, total_processed, total_bytes);
    printf("\nTotal frames processed: %zu\n", total_processed);
    printf("Total bytes downloaded: %zu\n", total_bytes);
    
    /* Cleanup */
    melvin_port_close(http_port);
    melvin_port_close(output_port);
    melvin_port_manager_free(manager);
    melvin_m_close(mfile);
    
    printf("\nHTTP range request processing stopped.\n");
    printf("Output written to: %s\n", output_file);
    return 0;
}

