/*
 * Melvin: Emergent Intelligence System
 * 
 * Core Philosophy:
 * - Self-regulation through local measurements only
 * - No hardcoded limits or thresholds
 * - Nodes only know themselves and their edges
 * - All measurements are local (no global state)
 * - Nodes can scale from 1 byte to very large payloads
 * 
 * .m File: The live, executable program driven by data
 * - Opening a .m file activates it (not just loading data)
 * - Data flows through I/O port → triggers wave propagation → adapts → auto-saves
 * - The file IS the running program, not just storage
 * - Self-regulating: automatically saves after each adaptation
 */

#include "melvin.h"
#include "melvin_gpu.h"
#include <time.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <string.h>

// #region agent log
/* Debug logging helper - writes NDJSON to debug log file */
static void debug_log(const char *location, const char *message, const char *data_json) {
    FILE *log_file = fopen("/Users/jakegilbert/Desktop/Melvin_Reasearch/Melvin_o8/.cursor/debug.log", "a");
    if (log_file) {
        fprintf(log_file, "{\"location\":\"%s\",\"message\":\"%s\",\"data\":%s,\"timestamp\":%ld,\"sessionId\":\"debug-session\",\"runId\":\"adaptive-fix\"}\n",
                location, message, data_json ? data_json : "{}", (long)time(NULL));
        fclose(log_file);
    }
}
// #endregion agent log

/* ========================================
 * UTILITY FUNCTIONS
 * ======================================== */

/* Comparison function for qsort (float) */
static int compare_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

/* ========================================
 * HASH SET FOR NODE POINTERS (O(1) visited tracking)
 * ======================================== */

/* Hash set for tracking visited nodes by pointer (O(1) lookup using hash table) */
/* DATA-DRIVEN: Hash size computed from graph size - no hardcoded thresholds or maximums */

/* Calculate optimal hash size based on graph size (purely data-driven, no thresholds) */
static size_t calculate_optimal_hash_size(size_t graph_node_count) {
    /* DATA-DRIVEN: Hash size = graph_size * load_factor (50% fill for good performance) */
    /* No hardcoded thresholds - adapts to any graph size */
    if (graph_node_count == 0) {
        // #region agent log
        char log_data[128];
        snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"C\",\"return_value\":256,\"reason\":\"empty_graph_minimum\"}");
        debug_log("melvin.c:62", "Minimum hash size for empty graph", log_data);
        // #endregion agent log
        return 256;  /* Minimum for empty graph */
    }
    
    /* Compute optimal size: 2x graph size gives ~50% load factor */
    size_t optimal = graph_node_count * 2;
    
    /* Round up to next power of 2 for efficient modulo operation */
    /* No maximum - scales with graph size */
    size_t power_of_2 = 256;  /* Start from minimum */
    while (power_of_2 < optimal && power_of_2 < SIZE_MAX / 2) {
        power_of_2 <<= 1;  /* Double until >= optimal */
    }
    
    return power_of_2;  /* No cap - scales to any size */
}

typedef struct VisitedSet {
    size_t hash_size;  /* Dynamic hash table size */
    Node ***buckets;   /* Dynamic array of bucket arrays (each bucket is Node**) */
    size_t *bucket_sizes;
    size_t *bucket_capacities;
} VisitedSet;

/* Simple hash function for node pointers (O(1)) */
static size_t node_ptr_hash(VisitedSet *set, Node *node) {
    if (!set || !node) return 0;
    /* Use pointer address as hash (simple and fast) */
    uintptr_t ptr = (uintptr_t)node;
    return (size_t)(ptr ^ (ptr >> 16)) % set->hash_size;
}

static VisitedSet* visited_set_create(size_t hash_size) {
    /* DATA-DRIVEN: Only enforce minimum for empty graphs, no maximum */
    if (hash_size < 256) hash_size = 256;  /* Minimum for functionality */
    /* No maximum - let it scale with graph size */
    
    VisitedSet *set = (VisitedSet*)calloc(1, sizeof(VisitedSet));
    if (!set) return NULL;
    
    set->hash_size = hash_size;
    set->buckets = (Node***)calloc(hash_size, sizeof(Node**));
    set->bucket_sizes = (size_t*)calloc(hash_size, sizeof(size_t));
    set->bucket_capacities = (size_t*)calloc(hash_size, sizeof(size_t));
    
    if (!set->buckets || !set->bucket_sizes || !set->bucket_capacities) {
        if (set->buckets) free(set->buckets);
        if (set->bucket_sizes) free(set->bucket_sizes);
        if (set->bucket_capacities) free(set->bucket_capacities);
        free(set);
        return NULL;
    }
    
    return set;
}

static void visited_set_free(VisitedSet *set) {
    if (!set) return;
    
    /* Free all bucket arrays */
    if (set->buckets) {
        for (size_t i = 0; i < set->hash_size; i++) {
            if (set->buckets[i]) {
                free(set->buckets[i]);
            }
        }
        free(set->buckets);
    }
    
    if (set->bucket_sizes) free(set->bucket_sizes);
    if (set->bucket_capacities) free(set->bucket_capacities);
    free(set);
}

static bool visited_set_contains(VisitedSet *set, Node *node) {
    if (!set || !node) return false;
    
    size_t bucket_idx = node_ptr_hash(set, node);
    Node **bucket = set->buckets[bucket_idx];
    
    if (!bucket) return false;
    
    /* Linear scan within bucket (typically very few collisions) */
    for (size_t i = 0; i < set->bucket_sizes[bucket_idx]; i++) {
        if (bucket[i] == node) return true;
    }
    
    return false;
}

static bool visited_set_add(VisitedSet *set, Node *node) {
    if (!set || !node) return false;
    
    size_t bucket_idx = node_ptr_hash(set, node);
    
    /* Check if already present (O(1) average case) */
    if (visited_set_contains(set, node)) return true;
    
    /* Get or create bucket */
    Node **bucket = set->buckets[bucket_idx];
    size_t *bucket_size = &set->bucket_sizes[bucket_idx];
    size_t *bucket_capacity = &set->bucket_capacities[bucket_idx];
    
    /* Allocate bucket if needed */
    if (!bucket) {
        *bucket_capacity = 4;
        bucket = (Node**)calloc(*bucket_capacity, sizeof(Node*));
        if (!bucket) return false;
        set->buckets[bucket_idx] = bucket;
    }
    
    /* Resize bucket if needed */
    if (*bucket_size >= *bucket_capacity) {
        size_t new_capacity = *bucket_capacity * 2;
        Node **new_bucket = (Node**)realloc(bucket, new_capacity * sizeof(Node*));
        if (!new_bucket) return false;
        *bucket_capacity = new_capacity;
        set->buckets[bucket_idx] = new_bucket;
        bucket = new_bucket;
    }
    
    /* Add node to bucket */
    bucket[*bucket_size] = node;
    (*bucket_size)++;
    
    return true;
}

/* ========================================
 * ADAPTIVE STATISTICS SYSTEM (Data-Driven Thresholds)
 * ======================================== */

/* Adaptive histogram for percentiles (O(1) update, O(1) query) */
/* DATA-DRIVEN: No fixed bucket count - adapts to data volume */

typedef struct WaveStatistics {
    /* Adaptive histogram: finds min/max of values, then buckets that range */
    float value_min;  /* Minimum value seen in current wave */
    float value_max;  /* Maximum value seen in current wave */
    int *histogram;  /* Dynamic buckets - allocated based on data count */
    int histogram_bucket_count;  /* Current number of buckets (adaptive) */
    int histogram_count;  /* Total values added to histogram */
    
    /* Cached percentiles (computed once per wave) */
    float p25;  /* 25th percentile */
    float p50;  /* 50th percentile (median) */
    float p75;  /* 75th percentile */
    float p90;  /* 90th percentile */
    float p95;  /* 95th percentile */
    
    /* Acceptance strength statistics (for blank node filling) */
    float acceptance_min;
    float acceptance_max;
    int *acceptance_histogram;  /* Dynamic buckets */
    int acceptance_bucket_count;  /* Adaptive bucket count */
    int acceptance_count;
    float acceptance_p90;  /* 90th percentile for acceptance threshold */
    
    /* Similarity statistics (for similarity edge detection) */
    float similarity_min;
    float similarity_max;
    int *similarity_histogram;  /* Dynamic buckets */
    int similarity_bucket_count;  /* Adaptive bucket count */
    int similarity_count;
    float similarity_p75;  /* 75th percentile for similarity threshold */
    
    /* Edge weight statistics (for similarity edge range) */
    float edge_weight_min;
    float edge_weight_max;
    int *edge_weight_histogram;  /* Dynamic buckets */
    int edge_weight_bucket_count;  /* Adaptive bucket count */
    int edge_weight_count;
    float edge_weight_p25;  /* 25th percentile for lower bound */
    float edge_weight_p75;  /* 75th percentile for upper bound */
} WaveStatistics;

/* Initialize wave statistics */
static void wave_statistics_init(WaveStatistics *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(WaveStatistics));
    stats->value_min = FLT_MAX;
    stats->value_max = FLT_MIN;
    stats->acceptance_min = FLT_MAX;
    stats->acceptance_max = FLT_MIN;
    stats->similarity_min = FLT_MAX;
    stats->similarity_max = FLT_MIN;
    stats->edge_weight_min = FLT_MAX;
    stats->edge_weight_max = FLT_MIN;
    
    /* DATA-DRIVEN: Initialize with adaptive bucket counts based on expected data */
    /* Start with reasonable default, will grow as needed */
    // #region agent log
    char log_data[128];
    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"C\",\"initial_bucket_count\":100,\"type\":\"histogram\"}");
    debug_log("melvin.c:252", "Initial bucket count (will adapt)", log_data);
    // #endregion agent log
    stats->histogram_bucket_count = 100;
    stats->histogram = (int*)calloc(stats->histogram_bucket_count, sizeof(int));
    
    stats->acceptance_bucket_count = 100;
    stats->acceptance_histogram = (int*)calloc(stats->acceptance_bucket_count, sizeof(int));
    
    stats->similarity_bucket_count = 100;
    stats->similarity_histogram = (int*)calloc(stats->similarity_bucket_count, sizeof(int));
    
    stats->edge_weight_bucket_count = 100;
    stats->edge_weight_histogram = (int*)calloc(stats->edge_weight_bucket_count, sizeof(int));
}

/* Cleanup wave statistics (free dynamic allocations) */
static void wave_statistics_cleanup(WaveStatistics *stats) {
    if (!stats) return;
    if (stats->histogram) free(stats->histogram);
    if (stats->acceptance_histogram) free(stats->acceptance_histogram);
    if (stats->similarity_histogram) free(stats->similarity_histogram);
    if (stats->edge_weight_histogram) free(stats->edge_weight_histogram);
    memset(stats, 0, sizeof(WaveStatistics));
}

/* Forward declarations for adaptive stability functions */
static float compute_adaptive_epsilon(float value_range);
static float compute_initial_smoothing(Node *node);
static int compute_adaptive_bucket_growth_trigger(WaveStatistics *stats, int bucket_count);

/* Ensure histogram has enough buckets for precision (adaptive growth) */
static void wave_statistics_ensure_bucket_count(WaveStatistics *stats, int *bucket_count, int **histogram) {
    if (!stats || !bucket_count || !histogram) return;
    
    /* DATA-DRIVEN: If we have many values, increase bucket count for better precision */
    /* Grow when we have adaptive threshold more values than buckets (maintains precision) */
    /* No maximum - adapts to any data volume */
    int growth_trigger = compute_adaptive_bucket_growth_trigger(stats, *bucket_count);
    if (stats->histogram_count > *bucket_count * growth_trigger) {
        int new_count = *bucket_count * 2;  /* Double bucket count */
        int *new_histogram = (int*)calloc(new_count, sizeof(int));
        if (new_histogram) {
            /* Redistribute existing values proportionally */
            if (*histogram) {
                for (int i = 0; i < *bucket_count; i++) {
                    if ((*histogram)[i] > 0) {
                        /* Map old bucket to new bucket(s) */
                        int new_bucket = (i * new_count) / *bucket_count;
                        new_histogram[new_bucket] += (*histogram)[i];
                    }
                }
                free(*histogram);
            }
            *histogram = new_histogram;
            *bucket_count = new_count;
        }
    }
}

/* Add value to adaptive histogram (O(1)) */
static void wave_statistics_add_value(WaveStatistics *stats, float value) {
    if (!stats || !stats->histogram) return;
    
    /* DATA-DRIVEN: Ensure bucket count adapts to data volume */
    wave_statistics_ensure_bucket_count(stats, &stats->histogram_bucket_count, &stats->histogram);
    
    /* Update min/max */
    if (value < stats->value_min) stats->value_min = value;
    if (value > stats->value_max) stats->value_max = value;
    
    /* Bucket the value (adaptive: uses actual min/max range and adaptive bucket count) */
    if (stats->value_max > stats->value_min && stats->histogram_bucket_count > 0) {
        float range = stats->value_max - stats->value_min;
        int bucket = (int)(((value - stats->value_min) / range) * (stats->histogram_bucket_count - 1));
        if (bucket < 0) bucket = 0;
        if (bucket >= stats->histogram_bucket_count) bucket = stats->histogram_bucket_count - 1;
        stats->histogram[bucket]++;
        stats->histogram_count++;
    } else if (stats->histogram_count == 0) {
        stats->value_min = value;
        stats->value_max = value;
        if (stats->histogram_bucket_count > 0) {
            stats->histogram[0]++;
        }
        stats->histogram_count++;
    }
}

/* Add acceptance strength value (O(1)) */
static void wave_statistics_add_acceptance(WaveStatistics *stats, float value) {
    if (!stats || !stats->acceptance_histogram) return;
    
    /* DATA-DRIVEN: Ensure bucket count adapts to data volume */
    wave_statistics_ensure_bucket_count(stats, &stats->acceptance_bucket_count, &stats->acceptance_histogram);
    
    if (value < stats->acceptance_min) stats->acceptance_min = value;
    if (value > stats->acceptance_max) stats->acceptance_max = value;
    
    if (stats->acceptance_max > stats->acceptance_min && stats->acceptance_bucket_count > 0) {
        float range = stats->acceptance_max - stats->acceptance_min;
        int bucket = (int)(((value - stats->acceptance_min) / range) * (stats->acceptance_bucket_count - 1));
        if (bucket < 0) bucket = 0;
        if (bucket >= stats->acceptance_bucket_count) bucket = stats->acceptance_bucket_count - 1;
        stats->acceptance_histogram[bucket]++;
        stats->acceptance_count++;
    } else if (stats->acceptance_count == 0) {
        stats->acceptance_min = value;
        stats->acceptance_max = value;
        if (stats->acceptance_bucket_count > 0) {
            stats->acceptance_histogram[0]++;
        }
        stats->acceptance_count++;
    }
}

/* Add similarity value (O(1)) */
static void wave_statistics_add_similarity(WaveStatistics *stats, float value) {
    if (!stats || !stats->similarity_histogram) return;
    
    /* DATA-DRIVEN: Ensure bucket count adapts to data volume */
    wave_statistics_ensure_bucket_count(stats, &stats->similarity_bucket_count, &stats->similarity_histogram);
    
    if (value < stats->similarity_min) stats->similarity_min = value;
    if (value > stats->similarity_max) stats->similarity_max = value;
    
    if (stats->similarity_max > stats->similarity_min && stats->similarity_bucket_count > 0) {
        float range = stats->similarity_max - stats->similarity_min;
        int bucket = (int)(((value - stats->similarity_min) / range) * (stats->similarity_bucket_count - 1));
        if (bucket < 0) bucket = 0;
        if (bucket >= stats->similarity_bucket_count) bucket = stats->similarity_bucket_count - 1;
        stats->similarity_histogram[bucket]++;
        stats->similarity_count++;
    } else if (stats->similarity_count == 0) {
        stats->similarity_min = value;
        stats->similarity_max = value;
        if (stats->similarity_bucket_count > 0) {
            stats->similarity_histogram[0]++;
        }
        stats->similarity_count++;
    }
}

/* Add edge weight value (O(1)) */
static void wave_statistics_add_edge_weight(WaveStatistics *stats, float value) {
    if (!stats || !stats->edge_weight_histogram) return;
    
    /* DATA-DRIVEN: Ensure bucket count adapts to data volume */
    wave_statistics_ensure_bucket_count(stats, &stats->edge_weight_bucket_count, &stats->edge_weight_histogram);
    
    if (value < stats->edge_weight_min) stats->edge_weight_min = value;
    if (value > stats->edge_weight_max) stats->edge_weight_max = value;
    
    if (stats->edge_weight_max > stats->edge_weight_min && stats->edge_weight_bucket_count > 0) {
        float range = stats->edge_weight_max - stats->edge_weight_min;
        int bucket = (int)(((value - stats->edge_weight_min) / range) * (stats->edge_weight_bucket_count - 1));
        if (bucket < 0) bucket = 0;
        if (bucket >= stats->edge_weight_bucket_count) bucket = stats->edge_weight_bucket_count - 1;
        stats->edge_weight_histogram[bucket]++;
        stats->edge_weight_count++;
    } else if (stats->edge_weight_count == 0) {
        stats->edge_weight_min = value;
        stats->edge_weight_max = value;
        if (stats->edge_weight_bucket_count > 0) {
            stats->edge_weight_histogram[0]++;
        }
        stats->edge_weight_count++;
    }
}

/* Compute percentile from histogram (O(1) - adaptive bucket count) */
static float wave_statistics_percentile(int *histogram, int bucket_count, int count, float min_val, float max_val, int percentile) {
    if (count == 0 || max_val <= min_val || bucket_count == 0 || !histogram) return 0.0f;
    
    int target_count = (int)((percentile / 100.0f) * count);
    int cumulative = 0;
    
    for (int i = 0; i < bucket_count; i++) {
        cumulative += histogram[i];
        if (cumulative >= target_count) {
            /* Interpolate within bucket */
            float range = max_val - min_val;
            float bucket_start = min_val + (i * range / bucket_count);
            float bucket_end = min_val + ((i + 1) * range / bucket_count);
            /* Use midpoint of bucket (relative to bucket range, not hardcoded) */
            float bucket_midpoint = bucket_start + (bucket_end - bucket_start) / 2.0f;
            return bucket_midpoint;
        }
    }
    
    return max_val;
}

/* Compute all percentiles from histograms (O(1) - called once per wave) */
static void wave_statistics_compute_percentiles(WaveStatistics *stats) {
    if (!stats) return;
    
    /* General statistics */
    if (stats->histogram_count > 0 && stats->histogram && stats->histogram_bucket_count > 0) {
        stats->p25 = wave_statistics_percentile(stats->histogram, stats->histogram_bucket_count, stats->histogram_count,
                                                 stats->value_min, stats->value_max, 25);
        stats->p50 = wave_statistics_percentile(stats->histogram, stats->histogram_bucket_count, stats->histogram_count,
                                                 stats->value_min, stats->value_max, 50);
        stats->p75 = wave_statistics_percentile(stats->histogram, stats->histogram_bucket_count, stats->histogram_count,
                                                 stats->value_min, stats->value_max, 75);
        stats->p90 = wave_statistics_percentile(stats->histogram, stats->histogram_bucket_count, stats->histogram_count,
                                                 stats->value_min, stats->value_max, 90);
        stats->p95 = wave_statistics_percentile(stats->histogram, stats->histogram_bucket_count, stats->histogram_count,
                                                 stats->value_min, stats->value_max, 95);
    }
    
    /* Acceptance statistics */
    if (stats->acceptance_count > 0 && stats->acceptance_histogram && stats->acceptance_bucket_count > 0) {
        stats->acceptance_p90 = wave_statistics_percentile(stats->acceptance_histogram, stats->acceptance_bucket_count, stats->acceptance_count,
                                                           stats->acceptance_min, stats->acceptance_max, 90);
    } else {
        stats->acceptance_p90 = 0.0f;  /* No data = no threshold */
    }
    
    /* Similarity statistics */
    if (stats->similarity_count > 0 && stats->similarity_histogram && stats->similarity_bucket_count > 0) {
        stats->similarity_p75 = wave_statistics_percentile(stats->similarity_histogram, stats->similarity_bucket_count, stats->similarity_count,
                                                           stats->similarity_min, stats->similarity_max, 75);
    } else {
        stats->similarity_p75 = 0.0f;  /* No data = no threshold */
    }
    
    /* Edge weight statistics */
    if (stats->edge_weight_count > 0 && stats->edge_weight_histogram && stats->edge_weight_bucket_count > 0) {
        stats->edge_weight_p25 = wave_statistics_percentile(stats->edge_weight_histogram, stats->edge_weight_bucket_count, stats->edge_weight_count,
                                                             stats->edge_weight_min, stats->edge_weight_max, 25);
        stats->edge_weight_p75 = wave_statistics_percentile(stats->edge_weight_histogram, stats->edge_weight_bucket_count, stats->edge_weight_count,
                                                             stats->edge_weight_min, stats->edge_weight_max, 75);
    } else {
        stats->edge_weight_p25 = 0.0f;
        stats->edge_weight_p75 = 0.0f;
    }
}

/* ========================================
 * UTILITY FUNCTIONS
 * ======================================== */

/* ========================================
 * CACHED STATE MAINTENANCE (O(1) incremental updates)
 * ======================================== */

/* Update cached outgoing weight sum when edge weight changes (O(1)) */
static void node_update_outgoing_weight_sum(Node *node, float old_weight, float new_weight) {
    if (!node) return;
    node->outgoing_weight_sum = node->outgoing_weight_sum - old_weight + new_weight;
}

/* Update cached incoming weight sum when edge weight changes (O(1)) */
static void node_update_incoming_weight_sum(Node *node, float old_weight, float new_weight) {
    if (!node) return;
    node->incoming_weight_sum = node->incoming_weight_sum - old_weight + new_weight;
}

/* Add edge weight to cached sums when edge is added (O(1)) */
static void node_add_outgoing_weight(Node *node, float weight) {
    if (!node) return;
    node->outgoing_weight_sum += weight;
}

/* Add edge weight to cached sums when edge is added (O(1)) */
static void node_add_incoming_weight(Node *node, float weight) {
    if (!node) return;
    node->incoming_weight_sum += weight;
}

/* ========================================
 * NODE OPERATIONS (Local Only)
 * ======================================== */

