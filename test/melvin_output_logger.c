/*
 * Melvin Output Logger
 * 
 * Logs outputs over time for analysis
 * - Tracks outputs with timestamps
 * - Records graph state
 * - Enables trend analysis
 */

#include "melvin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#define LOG_FILE_EXTENSION ".log"
#define MAX_LOG_LINE 4096

typedef struct {
    time_t timestamp;
    uint64_t adaptation_count;
    size_t input_size;
    size_t output_size;
    float similarity;
    size_t node_count;
    size_t edge_count;
    char *input_hex;
    char *output_hex;
    char *test_name;
} LogEntry;

/* Write output log entry */
bool melvin_log_output(const char *mfile_name, const char *test_name,
                       const uint8_t *input, size_t input_size,
                       const uint8_t *output, size_t output_size,
                       MelvinMFile *mfile) {
    if (!mfile_name) return false;
    
    /* Create log filename */
    char log_filename[512];
    snprintf(log_filename, sizeof(log_filename), "%s%s", mfile_name, LOG_FILE_EXTENSION);
    
    FILE *log_file = fopen(log_filename, "a");  /* Append mode */
    if (!log_file) return false;
    
    time_t now = time(NULL);
    
    /* Get graph state */
    size_t node_count = 0;
    size_t edge_count = 0;
    uint64_t adaptation_count = 0;
    if (mfile && mfile->graph) {
        node_count = mfile->graph->node_count;
        edge_count = mfile->graph->edge_count;
        adaptation_count = melvin_m_get_adaptation_count(mfile);
    }
    
    /* Calculate similarity */
    float similarity = 0.0f;
    if (input && output && input_size > 0 && output_size > 0) {
        size_t match_count = 0;
        size_t compare_len = (output_size < input_size) ? output_size : input_size;
        for (size_t i = 0; i < compare_len; i++) {
            if (output[i] == input[i]) match_count++;
        }
        similarity = compare_len > 0 ? (float)match_count / (float)compare_len : 0.0f;
    }
    
    /* Convert to hex strings */
    char input_hex[2048] = {0};
    char output_hex[2048] = {0};
    
    if (input && input_size > 0) {
        for (size_t i = 0; i < input_size && i < 1024; i++) {
            sprintf(input_hex + (i * 2), "%02X", input[i]);
        }
    }
    
    if (output && output_size > 0) {
        for (size_t i = 0; i < output_size && i < 1024; i++) {
            sprintf(output_hex + (i * 2), "%02X", output[i]);
        }
    }
    
    /* Write log entry */
    fprintf(log_file, "ENTRY\n");
    fprintf(log_file, "timestamp=%ld\n", now);
    fprintf(log_file, "test_name=%s\n", test_name ? test_name : "unnamed");
    fprintf(log_file, "adaptation_count=%llu\n", (unsigned long long)adaptation_count);
    fprintf(log_file, "input_size=%zu\n", input_size);
    fprintf(log_file, "output_size=%zu\n", output_size);
    fprintf(log_file, "similarity=%.4f\n", similarity);
    fprintf(log_file, "node_count=%zu\n", node_count);
    fprintf(log_file, "edge_count=%zu\n", edge_count);
    fprintf(log_file, "input_hex=%s\n", input_hex);
    fprintf(log_file, "output_hex=%s\n", output_hex);
    fprintf(log_file, "END_ENTRY\n\n");
    
    fclose(log_file);
    return true;
}

