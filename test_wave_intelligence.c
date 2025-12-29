#include "melvin.h"
#include <stdio.h>
#include <string.h>

int main() {
    // Create graph and test "hello" pattern
    MelvinGraph *g = graph_create();
    
    // Process "hello" 3 times to learn pattern
    for (int i = 0; i < 3; i++) {
        printf("\n=== Processing 'hello' iteration %d ===\n", i+1);
        
        uint8_t input[] = "hello";
        size_t count = 0;
        Node **nodes = wave_process_sequential_patterns(g, input, 5, &count);
        
        if (nodes && count > 0) {
            printf("Nodes created/found: %zu\n", count);
            printf("Total graph nodes: %zu\n", g->node_count);
            printf("Total graph edges: %zu\n", g->edge_count);
            
            // Form edges
            wave_form_intelligent_edges(g, nodes, count, NULL, NULL);
            printf("After edge formation: %zu edges\n", g->edge_count);
            
            // Try to generate output
            uint8_t *output = NULL;
            size_t output_size = 0;
            wave_collect_output(g, nodes, count, &output, &output_size);
            
            if (output && output_size > 0) {
                printf("Generated output (%zu bytes): ", output_size);
                for (size_t j = 0; j < output_size; j++) {
                    printf("%c", output[j]);
                }
                printf("\n");
                free(output);
            } else {
                printf("No output generated (still learning)\n");
            }
            
            free(nodes);
        }
    }
    
    printf("\n=== Final Graph State ===\n");
    printf("Total nodes: %zu\n", g->node_count);
    printf("Total edges: %zu\n", g->edge_count);
    
    // Check for duplicate nodes
    printf("\nChecking for duplicate payloads...\n");
    for (size_t i = 0; i < g->node_count; i++) {
        for (size_t j = i+1; j < g->node_count; j++) {
            if (g->nodes[i]->payload_size == g->nodes[j]->payload_size &&
                g->nodes[i]->payload_size == 1 &&
                memcmp(g->nodes[i]->payload, g->nodes[j]->payload, 1) == 0) {
                printf("  Duplicate '%c' nodes found (IDs: %s, %s)\n", 
                       g->nodes[i]->payload[0],
                       g->nodes[i]->id,
                       g->nodes[j]->id);
            }
        }
    }
    
    graph_free(g);
    return 0;
}