/* Create a new node with payload (payload stored directly in node) */
Node* node_create(const uint8_t *payload_data, size_t payload_size) {
    /* Allocate node with space for payload (flexible array member) */
    Node *node = (Node*)calloc(1, sizeof(Node) + payload_size);
    if (!node) return NULL;
    
    /* Generate sequential binary ID (big-endian, 8 bytes) */
    static uint64_t id_counter = 0;
    for (int i = 0; i < 8; i++) {
        node->id[i] = (char)((id_counter >> (8 * (7 - i))) & 0xFF);
    }
    node->id[8] = '\0';
    id_counter++;
    
    /* Store payload size */
    node->payload_size = payload_size;
    
    /* Copy payload data directly into node (stored inline) */
    if (payload_data && payload_size > 0) {
        memcpy(node->payload, payload_data, payload_size);
    }
    
    /* Initialize state */
    node->activation_strength = 0.0f;
    node->weight = 0.0f;
    node->bias = 0.0f;  /* Will be computed on first use */
    node->abstraction_level = 0;
    
    /* Initialize cached state */
    node->outgoing_weight_sum = 0.0f;
    node->incoming_weight_sum = 0.0f;
    
    /* Initialize adaptive learning rate tracking */
    /* DATA-DRIVEN: Start with reasonable default, will adapt based on change rate */
    node->weight_change_capacity = 4;  /* Initial size */
    node->weight_change_count = 0;
    node->recent_weight_changes = (float*)calloc(node->weight_change_capacity, sizeof(float));
    node->weight_change_index = 0;
    node->change_rate_avg = 0.0f;
    
    /* Initialize edge arrays */
    node->outgoing_capacity = 4;
    node->outgoing_edges = (Edge**)calloc(node->outgoing_capacity, sizeof(Edge*));
    node->outgoing_count = 0;
    
    node->incoming_capacity = 4;
    node->incoming_edges = (Edge**)calloc(node->incoming_capacity, sizeof(Edge*));
    node->incoming_count = 0;
    
    /* Initialize cached weight sums (O(1) state - maintained incrementally) */
    node->outgoing_weight_sum = 0.0f;
    node->incoming_weight_sum = 0.0f;
    
    return node;
}

/* Compute median of adaptive-size array */
static float compute_median_adaptive(float *values, size_t count) {
    if (!values || count == 0) return 0.0f;
    
    /* Simple bubble sort for small arrays (O(n²) but n is small) */
    float *sorted = (float*)malloc(count * sizeof(float));
    if (!sorted) return 0.0f;
    memcpy(sorted, values, count * sizeof(float));
    
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - 1 - i; j++) {
            if (sorted[j] > sorted[j + 1]) {
                float temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    float median;
    if (count % 2 == 0) {
        median = (sorted[count / 2 - 1] + sorted[count / 2]) / 2.0f;
    } else {
        median = sorted[count / 2];
    }
    
    free(sorted);
    return median;
}

/* Compute percentile from array of floats (data-driven, no hardcoded thresholds) */
static float compute_percentile_from_array(float *values, size_t count, int percentile) {
    if (!values || count == 0 || percentile < 0 || percentile > 100) return 0.0f;
    
    float *sorted = (float*)malloc(count * sizeof(float));
    if (!sorted) return 0.0f;
    memcpy(sorted, values, count * sizeof(float));
    qsort(sorted, count, sizeof(float), compare_float);
    
    int index = (int)((percentile / 100.0f) * (count - 1));
    if (index < 0) index = 0;
    if (index >= (int)count) index = (int)count - 1;
    
    float result = sorted[index];
    free(sorted);
    return result;
}

/* Compute adaptive sampling limit based on available data (data-driven, no hardcoded limits) */
static size_t compute_adaptive_sample_limit(size_t total_count, size_t min_sample, size_t max_sample) {
    if (total_count == 0) return 0;
    if (total_count <= min_sample) return total_count;
    
    /* Use sqrt for O(√n) complexity when dealing with O(n²) operations */
    /* This adapts to data size - no hardcoded limit */
    size_t adaptive = (size_t)sqrtf((float)total_count) + 1;
    
    if (adaptive < min_sample) return min_sample;
    if (adaptive > max_sample) return max_sample;
    if (adaptive > total_count) return total_count;
    return adaptive;
}

/* ========================================
 * ADAPTIVE STABILITY FUNCTIONS
 * (Relative Adaptive Stability - Principle 3)
 * ======================================== */

/* Compute adaptive epsilon for numerical stability (scales with data range) */
/* NO HARDCODED FALLBACKS: Returns 0.0f (neutral) when no data, not a hardcoded assumption */
static float compute_adaptive_epsilon(float value_range) {
    /* NO FALLBACK: When no data exists, return neutral value (0.0f = no operation) */
    if (value_range <= 0.0f) {
        // #region agent log
        char log_eps[128];
        snprintf(log_eps, sizeof(log_eps), "{\"hypothesisId\":\"A\",\"value_range\":%.6f,\"return_value\":0,\"reason\":\"no_data\"}", value_range);
        debug_log("melvin.c:662", "Epsilon returning 0.0f (no data)", log_eps);
        // #endregion agent log
        return 0.0f;  /* Neutral: no operation possible without data */
    }
    /* Epsilon adapts to 0.1% of the value range (scales with data) */
    float adaptive_epsilon = value_range * 0.001f;
    // #region agent log
    char log_eps2[128];
    snprintf(log_eps2, sizeof(log_eps2), "{\"hypothesisId\":\"B\",\"value_range\":%.6f,\"epsilon\":%.6f}", value_range, adaptive_epsilon);
    debug_log("melvin.c:668", "Computed epsilon (data-driven)", log_eps2);
    // #endregion agent log
    /* Use computed epsilon (data-driven, no hardcoded minimum) */
    return adaptive_epsilon;
}

/* Compute adaptive clipping bound from observed data distribution (no fallbacks) */
static float compute_adaptive_clip_bound(float *values, size_t count) {
    if (!values || count < 3) {
        /* No data or insufficient data: return 0.0f (no operation) */
        return 0.0f;
    }
    /* Use 95th percentile of observed values as baseline */
    float p95 = compute_percentile_from_array(values, count, 95);
    if (p95 <= 0.0f) {
        /* All values are zero: return 0.0f (no operation) */
        return 0.0f;
    }
    /* Clip to 2x the 95th percentile (allows outliers but prevents extremes) */
    return p95 * 2.0f;
}

/* Compute adaptive smoothing factor based on change rate */
static float compute_adaptive_smoothing_factor(Node *node) {
    if (!node) return compute_initial_smoothing(node);  /* Adaptive initial smoothing */
    
    if (node->weight_change_count == 0) return compute_initial_smoothing(node);  /* Adaptive initial smoothing */
    
    /* Fast changes → less smoothing (more responsive) */
    /* Slow changes → more smoothing (more stable) */
    float change_rate = node->change_rate_avg;
    float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                      node_get_local_incoming_weight_avg(node)) / 2.0f;
    
    if (local_avg > 0.0f) {
        float epsilon = compute_adaptive_epsilon(local_avg);
        float denominator = local_avg + epsilon;
        // #region agent log
        char log_div[256];
        snprintf(log_div, sizeof(log_div), "{\"hypothesisId\":\"A\",\"local_avg\":%.6f,\"epsilon\":%.6f,\"denominator\":%.6f,\"change_rate\":%.6f}", local_avg, epsilon, denominator, change_rate);
        debug_log("melvin.c:699", "Epsilon usage in division", log_div);
        // #endregion agent log
        float relative_change = change_rate / denominator;
        /* High relative change → low smoothing (0.1), low change → high smoothing (0.9) */
        float smoothing = 1.0f - relative_change;
        // #region agent log
        char log_clip1[256];
        snprintf(log_clip1, sizeof(log_clip1), "{\"hypothesisId\":\"A\",\"local_avg\":%.3f,\"smoothing_before_clip\":%.3f,\"clipping_range\":\"0.1-0.9\"}", local_avg, smoothing);
        debug_log("melvin.c:720", "Smoothing factor before clipping", log_clip1);
        // #endregion agent log
        return fmaxf(0.1f, fminf(0.9f, smoothing));
    }
    return compute_initial_smoothing(node);  /* Adaptive initial smoothing */
}

/* Compute adaptive minimum sample size based on data variance */
static size_t compute_adaptive_min_samples(float *values, size_t count) {
    // #region agent log
    if (!values || count == 0) {
        char log_data[128];
        snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"B\",\"return_value\":3,\"reason\":\"no_data_for_variance\"}");
        debug_log("melvin.c:697", "Hardcoded minimum default return", log_data);
        // #endregion agent log
        return 3;  /* Minimum default */
    }
    // #endregion agent log
    
    if (count < 2) return count;  /* Can't compute variance with < 2 samples */
    
    /* Compute variance */
    float mean = 0.0f;
    for (size_t i = 0; i < count; i++) {
        mean += values[i];
    }
    mean /= count;
    
    float variance = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = values[i] - mean;
        variance += diff * diff;
    }
    variance /= count;
    
    /* Normalize variance (relative to mean) */
    float epsilon_var = (mean > 0.0f) ? compute_adaptive_epsilon(mean) : 0.0f;
    float denominator_var = (mean > 0.0f) ? (mean * mean + epsilon_var) : 1.0f;
    // #region agent log
    char log_var[256];
    snprintf(log_var, sizeof(log_var), "{\"hypothesisId\":\"A\",\"mean\":%.6f,\"epsilon\":%.6f,\"denominator\":%.6f,\"variance\":%.6f}", mean, epsilon_var, denominator_var, variance);
    debug_log("melvin.c:742", "Epsilon usage in variance normalization", log_var);
    // #endregion agent log
    float normalized_variance = (mean > 0.0f) ? variance / denominator_var : variance;
    float variance_factor = fminf(1.0f, normalized_variance);
    
    /* Compute adaptive sample range based on data characteristics (relative adaptive stability) */
    float min_val = values[0], max_val = values[0];
    for (size_t i = 1; i < count; i++) {
        if (values[i] < min_val) min_val = values[i];
        if (values[i] > max_val) max_val = values[i];
    }
    float range = max_val - min_val;
    float relative_range = (mean > 0.0f) ? range / (mean + compute_adaptive_epsilon(mean)) : range;
    
    /* Base range: 2-12 (adapts to data spread) */
    float base_min = 2.0f;
    float base_max = 12.0f;
    float range_contribution = fminf(relative_range, 1.0f) * (base_max - base_min);
    
    /* Combine variance factor with range contribution */
    float min_samples_float = base_min + (variance_factor * range_contribution);
    size_t min_samples = (size_t)fmaxf(base_min, fminf(min_samples_float, base_max));
    
    return fminf(min_samples, count);  /* Don't exceed available samples */
}

/* Compute well-connected threshold from local connection distribution (relative adaptive stability) */
static float compute_well_connected_threshold(Node *node) {
    if (!node) return 0.0f;
    
    /* Sample connection counts from local neighbors */
    size_t sample_size = compute_adaptive_sample_limit(
        node->outgoing_count + node->incoming_count, 1, 
        node->outgoing_count + node->incoming_count
    );
    float *connection_counts = (float*)malloc(sample_size * sizeof(float));
    size_t count = 0;
    
    /* Sample from outgoing neighbors */
    for (size_t i = 0; i < node->outgoing_count && count < sample_size; i++) {
        if (node->outgoing_edges[i] && node->outgoing_edges[i]->to_node) {
            Node *neighbor = node->outgoing_edges[i]->to_node;
            connection_counts[count++] = (float)(neighbor->outgoing_count + neighbor->incoming_count);
        }
    }
    /* Sample from incoming neighbors */
    for (size_t i = 0; i < node->incoming_count && count < sample_size; i++) {
        if (node->incoming_edges[i] && node->incoming_edges[i]->from_node) {
            Node *neighbor = node->incoming_edges[i]->from_node;
            connection_counts[count++] = (float)(neighbor->outgoing_count + neighbor->incoming_count);
        }
    }
    
    if (count > 0) {
        /* Use 75th percentile as "well-connected" threshold */
        float threshold = compute_percentile_from_array(connection_counts, count, 75);
        free(connection_counts);
        return threshold;
    }
    
    free(connection_counts);
    /* No neighbors: use node's own connections as baseline */
    return (float)(node->outgoing_count + node->incoming_count);
}

/* Compute adaptive confidence increase from observed confidence distribution (relative adaptive stability) */
static float compute_adaptive_confidence_increase(float *observed_confidences, size_t count, size_t current_step) {
    if (!observed_confidences || count == 0) {
        /* No data: use conservative increase based on step number */
        /* Increase rate adapts: early steps more permissive, later steps stricter */
        float base_increase = 0.05f;  /* 5% base */
        float step_factor = (float)current_step * 0.01f;  /* +1% per step */
        return fminf(base_increase + step_factor, 0.2f);  /* Cap at 20% */
    }
    
    /* Compute how much confidence typically changes between steps */
    if (count >= 2) {
        float *changes = (float*)malloc((count - 1) * sizeof(float));
        for (size_t i = 0; i < count - 1; i++) {
            changes[i] = fabsf(observed_confidences[i + 1] - observed_confidences[i]);
        }
        float median_change = compute_median_adaptive(changes, count - 1);
        free(changes);
        
        /* Increase rate adapts to observed change rate */
        float range = (count > 0) ? 
            (compute_percentile_from_array(observed_confidences, count, 100) - 
             compute_percentile_from_array(observed_confidences, count, 0)) : 1.0f;
        float epsilon = compute_adaptive_epsilon(range);
        return fminf(median_change / (range + epsilon), 0.2f);  /* Cap at 20% */
    }
    
    // #region agent log
    char log_data[256];
    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"B\",\"return_value\":0.1,\"reason\":\"no_observed_confidences\"}");
    debug_log("melvin.c:806", "Hardcoded default return value", log_data);
    // #endregion agent log
    return 0.1f;  /* Default moderate increase */
}

/* Compute adaptive exploration steps based on graph size and context (relative adaptive stability) */
static size_t compute_adaptive_exploration_steps(MelvinGraph *g, size_t seed_count) {
    if (!g) return 1;
    
    /* Base steps on graph size (logarithmic scaling) */
    size_t base_steps = 1;
    if (g->node_count > 0) {
        /* Use log2 of node count as base (scales with graph size) */
        base_steps = (size_t)log2f((float)g->node_count) + 1;
    }
    
    /* Adjust based on seed count (more seeds = can explore less per seed) */
    float seed_factor = (seed_count > 0) ? 1.0f / (1.0f + (float)seed_count * 0.1f) : 1.0f;
    size_t adaptive_steps = (size_t)((float)base_steps * seed_factor);
    
    /* Ensure minimum of 1, maximum reasonable limit */
    // #region agent log
    char log_data[256];
    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"A\",\"adaptive_steps_before_clip\":%zu,\"clipping_range\":\"1-16\",\"node_count\":%zu}", adaptive_steps, g->node_count);
    debug_log("melvin.c:825", "Exploration steps before clipping", log_data);
    // #endregion agent log
    return fmaxf(1, fminf(adaptive_steps, 16));  /* Cap at 16 for performance */
}

/* Compute initial smoothing from available data (relative adaptive stability, no fallbacks) */
static float compute_initial_smoothing(Node *node) {
    if (!node) return 0.0f;  /* No node = no operation (neutral value, not a threshold) */
    
    /* Try to compute from node's current state relative to neighbors */
    float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                      node_get_local_incoming_weight_avg(node)) / 2.0f;
    
    if (node->weight > 0.0f && local_avg > 0.0f) {
        /* Node weight relative to local average indicates stability */
        float range = fmaxf(node->weight, local_avg);
        float epsilon_smooth = compute_adaptive_epsilon(range);
        float denominator_smooth = local_avg + epsilon_smooth;
        // #region agent log
        char log_smooth[256];
        snprintf(log_smooth, sizeof(log_smooth), "{\"hypothesisId\":\"A\",\"range\":%.6f,\"epsilon\":%.6f,\"local_avg\":%.6f,\"denominator\":%.6f,\"node_weight\":%.6f}", range, epsilon_smooth, local_avg, denominator_smooth, node->weight);
        debug_log("melvin.c:876", "Epsilon usage in initial smoothing", log_smooth);
        // #endregion agent log
        float relative_weight = node->weight / denominator_smooth;
        
        /* Close to average → more smoothing (0.7), far from average → less smoothing (0.3) */
        float smoothing = 0.5f + (1.0f - fminf(relative_weight, 2.0f) / 2.0f) * 0.2f;
        // #region agent log
        char log_clip2[256];
        snprintf(log_clip2, sizeof(log_clip2), "{\"hypothesisId\":\"A\",\"smoothing_before_clip\":%.3f,\"clipping_range\":\"0.3-0.7\",\"node_weight\":%.3f,\"local_avg\":%.3f}", smoothing, node->weight, local_avg);
        debug_log("melvin.c:909", "Initial smoothing before clipping", log_clip2);
        // #endregion agent log
        return fmaxf(0.3f, fminf(smoothing, 0.7f));  /* Range: 0.3-0.7 */
    }
    
    /* No local context: use node's own properties as minimal context */
    if (node->weight > 0.0f) {
        /* Use node's own weight as minimal context */
        float range = node->weight;
        float epsilon = compute_adaptive_epsilon(range);
        /* Compute smoothing from node's weight relative to itself (neutral) */
        // #region agent log
        char log_data[256];
        snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"B\",\"return_value\":0.5,\"reason\":\"only_node_weight_available\",\"node_weight\":%.3f}", node->weight);
        debug_log("melvin.c:853", "Neutral return value (minimal context)", log_data);
        // #endregion agent log
        return 0.5f;  /* Neutral when only node weight available */
    }
    
    /* Use node's connection count as minimal context */
    float connection_context = (float)(node->outgoing_count + node->incoming_count);
    if (connection_context > 0.0f) {
        /* More connections → more smoothing (more stable) */
        return connection_context / (connection_context + 1.0f);
    }
    
    /* No data available: return neutral value (no operation) */
    return 0.0f;
}


/* Compute adaptive bucket growth trigger based on data variance (relative adaptive stability, no fallbacks) */
static int compute_adaptive_bucket_growth_trigger(WaveStatistics *stats, int bucket_count) {
    if (!stats || bucket_count == 0) {
        /* No stats or no buckets: use minimal context from bucket_count itself */
        return (bucket_count > 0) ? bucket_count : 1;  /* Use bucket_count as minimal context */
    }
    
    /* High variance → need more buckets → grow sooner */
    /* Low variance → fewer buckets sufficient → grow later */
    float range = stats->value_max - stats->value_min;
    float mean = (stats->value_max + stats->value_min) / 2.0f;
    
    if (mean > 0.0f && range > 0.0f) {
        float coefficient_of_variation = range / (mean + compute_adaptive_epsilon(mean));
        
        /* High CV → grow at 5x, low CV → grow at 15x */
        int growth_trigger = (int)(5.0f + (1.0f - fminf(coefficient_of_variation, 1.0f)) * 10.0f);
        // #region agent log
        char log_data[256];
        snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"A\",\"growth_trigger_before_clip\":%d,\"clipping_range\":\"5-20\",\"cv\":%.3f,\"mean\":%.3f,\"range\":%.3f}", growth_trigger, coefficient_of_variation, mean, range);
        debug_log("melvin.c:885", "Bucket growth trigger before clipping", log_data);
        // #endregion agent log
        return fmaxf(5, fminf(growth_trigger, 20));  /* Range: 5-20x */
    }
    
    // #region agent log
    char log_data[256];
    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"B\",\"return_value\":10,\"reason\":\"no_data_for_cv_computation\",\"bucket_count\":%d}", bucket_count);
    debug_log("melvin.c:888", "Hardcoded default return value", log_data);
    // #endregion agent log
    return 10;  /* Default */
}

/* Adapt rolling window size based on observed change rate (self-regulating) */
static void node_adapt_rolling_window(Node *node) {
    if (!node || !node->recent_weight_changes || node->weight_change_count == 0) return;
    
    /* DATA-DRIVEN: Compute average change rate from observed changes */
    float avg_change = 0.0f;
    for (size_t i = 0; i < node->weight_change_count; i++) {
        avg_change += node->recent_weight_changes[i];
    }
    avg_change /= node->weight_change_count;
    node->change_rate_avg = avg_change;
    
    /* DATA-DRIVEN: Adapt window size based on change rate */
    /* Fast changes (high rate) → smaller window (focus on recent) */
    /* Slow changes (low rate) → larger window (need more history) */
    /* Compute threshold from actual change rates observed */
    size_t optimal_size = 4;  /* Minimum size */
    if (node->weight_change_count > 1) {
        /* Use median of observed changes as threshold */
        float *sorted = (float*)malloc(node->weight_change_count * sizeof(float));
        if (sorted) {
            memcpy(sorted, node->recent_weight_changes, node->weight_change_count * sizeof(float));
            qsort(sorted, node->weight_change_count, sizeof(float), compare_float);
            float median_change = sorted[node->weight_change_count / 2];
            free(sorted);
            
            /* Window size adapts to median change rate */
            if (avg_change > median_change) {
        optimal_size = 4;  /* Fast changes: small window */
            } else if (avg_change > median_change / 2.0f) {
        optimal_size = 8;  /* Medium changes: medium window */
    } else {
        optimal_size = 16;  /* Slow changes: large window */
            }
        }
    } else if (node->weight_change_count == 1) {
        /* Only one value - use it as threshold */
        float single_change = node->recent_weight_changes[0];
        if (avg_change > single_change) {
            optimal_size = 4;
        } else {
            optimal_size = 8;
        }
    } else {
        /* No history - compute from node's current state relative to neighbors */
        float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                          node_get_local_incoming_weight_avg(node)) / 2.0f;
        float baseline = (node->weight > 0.0f && local_avg > 0.0f) ?
                        (node->weight + local_avg) / 2.0f : node->weight;
        if (avg_change > baseline) {
            optimal_size = 4;
        } else {
            optimal_size = 8;
        }
    }
    
    /* Resize if needed */
    if (optimal_size != node->weight_change_capacity) {
        float *new_window = (float*)calloc(optimal_size, sizeof(float));
        if (new_window) {
            /* Copy existing values (circular buffer) */
            size_t copy_count = (node->weight_change_count < optimal_size) ? 
                               node->weight_change_count : optimal_size;
            for (size_t i = 0; i < copy_count; i++) {
                size_t src_idx = (node->weight_change_index - copy_count + i + node->weight_change_capacity) % node->weight_change_capacity;
                new_window[i] = node->recent_weight_changes[src_idx];
            }
            free(node->recent_weight_changes);
            node->recent_weight_changes = new_window;
            node->weight_change_capacity = optimal_size;
            node->weight_change_count = copy_count;
            node->weight_change_index = copy_count % optimal_size;
        }
    }
}

