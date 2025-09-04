/*
 * IQ Lab - pipeline.h: Pipeline Execution Engine
 *
 * Purpose: Execute YAML-defined processing pipelines with dependency management,
 * error handling, and progress tracking for batch IQ analysis workflows.
 *
  *
 * Date: 2025
 *
 * Key Features:
 * - Sequential/parallel pipeline execution
 * - Tool dependency management
 * - Error recovery and reporting
 * - Progress monitoring
 * - Resource management
 * - Deterministic execution for reproducibility
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "yaml_parse.h"

// Forward declarations
typedef struct pipeline_executor_t pipeline_executor_t;
typedef struct pipeline_step_result_t pipeline_step_result_t;
typedef struct pipeline_config_t pipeline_config_t;

/**
 * @brief Pipeline Execution Configuration
 */
struct pipeline_config_t {
    bool enable_parallel;           // Enable parallel execution where possible
    uint32_t max_parallel_jobs;     // Maximum number of parallel jobs
    double timeout_seconds;         // Timeout per pipeline step
    bool continue_on_error;         // Continue execution on step failures
    char log_file[256];             // Log file path
    char working_dir[256];          // Working directory for execution
};

/**
 * @brief Pipeline Step Execution Result
 */
struct pipeline_step_result_t {
    uint32_t step_index;            // Index of executed step
    char tool_name[64];             // Tool that was executed
    int exit_code;                  // Tool exit code (0 = success)
    double execution_time;          // Time taken to execute (seconds)
    time_t start_time;              // Start timestamp
    time_t end_time;                // End timestamp
    char output_files[512];         // Comma-separated list of output files
    char error_message[256];        // Error message if any
    bool success;                   // Overall success flag
};

/**
 * @brief Pipeline Execution Context
 */
struct pipeline_executor_t {
    pipeline_config_t config;       // Execution configuration

    // Pipeline data
    const yaml_document_t *document;    // YAML document to execute
    pipeline_step_result_t *results;    // Results for each step
    uint32_t result_count;           // Number of results

    // Execution state
    bool initialized;               // Initialization flag
    bool running;                   // Currently executing
    uint32_t current_step;          // Current step being executed
    double total_execution_time;    // Total time spent executing

    // Progress tracking
    uint32_t completed_steps;       // Number of completed steps
    uint32_t failed_steps;          // Number of failed steps

    // Logging
    void *log_handle;               // Log file handle
};

/**
 * @brief Initialize pipeline configuration with defaults
 *
 * @param config Pointer to configuration structure
 * @return true on success, false on error
 */
bool pipeline_config_init(pipeline_config_t *config);

/**
 * @brief Validate pipeline configuration
 *
 * @param config Pointer to configuration structure
 * @return true if valid, false if invalid
 */
bool pipeline_config_validate(const pipeline_config_t *config);

/**
 * @brief Create pipeline executor
 *
 * @param config Pointer to validated configuration
 * @param document Pointer to YAML document to execute
 * @return Pointer to initialized executor, NULL on error
 */
pipeline_executor_t *pipeline_executor_create(const pipeline_config_t *config,
                                            const yaml_document_t *document);

/**
 * @brief Destroy pipeline executor
 *
 * @param executor Pointer to executor instance
 */
void pipeline_executor_destroy(pipeline_executor_t *executor);

/**
 * @brief Execute pipeline
 *
 * @param executor Pointer to executor instance
 * @return true if all steps executed successfully, false on error
 */
bool pipeline_execute(pipeline_executor_t *executor);

/**
 * @brief Execute single pipeline step
 *
 * @param executor Pointer to executor instance
 * @param step_index Index of step to execute
 * @param result Pointer to result structure to fill
 * @return true on success, false on error
 */
bool pipeline_execute_step(pipeline_executor_t *executor, uint32_t step_index,
                          pipeline_step_result_t *result);

/**
 * @brief Build command line for pipeline step
 *
 * @param executor Pointer to executor instance
 * @param step_index Index of step to build command for
 * @param command_buffer Buffer to store command string
 * @param buffer_size Size of command buffer
 * @return true on success, false on error
 */
bool pipeline_build_command(const pipeline_executor_t *executor, uint32_t step_index,
                           char *command_buffer, uint32_t buffer_size);

/**
 * @brief Check if pipeline step can be executed in parallel
 *
 * @param executor Pointer to executor instance
 * @param step_index Index of step to check
 * @return true if can be parallelized, false if must be sequential
 */
bool pipeline_can_parallelize(const pipeline_executor_t *executor, uint32_t step_index);

/**
 * @brief Get pipeline execution progress
 *
 * @param executor Pointer to executor instance
 * @param completed Pointer to receive number of completed steps
 * @param total Pointer to receive total number of steps
 * @param percent_complete Pointer to receive completion percentage
 * @return true on success, false on error
 */
bool pipeline_get_progress(const pipeline_executor_t *executor,
                          uint32_t *completed, uint32_t *total, double *percent_complete);

/**
 * @brief Get pipeline execution statistics
 *
 * @param executor Pointer to executor instance
 * @param total_time Pointer to receive total execution time
 * @param avg_step_time Pointer to receive average step time
 * @param success_rate Pointer to receive success rate (0.0-1.0)
 * @return true on success, false on error
 */
bool pipeline_get_statistics(const pipeline_executor_t *executor,
                           double *total_time, double *avg_step_time, double *success_rate);

/**
 * @brief Generate execution summary
 *
 * @param executor Pointer to executor instance
 * @param summary_buffer Buffer to store summary text
 * @param buffer_size Size of summary buffer
 * @return true on success, false on error
 */
bool pipeline_generate_summary(const pipeline_executor_t *executor,
                             char *summary_buffer, uint32_t buffer_size);

/**
 * @brief Save pipeline results to file
 *
 * @param executor Pointer to executor instance
 * @param filename Output filename
 * @return true on success, false on error
 */
bool pipeline_save_results(const pipeline_executor_t *executor, const char *filename);

/**
 * @brief Get step result by index
 *
 * @param executor Pointer to executor instance
 * @param step_index Step index
 * @return Pointer to step result, NULL on error
 */
const pipeline_step_result_t *pipeline_get_step_result(const pipeline_executor_t *executor,
                                                     uint32_t step_index);

/**
 * @brief Check if pipeline execution is complete
 *
 * @param executor Pointer to executor instance
 * @return true if complete, false if still running
 */
bool pipeline_is_complete(const pipeline_executor_t *executor);

/**
 * @brief Reset pipeline executor for re-execution
 *
 * @param executor Pointer to executor instance
 * @return true on success, false on error
 */
bool pipeline_reset(pipeline_executor_t *executor);

#endif /* PIPELINE_H */
