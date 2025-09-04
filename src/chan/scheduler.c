/*
 * IQ Lab - scheduler.c: Channel Processing Scheduler Implementation
 *
 * Purpose: Manages scheduling and coordination of channel processing tasks
 * with overlap handling, priority management, and resource optimization.
 *
 *
 * Date: 2025
 */

#include "scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Initialize scheduler configuration with defaults
 */
bool scheduler_config_init(scheduler_config_t *config, uint32_t max_channels) {
    if (!config || max_channels == 0) {
        return false;
    }

    config->max_channels = max_channels;
    config->max_overlap = max_channels / 4;  // Allow 25% overlap
    config->processing_timeout = 30.0;       // 30 second timeout
    config->enable_priority = true;
    config->buffer_pool_size = 1024 * 1024;  // 1MB buffer pool

    return true;
}

/**
 * @brief Validate scheduler configuration
 */
bool scheduler_config_validate(const scheduler_config_t *config) {
    if (!config) return false;

    if (config->max_channels == 0) return false;
    if (config->max_overlap > config->max_channels) return false;
    if (config->processing_timeout <= 0.0) return false;
    if (config->buffer_pool_size == 0) return false;

    return true;
}

/**
 * @brief Create scheduler instance
 */
scheduler_t *scheduler_create(const scheduler_config_t *config) {
    if (!scheduler_config_validate(config)) {
        return NULL;
    }

    scheduler_t *scheduler = (scheduler_t *)malloc(sizeof(scheduler_t));
    if (!scheduler) return NULL;

    // Copy configuration
    memcpy(&scheduler->config, config, sizeof(scheduler_config_t));

    // Initialize task management
    scheduler->max_tasks = config->max_channels * 2;  // Allow 2 tasks per channel
    scheduler->tasks = (channel_task_t *)malloc(scheduler->max_tasks * sizeof(channel_task_t));
    if (!scheduler->tasks) {
        free(scheduler);
        return NULL;
    }

    scheduler->num_tasks = 0;

    // Initialize buffer pool
    scheduler->buffer_pool = (void **)malloc(config->buffer_pool_size * sizeof(void *));
    if (!scheduler->buffer_pool) {
        free(scheduler->tasks);
        free(scheduler);
        return NULL;
    }

    scheduler->buffer_pool_used = 0;

    // Initialize performance tracking
    scheduler->total_tasks_processed = 0;
    scheduler->total_processing_time = 0.0;
    scheduler->average_task_time = 0.0;

    // Initialize state
    scheduler->initialized = true;
    scheduler->last_update_time = 0.0;  // Will be set on first use

    return scheduler;
}

/**
 * @brief Destroy scheduler instance
 */
void scheduler_destroy(scheduler_t *scheduler) {
    if (!scheduler) return;

    // Free buffer pool
    if (scheduler->buffer_pool) {
        // Note: In a real implementation, you'd track and free individual buffers
        free(scheduler->buffer_pool);
    }

    // Free task array
    free(scheduler->tasks);

    free(scheduler);
}

/**
 * @brief Add a processing task to the scheduler
 */
int32_t scheduler_add_task(scheduler_t *scheduler, const channel_task_t *task) {
    if (!scheduler || !scheduler->initialized || !task) {
        return -1;
    }

    if (scheduler->num_tasks >= scheduler->max_tasks) {
        return -2;  // No space for new tasks
    }

    // Check for overlap conflicts
    if (!scheduler_check_overlap(scheduler, task->channel_id,
                                task->start_time, task->duration)) {
        return -3;  // Overlap conflict
    }

    // Find available slot
    uint32_t task_id = scheduler->num_tasks;
    memcpy(&scheduler->tasks[task_id], task, sizeof(channel_task_t));
    scheduler->tasks[task_id].completed = false;
    scheduler->tasks[task_id].completion_time = 0.0;

    scheduler->num_tasks++;

    return task_id;
}

/**
 * @brief Remove a task from the scheduler
 */
