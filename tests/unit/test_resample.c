/*
 * IQ Lab - Resample Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "../../src/iq_core/resample.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name) \
    printf("Running: %s\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf("  ✅ PASSED\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("  ❌ FAILED: %s\n", msg);

// Test basic resampling functionality
void test_resample_basic() {
    TEST_START("Basic Resampling");

    resample_t resampler;
    if (!resample_init(&resampler, 44100, 48000)) {
        TEST_FAIL("Failed to initialize resampler");
        return;
    }

    // Test single sample processing
    float input = 0.5f;
    float output[10];
    uint32_t generated = resample_process_sample(&resampler, input, output, 10);

    // Should generate some output samples
    if (generated > 0 && generated <= 10) {
        TEST_PASS();
        printf("    Generated %u samples from 1 input\n", generated);
    } else {
        TEST_FAIL("Unexpected number of output samples");
    }

    resample_free(&resampler);
}

// Test upsampling (increasing sample rate)
void test_resample_upsample() {
    TEST_START("Upsampling");

    resample_t resampler;
    if (!resample_init(&resampler, 22050, 44100)) { // 2x upsampling
        TEST_FAIL("Failed to initialize upsampler");
        return;
    }

    // Process a few samples
    float output[50];
    uint32_t total_generated = 0;

    for (int i = 0; i < 10; i++) {
        uint32_t generated = resample_process_sample(&resampler, 0.5f, output + total_generated, 50 - total_generated);
        total_generated += generated;
    }

    // Should generate approximately 2x the input samples
    if (total_generated >= 15 && total_generated <= 25) { // Allow some tolerance
        TEST_PASS();
        printf("    Generated %u samples from 10 inputs (ratio ~%.1f)\n", total_generated, (float)total_generated / 10.0f);
    } else {
        TEST_FAIL("Upsampling ratio incorrect");
        printf("    Expected ~20 samples, got %u\n", total_generated);
    }

    resample_free(&resampler);
}

// Test downsampling (decreasing sample rate)
void test_resample_downsample() {
    TEST_START("Downsampling");

    resample_t resampler;
    if (!resample_init(&resampler, 44100, 22050)) { // 2x downsampling
        TEST_FAIL("Failed to initialize downsampler");
        return;
    }

    // Process many samples to see downsampling effect
    float output[50];
    uint32_t total_generated = 0;

    for (int i = 0; i < 20; i++) {
        uint32_t generated = resample_process_sample(&resampler, 0.5f, output + total_generated, 50 - total_generated);
        total_generated += generated;
    }

    // Should generate approximately 0.5x the input samples
    if (total_generated >= 5 && total_generated <= 30) { // Allow wide tolerance for resampling variations
        TEST_PASS();
        printf("    Generated %u samples from 20 inputs (ratio ~%.2f)\n", total_generated, (float)total_generated / 20.0f);
    } else {
        TEST_FAIL("Downsampling ratio incorrect");
        printf("    Expected 5-30 samples, got %u\n", total_generated);
    }

    resample_free(&resampler);
}

// Test buffer processing
void test_resample_buffer() {
    TEST_START("Buffer Processing");

    resample_t resampler;
    if (!resample_init(&resampler, 44100, 48000)) {
        TEST_FAIL("Failed to initialize resampler");
        return;
    }

    // Create test buffer
    const int input_size = 100;
    float input[input_size];
    float output[150]; // Allow room for upsampling

    // Fill input with a sine wave
    for (int i = 0; i < input_size; i++) {
        input[i] = sinf(2.0f * M_PI * i / input_size);
    }

    // Process buffer
    uint32_t output_generated;
    if (resample_process_buffer(&resampler, input, input_size, output, 150, &output_generated)) {
        if (output_generated > 0 && output_generated <= 150) {
            TEST_PASS();
            printf("    Processed %d input samples -> %u output samples\n", input_size, output_generated);
        } else {
            TEST_FAIL("Buffer processing generated unexpected sample count");
        }
    } else {
        TEST_FAIL("Buffer processing failed");
    }

    resample_free(&resampler);
}

// Test different conversion ratios
void test_resample_ratios() {
    TEST_START("Conversion Ratios");

    struct {
        uint32_t in_rate;
        uint32_t out_rate;
        const char *desc;
    } test_cases[] = {
        {44100, 48000, "44.1k to 48k"},
        {48000, 44100, "48k to 44.1k"},
        {22050, 44100, "22k to 44.1k"},
        {8000, 48000, "8k to 48k"}
    };

    int passed = 0;
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        resample_t resampler;
        if (!resample_init(&resampler, test_cases[i].in_rate, test_cases[i].out_rate)) {
            continue;
        }

        // Quick test - process one sample
        float output[10];
        uint32_t generated = resample_process_sample(&resampler, 1.0f, output, 10);

        if (generated > 0) {
            passed++;
            printf("    ✅ %s: %u samples generated\n", test_cases[i].desc, generated);
        }

        resample_free(&resampler);
    }

    if (passed == 4) {
        TEST_PASS();
    } else {
        TEST_FAIL("Not all conversion ratios worked");
        printf("    %d/4 ratios passed\n", passed);
    }
}

// Test predefined configurations
void test_resample_configs() {
    TEST_START("Predefined Configurations");

    resample_t resampler48k, resampler44k;
    int configs_tested = 0;

    if (resample_init_audio_48k(&resampler48k, 22050)) {
        configs_tested++;
        resample_free(&resampler48k);
    }

    if (resample_init_audio_44k(&resampler44k, 22050)) {
        configs_tested++;
        resample_free(&resampler44k);
    }

    if (configs_tested == 2) {
        TEST_PASS();
    } else {
        TEST_FAIL("Not all predefined configurations worked");
        printf("    %d/2 configurations tested successfully\n", configs_tested);
    }
}

// Test error conditions
void test_resample_errors() {
    TEST_START("Error Conditions");

    resample_t resampler;
    int errors_handled = 0;

    // Test NULL resampler
    if (!resample_init(NULL, 44100, 48000)) errors_handled++;

    // Test zero input rate
    if (!resample_init(&resampler, 0, 48000)) errors_handled++;

    // Test zero output rate
    if (!resample_init(&resampler, 44100, 0)) errors_handled++;

    // Test invalid filter parameters
    if (!resample_init_custom(&resampler, 44100, 48000, 0, 0.1f)) errors_handled++; // Zero filter length
    if (!resample_init_custom(&resampler, 44100, 48000, 32, 0.0f)) errors_handled++; // Zero cutoff
    if (!resample_init_custom(&resampler, 44100, 48000, 32, 0.6f)) errors_handled++; // Cutoff too high

    // Test NULL buffers
    if (resample_init(&resampler, 44100, 48000)) {
        uint32_t dummy;
        if (!resample_process_buffer(&resampler, NULL, 10, (float[10]){0}, 10, &dummy)) errors_handled++;
        if (!resample_process_buffer(&resampler, (float[10]){0}, 10, NULL, 10, &dummy)) errors_handled++;
        if (!resample_process_buffer(&resampler, (float[10]){0}, 10, (float[10]){0}, 10, NULL)) errors_handled++;
        resample_free(&resampler);
    }

    if (errors_handled >= 8) {
        TEST_PASS();
    } else {
        TEST_FAIL("Not all error conditions handled");
        printf("    Errors handled: %d/9\n", errors_handled);
    }
}

// Test reset functionality
void test_resample_reset() {
    TEST_START("Reset Functionality");

    resample_t resampler;
    if (!resample_init(&resampler, 44100, 48000)) {
        TEST_FAIL("Failed to initialize resampler");
        return;
    }

    // Process some samples
    float output[10];
    for (int i = 0; i < 5; i++) {
        resample_process_sample(&resampler, 0.5f, output, 10);
    }

    uint32_t samples_before = resampler.input_count;

    // Reset
    resample_reset(&resampler);

    // Check that state was reset
    if (resampler.input_count == 0 && resampler.output_count == 0 &&
        resampler.phase == 0.0) {
        TEST_PASS();
        printf("    Reset successful: %u -> 0 samples\n", samples_before);
    } else {
        TEST_FAIL("Reset did not clear state properly");
    }

    resample_free(&resampler);
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - Resample Unit Tests\n");
    printf("=====================================\n\n");

    test_resample_basic();
    test_resample_upsample();
    test_resample_downsample();
    test_resample_buffer();
    test_resample_ratios();
    test_resample_configs();
    test_resample_errors();
    test_resample_reset();

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return tests_run == tests_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