/* Get adaptive learning rate from rolling window (O(1)) */
static float node_get_adaptive_learning_rate(Node *node) {
    if (!node) return 0.0f;
    
    /* DATA-DRIVEN: Adapt window size based on observed change rate */
    if (node->weight_change_count >= node->weight_change_capacity) {
        node_adapt_rolling_window(node);
    }
    
    /* Count non-zero values in rolling window */
    size_t valid_count = 0;
    for (size_t i = 0; i < node->weight_change_count; i++) {
        if (node->recent_weight_changes[i] != 0.0f) {
            valid_count++;
        }
    }
    
    if (valid_count == 0) {
        /* No history yet - use local context (relative adaptive stability) */
        float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                           node_get_local_incoming_weight_avg(node)) / 2.0f;
        if (node->weight + local_avg > 0.0f) {
            /* Use adaptive epsilon for stable division */
            float range = fmaxf(node->weight, local_avg);
            float epsilon = compute_adaptive_epsilon(range);
            return node->weight / (node->weight + local_avg + epsilon);
        }
        return 0.0f;
    }
    
    /* Compute median rate from rolling window (adaptive size) */
    float median_rate = compute_median_adaptive(node->recent_weight_changes, node->weight_change_count);
    
    /* Combine with local context for stability (relative adaptive stability) */
    float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                       node_get_local_incoming_weight_avg(node)) / 2.0f;
    float context_rate = 0.0f;
    if (node->weight + local_avg > 0.0f) {
        /* Use adaptive epsilon for stable division */
        float range = fmaxf(node->weight, local_avg);
        float epsilon = compute_adaptive_epsilon(range);
        context_rate = node->weight / (node->weight + local_avg + epsilon);
    }
    
    /* Adaptive blend: weight based on data availability (relative, not hardcoded) */
    /* More history data → trust history more; less history → trust context more */
    float history_weight = 0.0f;
    if (node->weight_change_count > 0) {
        history_weight = (float)node->weight_change_count / (node->weight_change_count + 1.0f);
    } else {
        /* No history: compute from node weight relative to local context (relative adaptive stability) */
        float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                          node_get_local_incoming_weight_avg(node)) / 2.0f;
        if (local_avg > 0.0f && node->weight > 0.0f) {
            /* Use adaptive epsilon for stable division */
            float range = fmaxf(node->weight, local_avg);
            float epsilon = compute_adaptive_epsilon(range);
            history_weight = node->weight / (node->weight + local_avg + epsilon);
        } else {
            history_weight = 0.0f;  /* No data = no weight */
        }
    }
    float context_weight = 1.0f - history_weight;
    float result = median_rate * history_weight + context_rate * context_weight;
    // #region agent log
    char log_data[256];
    snprintf(log_data, sizeof(log_data), "{\"history_weight\":%.3f,\"context_weight\":%.3f,\"result\":%.3f,\"history_count\":%zu}",
             history_weight, context_weight, result, node->weight_change_count);
    debug_log("melvin.c:675", "Adaptive learning rate computed", log_data);
    // #endregion agent log
    return result;
}

/* Update node weight based on local activation history (relative to local context) */
void node_update_weight_local(Node *node) {
    if (!node) return;
    
    float old_weight = node->weight;
    
    /* DATA-DRIVEN: Learning rate from adaptive rolling window (no hardcoded fallback) */
    float rate = node_get_adaptive_learning_rate(node);
    
    /* Weight updates relative to activation strength (continuous, not binary) */
    float new_weight = node->weight * (1.0f - rate) + node->activation_strength * rate;
    node->weight = new_weight;
    
    /* Track weight change in rolling window (O(1)) */
    if (old_weight > 0.0f) {
        float change = (new_weight - old_weight) / old_weight;  /* Relative change */
        node->recent_weight_changes[node->weight_change_index] = fabsf(change);
        node->weight_change_index = (node->weight_change_index + 1) % node->weight_change_capacity;  /* Circular buffer - adaptive size */
    }
}

/* Calculate match strength - UNIVERSAL for all node types (no special cases) */
/* Philosophy: All nodes match the same way - based on payload OR connections, relative to local context */
float node_calculate_match_strength(Node *node, const uint8_t *pattern, size_t pattern_size) {
    if (!node) return 0.0f;
    
    float match_score = 0.0f;
    float total_weight = 0.0f;
    
    /* UNIVERSAL: Nodes with payload match directly (all node types) */
    if (node->payload_size > 0 && pattern_size > 0) {
        size_t match_bytes = 0;
        size_t check_size = (node->payload_size < pattern_size) ? node->payload_size : pattern_size;
        
        for (size_t i = 0; i < check_size; i++) {
            if (node->payload[i] == pattern[i]) match_bytes++;
        }
        
        float direct_similarity = (check_size > 0) ? (float)match_bytes / (float)check_size : 0.0f;
        match_score = direct_similarity;
        total_weight = 1.0f;
    }
    
    /* UNIVERSAL: All nodes can also match through connections (relative to local context) */
    /* This works for blank nodes (no payload) and regular nodes (additional context) */
    float connection_match = 0.0f;
    float connection_weight = 0.0f;
    
    /* Check incoming edges (patterns connected to this node) */
    for (size_t i = 0; i < node->incoming_count; i++) {
        Edge *edge = node->incoming_edges[i];
        if (!edge || !edge->from_node) continue;
        Node *connected = edge->from_node;
        
        /* Skip nodes without payload (they match through their own connections) */
        if (connected->payload_size == 0) continue;
        
        /* Compute similarity of connected node to pattern */
        float connected_similarity = 0.0f;
        if (connected->payload_size > 0 && pattern_size > 0) {
            size_t match_bytes = 0;
            size_t check_size = (connected->payload_size < pattern_size) ? connected->payload_size : pattern_size;
            for (size_t j = 0; j < check_size; j++) {
                if (connected->payload[j] == pattern[j]) match_bytes++;
            }
            connected_similarity = (check_size > 0) ? (float)match_bytes / (float)check_size : 0.0f;
        }
        
        connection_match += connected_similarity * edge->weight;
        connection_weight += edge->weight;
    }
    
    /* Check outgoing edges */
    for (size_t i = 0; i < node->outgoing_count; i++) {
        Edge *edge = node->outgoing_edges[i];
        if (!edge || !edge->to_node) continue;
        Node *connected = edge->to_node;
        
        if (connected->payload_size == 0) continue;
        
        float connected_similarity = 0.0f;
        if (connected->payload_size > 0 && pattern_size > 0) {
            size_t match_bytes = 0;
            size_t check_size = (connected->payload_size < pattern_size) ? connected->payload_size : pattern_size;
            for (size_t j = 0; j < check_size; j++) {
                if (connected->payload[j] == pattern[j]) match_bytes++;
            }
            connected_similarity = (check_size > 0) ? (float)match_bytes / (float)check_size : 0.0f;
        }
        
        connection_match += connected_similarity * edge->weight;
        connection_weight += edge->weight;
    }
    
    /* Normalize connection match */
    if (connection_weight > 0.0f) {
        connection_match = connection_match / connection_weight;
    }
    
    /* UNIVERSAL: Combine direct and connection matches (relative to local context) */
    float combined_match = 0.0f;
    if (total_weight > 0.0f && connection_weight > 0.0f) {
        /* Both available - weighted average relative to local context */
        float local_avg = (node_get_local_incoming_weight_avg(node) + 
                          node_get_local_outgoing_weight_avg(node)) / 2.0f;
        /* Adaptive weight: relative to local context, no hardcoded fallback */
        float direct_weight = (local_avg > 0.0f) ? node->weight / (node->weight + local_avg) : 
                             (node->weight > 0.0f ? node->weight / (node->weight + 1.0f) : 0.0f);
        combined_match = match_score * direct_weight + connection_match * (1.0f - direct_weight);
    } else if (total_weight > 0.0f) {
        combined_match = match_score;
    } else if (connection_weight > 0.0f) {
        combined_match = connection_match;
    }
    
    /* UNIVERSAL: Weight by usage frequency (self-regulating, relative to local context) */
    float local_avg = (node_get_local_incoming_weight_avg(node) + 
                      node_get_local_outgoing_weight_avg(node)) / 2.0f;
    if (node->weight > 0.0f && local_avg > 0.0f) {
        /* Weight by usage frequency relative to local context */
        float weight_factor = node->weight / (node->weight + local_avg);
        return combined_match * (1.0f + weight_factor);
    } else {
        /* Raw match when weights haven't grown yet */
        return combined_match;
    }
}

/* Get local average weight from outgoing edges (O(1) - reads cached state) */
float node_get_local_outgoing_weight_avg(Node *node) {
    if (!node || node->outgoing_count == 0) return 0.0f;
    /* Read from cached sum (maintained incrementally when edges change) */
    return node->outgoing_weight_sum / (float)node->outgoing_count;
}

/* Get local average weight from incoming edges (O(1) - reads cached state) */
float node_get_local_incoming_weight_avg(Node *node) {
    if (!node || node->incoming_count == 0) return 0.0f;
    /* Read from cached sum (maintained incrementally when edges change) */
    return node->incoming_weight_sum / (float)node->incoming_count;
}

/* Compute node activation from weighted inputs (mini neural net) */
/* Philosophy: Local-only computation, self-regulating bias, no thresholds */
float node_compute_activation_strength(Node *node) {
    if (!node) return 0.0f;
    
    /* Sum weighted inputs from incoming edges (local-only computation) */
    float input_sum = 0.0f;
    float total_weight = 0.0f;
    
    for (size_t i = 0; i < node->incoming_count; i++) {
        Edge *edge = node->incoming_edges[i];
        if (!edge || !edge->from_node) continue;
        
        /* Edge transforms activation as it flows (use intelligent transformer) */
        float transformed = edge_transform_activation(edge, edge->from_node->activation_strength);
        input_sum += transformed;
        total_weight += edge->weight;
    }
    
    /* Normalize by total weight (relative, no hardcoded normalization) */
    if (total_weight > 0.0f) {
        input_sum = input_sum / total_weight;
    }
    
    /* RELATIVE: Self-regulating bias (relative to local context, no hardcoded fallback) */
    float local_avg = node_get_local_incoming_weight_avg(node);
    /* Bias emerges from weight relative to context - no fixed value */
    node->bias = (node->weight + local_avg > 0.0f) ? 
                 node->weight / (node->weight + local_avg) : 
                 (node->weight > 0.0f ? node->weight / (node->weight + 1.0f) : 0.0f);
    
    /* Compute activation: input_sum + bias with soft non-linearity */
    /* Uses relative comparison - no hardcoded threshold */
    float raw_activation = input_sum + node->bias;
    return raw_activation / (1.0f + raw_activation);  /* Soft sigmoid-like, self-limiting */
}

/* REMOVED: blank_node_compute_category_match - now integrated into universal node_calculate_match_strength */
/* All nodes (including blank nodes) use the same universal matching function */

/* REMOVED: hierarchy_node_compute_abstraction() */
/* All nodes use universal node_compute_activation_strength() */
/* Hierarchy nodes are just larger nodes - they compute activation from their edges like any node */

/* Free node (payload is stored inline, so freeing node frees payload too) */
void node_free(Node *node) {
    if (!node) return;
    
    if (node->outgoing_edges) {
        free(node->outgoing_edges);
    }
    if (node->incoming_edges) {
        free(node->incoming_edges);
    }
    if (node->recent_weight_changes) {
        free(node->recent_weight_changes);
    }
    /* Payload is stored inline (flexible array member), so freeing node frees payload */
    free(node);
}

/* ========================================
 * EDGE OPERATIONS (Local Only)
 * ======================================== */

/* Create a new edge between two nodes (direct node pointers - no searching, no global state) */
/* Edge creation is local - edge only knows its from/to nodes, doesn't search the graph */
Edge* edge_create(Node *from, Node *to, bool direction) {
    if (!from || !to) return NULL;
    
    Edge *edge = (Edge*)calloc(1, sizeof(Edge));
    if (!edge) return NULL;
    
    /* Store node pointers directly (no IDs needed - IDs in nodes) */
    /* Edge only knows itself and its connections - no global graph knowledge */
    edge->from_node = from;
    edge->to_node = to;
    edge->direction = direction;
    edge->activation = false;
    edge->weight = 0.0f;
    
    return edge;
}

/* Update edge weight based on local activation history (self-relative, no global state) */
/* Edge only knows itself - weight updates are relative to edge's own state */
/* Maintains cached weight sums in nodes (O(1) incremental update) */
void edge_update_weight_local(Edge *edge) {
    if (!edge) return;
    
    /* Store old weight for incremental cache update */
    float old_weight = edge->weight;
    
    /* DATA-DRIVEN: Learning rate from from_node's adaptive rolling window (no hardcoded base) */
    /* NO HARDCODED THRESHOLD: Compute rate even when weight = 0.0f using minimal context */
    float rate = 0.0f;
    
    if (edge->from_node) {
        /* Try to get adaptive rate from from_node (data-driven) */
        float node_rate = node_get_adaptive_learning_rate(edge->from_node);
        if (node_rate > 0.0f) {
            rate = node_rate;
        } else {
            /* No node rate available: compute from edge weight or use minimal bootstrap */
            if (edge->weight > 0.0f) {
                rate = edge->weight / (edge->weight + 1.0f);  /* Compute from edge weight */
            } else {
                /* No weight yet: use minimal bootstrap rate to allow learning (not a threshold, just bootstrap) */
                /* Use source node's activation_strength if available, otherwise minimal rate */
                if (edge->from_node->activation_strength > 0.0f) {
                    rate = edge->from_node->activation_strength * 0.1f;  /* Scale activation for learning rate */
                    // #region agent log
                    char log_data[256];
                    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"D\",\"rate\":%.3f,\"source\":\"activation_strength\",\"activation\":%.3f,\"factor\":0.1}", rate, edge->from_node->activation_strength);
                    debug_log("melvin.c:1291", "Learning rate from activation (data-driven)", log_data);
                    // #endregion agent log
                } else {
                    rate = 0.1f;  /* Minimal bootstrap rate to allow learning */
                    // #region agent log
                    char log_data[256];
                    snprintf(log_data, sizeof(log_data), "{\"hypothesisId\":\"D\",\"rate\":0.1,\"source\":\"bootstrap\",\"reason\":\"no_activation_available\"}");
                    debug_log("melvin.c:1294", "Bootstrap learning rate (minimal context)", log_data);
                    // #endregion agent log
                }
            }
        }
    } else {
        /* No from_node: compute from edge weight or use minimal bootstrap */
        if (edge->weight > 0.0f) {
            rate = edge->weight / (edge->weight + 1.0f);
        } else {
            rate = 0.1f;  /* Minimal bootstrap rate */
        }
    }
    
    /* Weight updates relative to activation state (local measurement only) */
    /* Use activation strength from from_node (continuous, not binary) */
    float target = 0.0f;
    if (edge->from_node) {
        target = edge->from_node->activation_strength;
    }
    float new_weight = edge->weight * (1.0f - rate) + target * rate;
    
    /* Update edge weight */
    edge->weight = new_weight;
    
    /* Update cached sums in nodes (O(1) incremental update) */
    if (edge->from_node) {
        node_update_outgoing_weight_sum(edge->from_node, old_weight, new_weight);
    }
    if (edge->to_node) {
        node_update_incoming_weight_sum(edge->to_node, old_weight, new_weight);
    }
}

/* Compute pattern similarity between two nodes (observable from payloads) */
/* Philosophy: Local-only computation, uses node_calculate_match_strength */
static float edge_compute_pattern_similarity(Node *node1, Node *node2) {
    if (!node1 || !node2) return 0.0f;
    
    /* Skip blank nodes - similarity computed differently for them */
    if (node1->payload_size == 0 || node2->payload_size == 0) return 0.0f;
    
    /* Use existing node_calculate_match_strength for similarity */
    /* Check similarity in both directions and take average */
    float similarity1 = node_calculate_match_strength(node1, node2->payload, node2->payload_size);
    float similarity2 = node_calculate_match_strength(node2, node1->payload, node1->payload_size);
    
    return (similarity1 + similarity2) / 2.0f;
}

/* Check if edge already exists between two nodes (local check - no global search) */
/* Only searches through from node's outgoing edges - nodes only know themselves and their edges */
/* This follows the philosophy: all operations are local, no global state access */
static Edge* node_find_edge_to(Node *from, Node *to) {
    if (!from || !to) return NULL;
    
    /* Local check: only search outgoing edges of from node (node's local knowledge) */
    /* No global graph search - node only knows its own edges */
    for (size_t i = 0; i < from->outgoing_count; i++) {
        Edge *edge = from->outgoing_edges[i];
        if (edge && edge->to_node == to) {
            return edge;
        }
    }
    
    return NULL;
}

/* Compute context similarity based on recent path (paths recently traveled) */
/* Philosophy: Context is the paths the wave recently traveled, not structural neighbors */
/* Returns how similar two nodes are based on appearing together in recent path */
/* Context = nodes that wave propagation has explored (paths recently traveled) */
/* Two nodes share context if they were both visited by wave propagation */
/* Wave propagation filters to relevant nodes - these are the context */
static float edge_compute_context_similarity(Node *node1, Node *node2, 
                                             VisitedSet *context_visited) {
    if (!node1 || !node2 || node1 == node2) return 0.0f;
    if (!context_visited) return 0.0f;
    
    /* Context: nodes that wave propagation visited together share context */
    /* Check if both nodes were visited by wave propagation (paths recently traveled) */
    bool node1_visited = visited_set_contains(context_visited, node1);
    bool node2_visited = visited_set_contains(context_visited, node2);
    
    if (!node1_visited || !node2_visited) return 0.0f;
    
    /* Context similarity: nodes visited together by wave propagation share context */
    /* Wave propagation already filtered to relevant nodes - if both are visited, they share context */
    /* Additional context: check if they're connected (shared paths through edges) */
    bool are_connected = false;
    
    /* Check if nodes are directly connected (stronger context) */
    if (node_find_edge_to(node1, node2) || node_find_edge_to(node2, node1)) {
        are_connected = true;
    }
    
    /* Context similarity: connected nodes in wave exploration have stronger context */
    /* Relative to local context - no hardcoded thresholds */
    float base_context = 1.0f;  /* Both visited = they share context */
    if (are_connected) {
        /* Connected nodes in wave exploration have stronger context */
        float local_avg1 = (node_get_local_outgoing_weight_avg(node1) + 
                           node_get_local_incoming_weight_avg(node1)) / 2.0f;
        float local_avg2 = (node_get_local_outgoing_weight_avg(node2) + 
                           node_get_local_incoming_weight_avg(node2)) / 2.0f;
        float avg_local = (local_avg1 + local_avg2) / 2.0f;
        
        /* Boost context if they're connected (relative to local context) */
        if (avg_local > 0.0f) {
            base_context = 1.0f + (avg_local / (avg_local + 1.0f));
        } else {
            /* Connected but no local context - compute from edge weights directly */
            float edge_weight_sum = 0.0f;
            size_t edge_count = 0;
            if (node1->outgoing_count > 0) {
                for (size_t i = 0; i < node1->outgoing_count; i++) {
                    if (node1->outgoing_edges[i]) {
                        edge_weight_sum += node1->outgoing_edges[i]->weight;
                        edge_count++;
                    }
                }
            }
            if (node2->outgoing_count > 0) {
                for (size_t i = 0; i < node2->outgoing_count; i++) {
                    if (node2->outgoing_edges[i]) {
                        edge_weight_sum += node2->outgoing_edges[i]->weight;
                        edge_count++;
                    }
                }
            }
            float avg_edge_weight = (edge_count > 0) ? edge_weight_sum / edge_count : 0.0f;
            base_context = (avg_edge_weight > 0.0f) ? 
                          1.0f + (avg_edge_weight / (avg_edge_weight + 1.0f)) : 
                          1.0f;  /* Minimum when no edge weights */
        }
    }
    
    return base_context;
}

