/**
 * Circuit Playground - Thread Pool Implementation
 * Cross-platform thread pool for parallel simulation
 */

#include "threadpool.h"
#include <stdlib.h>
#include <string.h>

// Internal structure for parallel_for context
typedef struct {
    void (*func)(int index, void* context);
    void* context;
    atomic_int_t current_index;
    int end_index;
} ParallelForContext;

// Internal structure for parallel_process context
typedef struct {
    ParallelWork* work;
    atomic_int_t current_index;
} ParallelProcessContext;

#ifdef _WIN32

static DWORD WINAPI worker_thread(LPVOID arg);

int threadpool_get_optimal_threads(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int cores = sysinfo.dwNumberOfProcessors;
    // Use all available cores, but cap at MAX_THREADS
    return (cores > MAX_THREADS) ? MAX_THREADS : cores;
}

static void mutex_init(mutex_t* mutex) {
    InitializeCriticalSection(mutex);
}

static void mutex_destroy(mutex_t* mutex) {
    DeleteCriticalSection(mutex);
}

static void mutex_lock(mutex_t* mutex) {
    EnterCriticalSection(mutex);
}

static void mutex_unlock(mutex_t* mutex) {
    LeaveCriticalSection(mutex);
}

static void cond_init(cond_t* cond) {
    InitializeConditionVariable(cond);
}

static void cond_destroy(cond_t* cond) {
    // No cleanup needed for Windows condition variables
    (void)cond;
}

static void cond_wait(cond_t* cond, mutex_t* mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
}

static void cond_signal(cond_t* cond) {
    WakeConditionVariable(cond);
}

static void cond_broadcast(cond_t* cond) {
    WakeAllConditionVariable(cond);
}

static bool thread_create(thread_t* thread, void* (*func)(void*), void* arg) {
    *thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)worker_thread, arg, 0, NULL);
    return *thread != NULL;
}

