/*
 * IQ Lab - pipeline.c: Pipeline Execution Engine Implementation
 *
 * Purpose: Execute YAML-defined processing pipelines with proper dependency
 * management, error handling, and progress tracking for reproducible batch
 * processing of IQ data.
 *
 * Execution Strategy:
 * 1. Parse YAML pipeline definition
 * 2. Validate dependencies and inputs
 * 3. Execute steps sequentially (with optional parallelism)
 * 4. Track progress and handle errors
 * 5. Generate execution summary and results
 *
 *
 * Date: 2025
 */

#include "pipeline.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Initialize pipeline configuration with defaults
 */
bool pipeline_config_init(pipeline_config_t *config) {
    if (!config) return false;

    config->enable_parallel = false;        // Sequential execution by default
    config->max_parallel_jobs = 2;          // Allow 2 parallel jobs max
    config->timeout_seconds = 300.0;        // 5 minute timeout per step
    config->continue_on_error = false;      // Stop on first error
    strcpy(config->log_file, "pipeline.log");
    strcpy(config->working_dir, ".");

    return true;
}

/**
 * @brief Validate pipeline configuration
 */
bool pipeline_config_validate(const pipeline_config_t *config) {
    if (!config) return false;

    if (config->max_parallel_jobs == 0) return false;
    if (config->timeout_seconds <= 0.0) return false;
    if (strlen(config->working_dir) == 0) return false;

    return true;
}

/**
 * @brief Create pipeline executor
 */
pipeline_executor_t *pipeline_executor_create(const pipeline_config_t *config,
                                            const yaml_document_t *document) {
    if (!pipeline_config_validate(config) || !document) {
        return NULL;
    }

    pipeline_executor_t *executor = (pipeline_executor_t *)malloc(sizeof(pipeline_executor_t));
    if (!executor) return NULL;

    // Copy configuration
    memcpy(&executor->config, config, sizeof(pipeline_config_t));

    // Set document reference
    executor->document = document;

    // Allocate results array
    executor->result_count = document->pipeline_count;
    executor->results = (pipeline_step_result_t *)malloc(
        executor->result_count * sizeof(pipeline_step_result_t));
    if (!executor->results) {
        free(executor);
        return NULL;
    }

    // Initialize results
    memset(executor->results, 0, executor->result_count * sizeof(pipeline_step_result_t));

    // Initialize state
    executor->initialized = true;
    executor->running = false;
    executor->current_step = 0;
    executor->total_execution_time = 0.0;
    executor->completed_steps = 0;
    executor->failed_steps = 0;

    // Initialize logging
    executor->log_handle = NULL;
    if (strlen(config->log_file) > 0) {
        executor->log_handle = (void *)fopen(config->log_file, "w");
    }

    return executor;
}

/**
 * @brief Destroy pipeline executor
 */
void pipeline_executor_destroy(pipeline_executor_t *executor) {
    if (!executor) return;

    // Close log file
    if (executor->log_handle) {
        fclose((FILE *)executor->log_handle);
    }

    // Free results array
    free(executor->results);

    free(executor);
}

/**
 * @brief Execute pipeline
 */
bool pipeline_execute(pipeline_executor_t *executor) {
    if (!executor || !executor->initialized || executor->running) {
        return false;
    }

    executor->running = true;
    bool overall_success = true;

    // Log execution start
    if (executor->log_handle) {
        time_t now = time(NULL);
        fprintf((FILE *)executor->log_handle, "[%s] Starting pipeline execution\n",
                ctime(&now));
        fflush((FILE *)executor->log_handle);
    }

    // Execute each step
    for (uint32_t i = 0; i < executor->result_count; i++) {
        executor->current_step = i;

        if (!pipeline_execute_step(executor, i, &executor->results[i])) {
            executor->failed_steps++;
            overall_success = false;

            if (!executor->config.continue_on_error) {
                break; // Stop on first error
            }
        } else {
            executor->completed_steps++;
        }

        // Update total execution time
        executor->total_execution_time += executor->results[i].execution_time;
    }

    executor->running = false;

    // Log execution completion
    if (executor->log_handle) {
        time_t now = time(NULL);
        fprintf((FILE *)executor->log_handle, "[%s] Pipeline execution %s\n",
                ctime(&now), overall_success ? "completed successfully" : "failed");
        fflush((FILE *)executor->log_handle);
    }

    return overall_success;
}

/**
 * @brief Execute single pipeline step
 */
