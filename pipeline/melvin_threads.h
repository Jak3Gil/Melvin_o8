/*
 * Multi-threading support for Melvin
 * Parallelizes independent operations to utilize all CPU cores
 */

#ifndef MELVIN_THREADS_H
#define MELVIN_THREADS_H

#include <pthread.h>
#include <stddef.h>
#include <stdbool.h>

/* Thread pool for parallel processing */
typedef struct ThreadPool {
    pthread_t *threads;
    size_t thread_count;
    bool shutdown;
} ThreadPool;

/* Initialize thread pool with specified number of threads */
ThreadPool* thread_pool_create(size_t thread_count);

/* Destroy thread pool */
void thread_pool_destroy(ThreadPool *pool);

/* Get number of threads in pool */
size_t thread_pool_get_count(ThreadPool *pool);

/* Process array of items in parallel using thread pool */
/* Each thread processes items[start_idx] to items[end_idx] */
typedef void (*ProcessItemFunc)(void *item, size_t index, void *context);
void thread_pool_process_array(ThreadPool *pool, void **items, size_t item_count,
                                ProcessItemFunc process_func, void *context);

#endif /* MELVIN_THREADS_H */

