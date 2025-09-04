/*
 * IQ Lab - test_iqchan_basic.c: Basic integration tests for iqchan
 *
 * Purpose: Test iqchan channelization functionality with synthetic data
 * Validates basic channelization operations and output generation.
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
#define TEST_DURATION_S 0.1
#define TEST_NUM_SAMPLES (TEST_SAMPLE_RATE * TEST_DURATION_S)
#define TEST_FREQUENCY 500000.0
#define TEST_AMPLITUDE 0.8

/**
 * @brief Generate test IQ signal with known tone
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
        printf("  Generated test signal: %s (%llu samples, %.0f Hz)\n",
               filename, (unsigned long long)TEST_NUM_SAMPLES, TEST_FREQUENCY);
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
 * @brief Count files matching pattern in directory
 */
static int count_files_in_directory(const char *dir_path, const char *pattern) {
    (void)pattern;  // Pattern not used in simple implementation
    // Simple implementation - just check for known channel files
    int count = 0;
    char filepath[512];

    for (int i = 0; i < 16; i++) {  // Check channels 0-15
        snprintf(filepath, sizeof(filepath), "%s/channel_%02d.iq", dir_path, i);
        if (file_exists(filepath)) {
            count++;
        }
    }

    return count;
}

/**
 * @brief Test basic iqchan functionality
 */
static bool test_basic_channelization(void) {
    printf("🧪 Testing basic channelization...\n");

    const char *input_file = "test_input.iq";
    const char *output_dir = "test_channels";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    // Run iqchan with debug parameters
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqchan --in %s --format s16 --rate %d --channels 4 --bandwidth 125000 --out %s 2>&1",
             input_file, TEST_SAMPLE_RATE, output_dir);

    printf("  Executing: %s\n", command);

    int result = system(command);
    if (result != 0) {
        printf("  ❌ iqchan execution failed (exit code: %d)\n", result);
        remove(input_file);
        return false;
    }

    // Verify outputs
    int channel_count = count_files_in_directory(output_dir, "channel_*.iq");
    printf("  Found %d channel files in output directory\n", channel_count);

    bool success = (channel_count == 4);

    if (success) {
        printf("  ✅ Basic channelization test passed\n");
    } else {
        printf("  ❌ Expected 4 channels, found %d\n", channel_count);
    }

    // Cleanup
    remove(input_file);
    // Note: Not cleaning up output directory for manual inspection

    return success;
}

/**
 * @brief Test iqchan with different channel counts
 */
static bool test_channel_count_variations(void) {
    printf("🧪 Testing channel count variations...\n");

    const char *input_file = "test_variations.iq";
    const char *output_dir = "test_variations";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    bool all_passed = true;

    // Test different channel counts
    int test_counts[] = {4, 8, 16};
    int num_tests = sizeof(test_counts) / sizeof(test_counts[0]);

    for (int i = 0; i < num_tests; i++) {
        int channels = test_counts[i];
        char test_output_dir[256];
        snprintf(test_output_dir, sizeof(test_output_dir), "%s_%d", output_dir, channels);

        // Run iqchan
        char command[1024];
        snprintf(command, sizeof(command),
                 ".\\iqchan --in %s --format s16 --rate %d --channels %d --bandwidth 250000 --out %s",
                 input_file, TEST_SAMPLE_RATE, channels, test_output_dir);

        printf("  Testing %d channels...\n", channels);
        int result = system(command);

        if (result != 0) {
            printf("    ❌ %d-channel test failed (exit code: %d)\n", channels, result);
            all_passed = false;
            continue;
        }

        // Verify output
        int channel_count = count_files_in_directory(test_output_dir, "channel_*.iq");
        if (channel_count == channels) {
            printf("    ✅ %d channels: PASSED\n", channels);
        } else {
            printf("    ❌ %d channels: Expected %d, found %d\n", channels, channels, channel_count);
            all_passed = false;
        }
    }

    // Cleanup
    remove(input_file);

    if (all_passed) {
        printf("  ✅ Channel count variations test passed\n");
    } else {
        printf("  ❌ Some channel count tests failed\n");
    }

    return all_passed;
}