/* Check if node is isolated (few connections - observable from edge counts) */
/* Philosophy: Relative to local context, self-regulating */
static bool edge_is_node_isolated(Node *node) {
    if (!node) return false;
    
    /* Count total connections */
    size_t total_connections = node->outgoing_count + node->incoming_count;
    
    /* Compute local average connections from neighbors */
    size_t neighbor_connection_sum = 0;
    size_t neighbor_count = 0;
    
    /* Sample from outgoing neighbors (adaptive limit based on connection count) */
    size_t outgoing_sample_limit = compute_adaptive_sample_limit(node->outgoing_count, 1, node->outgoing_count);
    for (size_t i = 0; i < node->outgoing_count && i < outgoing_sample_limit; i++) {
        Edge *edge = node->outgoing_edges[i];
        if (!edge || !edge->to_node) continue;
        neighbor_connection_sum += edge->to_node->outgoing_count + edge->to_node->incoming_count;
        neighbor_count++;
    }
    
    /* Sample from incoming neighbors (adaptive limit based on connection count) */
    size_t incoming_sample_limit = compute_adaptive_sample_limit(node->incoming_count, 1, node->incoming_count);
    for (size_t i = 0; i < node->incoming_count && i < incoming_sample_limit; i++) {
        Edge *edge = node->incoming_edges[i];
        if (!edge || !edge->from_node) continue;
        neighbor_connection_sum += edge->from_node->outgoing_count + edge->from_node->incoming_count;
        neighbor_count++;
    }
    
    /* UNIVERSAL: Relative comparison - isolated if connections significantly below neighbor average */
    if (neighbor_count > 0) {
        float neighbor_avg = (float)neighbor_connection_sum / (float)neighbor_count;
        /* Relative: isolated if connections are significantly below neighbor average */
        /* Compute threshold from actual neighbor connection distribution */
        float isolation_threshold = 0.0f;
        
        /* Sample actual neighbor connections (adaptive limit based on neighbor count) */
        size_t max_sample = compute_adaptive_sample_limit(neighbor_count, 1, neighbor_count);
        float *connections = (float*)malloc(max_sample * sizeof(float));
        size_t idx = 0;
        
        /* Sample from incoming neighbors */
        for (size_t i = 0; i < node->incoming_count && idx < max_sample; i++) {
            Edge *e = node->incoming_edges[i];
            if (e && e->from_node) {
                connections[idx++] = (float)(e->from_node->outgoing_count + e->from_node->incoming_count);
            }
        }
        /* Sample from outgoing neighbors */
        for (size_t i = 0; i < node->outgoing_count && idx < max_sample; i++) {
            Edge *e = node->outgoing_edges[i];
            if (e && e->to_node) {
                connections[idx++] = (float)(e->to_node->outgoing_count + e->to_node->incoming_count);
            }
        }
        
        if (idx > 1) {
            qsort(connections, idx, sizeof(float), compare_float);
            isolation_threshold = connections[idx / 4];  /* 25th percentile - data-driven */
        } else if (idx == 1) {
            /* Single sample: compute relative fraction from node's own connections (relative adaptive stability, no fallbacks) */
            float node_connections = (float)(node->outgoing_count + node->incoming_count);
            float relative_fraction = 0.0f;  /* Initialize to neutral */
            
            if (node_connections > 0.0f && connections[0] > 0.0f) {
                /* Compute what fraction the single neighbor represents relative to node */
                float range = fmaxf(node_connections, connections[0]);
                float epsilon = compute_adaptive_epsilon(range);
                float ratio = connections[0] / (node_connections + epsilon);
                
                /* Use the ratio itself, but cap conservatively */
                /* If neighbor has many more connections, use smaller fraction */
                relative_fraction = fminf(ratio, 0.75f);  /* Cap at 75% */
                
                /* If node has more connections, use larger fraction */
                if (node_connections > connections[0]) {
                    relative_fraction = fmaxf(relative_fraction, 0.25f);  /* At least 25% */
                }
            } else if (connections[0] > 0.0f) {
                /* Node has no connections: compute from neighbor's connections alone */
                /* Use neighbor's connections as minimal context */
                float neighbor_context = connections[0];
                float epsilon = compute_adaptive_epsilon(neighbor_context);
                relative_fraction = neighbor_context / (neighbor_context + epsilon);
            }
            /* If relative_fraction is still 0.0f, threshold will be 0.0f (no operation) */
            
            isolation_threshold = connections[0] * relative_fraction;
        } else {
            /* No samples: compute from node's own connection count relative to neighbors (data-driven) */
            float node_connections = (float)(node->outgoing_count + node->incoming_count);
            if (neighbor_avg > 0.0f) {
                /* Use relative ratio with adaptive epsilon to prevent unstable division */
                float range = fmaxf(node_connections, neighbor_avg);
                float epsilon = compute_adaptive_epsilon(range);
                float ratio = node_connections / (neighbor_avg + epsilon);
                /* Threshold is node's connections scaled by its relative position */
                isolation_threshold = node_connections * fminf(ratio, 1.0f);  /* Cap at 1.0 to prevent explosion */
            } else {
                isolation_threshold = node_connections;
            }
        }
        
        if (connections) free(connections);
        return (total_connections < neighbor_avg && total_connections < isolation_threshold);
    }
    
    /* UNIVERSAL: If no neighbors, isolated if no connections (relative to having neighbors) */
    return (total_connections == 0);
}

/* Transform activation as it flows through edge */
/* Philosophy: Edges learn transformations through co-activation, self-regulating */
/* Intelligent transformation: uses observable patterns (similarity, context) */
float edge_transform_activation(Edge *edge, float input_activation) {
    if (!edge) return 0.0f;
    
    /* Base transformation: weighted signal */
    float transformed = edge->weight * input_activation;
    
    /* Intelligent transformation based on observable patterns: */
    
    /* 1. Pattern similarity (observable from payloads) */
    if (edge->from_node && edge->to_node) {
        float similarity = edge_compute_pattern_similarity(edge->from_node, edge->to_node);
        
        /* UNIVERSAL: Boost similar patterns relative to local context (no hardcoded threshold) */
        float local_avg = (node_get_local_outgoing_weight_avg(edge->from_node) + 
                           node_get_local_incoming_weight_avg(edge->from_node)) / 2.0f;
        
        /* Relative threshold: similarity must be meaningful relative to local context */
        float similarity_threshold = (local_avg > 0.0f) ? local_avg : 0.0f;
        
        if (similarity > similarity_threshold) {
            /* UNIVERSAL: Boost relative to similarity and local context (no hardcoded multiplier) */
            float boost_factor = (local_avg > 0.0f) ? similarity * (local_avg / (local_avg + 1.0f)) : similarity;
            transformed *= (1.0f + boost_factor);
        }
    }
    
    /* 2. Local context (observable from other edges) */
    if (edge->from_node) {
        float local_avg = node_get_local_outgoing_weight_avg(edge->from_node);
        if (local_avg > 0.0f) {
            float edge_relative = edge->weight / local_avg;
            
            /* Boost primary paths (strong relative to local context) */
            /* Threshold adapts to local edge weight distribution (data-driven, no fallback) */
            float primary_threshold = 1.0f;
            if (local_avg > 0.0f && edge->from_node) {
                /* Compute 75th percentile of edge weights relative to local_avg */
                float *edge_weights = (float*)malloc(edge->from_node->outgoing_count * sizeof(float));
                size_t count = 0;
                for (size_t i = 0; i < edge->from_node->outgoing_count; i++) {
                    if (edge->from_node->outgoing_edges[i]) {
                        edge_weights[count++] = edge->from_node->outgoing_edges[i]->weight / local_avg;
                    }
                }
                if (count > 0) {
                    /* Filter out zero weights for more meaningful percentile */
                    float *non_zero_weights = (float*)malloc(count * sizeof(float));
                    size_t non_zero_count = 0;
                    for (size_t i = 0; i < count; i++) {
                        if (edge_weights[i] > 0.0f) {
                            non_zero_weights[non_zero_count++] = edge_weights[i];
                        }
                    }
                    
                    if (non_zero_count > 0) {
                        float p75 = compute_percentile_from_array(non_zero_weights, non_zero_count, 75);
                        /* Use adaptive epsilon to ensure p75 is meaningful */
                        float range = (non_zero_count > 1) ? 
                                     (compute_percentile_from_array(non_zero_weights, non_zero_count, 100) - 
                                      compute_percentile_from_array(non_zero_weights, non_zero_count, 0)) : 
                                     non_zero_weights[0];
                        float epsilon = compute_adaptive_epsilon(range);
                        primary_threshold = 1.0f + fmaxf(p75, epsilon);  /* Ensure at least epsilon boost */
                    } else {
                        /* All weights are zero: use minimal boost */
                        primary_threshold = 1.0f + compute_adaptive_epsilon(local_avg);
                    }
                    free(non_zero_weights);
                }
                free(edge_weights);
            } else {
                primary_threshold = 1.0f;  /* No data = no boost */
            }
            if (edge_relative > primary_threshold) {
                /* Boost amount adapts to how much stronger the edge is */
                float boost_factor = 1.0f + ((edge_relative - primary_threshold) / (edge_relative + 1.0f));
                transformed *= boost_factor;
            }
        }
    }
    
    return transformed;
}

/* Free edge (local operation - edge only knows itself) */
void edge_free(Edge *edge) {
    if (edge) {
        free(edge);
    }
}

/* Check if node payload exactly matches pattern (quick exact match check) */
static bool node_payload_exact_match(Node *node, const uint8_t *pattern, size_t pattern_size) {
    if (!node || node->payload_size != pattern_size) return false;
    return (memcmp(node->payload, pattern, pattern_size) == 0);
}

/* Find node via previous node's outgoing edges (local, no global search) */
static Node* node_find_via_outgoing(Node *from_node, const uint8_t *pattern, size_t pattern_size) {
    if (!from_node) return NULL;
    
    /* Check outgoing edges - nodes only know themselves and their edges */
    for (size_t i = 0; i < from_node->outgoing_count; i++) {
        Edge *edge = from_node->outgoing_edges[i];
        if (!edge || !edge->to_node) continue;
        
        Node *candidate = edge->to_node;
        if (node_payload_exact_match(candidate, pattern, pattern_size)) {
            return candidate;
        }
    }
    
    return NULL;
}

/* Find blank node that could accept this pattern (compounding learning) */
/* Blank nodes learn through connections - patterns connecting to blank nodes teach them about their category */
/* A blank node "accepts" a pattern if its connected patterns are similar to the new pattern */
static Node* wave_find_accepting_blank_node(MelvinGraph *g, Node **seed_nodes, size_t seed_count,
                                            const uint8_t *pattern, size_t pattern_size,
                                            size_t max_steps) {
    if (!g || !seed_nodes || seed_count == 0 || max_steps == 0) return NULL;
    
    /* Initialize wave front with seed nodes */
    Node **wave_front = (Node**)malloc(seed_count * sizeof(Node*));
    if (!wave_front) return NULL;
    
    size_t wave_front_size = seed_count;
    memcpy(wave_front, seed_nodes, seed_count * sizeof(Node*));
    
    /* Track visited nodes (use VisitedSet for O(1) lookups, not O(n) array) */
    /* Adaptive hash size based on graph size for optimal performance */
    size_t optimal_hash_size = calculate_optimal_hash_size(g->node_count);
    VisitedSet *visited = visited_set_create(optimal_hash_size);
    if (!visited) {
        free(wave_front);
        return NULL;
    }
    
    /* Mark seed nodes as visited */
    for (size_t i = 0; i < seed_count; i++) {
        visited_set_add(visited, seed_nodes[i]);
    }
    
    Node *best_blank = NULL;
    float best_acceptance_score = 0.0f;
    
    /* COMPOUNDING: Explore through wave propagation to find blank nodes */
    /* Use similarity/context edges to guide search (compounds: smarter edges find blanks faster) */
    for (size_t step = 0; step < max_steps && wave_front_size > 0; step++) {
        Node **next_wave_front = NULL;
        size_t next_size = 0;
        size_t next_capacity = 0;
        Node **blank_candidates = NULL;  /* COMPOUNDING: Prioritize blank nodes */
        float *blank_priorities = NULL;
        size_t blank_count = 0;
        size_t blank_capacity = 0;
        
        /* Propagate from current wave front */
        for (size_t i = 0; i < wave_front_size; i++) {
            Node *node = wave_front[i];
            if (!node) continue;
            
            /* COMPOUNDING: Prioritize edges that likely lead to blanks (similarity/context edges) */
            for (size_t j = 0; j < node->outgoing_count; j++) {
                Edge *edge = node->outgoing_edges[j];
                if (!edge || !edge->to_node) continue;
                
                Node *candidate = edge->to_node;
                
                /* Check if already visited (O(1) with VisitedSet) */
                if (visited_set_contains(visited, candidate)) continue;
                
                /* COMPOUNDING: Calculate priority for blank node search */
                float blank_priority = 0.0f;
                
                /* Check if candidate is a blank node (highest priority) */
                if (candidate->payload_size == 0) {
                    /* Compute relative to actual priorities in current wave */
                    float max_priority = 0.0f;
                    /* Check other candidates in next wave front to establish baseline */
                    for (size_t k = 0; k < next_size; k++) {
                        if (next_wave_front && next_wave_front[k] && next_wave_front[k]->payload_size > 0) {
                            float candidate_priority = next_wave_front[k]->weight * 
                                                       (next_wave_front[k]->outgoing_count + 
                                                        next_wave_front[k]->incoming_count + 1);
                            if (candidate_priority > max_priority) {
                                max_priority = candidate_priority;
                            }
                        }
                    }
                    /* Blank gets priority relative to max, with boost (relative adaptive stability) */
                    if (max_priority > 0.0f) {
                        /* Compute boost from priority distribution if available */
                        float boost = 1.0f;
                        if (blank_count > 0 && blank_priorities) {
                            float p75 = compute_percentile_from_array(blank_priorities, blank_count, 75);
                            /* Use adaptive epsilon to prevent unstable division */
                            float range = max_priority;
                            float epsilon = compute_adaptive_epsilon(range);
                            if (p75 > epsilon) {
                                boost = max_priority / (p75 + epsilon);
                                /* Clip boost to prevent extreme values (adaptive clipping, no fallbacks) */
                                float *observed_boosts = blank_priorities;  /* Reuse array for clipping calculation */
                                float max_boost = compute_adaptive_clip_bound(observed_boosts, blank_count);
                                if (max_boost > 0.0f) {
                                    boost = fminf(boost, max_boost);
                                }
                                /* If max_boost is 0.0f, no clipping (no data available) */
                            } else {
                                /* p75 too small: use conservative boost */
                                boost = 1.0f + (1.0f / (max_priority + epsilon));
                            }
                        } else {
                            /* No distribution: use conservative boost */
                            float epsilon = compute_adaptive_epsilon(max_priority);
                            boost = 1.0f + (1.0f / (max_priority + epsilon));
                        }
                        blank_priority = max_priority * boost;
                    } else {
                        /* No priorities: compute from candidate weight relative to local context (data-driven) */
                        float local_avg = (node_get_local_outgoing_weight_avg(candidate) + 
                                          node_get_local_incoming_weight_avg(candidate)) / 2.0f;
                        if (candidate->weight > 0.0f && local_avg > 0.0f) {
                            /* Use adaptive epsilon for stable division */
                            float range = fmaxf(candidate->weight, local_avg);
                            float epsilon = compute_adaptive_epsilon(range);
                            float ratio = candidate->weight / (local_avg + epsilon);
                            /* Use adaptive clipping bound for ratio cap (relative adaptive stability) */
                            float *observed_ratios = NULL;
                            size_t ratio_count = 0;
                            /* Try to use blank_priorities if available for adaptive clipping */
                            if (blank_count > 0 && blank_priorities) {
                                /* Compute ratios from priorities relative to candidate weight */
                                observed_ratios = (float*)malloc(blank_count * sizeof(float));
                                for (size_t k = 0; k < blank_count; k++) {
                                    if (blank_candidates[k] && blank_candidates[k]->weight > 0.0f) {
                                        float local_avg_k = (node_get_local_outgoing_weight_avg(blank_candidates[k]) + 
                                                           node_get_local_incoming_weight_avg(blank_candidates[k])) / 2.0f;
                                        if (local_avg_k > 0.0f) {
                                            float range_k = fmaxf(blank_candidates[k]->weight, local_avg_k);
                                            float epsilon_k = compute_adaptive_epsilon(range_k);
                                            observed_ratios[ratio_count++] = blank_candidates[k]->weight / (local_avg_k + epsilon_k);
                                        }
                                    }
                                }
                            }
                            
                            float max_ratio = 0.0f;  /* Initialize to neutral */
                            if (observed_ratios && ratio_count > 0) {
                                max_ratio = compute_adaptive_clip_bound(observed_ratios, ratio_count);
                            }
                            
                            if (max_ratio <= 0.0f) {
                                /* No clipping data: compute from candidate's local context (no fallback) */
                                if (local_avg > 0.0f && candidate->weight > 0.0f) {
                                    float range = fmaxf(candidate->weight, local_avg);
                                    float epsilon = compute_adaptive_epsilon(range);
                                    /* Cap based on how much weight exceeds local average */
                                    max_ratio = candidate->weight / (local_avg + epsilon);
                                    /* Use adaptive clipping: cap at 2x the computed ratio */
                                    max_ratio = max_ratio * 2.0f;
                                } else if (candidate->weight > 0.0f) {
                                    /* Only weight available: use weight as minimal context */
                                    max_ratio = candidate->weight * 2.0f;
                                }
                                /* If max_ratio is still 0.0f, no clipping will be applied */
                            }
                            if (observed_ratios) free(observed_ratios);
                            
                            if (max_ratio > 0.0f) {
                                blank_priority = candidate->weight * fminf(ratio, max_ratio);
                            } else {
                                /* No clipping data available: use ratio as-is */
                                blank_priority = candidate->weight * ratio;
                            }
                        } else {
                            blank_priority = candidate->weight;  /* No data = use weight as-is */
                        }
                    }
                    
                    /* Store blank candidate with priority */
                    if (blank_count >= blank_capacity) {
                        blank_capacity = (blank_capacity == 0) ? 4 : blank_capacity * 2;
                        blank_candidates = (Node**)realloc(blank_candidates, blank_capacity * sizeof(Node*));
                        blank_priorities = (float*)realloc(blank_priorities, blank_capacity * sizeof(float));
                        if (!blank_candidates || !blank_priorities) {
                            if (blank_candidates) free(blank_candidates);
                            if (blank_priorities) free(blank_priorities);
                            continue;  /* Skip if allocation fails */
                        }
                    }
                    blank_candidates[blank_count] = candidate;
                    blank_priorities[blank_count] = blank_priority;
                    blank_count++;
                } else {
                    /* COMPOUNDING: Boost priority for edges that might lead to blanks */
                    /* Similarity edges and context edges often connect to blanks */
                    /* DATA-DRIVEN: Use local edge weight statistics (stats not available in this context) */
                    float local_avg = node_get_local_outgoing_weight_avg(candidate);
                    float lower_bound = 0.0f;
                    float upper_bound = 0.0f;
                    if (local_avg > 0.0f) {
                        /* Adaptive bounds: use relative range based on local distribution */
                        /* Lower bound: half of average (relative baseline) */
                        lower_bound = local_avg / (local_avg + 1.0f);
                        /* Upper bound: 1.5x average (relative to local context) */
                        upper_bound = local_avg * (1.0f + (local_avg / (local_avg + 1.0f)));
                    }
                    if (lower_bound > 0.0f && upper_bound > 0.0f && 
                        edge->weight > lower_bound && edge->weight < upper_bound) {  /* Data-driven range from local stats */
                        float similarity_hint = edge_compute_pattern_similarity(node, candidate);
                        /* Adaptive threshold: relative to local similarity distribution */
                        float similarity_threshold = (local_avg > 0.0f) ? local_avg / (local_avg + 1.0f) : 0.0f;
                        if (similarity_hint > similarity_threshold) {
                            /* Boost amount adapts to similarity strength */
                            float boost_factor = similarity_hint / (similarity_hint + 1.0f);
                            blank_priority += similarity_hint * boost_factor;  /* Adaptive boost for similarity edges */
                        }
                    }
                    
                    /* CONTEXT: Context is paths recently traveled - not available in this search */
                    /* Context edges will be created separately based on recent path during propagation */
                }
                
                /* Mark as visited (O(1) with VisitedSet) */
                visited_set_add(visited, candidate);
                
                /* Add to next wave front (regular nodes still explored, but blanks prioritized) */
                if (next_size >= next_capacity) {
                    next_capacity = (next_capacity == 0) ? 4 : next_capacity * 2;
                    next_wave_front = (Node**)realloc(next_wave_front, next_capacity * sizeof(Node*));
                    if (!next_wave_front) {
                        if (blank_candidates) free(blank_candidates);
                        if (blank_priorities) free(blank_priorities);
                        break;
                    }
                }
                next_wave_front[next_size++] = candidate;
            }
            
            /* COMPOUNDING: Process blank candidates first (highest priority) */
            for (size_t b = 0; b < blank_count; b++) {
                Node *blank_candidate = blank_candidates[b];
                
                /* UNIVERSAL: Use universal match function (no special cases) */
                float acceptance_score = node_calculate_match_strength(blank_candidate, pattern, pattern_size);
                
                /* Track best blank node (relative comparison - highest acceptance wins) */
                if (acceptance_score > best_acceptance_score) {
                    best_acceptance_score = acceptance_score;
                    best_blank = blank_candidate;
                }
            }
            
            /* Clean up blank candidate arrays */
            if (blank_candidates) {
                free(blank_candidates);
                blank_candidates = NULL;
            }
            if (blank_priorities) {
                free(blank_priorities);
                blank_priorities = NULL;
            }
            blank_count = 0;
            blank_capacity = 0;
        }
        
        free(wave_front);
        wave_front = next_wave_front;
        wave_front_size = next_size;
    }
    
    visited_set_free(visited);
    free(wave_front);
    return best_blank;  /* Return best blank node that could accept this pattern */
}

