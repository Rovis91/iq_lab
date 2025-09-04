/*
 * IQ Lab - FM Demodulator Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "../../src/demod/fm.h"

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

// Test basic FM demodulation
void test_fm_basic() {
    TEST_START("Basic FM Demodulation");

    fm_demod_t fm;
    if (!fm_demod_init(&fm, 48000.0f)) {
        TEST_FAIL("Failed to initialize FM demodulator");
        return;
    }

    // Generate a simple FM signal: constant frequency (no modulation)
    // This should produce 0 output (no frequency deviation)
    float i_sample = 1.0f;  // Unit circle
    float q_sample = 0.0f;

    float output = fm_demod_process_sample(&fm, i_sample, q_sample);

    // First sample should be 0 (no previous phase)
    if (fabsf(output) < 0.01f) {
        TEST_PASS();
    } else {
        TEST_FAIL("Basic FM demodulation failed");
        printf("    Expected ~0, Got: %.6f\n", output);
    }

    fm_demod_reset(&fm);
}

// Test phase difference calculation
void test_fm_phase_difference() {
    TEST_START("Phase Difference Calculation");

    // Test 90-degree phase shift
    float phase_diff = fm_phase_difference(1.0f, 0.0f, 0.0f, 1.0f);
    float expected = M_PI / 2.0f; // 90 degrees

    if (fabsf(phase_diff - expected) < 0.01f) {
        TEST_PASS();
    } else {
        TEST_FAIL("Phase difference calculation incorrect");
        printf("    Expected: %.6f, Got: %.6f\n", expected, phase_diff);
    }

    // Test -90-degree phase shift
    phase_diff = fm_phase_difference(0.0f, 1.0f, 1.0f, 0.0f);
    expected = -M_PI / 2.0f;

    if (fabsf(phase_diff - expected) < 0.01f) {
        TEST_PASS();
    } else {
        TEST_FAIL("Negative phase difference calculation incorrect");
        printf("    Expected: %.6f, Got: %.6f\n", expected, phase_diff);
    }
}

// Test deemphasis filter coefficient calculation
void test_fm_deemphasis_coeff() {
    TEST_START("Deemphasis Coefficient");

    // Test 50μs deemphasis at 48kHz
    float coeff = fm_compute_deemphasis_coeff(50e-6f, 48000.0f);

    // Should be a value between 0 and 1
    if (coeff > 0.0f && coeff < 1.0f) {
        TEST_PASS();
        printf("    Deemphasis coefficient: %.6f\n", coeff);
    } else {
        TEST_FAIL("Deemphasis coefficient out of range");
        printf("    Coefficient: %.6f\n", coeff);
    }
}

// Test FM demodulation with frequency deviation
void test_fm_frequency_deviation() {
    TEST_START("Frequency Deviation");

    fm_demod_t fm;
    if (!fm_demod_init_custom(&fm, 48000.0f, 75000.0f, 50e-6f, false)) {
        TEST_FAIL("Failed to initialize FM demodulator");
        return;
    }

    // Generate two samples with 90-degree phase difference
    // This represents a frequency deviation
    float output1 = fm_demod_process_sample(&fm, 1.0f, 0.0f);  // Phase = 0
    float output2 = fm_demod_process_sample(&fm, 0.0f, 1.0f);  // Phase = π/2

    // The second sample should show frequency deviation
    if (fabsf(output2) > fabsf(output1)) {
        TEST_PASS();
        printf("    Sample 1: %.6f, Sample 2: %.6f\n", output1, output2);
    } else {
        TEST_FAIL("Frequency deviation not detected");
        printf("    Sample 1: %.6f, Sample 2: %.6f\n", output1, output2);
    }

    fm_demod_reset(&fm);
}

// Test buffer processing
void test_fm_buffer_processing() {
    TEST_START("Buffer Processing");

    fm_demod_t fm;
    if (!fm_demod_init(&fm, 48000.0f)) {
        TEST_FAIL("Failed to initialize FM demodulator");
        return;
    }

    const int buffer_size = 100;
    float i_buffer[buffer_size];
    float q_buffer[buffer_size];
    float output_buffer[buffer_size];

    // Generate simple rotating signal
    for (int i = 0; i < buffer_size; i++) {
        float phase = 2.0f * M_PI * i / buffer_size;
        i_buffer[i] = cosf(phase);
        q_buffer[i] = sinf(phase);
    }

    // Process buffer
    if (fm_demod_process_buffer(&fm, i_buffer, q_buffer, buffer_size, output_buffer)) {
        // Check that output is reasonable (not all zeros after initial samples)
        int non_zero_count = 0;
        for (int i = 10; i < buffer_size; i++) { // Skip first few samples
            if (fabsf(output_buffer[i]) > 0.001f) {
                non_zero_count++;
            }
        }

        if (non_zero_count > buffer_size / 4) {
            TEST_PASS();
            printf("    Non-zero samples: %d/%d\n", non_zero_count, buffer_size);
        } else {
            TEST_FAIL("Buffer processing produced too many zero samples");
        }
    } else {
        TEST_FAIL("Buffer processing failed");
    }

    fm_demod_reset(&fm);
}

// Test stereo pilot detection
void test_fm_stereo_pilot() {
    TEST_START("Stereo Pilot Detection");

    fm_demod_t fm;
    if (!fm_demod_init_custom(&fm, 48000.0f, 75000.0f, 50e-6f, true)) {
        TEST_FAIL("Failed to initialize FM demodulator with stereo");
        return;
    }

    // Process some samples
    for (int i = 0; i < 100; i++) {
        fm_demod_process_sample(&fm, 1.0f, 0.0f);
    }

    // Check pilot level (should be low for this simple signal)
    float pilot_level = fm_demod_get_pilot_level(&fm);
    bool is_stereo = fm_demod_is_stereo(&fm);

    // For a simple signal like this, pilot should be low
    if (pilot_level >= 0.0f && pilot_level < 1.0f) {
        TEST_PASS();
        printf("    Pilot level: %.6f, Stereo detected: %s\n",
               pilot_level, is_stereo ? "yes" : "no");
    } else {
        TEST_FAIL("Pilot level out of range");
        printf("    Pilot level: %.6f\n", pilot_level);
    }

    fm_demod_reset(&fm);
}

// Test reset functionality
void test_fm_reset() {
    TEST_START("Reset Functionality");

    fm_demod_t fm;
    if (!fm_demod_init(&fm, 48000.0f)) {
        TEST_FAIL("Failed to initialize FM demodulator");
        return;
    }

    // Process some samples
    for (int i = 0; i < 10; i++) {
        fm_demod_process_sample(&fm, 1.0f, 0.0f);
    }

    uint32_t samples_before = fm.samples_processed;

    // Reset
    fm_demod_reset(&fm);

    // Check that state was reset
    if (fm.samples_processed == 0 && !fm.phase_initialized) {
        TEST_PASS();
        printf("    Reset successful: %u -> 0 samples\n", samples_before);
    } else {
        TEST_FAIL("Reset did not clear state properly");
    }
}

// Test error conditions
void test_fm_errors() {
    TEST_START("Error Conditions");

    fm_demod_t fm;
    int errors_handled = 0;

    // Test NULL demodulator
    if (!fm_demod_init(NULL, 48000.0f)) errors_handled++;

    // Test invalid sample rate
    if (!fm_demod_init(&fm, 0.0f)) errors_handled++;

    // Test invalid parameters
    if (!fm_demod_init_custom(&fm, 48000.0f, -1.0f, 50e-6f, false)) errors_handled++;
    if (!fm_demod_init_custom(&fm, 48000.0f, 75000.0f, -1.0f, false)) errors_handled++;

    // Test NULL buffers
    if (fm_demod_init(&fm, 48000.0f)) {
        if (!fm_demod_process_buffer(&fm, NULL, (float[10]){0}, 10, (float[10]){0})) errors_handled++;
        if (!fm_demod_process_buffer(&fm, (float[10]){0}, NULL, 10, (float[10]){0})) errors_handled++;
        if (!fm_demod_process_buffer(&fm, (float[10]){0}, (float[10]){0}, 10, NULL)) errors_handled++;
        fm_demod_reset(&fm);
    }

    if (errors_handled >= 8) {
        TEST_PASS();
    } else {
        TEST_FAIL("Not all error conditions handled");
        printf("    Errors handled: %d/8\n", errors_handled);
    }
}

int main() {
    printf("=====================================\n");
    printf("IQ Lab - FM Demodulator Unit Tests\n");
    printf("=====================================\n\n");

    test_fm_basic();
    test_fm_phase_difference();
    test_fm_deemphasis_coeff();
    test_fm_frequency_deviation();
    test_fm_buffer_processing();
    test_fm_stereo_pilot();
    test_fm_reset();
    test_fm_errors();

    printf("\n=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    return tests_run == tests_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
