/*
 * Debug: Check output readiness values
 */

#include "melvin_m.h"
#include "melvin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    const char *brain_file = "test_readiness_debug.m";
    int iterations = 10;
    
    if (argc >= 2) {
        iterations = atoi(argv[1]);
    }
    
    printf("========================================\n");
    printf("Output Readiness Debug Test\n");
    printf("========================================\n\n");
    
    unlink(brain_file);
    
    MelvinGraph *graph = graph_create();
    if (!graph) {
        fprintf(stderr, "ERROR: Failed to create graph\n");
        return 1;
    }
    
    const char *input_text = "Hello Melvin!";
    size_t input_len = strlen(input_text);
    size_t input_size = input_len + 1;
    uint8_t *input_data = (uint8_t*)malloc(input_size);
    input_data[0] = 1;
    memcpy(input_data + 1, input_text, input_len);
    
    MelvinMFile *mfile = melvin_m_create(brain_file);
    if (!mfile) {
        fprintf(stderr, "ERROR: Failed to create .m file\n");
        free(input_data);
        graph_free(graph);
        return 1;
    }
    
    for (int i = 0; i < iterations; i++) {
        melvin_m_universal_input_write(mfile, input_data, input_size);
        bool success = melvin_m_process_input(mfile);
        
        if (!success) {
            fprintf(stderr, "ERROR: Failed to process input\n");
            break;
        }
        
        /* After processing, check readiness on nodes that should have edges */
        /* Find nodes that match the input pattern (these are the ones that should have edges) */
        size_t output_size = melvin_m_universal_output_size(mfile);
        
        /* Get a sample of nodes from the graph to check */
        Node **sample_nodes = NULL;
        size_t sample_count = 0;
        size_t sample_capacity = 1;  /* Minimal context */
        
        /* Sample up to 20 nodes from the graph */
        size_t max_sample = (mfile->graph->node_count < 20) ? mfile->graph->node_count : 20;
        for (size_t j = 0; j < max_sample && j < mfile->graph->node_count; j++) {
            Node *node = mfile->graph->nodes[j];
            if (!node) continue;
            
            if (sample_count >= sample_capacity) {
                sample_capacity = (sample_capacity == 0) ? 1 : sample_capacity * 2;
                sample_nodes = (Node**)realloc(sample_nodes, sample_capacity * sizeof(Node*));
            }
            sample_nodes[sample_count++] = node;
        }
        
        if (sample_nodes && sample_count > 0) {
            float readiness = compute_output_readiness(mfile->graph, sample_nodes, sample_count);
            
            printf("Iteration %2d: ", i + 1);
            printf("Nodes: %3zu, Edges: %3zu, ", 
                   mfile->graph->node_count, mfile->graph->edge_count);
            printf("Readiness: %.6f", readiness);
            printf(", Output: %zu bytes", output_size);
            
            if (readiness > 0.0f) {
                printf(" ✓ (should output)");
            } else {
                printf(" ✗ (no output)");
            }
            printf("\n");
            
            /* Show edge details for first few iterations */
            if (i < 3 && sample_count > 0) {
                Node *first_node = sample_nodes[0];
                if (first_node) {
                    printf("  Sample node: %zu outgoing, %zu incoming, ", 
                           first_node->outgoing_count, first_node->incoming_count);
                    float local_avg = node_get_local_outgoing_weight_avg(first_node);
                    printf("local_avg: %.4f\n", local_avg);
                }
            }
            
            free(sample_nodes);
        } else {
            printf("Iteration %2d: No nodes to check\n", i + 1);
        }
    }
    
    melvin_m_close(mfile);
    free(input_data);
    graph_free(graph);
    
    return 0;
}