/**
 * @brief Test iqchan parameter validation
 */
static bool test_parameter_validation(void) {
    printf("🧪 Testing parameter validation...\n");

    const char *input_file = "test_validation.iq";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    bool validation_passed = true;

    // Test invalid channel count (too low)
    printf("  Testing invalid parameters...\n");
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqchan --in %s --format s16 --rate %d --channels 1 --bandwidth 250000 --out test_invalid 2>nul",
             input_file, TEST_SAMPLE_RATE);

    int result = system(command);
    if (result == 0) {
        printf("    ❌ Should have rejected invalid channel count\n");
        validation_passed = false;
    } else {
        printf("    ✅ Correctly rejected invalid channel count\n");
    }

    // Test invalid bandwidth
    snprintf(command, sizeof(command),
             ".\\iqchan --in %s --format s16 --rate %d --channels 4 --bandwidth 0 --out test_invalid2 2>nul",
             input_file, TEST_SAMPLE_RATE);

    result = system(command);
    if (result == 0) {
        printf("    ❌ Should have rejected invalid bandwidth\n");
        validation_passed = false;
    } else {
        printf("    ✅ Correctly rejected invalid bandwidth\n");
    }

    // Cleanup
    remove(input_file);

    if (validation_passed) {
        printf("  ✅ Parameter validation test passed\n");
    } else {
        printf("  ❌ Parameter validation test failed\n");
    }

    return validation_passed;
}

/**
 * @brief Test iqchan output file formats
 */
static bool test_output_formats(void) {
    printf("🧪 Testing output file formats...\n");

    const char *input_file = "test_formats.iq";
    const char *output_dir = "test_formats";

    // Generate test signal
    if (!generate_test_signal(input_file)) {
        printf("  ❌ Failed to generate test signal\n");
        return false;
    }

    // Run iqchan
    char command[1024];
    snprintf(command, sizeof(command),
             ".\\iqchan --in %s --format s16 --rate %d --channels 4 --bandwidth 250000 --out %s",
             input_file, TEST_SAMPLE_RATE, output_dir);

    int result = system(command);
    if (result != 0) {
        printf("  ❌ iqchan execution failed\n");
        remove(input_file);
        return false;
    }

    // Check for expected output files
    bool iq_files_exist = true;
    bool meta_files_exist = true;

    for (int i = 0; i < 4; i++) {
        char iq_file[256], meta_file[256];
        snprintf(iq_file, sizeof(iq_file), "%s/channel_%02d.iq", output_dir, i);
        snprintf(meta_file, sizeof(meta_file), "%s/channel_%02d.sigmf-meta", output_dir, i);

        if (!file_exists(iq_file)) {
            printf("    ❌ Missing IQ file: %s\n", iq_file);
            iq_files_exist = false;
        }

        if (!file_exists(meta_file)) {
            printf("    ❌ Missing metadata file: %s\n", meta_file);
            meta_files_exist = false;
        }
    }

    // Cleanup
    remove(input_file);

    if (iq_files_exist && meta_files_exist) {
        printf("  ✅ Output format test passed\n");
        return true;
    } else {
        printf("  ❌ Output format test failed\n");
        return false;
    }
}

/**
 * @brief Main test function
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("🧪 IQCHAN Basic Integration Tests\n");
    printf("================================\n\n");

    bool all_passed = true;

    // Run all tests
    all_passed &= test_basic_channelization();
    printf("\n");

    all_passed &= test_channel_count_variations();
    printf("\n");

    all_passed &= test_parameter_validation();
    printf("\n");

    all_passed &= test_output_formats();
    printf("\n");

    // Summary
    printf("================================\n");
    if (all_passed) {
        printf("🎉 All iqchan integration tests PASSED!\n\n");
        printf("✅ Basic channelization: Working\n");
        printf("✅ Channel count variations: Working\n");
        printf("✅ Parameter validation: Working\n");
        printf("✅ Output formats: Working\n\n");
    } else {
        printf("❌ Some iqchan integration tests FAILED!\n\n");
    }

    return all_passed ? 0 : 1;
}