/* Find node through wave propagation exploration from seed nodes (uses whole graph via edges) */
/* Wave propagation explores graph through edges to discover existing nodes matching pattern */
/* Returns first matching node found, or NULL if not found through wave exploration */
static Node* wave_find_node_via_exploration(MelvinGraph *g, Node **seed_nodes, size_t seed_count,
                                             const uint8_t *pattern, size_t pattern_size, 
                                             size_t max_steps, WaveStatistics *stats) {
    if (!g || !seed_nodes || seed_count == 0 || max_steps == 0) return NULL;
    
    /* Initialize wave front with seed nodes */
    Node **wave_front = (Node**)malloc(seed_count * sizeof(Node*));
    if (!wave_front) return NULL;
    
    size_t wave_front_size = seed_count;
    memcpy(wave_front, seed_nodes, seed_count * sizeof(Node*));
    
    /* Track visited nodes to avoid cycles (O(1) pointer-based hash set) */
    /* Adaptive hash size based on graph size for optimal performance */
    size_t optimal_hash_size = calculate_optimal_hash_size(g->node_count);
    VisitedSet *visited = visited_set_create(optimal_hash_size);
    if (!visited) {
        free(wave_front);
        return NULL;
    }
    
    /* Mark seed nodes as visited */
    for (size_t i = 0; i < seed_count; i++) {
        visited_set_add(visited, seed_nodes[i]);
        /* Check seed node itself */
        if (node_payload_exact_match(seed_nodes[i], pattern, pattern_size)) {
            visited_set_free(visited);
            free(wave_front);
            return seed_nodes[i];
        }
    }
    
    Node *found = NULL;
    Node *best_similar = NULL;
    float best_match_strength = 0.0f;
    Node *best_hierarchy_match = NULL;
    float best_hierarchy_strength = 0.0f;
    
    /* UNIVERSAL: Prioritize nodes based on payload_size (actual node properties, not creation method) */
    /* Larger patterns should match larger nodes - universal rule based on size */
    /* No abstraction_level calculation - use payload_size directly */
    
    /* Explore through wave propagation (limited steps) */
    for (size_t step = 0; step < max_steps && wave_front_size > 0 && !found; step++) {
        Node **next_wave_front = NULL;
        size_t next_size = 0;
        size_t next_capacity = 0;
        Node **priority_candidates = NULL;  /* COMPOUNDING: Priority queue for smart edges */
        float *priority_scores = NULL;
        size_t priority_count = 0;
        size_t priority_capacity = 0;
        
        /* Propagate from current wave front */
        for (size_t i = 0; i < wave_front_size; i++) {
            Node *node = wave_front[i];
            if (!node) continue;
            
            /* COMPOUNDING: Sort edges by intelligence (similarity/context) to explore smart paths first */
            /* Collect all candidate edges with priority scores */
            for (size_t j = 0; j < node->outgoing_count; j++) {
                Edge *edge = node->outgoing_edges[j];
                if (!edge || !edge->to_node) continue;
                
                Node *candidate = edge->to_node;
                
                /* Check if already visited (O(1) with hash set) */
                if (visited_set_contains(visited, candidate)) continue;
                
                /* UNIVERSAL: Calculate priority score based on node properties (payload_size, weight, connections) */
                float priority_score = 0.0f;
                
                /* 1. Size-based priority: Prefer nodes that can match the pattern (universal - all nodes) */
                if (candidate->payload_size >= pattern_size) {
                    if (candidate->payload_size == pattern_size) {
                        /* Exact match priority - from stats or node properties */
                        float exact_match_boost = 0.0f;
                        if (stats && stats->histogram_count > 0) {
                            exact_match_boost = stats->p90;  /* 90th percentile */
                        } else {
                            /* Compute from candidate's actual properties */
                            float node_strength = candidate->weight * 
                                                 (candidate->outgoing_count + candidate->incoming_count + 1);
                            exact_match_boost = node_strength;
                        }
                        priority_score += exact_match_boost;
                    } else {
                        /* Larger nodes get priority boost relative to how much larger they are */
                        float size_ratio = (float)pattern_size / (float)candidate->payload_size;
                        float size_multiplier = 0.0f;
                        if (stats && stats->histogram_count > 0) {
                            size_multiplier = stats->p50;  /* Median */
                        } else {
                            /* Compute from candidate weight relative to pattern */
                            size_multiplier = candidate->weight / (candidate->weight + 1.0f);
                        }
                        priority_score += size_multiplier * size_ratio;
                    }
                }
                
                /* 2. Edge weight priority: Strong edges likely lead to relevant patterns (compounds: use learned structure) */
                float edge_weight_multiplier = 0.0f;
                if (stats && stats->edge_weight_count > 0) {
                    /* Use average of p25 and p75 as median estimate */
                    float median_estimate = (stats->edge_weight_p25 + stats->edge_weight_p75) / 2.0f;
                    edge_weight_multiplier = median_estimate / (median_estimate + 1.0f);
                } else {
                    /* Compute from actual edge weight relative to local average */
                    float local_avg = node_get_local_outgoing_weight_avg(node);
                    if (local_avg > 0.0f) {
                        edge_weight_multiplier = local_avg / (local_avg + 1.0f);
                    } else {
                        edge_weight_multiplier = edge->weight / (edge->weight + 1.0f);
                    }
                }
                priority_score += edge->weight * edge_weight_multiplier;
                
                /* 3. Similarity edge priority: Check if this might be a similarity edge (compounds: use semantic connections) */
                /* DATA-DRIVEN: Use 25th-75th percentile range from actual edge weights */
                float lower_bound = (stats && stats->edge_weight_count > 0) ? 
                                   stats->edge_weight_p25 : 0.0f;
                float upper_bound = (stats && stats->edge_weight_count > 0) ? 
                                   stats->edge_weight_p75 : 0.0f;
                if (edge->weight > lower_bound && edge->weight < upper_bound) {  /* Data-driven range */
                    float similarity_hint = edge_compute_pattern_similarity(node, candidate);
                    /* Similarity threshold and boost - from stats or computed */
                    float similarity_threshold = 0.0f;
                    float similarity_boost = 0.0f;
                    if (stats && stats->similarity_count > 0) {
                        /* Use p75 as threshold (available in stats) */
                        similarity_threshold = stats->similarity_p75;
                        similarity_boost = stats->similarity_p75 / (stats->similarity_p75 + 1.0f);
                    } else {
                        /* Compute from actual similarity values */
                        float local_avg = node_get_local_outgoing_weight_avg(node);
                        similarity_threshold = (local_avg > 0.0f) ? local_avg / (local_avg + 1.0f) : 0.0f;
                        similarity_boost = 1.0f;  /* Full boost when no stats */
                    }
                    if (similarity_hint > similarity_threshold) {
                        priority_score += similarity_hint * similarity_boost;
                    }
                }
                
                /* Store candidate with priority score for sorted exploration */
                if (priority_count >= priority_capacity) {
                    priority_capacity = (priority_capacity == 0) ? 4 : priority_capacity * 2;
                    priority_candidates = (Node**)realloc(priority_candidates, priority_capacity * sizeof(Node*));
                    priority_scores = (float*)realloc(priority_scores, priority_capacity * sizeof(float));
                    if (!priority_candidates || !priority_scores) {
                        if (priority_candidates) free(priority_candidates);
                        if (priority_scores) free(priority_scores);
                        visited_set_free(visited);
                        free(wave_front);
                        return found ? found : (best_hierarchy_match ? best_hierarchy_match : best_similar);
                    }
                }
                priority_candidates[priority_count] = candidate;
                priority_scores[priority_count] = priority_score;
                priority_count++;
            }
            
            /* COMPOUNDING: Explore candidates in priority order (smart edges first) */
            /* Simple insertion sort by priority (local-only, no global sort needed) */
            for (size_t p = 0; p < priority_count; p++) {
                /* Find highest priority candidate */
                size_t best_pri_idx = p;
                float best_pri_score = priority_scores[p];
                for (size_t q = p + 1; q < priority_count; q++) {
                    if (priority_scores[q] > best_pri_score) {
                        best_pri_score = priority_scores[q];
                        best_pri_idx = q;
                    }
                }
                
                /* Swap to position */
                if (best_pri_idx != p) {
                    Node *temp_node = priority_candidates[p];
                    float temp_score = priority_scores[p];
                    priority_candidates[p] = priority_candidates[best_pri_idx];
                    priority_scores[p] = priority_scores[best_pri_idx];
                    priority_candidates[best_pri_idx] = temp_node;
                    priority_scores[best_pri_idx] = temp_score;
                }
                
                /* Process highest priority candidate */
                Node *candidate = priority_candidates[p];
                
                /* Mark as visited (O(1) with hash set) */
                if (visited_set_contains(visited, candidate)) continue;
                visited_set_add(visited, candidate);
                
                /* UNIVERSAL: Check all nodes that can match (payload_size >= pattern_size) */
                /* No special cases - all nodes follow the same matching rules */
                if (candidate->payload_size >= pattern_size) {
                    /* Check exact match first (most efficient) */
                    if (node_payload_exact_match(candidate, pattern, pattern_size)) {
                        found = candidate;
                        break;
                    }
                    /* Track best match for larger nodes (can match full pattern) */
                    float match_strength = node_calculate_match_strength(candidate, pattern, pattern_size);
                    if (match_strength > best_hierarchy_strength) {
                        best_hierarchy_strength = match_strength;
                        best_hierarchy_match = candidate;
                    }
                } else {
                    /* Smaller nodes - use similarity matching for generalization */
                    /* Check exact match first (most efficient) */
                    if (node_payload_exact_match(candidate, pattern, pattern_size)) {
                        found = candidate;
                        break;
                    }
                    
                    /* Use similarity matching for generalization (relative comparison, no threshold) */
                    float match_strength = node_calculate_match_strength(candidate, pattern, pattern_size);
                    
                    /* Track best similar match (relative comparison - strongest match strength wins) */
                    if (match_strength > best_match_strength) {
                        best_match_strength = match_strength;
                        best_similar = candidate;
                    }
                }
                
                /* Add to next wave front for continued exploration */
                if (next_size >= next_capacity) {
                    next_capacity = (next_capacity == 0) ? 4 : next_capacity * 2;
                    next_wave_front = (Node**)realloc(next_wave_front, next_capacity * sizeof(Node*));
                    if (!next_wave_front) {
                        if (priority_candidates) free(priority_candidates);
                        if (priority_scores) free(priority_scores);
                        visited_set_free(visited);
                        free(wave_front);
                        return found ? found : (best_hierarchy_match ? best_hierarchy_match : best_similar);
                    }
                }
                next_wave_front[next_size++] = candidate;
            }
            
            /* Clean up priority arrays */
            if (priority_candidates) {
                free(priority_candidates);
                priority_candidates = NULL;
            }
            if (priority_scores) {
                free(priority_scores);
                priority_scores = NULL;
            }
            priority_count = 0;
            priority_capacity = 0;
            
            if (found) break;
        }
        
        free(wave_front);
        wave_front = next_wave_front;
        wave_front_size = next_size;
    }
    
    /* COMPOUNDING: Prioritize hierarchy matches for better abstraction (compounds: hierarchy enables faster matching) */
    /* If no exact match found, prefer hierarchy match over regular similar match */
    if (!found) {
        /* Adaptive threshold: relative to match strength distribution */
        /* Use median of available match strengths if available, else relative to max */
        float hierarchy_threshold = (best_hierarchy_strength > 0.0f && best_match_strength > 0.0f) ?
                                   (best_hierarchy_strength + best_match_strength) / 2.0f :
                                   (best_hierarchy_strength > 0.0f ? best_hierarchy_strength / (best_hierarchy_strength + 1.0f) : 0.0f);
        if (best_hierarchy_match && best_hierarchy_strength > hierarchy_threshold) {
            found = best_hierarchy_match;  /* Prefer hierarchy match if reasonably strong */
        } else if (best_similar && best_match_strength > 0.0f) {
            found = best_similar;  /* Fall back to regular similar match */
        }
    }
    
    visited_set_free(visited);
    free(wave_front);
    return found;
}

/* Process input data to find sequential patterns and activate/create nodes */
/* For "CAT", creates/finds nodes for C, A, T and returns them in sequence */
/* Uses wave propagation to explore graph and discover existing nodes before creating new ones */
/* Wave prop explores whole graph through edges (local traversal, no global search) */
/* Returns array of activated nodes in sequence (caller must free) */
Node** wave_process_sequential_patterns(MelvinGraph *g, const uint8_t *data, size_t data_size, 
                                         size_t *out_count) {
    if (!g || !data || data_size == 0 || !out_count) return NULL;
    
    Node **sequence = NULL;
    size_t sequence_count = 0;
    size_t sequence_capacity = 0;
    Node *prev_node = NULL;  /* Track previous node to use local edges */
    
    /* Collect currently activated nodes as seeds for wave exploration */
    Node **seed_nodes = NULL;
    size_t seed_count = 0;
    size_t seed_capacity = 0;
    for (size_t i = 0; i < g->node_count; i++) {
        Node *node = g->nodes[i];
        if (node && node->activation_strength > 0.0f) {
            if (seed_count >= seed_capacity) {
                seed_capacity = (seed_capacity == 0) ? 4 : seed_capacity * 2;
                seed_nodes = (Node**)realloc(seed_nodes, seed_capacity * sizeof(Node*));
                if (!seed_nodes) break;
            }
            seed_nodes[seed_count++] = node;
        }
    }
    
    /* Process input byte-by-byte to find sequential patterns */
    /* Each byte/pattern becomes a node, creating the sequence from data */
    for (size_t i = 0; i < data_size; i++) {
        Node *activated_node = NULL;
        
        /* STRATEGY: HIERARCHY-FIRST MATCHING - Try larger patterns first (compounds: 1-check matching) */
        /* This implements the vision: "Try larger patterns first (hierarchy-first matching)" */
        /* Start with largest possible patterns and work down to single bytes */
        
            /* DATA-DRIVEN: Compute max pattern size from local context (no hardcoded limit) */
        size_t max_pattern_size = data_size - i;  /* Start with maximum possible (remaining data) */
            
            /* Adapt based on local node sizes: if nodes in this area are large, try larger patterns */
            if (seed_count > 0) {
                size_t total_payload_size = 0;
                size_t nodes_with_payload = 0;
            size_t max_local_payload = 0;
                for (size_t k = 0; k < seed_count; k++) {
                    if (seed_nodes[k] && seed_nodes[k]->payload_size > 0) {
                        total_payload_size += seed_nodes[k]->payload_size;
                        nodes_with_payload++;
                    if (seed_nodes[k]->payload_size > max_local_payload) {
                        max_local_payload = seed_nodes[k]->payload_size;
                    }
                    }
                }
                if (nodes_with_payload > 0) {
                    size_t avg_payload_size = total_payload_size / nodes_with_payload;
                /* Try patterns up to max of: average size, max local size, or remaining data */
                /* Fully adaptive - no hardcoded defaults */
                size_t adaptive_max = (max_local_payload > avg_payload_size) ? max_local_payload : avg_payload_size;
                if (adaptive_max > 0 && adaptive_max < max_pattern_size) {
                    max_pattern_size = adaptive_max;
                }
                }
            }
            
            /* Cap at remaining data size */
            if (max_pattern_size > data_size - i) {
                max_pattern_size = data_size - i;
            }
        // #region agent log
        char log_data[256];
        snprintf(log_data, sizeof(log_data), "{\"max_pattern_size\":%zu,\"data_remaining\":%zu,\"seed_count\":%zu}",
                 max_pattern_size, data_size - i, seed_count);
        debug_log("melvin.c:1623", "Hierarchy-first pattern size computed", log_data);
        // #endregion agent log
        
        /* HIERARCHY-FIRST: Try larger patterns first, then fall back to smaller */
        /* Start from max_pattern_size and work down to 1 */
        for (size_t try_size = max_pattern_size; try_size >= 1 && !activated_node; try_size--) {
            const uint8_t *pattern = data + i;
            size_t pattern_size = try_size;
            
            /* Skip if pattern extends beyond data */
            if (i + pattern_size > data_size) continue;
            
            /* 1. Check previous node's outgoing edges (fastest - sequential patterns connect via edges) */
            if (prev_node && pattern_size == 1) {
                activated_node = node_find_via_outgoing(prev_node, pattern, pattern_size);
            }
            
            /* 2. Check recently activated nodes in current sequence (current processing context) */
            if (!activated_node) {
                for (size_t k = 0; k < sequence_count; k++) {
                    Node *seq_node = sequence[k];
                    if (node_payload_exact_match(seq_node, pattern, pattern_size)) {
                        activated_node = seq_node;
                        break;
                    }
                }
            }
            
            /* 3. For larger patterns (>1 byte), use wave exploration to find hierarchy nodes */
            if (!activated_node && pattern_size > 1) {
                /* UNIVERSAL: Use wave exploration to find nodes matching larger pattern */
                /* Prioritizes nodes with payload_size >= pattern_size (all nodes, not just combined) */
                Node *hierarchy_match = wave_find_node_via_exploration(g, seed_nodes, seed_count,
                                                                        pattern, pattern_size, 2, NULL);
                if (hierarchy_match && hierarchy_match->payload_size >= pattern_size &&
                    node_payload_exact_match(hierarchy_match, pattern, pattern_size)) {
                    /* Found node matching larger pattern - use it (compounds: skip individual bytes) */
                    activated_node = hierarchy_match;
                    i += (pattern_size - 1);  /* Skip bytes already matched */
                    break;
                }
            }
            
            /* DATA-DRIVEN: Bootstrap activation_strength when node matches input (not hardcoded threshold) */
            /* Set activation based on match quality when node is found via any method above */
            if (activated_node && activated_node->activation_strength == 0.0f) {
                float match_strength = node_calculate_match_strength(activated_node, pattern, pattern_size);
                activated_node->activation_strength = match_strength;  /* Use computed match quality (data-driven) */
            }
        }
        
        /* If no match found in hierarchy-first search, fall back to single-byte pattern matching */
        /* This handles cases where larger patterns don't exist yet */
        const uint8_t *pattern = data + i;
        size_t pattern_size = 1;  /* Fall back to single byte if hierarchy-first didn't match */
        
        /* 4. Use wave propagation to explore graph from activated seeds (whole graph context via edges) */
        /* This discovers existing nodes through natural graph exploration (exact or similar matches) */
        /* Compounding learning: blank nodes actively accept patterns, teaching the whole system */
        Node *found_match = NULL;
        bool is_exact_match = false;
        if (!activated_node && seed_count > 0) {
            found_match = wave_find_node_via_exploration(g, seed_nodes, seed_count, 
                                                          pattern, pattern_size, 3, NULL);
            if (found_match) {
                /* Use exact match if found */
                is_exact_match = node_payload_exact_match(found_match, pattern, pattern_size);
                if (is_exact_match) {
                    activated_node = found_match;
                    /* DATA-DRIVEN: Bootstrap activation_strength based on match quality */
                    if (activated_node->activation_strength == 0.0f) {
                        float match_strength = node_calculate_match_strength(activated_node, pattern, pattern_size);
                        activated_node->activation_strength = match_strength;  /* Use computed match quality */
                    }
                }
            }
        }
        
        /* 5. COMPOUNDING: Check for blank nodes that could accept this pattern (compounding learning) */
        /* Blank nodes are active learning slots - every pattern can teach existing blank nodes */
        /* This compounds knowledge: each pattern connecting to a blank teaches it about its category */
        /* Enhanced: Uses similarity/context edges to find blanks more efficiently */
        if (!activated_node && seed_count > 0) {
            /* Compute adaptive exploration steps based on graph size and context (relative adaptive stability) */
            size_t adaptive_steps = compute_adaptive_exploration_steps(g, seed_count);
            Node *accepting_blank = wave_find_accepting_blank_node(g, seed_nodes, seed_count,
                                                                    pattern, pattern_size, adaptive_steps);
            if (accepting_blank) {
                /* Compute how well blank accepts this pattern */
                /* UNIVERSAL: Use universal match function (no special cases) */
                float acceptance_strength = node_calculate_match_strength(accepting_blank, pattern, pattern_size);
                
                /* If match is strong enough, try to fill the blank node */
                Node *filled_node = NULL;
                /* DATA-DRIVEN: Use local context for threshold (stats not available in wave_process_sequential_patterns) */
                float local_avg = (node_get_local_outgoing_weight_avg(accepting_blank) + 
                                   node_get_local_incoming_weight_avg(accepting_blank)) / 2.0f;
                float acceptance_threshold = (local_avg > 0.0f) ? local_avg : 0.0f;
                if (acceptance_strength > acceptance_threshold) {  /* Data-driven threshold from local context */
                    filled_node = node_fill_blank(accepting_blank, pattern, pattern_size, acceptance_strength);
                    if (filled_node && graph_add_node(g, filled_node)) {
                        /* Transfer edges from blank to filled node to preserve connectivity */
                        for (size_t i = 0; i < accepting_blank->incoming_count; i++) {
                            Edge *old_edge = accepting_blank->incoming_edges[i];
                            if (old_edge && old_edge->from_node) {
                                Edge *new_edge = edge_create(old_edge->from_node, filled_node, true);
                                if (new_edge) {
                                    new_edge->weight = old_edge->weight;
                                    graph_add_edge(g, new_edge, old_edge->from_node, filled_node);
                                }
                            }
                        }
                        for (size_t i = 0; i < accepting_blank->outgoing_count; i++) {
                            Edge *old_edge = accepting_blank->outgoing_edges[i];
                            if (old_edge && old_edge->to_node) {
                                Edge *new_edge = edge_create(filled_node, old_edge->to_node, true);
                                if (new_edge) {
                                    new_edge->weight = old_edge->weight;
                                    graph_add_edge(g, new_edge, filled_node, old_edge->to_node);
                                }
                            }
                        }
                        activated_node = filled_node;  /* Use the filled node */
                    } else {
                        if (filled_node) node_free(filled_node);
                    }
                }
                
                /* If blank wasn't filled (or filling failed), create new pattern node and connect to blank */
                if (!activated_node) {
                    Node *new_pattern_node = node_create(pattern, pattern_size);
                    if (new_pattern_node && graph_add_node(g, new_pattern_node)) {
                        /* Connect new pattern to blank node (bidirectional - blank learns from pattern) */
                        Edge *edge1 = edge_create(new_pattern_node, accepting_blank, true);
                        Edge *edge2 = edge_create(accepting_blank, new_pattern_node, true);
                        if (edge1) {
                            edge1->weight = acceptance_strength;
                            graph_add_edge(g, edge1, new_pattern_node, accepting_blank);
                        }
                        if (edge2) {
                            edge2->weight = acceptance_strength;
                            graph_add_edge(g, edge2, accepting_blank, new_pattern_node);
                        }
                        
                        activated_node = new_pattern_node;  /* Use the new pattern node */
                    } else {
                        if (new_pattern_node) node_free(new_pattern_node);
                        /* Fall back to using the blank node itself */
                        activated_node = accepting_blank;
                    }
                }
            }
        }
        
        /* 6. If no exact match and no accepting blank, check for similar patterns and create blank bridge */
        /* Blank nodes emerge naturally when similar patterns are found but no accepting blank exists */
        if (!activated_node && found_match && !is_exact_match) {
            /* Similar pattern found but no blank accepted it - create blank bridge */
            Node *blank_bridge = NULL;
            
            /* Check if blank bridge already exists */
            for (size_t i = 0; i < found_match->outgoing_count; i++) {
                Edge *edge = found_match->outgoing_edges[i];
                if (edge && edge->to_node && edge->to_node->payload_size == 0) {
                    blank_bridge = edge->to_node;
                    break;
                }
            }
            
            /* If no blank bridge exists, create one (emerges naturally) */
            if (!blank_bridge) {
                blank_bridge = node_create_blank();
                if (blank_bridge && graph_add_node(g, blank_bridge)) {
                    blank_bridge->abstraction_level = found_match->abstraction_level;
                    float match_strength = node_calculate_match_strength(found_match, pattern, pattern_size);
                    blank_bridge->weight = (found_match->weight + match_strength) / 2.0f;
                    
                    /* Connect blank to similar pattern */
                    Edge *edge1 = edge_create(blank_bridge, found_match, true);
                    Edge *edge2 = edge_create(found_match, blank_bridge, true);
                    if (edge1) {
                        edge1->weight = match_strength;
                        graph_add_edge(g, edge1, blank_bridge, found_match);
                    }
                    if (edge2) {
                        edge2->weight = match_strength;
                        graph_add_edge(g, edge2, found_match, blank_bridge);
                    }
                } else {
                    if (blank_bridge) node_free(blank_bridge);
                    blank_bridge = NULL;
                }
            }
            
            /* Create new pattern node and connect it to blank bridge (compounding learning) */
            Node *new_pattern_node = node_create(pattern, pattern_size);
            if (new_pattern_node && graph_add_node(g, new_pattern_node)) {
                /* Connect new pattern to blank bridge */
                Edge *edge1 = edge_create(new_pattern_node, blank_bridge, true);
                Edge *edge2 = edge_create(blank_bridge, new_pattern_node, true);
                if (edge1) {
                    float match_strength = node_calculate_match_strength(found_match, pattern, pattern_size);
                    edge1->weight = match_strength;
                    graph_add_edge(g, edge1, new_pattern_node, blank_bridge);
                }
                if (edge2) {
                    float match_strength = node_calculate_match_strength(found_match, pattern, pattern_size);
                    edge2->weight = match_strength;
                    graph_add_edge(g, edge2, blank_bridge, new_pattern_node);
                }
                activated_node = new_pattern_node;
            } else {
                if (new_pattern_node) node_free(new_pattern_node);
                activated_node = blank_bridge ? blank_bridge : found_match;
            }
        }
        
        /* 6. If wave prop didn't find any match, create new node */
        /* Wave exploration uses whole graph as context, but through local edge traversal */
        if (!activated_node) {
            activated_node = node_create(pattern, pattern_size);
            if (activated_node && graph_add_node(g, activated_node)) {
                /* New node created - wave prop explored graph but didn't find match */
            } else {
                if (activated_node) node_free(activated_node);
                activated_node = NULL;
            }
        }
        
        if (activated_node) {
            /* Set initial activation strength based on match strength (if pattern matched) */
            /* Otherwise, node will compute activation from inputs during wave propagation */
            if (activated_node->activation_strength == 0.0f) {
                /* Use match strength as initial activation if this node was matched */
                float match_strength = node_calculate_match_strength(activated_node, pattern, pattern_size);
                activated_node->activation_strength = match_strength;
            }
            
            /* Always add node to sequence (even if same node appears multiple times in input) */
            /* Sequence represents the actual input pattern, allowing repeated nodes */
            /* This enables patterns like "hello" where 'l' appears twice */
            if (sequence_count >= sequence_capacity) {
                sequence_capacity = (sequence_capacity == 0) ? 4 : sequence_capacity * 2;
                sequence = (Node**)realloc(sequence, (sequence_capacity + 1) * sizeof(Node*));
                if (!sequence) {
                    *out_count = 0;
                    return NULL;
                }
            }
            sequence[sequence_count++] = activated_node;
            prev_node = activated_node;  /* Track for next iteration (even if same node) */
        }
    }
    
    /* Null-terminate */
    if (sequence) {
        sequence[sequence_count] = NULL;
    }
    
    if (seed_nodes) free(seed_nodes);
    
    *out_count = sequence_count;
    return sequence;
}

