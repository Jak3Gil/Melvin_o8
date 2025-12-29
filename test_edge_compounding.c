/*
 * Test: Analyze Edge Compounding vs Duplication
 * 
 * This test processes multiple inputs and tracks:
 * 1. Are edges duplicating or compounding (strengthening)?
 * 2. What are outputs like?
 * 3. How is the system growing?
 * 
 * Usage:
 *   test_edge_compounding [brain_file.m] [iterations]
 */

#include "melvin_m.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <float.h>

typedef struct {
    size_t nodes;
    size_t edges;
    float edges_per_node;
    float avg_edge_weight;
    float max_edge_weight;
    float min_edge_weight;
    size_t output_size;
    size_t adaptations;
} GraphStats;

GraphStats analyze_graph(MelvinGraph *graph) {
    GraphStats stats = {0};
    if (!graph || graph->node_count == 0) return stats;
    
    stats.nodes = graph->node_count;
    stats.edges = graph->edge_count;
    stats.edges_per_node = (float)stats.edges / (float)stats.nodes;
    
    /* Analyze edge weights */
    float total_weight = 0.0f;
    float max_weight = 0.0f;
    float min_weight = FLT_MAX;
    size_t weighted_edges = 0;
    
    for (size_t i = 0; i < graph->edge_count; i++) {
        Edge *edge = graph->edges[i];
        if (!edge) continue;
        
        float weight = edge->weight;
        if (weight > 0.0f) {
            total_weight += weight;
            weighted_edges++;
            if (weight > max_weight) max_weight = weight;
            if (weight < min_weight) min_weight = weight;
        }
    }
    
    if (weighted_edges > 0) {
        stats.avg_edge_weight = total_weight / (float)weighted_edges;
        stats.max_edge_weight = max_weight;
        stats.min_edge_weight = min_weight;
    }
    
    return stats;
}

void print_stats(const char *label, GraphStats *stats, GraphStats *prev_stats) {
    printf("\n=== %s ===\n", label);
    printf("Nodes: %zu", stats->nodes);
    if (prev_stats && prev_stats->nodes > 0) {
        printf(" (+%zu, +%.1f%%)", 
               stats->nodes - prev_stats->nodes,
               ((float)(stats->nodes - prev_stats->nodes) / (float)prev_stats->nodes) * 100.0f);
    }
    printf("\n");
    
    printf("Edges: %zu", stats->edges);
    if (prev_stats && prev_stats->edges > 0) {
        printf(" (+%zu, +%.1f%%)", 
               stats->edges - prev_stats->edges,
               ((float)(stats->edges - prev_stats->edges) / (float)prev_stats->edges) * 100.0f);
    }
    printf("\n");
    
    printf("Edges/Node: %.2f", stats->edges_per_node);
    if (prev_stats && prev_stats->edges_per_node > 0.0f) {
        float change = stats->edges_per_node - prev_stats->edges_per_node;
        printf(" (%+.2f", change);
        if (change > 0.1f) {
            printf(" ⚠️ DUPLICATING");
        } else if (change < -0.1f) {
            printf(" ✓ COMPOUNDING");
        } else {
            printf(" ~ STABLE");
        }
        printf(")");
    }
    printf("\n");
    
    printf("Edge Weights:\n");
    printf("  Average: %.4f", stats->avg_edge_weight);
    if (prev_stats && prev_stats->avg_edge_weight > 0.0f) {
        float change = stats->avg_edge_weight - prev_stats->avg_edge_weight;
        printf(" (%+.4f", change);
        if (change > 0.001f) {
            printf(" ✓ STRENGTHENING");
        } else if (change < -0.001f) {
            printf(" ⚠️ WEAKENING");
        } else {
            printf(" ~ STABLE");
        }
        printf(")");
    }
    printf("\n");
    printf("  Max: %.4f\n", stats->max_edge_weight);
    printf("  Min: %.4f\n", stats->min_edge_weight);
    
    printf("Output Size: %zu bytes\n", stats->output_size);
    printf("Adaptations: %zu\n", stats->adaptations);
}

void print_output_preview(MelvinMFile *mfile) {
    size_t output_size = melvin_m_universal_output_size(mfile);
    if (output_size == 0) {
        printf("  (No output - thinking mode)\n");
        return;
    }
    
    uint8_t *output = (uint8_t*)malloc(output_size);
    if (!output) {
        printf("  (Failed to read output)\n");
        return;
    }
    
    size_t read = melvin_m_universal_output_read(mfile, output, output_size);
    printf("  Output (%zu bytes): \"", read);
    for (size_t i = 0; i < read && i < 80; i++) {
        if (output[i] >= 32 && output[i] < 127) {
            printf("%c", output[i]);
        } else {
            printf(".");
        }
    }
    if (read > 80) printf("...");
    printf("\"\n");
    
    free(output);
}

