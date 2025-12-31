/*
 * .m File Analysis Tool
 * 
 * Analyzes .m file structure, features, and learning patterns
 */

#include "melvin_m.h"
#include "melvin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void print_node_details(Node *node, size_t index, FILE *out) {
    if (!node || !out) return;
    
    fprintf(out, "  Node %zu:\n", index);
    fprintf(out, "    ID: %s\n", node->id);
    fprintf(out, "    Payload size: %zu bytes\n", node->payload_size);
    fprintf(out, "    Abstraction level: %u\n", node->abstraction_level);
    fprintf(out, "    Weight: %.4f\n", node->weight);
    fprintf(out, "    Activation: %.4f\n", node->activation_strength);
    fprintf(out, "    Bias: %.4f\n", node->bias);
    fprintf(out, "    Outgoing edges: %zu\n", node->outgoing_count);
    fprintf(out, "    Incoming edges: %zu\n", node->incoming_count);
    fprintf(out, "    Outgoing weight sum: %.4f\n", node->outgoing_weight_sum);
    fprintf(out, "    Incoming weight sum: %.4f\n", node->incoming_weight_sum);
    
    if (node->payload_size > 0 && node->payload_size <= 64) {
        fprintf(out, "    Payload preview: ");
        for (size_t i = 0; i < node->payload_size; i++) {
            if (node->payload[i] >= 32 && node->payload[i] < 127) {
                fprintf(out, "%c", node->payload[i]);
            } else {
                fprintf(out, "\\x%02x", node->payload[i]);
            }
        }
        fprintf(out, "\n");
    }
    
    /* Show top outgoing edges */
    if (node->outgoing_count > 0) {
        fprintf(out, "    Top outgoing edges:\n");
        size_t show_count = (node->outgoing_count < 5) ? node->outgoing_count : 5;
        for (size_t i = 0; i < show_count; i++) {
            Edge *edge = node->outgoing_edges[i];
            if (edge && edge->to_node) {
                fprintf(out, "      -> %s (weight: %.4f)\n", 
                        edge->to_node->id, edge->weight);
            }
        }
    }
    fprintf(out, "\n");
}

