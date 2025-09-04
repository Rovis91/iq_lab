/*
 * IQ Lab - scheduler.h: Channel Processing Scheduler
 *
 * Purpose: Manages the scheduling and coordination of channel processing
 * operations, including overlap handling and resource allocation.
 *
  *
 * Date: 2025
 *
 * Key Features:
 * - Channel overlap management
 * - Processing priority handling
 * - Resource allocation optimization
 * - Progress tracking and reporting
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct scheduler_t scheduler_t;
typedef struct channel_task_t channel_task_t;
typedef struct scheduler_config_t scheduler_config_t;

/**
 * @brief Scheduler Configuration Structure
 */
struct scheduler_config_t {
    uint32_t max_channels;       // Maximum number of channels to manage
    uint32_t max_overlap;        // Maximum allowed channel overlap
    double processing_timeout;   // Timeout for processing operations (seconds)
    bool enable_priority;        // Enable priority-based scheduling
    uint32_t buffer_pool_size;   // Size of buffer pool for temporary storage
};

/**
 * @brief Channel Processing Task
 */
struct channel_task_t {
    uint32_t channel_id;         // Unique channel identifier
    uint32_t priority;           // Processing priority (0 = highest)
    double start_time;           // Task start time (seconds)
    double duration;             // Expected processing duration (seconds)
    uint32_t input_samples;      // Number of input samples to process
    uint32_t output_samples;     // Expected output samples
    bool completed;              // Completion status
    double completion_time;      // Actual completion time
    void *user_data;             // User-defined data pointer
};

/**
 * @brief Main Scheduler Context Structure
 */
struct scheduler_t {
    scheduler_config_t config;   // Configuration parameters

    // Task management
    channel_task_t *tasks;       // Array of active tasks
    uint32_t num_tasks;          // Number of active tasks
    uint32_t max_tasks;          // Maximum number of tasks

    // Resource management
    void **buffer_pool;          // Pool of temporary buffers
    uint32_t buffer_pool_used;   // Number of buffers in use

    // Performance tracking
    uint64_t total_tasks_processed;  // Total tasks completed
    double total_processing_time;    // Total processing time
    double average_task_time;        // Average task completion time

    // State management
    bool initialized;            // Initialization flag
    double last_update_time;     // Last status update time
};

/**
 * @brief Initialize scheduler configuration with defaults
 *
 * @param config Pointer to configuration structure
 * @param max_channels Maximum number of channels to support
 * @return true on success, false on error
 */
bool scheduler_config_init(scheduler_config_t *config, uint32_t max_channels);

/**
 * @brief Validate scheduler configuration
 *
 * @param config Pointer to configuration structure
 * @return true if valid, false if invalid
 */
bool scheduler_config_validate(const scheduler_config_t *config);

/**
 * @brief Create scheduler instance
 *
 * @param config Pointer to validated configuration
 * @return Pointer to initialized scheduler, NULL on error
 */
scheduler_t *scheduler_create(const scheduler_config_t *config);

/**
 * @brief Destroy scheduler instance
 *
 * @param scheduler Pointer to scheduler instance
 */
void scheduler_destroy(scheduler_t *scheduler);

/**
 * @brief Add a processing task to the scheduler
 *
 * @param scheduler Pointer to scheduler instance
 * @param task Pointer to task description
 * @return Task ID on success, negative on error
 */
int32_t scheduler_add_task(scheduler_t *scheduler, const channel_task_t *task);

/**
 * @brief Remove a task from the scheduler
 *
 * @param scheduler Pointer to scheduler instance
 * @param task_id Task identifier
 * @return true on success, false on error
 */
bool scheduler_remove_task(scheduler_t *scheduler, uint32_t task_id);

/**
 * @brief Get next task to process (priority-based)
 *
 * @param scheduler Pointer to scheduler instance
 * @param task Pointer to receive task description
 * @return true if task available, false if no tasks pending
 */
bool scheduler_get_next_task(scheduler_t *scheduler, channel_task_t *task);

/**
 * @brief Mark a task as completed
 *
 * @param scheduler Pointer to scheduler instance
 * @param task_id Task identifier
 * @param completion_time Actual completion time
 * @return true on success, false on error
 */
bool scheduler_complete_task(scheduler_t *scheduler, uint32_t task_id, double completion_time);

/**
 * @brief Allocate a temporary buffer from the pool
 *
 * @param scheduler Pointer to scheduler instance
 * @param size Buffer size in bytes
 * @return Pointer to allocated buffer, NULL on error
 */
void *scheduler_allocate_buffer(scheduler_t *scheduler, uint32_t size);

/**
 * @brief Free a buffer back to the pool
 *
 * @param scheduler Pointer to scheduler instance
 * @param buffer Pointer to buffer to free
 * @return true on success, false on error
 */
bool scheduler_free_buffer(scheduler_t *scheduler, void *buffer);

/**
 * @brief Get scheduler statistics
 *
 * @param scheduler Pointer to scheduler instance
 * @param total_tasks Pointer to receive total tasks processed
 * @param avg_time Pointer to receive average processing time
 * @param active_tasks Pointer to receive number of active tasks
 * @return true on success, false on error
 */
bool scheduler_get_statistics(const scheduler_t *scheduler,
                             uint64_t *total_tasks, double *avg_time, uint32_t *active_tasks);

/**
 * @brief Check for channel overlap conflicts
 *
 * @param scheduler Pointer to scheduler instance
 * @param channel_id Channel to check
 * @param start_time Proposed start time
 * @param duration Proposed duration
 * @return true if no conflicts, false if overlap detected
 */
bool scheduler_check_overlap(const scheduler_t *scheduler, uint32_t channel_id,
                           double start_time, double duration);

/**
 * @brief Optimize task scheduling for minimal conflicts
 *
 * @param scheduler Pointer to scheduler instance
 * @return true on success, false on error
 */
bool scheduler_optimize_schedule(scheduler_t *scheduler);

/**
 * @brief Get scheduler status string
 *
 * @param scheduler Pointer to scheduler instance
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return true on success, false on error
 */
bool scheduler_get_status_string(const scheduler_t *scheduler,
                                char *buffer, uint32_t buffer_size);

/**
 * @brief Reset scheduler state
 *
 * @param scheduler Pointer to scheduler instance
 * @return true on success, false on error
 */
bool scheduler_reset(scheduler_t *scheduler);

#endif /* SCHEDULER_H */
