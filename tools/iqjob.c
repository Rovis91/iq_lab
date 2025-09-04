/*
 * IQ Lab - iqjob: YAML Pipeline Job Runner
 *
 * Purpose: Execute complex processing pipelines defined in YAML configuration
 * files for batch processing of IQ data with reproducible results.
 *
 * Usage: iqjob --config <pipeline.yaml> --out <results_dir> [--parallel <N>] [--verbose]
 *
 * Pipeline Features:
 * - Sequential tool execution with dependency management
 * - Progress tracking and error handling
 * - Comprehensive logging and reporting
 * - Deterministic execution for reproducibility
 *
 *
 * Date: 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#include "../src/jobs/yaml_parse.h"
#include "../src/jobs/pipeline.h"

/**
 * @brief Command-line options structure
 */
typedef struct {
    const char *config_file;
    const char *output_dir;
    uint32_t max_parallel;
    bool verbose;
    bool help;
} iqjob_options_t;

/**
 * @brief Default options
 */
static const iqjob_options_t DEFAULT_OPTIONS = {
    .config_file = NULL,
    .output_dir = "iqjob_results",
    .max_parallel = 1,
    .verbose = false,
    .help = false
};

/**
 * @brief Print usage information
 */
static void print_usage(const char *program_name) {
    printf("IQ Lab - iqjob: YAML Pipeline Job Runner\n\n");
    printf("USAGE:\n");
    printf("  %s --config <pipeline.yaml> --out <results_dir> [options]\n\n", program_name);

    printf("REQUIRED ARGUMENTS:\n");
    printf("  --config <file>        YAML pipeline configuration file\n");
    printf("  --out <dir>           Output directory for results\n\n");

    printf("OPTIONAL ARGUMENTS:\n");
    printf("  --parallel <N>         Maximum number of parallel jobs (default: 1)\n");
    printf("  --verbose              Enable verbose output\n");
    printf("  --help                 Show this help message\n\n");

    printf("YAML CONFIGURATION EXAMPLE:\n");
    printf("  inputs:\n");
    printf("    - file: data/capture.iq\n");
    printf("      meta: data/capture.sigmf-meta\n");
    printf("  pipeline:\n");
    printf("    - iqls: { fft: 4096, hop: 1024, avg: 20 }\n");
    printf("    - iqdetect: { pfa: 1e-3, min_dur_ms: 50 }\n");
    printf("    - iqdemod-fm: { deemphasis_us: 50 }\n");
    printf("  report:\n");
    printf("    consolidate: { events: true, spectra: true }\n\n");

    printf("OUTPUT:\n");
    printf("  Creates timestamped results directory with:\n");
    printf("  - All tool outputs and intermediate files\n");
    printf("  - Execution log and summary report\n");
    printf("  - Consolidated artifacts as specified\n\n");
}

/**
 * @brief Parse command-line arguments
 */
static bool parse_arguments(int argc, char **argv, iqjob_options_t *options) {
    *options = DEFAULT_OPTIONS;

    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"out", required_argument, 0, 'o'},
        {"parallel", required_argument, 0, 'p'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "c:o:p:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                options->config_file = optarg;
                break;
            case 'o':
                options->output_dir = optarg;
                break;
            case 'p':
                options->max_parallel = (uint32_t)atoi(optarg);
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'h':
                options->help = true;
                return true;
            default:
                fprintf(stderr, "Unknown option. Use --help for usage.\n");
                return false;
        }
    }

    return true;
}

/**
 * @brief Validate command-line options
 */
static bool validate_options(const iqjob_options_t *options) {
    if (!options->config_file) {
        fprintf(stderr, "ERROR: Configuration file not specified. Use --config <file>\n");
        return false;
    }

    if (access(options->config_file, F_OK) != 0) {
        fprintf(stderr, "ERROR: Configuration file '%s' does not exist\n", options->config_file);
        return false;
    }

    if (options->max_parallel == 0) {
        fprintf(stderr, "ERROR: Maximum parallel jobs must be greater than 0\n");
        return false;
    }

    return true;
}

/**
 * @brief Create timestamped output directory
 */
