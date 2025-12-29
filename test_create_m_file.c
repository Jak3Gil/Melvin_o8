/*
 * Test: Create a new .m file with updated melvin.c code
 * 
 * This test creates a fresh .m file and processes some input to verify
 * the new relative threshold logic and output readiness system work correctly.
 * 
 * Usage:
 *   test_create_m_file <output_file.m> [input_text]
 */

#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

void print_hex(const char *label, const uint8_t *data, size_t size) {
    printf("%s (%zu bytes): ", label, size);
    for (size_t i = 0; i < size && i < 64; i++) {
        printf("%02X ", data[i]);
    }
    if (size > 64) printf("...");
    printf("\n");
}

void print_ascii(const char *label, const uint8_t *data, size_t size) {
    printf("%s (%zu bytes): \"", label, size);
    for (size_t i = 0; i < size && i < 64; i++) {
        if (data[i] >= 32 && data[i] < 127) {
            printf("%c", data[i]);
        } else {
            printf(".");
        }
    }
    if (size > 64) printf("...");
    printf("\"\n");
}

void print_graph_stats(MelvinGraph *graph) {
    if (!graph) {
        printf("Graph: NULL\n");
        return;
    }
    
    printf("Graph Statistics:\n");
    printf("  Nodes: %zu\n", graph->node_count);
    printf("  Edges: %zu\n", graph->edge_count);
    
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
    
    printf("  Nodes by abstraction level:\n");
    for (int i = 0; i < 10; i++) {
        if (level_counts[i] > 0) {
            printf("    Level %d: %zu\n", i, level_counts[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *output_file = "test_new.m";
    const char *input_text = "Hello Melvin! This is a test of the new relative threshold system.";
    
    if (argc >= 2) {
        output_file = argv[1];
    }
    if (argc >= 3) {
        input_text = argv[2];
    }
    
    printf("========================================\n");
    printf("Melvin .m File Creation Test\n");
    printf("Testing with updated melvin.c code\n");
    printf("========================================\n\n");
    
    /* Remove old file if it exists */
    printf("Creating new .m file: %s\n", output_file);
    unlink(output_file);
    
    /* Create new .m file */
    MelvinMFile *mfile = melvin_m_create(output_file);
    if (!mfile) {
        fprintf(stderr, "ERROR: Failed to create .m file\n");
        return 1;
    }
    
    printf("✓ Created new .m file\n");
    printf("  Initial state:\n");
    printf("    Nodes: %llu\n", (unsigned long long)mfile->header.node_count);
    printf("    Edges: %llu\n", (unsigned long long)mfile->header.edge_count);
    printf("    Adaptations: %llu\n", (unsigned long long)mfile->header.adaptation_count);
    printf("\n");
    
    /* Prepare input data (with port ID in CAN bus format) */
    size_t input_text_len = strlen(input_text);
    size_t input_size = input_text_len + 1;  /* +1 for port ID */
    uint8_t *input_data = (uint8_t*)malloc(input_size);
    if (!input_data) {
        fprintf(stderr, "ERROR: Failed to allocate input buffer\n");
        melvin_m_close(mfile);
        return 1;
    }
    
    input_data[0] = 1;  /* Port ID = 1 (test port) */
    memcpy(input_data + 1, input_text, input_text_len);
    
    printf("--- Test 1: First Input (Novel Pattern) ---\n");
    printf("Input text: \"%s\"\n", input_text);
    print_hex("Input (with port ID)", input_data, input_size);
    printf("\n");
    
    /* Write input to .m file */
    melvin_m_universal_input_write(mfile, input_data, input_size);
    
    /* Process input through graph */
    printf("Processing input...\n");
    bool success = melvin_m_process_input(mfile);
    if (!success) {
        fprintf(stderr, "ERROR: Failed to process input\n");
        free(input_data);
        melvin_m_close(mfile);
        return 1;
    }
    
    printf("✓ Processing complete\n");
    print_graph_stats(mfile->graph);
    
    /* Check output */
    size_t output_size = melvin_m_universal_output_size(mfile);
    printf("\nOutput readiness check:\n");
    printf("  Output size: %zu bytes\n", output_size);
    
    if (output_size > 0) {
        uint8_t *output_data = (uint8_t*)malloc(output_size);
        if (output_data) {
            size_t read_size = melvin_m_universal_output_read(mfile, output_data, output_size);
            printf("  Read %zu bytes\n", read_size);
            print_hex("Output (hex)", output_data, read_size);
            print_ascii("Output (text)", output_data, read_size);
            
            /* Check if output is echo or continuation */
            if (read_size == input_size - 1) {  /* -1 because port ID not in output */
                size_t match = 0;
                for (size_t i = 0; i < read_size && i < input_text_len; i++) {
                    if (output_data[i] == input_text[i]) match++;
                }
                float similarity = (read_size > 0) ? (float)match / (float)read_size : 0.0f;
                if (similarity > 0.95f) {
                    printf("  ⚠️  WARNING: Output appears to be echo (%.1f%% match)\n", similarity * 100.0f);
                } else {
                    printf("  ✓ Output differs from input (%.1f%% match) - likely continuation\n", similarity * 100.0f);
                }
            } else if (read_size > input_text_len) {
                printf("  ✓ Output extended beyond input - learned continuation!\n");
            } else {
                printf("  ✓ Output generated (different size)\n");
            }
            
            free(output_data);
        }
    } else {
        printf("  ✓ No output generated (thinking mode - patterns not mature enough)\n");
        printf("  This is expected for novel input with new code\n");
    }
    
    printf("\n");
    
    /* Test 2: Process same input again (should have learned patterns) */
    printf("--- Test 2: Second Input (Familiar Pattern) ---\n");
    printf("Processing same input again...\n");
    
    melvin_m_universal_input_write(mfile, input_data, input_size);
    success = melvin_m_process_input(mfile);
    if (!success) {
        fprintf(stderr, "ERROR: Failed to process input second time\n");
        free(input_data);
        melvin_m_close(mfile);
        return 1;
    }
    
    printf("✓ Processing complete\n");
    print_graph_stats(mfile->graph);
    
    output_size = melvin_m_universal_output_size(mfile);
    printf("\nOutput readiness check:\n");
    printf("  Output size: %zu bytes\n", output_size);
    
    if (output_size > 0) {
        uint8_t *output_data = (uint8_t*)malloc(output_size);
        if (output_data) {
            size_t read_size = melvin_m_universal_output_read(mfile, output_data, output_size);
            printf("  Read %zu bytes\n", read_size);
            print_hex("Output (hex)", output_data, read_size);
            print_ascii("Output (text)", output_data, read_size);
            printf("  ✓ Output generated (patterns should be more mature now)\n");
            free(output_data);
        }
    } else {
        printf("  No output (patterns still learning)\n");
    }
    
    printf("\n");
    
    /* Test 3: Process different input (test generalization) */
    const char *new_input_text = "Hello World!";
    size_t new_input_text_len = strlen(new_input_text);
    size_t new_input_size = new_input_text_len + 1;
    uint8_t *new_input_data = (uint8_t*)malloc(new_input_size);
    if (new_input_data) {
        new_input_data[0] = 1;  /* Port ID = 1 */
        memcpy(new_input_data + 1, new_input_text, new_input_text_len);
        
        printf("--- Test 3: Different Input (Generalization Test) ---\n");
        printf("Input text: \"%s\"\n", new_input_text);
        print_hex("Input (with port ID)", new_input_data, new_input_size);
        printf("\n");
        
        melvin_m_universal_input_write(mfile, new_input_data, new_input_size);
        success = melvin_m_process_input(mfile);
        if (success) {
            printf("✓ Processing complete\n");
            print_graph_stats(mfile->graph);
            
            output_size = melvin_m_universal_output_size(mfile);
            printf("\nOutput readiness check:\n");
            printf("  Output size: %zu bytes\n", output_size);
            
            if (output_size > 0) {
                uint8_t *output_data = (uint8_t*)malloc(output_size);
                if (output_data) {
                    size_t read_size = melvin_m_universal_output_read(mfile, output_data, output_size);
                    print_hex("Output (hex)", output_data, read_size);
                    print_ascii("Output (text)", output_data, read_size);
                    free(output_data);
                }
            }
        }
        
        free(new_input_data);
    }
    
    printf("\n");
    
    /* Save .m file */
    printf("--- Saving .m File ---\n");
    if (melvin_m_save(mfile)) {
        printf("✓ Saved %s\n", output_file);
        printf("  Final state:\n");
        printf("    Nodes: %llu\n", (unsigned long long)mfile->header.node_count);
        printf("    Edges: %llu\n", (unsigned long long)mfile->header.edge_count);
        printf("    Adaptations: %llu\n", (unsigned long long)melvin_m_get_adaptation_count(mfile));
        
        /* Get file size */
        FILE *f = fopen(output_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long file_size = ftell(f);
            fclose(f);
            if (file_size > 0) {
                printf("    File size: %ld bytes\n", file_size);
            }
        }
    } else {
        fprintf(stderr, "ERROR: Failed to save .m file\n");
    }
    
    /* Cleanup */
    free(input_data);
    melvin_m_close(mfile);
    
    printf("\n========================================\n");
    printf("Test complete!\n");
    printf("Created .m file: %s\n", output_file);
    printf("========================================\n");
    
    return 0;
}

