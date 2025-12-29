/*
 * Test: Repetition Learning - Does the system reuse nodes and generate outputs?
 */

#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <float.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *brain_file = "test_repetition.m";
    int repetitions = 20;
    
    if (argc >= 2) {
        repetitions = atoi(argv[1]);
        if (repetitions < 1) repetitions = 20;
    }
    
    printf("========================================\n");
    printf("Repetition Learning Test\n");
    printf("========================================\n\n");
    printf("Processing same input %d times\n", repetitions);
    printf("Brain file: %s\n\n", brain_file);
    
    unlink(brain_file);
    
    MelvinMFile *mfile = melvin_m_create(brain_file);
    if (!mfile) {
        fprintf(stderr, "ERROR: Failed to create .m file\n");
        return 1;
    }
    
    const char *input_text = "Hello Melvin!";
    size_t input_len = strlen(input_text);
    size_t input_size = input_len + 1;
    uint8_t *input_data = (uint8_t*)malloc(input_size);
    input_data[0] = 1;
    memcpy(input_data + 1, input_text, input_len);
    
    size_t prev_nodes = 0;
    size_t prev_edges = 0;
    float prev_avg_weight = 0.0f;
    
    for (int i = 0; i < repetitions; i++) {
        melvin_m_universal_input_write(mfile, input_data, input_size);
        
        clock_t start = clock();
        bool success = melvin_m_process_input(mfile);
        clock_t end = clock();
        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
        
        if (!success) {
            fprintf(stderr, "ERROR: Failed to process input\n");
            break;
        }
        
        MelvinGraph *graph = mfile->graph;
        size_t nodes = graph->node_count;
        size_t edges = graph->edge_count;
        
        /* Calculate average edge weight */
        float total_weight = 0.0f;
        size_t weighted_edges = 0;
        for (size_t j = 0; j < edges; j++) {
            Edge *e = graph->edges[j];
            if (e && e->weight > 0.0f) {
                total_weight += e->weight;
                weighted_edges++;
            }
        }
        float avg_weight = (weighted_edges > 0) ? total_weight / weighted_edges : 0.0f;
        
        size_t output_size = melvin_m_universal_output_size(mfile);
        
        printf("Iteration %2d: ", i + 1);
        printf("Nodes: %3zu", nodes);
        if (i > 0) {
            size_t node_diff = nodes - prev_nodes;
            if (node_diff == 0) {
                printf(" (REUSED)");
            } else {
                printf(" (+%zu)", node_diff);
            }
        }
        
        printf(" | Edges: %3zu", edges);
        if (i > 0) {
            size_t edge_diff = edges - prev_edges;
            if (edge_diff == 0) {
                printf(" (STABLE)");
            } else {
                printf(" (+%zu)", edge_diff);
            }
        }
        
        printf(" | Avg Weight: %.4f", avg_weight);
        if (i > 0 && avg_weight > prev_avg_weight) {
            printf(" (↑ STRENGTHENING)");
        }
        
        printf(" | Output: %zu bytes", output_size);
        if (output_size > 0) {
            printf(" ✓");
        }
        printf(" | Time: %.2f ms\n", elapsed);
        
        prev_nodes = nodes;
        prev_edges = edges;
        prev_avg_weight = avg_weight;
    }
    
    printf("\n");
    printf("Final: %zu nodes, %zu edges, %.2f edges/node\n", 
           mfile->graph->node_count, 
           mfile->graph->edge_count,
           (float)mfile->graph->edge_count / (float)mfile->graph->node_count);
    
    melvin_m_save(mfile);
    melvin_m_close(mfile);
    free(input_data);
    
    return 0;
}

