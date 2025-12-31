/*
 * Melvin GPU Acceleration Module - CPU Fallback Implementation
 * 
 * This file provides CPU fallback implementations when CUDA is not available.
 * When CUDA is available, melvin_gpu_cuda.cu provides the actual GPU implementations.
 */

#include "melvin_gpu.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Static GPU context singleton */
static MelvinGPUContext *g_gpu_context = NULL;

/* Forward declarations for CPU fallback implementations */
static void cpu_batch_compute_activations(Node **nodes, size_t node_count);
static void cpu_batch_transform_edges(Node *from_node, Edge **edges, size_t edge_count,
                                       float *edge_outputs, float *max_output);
static void cpu_batch_compute_statistics(Node **nodes, size_t node_count,
                                         float *local_avgs_out, float *local_stds_out);
static void cpu_batch_update_weights(Node **nodes, size_t node_count);

/* CUDA initialization function (defined in melvin_gpu_cuda.cu if CUDA available) */
#ifdef CUDA_AVAILABLE
extern bool melvin_gpu_cuda_init(MelvinGPUContext *ctx);
#else
/* Stub when CUDA not available */
static bool melvin_gpu_cuda_init(MelvinGPUContext *ctx) {
    (void)ctx;  /* Unused parameter */
    return false;
}
#endif

/* Initialize GPU context - auto-detects CUDA if available */
bool melvin_gpu_init(MelvinGPUContext **ctx) {
    if (!ctx) return false;
    
    if (g_gpu_context) {
        *ctx = g_gpu_context;
        return g_gpu_context->gpu_available;
    }
    
    MelvinGPUContext *new_ctx = (MelvinGPUContext*)calloc(1, sizeof(MelvinGPUContext));
    if (!new_ctx) return false;
    
    /* Try to initialize CUDA - if CUDA is available and linked, this will succeed */
    bool cuda_success = false;
    
#ifdef CUDA_AVAILABLE
    /* CUDA module is linked - try to initialize */
    cuda_success = melvin_gpu_cuda_init(new_ctx);
#endif
    
    if (!cuda_success) {
        /* CPU fallback mode - GPU not available */
        new_ctx->cuda_available = false;
        new_ctx->gpu_available = false;
        new_ctx->device_count = 0;
        new_ctx->current_device = -1;
        new_ctx->total_memory = 0;
        new_ctx->free_memory = 0;
    }
    
    g_gpu_context = new_ctx;
    *ctx = new_ctx;
    
    return new_ctx->gpu_available;
}

/* Cleanup GPU context */
void melvin_gpu_cleanup(MelvinGPUContext *ctx) {
    if (!ctx || ctx != g_gpu_context) return;
    
    free(ctx);
    g_gpu_context = NULL;
}

/* Check if GPU is available */
bool melvin_gpu_is_available(MelvinGPUContext *ctx) {
    return ctx && ctx->gpu_available;
}

/* Get GPU context singleton */
MelvinGPUContext* melvin_gpu_get_context(void) {
    if (!g_gpu_context) {
        MelvinGPUContext *ctx = NULL;
        melvin_gpu_init(&ctx);
    }
    return g_gpu_context;
}

/* CPU fallback: Batch compute node activations */
static void cpu_batch_compute_activations(Node **nodes, size_t node_count) {
    if (!nodes || node_count == 0) return;
    
    /* Process nodes sequentially (CPU fallback) */
    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i]) {
            nodes[i]->activation_strength = node_compute_activation_strength(nodes[i]);
        }
    }
}

/* CPU fallback: Batch transform edges */
static void cpu_batch_transform_edges(Node *from_node, Edge **edges, size_t edge_count,
                                       float *edge_outputs, float *max_output) {
    if (!from_node || !edges || !edge_outputs || edge_count == 0) {
        if (max_output) *max_output = 0.0f;
        return;
    }
    
    float max = 0.0f;
    float activation = from_node->activation_strength;
    
    /* Process edges sequentially (CPU fallback) */
    for (size_t i = 0; i < edge_count; i++) {
        if (!edges[i]) {
            edge_outputs[i] = 0.0f;
            continue;
        }
        
        float output = edge_transform_activation(edges[i], activation);
        edge_outputs[i] = output;
        
        if (output > max) {
            max = output;
        }
    }
    
    if (max_output) *max_output = max;
}