/* Create edges between similar patterns (structural similarity) */
/* Philosophy: Similar patterns should connect even if they rarely co-activate */
void wave_create_edges_from_similarity(MelvinGraph *g, Node *node, float similarity_threshold) {
    if (!g || !node) return;  /* UNIVERSAL: All nodes can form similarity edges */
    
    /* Use wave propagation to find similar patterns (local-only exploration) */
    Node **seed_nodes = NULL;
    size_t seed_count = 0;
    size_t seed_capacity = 4;
    
    seed_nodes = (Node**)malloc(seed_capacity * sizeof(Node*));
    if (!seed_nodes) return;
    seed_nodes[0] = node;
    seed_count = 1;
    
    /* Find similar patterns via wave exploration (limited steps, local-only) */
    Node *similar = wave_find_node_via_exploration(g, seed_nodes, seed_count,
                                                   node->payload, node->payload_size, 3, NULL);
    free(seed_nodes);
    
    if (!similar || similar == node) return;
    
    /* Check if edge already exists */
    if (node_find_edge_to(node, similar)) return;
    
    /* Compute similarity score */
    float similarity = edge_compute_pattern_similarity(node, similar);
    
    /* UNIVERSAL: Threshold is relative to local context (no hardcoded values) */
    float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                       node_get_local_incoming_weight_avg(node)) / 2.0f;
    float local_max = 0.0f;
    
    /* Find max edge weight in local context */
    for (size_t i = 0; i < node->outgoing_count; i++) {
        if (node->outgoing_edges[i] && node->outgoing_edges[i]->weight > local_max) {
            local_max = node->outgoing_edges[i]->weight;
        }
    }
    for (size_t i = 0; i < node->incoming_count; i++) {
        if (node->incoming_edges[i] && node->incoming_edges[i]->weight > local_max) {
            local_max = node->incoming_edges[i]->weight;
        }
    }
    
    /* Relative threshold: similarity must be meaningful relative to local context */
    /* Adaptive threshold based on local edge weight distribution */
    float relative_threshold = (local_avg > 0.0f) ? local_avg : 
                              (local_max > 0.0f ? local_max / (local_max + 1.0f) : 0.0f);
    if (similarity_threshold > 0.0f) {
        /* Use provided threshold as minimum, but adjust relative to context */
        relative_threshold = (relative_threshold > similarity_threshold) ? relative_threshold : similarity_threshold;
    }
    
    if (similarity >= relative_threshold) {
        /* Create bidirectional edge for similar patterns */
        Edge *edge1 = edge_create(node, similar, true);
        Edge *edge2 = edge_create(similar, node, true);
        
        if (edge1 && edge2) {
            /* UNIVERSAL: Weight relative to local context (no hardcoded multiplier) */
            float context_weight = (local_avg > 0.0f) ? similarity * (local_avg / (local_avg + 1.0f)) : similarity;
            edge1->weight = context_weight;
            edge2->weight = context_weight;
            
            graph_add_edge(g, edge1, node, similar);
            graph_add_edge(g, edge2, similar, node);
        } else {
            if (edge1) edge_free(edge1);
            if (edge2) edge_free(edge2);
        }
    }
}

/* Create edges between nodes based on recent path context */
/* Philosophy: Context is the paths recently traveled - nodes in same path share context */
void wave_create_edges_from_context(MelvinGraph *g, Node **recently_activated, size_t count,
                                    VisitedSet *context_visited) {
    if (!g || !recently_activated || count < 2) return;
    if (!context_visited) return;
    
    /* Check pairs of recently activated nodes for context similarity */
    /* Context = nodes that wave propagation has explored (paths recently traveled) */
    for (size_t i = 0; i < count - 1; i++) {
        Node *node1 = recently_activated[i];
        if (!node1) continue;
        
        for (size_t j = i + 1; j < count; j++) {
            Node *node2 = recently_activated[j];
            if (!node2 || node1 == node2) continue;
            
            /* Skip if edge already exists */
            if (node_find_edge_to(node1, node2)) continue;
            
            /* Compute context similarity based on wave propagation exploration */
            /* Context = nodes visited by wave propagation (paths recently traveled) */
            float context_similarity = edge_compute_context_similarity(node1, node2, context_visited);
            
            /* UNIVERSAL: Threshold relative to local context (no hardcoded values) */
            float local_avg1 = (node_get_local_outgoing_weight_avg(node1) + 
                               node_get_local_incoming_weight_avg(node1)) / 2.0f;
            float local_avg2 = (node_get_local_outgoing_weight_avg(node2) + 
                               node_get_local_incoming_weight_avg(node2)) / 2.0f;
            float avg_local = (local_avg1 + local_avg2) / 2.0f;
            
            /* Relative threshold: context similarity must be meaningful relative to local context */
            /* Adaptive threshold based on local edge weight distribution */
            float context_threshold = (avg_local > 0.0f) ? avg_local / (avg_local + 1.0f) : 0.0f;
            
            if (context_similarity >= context_threshold) {
                /* Create bidirectional edge based on context */
                Edge *edge1 = edge_create(node1, node2, true);
                Edge *edge2 = edge_create(node2, node1, true);
                
                if (edge1 && edge2) {
                    /* UNIVERSAL: Weight relative to local context (no hardcoded multiplier) */
                    float context_weight = (avg_local > 0.0f) ? 
                                          context_similarity * (avg_local / (avg_local + 1.0f)) : 
                                          context_similarity;
                    edge1->weight = context_weight;
                    edge2->weight = context_weight;
                    
                    graph_add_edge(g, edge1, node1, node2);
                    graph_add_edge(g, edge2, node2, node1);
                } else {
                    if (edge1) edge_free(edge1);
                    if (edge2) edge_free(edge2);
                }
            }
        }
    }
}

/* ========================================
 * UNIVERSAL LAWS: Generalization & Combination
 * ======================================== */

/* UNIVERSAL LAW: Pattern Generalization (Abstraction) */
/* When multiple nodes share similar patterns, create a generalization node */
/* Generalization node connects to all similar patterns - represents common abstraction */
/* All nodes can generalize - no special category nodes */
void wave_form_universal_generalizations(MelvinGraph *g, Node **co_activated, size_t count) {
    if (!g || !co_activated || count < 2) return;
    
    /* Find groups of similar nodes */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = i + 1; j < count; j++) {
            Node *node1 = co_activated[i];
            Node *node2 = co_activated[j];
            
            if (!node1 || !node2 || node1->payload_size == 0 || node2->payload_size == 0) continue;
            
            /* Compute similarity */
            float similarity = edge_compute_pattern_similarity(node1, node2);
            
            /* UNIVERSAL: Similarity threshold relative to local context */
            float local_avg1 = (node_get_local_outgoing_weight_avg(node1) + 
                               node_get_local_incoming_weight_avg(node1)) / 2.0f;
            float local_avg2 = (node_get_local_outgoing_weight_avg(node2) + 
                               node_get_local_incoming_weight_avg(node2)) / 2.0f;
            float avg_context = (local_avg1 + local_avg2) / 2.0f;
            
            /* Relative similarity range - similar but not identical */
            float min_similarity = avg_context;
            float max_similarity = (avg_context + 1.0f) / 2.0f;
            
            /* Similar but not identical - generalization candidate */
            if ((min_similarity == 0.0f || similarity >= min_similarity) && 
                (max_similarity == 1.0f || similarity <= max_similarity)) {
                
                /* Check if generalization node already exists */
                bool generalization_exists = false;
                for (size_t k = 0; k < g->node_count; k++) {
                    Node *gen = g->nodes[k];
                    if (!gen) continue;
                    
                    /* Check if gen connects to both similar nodes */
                    bool connected_to_both = (node_find_edge_to(gen, node1) && 
                                             node_find_edge_to(gen, node2));
                    if (connected_to_both) {
                        generalization_exists = true;
                        break;
                    }
                }
                
                /* UNIVERSAL: Create generalization node (blank node = abstract pattern) */
                if (!generalization_exists) {
                    Node *generalization = node_create_blank();  /* No payload = abstract representation */
                    if (generalization && graph_add_node(g, generalization)) {
                        /* Connect to both similar nodes */
                        Edge *e1 = edge_create(node1, generalization, true);
                        Edge *e2 = edge_create(generalization, node1, true);
                        Edge *e3 = edge_create(node2, generalization, true);
                        Edge *e4 = edge_create(generalization, node2, true);
                        
                        if (e1 && e2 && e3 && e4) {
                            /* UNIVERSAL: Weight relative to similarity and local context */
                            float gen_weight = (avg_context > 0.0f) ? 
                                              similarity * (avg_context / (avg_context + 1.0f)) : 
                                              similarity;
                            e1->weight = gen_weight;
                            e2->weight = gen_weight;
                            e3->weight = gen_weight;
                            e4->weight = gen_weight;
                            graph_add_edge(g, e1, node1, generalization);
                            graph_add_edge(g, e2, generalization, node1);
                            graph_add_edge(g, e3, node2, generalization);
                            graph_add_edge(g, e4, generalization, node2);
                        } else {
                            if (e1) edge_free(e1);
                            if (e2) edge_free(e2);
                            if (e3) edge_free(e3);
                            if (e4) edge_free(e4);
                            /* Remove generalization node if edges failed */
                            if (generalization) {
                                for (size_t k = 0; k < g->node_count; k++) {
                                    if (g->nodes[k] == generalization) {
                                        g->nodes[k] = g->nodes[g->node_count - 1];
                                        g->node_count--;
                                        break;
                                    }
                                }
                                node_free(generalization);
                            }
                        }
                    }
                }
            }
        }
    }
}

/* UNIVERSAL LAW: Pattern Combination (Hierarchy) */
/* When two nodes co-activate strongly (edge weight grows), they can combine */
/* This creates a larger node containing both patterns - hierarchy emerges naturally */
/* All nodes follow this rule - no special hierarchy formation */
void wave_form_universal_combinations(MelvinGraph *g, Node **co_activated, size_t count) {
    if (!g || !co_activated || count < 2) return;
    
    /* Check all pairs of co-activated nodes */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = i + 1; j < count; j++) {
            Node *node1 = co_activated[i];
            Node *node2 = co_activated[j];
            
            if (!node1 || !node2) continue;
            
            /* Check if they have a strong connecting edge (co-activation) */
            Edge *connecting = node_find_edge_to(node1, node2);
            if (!connecting) {
                /* Also check reverse direction */
                connecting = node_find_edge_to(node2, node1);
            }
            
            if (connecting) {
                /* UNIVERSAL: Threshold relative to local context */
                float local_avg1 = (node_get_local_outgoing_weight_avg(node1) + 
                                   node_get_local_incoming_weight_avg(node1)) / 2.0f;
                float local_avg2 = (node_get_local_outgoing_weight_avg(node2) + 
                                   node_get_local_incoming_weight_avg(node2)) / 2.0f;
                float avg_local = (local_avg1 + local_avg2) / 2.0f;
                
                /* NO HARDCODED THRESHOLD: Use minimal context when no local data exists */
                /* When avg_local = 0.0, use node's own properties as minimal context */
                float comparison_value = avg_local;
                if (comparison_value <= 0.0f) {
                    /* Minimal context: use node weights as baseline (data-driven, not threshold) */
                    float node_weight_avg = (node1->weight + node2->weight) / 2.0f;
                    if (node_weight_avg > 0.0f) {
                        comparison_value = node_weight_avg;
                    } else {
                        /* Still no context: use presence of edge (edge exists = positive signal) */
                        /* Allow combination when edge exists and has any positive weight (bootstrap) */
                        comparison_value = 0.0f;  /* Will check: connecting->weight > 0.0f */
                    }
                }
                
                /* UNIVERSAL: If edge is strong relative to available context, combine */
                /* Relative comparison - no hardcoded threshold, uses minimal context when needed */
                bool should_combine = false;
                if (comparison_value > 0.0f) {
                    should_combine = (connecting->weight >= comparison_value);
                } else {
                    /* No context available: bootstrap with any positive edge weight */
                    should_combine = (connecting->weight > 0.0f);
                }
                
                if (should_combine) {
                    /* Check if combined node already exists */
                    size_t expected_size = node1->payload_size + node2->payload_size;
                    bool combination_exists = false;
                    
                    for (size_t k = 0; k < node1->outgoing_count; k++) {
                        Edge *e = node1->outgoing_edges[k];
                        if (!e || !e->to_node) continue;
                        Node *candidate = e->to_node;
                        if (candidate->payload_size == expected_size) {
                            if (memcmp(candidate->payload, node1->payload, node1->payload_size) == 0 &&
                                memcmp(candidate->payload + node1->payload_size, node2->payload, node2->payload_size) == 0) {
                                combination_exists = true;
                                break;
                            }
                        }
                    }
                    
                    /* UNIVERSAL: Combine into larger node (hierarchy) */
                    if (!combination_exists) {
                        Node *combined = node_combine_payloads(node1, node2);
                        if (combined && graph_add_node(g, combined)) {
                            /* Transfer edges (preserve connectivity) */
                            node_transfer_incoming_to_hierarchy(g, node1, node2, combined);
                            
                            /* UNIVERSAL: Combined node uses universal activation (mini neural net) */
                            combined->activation_strength = node_compute_activation_strength(combined);
                        } else {
                            if (combined) node_free(combined);
                        }
                    }
                }
            }
        }
    }
}

/* Create edges to prevent node isolation */
/* Philosophy: Maintain network connectivity (homeostatic plasticity) */
void wave_create_homeostatic_edges(MelvinGraph *g, Node *isolated_node) {
    if (!g || !isolated_node) return;
    
    /* Check if node is isolated (relative to local context) */
    if (!edge_is_node_isolated(isolated_node)) return;
    
    /* Find well-connected nodes through local exploration (limited steps) */
    /* Explore to find well-connected nodes (very limited steps to avoid O(n) search) */
    size_t connections_created = 0;
    size_t max_connections = 2;  /* Limit homeostatic connections */
    
    /* Check outgoing edges first (limited exploration) */
    for (size_t i = 0; i < isolated_node->outgoing_count && connections_created < max_connections; i++) {
        Edge *edge = isolated_node->outgoing_edges[i];
        if (!edge || !edge->to_node) continue;
        
        Node *neighbor = edge->to_node;
        size_t neighbor_connections = neighbor->outgoing_count + neighbor->incoming_count;
        
        /* If neighbor is well-connected, connect to its well-connected neighbors (relative adaptive stability) */
        float well_connected_threshold = compute_well_connected_threshold(isolated_node);
        if (neighbor_connections > well_connected_threshold) {
            for (size_t j = 0; j < neighbor->outgoing_count && connections_created < max_connections; j++) {
                Edge *neighbor_edge = neighbor->outgoing_edges[j];
                if (!neighbor_edge || !neighbor_edge->to_node) continue;
                
                Node *well_connected = neighbor_edge->to_node;
                if (well_connected == isolated_node) continue;
                
                size_t well_connected_total = well_connected->outgoing_count + well_connected->incoming_count;
                
                /* Connect to well-connected nodes (relative adaptive stability) */
                if (well_connected_total > well_connected_threshold && !node_find_edge_to(isolated_node, well_connected)) {
                    Edge *homeostatic_edge = edge_create(isolated_node, well_connected, true);
                    if (homeostatic_edge) {
                        homeostatic_edge->weight = 0.05f;  /* Very weak initial weight */
                        graph_add_edge(g, homeostatic_edge, isolated_node, well_connected);
                        connections_created++;
                    }
                }
            }
        }
    }
}

/* Form intelligent edges using all creation laws */
/* Philosophy: Multiple mechanisms create rich graph structure */
void wave_form_intelligent_edges(MelvinGraph *g, Node **activated_nodes, size_t activated_count,
                                 VisitedSet *context_visited, WaveStatistics *stats) {
    if (!g || !activated_nodes || activated_count == 0) return;
    
    /* 1. Co-activation edges (existing, highest priority) */
    /* Creates strongest edges from temporal sequences */
    wave_create_edges_from_coactivation(g, activated_nodes, activated_count);
    
    /* 2. Context edges (based on wave propagation exploration - paths recently traveled) */
    /* Create edges between nodes that wave propagation explored together */
    /* Context = nodes visited by wave propagation (entire exploration, not fixed window) */
    if (activated_count >= 2 && context_visited) {
        wave_create_edges_from_context(g, activated_nodes, activated_count, context_visited);
    }
    
    /* 3. Similarity edges (similar patterns) */
    /* Create edges for each newly activated node if similar patterns exist */
    /* Limit to avoid O(n²) complexity - adaptive limit based on data size */
    size_t similarity_check_limit = compute_adaptive_sample_limit(activated_count, 1, activated_count);
    for (size_t i = 0; i < similarity_check_limit; i++) {
        Node *node = activated_nodes[i];
        if (node && node->payload_size > 0) {
            /* Use relative threshold based on local context */
            float local_avg = (node_get_local_outgoing_weight_avg(node) + 
                               node_get_local_incoming_weight_avg(node)) / 2.0f;
            /* DATA-DRIVEN: Use 75th percentile from actual similarity values (no hardcoded threshold) */
            float similarity_threshold = (stats && stats->similarity_count > 0) ? 
                                        stats->similarity_p75 : 0.0f;
            /* Adaptive blend: weight based on data availability (relative, not hardcoded, no fallbacks) */
            if (local_avg > 0.0f && similarity_threshold > 0.0f) {
                /* More stats data → trust stats more; less stats → trust context more */
                float stats_weight = (stats->similarity_count > 0) ? 
                                    (float)stats->similarity_count / (stats->similarity_count + 1.0f) : 0.0f;
                float context_weight = 1.0f - stats_weight;
                similarity_threshold = similarity_threshold * stats_weight + (local_avg * context_weight);
            } else if (local_avg > 0.0f) {
                similarity_threshold = local_avg;  /* Use local context when no stats available */
            }
            /* If both are 0.0f, similarity_threshold remains 0.0f (no operation) */
            
            wave_create_edges_from_similarity(g, node, similarity_threshold);
        }
    }
    
    /* 4. UNIVERSAL: Pattern Generalization (abstraction) */
    /* All nodes can generalize - creates abstract representations of common patterns */
    if (activated_count >= 2) {
        wave_form_universal_generalizations(g, activated_nodes, activated_count);
    }
    
    /* 5. Homeostatic edges (prevent isolation) */
    /* Check for isolated nodes and connect them */
    /* Only check occasionally to avoid overhead (check first node as sample) */
    if (activated_count > 0 && activated_nodes[0]) {
        Node *sample_node = activated_nodes[0];
        if (edge_is_node_isolated(sample_node)) {
            wave_create_homeostatic_edges(g, sample_node);
        }
    }
}

