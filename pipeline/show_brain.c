/*
 * Show Brain Stats
 * 
 * Simple utility to show nodes, edges, and outputs from brain.m
 */

#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    const char *brain_path = "../brain.m";  /* Always use brain.m in root directory */
    
    MelvinMFile *mfile = melvin_m_open(brain_path);
    if (!mfile) {
        fprintf(stderr, "Failed to open: %s\n", brain_path);
        return 1;
    }
    
    MelvinGraph *graph = melvin_m_get_graph(mfile);
    if (!graph) {
        melvin_m_close(mfile);
        return 1;
    }
    
    printf("Nodes: %zu\n", graph->node_count);
    printf("Edges: %zu\n", graph->edge_count);
    
    size_t output_size = melvin_m_universal_output_size(mfile);
    if (output_size > 0) {
        printf("Output: %zu bytes\n", output_size);
        uint8_t *output = malloc(output_size);
        if (output) {
            melvin_m_universal_output_read(mfile, output, output_size);
            printf("  ");
            for (size_t i = 0; i < output_size && i < 256; i++) {
                if (output[i] >= 32 && output[i] < 127) {
                    printf("%c", output[i]);
                } else {
                    printf(".");
                }
            }
            printf("\n");
            free(output);
        }
    } else {
        printf("Output: (none)\n");
    }
    
    melvin_m_close(mfile);
    return 0;
}