void analyze_mfile(const char *filename, FILE *report) {
    if (!filename || !report) return;
    
    fprintf(report, "=== .m File Analysis: %s ===\n\n", filename);
    
    /* Open .m file */
    MelvinMFile *mfile = melvin_m_open(filename);
    if (!mfile) {
        fprintf(report, "ERROR: Could not open .m file\n");
        return;
    }
    
    /* File info */
    MelvinMHeader *header = &mfile->header;
    fprintf(report, "File Information:\n");
    fprintf(report, "  Magic: 0x%016llX\n", (unsigned long long)header->magic);
    fprintf(report, "  Version: %u\n", header->version);
    fprintf(report, "  Last modified: %s", ctime((time_t*)&header->last_modified));
    fprintf(report, "  Adaptations: %llu\n", (unsigned long long)header->adaptation_count);
    fprintf(report, "\n");
    
    /* Graph structure */
    MelvinGraph *graph = melvin_m_get_graph(mfile);
    if (!graph) {
        fprintf(report, "ERROR: Could not get graph\n");
        melvin_m_close(mfile);
        return;
    }
    
    fprintf(report, "Graph Structure:\n");
    fprintf(report, "  Nodes: %zu\n", graph->node_count);
    fprintf(report, "  Edges: %zu\n", graph->edge_count);
    fprintf(report, "\n");
    
    /* Node analysis */
    fprintf(report, "Node Analysis:\n");
    size_t hierarchy_count = 0;
    size_t blank_count = 0;
    size_t total_payload = 0;
    float total_weight = 0.0f;
    float max_weight = 0.0f;
    Node *max_weight_node = NULL;
    
    for (size_t i = 0; i < graph->node_count; i++) {
        Node *node = graph->nodes[i];
        if (!node) continue;
        
        if (node->abstraction_level > 0) hierarchy_count++;
        if (node->payload_size == 0) blank_count++;
        total_payload += node->payload_size;
        total_weight += node->weight;
        
        if (node->weight > max_weight) {
            max_weight = node->weight;
            max_weight_node = node;
        }
    }
    
    fprintf(report, "  Hierarchy nodes: %zu (%.1f%%)\n", hierarchy_count,
            graph->node_count > 0 ? (100.0f * hierarchy_count / graph->node_count) : 0.0f);
    fprintf(report, "  Blank nodes: %zu (%.1f%%)\n", blank_count,
            graph->node_count > 0 ? (100.0f * blank_count / graph->node_count) : 0.0f);
    fprintf(report, "  Total payload: %zu bytes\n", total_payload);
    fprintf(report, "  Average payload: %.2f bytes\n",
            graph->node_count > 0 ? (float)total_payload / graph->node_count : 0.0f);
    fprintf(report, "  Average weight: %.4f\n",
            graph->node_count > 0 ? total_weight / graph->node_count : 0.0f);
    fprintf(report, "  Max weight: %.4f\n", max_weight);
    if (max_weight_node) {
        fprintf(report, "  Max weight node: %s (level %u, %zu bytes)\n",
                max_weight_node->id, max_weight_node->abstraction_level,
                max_weight_node->payload_size);
    }
    fprintf(report, "\n");
    
    /* Edge analysis */
    fprintf(report, "Edge Analysis:\n");
    float total_edge_weight = 0.0f;
    float max_edge_weight = 0.0f;
    size_t self_loops = 0;
    
    for (size_t i = 0; i < graph->edge_count; i++) {
        Edge *edge = graph->edges[i];
        if (!edge) continue;
        
        total_edge_weight += edge->weight;
        if (edge->weight > max_edge_weight) max_edge_weight = edge->weight;
        if (edge->from_node == edge->to_node) self_loops++;
    }
    
    fprintf(report, "  Average edge weight: %.4f\n",
            graph->edge_count > 0 ? total_edge_weight / graph->edge_count : 0.0f);
    fprintf(report, "  Max edge weight: %.4f\n", max_edge_weight);
    fprintf(report, "  Self-loops: %zu (%.1f%%)\n", self_loops,
            graph->edge_count > 0 ? (100.0f * self_loops / graph->edge_count) : 0.0f);
    fprintf(report, "\n");
    
    /* Show top nodes */
    fprintf(report, "Top 10 Nodes by Weight:\n");
    Node **sorted = (Node**)malloc(graph->node_count * sizeof(Node*));
    if (sorted) {
        for (size_t i = 0; i < graph->node_count; i++) {
            sorted[i] = graph->nodes[i];
        }
        
        /* Bubble sort by weight */
        for (size_t i = 0; i < graph->node_count - 1; i++) {
            for (size_t j = 0; j < graph->node_count - 1 - i; j++) {
                if (sorted[j]->weight < sorted[j + 1]->weight) {
                    Node *temp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = temp;
                }
            }
        }
        
        size_t show = (graph->node_count < 10) ? graph->node_count : 10;
        for (size_t i = 0; i < show; i++) {
            print_node_details(sorted[i], i + 1, report);
        }
        free(sorted);
    }
    
    /* I/O state */
    fprintf(report, "I/O State:\n");
    fprintf(report, "  Input buffer: %zu bytes\n", melvin_m_universal_input_size(mfile));
    fprintf(report, "  Output buffer: %zu bytes\n", melvin_m_universal_output_size(mfile));
    fprintf(report, "\n");
    
    melvin_m_close(mfile);
    
    fprintf(report, "Analysis complete.\n");
}

int main(int argc, char *argv[]) {
    const char *mfile = "../brain.m";  /* Always use brain.m in root directory */
    const char *report_file = (argc > 1) ? argv[1] : "mfile_analysis.txt";
    
    FILE *report = fopen(report_file, "w");
    if (!report) {
        fprintf(stderr, "Error: Could not open report file: %s\n", report_file);
        return 1;
    }
    
    fprintf(report, "Melvin .m File Analysis Report\n");
    fprintf(report, "Generated: %s", ctime(&(time_t){time(NULL)}));
    fprintf(report, "=====================================\n\n");
    
    analyze_mfile(mfile, report);
    
    fclose(report);
    
    printf("Analysis complete. Report: %s\n", report_file);
    return 0;
}