bool pipeline_execute_step(pipeline_executor_t *executor, uint32_t step_index,
                          pipeline_step_result_t *result) {
    if (!executor || !result || step_index >= executor->result_count) {
        return false;
    }

    // Initialize result
    memset(result, 0, sizeof(pipeline_step_result_t));
    result->step_index = step_index;
    result->start_time = time(NULL);

    const yaml_pipeline_step_t *step = &executor->document->pipeline[step_index];
    strcpy(result->tool_name, step->tool_name);

    // Build command
    char command[2048];
    if (!pipeline_build_command(executor, step_index, command, sizeof(command))) {
        strcpy(result->error_message, "Failed to build command");
        result->end_time = time(NULL);
        result->execution_time = difftime(result->end_time, result->start_time);
        return false;
    }

    // Log command execution
    if (executor->log_handle) {
        fprintf((FILE *)executor->log_handle, "[STEP %u] Executing: %s\n",
                step_index, command);
        fflush((FILE *)executor->log_handle);
    }

    // Execute command
    clock_t start_clock = clock();
    int exit_code = system(command);
    clock_t end_clock = clock();

    result->end_time = time(NULL);
    result->execution_time = (double)(end_clock - start_clock) / CLOCKS_PER_SEC;
    result->exit_code = exit_code;

    // Check execution result
    if (exit_code == 0) {
        result->success = true;

        // Try to identify output files (basic implementation)
        // In a real implementation, this would parse tool output
        if (strstr(step->tool_name, "iqls") != NULL) {
            strcpy(result->output_files, "spectrum.png,waterfall.png");
        } else if (strstr(step->tool_name, "iqdetect") != NULL) {
            strcpy(result->output_files, "events.csv");
        } else if (strstr(step->tool_name, "iqdemod") != NULL) {
            strcpy(result->output_files, "audio.wav");
        }

        if (executor->log_handle) {
            fprintf((FILE *)executor->log_handle, "[STEP %u] SUCCESS (%.3fs)\n",
                    step_index, result->execution_time);
        }
    } else {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                "Command failed with exit code %d", exit_code);

        if (executor->log_handle) {
            fprintf((FILE *)executor->log_handle, "[STEP %u] FAILED (%.3fs): %s\n",
                    step_index, result->execution_time, result->error_message);
        }
    }

    return result->success;
}

/**
 * @brief Build command line for pipeline step
 */
bool pipeline_build_command(const pipeline_executor_t *executor, uint32_t step_index,
                           char *command_buffer, uint32_t buffer_size) {
    if (!executor || !command_buffer || step_index >= executor->result_count) {
        return false;
    }

    const yaml_pipeline_step_t *step = &executor->document->pipeline[step_index];

    // Start with tool name
    int written = snprintf(command_buffer, buffer_size, ".\\%s.exe", step->tool_name);
    if (written < 0 || (uint32_t)written >= buffer_size) return false;

    // Add parameters
    for (uint32_t i = 0; i < step->param_count; i++) {
        const char *key = step->params[i].key;
        const char *value = step->params[i].value;

        int param_written = snprintf(command_buffer + written,
                                   buffer_size - written,
                                   " --%s %s", key, value);
        if (param_written < 0 || (uint32_t)(written + param_written) >= buffer_size) {
            return false;
        }
        written += param_written;
    }

    return true;
}

/**
 * @brief Check if pipeline step can be executed in parallel
 */
bool pipeline_can_parallelize(const pipeline_executor_t *executor, uint32_t step_index) {
    if (!executor || step_index >= executor->result_count) {
        return false;
    }

    // Simple heuristic: certain tools can be parallelized if they don't depend on previous results
    const yaml_pipeline_step_t *step = &executor->document->pipeline[step_index];
    const char *tool_name = step->tool_name;

    // Tools that typically can be parallelized
    if (strcmp(tool_name, "iqls") == 0 ||
        strcmp(tool_name, "iqinfo") == 0) {
        return executor->config.enable_parallel;
    }

    // Tools that should be sequential (depend on previous results)
    if (strcmp(tool_name, "iqdetect") == 0 ||
        strcmp(tool_name, "iqdemod-fm") == 0 ||
        strcmp(tool_name, "iqdemod-am") == 0 ||
        strcmp(tool_name, "iqdemod-ssb") == 0) {
        return false;
    }

    // Default: assume sequential for safety
    return false;
}

/**
 * @brief Get pipeline execution progress
 */
bool pipeline_get_progress(const pipeline_executor_t *executor,
                          uint32_t *completed, uint32_t *total, double *percent_complete) {
    if (!executor || !completed || !total || !percent_complete) {
        return false;
    }

    *completed = executor->completed_steps;
    *total = executor->result_count;

    if (*total > 0) {
        *percent_complete = (double)*completed / *total * 100.0;
    } else {
        *percent_complete = 0.0;
    }

    return true;
}

/**
 * @brief Get pipeline execution statistics
 */
bool pipeline_get_statistics(const pipeline_executor_t *executor,
                           double *total_time, double *avg_step_time, double *success_rate) {
    if (!executor || !total_time || !avg_step_time || !success_rate) {
        return false;
    }

    *total_time = executor->total_execution_time;

    uint32_t total_steps = executor->completed_steps + executor->failed_steps;
    if (total_steps > 0) {
        *avg_step_time = *total_time / total_steps;
    } else {
        *avg_step_time = 0.0;
    }

    if (executor->result_count > 0) {
        *success_rate = (double)executor->completed_steps / executor->result_count;
    } else {
        *success_rate = 0.0;
    }

    return true;
}

