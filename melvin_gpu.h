/*
 * Melvin GPU Acceleration Module
 * 
 * Hybrid CPU/GPU system that auto-detects available compute resources.
 * .m files automatically use GPU acceleration when available, falling back to CPU otherwise.
 * 
 * Hardware-aware optimization: Systems optimize themselves with available compute.
 */

#ifndef MELVIN_GPU_H
#define MELVIN_GPU_H

#include "melvin.h"
#include <stdbool.h>
#include <stddef.h>

/* GPU Context - manages GPU state and auto-detection */
typedef struct MelvinGPUContext {
    bool gpu_available;      /* True if GPU was successfully initialized */
    bool cuda_available;     /* True if CUDA is available */
    int device_count;        /* Number of CUDA devices available */
    int current_device;      /* Current CUDA device ID */
    size_t total_memory;     /* Total GPU memory in bytes */
    size_t free_memory;      /* Free GPU memory in bytes */
} MelvinGPUContext;

/* ========================================
 * GPU INITIALIZATION & DETECTION
 * ======================================== */

/* Initialize GPU context - auto-detects available hardware */
/* Returns true if GPU acceleration is available, false for CPU fallback */
bool melvin_gpu_init(MelvinGPUContext **ctx);

/* Cleanup GPU context */
void melvin_gpu_cleanup(MelvinGPUContext *ctx);

/* Check if GPU is available and ready */
bool melvin_gpu_is_available(MelvinGPUContext *ctx);

/* Get GPU context singleton (auto-initializes on first call) */
MelvinGPUContext* melvin_gpu_get_context(void);

/* ========================================
 * GPU-ACCELERATED OPERATIONS (with CPU fallback)
 * ======================================== */

/* Batch compute node activations (GPU if available, CPU fallback) */
/* Processes multiple nodes in parallel for efficient wave propagation */
void melvin_gpu_batch_compute_activations(MelvinGPUContext *ctx, Node **nodes, size_t node_count);

/* Batch transform edges (GPU if available, CPU fallback) */
/* Processes multiple edge transformations in parallel */
void melvin_gpu_batch_transform_edges(MelvinGPUContext *ctx, Node *from_node, 
                                       Edge **edges, size_t edge_count, 
                                       float *edge_outputs, float *max_output);

/* Batch compute local statistics (GPU if available, CPU fallback) */
/* Computes local averages, standard deviations for adaptive thresholds */
void melvin_gpu_batch_compute_statistics(MelvinGPUContext *ctx, Node **nodes, size_t node_count,
                                         float *local_avgs_out, float *local_stds_out);

/* Batch update node weights (GPU if available, CPU fallback) */
/* Updates weights using adaptive learning rates in parallel */
void melvin_gpu_batch_update_weights(MelvinGPUContext *ctx, Node **nodes, size_t node_count);

/* ========================================
 * GPU MEMORY MANAGEMENT
 * ======================================== */

/* Get current GPU memory usage */
size_t melvin_gpu_get_memory_used(MelvinGPUContext *ctx);

/* Get available GPU memory */
size_t melvin_gpu_get_memory_free(MelvinGPUContext *ctx);

#endif /* MELVIN_GPU_H */