bool scheduler_remove_task(scheduler_t *scheduler, uint32_t task_id) {
    if (!scheduler || !scheduler->initialized || task_id >= scheduler->num_tasks) {
        return false;
    }

    // Remove task by shifting remaining tasks
    for (uint32_t i = task_id; i < scheduler->num_tasks - 1; i++) {
        memcpy(&scheduler->tasks[i], &scheduler->tasks[i + 1], sizeof(channel_task_t));
    }

    scheduler->num_tasks--;
    return true;
}

/**
 * @brief Get next task to process (priority-based)
 */
bool scheduler_get_next_task(scheduler_t *scheduler, channel_task_t *task) {
    if (!scheduler || !scheduler->initialized || !task || scheduler->num_tasks == 0) {
        return false;
    }

    // Find highest priority incomplete task
    uint32_t best_task = (uint32_t)-1;
    uint32_t best_priority = (uint32_t)-1;

    for (uint32_t i = 0; i < scheduler->num_tasks; i++) {
        if (!scheduler->tasks[i].completed &&
            scheduler->tasks[i].priority < best_priority) {
            best_task = i;
            best_priority = scheduler->tasks[i].priority;
        }
    }

    if (best_task == (uint32_t)-1) {
        return false;  // No incomplete tasks
    }

    memcpy(task, &scheduler->tasks[best_task], sizeof(channel_task_t));
    return true;
}

/**
 * @brief Mark a task as completed
 */
bool scheduler_complete_task(scheduler_t *scheduler, uint32_t task_id, double completion_time) {
    if (!scheduler || !scheduler->initialized || task_id >= scheduler->num_tasks) {
        return false;
    }

    if (scheduler->tasks[task_id].completed) {
        return false;  // Already completed
    }

    scheduler->tasks[task_id].completed = true;
    scheduler->tasks[task_id].completion_time = completion_time;

    // Update performance statistics
    double task_time = completion_time - scheduler->tasks[task_id].start_time;
    scheduler->total_tasks_processed++;
    scheduler->total_processing_time += task_time;
    scheduler->average_task_time = scheduler->total_processing_time / scheduler->total_tasks_processed;

    return true;
}

/**
 * @brief Allocate a temporary buffer from the pool
 */
void *scheduler_allocate_buffer(scheduler_t *scheduler, uint32_t size) {
    if (!scheduler || !scheduler->initialized || size == 0) {
        return NULL;
    }

    if (scheduler->buffer_pool_used >= scheduler->config.buffer_pool_size) {
        return NULL;  // No buffers available
    }

    // Simplified allocation - in practice you'd manage a proper pool
    void *buffer = malloc(size);
    if (!buffer) return NULL;

    // Track buffer (simplified)
    scheduler->buffer_pool[scheduler->buffer_pool_used] = buffer;
    scheduler->buffer_pool_used++;

    return buffer;
}

/**
 * @brief Free a buffer back to the pool
 */
bool scheduler_free_buffer(scheduler_t *scheduler, void *buffer) {
    if (!scheduler || !scheduler->initialized || !buffer) {
        return false;
    }

    // Find buffer in pool and free it
    for (uint32_t i = 0; i < scheduler->buffer_pool_used; i++) {
        if (scheduler->buffer_pool[i] == buffer) {
            free(buffer);
            // Remove from pool by shifting
            for (uint32_t j = i; j < scheduler->buffer_pool_used - 1; j++) {
                scheduler->buffer_pool[j] = scheduler->buffer_pool[j + 1];
            }
            scheduler->buffer_pool_used--;
            return true;
        }
    }

    return false;  // Buffer not found in pool
}

/**
 * @brief Get scheduler statistics
 */