/**
 * @brief Generate execution summary
 */
bool pipeline_generate_summary(const pipeline_executor_t *executor,
                             char *summary_buffer, uint32_t buffer_size) {
    if (!executor || !summary_buffer || buffer_size == 0) {
        return false;
    }

    uint32_t completed, total;
    double percent_complete, total_time, avg_step_time, success_rate;

    if (!pipeline_get_progress(executor, &completed, &total, &percent_complete) ||
        !pipeline_get_statistics(executor, &total_time, &avg_step_time, &success_rate)) {
        return false;
    }

    int written = snprintf(summary_buffer, buffer_size,
                          "Pipeline Execution Summary\n"
                          "==========================\n"
                          "Total Steps: %u\n"
                          "Completed: %u\n"
                          "Failed: %u\n"
                          "Success Rate: %.1f%%\n"
                          "Total Time: %.3f seconds\n"
                          "Average Step Time: %.3f seconds\n"
                          "Completion: %.1f%%\n",
                          total, completed, executor->failed_steps,
                          success_rate * 100.0, total_time, avg_step_time,
                          percent_complete);

    if (written < 0 || (uint32_t)written >= buffer_size) {
        return false;
    }

    // Add step details
    for (uint32_t i = 0; i < executor->result_count && (uint32_t)written < buffer_size - 100; i++) {
        const pipeline_step_result_t *result = &executor->results[i];

        int step_written = snprintf(summary_buffer + written,
                                  buffer_size - written,
                                  "\nStep %u: %s\n"
                                  "  Status: %s\n"
                                  "  Time: %.3f seconds\n",
                                  i, result->tool_name,
                                  result->success ? "SUCCESS" : "FAILED",
                                  result->execution_time);

        if (step_written < 0) break;
        written += step_written;

        if (!result->success && strlen(result->error_message) > 0 &&
            (uint32_t)written < buffer_size - 50) {
            step_written = snprintf(summary_buffer + written,
                                  buffer_size - written,
                                  "  Error: %s\n",
                                  result->error_message);
            if (step_written > 0) written += step_written;
        }
    }

    return true;
}

/**
 * @brief Save pipeline results to file
 */
bool pipeline_save_results(const pipeline_executor_t *executor, const char *filename) {
    if (!executor || !filename) {
        return false;
    }

    FILE *file = fopen(filename, "w");
    if (!file) {
        return false;
    }

    // Write header
    fprintf(file, "# IQ Lab Pipeline Results\n");
    fprintf(file, "# Generated: %s", ctime(&(time_t){time(NULL)}));
    fprintf(file, "# Total steps: %u\n", executor->result_count);
    fprintf(file, "# Completed: %u\n", executor->completed_steps);
    fprintf(file, "# Failed: %u\n", executor->failed_steps);
    fprintf(file, "# Total time: %.3f seconds\n", executor->total_execution_time);
    fprintf(file, "\n");

    // Write CSV header
    fprintf(file, "step_index,tool_name,success,exit_code,execution_time,start_time,end_time,output_files,error_message\n");

    // Write results
    for (uint32_t i = 0; i < executor->result_count; i++) {
        const pipeline_step_result_t *result = &executor->results[i];

        fprintf(file, "%u,%s,%s,%d,%.3f,%lld,%lld,\"%s\",\"%s\"\n",
                result->step_index,
                result->tool_name,
                result->success ? "true" : "false",
                result->exit_code,
                result->execution_time,
                (long long)result->start_time,
                (long long)result->end_time,
                result->output_files,
                result->error_message);
    }

    fclose(file);
    return true;
}

/**
 * @brief Get step result by index
 */
const pipeline_step_result_t *pipeline_get_step_result(const pipeline_executor_t *executor,
                                                     uint32_t step_index) {
    if (!executor || step_index >= executor->result_count) {
        return NULL;
    }

    return &executor->results[step_index];
}

/**
 * @brief Check if pipeline execution is complete
 */
bool pipeline_is_complete(const pipeline_executor_t *executor) {
    if (!executor) return true;

    return !executor->running && (executor->completed_steps + executor->failed_steps >= executor->result_count);
}

/**
 * @brief Reset pipeline executor for re-execution
 */
bool pipeline_reset(pipeline_executor_t *executor) {
    if (!executor || !executor->initialized) {
        return false;
    }

    // Reset state
    executor->running = false;
    executor->current_step = 0;
    executor->total_execution_time = 0.0;
    executor->completed_steps = 0;
    executor->failed_steps = 0;

    // Reset results
    memset(executor->results, 0, executor->result_count * sizeof(pipeline_step_result_t));

    return true;
}
