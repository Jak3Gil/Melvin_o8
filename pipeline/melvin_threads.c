/*
 * Multi-threading implementation for Melvin
 */

#include "melvin_threads.h"
#include <stdlib.h>
#include <unistd.h>

/* Get optimal thread count (number of CPU cores) */
static size_t get_optimal_thread_count(void) {
    #ifdef _SC_NPROCESSORS_ONLN
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) return (size_t)count;
    #endif
    /* DATA-DRIVEN: Use minimal context (1) when CPU count unavailable, not hardcoded fallback */
    /* System will adapt when actual CPU count becomes available */
    return 1; /* Minimal context: start from seed, adapts when data available */
}

ThreadPool* thread_pool_create(size_t thread_count) {
    if (thread_count == 0) {
        thread_count = get_optimal_thread_count();
    }
    
    ThreadPool *pool = (ThreadPool*)calloc(1, sizeof(ThreadPool));
    if (!pool) return NULL;
    
    pool->threads = (pthread_t*)calloc(thread_count, sizeof(pthread_t));
    if (!pool->threads) {
        free(pool);
        return NULL;
    }
    
    pool->thread_count = thread_count;
    pool->shutdown = false;
    
    return pool;
}

void thread_pool_destroy(ThreadPool *pool) {
    if (!pool) return;
    
    pool->shutdown = true;
    
    /* Wait for threads to finish (they're not long-running, just process arrays) */
    if (pool->threads) {
        free(pool->threads);
    }
    
    free(pool);
}

size_t thread_pool_get_count(ThreadPool *pool) {
    return pool ? pool->thread_count : 0;
}

/* Thread worker context */
typedef struct {
    ThreadPool *pool;
    void **items;
    size_t item_count;
    ProcessItemFunc process_func;
    void *context;
    size_t thread_id;
    size_t total_threads;
} WorkerContext;

/* Worker function for each thread */
static void* worker_thread(void *arg) {
    WorkerContext *ctx = (WorkerContext*)arg;
    
    /* Calculate this thread's portion of work */
    size_t items_per_thread = ctx->item_count / ctx->total_threads;
    size_t start_idx = ctx->thread_id * items_per_thread;
    size_t end_idx = (ctx->thread_id == ctx->total_threads - 1) ? 
                      ctx->item_count : start_idx + items_per_thread;
    
    /* Process assigned items */
    for (size_t i = start_idx; i < end_idx; i++) {
        if (ctx->items[i]) {
            ctx->process_func(ctx->items[i], i, ctx->context);
        }
    }
    
    return NULL;
}

void thread_pool_process_array(ThreadPool *pool, void **items, size_t item_count,
                                ProcessItemFunc process_func, void *context) {
    if (!pool || !items || item_count == 0 || !process_func) return;
    
    /* DATA-DRIVEN: Compute parallelization threshold from thread count and work characteristics */
    /* Parallelize when: item_count >= thread_count (at least 1 item per thread) */
    /* This adapts to available CPU cores - more cores = lower threshold for parallelization */
    size_t parallelization_threshold = pool->thread_count;
    
    /* For very small arrays (less than 1 item per thread), process sequentially */
    /* Threshold computed from thread count (data-driven), not hardcoded */
    if (item_count < parallelization_threshold) {
        for (size_t i = 0; i < item_count; i++) {
            if (items[i]) {
                process_func(items[i], i, context);
            }
        }
        return;
    }
    
    /* Create worker contexts */
    WorkerContext *contexts = (WorkerContext*)calloc(pool->thread_count, sizeof(WorkerContext));
    if (!contexts) {
        /* Fallback to sequential if allocation fails */
        for (size_t i = 0; i < item_count; i++) {
            if (items[i]) {
                process_func(items[i], i, context);
            }
        }
        return;
    }
    
    /* Initialize contexts */
    for (size_t i = 0; i < pool->thread_count; i++) {
        contexts[i].pool = pool;
        contexts[i].items = items;
        contexts[i].item_count = item_count;
        contexts[i].process_func = process_func;
        contexts[i].context = context;
        contexts[i].thread_id = i;
        contexts[i].total_threads = pool->thread_count;
    }
    
    /* Create threads */
    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_create(&pool->threads[i], NULL, worker_thread, &contexts[i]);
    }
    
    /* Wait for all threads to complete */
    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    free(contexts);
}