static void thread_join(thread_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

#else // POSIX

static void* worker_thread(void* arg);

int threadpool_get_optimal_threads(void) {
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    return (cores > MAX_THREADS) ? MAX_THREADS : cores;
}

static void mutex_init(mutex_t* mutex) {
    pthread_mutex_init(mutex, NULL);
}

static void mutex_destroy(mutex_t* mutex) {
    pthread_mutex_destroy(mutex);
}

static void mutex_lock(mutex_t* mutex) {
    pthread_mutex_lock(mutex);
}

static void mutex_unlock(mutex_t* mutex) {
    pthread_mutex_unlock(mutex);
}

static void cond_init(cond_t* cond) {
    pthread_cond_init(cond, NULL);
}

static void cond_destroy(cond_t* cond) {
    pthread_cond_destroy(cond);
}

static void cond_wait(cond_t* cond, mutex_t* mutex) {
    pthread_cond_wait(cond, mutex);
}

static void cond_signal(cond_t* cond) {
    pthread_cond_signal(cond);
}

static void cond_broadcast(cond_t* cond) {
    pthread_cond_broadcast(cond);
}

static bool thread_create(thread_t* thread, void* (*func)(void*), void* arg) {
    return pthread_create(thread, NULL, func, arg) == 0;
}

static void thread_join(thread_t thread) {
    pthread_join(thread, NULL);
}

#endif

// Worker thread ID for thread-local identification
#ifdef _WIN32
static __declspec(thread) int tls_thread_id = -1;
#else
static __thread int tls_thread_id = -1;
#endif

// Worker thread function
#ifdef _WIN32
static DWORD WINAPI worker_thread(LPVOID arg)
#else
static void* worker_thread(void* arg)
#endif
{
    ThreadPool* pool = (ThreadPool*)arg;

    // Get thread ID from the pool (count active threads at start)
    mutex_lock(&pool->mutex);
    static int thread_counter = 0;
    tls_thread_id = thread_counter++;
    mutex_unlock(&pool->mutex);

    while (1) {
        mutex_lock(&pool->mutex);

        // Wait for a task or shutdown signal
        while (pool->queue_size == 0 && !pool->shutdown) {
            cond_wait(&pool->cond_task_available, &pool->mutex);
        }

        if (pool->shutdown && pool->queue_size == 0) {
            mutex_unlock(&pool->mutex);
            break;
        }

        // Get task from queue
        Task task = pool->task_queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % MAX_TASKS;
        pool->queue_size--;

        atomic_inc(&pool->active_tasks);
        mutex_unlock(&pool->mutex);

        // Execute task
        if (task.func) {
            task.func(task.arg, tls_thread_id);
        }

        // Mark task as done
        atomic_dec(&pool->active_tasks);
        atomic_dec(&pool->pending_tasks);

        // Signal that a task is done
        mutex_lock(&pool->mutex);
        cond_signal(&pool->cond_task_done);
        mutex_unlock(&pool->mutex);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

bool threadpool_init(ThreadPool* pool, int num_threads) {
    if (!pool) return false;

    memset(pool, 0, sizeof(ThreadPool));

    // Determine number of threads
    if (num_threads <= 0) {
        num_threads = threadpool_get_optimal_threads();
    }
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    // Ensure at least 1 thread
    if (num_threads < 1) {
        num_threads = 1;
    }

    pool->num_threads = num_threads;
    pool->queue_head = 0;
    pool->queue_tail = 0;
    pool->queue_size = 0;
    atomic_store(&pool->active_tasks, 0);
    atomic_store(&pool->pending_tasks, 0);
    pool->shutdown = false;
    pool->initialized = false;

    // Initialize synchronization primitives
    mutex_init(&pool->mutex);
    cond_init(&pool->cond_task_available);
    cond_init(&pool->cond_task_done);

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (!thread_create(&pool->threads[i], NULL, pool)) {
            // Cleanup on failure
            pool->shutdown = true;
            cond_broadcast(&pool->cond_task_available);
            for (int j = 0; j < i; j++) {
                thread_join(pool->threads[j]);
            }
            mutex_destroy(&pool->mutex);
            cond_destroy(&pool->cond_task_available);
            cond_destroy(&pool->cond_task_done);
            return false;
        }
    }

    pool->initialized = true;
    return true;
}

void threadpool_destroy(ThreadPool* pool) {
    if (!pool || !pool->initialized) return;

    // Signal shutdown
    mutex_lock(&pool->mutex);
    pool->shutdown = true;
    cond_broadcast(&pool->cond_task_available);
    mutex_unlock(&pool->mutex);

    // Wait for all threads to finish
    for (int i = 0; i < pool->num_threads; i++) {
        thread_join(pool->threads[i]);
    }

    // Cleanup
    mutex_destroy(&pool->mutex);
    cond_destroy(&pool->cond_task_available);
    cond_destroy(&pool->cond_task_done);
    pool->initialized = false;
}

bool threadpool_submit(ThreadPool* pool, task_func_t func, void* arg) {
    if (!pool || !pool->initialized || !func) return false;

    mutex_lock(&pool->mutex);

    // Check if queue is full
    if (pool->queue_size >= MAX_TASKS) {
        mutex_unlock(&pool->mutex);
        return false;
    }

    // Add task to queue
    pool->task_queue[pool->queue_tail].func = func;
    pool->task_queue[pool->queue_tail].arg = arg;
    pool->queue_tail = (pool->queue_tail + 1) % MAX_TASKS;
    pool->queue_size++;

    atomic_inc(&pool->pending_tasks);

    // Signal a worker thread
    cond_signal(&pool->cond_task_available);
    mutex_unlock(&pool->mutex);

    return true;
}

void threadpool_wait(ThreadPool* pool) {
    if (!pool || !pool->initialized) return;

    mutex_lock(&pool->mutex);
    while (atomic_load(&pool->pending_tasks) > 0 ||
           atomic_load(&pool->active_tasks) > 0) {
        cond_wait(&pool->cond_task_done, &pool->mutex);
    }
    mutex_unlock(&pool->mutex);
}

int threadpool_get_num_threads(ThreadPool* pool) {
    return pool ? pool->num_threads : 0;
}

// Parallel for worker function
static void parallel_for_worker(void* arg, int thread_id) {
    (void)thread_id;
    ParallelForContext* ctx = (ParallelForContext*)arg;

    while (1) {
        // Get next index atomically
        int index;
#ifdef _WIN32
        index = InterlockedIncrement(&ctx->current_index) - 1;
#else
        index = __atomic_fetch_add(&ctx->current_index, 1, __ATOMIC_SEQ_CST);
#endif

        if (index >= ctx->end_index) break;

        // Process this index
        ctx->func(index, ctx->context);
    }
}

void threadpool_parallel_for(ThreadPool* pool, int start, int end,
                             void (*func)(int index, void* context), void* context) {
    if (!pool || !pool->initialized || !func || start >= end) return;

    int count = end - start;

    // For small work items, just run sequentially
    if (count <= pool->num_threads || pool->num_threads <= 1) {
        for (int i = start; i < end; i++) {
            func(i, context);
        }
        return;
    }

    // Set up parallel for context
    ParallelForContext ctx;
    ctx.func = func;
    ctx.context = context;
    atomic_store(&ctx.current_index, start);
    ctx.end_index = end;

    // Submit tasks for each worker thread
    for (int i = 0; i < pool->num_threads; i++) {
        threadpool_submit(pool, parallel_for_worker, &ctx);
    }

    // Wait for all tasks to complete
    threadpool_wait(pool);
}

// Parallel process worker function
static void parallel_process_worker(void* arg, int thread_id) {
    (void)thread_id;
    ParallelProcessContext* ctx = (ParallelProcessContext*)arg;
    ParallelWork* work = ctx->work;

    while (1) {
        // Get next index atomically
        int index;
#ifdef _WIN32
        index = InterlockedIncrement(&ctx->current_index) - 1;
#else
        index = __atomic_fetch_add(&ctx->current_index, 1, __ATOMIC_SEQ_CST);
#endif

        if (index >= (int)work->count) break;

        // Calculate pointer to item
        char* item_ptr = (char*)work->data + (size_t)index * work->item_size;

        // Process this item
        work->process_item(item_ptr, index, work->context);
    }
}

void threadpool_parallel_process(ThreadPool* pool, ParallelWork* work) {
    if (!pool || !pool->initialized || !work || !work->process_item) return;

    // For small work items, just run sequentially
    if (work->count <= (size_t)pool->num_threads || pool->num_threads <= 1) {
        for (size_t i = 0; i < work->count; i++) {
            char* item_ptr = (char*)work->data + i * work->item_size;
            work->process_item(item_ptr, (int)i, work->context);
        }
        return;
    }

    // Set up parallel process context
    ParallelProcessContext ctx;
    ctx.work = work;
    atomic_store(&ctx.current_index, 0);

    // Submit tasks for each worker thread
    for (int i = 0; i < pool->num_threads; i++) {
        threadpool_submit(pool, parallel_process_worker, &ctx);
    }

    // Wait for all tasks to complete
    threadpool_wait(pool);
}
