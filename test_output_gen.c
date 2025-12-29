#include "melvin.h"
#include <stdio.h>
#include <string.h>

int main() {
    MelvinGraph *g = graph_create();
    
    printf("=== Testing Output Generation ===\n\n");
    
    // Train on "hello" multiple times to learn the pattern
    for (int i = 0; i < 5; i++) {
        printf("Training iteration %d: ", i+1);
        uint8_t input[] = "hello";
        size_t count = 0;
        Node **nodes = wave_process_sequential_patterns(g, input, 5, &count);
        
        if (nodes && count > 0) {
            wave_form_intelligent_edges(g, nodes, count, NULL, NULL);
            wave_propagate_multi_step(g, nodes, count);
            
            // Try to generate output
            uint8_t *output = NULL;
            size_t output_size = 0;
            wave_collect_output(g, nodes, count, &output, &output_size);
            
            if (output && output_size > 0) {
                printf("Output (%zu bytes): '", output_size);
                for (size_t j = 0; j < output_size; j++) {
                    printf("%c", output[j]);
                }
                printf("'\n");
                free(output);
            } else {
                printf("No output (still learning)\n");
            }
            
            free(nodes);
        }
    }
    
    printf("\n=== Testing partial input for continuation ===\n");
    // Now give it "hel" and see if it continues with "lo"
    uint8_t partial[] = "hel";
    size_t count = 0;
    Node **nodes = wave_process_sequential_patterns(g, partial, 3, &count);
    
    if (nodes && count > 0) {
        wave_form_intelligent_edges(g, nodes, count, NULL, NULL);
        wave_propagate_multi_step(g, nodes, count);
        
        uint8_t *output = NULL;
        size_t output_size = 0;
        wave_collect_output(g, nodes, count, &output, &output_size);
        
        printf("Input: 'hel'\n");
        if (output && output_size > 0) {
            printf("Output (%zu bytes): '", output_size);
            for (size_t j = 0; j < output_size; j++) {
                printf("%c", output[j]);
            }
            printf("'\n");
            free(output);
        } else {
            printf("No output generated\n");
        }
        
        free(nodes);
    }
    
    printf("\nFinal graph: %zu nodes, %zu edges\n", g->node_count, g->edge_count);
    graph_free(g);
    return 0;
}
