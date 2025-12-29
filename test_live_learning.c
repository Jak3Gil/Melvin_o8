/*
 * Live Learning from Large Database
 * 
 * Pulls data from HTTP URL continuously and shows:
 * - Live terminal stats (graph state, processing rate, etc.)
 * - Real-time .m file outputs
 * 
 * Usage: test_live_learning <url> [brain.m] [--chunk-size SIZE] [--loop]
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

/* Print live statistics and output */
void print_live_status(MelvinPortManager *manager, size_t total_processed, size_t total_bytes) {
    if (!manager || !manager->mfile) return;
    
    MelvinGraph *graph = melvin_m_get_graph(manager->mfile);
    if (!graph) return;
    
    time_t now = time(NULL);
    time_t elapsed = now - start_time;
    
    /* Clear screen and print status */
    printf("\033[2J\033[H");  /* ANSI clear screen and home */
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║         MELVIN LIVE LEARNING - DATABASE PULL                 ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Graph statistics */
    printf("📊 BRAIN STATE:\n");
    printf("   Nodes: %zu\n", graph->node_count);
    printf("   Edges: %zu\n", graph->edge_count);
    printf("   Adaptations: %llu\n", (unsigned long long)melvin_m_get_adaptation_count(manager->mfile));
    
    /* Count nodes by abstraction level */
    size_t level_counts[10] = {0};
    for (size_t i = 0; i < graph->node_count; i++) {
        Node *node = graph->nodes[i];
        if (!node) continue;
        uint32_t level = node->abstraction_level;
        if (level < 10) {
            level_counts[level]++;
        }
    }
    
    printf("   Hierarchy: ");
    bool first = true;
    for (int i = 0; i < 10; i++) {
        if (level_counts[i] > 0) {
            if (!first) printf(", ");
            printf("L%d=%zu", i, level_counts[i]);
            first = false;
        }
    }
    printf("\n\n");
    
    /* Port statistics */
    printf("🔌 PORT ACTIVITY:\n");
    for (size_t i = 0; i < manager->port_count; i++) {
        MelvinPort *port = manager->ports[i];
        if (!port) continue;
        
        printf("   Port %d (%s): %s\n", port->port_id,
               melvin_port_type_name(port->type),
               port->is_open ? "✓ Open" : "✗ Closed");
        if (port->is_open && port->read_func) {
            printf("      Downloaded: %llu bytes (%llu chunks)\n", 
                   (unsigned long long)port->bytes_read,
                   (unsigned long long)port->frames_read);
        }
        if (port->is_open && port->write_func) {
            printf("      Written: %llu bytes (%llu chunks)\n",
                   (unsigned long long)port->bytes_written,
                   (unsigned long long)port->frames_written);
        }
    }
    printf("\n");
    
    /* Processing statistics */
    printf("⚡ PROCESSING STATS:\n");
    printf("   Elapsed: %ld seconds\n", elapsed);
    printf("   Frames processed: %zu\n", total_processed);
    printf("   Bytes downloaded: %zu (%.2f MB)\n", total_bytes, total_bytes / (1024.0 * 1024.0));
    if (elapsed > 0) {
        double fps = (double)total_processed / elapsed;
        double mbps = (double)total_bytes / elapsed / (1024.0 * 1024.0);
        printf("   Rate: %.2f frames/sec, %.2f MB/sec\n", fps, mbps);
    }
    printf("\n");
    
    /* OUTPUT FROM .M FILE */
    size_t output_size = melvin_m_universal_output_size(manager->mfile);
    printf("💭 BRAIN OUTPUT:\n");
    if (output_size > 0) {
        uint8_t *output_data = (uint8_t*)malloc(output_size);
        if (output_data) {
            size_t read_size = melvin_m_universal_output_read(manager->mfile, output_data, output_size);
            
            printf("   Size: %zu bytes\n", read_size);
            printf("   Hex: ");
            size_t hex_preview = (read_size < 32) ? read_size : 32;
            for (size_t i = 0; i < hex_preview; i++) {
                printf("%02x ", output_data[i]);
            }
            if (read_size > 32) printf("...");
            printf("\n");
            
            printf("   Text: \"");
            size_t text_preview = (read_size < 64) ? read_size : 64;
            for (size_t i = 0; i < text_preview; i++) {
                if (output_data[i] >= 32 && output_data[i] < 127) {
                    printf("%c", output_data[i]);
                } else if (output_data[i] == '\n') {
                    printf("\\n");
                } else if (output_data[i] == '\r') {
                    printf("\\r");
                } else if (output_data[i] == '\t') {
                    printf("\\t");
                } else {
                    printf(".");
                }
            }
            if (read_size > 64) printf("...");
            printf("\"\n");
            
            free(output_data);
        }
    } else {
        printf("   (thinking mode - no output yet)\n");
    }
    printf("\n");
    
    printf("Press Ctrl-C to stop\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    const char *mfile_name = "brain.m";
    const char *url = NULL;
    size_t chunk_size = 65536;  /* Default 64KB chunks */
    size_t total_size = 0;       /* 0 = auto-discover */
    bool loop_on_eof = false;
    
    /* Parse command line arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <url> [brain.m] [--chunk-size SIZE] [--loop]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s https://example.com/dataset.bin\n", argv[0]);
        fprintf(stderr, "  %s https://example.com/dataset.bin /Volumes/512GB/brain.m --loop\n", argv[0]);
        fprintf(stderr, "  %s https://example.com/dataset.bin brain.m --chunk-size 131072 --loop\n", argv[0]);
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
            } else if (argv[i][0] != '-' && strcmp(mfile_name, "brain.m") == 0) {
                /* Assume positional argument is brain.m filename */
                mfile_name = argv[i];
            }
        }
    }
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("MELVIN LIVE LEARNING - DATABASE PULL\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    printf("URL: %s\n", url);
    printf("Brain file: %s\n", mfile_name);
    printf("Chunk size: %zu bytes (%.2f KB)\n", chunk_size, chunk_size / 1024.0);
    if (total_size > 0) {
        printf("Total size: %zu bytes (%.2f MB)\n", total_size, total_size / (1024.0 * 1024.0));
    } else {
        printf("Total size: Auto-discover via HEAD request\n");
    }
    printf("Loop on EOF: %s\n", loop_on_eof ? "Yes (continuous)" : "No");
    printf("\n");
    printf("Starting in 2 seconds...\n");
    sleep(2);
    
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
            fprintf(stderr, "  Error: %s\n", strerror(errno));
            return 1;
        }
        printf("✓ Created new brain file\n");
    } else {
        printf("✓ Opened existing brain file\n");
        MelvinGraph *graph = melvin_m_get_graph(mfile);
        if (graph) {
            printf("  Loaded: %zu nodes, %zu edges\n", graph->node_count, graph->edge_count);
        }
    }
    printf("\n");
    
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
    
    /* Register file output port (optional - for saving outputs) */
    char output_file[512];
    snprintf(output_file, sizeof(output_file), "%s.output", mfile_name);
    MelvinPort *output_port = melvin_port_register_file_output(
        manager, 
        output_file, 
        FILE_OUTPUT_PORT_ID, 
        true  /* append mode */
    );
    
    if (!output_port) {
        fprintf(stderr, "Warning: Could not register file output port\n");
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
    printf("✓ HTTP port opened\n");
    
    if (output_port && !melvin_port_open(output_port)) {
        fprintf(stderr, "Warning: Could not open output file: %s\n", output_file);
    } else if (output_port) {
        printf("✓ Output file: %s (append mode)\n", output_file);
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("LIVE LEARNING STARTED\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
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
                printf("\n✓ Download complete (EOF reached)\n");
                break;
            }
        }
        
        /* Print live status every second */
        time_t now = time(NULL);
        if (now - last_status_time >= 1) {
            print_live_status(manager, total_processed, total_bytes);
            last_status_time = now;
        }
        
        /* Auto-save brain.m every 30 seconds */
        if (now - last_save_time >= 30) {
            if (melvin_m_is_dirty(mfile)) {
                if (melvin_m_save(mfile)) {
                    printf("\n[Auto-saved brain.m]\n");
                } else {
                    fprintf(stderr, "\n[Warning: Auto-save failed]\n");
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
            printf("[✓ Saved successfully]\n");
        } else {
            fprintf(stderr, "[✗ Save failed]\n");
        }
    } else {
        printf("[No changes to save]\n");
    }
    
    /* Final status */
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("FINAL STATISTICS\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    print_live_status(manager, total_processed, total_bytes);
    
    printf("\nTotal frames processed: %zu\n", total_processed);
    printf("Total bytes downloaded: %zu (%.2f MB)\n", total_bytes, total_bytes / (1024.0 * 1024.0));
    if (output_port) {
        printf("Output written to: %s\n", output_file);
    }
    printf("Brain file: %s\n", mfile_name);
    
    /* Cleanup */
    melvin_port_close(http_port);
    if (output_port) melvin_port_close(output_port);
    melvin_port_manager_free(manager);
    melvin_m_close(mfile);
    
    printf("\n✓ Learning session complete!\n");
    return 0;
}

