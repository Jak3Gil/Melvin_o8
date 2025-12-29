#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void print_node_edges(Node *n, const char *label) {
    printf("\n%s (payload: '", label);
    if (n->payload && n->payload_size > 0) {
        printf("%.*s", (int)n->payload_size, (char*)n->payload);
    }
    printf("', weight: %.3f)\n", n->weight);
    
    printf("  Outgoing edges (%zu):\n", n->outgoing_count);
    for (size_t i = 0; i < n->outgoing_count; i++) {
        Edge *e = n->outgoing_edges[i];
        if (e && e->to_node) {
            printf("    -> '");
            if (e->to_node->payload && e->to_node->payload_size > 0) {
                printf("%.*s", (int)e->to_node->payload_size, (char*)e->to_node->payload);
            }
            printf("' (weight: %.3f)\n", e->weight);
        }
    }
}

int main() {
    printf("Testing output with edge debugging...\n\n");
    
    /* Create brain */
    MelvinMFile *brain = melvin_m_create("test_output_debug.m");
    assert(brain != NULL);
    
    /* Train on "hello" 10 times */
    const char *pattern = "hello";
    for (int i = 0; i < 10; i++) {
        melvin_m_universal_input_write(brain, (const uint8_t*)pattern, strlen(pattern));
        melvin_m_process_input(brain);
    }
    
    printf("After training on 'hello' x10:\n");
    printf("Graph: %zu nodes, %zu edges\n", 
           brain->graph->node_count, brain->graph->edge_count);
    
    /* Print all nodes and their edges */
    for (size_t i = 0; i < brain->graph->node_count; i++) {
        Node *n = brain->graph->nodes[i];
        if (n) {
            char label[100];
            snprintf(label, sizeof(label), "Node %zu", i);
            print_node_edges(n, label);
        }
    }
    
    /* Now test with "hel" */
    printf("\n\n=== Testing input 'hel' ===\n");
    const char *test_input = "hel";
    melvin_m_universal_input_write(brain, (const uint8_t*)test_input, strlen(test_input));
    melvin_m_process_input(brain);
    
    size_t output_size = melvin_m_universal_output_size(brain);
    printf("Output size: %zu bytes\n", output_size);
    
    if (output_size > 0) {
        uint8_t *output = (uint8_t*)malloc(output_size + 1);
        memcpy(output, brain->universal_output, output_size);
        output[output_size] = '\0';
        printf("Output: '%s'\n", (char*)output);
        free(output);
    }
    
    melvin_m_close(brain);
    return 0;
}
