#include "melvin_m.h"
#include <stdio.h>
#include <string.h>

/* Add debug version of compute_output_readiness */
extern float compute_output_readiness(MelvinGraph *g, Node **input_nodes, size_t input_count);

int main() {
    MelvinMFile *mfile = melvin_m_create("readiness_debug.m");
    if (!mfile) return 1;
    
    printf("=== Debugging Output Readiness ===\n\n");
    
    const char *input = "hello";
    
    /* Process multiple times to build edges */
    for (int i = 0; i < 3; i++) {
        printf("Iteration %d:\n", i+1);
        melvin_m_universal_input_write(mfile, (uint8_t*)input, strlen(input));
        melvin_m_process_input(mfile);
        
        /* Get initial nodes from sequential pattern processing */
        Node **seq_nodes = NULL;
        size_t seq_count = 0;
        if (mfile->universal_input && mfile->header.universal_input_size > 0) {
            /* Manually call to get nodes */
            seq_nodes = wave_process_sequential_patterns(mfile->graph, 
                mfile->universal_input, mfile->header.universal_input_size, &seq_count);
        }
        
        if (seq_nodes && seq_count > 0) {
            float readiness = compute_output_readiness(mfile->graph, seq_nodes, seq_count);
            printf("  Output readiness: %.6f\n", readiness);
            printf("  Input nodes: %zu\n", seq_count);
            
            /* Check edges on first node */
            if (seq_nodes[0]) {
                printf("  First node outgoing edges: %zu\n", seq_nodes[0]->outgoing_count);
                if (seq_nodes[0]->outgoing_count > 0) {
                    printf("  First edge weight: %.6f\n", seq_nodes[0]->outgoing_edges[0]->weight);
                }
            }
        }
        
        size_t output_size = melvin_m_universal_output_size(mfile);
        printf("  Output size: %zu\n\n", output_size);
        
        if (seq_nodes) free(seq_nodes);
    }
    
    melvin_m_close(mfile);
    return 0;
}
