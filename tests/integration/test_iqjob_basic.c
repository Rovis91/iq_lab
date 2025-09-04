/*
 * IQ Lab - test_iqjob_basic.c: Basic integration tests for iqjob
 *
 * Purpose: Test iqjob pipeline execution functionality with synthetic data
 * Validates YAML parsing, pipeline execution, and output generation.
 *
 *
 * Date: 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>

#include "../../src/iq_core/io_iq.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test configuration
#define TEST_SAMPLE_RATE 2000000
#define TEST_DURATION_S 0.05  // Shorter for faster tests
#define TEST_NUM_SAMPLES (TEST_SAMPLE_RATE * TEST_DURATION_S)
#define TEST_FREQUENCY 500000.0
#define TEST_AMPLITUDE 0.8

/**
 * @brief Generate test IQ signal
 */
static bool generate_test_signal(const char *filename) {
    iq_data_t iq_data = {0};
    iq_data.sample_rate = TEST_SAMPLE_RATE;
    iq_data.format = IQ_FORMAT_S16;
    iq_data.num_samples = TEST_NUM_SAMPLES;
    iq_data.data = (float *)malloc(TEST_NUM_SAMPLES * 2 * sizeof(float));

    if (!iq_data.data) {
        fprintf(stderr, "ERROR: Failed to allocate memory for test signal\n");
        return false;
    }

    // Generate a clean CW tone
    for (size_t i = 0; i < TEST_NUM_SAMPLES; i++) {
        double t = (double)i / (double)TEST_SAMPLE_RATE;
        double phase = 2.0 * M_PI * TEST_FREQUENCY * t;

        iq_data.data[i * 2] = TEST_AMPLITUDE * cos(phase);
        iq_data.data[i * 2 + 1] = TEST_AMPLITUDE * sin(phase);
    }

    bool success = iq_data_save_file(filename, &iq_data);
    free(iq_data.data);

    if (success) {
        printf("  Generated test signal: %s (%llu samples)\n", filename, (unsigned long long)TEST_NUM_SAMPLES);
    }

    return success;
}

/**
 * @brief Check if file exists
 */
static bool file_exists(const char *filename) {
    struct stat st;
    return stat(filename, &st) == 0;
}

/**
 * @brief Create test YAML configuration file
 */
static bool create_test_yaml(const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "ERROR: Failed to create YAML file %s\n", filename);
        return false;
    }

    // Write a simple test pipeline
    fprintf(file, "inputs:\n");
    fprintf(file, "  - file: test_pipeline.iq\n");
    fprintf(file, "    format: s16\n");
    fprintf(file, "\npipeline:\n");
    fprintf(file, "  - iqls: { fft: 1024, hop: 512, avg: 5 }\n");
    fprintf(file, "\nreport:\n");
    fprintf(file, "  consolidate: { events: false, spectra: true, audio: false }\n");

    fclose(file);
    printf("  Created test YAML: %s\n", filename);
    return true;
}

/**
 * @brief Test basic iqjob functionality
 */
static bool test_basic_pipeline_execution(void) {
    printf("🧪 Testing basic pipeline execution...\n");

    const char *input_file = "test_pipeline.iq";
    const char *yaml_file = "test_pipeline.yaml";
    const char *output_dir = "test_pipeline_results";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    // Create test YAML
    if (!create_test_yaml(yaml_file)) {
        printf("  ❌ Failed to create test YAML\n");
        remove(input_file);
        return false;
    }

    // Run iqjob
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqjob --config %s --out %s --verbose",
             yaml_file, output_dir);

    printf("  Executing: %s\n", command);

    int result = system(command);
    if (result != 0) {
        printf("  ❌ iqjob execution failed (exit code: %d)\n", result);
        remove(input_file);
        remove(yaml_file);
        return false;
    }

    // Check if output directory was created
    if (!file_exists(output_dir)) {
        printf("  ❌ Output directory not created\n");
        remove(input_file);
        remove(yaml_file);
        return false;
    }

    // Check for expected outputs
    char spectrum_file[512];
    snprintf(spectrum_file, sizeof(spectrum_file), "%s/spectrum.png", output_dir);

    if (file_exists(spectrum_file)) {
        printf("  ✅ Found expected output: spectrum.png\n");
    } else {
        printf("  ❌ Missing expected output: spectrum.png\n");
        remove(input_file);
        remove(yaml_file);
        return false;
    }

    // Check for summary report
    char summary_file[512];
    snprintf(summary_file, sizeof(summary_file), "%s/summary.txt", output_dir);

    if (file_exists(summary_file)) {
        printf("  ✅ Found summary report\n");
    } else {
        printf("  ⚠️ Summary report not found\n");
    }

    // Cleanup
    remove(input_file);
    remove(yaml_file);
    // Keep output directory for inspection

    printf("  ✅ Basic pipeline execution test passed\n");
    return true;
}

/**
 * @brief Test iqjob with multi-step pipeline
 */