static bool create_output_directory(const char *base_dir, char *output_dir, uint32_t buffer_size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    // Create timestamped directory name
    int written = snprintf(output_dir, buffer_size, "%s/%04d%02d%02d_%02d%02d%02d",
                          base_dir,
                          tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                          tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    if (written < 0 || (uint32_t)written >= buffer_size) {
        return false;
    }

    // Create directory
#ifdef _WIN32
    if (mkdir(output_dir) != 0) {
#else
    if (mkdir(output_dir, 0755) != 0) {
#endif
        if (errno != EEXIST) {
            fprintf(stderr, "ERROR: Failed to create output directory '%s': %s\n",
                    output_dir, strerror(errno));
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate input files exist
 */
static bool validate_inputs(const yaml_document_t *document) {
    for (uint32_t i = 0; i < document->input_count; i++) {
        const yaml_input_t *input = &document->inputs[i];

        if (access(input->file, F_OK) != 0) {
            fprintf(stderr, "ERROR: Input file '%s' does not exist\n", input->file);
            return false;
        }

        if (strlen(input->meta) > 0 && access(input->meta, F_OK) != 0) {
            fprintf(stderr, "WARNING: Metadata file '%s' does not exist\n", input->meta);
        }
    }

    return true;
}

/**
 * @brief Execute pipeline with progress reporting
 */
static bool execute_pipeline_with_progress(pipeline_executor_t *executor,
                                         const iqjob_options_t *options) {
    (void)options;  // Suppress unused parameter warning
    printf("🚀 Starting pipeline execution...\n");

    // Execute pipeline
    bool success = pipeline_execute(executor);

    // Get final statistics
    uint32_t completed, total;
    double percent_complete, total_time, avg_step_time, success_rate;

    if (pipeline_get_progress(executor, &completed, &total, &percent_complete) &&
        pipeline_get_statistics(executor, &total_time, &avg_step_time, &success_rate)) {

        printf("\n📊 Execution Summary:\n");
        printf("  Completed: %u/%u steps (%.1f%%)\n", completed, total, percent_complete);
        printf("  Success Rate: %.1f%%\n", success_rate * 100.0);
        printf("  Total Time: %.3f seconds\n", total_time);
        printf("  Average Step Time: %.3f seconds\n", avg_step_time);
    }

    if (success) {
        printf("✅ Pipeline execution completed successfully!\n");
    } else {
        printf("❌ Pipeline execution failed!\n");

        // Show failed steps
        for (uint32_t i = 0; i < executor->result_count; i++) {
            const pipeline_step_result_t *result = pipeline_get_step_result(executor, i);
            if (result && !result->success) {
                printf("  Step %u (%s): %s\n", i, result->tool_name, result->error_message);
            }
        }
    }

    return success;
}

/**
 * @brief Generate final summary report
 */
static bool generate_summary_report(pipeline_executor_t *executor,
                                  const iqjob_options_t *options,
                                  const char *output_dir) {
    char summary_file[512];
    if (strlen(output_dir) + 12 < sizeof(summary_file)) {
        snprintf(summary_file, sizeof(summary_file), "%s/summary.txt", output_dir);
    } else {
        fprintf(stderr, "ERROR: Output directory path too long for summary file\n");
        return false;
    }

    char summary_buffer[4096];
    if (!pipeline_generate_summary(executor, summary_buffer, sizeof(summary_buffer))) {
        fprintf(stderr, "WARNING: Failed to generate summary report\n");
        return false;
    }

    FILE *file = fopen(summary_file, "w");
    if (!file) {
        fprintf(stderr, "WARNING: Failed to write summary report\n");
        return false;
    }

    fprintf(file, "%s\n", summary_buffer);
    fclose(file);

    if (options->verbose) {
        printf("📝 Summary report saved to: %s\n", summary_file);
    }

    return true;
}

/**
 * @brief Save execution results
 */
static bool save_execution_results(pipeline_executor_t *executor,
                                 const iqjob_options_t *options,
                                 const char *output_dir) {
    char results_file[512];
    if (strlen(output_dir) + 12 < sizeof(results_file)) {
        snprintf(results_file, sizeof(results_file), "%s/results.csv", output_dir);
    } else {
        fprintf(stderr, "ERROR: Output directory path too long for results file\n");
        return false;
    }

    if (!pipeline_save_results(executor, results_file)) {
        fprintf(stderr, "WARNING: Failed to save execution results\n");
        return false;
    }

    if (options->verbose) {
        printf("💾 Execution results saved to: %s\n", results_file);
    }

    return true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char **argv) {
    iqjob_options_t options;

    // Parse command-line arguments
    if (!parse_arguments(argc, argv, &options)) {
        return EXIT_FAILURE;
    }

    // Handle help
    if (options.help) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    // Validate options
    if (!validate_options(&options)) {
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        printf("📋 IQ Lab iqjob - Loading configuration...\n");
        printf("  Config file: %s\n", options.config_file);
        printf("  Base output dir: %s\n", options.output_dir);
        printf("  Max parallel: %u\n", options.max_parallel);
    }

    // Load YAML configuration
    yaml_document_t document;
    if (!yaml_load_file(options.config_file, &document)) {
        fprintf(stderr, "ERROR: Failed to load configuration file '%s'\n", options.config_file);
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        printf("✅ Configuration loaded: %u inputs, %u pipeline steps\n",
               document.input_count, document.pipeline_count);

        // Debug output
        printf("  Debug: input_count=%u, pipeline_count=%u\n",
               document.input_count, document.pipeline_count);
        if (document.input_count > 0) {
            printf("  Debug: first input file='%s'\n", document.inputs[0].file);
        }
        if (document.pipeline_count > 0) {
            printf("  Debug: first pipeline tool='%s'\n", document.pipeline[0].tool_name);
        }
    }

    // Validate configuration
    if (!yaml_validate_document(&document)) {
        fprintf(stderr, "ERROR: Invalid configuration document\n");
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    // Validate input files exist
    if (!validate_inputs(&document)) {
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    // Create timestamped output directory
    char output_dir[512];
    if (!create_output_directory(options.output_dir, output_dir, sizeof(output_dir))) {
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        printf("📁 Output directory: %s\n", output_dir);
    }

    // Initialize pipeline configuration
    pipeline_config_t pipeline_config;
    if (!pipeline_config_init(&pipeline_config)) {
        fprintf(stderr, "ERROR: Failed to initialize pipeline configuration\n");
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    // Configure pipeline
    pipeline_config.enable_parallel = (options.max_parallel > 1);
    pipeline_config.max_parallel_jobs = options.max_parallel;
    pipeline_config.continue_on_error = false;  // Stop on first error
    pipeline_config.timeout_seconds = 600.0;    // 10 minute timeout per step

    // Set log file in output directory
    if (strlen(output_dir) + 13 < sizeof(pipeline_config.log_file)) {
        snprintf(pipeline_config.log_file, sizeof(pipeline_config.log_file),
                 "%s/pipeline.log", output_dir);
    } else {
        fprintf(stderr, "ERROR: Output directory path too long for log file\n");
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    // Set working directory
    strcpy(pipeline_config.working_dir, output_dir);

    // Create pipeline executor
    pipeline_executor_t *executor = pipeline_executor_create(&pipeline_config, &document);
    if (!executor) {
        fprintf(stderr, "ERROR: Failed to create pipeline executor\n");
        yaml_free_document(&document);
        return EXIT_FAILURE;
    }

    if (options.verbose) {
        printf("🔧 Pipeline executor initialized\n");
        printf("  Parallel execution: %s\n", pipeline_config.enable_parallel ? "enabled" : "disabled");
        printf("  Max parallel jobs: %u\n", pipeline_config.max_parallel_jobs);
        printf("  Log file: %s\n", pipeline_config.log_file);
    }

    // Execute pipeline
    bool execution_success = execute_pipeline_with_progress(executor, &options);

    // Generate reports
    generate_summary_report(executor, &options, output_dir);
    save_execution_results(executor, &options, output_dir);

    // Cleanup
    pipeline_executor_destroy(executor);
    yaml_free_document(&document);

    if (execution_success) {
        printf("\n🎉 iqjob completed successfully!\n");
        printf("📂 Results available in: %s\n", output_dir);
        return EXIT_SUCCESS;
    } else {
        printf("\n💥 iqjob failed!\n");
        printf("📂 Partial results available in: %s\n", output_dir);
        return EXIT_FAILURE;
    }
}
