/*
 * Production Dataset Port Test
 * 
 * Feeds a dataset file to brain.m through a port
 * Production-ready with proper error handling, statistics, and monitoring
 */

#include "melvin_ports.h"
#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>

static volatile bool running = true;
static time_t start_time = 0;

void signal_handler(int sig) {
    (void)sig;
    running = false;
    printf("\n[Shutting down...]\n");
}

/* Print statistics - simplified: just nodes, edges, outputs */
void print_status(MelvinPortManager *manager, size_t total_processed, size_t total_bytes) {
    (void)total_processed;  /* Unused but kept for API compatibility */
    if (!manager || !manager->mfile) return;
    
    MelvinGraph *graph = melvin_m_get_graph(manager->mfile);
    if (!graph) return;
    
    time_t now = time(NULL);
    time_t elapsed = now - start_time;
    
    printf("\033[2J\033[H");
    printf("=== Melvin Dataset Port ===\n");
    printf("Elapsed: %ld seconds\n\n", elapsed);
    
    /* Just nodes and edges */
    printf("Nodes: %zu\n", graph->node_count);
    printf("Edges: %zu\n", graph->edge_count);
    printf("Bytes processed: %zu\n\n", total_bytes);
    
    /* Output */
    size_t output_size = melvin_m_universal_output_size(manager->mfile);
    if (output_size > 0) {
        printf("Output: %zu bytes\n", output_size);
        uint8_t output[256];
        size_t read = melvin_m_universal_output_read(manager->mfile, output, 
            output_size < sizeof(output) ? output_size : sizeof(output));
        printf("  ");
        for (size_t i = 0; i < read && i < 128; i++) {
            if (output[i] >= 32 && output[i] < 127) {
                printf("%c", output[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
    } else {
        printf("Output: (none)\n");
    }
    
    printf("\nPress Ctrl-C to stop\n");
    fflush(stdout);
}

/* Check if file exists and get size */
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    const char *mfile_name = "../brain.m";  /* Always use brain.m in root directory */
    const char *dataset_file = NULL;
    size_t chunk_size = 4096;  /* Default chunk size */
    bool loop_on_eof = false;
    
    /* Parse command line arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dataset_file> [--chunk-size SIZE] [--loop]\n", argv[0]);
        fprintf(stderr, "  dataset_file: Path to dataset file to feed through port\n");
        fprintf(stderr, "  --chunk-size SIZE: Size of each read chunk in bytes (default: 4096)\n");
        fprintf(stderr, "  --loop: Loop dataset file when EOF is reached\n");
        fprintf(stderr, "\nNote: Always uses brain.m file\n");
        return 1;
    }
    
    dataset_file = argv[1];
    
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
            }
        }
    }
    
    /* Verify dataset file exists */
    long file_size = get_file_size(dataset_file);
    if (file_size < 0) {
        fprintf(stderr, "Error: Dataset file not found or cannot be accessed: %s\n", dataset_file);
        fprintf(stderr, "  Error: %s\n", strerror(errno));
        return 1;
    }
    
    printf("Dataset file: %s (%ld bytes)\n", dataset_file, file_size);
    printf("Chunk size: %zu bytes\n", chunk_size);
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
    
    /* Register file input port (reads from dataset file) */
    const uint8_t DATASET_INPUT_PORT_ID = 1;
    const uint8_t TEXT_OUTPUT_PORT_ID = 2;
    
    MelvinPort *input_port = NULL;
    if (loop_on_eof) {
        input_port = melvin_port_register_file_input_loop(manager, dataset_file, DATASET_INPUT_PORT_ID, chunk_size);
    } else {
        input_port = melvin_port_register_file_input(manager, dataset_file, DATASET_INPUT_PORT_ID, chunk_size);
    }
    
    if (!input_port) {
        fprintf(stderr, "Error: Could not register file input port\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Register file output port (writes to output file) */
    char output_file[512];
    snprintf(output_file, sizeof(output_file), "%s.output", dataset_file);
    MelvinPort *output_port = melvin_port_register_file_output(manager, output_file, TEXT_OUTPUT_PORT_ID, true);
    
    if (!output_port) {
        fprintf(stderr, "Error: Could not register file output port\n");
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    /* Set routing: input port → output port */
    melvin_port_set_route(manager, DATASET_INPUT_PORT_ID, TEXT_OUTPUT_PORT_ID);
    
    /* Open ports */
    if (!melvin_port_open(input_port)) {
        fprintf(stderr, "Error: Could not open input file: %s\n", dataset_file);
        fprintf(stderr, "  Error: %s\n", strerror(errno));
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    if (!melvin_port_open(output_port)) {
        fprintf(stderr, "Error: Could not open output file: %s\n", output_file);
        fprintf(stderr, "  Error: %s\n", strerror(errno));
        melvin_port_close(input_port);
        melvin_port_manager_free(manager);
        melvin_m_close(mfile);
        return 1;
    }
    
    printf("File input port opened: %s\n", dataset_file);
    printf("File output port opened: %s (append mode)\n", output_file);
    printf("Routing: Port %d → Port %d\n", DATASET_INPUT_PORT_ID, TEXT_OUTPUT_PORT_ID);
    printf("Starting dataset processing...\n\n");
    
    start_time = time(NULL);
    size_t total_processed = 0;
    size_t total_bytes = 0;
    time_t last_status_time = 0;
    time_t last_save_time = 0;
    
    /* Main processing loop */
    while (running) {
        /* Process dataset port */
        bool processed = melvin_port_manager_process_all(manager);
        
        if (processed) {
            total_processed++;
            /* Count bytes from the input port */
            if (input_port && input_port->is_open) {
                total_bytes = input_port->bytes_read;
            }
        }
        
        /* Check if dataset is exhausted (for non-looping mode) */
        if (!loop_on_eof && input_port && input_port->is_open) {
            /* Check if we've processed all data */
            /* In non-looping mode, process_all will return false when EOF is reached */
            if (!processed && input_port->frames_read > 0) {
                printf("\nDataset processing complete (EOF reached)\n");
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
    printf("Total bytes processed: %zu\n", total_bytes);
    
    /* Cleanup */
    melvin_port_close(input_port);
    melvin_port_close(output_port);
    melvin_port_manager_free(manager);
    melvin_m_close(mfile);
    
    printf("\nDataset processing stopped.\n");
    printf("Output written to: %s\n", output_file);
    return 0;
}