/* CPU fallback: Batch compute local statistics */
static void cpu_batch_compute_statistics(Node **nodes, size_t node_count,
                                         float *local_avgs_out, float *local_stds_out) {
    if (!nodes || !local_avgs_out || node_count == 0) return;
    
    /* Process nodes sequentially (CPU fallback) */
    for (size_t i = 0; i < node_count; i++) {
        if (!nodes[i]) {
            if (local_avgs_out) local_avgs_out[i] = 0.0f;
            if (local_stds_out) local_stds_out[i] = 0.0f;
            continue;
        }
        
        /* Compute local averages */
        float outgoing_avg = node_get_local_outgoing_weight_avg(nodes[i]);
        float incoming_avg = node_get_local_incoming_weight_avg(nodes[i]);
        float avg = (outgoing_avg + incoming_avg) / 2.0f;
        if (local_avgs_out) local_avgs_out[i] = avg;
        
        /* Compute local standard deviation for outgoing edges */
        float local_std = 0.0f;
        if (nodes[i]->outgoing_count > 1 && outgoing_avg > 0.0f) {
            float variance = 0.0f;
            for (size_t j = 0; j < nodes[i]->outgoing_count; j++) {
                if (!nodes[i]->outgoing_edges[j]) continue;
                float diff = nodes[i]->outgoing_edges[j]->weight - outgoing_avg;
                variance += diff * diff;
            }
            local_std = sqrtf(variance / (float)nodes[i]->outgoing_count);
        }
        if (local_stds_out) local_stds_out[i] = local_std;
    }
}

/* CPU fallback: Batch update node weights */
static void cpu_batch_update_weights(Node **nodes, size_t node_count) {
    if (!nodes || node_count == 0) return;
    
    /* Process nodes sequentially (CPU fallback) */
    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i]) {
            node_update_weight_local(nodes[i]);
        }
    }
}

/* GPU-accelerated operations with CPU fallback */
void melvin_gpu_batch_compute_activations(MelvinGPUContext *ctx, Node **nodes, size_t node_count) {
    if (!ctx || !nodes || node_count == 0) return;
    
    /* Always use CPU implementation for now */
    /* Full GPU implementation would require flattening the pointer-based graph structure */
    /* This is a framework - future work can optimize specific hot spots */
    cpu_batch_compute_activations(nodes, node_count);
}

void melvin_gpu_batch_transform_edges(MelvinGPUContext *ctx, Node *from_node, 
                                       Edge **edges, size_t edge_count,
                                       float *edge_outputs, float *max_output) {
    if (!ctx || !from_node || !edges || !edge_outputs || edge_count == 0) {
        if (max_output) *max_output = 0.0f;
        return;
    }
    
    /* Always use CPU implementation for now */
    /* Full GPU implementation would require flattening the pointer-based graph structure */
    cpu_batch_transform_edges(from_node, edges, edge_count, edge_outputs, max_output);
}

void melvin_gpu_batch_compute_statistics(MelvinGPUContext *ctx, Node **nodes, size_t node_count,
                                         float *local_avgs_out, float *local_stds_out) {
    if (!ctx || !nodes || !local_avgs_out || node_count == 0) return;
    
    /* Always use CPU implementation for now */
    /* Full GPU implementation would require flattening the pointer-based graph structure */
    cpu_batch_compute_statistics(nodes, node_count, local_avgs_out, local_stds_out);
}

void melvin_gpu_batch_update_weights(MelvinGPUContext *ctx, Node **nodes, size_t node_count) {
    if (!ctx || !nodes || node_count == 0) return;
    
    /* Always use CPU implementation for now */
    /* Full GPU implementation would require flattening the pointer-based graph structure */
    cpu_batch_update_weights(nodes, node_count);
}

/* GPU memory management */
size_t melvin_gpu_get_memory_used(MelvinGPUContext *ctx) {
    if (!ctx || !ctx->gpu_available) return 0;
    return ctx->total_memory - ctx->free_memory;
}

size_t melvin_gpu_get_memory_free(MelvinGPUContext *ctx) {
    if (!ctx || !ctx->gpu_available) return 0;
    return ctx->free_memory;
}