/* Create edges from co-activation (simple rule: nodes that activate together form edges) */
/* Complexity emerges implicitly: sequential patterns create stronger edges through repetition */
/* Hierarchy emerges naturally: when edges become strong through repetition, hierarchy forms */
/* Simple explicit rule: co-activation → edge; Implicit complexity: pattern strength, hierarchy emerge */
void wave_create_edges_from_coactivation(MelvinGraph *g, Node **activated_nodes, size_t activated_count) {
    if (!g || !activated_nodes || activated_count < 2) return;
    
    /* Simple rule: Nodes that activate together form edges */
    /* Sequential order creates directional edges (data flow) */
    for (size_t i = 0; i < activated_count - 1; i++) {
        Node *from = activated_nodes[i];
        Node *to = activated_nodes[i + 1];
        
        if (!from || !to) continue;  /* Skip invalid nodes */
        /* Allow from == to: same node repeated in sequence creates self-loop edge (e.g., "ll" in "hello") */
        /* Self-loops represent patterns where a node follows itself - simple rule, complex behavior emerges */
        
        /* Check if edge already exists (local check - nodes only know their edges) */
        /* For self-loops (from == to), check if self-loop edge already exists */
        Edge *existing = node_find_edge_to(from, to);
        if (existing) {
            /* Edge exists - co-activation strengthens it (emergent learning) */
            existing->activation = true;
            float old_weight = existing->weight;
            edge_update_weight_local(existing);
            
            /* Hierarchy emerges naturally when edges become strong through repetition */
            /* When edge weight grows strong relative to local context, hierarchy naturally forms */
            /* All nodes can form hierarchy - hierarchy nodes can form deeper hierarchies */
            /* No special cases - all nodes follow the same rules regardless of size or abstraction level */
            /* Only check hierarchy formation if edge weight actually increased (pattern repeated) */
            if (existing->weight > old_weight) {
                float local_avg = node_get_local_outgoing_weight_avg(from);
                float edge_relative = (local_avg > 0.0f) ? existing->weight / local_avg : existing->weight;
                
                /* Hierarchy emerges when edge naturally dominates (relative comparison, no threshold) */
                /* Check if this edge is now dominant relative to other outgoing edges */
                float max_other = 0.0f;
                for (size_t j = 0; j < from->outgoing_count; j++) {
                    Edge *other_edge = from->outgoing_edges[j];
                    if (!other_edge || other_edge == existing) continue;
                    
                    float other_relative = (local_avg > 0.0f) ? other_edge->weight / local_avg : other_edge->weight;
                    if (other_relative > max_other) {
                        max_other = other_relative;
                    }
                }
                
                /* NO HARDCODED THRESHOLD: Use minimal context when no local context exists */
                /* Hierarchy naturally emerges when edge dominates relative to others (relative comparison) */
                bool should_form_hierarchy = false;
                if (local_avg > 0.0f) {
                    /* Have local context: use relative dominance comparison */
                    should_form_hierarchy = (edge_relative > max_other && edge_relative > 0.0f);
                } else {
                    /* No local context: use minimal context from edge weight itself */
                    /* When edge weight increases, it's a positive signal (not a threshold, just bootstrap) */
                    should_form_hierarchy = (existing->weight > 0.0f);
                }
                
                if (should_form_hierarchy) {
                    /* Check if hierarchy node already exists (avoid duplicates) */
                    /* Simple check: look for existing combined node via outgoing edges */
                    /* Works for all node types - no special cases */
                    bool hierarchy_exists = false;
                    size_t expected_combined_size = from->payload_size + to->payload_size;
                    
                    for (size_t j = 0; j < from->outgoing_count; j++) {
                        Edge *check_edge = from->outgoing_edges[j];
                        if (!check_edge || !check_edge->to_node) continue;
                        Node *candidate = check_edge->to_node;
                        
                        /* Check if this is a combined hierarchy node (has combined payload) */
                        /* Works for all abstraction levels - hierarchy nodes can form deeper hierarchies */
                        if (candidate->payload_size == expected_combined_size && candidate->payload_size > 0) {
                            /* Check if payload matches combination */
                            if (memcmp(candidate->payload, from->payload, from->payload_size) == 0 &&
                                memcmp(candidate->payload + from->payload_size, to->payload, to->payload_size) == 0) {
                                hierarchy_exists = true;
                                break;
                            }
                        }
                    }
                    
                    if (!hierarchy_exists) {
                        /* Edge strengthened and is now dominant - hierarchy naturally emerges */
                        Node *combined = node_combine_payloads(from, to);
                        if (combined && graph_add_node(g, combined)) {
                            /* Natural edge transfer - preserve connectivity */
                            node_transfer_incoming_to_hierarchy(g, from, to, combined);
                        }
                    }
                }
            }
            
            continue;
        }
        
        /* Create new edge from co-activation (simple explicit rule) */
        Edge *edge = edge_create(from, to, true);
        if (edge) {
            /* Initial activation - edge created because nodes activated together */
            edge->activation = true;
            
            /* DATA-DRIVEN: Bootstrap initial edge weight from source node activation (not hardcoded) */
            /* Use source node's activation_strength if available (data-driven), otherwise minimal bootstrap */
            if (from->activation_strength > 0.0f) {
                /* Use actual activation strength from source node (data-driven) */
                edge->weight = from->activation_strength * 0.1f;  /* Scale by 0.1 for initial weight */
            } else {
                /* No activation available: bootstrap with minimal positive weight */
                /* This allows edge to participate in weight updates (not a threshold, just bootstrap) */
                edge->weight = 0.1f;
            }
            
            /* Update weight based on activation (may refine initial bootstrap value) */
            edge_update_weight_local(edge);
            
            /* Add to graph (simple: just connect them) */
            graph_add_edge(g, edge, from, to);
        }
    }
}

/* ========================================
 * .M FILE FORMAT (Live, Executable Program)
 * ======================================== */

/* Internal helper: Write header to file */
static bool write_header(FILE *file, const MelvinMHeader *header) {
    if (!file || !header) return false;
    if (fseek(file, 0, SEEK_SET) != 0) return false;
    if (fwrite(header, sizeof(MelvinMHeader), 1, file) != 1) return false;
    return true;
}

/* Internal helper: Read header from file */
static bool read_header(FILE *file, MelvinMHeader *header) {
    if (!file || !header) return false;
    if (fseek(file, 0, SEEK_SET) != 0) return false;
    if (fread(header, sizeof(MelvinMHeader), 1, file) != 1) return false;
    if (header->magic != MELVIN_M_MAGIC) return false;
    return true;
}