bool scheduler_get_statistics(const scheduler_t *scheduler,
                             uint64_t *total_tasks, double *avg_time, uint32_t *active_tasks) {
    if (!scheduler || !scheduler->initialized) {
        return false;
    }

    if (total_tasks) *total_tasks = scheduler->total_tasks_processed;
    if (avg_time) *avg_time = scheduler->average_task_time;
    if (active_tasks) {
        uint32_t active = 0;
        for (uint32_t i = 0; i < scheduler->num_tasks; i++) {
            if (!scheduler->tasks[i].completed) active++;
        }
        *active_tasks = active;
    }

    return true;
}

/**
 * @brief Check for channel overlap conflicts
 */
bool scheduler_check_overlap(const scheduler_t *scheduler, uint32_t channel_id,
                           double start_time, double duration) {
    if (!scheduler || !scheduler->initialized) {
        return false;
    }

    double end_time = start_time + duration;

    // Check all active tasks for this channel
    for (uint32_t i = 0; i < scheduler->num_tasks; i++) {
        if (scheduler->tasks[i].channel_id == channel_id && !scheduler->tasks[i].completed) {
            double task_end = scheduler->tasks[i].start_time + scheduler->tasks[i].duration;

            // Check for time overlap
            if (!(end_time <= scheduler->tasks[i].start_time || start_time >= task_end)) {
                return false;  // Overlap detected
            }
        }
    }

    return true;  // No overlap
}

/**
 * @brief Optimize task scheduling for minimal conflicts
 */
bool scheduler_optimize_schedule(scheduler_t *scheduler) {
    if (!scheduler || !scheduler->initialized) {
        return false;
    }

    // Simple bubble sort by priority and start time
    for (uint32_t i = 0; i < scheduler->num_tasks - 1; i++) {
        for (uint32_t j = 0; j < scheduler->num_tasks - i - 1; j++) {
            bool should_swap = false;

            // Sort by priority first (lower number = higher priority)
            if (scheduler->tasks[j].priority > scheduler->tasks[j + 1].priority) {
                should_swap = true;
            }
            // Then by start time for same priority
            else if (scheduler->tasks[j].priority == scheduler->tasks[j + 1].priority &&
                     scheduler->tasks[j].start_time > scheduler->tasks[j + 1].start_time) {
                should_swap = true;
            }

            if (should_swap) {
                channel_task_t temp;
                memcpy(&temp, &scheduler->tasks[j], sizeof(channel_task_t));
                memcpy(&scheduler->tasks[j], &scheduler->tasks[j + 1], sizeof(channel_task_t));
                memcpy(&scheduler->tasks[j + 1], &temp, sizeof(channel_task_t));
            }
        }
    }

    return true;
}

/**
 * @brief Get scheduler status string
 */
bool scheduler_get_status_string(const scheduler_t *scheduler,
                                char *buffer, uint32_t buffer_size) {
    if (!scheduler || !buffer || buffer_size == 0) {
        return false;
    }

    uint32_t active_tasks = 0;
    for (uint32_t i = 0; i < scheduler->num_tasks; i++) {
        if (!scheduler->tasks[i].completed) active_tasks++;
    }

    int written = snprintf(buffer, buffer_size,
                          "Scheduler: %u/%u active tasks, %llu total processed, "
                          "%.3f avg time",
                          active_tasks, scheduler->num_tasks,
                          scheduler->total_tasks_processed,
                          scheduler->average_task_time);

    return written > 0 && (uint32_t)written < buffer_size;
}

/**
 * @brief Reset scheduler state
 */
bool scheduler_reset(scheduler_t *scheduler) {
    if (!scheduler || !scheduler->initialized) {
        return false;
    }

    // Clear all tasks
    scheduler->num_tasks = 0;

    // Free all buffers in pool
    for (uint32_t i = 0; i < scheduler->buffer_pool_used; i++) {
        free(scheduler->buffer_pool[i]);
    }
    scheduler->buffer_pool_used = 0;

    // Reset statistics
    scheduler->total_tasks_processed = 0;
    scheduler->total_processing_time = 0.0;
    scheduler->average_task_time = 0.0;
    scheduler->last_update_time = 0.0;

    return true;
}