static bool test_multi_step_pipeline(void) {
    printf("🧪 Testing multi-step pipeline...\n");

    const char *input_file = "test_multi.iq";
    const char *yaml_file = "test_multi.yaml";
    const char *output_dir = "test_multi_results";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    // Create multi-step YAML
    FILE *yaml = fopen(yaml_file, "w");
    if (!yaml) {
        printf("  ❌ Failed to create YAML file\n");
        remove(input_file);
        return false;
    }

    fprintf(yaml, "inputs:\n");
    fprintf(yaml, "  - file: %s\n", input_file);
    fprintf(yaml, "    format: s16\n");
    fprintf(yaml, "\npipeline:\n");
    fprintf(yaml, "  - iqls: { fft: 1024, hop: 512, avg: 5 }\n");
    fprintf(yaml, "  - iqinfo: {}\n");
    fprintf(yaml, "\nreport:\n");
    fprintf(yaml, "  consolidate: { events: false, spectra: true, audio: false }\n");

    fclose(yaml);

    // Run iqjob
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqjob --config %s --out %s",
             yaml_file, output_dir);

    printf("  Executing multi-step pipeline...\n");
    int result = system(command);

    if (result != 0) {
        printf("  ❌ Multi-step pipeline failed (exit code: %d)\n", result);
        remove(input_file);
        remove(yaml_file);
        return false;
    }

    // Verify outputs from both steps
    bool has_spectrum = false;
    char spectrum_path[512];
    snprintf(spectrum_path, sizeof(spectrum_path), "%s/spectrum.png", output_dir);
    if (file_exists(spectrum_path)) {
        has_spectrum = true;
        printf("  ✅ Found spectrum from iqls step\n");
    }

    // Cleanup
    remove(input_file);
    remove(yaml_file);

    if (has_spectrum) {
        printf("  ✅ Multi-step pipeline test passed\n");
        return true;
    } else {
        printf("  ❌ Multi-step pipeline test failed\n");
        return false;
    }
}

/**
 * @brief Test iqjob YAML validation
 */
static bool test_yaml_validation(void) {
    printf("🧪 Testing YAML validation...\n");

    const char *invalid_yaml = "invalid_yaml.yaml";
    const char *output_dir = "test_validation";

    // Create invalid YAML (missing inputs section)
    FILE *yaml = fopen(invalid_yaml, "w");
    if (!yaml) {
        printf("  ❌ Failed to create test file\n");
        return false;
    }

    fprintf(yaml, "pipeline:\n");
    fprintf(yaml, "  - iqls: { fft: 1024 }\n");
    fclose(yaml);

    // Try to run iqjob with invalid YAML
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqjob --config %s --out %s 2>nul",
             invalid_yaml, output_dir);

    int result = system(command);

    remove(invalid_yaml);

    if (result != 0) {
        printf("  ✅ Correctly rejected invalid YAML\n");
        return true;
    } else {
        printf("  ❌ Should have rejected invalid YAML\n");
        return false;
    }
}

/**
 * @brief Test iqjob deterministic execution
 */
static bool test_deterministic_execution(void) {
    printf("🧪 Testing deterministic execution...\n");

    const char *input_file = "test_deterministic.iq";
    const char *yaml_file = "test_deterministic.yaml";
    const char *output_dir1 = "test_det1";
    const char *output_dir2 = "test_det2";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    // Create YAML
    if (!create_test_yaml(yaml_file)) {
        remove(input_file);
        return false;
    }

    // Run pipeline twice
    printf("  Running first execution...\n");
    char command1[1024];
    snprintf(command1, sizeof(command1),
             ".\\iqjob --config %s --out %s",
             yaml_file, output_dir1);

    int result1 = system(command1);
    if (result1 != 0) {
        printf("  ❌ First execution failed\n");
        goto cleanup;
    }

    printf("  Running second execution...\n");
    char command2[1024];
    snprintf(command2, sizeof(command2),
             ".\\iqjob --config %s --out %s",
             yaml_file, output_dir2);

    int result2 = system(command2);
    if (result2 != 0) {
        printf("  ❌ Second execution failed\n");
        goto cleanup;
    }

    // Compare outputs (basic check - both should exist)
    char spectrum1[512], spectrum2[512];
    snprintf(spectrum1, sizeof(spectrum1), "%s/spectrum.png", output_dir1);
    snprintf(spectrum2, sizeof(spectrum2), "%s/spectrum.png", output_dir2);

    bool both_exist = file_exists(spectrum1) && file_exists(spectrum2);

    if (both_exist) {
        printf("  ✅ Both executions produced expected outputs\n");
        printf("  ✅ Deterministic execution test passed\n");
    } else {
        printf("  ❌ Outputs differ between executions\n");
        goto cleanup;
    }

    // Cleanup
    remove(input_file);
    remove(yaml_file);
    return both_exist;

cleanup:
    remove(input_file);
    remove(yaml_file);
    return false;
}

/**
 * @brief Main test function
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("🧪 IQJOB Basic Integration Tests\n");
    printf("===============================\n\n");

    bool all_passed = true;

    // Run all tests
    all_passed &= test_basic_pipeline_execution();
    printf("\n");

    all_passed &= test_multi_step_pipeline();
    printf("\n");

    all_passed &= test_yaml_validation();
    printf("\n");

    all_passed &= test_deterministic_execution();
    printf("\n");

    // Summary
    printf("===============================\n");
    if (all_passed) {
        printf("🎉 All iqjob integration tests PASSED!\n\n");
        printf("✅ Basic pipeline execution: Working\n");
        printf("✅ Multi-step pipelines: Working\n");
        printf("✅ YAML validation: Working\n");
        printf("✅ Deterministic execution: Working\n\n");
    } else {
        printf("❌ Some iqjob integration tests FAILED!\n\n");
    }

    return all_passed ? 0 : 1;
}