/* Read log entries from file */
LogEntry* melvin_read_log_entries(const char *mfile_name, size_t *entry_count) {
    if (!mfile_name || !entry_count) return NULL;
    
    char log_filename[512];
    snprintf(log_filename, sizeof(log_filename), "%s%s", mfile_name, LOG_FILE_EXTENSION);
    
    FILE *log_file = fopen(log_filename, "r");
    if (!log_file) {
        *entry_count = 0;
        return NULL;
    }
    
    size_t capacity = 64;
    LogEntry *entries = (LogEntry*)calloc(capacity, sizeof(LogEntry));
    if (!entries) {
        fclose(log_file);
        *entry_count = 0;
        return NULL;
    }
    
    size_t count = 0;
    char line[MAX_LOG_LINE];
    LogEntry *current = NULL;
    
    while (fgets(line, sizeof(line), log_file)) {
        /* Remove newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        if (strcmp(line, "ENTRY") == 0) {
            if (count >= capacity) {
                capacity *= 2;
                entries = (LogEntry*)realloc(entries, capacity * sizeof(LogEntry));
                if (!entries) break;
            }
            current = &entries[count++];
            memset(current, 0, sizeof(LogEntry));
        } else if (strcmp(line, "END_ENTRY") == 0) {
            current = NULL;
        } else if (current) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = line;
            char *value = eq + 1;
            
            if (strcmp(key, "timestamp") == 0) {
                current->timestamp = (time_t)atoll(value);
            } else if (strcmp(key, "test_name") == 0) {
                current->test_name = strdup(value);
            } else if (strcmp(key, "adaptation_count") == 0) {
                current->adaptation_count = (uint64_t)atoll(value);
            } else if (strcmp(key, "input_size") == 0) {
                current->input_size = (size_t)atoll(value);
            } else if (strcmp(key, "output_size") == 0) {
                current->output_size = (size_t)atoll(value);
            } else if (strcmp(key, "similarity") == 0) {
                current->similarity = (float)atof(value);
            } else if (strcmp(key, "node_count") == 0) {
                current->node_count = (size_t)atoll(value);
            } else if (strcmp(key, "edge_count") == 0) {
                current->edge_count = (size_t)atoll(value);
            } else if (strcmp(key, "input_hex") == 0) {
                current->input_hex = strdup(value);
            } else if (strcmp(key, "output_hex") == 0) {
                current->output_hex = strdup(value);
            }
        }
    }
    
    fclose(log_file);
    *entry_count = count;
    return entries;
}

/* Free log entries */
void melvin_free_log_entries(LogEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; i++) {
        if (entries[i].test_name) free(entries[i].test_name);
        if (entries[i].input_hex) free(entries[i].input_hex);
        if (entries[i].output_hex) free(entries[i].output_hex);
    }
    free(entries);
}

/* Analyze log entries over time */
void melvin_analyze_logs(const char *mfile_name) {
    if (!mfile_name) return;
    
    size_t entry_count = 0;
    LogEntry *entries = melvin_read_log_entries(mfile_name, &entry_count);
    
    if (!entries || entry_count == 0) {
        printf("No log entries found for %s\n", mfile_name);
        return;
    }
    
    printf("========================================\n");
    printf("Output Log Analysis: %s\n", mfile_name);
    printf("========================================\n\n");
    printf("Total entries: %zu\n\n", entry_count);
    
    /* Calculate statistics */
    float avg_similarity = 0.0f;
    float avg_output_ratio = 0.0f;
    size_t total_input_size = 0;
    size_t total_output_size = 0;
    size_t max_nodes = 0;
    size_t max_edges = 0;
    
    for (size_t i = 0; i < entry_count; i++) {
        avg_similarity += entries[i].similarity;
        if (entries[i].input_size > 0) {
            avg_output_ratio += (float)entries[i].output_size / (float)entries[i].input_size;
        }
        total_input_size += entries[i].input_size;
        total_output_size += entries[i].output_size;
        if (entries[i].node_count > max_nodes) max_nodes = entries[i].node_count;
        if (entries[i].edge_count > max_edges) max_edges = entries[i].edge_count;
    }
    
    if (entry_count > 0) {
        avg_similarity /= entry_count;
        avg_output_ratio /= entry_count;
    }
    
    printf("=== Overall Statistics ===\n");
    printf("Average similarity: %.2f%%\n", avg_similarity * 100.0f);
    printf("Average output/input ratio: %.2f\n", avg_output_ratio);
    printf("Total input bytes: %zu\n", total_input_size);
    printf("Total output bytes: %zu\n", total_output_size);
    printf("Peak nodes: %zu\n", max_nodes);
    printf("Peak edges: %zu\n\n", max_edges);
    
    /* Trend analysis */
    if (entry_count >= 2) {
        printf("=== Trend Analysis ===\n");
        float first_similarity = entries[0].similarity;
        float last_similarity = entries[entry_count - 1].similarity;
        float similarity_change = last_similarity - first_similarity;
        
        size_t first_nodes = entries[0].node_count;
        size_t last_nodes = entries[entry_count - 1].node_count;
        long node_growth = (long)last_nodes - (long)first_nodes;
        
        printf("Similarity change: %.2f%% -> %.2f%% (%.2f%% change)\n",
               first_similarity * 100.0f, last_similarity * 100.0f, similarity_change * 100.0f);
        if (node_growth >= 0) {
            printf("Node growth: %zu -> %zu (+%ld nodes)\n",
                   first_nodes, last_nodes, node_growth);
        } else {
            printf("Node growth: %zu -> %zu (%ld nodes)\n",
                   first_nodes, last_nodes, node_growth);
        }
        
        if (similarity_change > 0.05f) {
            printf("→ Similarity improving over time\n");
        } else if (similarity_change < -0.05f) {
            printf("→ Similarity decreasing (may be learning new patterns)\n");
        } else {
            printf("→ Similarity stable\n");
        }
        printf("\n");
    }
    
    /* Recent entries */
    printf("=== Recent Entries (last 5) ===\n");
    size_t start = (entry_count > 5) ? entry_count - 5 : 0;
    for (size_t i = start; i < entry_count; i++) {
        struct tm *tm_info = localtime(&entries[i].timestamp);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("\nEntry %zu: %s\n", i + 1, entries[i].test_name ? entries[i].test_name : "unnamed");
        printf("  Time: %s\n", time_str);
        printf("  Adaptation: %llu\n", (unsigned long long)entries[i].adaptation_count);
        printf("  Input: %zu bytes, Output: %zu bytes\n", entries[i].input_size, entries[i].output_size);
        printf("  Similarity: %.2f%%\n", entries[i].similarity * 100.0f);
        printf("  Graph: %zu nodes, %zu edges\n", entries[i].node_count, entries[i].edge_count);
        
        if (entries[i].output_hex && strlen(entries[i].output_hex) <= 64) {
            printf("  Output (hex): %s\n", entries[i].output_hex);
        }
    }
    
    melvin_free_log_entries(entries, entry_count);
}

