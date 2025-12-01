/**
 * Circuit Playground - Thread Pool for Parallel Simulation
 * Cross-platform thread pool supporting Windows and POSIX systems
 */

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE cond_t;
typedef LONG atomic_int_t;
#else
#include <pthread.h>
typedef pthread_t thread_t;
typedef pthread_mutex_t mutex_t;
typedef pthread_cond_t cond_t;
typedef volatile int atomic_int_t;
#endif

// Maximum number of worker threads
#define MAX_THREADS 32

// Maximum number of queued tasks
#define MAX_TASKS 1024

// Task function type
typedef void (*task_func_t)(void* arg, int thread_id);

// Task structure
typedef struct {
    task_func_t func;
    void* arg;
} Task;

// Thread pool structure
typedef struct {
    thread_t threads[MAX_THREADS];
    int num_threads;

    Task task_queue[MAX_TASKS];
    int queue_head;
    int queue_tail;
    int queue_size;

    mutex_t mutex;
    cond_t cond_task_available;
    cond_t cond_task_done;

    atomic_int_t active_tasks;
    atomic_int_t pending_tasks;

    bool shutdown;
    bool initialized;
} ThreadPool;

// Parallel work item for batch processing
typedef struct {
    void* data;             // Pointer to data array
    size_t count;           // Number of items
    size_t item_size;       // Size of each item
    void (*process_item)(void* item, int index, void* context);
    void* context;          // User context passed to process_item
} ParallelWork;

// Initialize thread pool with specified number of threads (0 = auto-detect)
bool threadpool_init(ThreadPool* pool, int num_threads);

// Destroy thread pool
void threadpool_destroy(ThreadPool* pool);

// Submit a task to the thread pool
bool threadpool_submit(ThreadPool* pool, task_func_t func, void* arg);

// Wait for all submitted tasks to complete
void threadpool_wait(ThreadPool* pool);

// Get the number of worker threads
int threadpool_get_num_threads(ThreadPool* pool);

// Parallel for loop - distributes work across threads
void threadpool_parallel_for(ThreadPool* pool, int start, int end,
                             void (*func)(int index, void* context), void* context);

// Parallel process items in an array
void threadpool_parallel_process(ThreadPool* pool, ParallelWork* work);

// Get optimal thread count for the system
int threadpool_get_optimal_threads(void);

// Atomic operations
static inline int atomic_load(atomic_int_t* val) {
#ifdef _WIN32
    return InterlockedCompareExchange(val, 0, 0);
#else
    return __atomic_load_n(val, __ATOMIC_SEQ_CST);
#endif
}

static inline void atomic_store(atomic_int_t* val, int new_val) {
#ifdef _WIN32
    InterlockedExchange(val, new_val);
#else
    __atomic_store_n(val, new_val, __ATOMIC_SEQ_CST);
#endif
}

static inline int atomic_inc(atomic_int_t* val) {
#ifdef _WIN32
    return InterlockedIncrement(val);
#else
    return __atomic_add_fetch(val, 1, __ATOMIC_SEQ_CST);
#endif
}

static inline int atomic_dec(atomic_int_t* val) {
#ifdef _WIN32
    return InterlockedDecrement(val);
#else
    return __atomic_sub_fetch(val, 1, __ATOMIC_SEQ_CST);
#endif
}

#endif // THREADPOOL_H
