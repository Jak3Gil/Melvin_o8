#include "melvin.h"
#include <stdio.h>
#include <string.h>

int main() {
    MelvinGraph *g = graph_create();
    
    // Process "hello world hello" - should form hierarchy for repeated "hello"
    printf("=== Processing: 'hello world hello' ===\n\n");
    
    for (int iter = 1; iter <= 3; iter++) {
        printf("--- Iteration %d ---\n", iter);
        uint8_t input[] = "hello world hello";
        size_t count = 0;
        Node **nodes = wave_process_sequential_patterns(g, input, 17, &count);
        
        if (nodes && count > 0) {
            // Form edges (triggers hierarchy formation)
            wave_form_intelligent_edges(g, nodes, count, NULL, NULL);
            free(nodes);
        }
        
        printf("Total nodes: %zu\n", g->node_count);
        printf("Total edges: %zu\n", g->edge_count);
        
        // Analyze node types
        int single_byte_nodes = 0;
        int multi_byte_nodes = 0;
        int blank_nodes = 0;
        
        for (size_t i = 0; i < g->node_count; i++) {
            Node *n = g->nodes[i];
            if (n->payload_size == 0) {
                blank_nodes++;
            } else if (n->payload_size == 1) {
                single_byte_nodes++;
            } else {
                multi_byte_nodes++;
                printf("  Multi-byte node: size=%zu, abstraction_level=%u\n", 
                       n->payload_size, n->abstraction_level);
            }
        }
        
        printf("  Single-byte: %d, Multi-byte: %d, Blank: %d\n", 
               single_byte_nodes, multi_byte_nodes, blank_nodes);
        printf("\n");
    }
    
    // Check for actual duplicates (same payload, same size)
    printf("=== Checking for duplicate payloads ===\n");
    int dup_count = 0;
    for (size_t i = 0; i < g->node_count; i++) {
        for (size_t j = i+1; j < g->node_count; j++) {
            if (g->nodes[i]->payload_size == g->nodes[j]->payload_size &&
                g->nodes[i]->payload_size > 0 &&
                memcmp(g->nodes[i]->payload, g->nodes[j]->payload, 
                       g->nodes[i]->payload_size) == 0) {
                printf("  DUPLICATE: ");
                for (size_t k = 0; k < g->nodes[i]->payload_size && k < 10; k++) {
                    printf("%c", g->nodes[i]->payload[k]);
                }
                printf(" (size=%zu)\n", g->nodes[i]->payload_size);
                dup_count++;
            }
        }
    }
    
    if (dup_count == 0) {
        printf("  No duplicates found!\n");
    } else {
        printf("  Total duplicates: %d\n", dup_count);
    }
    
    graph_free(g);
    return 0;
}