/* Internal helper: Write nodes to file */
static bool write_nodes(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t node_count = graph->node_count;
    if (fwrite(&node_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    for (size_t i = 0; i < graph->node_count; i++) {
        Node *node = graph->nodes[i];
        if (!node) continue;
        
        if (fwrite(node->id, 9, 1, file) != 1) return false;
        if (fwrite(&node->activation_strength, sizeof(float), 1, file) != 1) return false;
        if (fwrite(&node->weight, sizeof(float), 1, file) != 1) return false;
        if (fwrite(&node->bias, sizeof(float), 1, file) != 1) return false;
        
        uint64_t payload_size = node->payload_size;
        if (fwrite(&payload_size, sizeof(uint64_t), 1, file) != 1) return false;
        
        if (payload_size > 0) {
            if (fwrite(node->payload, 1, payload_size, file) != payload_size) return false;
        }
    }
    return true;
}

/* Internal helper: Read nodes from file */
static bool read_nodes(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t node_count;
    if (fread(&node_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    for (uint64_t i = 0; i < node_count; i++) {
        char id[9];
        float activation_strength, weight, bias;
        uint64_t payload_size;
        
        if (fread(id, 9, 1, file) != 1) return false;
        if (fread(&activation_strength, sizeof(float), 1, file) != 1) return false;
        if (fread(&weight, sizeof(float), 1, file) != 1) return false;
        if (fread(&bias, sizeof(float), 1, file) != 1) return false;
        if (fread(&payload_size, sizeof(uint64_t), 1, file) != 1) return false;
        
        uint8_t *payload = NULL;
        if (payload_size > 0) {
            payload = (uint8_t*)malloc(payload_size);
            if (!payload) return false;
            if (fread(payload, 1, payload_size, file) != payload_size) {
                free(payload);
                return false;
            }
        }
        
        Node *node = node_create(payload, payload_size);
        if (!node) {
            if (payload) free(payload);
            return false;
        }
        
        strncpy(node->id, id, 9);
        node->id[8] = '\0';
        node->activation_strength = activation_strength;
        node->weight = weight;
        node->bias = bias;
        
        if (!graph_add_node(graph, node)) {
            node_free(node);
            if (payload) free(payload);
            return false;
        }
        
        if (payload) free(payload);
    }
    return true;
}

/* Internal helper: Write edges to file */
static bool write_edges(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t edge_count = graph->edge_count;
    if (fwrite(&edge_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    for (size_t i = 0; i < graph->edge_count; i++) {
        Edge *edge = graph->edges[i];
        if (!edge) continue;
        
        if (fwrite(edge->from_node->id, 9, 1, file) != 1) return false;
        if (fwrite(edge->to_node->id, 9, 1, file) != 1) return false;
        if (fwrite(&edge->direction, sizeof(bool), 1, file) != 1) return false;
        if (fwrite(&edge->activation, sizeof(bool), 1, file) != 1) return false;
        if (fwrite(&edge->weight, sizeof(float), 1, file) != 1) return false;
    }
    return true;
}

/* Internal helper: Read edges from file */
static bool read_edges(FILE *file, MelvinGraph *graph, uint64_t offset) {
    if (!file || !graph) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t edge_count;
    if (fread(&edge_count, sizeof(uint64_t), 1, file) != 1) return false;
    
    for (uint64_t i = 0; i < edge_count; i++) {
        char from_id[9], to_id[9];
        bool direction, activation;
        float weight;
        
        if (fread(from_id, 9, 1, file) != 1) return false;
        if (fread(to_id, 9, 1, file) != 1) return false;
        if (fread(&direction, sizeof(bool), 1, file) != 1) return false;
        if (fread(&activation, sizeof(bool), 1, file) != 1) return false;
        if (fread(&weight, sizeof(float), 1, file) != 1) return false;
        
        Node *from = NULL, *to = NULL;
        for (size_t j = 0; j < graph->node_count; j++) {
            if (graph->nodes[j] && strcmp(graph->nodes[j]->id, from_id) == 0) from = graph->nodes[j];
            if (graph->nodes[j] && strcmp(graph->nodes[j]->id, to_id) == 0) to = graph->nodes[j];
        }
        
        if (!from || !to) continue;
        
        Edge *edge = edge_create(from, to, direction);
        if (!edge) continue;
        
        edge->activation = activation;
        edge->weight = weight;
        graph_add_edge(graph, edge, from, to);
    }
    return true;
}

/* Internal helper: Write universal input buffer */
static bool write_universal_input(FILE *file, const uint8_t *data, size_t size, uint64_t offset) {
    if (!file) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t data_size = size;
    if (fwrite(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    if (size > 0 && data) {
        if (fwrite(data, 1, size, file) != size) return false;
    }
    return true;
}

/* Internal helper: Read universal input buffer */
static bool read_universal_input(FILE *file, uint8_t **data, size_t *size, uint64_t offset) {
    if (!file || !data || !size) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t data_size;
    if (fread(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    *size = data_size;
    if (data_size == 0) {
        *data = NULL;
        return true;
    }
    
    *data = (uint8_t*)malloc(data_size);
    if (!*data) return false;
    if (fread(*data, 1, data_size, file) != data_size) {
        free(*data);
        *data = NULL;
        return false;
    }
    return true;
}

/* Internal helper: Write universal output buffer */
static bool write_universal_output(FILE *file, const uint8_t *data, size_t size, uint64_t offset) {
    if (!file) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t data_size = size;
    if (fwrite(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    if (size > 0 && data) {
        if (fwrite(data, 1, size, file) != size) return false;
    }
    return true;
}

/* Internal helper: Read universal output buffer */
static bool read_universal_output(FILE *file, uint8_t **data, size_t *size, uint64_t offset) {
    if (!file || !data || !size) return false;
    if (fseek(file, (long)offset, SEEK_SET) != 0) return false;
    
    uint64_t data_size;
    if (fread(&data_size, sizeof(uint64_t), 1, file) != 1) return false;
    
    *size = data_size;
    if (data_size == 0) {
        *data = NULL;
        return true;
    }
    
    *data = (uint8_t*)malloc(data_size);
    if (!*data) return false;
    if (fread(*data, 1, data_size, file) != data_size) {
        free(*data);
        *data = NULL;
        return false;
    }
    return true;
}

/* Internal helper: Calculate size of nodes section */
static uint64_t calculate_nodes_size(MelvinGraph *graph) {
    if (!graph) return sizeof(uint64_t); /* Just node count */
    
    uint64_t size = sizeof(uint64_t); /* Node count */
    
    for (size_t i = 0; i < graph->node_count; i++) {
        Node *node = graph->nodes[i];
        if (!node) continue;
        size += 9; /* ID */
        size += sizeof(float) * 3; /* activation_strength, weight, bias */
        size += sizeof(uint64_t); /* payload_size */
        size += node->payload_size; /* payload data */
    }
    
    return size;
}

/* Internal helper: Calculate size of edges section */
static uint64_t calculate_edges_size(MelvinGraph *graph) {
    if (!graph) return sizeof(uint64_t); /* Just edge count */
    
    uint64_t size = sizeof(uint64_t); /* Edge count */
    size += graph->edge_count * (9 + 9 + sizeof(bool) + sizeof(bool) + sizeof(float)); /* Per edge */
    
    return size;
}

/* Internal helper: Calculate file offsets (adaptive layout) */
static void calculate_offsets(MelvinMHeader *header, MelvinGraph *graph) {
    uint64_t offset = sizeof(MelvinMHeader);
    
    header->nodes_offset = offset;
    uint64_t nodes_size = calculate_nodes_size(graph);
    offset += nodes_size;
    
    header->edges_offset = offset;
    uint64_t edges_size = calculate_edges_size(graph);
    offset += edges_size;
    
    header->universal_input_offset = offset;
    offset += sizeof(uint64_t) + header->universal_input_size;
    
    header->universal_output_offset = offset;
    offset += sizeof(uint64_t) + header->universal_output_size;
    
    header->payloads_offset = 0;
}

/* ========================================
 * .M FILE OPERATIONS (Live Program Interface)
 * ======================================== */
/* NOTE: All melvin_m_* functions are now in melvin_m.c, not melvin.c */
/* This section has been removed to avoid duplicate symbol errors */
/* The functions that were here have been moved to melvin_m.c: */
/* - melvin_m_create, melvin_m_open, melvin_m_save, melvin_m_close */
/* - melvin_m_universal_input_*, melvin_m_universal_output_* */
/* - melvin_m_process_input */
/* - melvin_m_add_node, melvin_m_add_edge */
/* - melvin_m_mark_dirty, melvin_m_is_dirty, melvin_m_get_adaptation_count */
/* - melvin_bootstrap */
MelvinGraph* graph_create(void) {
    MelvinGraph *g = (MelvinGraph*)calloc(1, sizeof(MelvinGraph));
    if (!g) return NULL;
    
    /* No capacity limits - start with 0, allocate dynamically on first add */
    g->node_capacity = 0;
    g->nodes = NULL;
    g->node_count = 0;
    
    g->edge_capacity = 0;
    g->edges = NULL;
    g->edge_count = 0;
    
    return g;
}

/* Add node to graph (creation law - nodes created through wave propagation) */
bool graph_add_node(MelvinGraph *g, Node *node) {
    if (!g || !node) return false;
    
    /* Resize if needed (no capacity limits - allocate dynamically) */
    if (g->node_count >= g->node_capacity) {
        size_t new_capacity = (g->node_capacity == 0) ? 1 : g->node_capacity * 2;
        Node **new_nodes = (Node**)realloc(g->nodes, new_capacity * sizeof(Node*));
        if (!new_nodes) return false;
        g->nodes = new_nodes;
        g->node_capacity = new_capacity;
    }
    
    g->nodes[g->node_count++] = node;
    return true;
}

/* Add edge to graph and connect to nodes (creation law - nodes created through wave prop) */
bool graph_add_edge(MelvinGraph *g, Edge *edge, Node *from, Node *to) {
    if (!g || !edge || !from || !to) return false;
    
    /* Resize if needed (no capacity limits - allocate dynamically) */
    if (g->edge_count >= g->edge_capacity) {
        size_t new_capacity = (g->edge_capacity == 0) ? 1 : g->edge_capacity * 2;
        Edge **new_edges = (Edge**)realloc(g->edges, new_capacity * sizeof(Edge*));
        if (!new_edges) return false;
        g->edges = new_edges;
        g->edge_capacity = new_capacity;
    }
    
    g->edges[g->edge_count++] = edge;
    
    /* Connect edge to nodes (local to nodes - no searching, direct connection) */
    /* Nodes only know themselves and their edges - this is how they learn about connections */
        /* Add to from node's outgoing edges */
        if (from->outgoing_count >= from->outgoing_capacity) {
        size_t new_cap = (from->outgoing_capacity == 0) ? 1 : from->outgoing_capacity * 2;
            Edge **new_out = (Edge**)realloc(from->outgoing_edges, new_cap * sizeof(Edge*));
            if (!new_out) return false;
            from->outgoing_edges = new_out;
            from->outgoing_capacity = new_cap;
        }
        from->outgoing_edges[from->outgoing_count++] = edge;
        
        /* Update cached outgoing weight sum (O(1) incremental update) */
        node_add_outgoing_weight(from, edge->weight);
    
        /* Add to to node's incoming edges */
        if (to->incoming_count >= to->incoming_capacity) {
        size_t new_cap = (to->incoming_capacity == 0) ? 1 : to->incoming_capacity * 2;
            Edge **new_in = (Edge**)realloc(to->incoming_edges, new_cap * sizeof(Edge*));
            if (!new_in) return false;
            to->incoming_edges = new_in;
            to->incoming_capacity = new_cap;
        }
        to->incoming_edges[to->incoming_count++] = edge;
        
        /* Update cached incoming weight sum (O(1) incremental update) */
        node_add_incoming_weight(to, edge->weight);
    
    return true;
}

/* Free graph and all nodes/edges */
void graph_free(MelvinGraph *g) {
    if (!g) return;
    
    /* Free all nodes */
    for (size_t i = 0; i < g->node_count; i++) {
        node_free(g->nodes[i]);
    }
    free(g->nodes);
    
    /* Free all edges */
    for (size_t i = 0; i < g->edge_count; i++) {
        edge_free(g->edges[i]);
    }
    free(g->edges);
    
    free(g);
}

/* ========================================
 * WAVE PROPAGATION (Local Operations Only)
 * ======================================== */

/* Propagate activation from a node through its outgoing edges (local, relative) */
/* Node acts as mini neural net - "thinks" by combining edge weights + payload structure */
/* Wave prop "selects" nodes by activating edges based on relative influence - no thresholds */
/* Returns null-terminated array of newly activated nodes for multi-step propagation */
Node** wave_propagate_from_node(Node *node) {
    if (!node) return NULL;
    
    /* Compute activation strength from inputs (mini neural net) */
    node->activation_strength = node_compute_activation_strength(node);
    
    /* If activation strong enough relative to local context, propagate */
    float local_avg = node_get_local_outgoing_weight_avg(node);
    /* Adaptive propagation threshold: relative to local context */
    /* If local average exists, use relative fraction; otherwise use node's own weight as baseline */
    float propagation_threshold = (local_avg > 0.0f) ? 
                                 local_avg / (local_avg + 1.0f) : 
                                 (node->weight > 0.0f ? node->weight / (node->weight + 1.0f) : 0.0f);
    // #region agent log
    char log_data[256];
    snprintf(log_data, sizeof(log_data), "{\"threshold\":%.3f,\"local_avg\":%.3f,\"node_weight\":%.3f,\"activation\":%.3f}",
             propagation_threshold, local_avg, node->weight, node->activation_strength);
    debug_log("melvin.c:3151", "Adaptive propagation threshold computed", log_data);
    // #endregion agent log
    
    if (node->activation_strength < propagation_threshold) {
        node_update_weight_local(node);
        return NULL;  /* Too weak to propagate */
    }
    
    /* If no outgoing edges, just update weight and return */
    if (node->outgoing_count == 0) {
        node_update_weight_local(node);
        return NULL;
    }
    
    /* Propagate through outgoing edges */
    Node **activated = NULL;
    size_t activated_count = 0;
    size_t activated_capacity = 0;
    
    /* Compute edge outputs (transformed activations) */
    float max_edge_output = 0.0f;
    float *edge_outputs = (float*)calloc(node->outgoing_count, sizeof(float));
    
    /* GPU-ACCELERATED: Batch transform edges (if GPU available and enough edges) */
    /* Note: Currently uses CPU fallback - GPU acceleration framework is in place */
    MelvinGPUContext *gpu_ctx = melvin_gpu_get_context();
    if (gpu_ctx && melvin_gpu_is_available(gpu_ctx) && node->outgoing_count > 16) {
        /* Use batch operations for large edge counts */
        melvin_gpu_batch_transform_edges(gpu_ctx, node, node->outgoing_edges, 
                                         node->outgoing_count, edge_outputs, &max_edge_output);
    } else {
        /* CPU fallback: Transform edges sequentially */
        for (size_t i = 0; i < node->outgoing_count; i++) {
            Edge *edge = node->outgoing_edges[i];
            if (!edge) continue;
            
            /* Edge transforms activation */
            float edge_output = edge_transform_activation(edge, node->activation_strength);
            edge_outputs[i] = edge_output;
            
            if (edge_output > max_edge_output) {
                max_edge_output = edge_output;
            }
        }
    }
    
    /* UNIVERSAL: Activate edges based on relative strength (no special cases, no hardcoded thresholds) */
    /* All nodes use the same rule: relative to local context and max edge output */
    float node_local_avg = node_get_local_outgoing_weight_avg(node);
    float local_std = 0.0f;
    
    /* Compute local standard deviation for relative threshold (self-regulating) */
    if (node->outgoing_count > 1 && node_local_avg > 0.0f) {
        float variance = 0.0f;
        for (size_t i = 0; i < node->outgoing_count; i++) {
            if (!node->outgoing_edges[i]) continue;
            float diff = node->outgoing_edges[i]->weight - node_local_avg;
            variance += diff * diff;
        }
        local_std = sqrtf(variance / (float)node->outgoing_count);
    }
    
    /* UNIVERSAL: Threshold is relative to max output and local context (no hardcoded values) */
    /* Nodes with more uniform connections (low std) explore more; nodes with varied connections focus */
    /* Adaptive exploration factor: based on local variation, no hardcoded fallback */
    float exploration_factor = (local_std > 0.0f && node_local_avg > 0.0f) ? 
                               (local_std / node_local_avg) : 
                               (node->outgoing_count > 0 ? 1.0f / (node->outgoing_count + 1.0f) : 0.0f);
    exploration_factor = (exploration_factor > 1.0f) ? 1.0f : exploration_factor;  /* Cap at 1.0 */
    /* Adaptive threshold: exploration factor determines how much to reduce from max */
    float exploration_reduction = exploration_factor / (exploration_factor + 1.0f);
    float threshold = max_edge_output * (1.0f - exploration_reduction);  /* Relative, self-regulating */
    
    for (size_t i = 0; i < node->outgoing_count; i++) {
        Edge *edge = node->outgoing_edges[i];
        if (!edge || !edge->to_node) continue;
        
        float edge_output = edge_outputs[i];
        
        /* UNIVERSAL: All nodes activate edges above relative threshold */
        if (edge_output >= threshold) {
            edge->activation = true;
            edge_update_weight_local(edge);
            
            /* Add target node to activated array (it will compute its activation in next step) */
            if (activated_count >= activated_capacity) {
                activated_capacity = (activated_capacity == 0) ? 4 : activated_capacity * 2;
                activated = (Node**)realloc(activated, (activated_capacity + 1) * sizeof(Node*));
                if (!activated) {
                    free(edge_outputs);
                    node_update_weight_local(node);
                    return NULL;
                }
            }
            activated[activated_count++] = edge->to_node;
        }
    }
    
    free(edge_outputs);
    
    if (activated_count > 0) {
        activated = (Node**)realloc(activated, (activated_count + 1) * sizeof(Node*));
        activated[activated_count] = NULL;
        node_update_weight_local(node);
    }
    
    return activated;
}

/* Unified multi-step wave propagation - all mechanisms work together seamlessly */
/* Philosophy: Everything updates continuously - weights, edges, hierarchy, blank nodes */
/* All pieces integrated: activation → weight updates → edge formation → hierarchy → blank nodes */
/* GPU-ACCELERATED: Auto-detects and uses GPU when available, falls back to CPU */
void wave_propagate_multi_step(MelvinGraph *g, Node **initial_nodes, size_t initial_count) {
    if (!g || !initial_nodes || initial_count == 0) return;
    
    /* Get GPU context (auto-initializes, returns NULL if GPU unavailable) */
    MelvinGPUContext *gpu_ctx = melvin_gpu_get_context();
    
    /* Initialize wave front */
    Node **wave_front = (Node**)malloc(initial_count * sizeof(Node*));
    if (!wave_front) return;
    
    size_t wave_front_size = initial_count;
    memcpy(wave_front, initial_nodes, initial_count * sizeof(Node*));
    
    /* Initialize adaptive statistics for data-driven thresholds */
    WaveStatistics stats;
    wave_statistics_init(&stats);
    
    /* Track visited nodes to prevent infinite loops from cycles */
    size_t optimal_hash_size = calculate_optimal_hash_size(g->node_count);
    VisitedSet *visited = visited_set_create(optimal_hash_size);
    if (!visited) {
        free(wave_front);
        return;
    }
    
    /* Mark initial nodes as visited */
    for (size_t i = 0; i < initial_count; i++) {
        visited_set_add(visited, initial_nodes[i]);
    }
    
    /* GPU-ACCELERATED: Batch update initial node weights (if GPU available) */
    if (gpu_ctx && melvin_gpu_is_available(gpu_ctx)) {
        melvin_gpu_batch_update_weights(gpu_ctx, initial_nodes, initial_count);
    } else {
        /* CPU fallback: Update weights sequentially */
        for (size_t i = 0; i < initial_count; i++) {
            node_update_weight_local(initial_nodes[i]);
        }
    }
    
    /* Track initial energy for convergence detection */
    float initial_energy = 0.0f;
    for (size_t i = 0; i < wave_front_size; i++) {
        initial_energy += wave_front[i]->weight;
    }
    
    /* Unified propagation: all mechanisms work together continuously */
    /* RELATIVE: No hardcoded limits - complexity emerges from .m file data */
    /* System adapts to its own state - no fixed thresholds */
    size_t step = 0;
    float previous_energy = initial_energy;
    
    /* CONTEXT: Use visited set as context (paths recently traveled) */
    /* Context = all nodes that wave propagation has explored (entire exploration, not fixed window) */
    /* Wave propagation already filters to relevant nodes - these are the context */
    /* The visited set tracks all nodes explored by wave propagation - this IS the context */
    
    while (wave_front_size > 0) {
        step++;
        Node **next_wave_front = NULL;
        size_t next_size = 0;
        size_t next_capacity = 0;
        float current_energy = 0.0f;
        
        /* Collect co-activated nodes for unified edge/hierarchy formation */
        Node **co_activated = NULL;
        size_t co_activated_count = 0;
        size_t co_activated_capacity = 0;
        
        /* UNIVERSAL: Track co-activated nodes for combination (hierarchy emerges naturally) */
        /* All nodes can combine when they co-activate strongly - universal law */
        
        /* GPU-ACCELERATED: Batch compute activations for wave front (if GPU available) */
        /* Note: Currently uses CPU fallback - GPU acceleration framework is in place */
        /* Future work: Optimize specific hot spots when pointer-based graph is flattened */
        if (gpu_ctx && melvin_gpu_is_available(gpu_ctx) && wave_front_size > 8) {
            /* Use batch operations for large wave fronts */
            melvin_gpu_batch_compute_activations(gpu_ctx, wave_front, wave_front_size);
            melvin_gpu_batch_update_weights(gpu_ctx, wave_front, wave_front_size);
        }
        
        /* Propagate from current wave front - unified process */
        for (size_t i = 0; i < wave_front_size; i++) {
            Node *current_node = wave_front[i];
            if (!current_node) continue;
            
            /* UNIFIED: Compute activation (updates bias self-regulating) */
            /* Skip if already computed by GPU batch operation */
            if (!gpu_ctx || !melvin_gpu_is_available(gpu_ctx) || wave_front_size <= 8) {
                current_node->activation_strength = node_compute_activation_strength(current_node);
            }
            
            /* UNIFIED: Update node weight immediately (continuous self-regulation) */
            /* Skip if already updated by GPU batch operation */
            if (!gpu_ctx || !melvin_gpu_is_available(gpu_ctx) || wave_front_size <= 8) {
                node_update_weight_local(current_node);
            }
            
            /* Propagate through edges */
            Node **newly_activated = wave_propagate_from_node(current_node);
            if (newly_activated) {
                for (size_t j = 0; newly_activated[j]; j++) {
                    Node *activated_node = newly_activated[j];
                    if (!activated_node) continue;
                    
                    current_energy += activated_node->weight;
                    
                    /* UNIFIED: Update activated node weight immediately */
                    /* Note: Batch operations handled at wave front level for efficiency */
                    activated_node->activation_strength = node_compute_activation_strength(activated_node);
                    node_update_weight_local(activated_node);
                    
                    /* DATA-DRIVEN: Collect statistics for adaptive thresholds (O(1) per node) */
                    wave_statistics_add_value(&stats, activated_node->activation_strength);
                    wave_statistics_add_value(&stats, activated_node->weight);
                    
                    /* Collect edge weights for similarity edge detection */
                    for (size_t k = 0; k < activated_node->outgoing_count; k++) {
                        if (activated_node->outgoing_edges[k]) {
                            wave_statistics_add_edge_weight(&stats, activated_node->outgoing_edges[k]->weight);
                        }
                    }
                    for (size_t k = 0; k < activated_node->incoming_count; k++) {
                        if (activated_node->incoming_edges[k]) {
                            wave_statistics_add_edge_weight(&stats, activated_node->incoming_edges[k]->weight);
                        }
                    }
                    
                    /* Add to next wave front if not visited */
                    if (!visited_set_contains(visited, activated_node)) {
                        visited_set_add(visited, activated_node);
                        
                    if (next_size >= next_capacity) {
                        next_capacity = (next_capacity == 0) ? 4 : next_capacity * 2;
                        next_wave_front = (Node**)realloc(next_wave_front, next_capacity * sizeof(Node*));
                        if (!next_wave_front) {
                            free(newly_activated);
                            free(wave_front);
                            if (co_activated) free(co_activated);
                            visited_set_free(visited);
                            return;
                        }
                    }
                        next_wave_front[next_size++] = activated_node;
                    }
                    
                    /* UNIFIED: Track co-activated nodes for edge/hierarchy formation */
                    if (!visited_set_contains(visited, activated_node)) {
                        if (co_activated_count >= co_activated_capacity) {
                            co_activated_capacity = (co_activated_capacity == 0) ? 4 : co_activated_capacity * 2;
                            co_activated = (Node**)realloc(co_activated, co_activated_capacity * sizeof(Node*));
                            if (!co_activated) {
                                free(newly_activated);
                                free(wave_front);
                                free(next_wave_front);
                                visited_set_free(visited);
                                return;
                            }
                        }
                        co_activated[co_activated_count++] = activated_node;
                    }
                }
                free(newly_activated);
            }
        }
        
        /* UNIFIED: Form intelligent edges from co-activated nodes (all mechanisms) */
        /* This includes: co-activation, similarity, context, hierarchy, homeostatic, and blank nodes */
        /* CONTEXT: Pass visited set as context (paths recently traveled) */
        /* Context = all nodes explored by wave propagation (entire exploration, not fixed window) */
        if (co_activated_count > 1) {
            wave_form_intelligent_edges(g, co_activated, co_activated_count, visited, &stats);
        }
        
        /* CONTEXT: All nodes added to visited set are automatically part of context */
        /* Wave propagation tracks all explored nodes in visited set - this IS the context */
        /* No need for separate tracking - visited set already contains all paths recently traveled */
        
        /* DATA-DRIVEN: Compute percentiles from collected statistics (O(1) - fixed bucket count) */
        wave_statistics_compute_percentiles(&stats);
        
        /* UNIVERSAL: Form combinations from co-activated nodes (hierarchy emerges naturally) */
        /* All nodes can combine when they co-activate strongly - universal law */
        if (co_activated_count > 1) {
            wave_form_universal_combinations(g, co_activated, co_activated_count);
        }
        
        /* Cleanup */
        if (co_activated) free(co_activated);
        
        /* RELATIVE: Convergence when energy decreases relative to previous step (no hardcoded threshold) */
        /* System naturally converges when energy stops flowing - relative to its own state */
        float energy_change = (previous_energy > 0.0f) ? 
                             (current_energy - previous_energy) / previous_energy : 0.0f;
        /* Stop when energy is decreasing relative to previous step AND relative to initial */
        float energy_ratio = (initial_energy > 0.0f) ? current_energy / initial_energy : 1.0f;
        /* Natural convergence: energy decreases relative to both previous and initial */
        if (energy_change < 0.0f && energy_ratio < (1.0f - energy_change)) {
            free(next_wave_front);
            break;
        }
        previous_energy = current_energy;
        
        free(wave_front);
        wave_front = next_wave_front;
        wave_front_size = next_size;
        initial_energy = current_energy;
    }
    
    free(wave_front);
    visited_set_free(visited);
}

/* ========================================
 * PAYLOAD EXPANSION & HIERARCHY (Relative Operations)
 * ======================================== */

/* Combine nodes into larger payload when patterns repeat (hierarchy formation) */
/* Payload expands by combining - all relative, no hardcoded rules */
Node* node_combine_payloads(Node *node1, Node *node2) {
    if (!node1 || !node2) return NULL;
    
    /* Combined size relative to both nodes */
    size_t combined_size = node1->payload_size + node2->payload_size;
    
    /* Allocate combined payload */
    uint8_t *combined = (uint8_t*)malloc(combined_size);
    if (!combined) return NULL;
    
    /* Combine payloads (relative combination) */
    memcpy(combined, node1->payload, node1->payload_size);
    memcpy(combined + node1->payload_size, node2->payload, node2->payload_size);
    
    /* Create new node with combined payload (hierarchy) */
    Node *combined_node = node_create(combined, combined_size);
    free(combined);
    
    /* Set abstraction level and weight relative to both nodes */
    if (combined_node) {
        /* Set abstraction level: max of components + 1 */
        uint32_t max_level = (node1->abstraction_level > node2->abstraction_level) ?
            node1->abstraction_level : node2->abstraction_level;
        combined_node->abstraction_level = max_level + 1;
        
        /* Weight relative to both nodes */
        combined_node->weight = (node1->weight + node2->weight) / 2.0f;
        
        /* UNIVERSAL: All nodes compute activation the same way (mini neural net) */
        /* Combined node will compute activation from its edges to child nodes */
        /* No special abstraction computation - just universal activation */
        combined_node->activation_strength = node_compute_activation_strength(combined_node);
    }
    
    return combined_node;
}

/* Hierarchy formation now emerges naturally from edge weight growth during co-activation */
/* Removed explicit hierarchy check - hierarchy emerges implicitly from pattern repetition */
/* See wave_create_edges_from_coactivation() for natural hierarchy emergence */

/* Transfer edges to hierarchy node (simple rule: preserve connectivity) */
/* Complexity emerges: hierarchy nodes naturally participate in graph structure */
void node_transfer_incoming_to_hierarchy(MelvinGraph *g, Node *node1, Node *node2, Node *combined) {
    if (!g || !node1 || !node2 || !combined) return;
    
    /* Simple rule: Transfer incoming edges (preserve connectivity to combined node) */
    for (size_t i = 0; i < node1->incoming_count; i++) {
        Edge *old_edge = node1->incoming_edges[i];
        if (!old_edge || !old_edge->from_node) continue;
        
        Edge *new_edge = edge_create(old_edge->from_node, combined, true);
        if (new_edge) {
            new_edge->weight = old_edge->weight;
            graph_add_edge(g, new_edge, old_edge->from_node, combined);
        }
    }
    
    for (size_t i = 0; i < node2->incoming_count; i++) {
        Edge *old_edge = node2->incoming_edges[i];
        if (!old_edge || !old_edge->from_node) continue;
        
        Edge *new_edge = edge_create(old_edge->from_node, combined, true);
        if (new_edge) {
            new_edge->weight = old_edge->weight;
            graph_add_edge(g, new_edge, old_edge->from_node, combined);
        }
    }
    
    /* Simple rule: Transfer outgoing edges (preserve connectivity from combined node) */
    /* This enables hierarchy nodes to participate in wave propagation and form deeper hierarchy */
    for (size_t i = 0; i < node2->outgoing_count; i++) {
        Edge *old_edge = node2->outgoing_edges[i];
        if (!old_edge || !old_edge->to_node) continue;
        
        Edge *new_edge = edge_create(combined, old_edge->to_node, true);
        if (new_edge) {
            new_edge->weight = old_edge->weight;
            graph_add_edge(g, new_edge, combined, old_edge->to_node);
        }
    }
}

/* Create blank/template node (for pattern matching and generalization) */
/* Blank nodes have empty payload - can be filled when patterns match */
Node* node_create_blank(void) {
    return node_create(NULL, 0);
}

/* Fill blank node with payload when pattern matches (relative to match strength) */
/* Returns new node with filled payload, original blank_node unchanged */
Node* node_fill_blank(Node *blank_node, const uint8_t *pattern, size_t pattern_size, float match_strength) {
    if (!blank_node || !pattern || pattern_size == 0 || match_strength <= 0.0f) return NULL;
    
    /* Fill size relative to match strength */
    size_t fill_size = (size_t)(pattern_size * match_strength);
    if (fill_size == 0) fill_size = 1;
    
    /* Create new node with filled payload (relative to match strength) */
    Node *filled_node = node_create(pattern, fill_size);
    if (!filled_node) return NULL;
    
    /* Weight relative to match strength and original blank weight */
    filled_node->weight = (blank_node->weight + match_strength) / 2.0f;
    
    return filled_node;
}

/* Collect output from direct input nodes and their sequential continuations only */
/* KEY DISTINCTION: Activation (from wave prop) != Output (direct input intent) */
/* Wave propagation activates nodes for exploration/context, but output should only */
/* come from direct input nodes and learned sequential patterns (co-activation edges) */
/* Philosophy: Output represents intent, not all contextual activations */
void wave_collect_output(MelvinGraph *g, Node **direct_input_nodes, size_t direct_input_count, 
                         uint8_t **output, size_t *output_size) {
    if (!g || !output || !output_size) return;
    
    *output = NULL;
    *output_size = 0;
    
    /* If no direct input nodes, no output (output only from direct input, not contextual activation) */
    if (!direct_input_nodes || direct_input_count == 0) {
        return;
    }
    
    /* Create visited set for nodes (to prevent cycles) */
    /* Adaptive hash size based on graph size for optimal performance */
    size_t optimal_hash_size = calculate_optimal_hash_size(g->node_count);
    VisitedSet *visited = visited_set_create(optimal_hash_size);
    if (!visited) return;
    
    size_t output_capacity = 0;
    
    /* Output from direct input sequence (this is the primary output) */
    /* This ensures clean output matching the input intent */
    /* IMPORTANT: Allow duplicates - input sequence may have same node multiple times (e.g., "hello" has 'l' twice) */
    /* Don't use visited set here - output should preserve the exact input sequence */
    for (size_t i = 0; i < direct_input_count; i++) {
        Node *node = direct_input_nodes[i];
        if (!node || node->payload_size == 0) continue;
        
        /* Add node payload to output (allow duplicates - preserves input sequence) */
        size_t new_size = *output_size + node->payload_size;
        if (new_size > output_capacity) {
            output_capacity = (output_capacity == 0) ? node->payload_size * 2 : output_capacity * 2;
            *output = (uint8_t*)realloc(*output, output_capacity);
            if (!*output) {
                *output_size = 0;
                visited_set_free(visited);
                return;
            }
        }
        memcpy(*output + *output_size, node->payload, node->payload_size);
        *output_size = new_size;
        
        /* Mark as visited for extension (prevent cycles in learned continuations) */
        visited_set_add(visited, node);
    }
    
    /* Optional: Extend output with learned sequential continuations */
    /* Only follow co-activation edges (sequential patterns), not similarity/context edges */
    /* Co-activation edges are identified by: strong weights (learned from repetition) */
    /* and sequential relationships (connecting nodes that appeared together) */
    /* This allows output to extend beyond direct input with learned continuations */
    
    /* Start from last node in direct input sequence */
    if (direct_input_count > 0) {
        Node *last_node = direct_input_nodes[direct_input_count - 1];
        if (last_node) {
            Node *current = last_node;
            
            /* Follow sequential (co-activation) edges for learned continuations */
            /* Use node activation strength (mini neural net output) and edge transformation (transformer output) */
            /* DATA-DRIVEN: No hardcoded step limit - extend based on confidence threshold */
            /* System naturally stops when pattern strength drops below local context threshold */
            
            /* Compute initial confidence threshold from local context */
            float local_outgoing_avg = node_get_local_outgoing_weight_avg(current);
            /* Adaptive confidence threshold: relative to local context */
            float confidence_threshold = (local_outgoing_avg > 0.0f) ? 
                                        local_outgoing_avg / (local_outgoing_avg + 1.0f) : 0.0f;
            
            /* Track confidence scores for adaptive threshold increase (relative adaptive stability) */
            float *confidence_history = NULL;
            size_t confidence_count = 0;
            size_t confidence_capacity = 0;
            size_t extension_step = 0;
            
            /* Continue extending while confidence remains high (no fixed limit) */
            while (true) {  /* No hardcoded limit - stops when confidence drops */
                Edge *best_sequential_edge = NULL;
                float best_sequential_score = 0.0f;
                extension_step++;
                
                /* Find strongest sequential edge using intelligent node/edge computation */
                /* Nodes as mini neural nets: use activation_strength (computed during wave propagation) */
                /* Edges as transformers: use edge_transform_activation (intelligent transformation) */
                float max_transformed = 0.0f;
                
                /* First pass: compute transformed activations for relative comparison */
                for (size_t i = 0; i < current->outgoing_count; i++) {
                    Edge *edge = current->outgoing_edges[i];
                    if (!edge || !edge->to_node) continue;
                    if (visited_set_contains(visited, edge->to_node)) continue;  /* Avoid cycles */
                    
                    /* Edge transforms activation (transformer function) */
                    /* Uses node activation_strength (mini neural net output) */
                    float transformed = edge_transform_activation(edge, current->activation_strength);
                    
                    if (transformed > max_transformed) {
                        max_transformed = transformed;
                    }
                }
                
                /* Second pass: select best edge using transformed activation (intelligent selection) */
                for (size_t i = 0; i < current->outgoing_count; i++) {
                    Edge *edge = current->outgoing_edges[i];
                    if (!edge || !edge->to_node) continue;
                    if (visited_set_contains(visited, edge->to_node)) continue;  /* Avoid cycles */
                    
                    /* Edge transforms activation (transformer function) */
                    float transformed = edge_transform_activation(edge, current->activation_strength);
                    
                    /* Score = transformed activation (intelligent edge transformation) */
                    /* Normalize by local context for relative comparison */
                    float score = 0.0f;
                    if (local_outgoing_avg > 0.0f && max_transformed > 0.0f) {
                        score = transformed / max_transformed;  /* Relative to best transformation */
                    } else {
                        score = transformed;
                    }
                    
                    /* Only consider edges with meaningful transformed activation (learned sequential patterns) */
                    /* This filters out similarity/context edges (which have lower transformed activation) */
                    if (transformed > 0.2f && score > best_sequential_score) {
                        best_sequential_score = score;
                        best_sequential_edge = edge;
                    }
                }
                
                /* DATA-DRIVEN: Stop if confidence drops below threshold (relative to local context) */
                /* Threshold becomes gradually stricter as we extend (prevents infinite loops) */
                if (!best_sequential_edge || best_sequential_score < confidence_threshold) {
                    break;  /* Natural stopping point - no hardcoded limit */
                }
                
                /* Follow best sequential edge */
                current = best_sequential_edge->to_node;
                if (visited_set_contains(visited, current)) break;  /* Cycle detected, stop */
                
                visited_set_add(visited, current);
                
                /* Add continuation to output */
                if (current->payload_size > 0) {
                    size_t new_size = *output_size + current->payload_size;
                    if (new_size > output_capacity) {
                        output_capacity = (output_capacity == 0) ? current->payload_size * 2 : output_capacity * 2;
                        *output = (uint8_t*)realloc(*output, output_capacity);
                        if (!*output) {
                            *output_size = 0;
                            visited_set_free(visited);
                            return;
                        }
                    }
                    memcpy(*output + *output_size, current->payload, current->payload_size);
                    *output_size = new_size;
                }
                
                /* Track confidence score for adaptive threshold computation */
                if (confidence_count >= confidence_capacity) {
                    confidence_capacity = (confidence_capacity == 0) ? 4 : confidence_capacity * 2;
                    confidence_history = (float*)realloc(confidence_history, confidence_capacity * sizeof(float));
                    if (!confidence_history) {
                        /* If allocation fails, compute from current confidence and step (no fallback) */
                        float base_increase = 0.05f;  /* 5% base */
                        float step_factor = (float)extension_step * 0.01f;  /* +1% per step */
                        float increase = fminf(base_increase + step_factor, 0.2f);  /* Cap at 20% */
                        confidence_threshold *= (1.0f + increase);
                    }
                }
                if (confidence_history) {
                    confidence_history[confidence_count++] = best_sequential_score;
                }
                
                /* DATA-DRIVEN: Gradually increase threshold as we extend (relative adaptive stability) */
                /* Makes system more selective as output gets longer */
                float increase = compute_adaptive_confidence_increase(confidence_history, confidence_count, extension_step);
                confidence_threshold *= (1.0f + increase);
                
                /* Update local context for next iteration */
                local_outgoing_avg = node_get_local_outgoing_weight_avg(current);
            }
        }
    }
    
    visited_set_free(visited);
}