int main(int argc, char *argv[]) {
    const char *brain_file = "test_edge_analysis.m";
    int iterations = 5;
    
    if (argc >= 2) {
        brain_file = argv[1];
    }
    if (argc >= 3) {
        iterations = atoi(argv[2]);
        if (iterations < 1) iterations = 5;
    }
    
    printf("========================================\n");
    printf("Edge Compounding vs Duplication Test\n");
    printf("========================================\n\n");
    printf("Brain file: %s\n", brain_file);
    printf("Iterations: %d\n", iterations);
    printf("\n");
    
    /* Remove old file if it exists */
    unlink(brain_file);
    
    /* Create new .m file */
    MelvinMFile *mfile = melvin_m_create(brain_file);
    if (!mfile) {
        fprintf(stderr, "ERROR: Failed to create .m file\n");
        return 1;
    }
    
    const char *test_inputs[] = {
        "Hello Melvin!",
        "Hello Melvin!",
        "Hello Melvin!",
        "Hello World!",
        "Hello World!",
        "The quick brown fox",
        "The quick brown fox",
        "Hello Melvin!",
    };
    int num_inputs = sizeof(test_inputs) / sizeof(test_inputs[0]);
    if (iterations > num_inputs) iterations = num_inputs;
    
    GraphStats prev_stats = {0};
    
    for (int i = 0; i < iterations; i++) {
        printf("\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("ITERATION %d/%d\n", i + 1, iterations);
        printf("═══════════════════════════════════════════════════════════\n");
        
        const char *input_text = test_inputs[i];
        size_t input_len = strlen(input_text);
        size_t input_size = input_len + 1;  /* +1 for port ID */
        uint8_t *input_data = (uint8_t*)malloc(input_size);
        if (!input_data) {
            fprintf(stderr, "ERROR: Failed to allocate input\n");
            melvin_m_close(mfile);
            return 1;
        }
        
        input_data[0] = 1;  /* Port ID = 1 */
        memcpy(input_data + 1, input_text, input_len);
        
        printf("Input: \"%s\"\n", input_text);
        
        /* Write input */
        melvin_m_universal_input_write(mfile, input_data, input_size);
        
        /* Process */
        clock_t start = clock();
        bool success = melvin_m_process_input(mfile);
        clock_t end = clock();
        double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
        
        if (!success) {
            fprintf(stderr, "ERROR: Failed to process input\n");
            free(input_data);
            continue;
        }
        
        printf("Processing time: %.2f ms\n", elapsed);
        
        /* Analyze graph */
        GraphStats stats = analyze_graph(mfile->graph);
        stats.adaptations = melvin_m_get_adaptation_count(mfile);
        stats.output_size = melvin_m_universal_output_size(mfile);
        
        print_stats("Graph Statistics", &stats, i > 0 ? &prev_stats : NULL);
        
        printf("\nOutput:\n");
        print_output_preview(mfile);
        
        /* Check for edge duplication */
        if (i > 0) {
            float edges_growth_rate = (float)(stats.edges - prev_stats.edges) / (float)prev_stats.edges;
            float nodes_growth_rate = (float)(stats.nodes - prev_stats.nodes) / (float)prev_stats.nodes;
            
            if (edges_growth_rate > nodes_growth_rate * 1.5f && stats.edges_per_node > prev_stats.edges_per_node) {
                printf("\n⚠️  WARNING: Edges growing faster than nodes (possible duplication)\n");
                printf("   Edges growth: +%.1f%%, Nodes growth: +%.1f%%\n", 
                       edges_growth_rate * 100.0f, nodes_growth_rate * 100.0f);
            } else if (stats.avg_edge_weight > prev_stats.avg_edge_weight && 
                      stats.edges_per_node <= prev_stats.edges_per_node + 0.1f) {
                printf("\n✓ GOOD: Edges are compounding (weights increasing, count stable)\n");
            }
        }
        
        prev_stats = stats;
        free(input_data);
    }
    
    /* Final summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("FINAL SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    GraphStats final_stats = analyze_graph(mfile->graph);
    final_stats.adaptations = melvin_m_get_adaptation_count(mfile);
    final_stats.output_size = melvin_m_universal_output_size(mfile);
    
    print_stats("Final State", &final_stats, NULL);
    
    /* Save */
    printf("\nSaving brain file...\n");
    if (melvin_m_save(mfile)) {
        printf("✓ Saved %s\n", brain_file);
        
        FILE *f = fopen(brain_file, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long file_size = ftell(f);
            fclose(f);
            printf("  File size: %ld bytes\n", file_size);
        }
    } else {
        fprintf(stderr, "ERROR: Failed to save\n");
    }
    
    melvin_m_close(mfile);
    
    printf("\n========================================\n");
    printf("Test complete!\n");
    printf("========================================\n");
    
    return 0;
}

